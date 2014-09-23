/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_IO_SERIALIZE_H_
#define _LIBP_IO_SERIALIZE_H_

#include "libp/types.h"

/*
 *	A set of functions responsible for packing and extracing
 *	various data types to/from a binary blob.
 *
 *	Each function returns a number of bytes used from the
 *	buffer or -1 for an error, which means 'no enough space' 
 *	for store() and 'malformed data' for parse()
 */
int io_store_size(uint8_t * buf, size_t size, size_t value);
int io_parse_size(const uint8_t * buf, size_t size, size_t * value);

#endif

