/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/io_pipe.h"
#include "libp/io_serialize.h"

#include "libp/assert.h"
#include "libp/macros.h"
#include "libp/alloc.h"
#include "libp/socket.h"

#include "io_buffer.h"
#include "pipe_misc.h"

#include <string.h>

/*
 *
 */
struct dgm_pipe
{
	io_pipe     base;
	io_pipe   * io;

	size_t      max_size;
	size_t      max_hdr_size;

	io_buffer * rx;
};

typedef struct dgm_pipe dgm_pipe;

/*
 *
 */
static
void dgm_pipe_clone_state(dgm_pipe * p)
{
	clone_pipe_state(&p->base, p->io);
}

/*
 *	io_pipe api
 */
static
void dgm_pipe_init(io_pipe * self, event_loop * evl)
{
	dgm_pipe * p = struct_of(self, dgm_pipe, base);

	assert(self->on_activity); /* must be set */

	p->io->init(p->io, evl);   /* just pass it through */
	dgm_pipe_clone_state(p);
}

static
int process_rx(dgm_pipe * p, void * buf, size_t len, size_t * dgm_size)
{
	io_buffer * rx = p->rx;
	int r;

	assert(p->rx);

	/*
	 *	parse header
	 */
	*dgm_size = 0;

	r = io_parse_size(rx->head, rx->size, dgm_size);
	if (r < 0)
		return -1; /* malformed */
		
	if (r == 0)
		return 0;  /* no enough header data */

	if (*dgm_size > p->max_size)
		return -1; /* too big */

	if (r + *dgm_size > rx->size)
		return 0;  /* no enough payload */

	/*
	 *	extract dgm
	 */
	memmove(buf, p->rx->head + r, *dgm_size);
	
	rx->head += r + *dgm_size;
	rx->size -= r + *dgm_size;

	return r;
}


static
void adjust_rx(dgm_pipe * p, size_t min_cap)
{
	io_buffer * rx;

	assert(p->rx);

	if (p->rx->capacity < min_cap)
	{
		rx = alloc_io_buffer(min_cap, p->rx->head, p->rx->size);
		free_io_buffer(p->rx);
		p->rx = rx;
	}
	else
	{
		rx = p->rx;

		if (rx->size && rx->head != rx->data)
			memmove(rx->data, rx->head, rx->size);

		rx->head = rx->data;
	}
}

static
int dgm_pipe_recv(io_pipe * self, void * buf, size_t len)
{
	dgm_pipe * p = struct_of(self, dgm_pipe, base);
	size_t  dgm_size;
	size_t  space;
	int     r;

	if (p->base.broken)
		return -1;

	/* leftovers ? */
	if (p->rx)
	{
		r = process_rx(p, buf, len, &dgm_size);
		if (r < 0)
			/* malformed header or dgm is bigger than buf */
			goto err;

		if (r > 0)
		{
			/* successfully extracted into buf */
			r = dgm_size;
			goto pass;
		}

		/* no enough header or payload data */

		if (dgm_size && dgm_size > len)
		{
			/* buf is too small anyways */
			goto err;
		}

		/* move what's left to the front of p->rx
		   and ensure there's enough head space */
		adjust_rx(p, p->max_hdr_size + len);
	}
	else
	{
		p->rx = alloc_io_buffer(p->max_hdr_size + len, NULL, 0);
	}

	assert(p->rx && p->rx->head == p->rx->data);
	assert(p->rx->size < p->rx->capacity);

	/* OK, read a bit more */
	space  = p->rx->capacity;
	space -= p->rx->size;

	r = p->io->recv(p->io, p->rx->head + p->rx->size, space);
	if (r < 0)
		goto pass;

	if (r == 0)
	{
		/* got EOF halfway through a datagram ? */
		if (p->rx->size)
			goto err;

		goto pass;
	}

	p->rx->size += r;

	/*
	 *
	 */
	r = process_rx(p, buf, len, &dgm_size);
	if (r < 0)
		goto err;

	/* got a datagram or no enough data */
	r = (r > 0) ? dgm_size: -1;

pass:
	if (! p->rx->size)
	{
		free_io_buffer(p->rx);
		p->rx = NULL;
	}

	dgm_pipe_clone_state(p);
	return r;

err:

	free_io_buffer(p->rx);
	p->rx = NULL;

	tag_pipe_as_broken(&p->base);
	return -1;
}

static
int dgm_pipe_send(io_pipe * self, const void * buf, size_t len)
{
	dgm_pipe * p = struct_of(self, dgm_pipe, base);
	io_buffer * dgm;
	int r;

	if (! p->base.writable)
		return -1;

	/*
	 *	format the datagram
	 */
	dgm = alloc_io_buffer(len + p->max_hdr_size, NULL, 0);
	assert(dgm);

	r = io_store_size(dgm->head, dgm->capacity, len);
	if (r < 0)
	{
		tag_pipe_as_broken(&p->base);
		free_io_buffer(dgm);
		return -1;
	}

	assert(r + len <= dgm->capacity);

	memcpy(dgm->head + r, buf, len); /* oi vey */
	dgm->size = r + len;

	/*
	 *	send it
	 */
	r = p->io->send(p->io, dgm->head, dgm->size);
	dgm_pipe_clone_state(p);

	assert(r < 0 || r == dgm->size); /* due to p->io being atx_pipe */

	free_io_buffer(dgm);
	return (r > 0) ? len : -1;
}

static
int dgm_pipe_send_fin(io_pipe * self)
{
	dgm_pipe * p = struct_of(self, dgm_pipe, base);
	int r;

	r = p->io->send_fin(p->io);
	dgm_pipe_clone_state(p);
	return r;
}

static
void dgm_pipe_discard(io_pipe * self)
{
	dgm_pipe * p = struct_of(self, dgm_pipe, base);

	p->io->discard(p->io);

	free_io_buffer(p->rx);
	heap_free(p);
}

/*
 *	dgm_pipe.io's callback
 */
void dgm_pipe_on_activity(void * context, uint events)
{
	dgm_pipe * p = (dgm_pipe *)context;

	dgm_pipe_clone_state(p);
	p->base.on_activity(p->base.on_context, events);
}

/*
 *
 */
io_pipe * new_dgm_pipe(io_pipe * io, size_t max_size)
{
	dgm_pipe * p;

	p = (dgm_pipe*)heap_zalloc(sizeof *p);

	p->base.init     = dgm_pipe_init;
	p->base.recv     = dgm_pipe_recv;
	p->base.send     = dgm_pipe_send;
	p->base.send_fin = dgm_pipe_send_fin;
	p->base.discard  = dgm_pipe_discard;

	p->io = new_atx_pipe(io);
	p->io->on_activity = dgm_pipe_on_activity;
	p->io->on_context = p;

	p->max_size = max_size;
	p->max_hdr_size = 5;

	return &p->base;
}

