#ifndef _DATA_BUFFER_H_tcpstriper_
#define _DATA_BUFFER_H_tcpstriper_

#include "types.h"

/*
 *
 */
struct data_buffer
{
	size_t capacity;
	size_t size;
	char   data[1];
};

typedef struct data_buffer data_buffer;

data_buffer * alloc_data_buffer(size_t capacity);
data_buffer * init_data_buffer (size_t capacity, void * data, size_t size);
void          free_data_buffer (data_buffer * buf);

#endif

