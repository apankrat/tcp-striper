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

struct br_stream
{
	br_bridge * bridge;
	br_stream * peer;

	io_pipe   * pipe;
	io_buffer * pending;
};

struct br_bridge
{
	io_bridge    base;

	event_loop * evl;
	int          in_callback : 1;
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
 *	a callback from pipes
 */
static
void br_stream_on_activity(void * context, uint events)
{
	br_stream * st = (br_stream *)context;
	br_bridge * br = st->bridge;

	/*
	 *	Catch recursion
	 */
	assert(! br->in_callback);
	if (br->in_callback)
		return;

//	xx
}

/*
 *	api / init
 */
static
void br_bridge_init(io_bridge * self, event_loop * evl)
{
	br_bridge * br = struct_of(self, br_bridge, base);

	assert(! br->evl);
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

	assert(! br->dead);
	br->dead = 1;

	if (br->in_callback)
		return;

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

	return &br->base;
}

