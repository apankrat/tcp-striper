/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_IO_BRIDGE_H_
#define _LIBP_IO_BRIDGE_H_

#include "libp/io_pipe.h"
#include "libp/io_buffer.h"

/*
 *	io_bridge connects two io_pipe instances so that whatever
 *	comes out of one is forwarded into another.
 *
 *	io_bridge also propagates the reception of EOF from one
 *	pipe to another and it replicates the state of congestion
 *	between the pipes, meaning that if it cannot send() data
 *	received from one pipe into another, then it temporarily
 *	stops reading data from former until send() goes through.
 */
typedef struct io_bridge io_bridge;

struct io_bridge
{
	/* publics */
	io_pipe * l;       /* left  */
	io_pipe * r;       /* right */

	/* api */
	void (* init)(io_bridge * self, event_loop * evl);
	void (* discard)(io_bridge * self);

	/* callback */
	void (* on_shutdown)(void * context, int graceful);
	void  * on_context;
};

/*
 *
 */
io_bridge * new_io_bridge(io_pipe * l, io_pipe * r);

#endif

