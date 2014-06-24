/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_IO_BRIDGE_H_
#define _LIBP_IO_BRIDGE_H_

#include "libp/io_pipe.h"

/*
 *	io_bridge connects two io_pipe instances so that whatever
 *	comes out of one is forwarded into another.
 *
 *	io_bridge also propagates the reception of FIN from one
 *	pipe to another and it replicates the state of congestion
 *	between the pipes, meaning that if it cannot send() data
 *	received from one pipe into another, then it temporarily
 *	stops reading data from former until send() goes through.
 */
typedef struct br_pipe    br_pipe;
typedef struct io_bridge  io_bridge;

struct br_pipe
{
	/* pipe */
	io_pipe * pipe;

	/* config */
	size_t    recv_size;

	/* stats */
	uint64_t  tx;
	uint64_t  rx;
	size_t    congestions;
};

struct io_bridge
{
	br_pipe * l;
	br_pipe * r;

	/* The API */
	void (* init)(io_bridge * self, event_loop * evl);
	void (* discard)(io_bridge * self);

	/* The callback */
	void (* on_shutdown)(void * context, int graceful);
	void  * on_context;
};

/*
 *
 */
io_bridge * new_io_bridge(io_pipe * l, io_pipe * r);

#endif

