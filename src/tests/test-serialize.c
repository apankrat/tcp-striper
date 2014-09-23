/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/macros.h"
#include "libp/io_serialize.h"
#include "libp/assert.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/time.h>

/*
 *
 */
uint64_t usec()
{
	struct timeval tv;
	struct timezone tz;
	uint64_t us;

	gettimeofday(&tv, &tz);

	us = tv.tv_sec;
	us *= 1000*1000;
	return us + tv.tv_usec;
}

int io_store_size_unrolled(uint8_t * buf, size_t len, size_t val);
int io_store_size_generic (uint8_t * buf, size_t len, size_t val);

int io_parse_size_unrolled(const uint8_t * buf, size_t len, size_t * ret);
int io_parse_size_generic (const uint8_t * buf, size_t len, size_t * ret);

int main(int argc, char ** argv)
{
	uint64_t t0, t1;
	uint8_t buf[2048];
	size_t  buf_fill;
	int i, j, r, pos;

	size_t samples[1024], v;

	srand48(usec());

	/*
	 *
	 */
	for (j=0; j<20; j++)
	{
		printf("io_store_size()...  ");

		for (i=0; i<sizeof_array(samples); i++)
			samples[i] = lrand48();

		t0 = usec();
		for (i=0; i<10000000; i++)
		{
			io_store_size(buf, sizeof(buf), samples[i & 1023]);
		}
		t1 = usec();
		printf("%llu usec\n", t1-t0);

		/*
		 *
		 */
		printf("io_parse_size()...  ");

		for (buf_fill=0; ;)
		{
			v = lrand48() & 0x0FFFFFFF;
			r = io_store_size(buf+buf_fill, sizeof(buf)-buf_fill, v);
			if (r < 0)
				break;
			buf_fill += r;
		}

		t0 = usec();
		for (i=0, pos=0; i<10000000; i++)
		{
			r = io_parse_size(buf + pos, buf_fill - pos, &v);
			pos += r;
			if (pos == buf_fill)
				pos = 0;
		}
		t1 = usec();
		printf("%llu usec\n", t1-t0);
	}

	return 0;
}

