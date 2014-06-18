/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/io_bridge.h"

#include "libp/alloc.h"
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
	int          busy;     /* in callback(s) */
	int          dead : 1; /* discard was called */

	br_stream    l;        /* left  */
	br_stream    r;        /* right */
};

/*
 *	callbacks from pipes
 */
static
void br_stream_on_connected(void * context, int ok)
{
	br_stream * st = (br_stream *)context;
	br_bridge * br = st->bridge;
}

static
void br_stream_on_activity(void * context, uint events)
{
	br_stream * st = (br_stream *)context;
	br_bridge * br = st->bridge;
}

/*
 *	api / init
 */
static
void br_bridge_init(io_bridge * self, event_loop * evl)
{
	br_bridge * br = container_of(self, br_bridge, base);

	assert(! br->evl);
	br->evl = evl;

	br->l.pipe->init(br->l.pipe, evl);
	br->r.pipe->init(br->r.pipe, evl);
}

/* 
 *	api / cleanup
 */
static
void br_stream_cleanup(br_stream * st)
{
	st->pipe->discard(st->pipe);
	heap_free(st->pending);
}

static
void br_bridge_dispose(br_bridge * br)
{
	br_stream_cleanup(&br->l);
	br_stream_cleanup(&br->r);
}

static
void br_bridge_discard(io_bridge * self)
{
	br_bridge * br = container_of(self, br_bridge, base);

	assert(! br->dead);
	br->dead = 1;

	if (! br->busy)
		br_bridge_dispose(br);
	
	/* otherwise will dispose once we
	   are back from all callbacks */
}

/*
 *
 */
io_bridge * new_io_bridge(io_pipe * l, io_pipe * r)
{
	br_bridge * br;
	
	br = (br_bridge*)heap_zalloc(sizeof *br);
	
	br->l.bridge = br;
	br->l.peer = &br->r;
	br->l.pipe = l;

	br->r.bridge = br;
	br->r.peer = &br->l;
	br->r.pipe = r;
	
	br->base.l = l;
	br->base.r = r;
	br->base.init = br_bridge_init;
	br->base.discard = br_bridge_discard;_

	return &br->base;
}

