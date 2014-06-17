#include "libp/io_pipe.h"

#include "libp/assert.h"
#include "libp/macros.h"
#include "libp/alloc.h"
#include "libp/socket.h"

/*
 *
 */
struct tcp_pipe
{
	io_pipe      base;
	event_loop * evl;

	int  sk;
	int  activated : 1;
};

typedef struct tcp_pipe tcp_pipe;

/*
 *	internal
 */
static
void tcp_pipe_adjust_event_mask(tcp_pipe * c)
{
	uint mask = 0;

	if (! c->base.readable && ! c->base.fin_rcvd)
		mask |= SK_EV_readable;

	if (! c->base.writable && ! c->base.fin_sent)
		mask |= SK_EV_writable;

	c->evl->mod_socket(c->evl, c->sk, mask);
}

static
void tcp_pipe_on_activity(void * ctx, uint events)
{
	tcp_pipe * c = (tcp_pipe *)ctx;
	io_pipe * self = &c->base;
	int err;

	if (! c->activated)
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
		assert(events & (SK_EV_error | SK_EV_writable));

		c->activated = 1;

		err = sk_error(c->sk);
		if ( (events & SK_EV_error) ||
		    ((events & SK_EV_writable) && (err != 0)) )
		{
			c->evl->mod_socket(c->evl, c->sk, 0);
			events = SK_EV_error;
		}
		else
		{
			self->writable = 1;
			tcp_pipe_adjust_event_mask(c);
			events = SK_EV_writable;
		}
	}

	self->on_activity(self->on_context, events);
}

/*
 *	the api
 */
static
void tcp_pipe_init(io_pipe * self, event_loop * evl)
{
	tcp_pipe * c = structof(self, tcp_pipe, base);
	uint mask;

	assert(! c->evl);

	c->evl = evl;

	mask = SK_EV_writable;
	c->evl->add_socket(c->evl, c->sk, mask, tcp_pipe_on_activity, c);
}

static
int tcp_pipe_recv(io_pipe * self, void * buf, size_t len, int * fatal)
{
	tcp_pipe * c = structof(self, tcp_pipe, base);
	int r;

	assert(c->evl); /* must be initialized */

	r = sk_recv(c->sk, buf, len);

	if (r == 0)
	{
		self->fin_rcvd = 1;
		self->readable = 0;
		tcp_pipe_adjust_event_mask(c);
		return 0;
	}

	if (r < 0)
	{
		*fatal = sk_recv_fatal( sk_errno(c->sk) );
		self->readable = 0;
		tcp_pipe_adjust_event_mask(c);
		return -1;
	}

	return r;
}

static
int tcp_pipe_send(io_pipe * self, const void * buf, size_t len, int * fatal)
{
	tcp_pipe * c = structof(self, tcp_pipe, base);
	int r;

	assert(c->evl); /* must be initialized */

	r = sk_send(c->sk, buf, len);

	if (r < 0)
	{
		*fatal = sk_send_fatal( sk_errno(c->sk) );
		self->writable = 0;
		tcp_pipe_adjust_event_mask(c);
		return -1;
	}

	return r;
}

static
int tcp_pipe_shutdown(io_pipe * self)
{
	tcp_pipe * c = structof(self, tcp_pipe, base);

	assert(! self->fin_sent);

	if (sk_shutdown(c->sk) < 0)
		return -1;

	self->fin_sent = 1;
	self->writable = 0;
	tcp_pipe_adjust_event_mask(c);

	return 0;
}

static
void tcp_pipe_discard(io_pipe * self)
{
	tcp_pipe * c = structof(self, tcp_pipe, base);

	if (c->evl)
		c->evl->del_socket(c->evl, c->sk);

	sk_close(c->sk);

	heap_free(c);
}

/*
 *
 */
io_pipe * new_tcp_pipe(int sk)
{
	tcp_pipe * c;
	
	c = (tcp_pipe*)heap_zalloc(sizeof *c);

	c->base.init     = tcp_pipe_init;
	c->base.recv     = tcp_pipe_recv;
	c->base.send     = tcp_pipe_send;
	c->base.shutdown = tcp_pipe_shutdown;
	c->base.discard  = tcp_pipe_discard;

	c->sk = sk;

	return &c->base;
}

