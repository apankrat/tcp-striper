/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/io_pipe.h"

#include "libp/assert.h"
#include "libp/macros.h"
#include "libp/alloc.h"
#include "libp/socket.h"

#include "libp/io_pipe_misc.h"

/*
 *
 */
struct tcp_pipe
{
	io_pipe      base;
	event_loop * evl;
	int          sk;
	uint         sk_mask;
};

typedef struct tcp_pipe tcp_pipe;

/*
 *	internal
 */
static
void tcp_pipe_adjust_event_mask(tcp_pipe * p)
{
	uint sk_mask = 0;

	assert(p->base.ready);

	if (! p->base.readable && ! p->base.fin_rcvd && ! p->base.broken)
		sk_mask |= SK_EV_readable;

	if (! p->base.writable && ! p->base.fin_sent && ! p->base.broken)
		sk_mask |= SK_EV_writable;

	if (p->sk_mask == sk_mask)
		return;

	p->sk_mask = sk_mask;
	p->evl->mod_socket(p->evl, p->sk, p->sk_mask);
}

static
void tcp_pipe_tag_broken(tcp_pipe * p)
{
	p->base.broken = 1;

	/* be pedantic */
	p->base.readable = 1;
	p->base.writable = 1;
}

static
void tcp_pipe_on_activity(void * ctx, uint sk_events)
{
	tcp_pipe * p = (tcp_pipe *)ctx;
	io_pipe * self = &p->base;
	uint      io_events = 0;

	if (! p->base.ready)
	{
		/*
		 *	Just make sure to wrap tcp_pipe around
		 *	either freshly connected socket or a
		 *	socket that has a pending connect() on
		 *	it. Not around connected socket with an
		 *	already congested outbound stream and
		 *	unread inbound data, because this would
		 *	produce a single SK_EV_readable event.
		 */
		int err;
		
		assert(sk_events & (SK_EV_error | SK_EV_writable));

		err = sk_error(p->sk);
		if ( (sk_events & SK_EV_error) ||
		    ((sk_events & SK_EV_writable) && (err != 0)) )
		{
			tcp_pipe_tag_broken(p);
			io_events = IO_EV_broken;
			goto callback;
		}
		
		p->base.ready = 1;
		io_events |= IO_EV_ready;
	}

	if (sk_events & SK_EV_readable)
	{
		assert(! self->fin_rcvd);

		self->readable = 1;
		io_events |= IO_EV_readable;
	}

	if (sk_events & SK_EV_writable)
	{
		assert(! self->fin_sent);

		self->writable = 1;
		io_events |= IO_EV_writable;
	}
			
callback:
	
	tcp_pipe_adjust_event_mask(p);

	/*
	 *	This MUST be a tail call, so that if
	 *	the receiving code decides to call a
	 *	discard() on us, we won't end up using
	 *	'self' after it becomes invalid.
	 */
	self->on_activity(self->on_context, io_events);
}

/*
 *	the api
 */
static
void tcp_pipe_init(io_pipe * self, event_loop * evl)
{
	tcp_pipe * p = struct_of(self, tcp_pipe, base);

	assert(! p->evl);              /* don't initialize twice   */
	assert(  p->base.on_activity); /* must be set */
	assert(  io_pipe_state(self) == 0x00 );

	p->evl = evl;
	p->sk_mask = SK_EV_writable;

	p->evl->add_socket(p->evl, p->sk, p->sk_mask, tcp_pipe_on_activity, p);
}

static
int tcp_pipe_recv(io_pipe * self, void * buf, size_t len)
{
	tcp_pipe * p = struct_of(self, tcp_pipe, base);
	int r;

	assert(p->evl); /* must be initialized */

	r = sk_recv(p->sk, buf, len);

	if (r > 0)
	{
		self->readable = 1;
	}
	else
	if (r < 0)
	{
		self->readable = 0;
		if (sk_recv_fatal( sk_errno(p->sk) ))
			tcp_pipe_tag_broken(p);
	}
	else
	{
		/* fin */
		self->readable = 0;
		self->fin_rcvd = 1;
	}

	tcp_pipe_adjust_event_mask(p);
	return r;
}

static
int tcp_pipe_send(io_pipe * self, const void * buf, size_t len)
{
	tcp_pipe * p = struct_of(self, tcp_pipe, base);
	int r;

	assert(p->evl); /* must be initialized */

	r = sk_send(p->sk, buf, len);

	if (r == len)
	{
		self->writable = 1;
	}
	else
	if (r >= 0)
	{
		/* partial send */
		self->writable = 0;
	}
	else
	{
		self->writable = 0;
		if (sk_send_fatal( sk_errno(p->sk) ))
			tcp_pipe_tag_broken(p);
	}

	tcp_pipe_adjust_event_mask(p);
	return r;
}

static
int tcp_pipe_send_fin(io_pipe * self)
{
	tcp_pipe * p = struct_of(self, tcp_pipe, base);
	int r;

	assert(p->evl);           /* must be initialized  */
	assert(! self->fin_sent); /* don't sent FIN twice */

	r = sk_shutdown(p->sk);

	if (r < 0)
	{
		tcp_pipe_tag_broken(p);
	}
	else
	{
		self->writable = 0;
		self->fin_sent = 1;
	}

	tcp_pipe_adjust_event_mask(p);
	return r;
}

static
void tcp_pipe_discard(io_pipe * self)
{
	tcp_pipe * p = struct_of(self, tcp_pipe, base);

	if (p->evl)
		p->evl->del_socket(p->evl, p->sk);

	sk_close(p->sk);

	heap_free(p);
}

/*
 *
 */
io_pipe * new_tcp_pipe(int sk)
{
	tcp_pipe * p;

	p = (tcp_pipe*)heap_zalloc(sizeof *p);

	p->base.init     = tcp_pipe_init;
	p->base.recv     = tcp_pipe_recv;
	p->base.send     = tcp_pipe_send;
	p->base.send_fin = tcp_pipe_send_fin;
	p->base.discard  = tcp_pipe_discard;

	p->sk = sk;

	return &p->base;
}

