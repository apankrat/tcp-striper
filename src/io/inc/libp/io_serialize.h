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
 *	store() returns a number of bytes used from the buffer.
 *	If 'buf' is NULL, store() returns space required.
 *
 *	parse() returns a number of bytes used from the buffer,
 *	0 if it rans out of data while parsing and -1 is 'buf' 
 *	contains invalid data.
 */
int io_store_size(uint8_t * buf, size_t size, size_t value);
int io_parse_size(const uint8_t * buf, size_t size, size_t * value);

#endif

