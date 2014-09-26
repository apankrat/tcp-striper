/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_IO_BUFFER_H_
#define _LIBP_IO_BUFFER_H_

#include "libp/types.h"

/*
 *
 */
struct io_buffer
{
	size_t    capacity;
	
	uint8_t * head;    /* data <= head */
	size_t    size;    /* head + size <= data + capacity */

	uint8_t   data[1]; /* capacity bytes */
};

typedef struct io_buffer io_buffer;

/*
 *
 */
io_buffer * alloc_io_buffer(size_t capacity, const void * data, size_t size);
void reset_io_buffer(io_buffer * buf);
void free_io_buffer (io_buffer * buf);

#endif

