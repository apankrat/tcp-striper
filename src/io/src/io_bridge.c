/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/io_bridge.h"

#include "libp/alloc.h"
#include "libp/assert.h"
#include "libp/macros.h"

#include "io_buffer.h"

/*
 *
 */
typedef struct br_stream br_stream;
typedef struct br_bridge br_bridge;

struct br_stream  /* : br_pipe */
{
	br_bridge * bridge;
	br_stream * peer;

	br_pipe     base;
	io_pipe   * pipe; /* a shortcut for base.pipe */

	io_buffer * pending;
};

struct br_bridge  /* : io_bridge */
{
	io_bridge    base;

	event_loop * evl;
	int          dead : 1;

	br_stream    l; /* left  */
	br_stream    r; /* right */
};

/*
 *	internal
 */
static
void br_stream_cleanup(br_stream * st)
{
	st->pipe->discard(st->pipe);
	free_io_buffer(st->pending);
}

static
void br_bridge_dispose(br_bridge * br)
{
	br_stream_cleanup(&br->l);
	br_stream_cleanup(&br->r);
}

/*
 *	relaying
 */
static
int br_stream_flush(br_stream * dst)
{
	io_buffer * buf = dst->pending;
	int bytes;

	assert(dst->pending && dst->pipe->writable);

	bytes = dst->pipe->send(dst->pipe, buf->head, buf->size);

	if (bytes < (int)buf->size || ! dst->pipe->writable)
		dst->base.congestions++;

	if (bytes < 0)
	{
		assert(! dst->pipe->writable);
		return dst->pipe->broken ? -1 : 0;
	}

	dst->base.tx += bytes;

	if (bytes < (int)buf->size)
	{
		/* partial write */
		assert(! dst->pipe->writable);

		buf->head += bytes;
		buf->size -= bytes;
		return 0;
	}

	/* complete write */
	free_io_buffer(buf);
	dst->pending = NULL;

	return 0;
}

static
int br_bridge_rx_tx(br_stream * src, br_stream * dst, io_buffer ** space)
{
	io_buffer * buf = *space;
	size_t recv_size = src->base.recv_size;
	int bytes;

	assert(src->pipe->readable && dst->pipe->writable);
	assert(! dst->pending);

	assert(buf && recv_size <= buf->capacity);

	/*
	 *	rx
	 */
	bytes = src->pipe->recv(src->pipe, buf->data, recv_size);

	if (bytes < 0)
		return src->pipe->broken ? -1 : 0;

	if (bytes == 0)
	{
		/* got FIN */
		assert(  src->pipe->fin_rcvd);
		assert(! dst->pipe->fin_sent);

		return (dst->pipe->send_fin(dst->pipe) < 0) &&
		        dst->pipe->broken ? -1 : 0;
	}

	src->base.rx += bytes;

	/*
	 *	tx
	 */
	buf->size = bytes;
	bytes = dst->pipe->send(dst->pipe, buf->data, buf->size);

	if (bytes < (int)buf->size || ! dst->pipe->writable)
		dst->base.congestions++;

	if (bytes > 0)
		dst->base.tx += bytes;

	if (bytes == buf->size)
		return 0;

	if (bytes < 0)
	{
		if (dst->pipe->broken)
			return -1;
		bytes = 0;
	}

	/*
	 *	congested
	 */
	assert(0 <= bytes && bytes < (int)buf->size);
	assert(! dst->pipe->writable);

	buf->head += bytes;
	buf->size -= bytes;

	/* appropriate the buffer */
	dst->pending = buf;
	*space = NULL;
	return 0;
}

static
int br_bridge_relay(br_stream * src, br_stream * dst)
{
	io_buffer * buf;
	size_t buf_size;

	buf_size = src->peer->base.recv_size;
	if (buf_size < src->base.recv_size)
		buf_size = src->base.recv_size;

	buf = alloc_io_buffer(buf_size, NULL, 0);
	if (! buf)
		return -1;

	while (src->pipe->readable && dst->pipe->writable)
	{
		if (br_bridge_rx_tx(src, dst, &buf) < 0)
		{
			free_io_buffer(buf);
			return -1;
		}

		/*
		 *	We either still have the buf or it was
		 *	taken away by dst, because it got clogged
		 *	and so it's no longer writable (and we'll
		 *	break out of the loop in a moment).
		 */
		assert(buf || ! dst->pipe->writable);
	}

	free_io_buffer(buf);
	return 0;
}

/*
 *	a callback from the pipes
 */
static
void br_stream_on_activity(void * context, uint events)
{
	br_stream * self = (br_stream *)context;
	br_stream * peer = self->peer;
	br_bridge * br = self->bridge;

	/*
	 *
	 */
	if (br->dead)
		return;

	/*
	 *
	 */
	if (events & IO_EV_writable)
	{
		assert(self->pipe->writable);

		if (self->pending &&
		    br_stream_flush(self) < 0)
		{
			goto err;
		}

		if (self->pipe->writable &&
		    br_bridge_relay(peer, self) < 0)
		{
			goto err;
		}
	}

	if (events & IO_EV_readable)
	{
		if (peer->pipe->writable &&
		    br_bridge_relay(self, peer) < 0)
		{
			goto err;
		}
	}

	if (events & IO_EV_broken)
	{
		goto err;
	}

	/*
	 *
	 */
	if (peer->pipe->fin_sent && peer->pipe->fin_rcvd)
	{
		br->dead = 1;
		br->base.on_shutdown(br->base.on_context, 1);
	}

	return;

err:
	br->dead = 1;
	br->base.on_shutdown(br->base.on_context, 0);
}

/*
 *	api / init
 */
static
void br_bridge_init(io_bridge * self, event_loop * evl)
{
	br_bridge * br = struct_of(self, br_bridge, base);

	assert(! br->evl);            /* don't initialize twice */
	assert(br->base.on_shutdown); /* the callback must be set */

	br->evl = evl;

	br->l.pipe->init(br->l.pipe, evl);
	br->r.pipe->init(br->r.pipe, evl);
}

/*
 *	api / cleanup
 */
static
void br_bridge_discard(io_bridge * self)
{
	br_bridge * br = struct_of(self, br_bridge, base);

	br_bridge_dispose(br);
}

/*
 *
 */
static
void br_setup_stream(br_bridge * br, br_stream * st, io_pipe * io)
{
	st->bridge = br;
	st->peer = (st == &br->l) ? &br->r : &br->l;

	st->base.pipe = io;
	st->base.recv_size = 1024*1024;
	st->base.tx = 0;
	st->base.rx = 0;
	st->base.congestions = 0;

	st->pipe = io;
	st->pipe->on_activity = br_stream_on_activity;
	st->pipe->on_context = st;
}

io_bridge * new_io_bridge(io_pipe * l, io_pipe * r)
{
	br_bridge * br;

	br = (br_bridge*)heap_zalloc(sizeof *br);

	br->base.l = &br->l.base;
	br->base.r = &br->r.base;

	br->base.init = br_bridge_init;
	br->base.discard = br_bridge_discard;

	br_setup_stream(br, &br->l, l);
	br_setup_stream(br, &br->r, r);

	return &br->base;
}

