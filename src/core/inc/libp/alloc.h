/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_ALLOC_H_
#define _LIBP_ALLOC_H_

#include <stdlib.h>

/*
 *	malloc/free proxy api
 *	zalloc = malloc + bzero
 */
void * heap_malloc(size_t size);
void * heap_zalloc(size_t size);
void * heap_realloc(void * ptr, size_t size);
void   heap_free(void * ptr);

/*
 *	What's used for actual de/allocation calls.
 *	Defaults to realloc()
 */
extern void * (* heap_allocator)(void * ptr, size_t len);

#endif
