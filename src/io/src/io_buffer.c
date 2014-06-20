/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "io_buffer.h"

#include "libp/alloc.h"
#include "libp/assert.h"

#include <string.h>

/*
 *
 */
io_buffer * alloc_io_buffer(size_t capacity, const void * data, size_t size)
{
	io_buffer * buf;
	size_t need;

	assert(! data || size <= capacity);

	need = sizeof(io_buffer) - 1 + capacity;
	buf = (io_buffer *)heap_zalloc(need);

	buf->capacity = capacity;
	buf->head = buf->data;

	if (data)
	{
		memcpy(buf->data, data, size);
		buf->size = size;
	}
	else
	{
		buf->size = 0;
	}

	return buf;
}

void reset_io_buffer(io_buffer * buf)
{
	buf->head = buf->data;
	buf->size = 0;
}

void free_io_buffer (io_buffer * buf)
{
	heap_free(buf);
}

