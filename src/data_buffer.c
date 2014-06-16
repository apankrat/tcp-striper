/*
 *	This file is a part of the tcp-striper project.
 *	Copyright (c) 2004-2011 Alex Pankratov.
 *
 *	http://github.com/apankrat/tcp-striper
 */

/*
 *	The program is distributed under terms of BSD license.
 *	You can obtain the copy of the license by visiting:
 *
 *	http://www.opensource.org/licenses/bsd-license.php
 */

#include "data_buffer.h"

#include "alloc.h"
#include "assert.h"

#include <string.h>

/*
 *
 */
data_buffer * alloc_data_buffer(size_t capacity)
{
	data_buffer * buf;
	size_t need;

	need = sizeof(data_buffer) - 1 + capacity;
	buf = (data_buffer *)heap_alloc(need);

	buf->capacity = capacity;
	buf->size = 0;
	return buf;
}

data_buffer * init_data_buffer(size_t capacity, void * data, size_t size)
{
	data_buffer * buf;
	
	assert(data && size && capacity >= size);

	buf = alloc_data_buffer(capacity);
	buf->size = size;
	memcpy(buf->data, data, size);

	return buf;
}

void free_data_buffer(data_buffer * buf)
{
	heap_free(buf);
}

