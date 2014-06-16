#ifndef _ALLOC_H_tcpstriper_
#define _ALLOC_H_tcpstriper_

#include <stdlib.h>

void * heap_alloc(size_t size);
void * heap_realloc(void * ptr, size_t size);
void   heap_free(void * ptr);

#endif
