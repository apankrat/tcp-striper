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

	#include <stdio.h>

/*
 *
 */
typedef struct br_stream br_stream;
typedef struct br_bridge br_bridge;

struct br_stream
{
	br_bridge * bridge;
	br_stream * peer;

	io_pipe   * pipe;
	io_buffer * pending;

	size_t      recv_size;

	const char * name;
	uint64_t     tx;
	uint64_t     rx;
	uint64_t     cong;
};

struct br_bridge
{
	io_bridge    base;

	event_loop * evl;
	int          dead : 1;
	
	size_t       buf_size;

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

	if (st->pending)
		heap_free(st->pending);
}

static
void br_bridge_dispose(br_bridge * br)
{
	br_stream_cleanup(&br->l);
	br_stream_cleanup(&br->r);
}

/*
 *	flushery and pumpery
 */
static
int br_stream_flush(br_stream * dst)
{
	io_buffer * buf = dst->pending;
	int bytes;
	int fatal;

	assert(dst->pending && dst->pipe->writable);

	bytes = dst->pipe->send(dst->pipe, buf->head, buf->size, &fatal);
	if (bytes < 0)
		return fatal ? -1 : 0;

	dst->tx += bytes;

	if (bytes < buf->size)
	{
		/* partial write */
		assert(! dst->pipe->writable);
		buf->head += bytes;
		buf->size -= bytes;

		dst->cong++;
		return 0;
	}

	/* complete write */
	free_io_buffer(buf);
	dst->pending = NULL;

	/* propagate FIN */
	if (dst->peer->pipe->fin_rcvd)
		return dst->pipe->shutdown(dst->pipe);

	return 0;
}

static
int br_bridge_rx_tx(br_stream * src, br_stream * dst, io_buffer ** space)
{
	io_buffer * buf = *space;
	int bytes;
	int fatal;

	assert(src->pipe->readable && dst->pipe->writable);
	assert(! dst->pending);
	assert(src->recv_size <= buf->capacity);

	bytes = src->pipe->recv(src->pipe, buf->data, src->recv_size, &fatal);
	if (bytes < 0)
		return fatal ? -1 : 0;

	if (bytes == 0)
	{
		assert(  src->pipe->fin_rcvd);
		assert(! dst->pipe->fin_sent);
		return dst->pipe->shutdown(dst->pipe);
	}

	src->rx += bytes;

	buf->size = bytes;
	bytes = dst->pipe->send(dst->pipe, buf->data, buf->size, &fatal);

	if (bytes > 0)
		dst->tx += bytes;

	if (bytes == buf->size)
		return 0;

	if (bytes < 0)
	{
		if (fatal)
			return -1;
		bytes = 0;
	}

	assert(0 <= bytes && bytes < buf->size);
	assert(! dst->pipe->writable);

	buf->head += bytes;
	buf->size -= bytes;

	/* appropriate the buffer */
	dst->pending = buf;
	*space = NULL;

	dst->cong++;
	return 0;
}

static
int br_bridge_relay(br_stream * src, br_stream * dst)
{
	io_buffer * buf;

	buf = alloc_io_buffer(src->bridge->buf_size, NULL, 0);
	if (! buf)
		return -1;

	while (src->pipe->readable && dst->pipe->writable)
	{
		if (br_bridge_rx_tx(src, dst, &buf) < 0)
		{
			free_io_buffer(buf);
			return -1;
		}

		/* if tx_rx() took away 'buf', then it can only
		 * be because dst wasn't fully writable, and so
		 * we'll break out of this while() in a moment */
		assert(buf || ! dst->pipe->writable);
	}

	free_io_buffer(buf);
	return 0;
}

/*
 *	a callback from pipes
 */
static
void br_stream_dump(br_stream * st, uint events)
{
	printf("( [%c%c%c] w:%d r:%d f:%d/%d rx:%llu tx:%llu cong:%llu )",
		(events & SK_EV_writable) ? 'W' : '.',
		(events & SK_EV_readable) ? 'R' : '.',
		(events & SK_EV_error)    ? 'E' : '.',
		st->pipe->writable ? 1 : 0,
		st->pipe->readable ? 1 : 0,
		st->pipe->fin_sent ? 1 : 0,
		st->pipe->fin_rcvd ? 1 : 0,
		st->rx, st->tx, st->cong);
}

static
void br_stream_on_activity(void * context, uint events)
{
	br_stream * self = (br_stream *)context;
	br_stream * peer = self->peer;
	br_bridge * br = self->bridge;

	br_stream_dump(&br->l, (&br->l == self) ? events : 0);
	printf("  ");
	br_stream_dump(&br->r, (&br->r == self) ? events : 0);
	printf("\r");

	/*
	 *	Catch recursion
	 */
	if (events & SK_EV_writable)
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

	if (events & SK_EV_readable)
	{
		if (peer->pipe->writable &&
		    br_bridge_relay(self, peer) < 0)
		{
			goto err;
		}
	}

	if (events & SK_EV_error)
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
void br_setup_stream(br_stream * st, io_pipe * io, br_bridge * br)
{
	st->bridge = br;
	st->peer = (st == &br->l) ? &br->r : &br->l;
	st->pipe = io;
	st->pipe->on_activity = br_stream_on_activity;
	st->pipe->on_context = st;
	st->recv_size = 8*1024;

	st->name = (st == &br->l) ? "L" : "R";
	st->tx = 0;
	st->rx = 0;
	st->cong = 0;
}

io_bridge * new_io_bridge(io_pipe * l, io_pipe * r)
{
	br_bridge * br;

	br = (br_bridge*)heap_zalloc(sizeof *br);

	br->base.l = l;
	br->base.r = r;
	br->base.init = br_bridge_init;
	br->base.discard = br_bridge_discard;

	br_setup_stream(&br->l, l, br);
	br_setup_stream(&br->r, r, br);

	br->buf_size = 
		(br->l.recv_size < br->r.recv_size) ? 
		 br->r.recv_size : br->l.recv_size;

	return &br->base;
}

