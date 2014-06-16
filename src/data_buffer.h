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

