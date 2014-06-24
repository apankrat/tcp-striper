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

#include "io_buffer.h"

/*
 *
 */
struct atx_pipe
{
	io_pipe     base;
	io_pipe   * io;
	io_buffer * pending;
	int         want_fin : 1;
};

typedef struct atx_pipe atx_pipe;

/*
 *
 */
static
void atx_pipe_clone_state(atx_pipe * p)
{
	p->base.ready    = p->io->ready;
	p->base.broken   = p->io->broken;
	p->base.readable = p->io->readable;
	p->base.writable = p->pending ? 0 : p->io->writable;
	p->base.fin_sent = p->io->fin_sent;
	p->base.fin_rcvd = p->io->fin_rcvd;
}

/*
 *	io_pipe api
 */
static
void atx_pipe_init(io_pipe * self, event_loop * evl)
{
	atx_pipe * p = struct_of(self, atx_pipe, base);

	assert(self->on_activity); /* must be set */

	p->io->init(p->io, evl);   /* just pass it through */
	atx_pipe_clone_state(p);
}

static
int atx_pipe_recv(io_pipe * self, void * buf, size_t len)
{
	atx_pipe * p = struct_of(self, atx_pipe, base);
	int r;
	
	r = p->io->recv(p->io, buf, len);
	atx_pipe_clone_state(p);
	return r;
}

static
int atx_pipe_send(io_pipe * self, const void * buf, size_t len)
{
	atx_pipe * p = struct_of(self, atx_pipe, base);
	int r;

	if (p->pending)
		return -1;

	r = p->io->send(p->io, buf, len);
	atx_pipe_clone_state(p);

	if (r < 0)
		return -1; /* assert(! self->writable); */

	if (r == len)
		return len;

	assert(r < len);

	/* hellooo ... partial send */
	assert(! p->base.writable);

	p->pending = alloc_io_buffer(len-r, buf+r, len-r);

	return len;
}

static
int atx_pipe_send_fin(io_pipe * self)
{
	atx_pipe * p = struct_of(self, atx_pipe, base);
	int r;

	assert(! p->base.fin_sent && ! p->want_fin); /* don't call twice */

	if (p->pending)
	{
		p->want_fin = 1;
		return 0;
	}

	r = p->io->send_fin(p->io);
	atx_pipe_clone_state(p);
	return r;
}

static
void atx_pipe_discard(io_pipe * self)
{
	atx_pipe * p = struct_of(self, atx_pipe, base);

	p->io->discard(p->io);

	free_io_buffer(p->pending);
	heap_free(p);
}

/*
 *	atx_pipe.io's callback
 */
void atx_pipe_on_activity(void * context, uint events)
{
	atx_pipe * p = (atx_pipe *)context;

	if ( (events & IO_EV_writable) && p->pending )
	{
		/*
		 *	flush pending data
		 */
		io_buffer * buf = p->pending;
		int r;

		r = p->io->send(p->io, buf->head, buf->size);

		if (r < 0)
		{
			if (p->io->broken)
				events |= IO_EV_broken;
				
			events &= ~IO_EV_writable;
		}
		else
		if (r < buf->size)
		{
			buf->head += r;
			buf->size -= r;
			events &= ~IO_EV_writable;
		}
		else
		{
			assert(r == buf->size);

			free_io_buffer(buf);
			p->pending = NULL;

			if (! p->io->writable)
				events &= ~IO_EV_writable;
		}

		/*
		 *	execute pending shutdown()
		 */
		if (! p->pending && p->want_fin)
		{
			r = p->io->send_fin(p->io);
			if (r < 0 && p->io->broken)
				events |= IO_EV_broken;

			events &= ~IO_EV_writable;
		}
	}

	atx_pipe_clone_state(p);

	if (! events)
		return;

	/*
	 *	This MUST be a tail call. See tcp_pipe
	 *	for the details.
	 */
	p->base.on_activity(p->base.on_context, events);
}

/*
 *
 */
io_pipe * new_atx_pipe(io_pipe * io)
{
	atx_pipe * p;

	p = (atx_pipe*)heap_zalloc(sizeof *p);

	p->base.init     = atx_pipe_init;
	p->base.recv     = atx_pipe_recv;
	p->base.send     = atx_pipe_send;
	p->base.send_fin = atx_pipe_send_fin;
	p->base.discard  = atx_pipe_discard;

	p->io = io;
	p->io->on_activity = atx_pipe_on_activity;
	p->io->on_context = p;

	p->pending = NULL;

	return &p->base;
}

