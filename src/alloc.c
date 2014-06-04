#include "alloc.h"
#include <stdlib.h>

void * heap_alloc(size_t n)
{
	return malloc(n);
}

void heap_free(void * p)
{
	free(p);
}

