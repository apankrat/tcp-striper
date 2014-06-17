/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/alloc.h"

#include <stdlib.h>
#include <string.h>

/*
 *	The actual allocator
 */
void * (* heap_allocator)(void * ptr, size_t len) = realloc;

/*
 *
 */
void * heap_malloc(size_t n)
{
	return heap_allocator(NULL, n);
}

void * heap_zalloc(size_t n)
{
	void * p = heap_allocator(NULL, n);
	return p ? memset(p, 0, n) : NULL;
}

void * heap_realloc(void * ptr, size_t size)
{
	return heap_allocator(ptr, size);
}

void heap_free(void * p)
{
	heap_realloc(p, 0);
}

