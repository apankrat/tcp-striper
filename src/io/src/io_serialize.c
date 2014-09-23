/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/io_serialize.h"

/*
 *	Variable-size LSB in 7bit chunks
 *	000zzzzz zzyyyyyy yxxxxxxx -> 1xxxxxxx 1yyyyyyy 0zzzzzzz
 */
static inline
int io_store_size_unrolled(uint8_t * buf, size_t len, size_t val)
{
	if (val & 0xF0000000)
		return -1;

	if (val & 0x0FE00000)
	{
		if (len < 4)
			return -1;

		*buf++ = (val      ) | 0x80;
		*buf++ = (val >>= 7) | 0x80;
		*buf++ = (val >>= 7) | 0x80;
		*buf   = (val >>= 7);
		return 4;
	}

	if (val & 0x001FC000)
	{
		if (len < 3)
			return -1;

		*buf++ = (val      ) | 0x80;
		*buf++ = (val >>= 7) | 0x80;
		*buf   = (val >>= 7);
		return 3;
	}

	if (val & 0x00003F80)
	{
		if (len < 2)
			return -1;

		*buf++ = (val      ) | 0x80;
		*buf   = (val >>= 7);
		return 2;
	}

//	if (val & 0x0000007F)
	{
		if (len < 1)
			return -1;

		buf[0] = val;
		return 1;
	}
}

/*
 *	Unrolled version is almost twice as fast as generic,
 *	so use the unrolled one.
 */
int io_store_size(uint8_t * buf, size_t len, size_t val)
{
/*
	int r;

	for (r = 0 ; r < len; r++)
	{
		if (val & ~0x7F)
		{
			*buf++ = val | 0x80;
			val >>= 7;
			continue;
		}

		*buf = val;
		return r + 1;
	}

	return -1;
*/
	return io_store_size_unrolled(buf, len, val);
}

/*
 *	Unrolled version is within a couple of % of generic
 *	version performance, so use the generic one.
 */
int io_parse_size(const uint8_t * buf, size_t len, size_t * ret)
{
	size_t val = 0;
	int    off = 0;
	size_t oct;
	int    r;

	for (r = 0; r < len; off += 7, r++)
	{
		oct = *buf++;
			
		if (oct & 0x80)
		{
			val |= (oct & 0x7F) << off;
			continue;
		}
		
		if ( (off < 28) || ((off == 28) && (oct < 0x10)) )
		{
			*ret = val | (oct << off);
			return r;
		}

		/* 32-bit overflow ! */
		break;
	}

	return -1;
}

