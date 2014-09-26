/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_IO_PIPE_MISC_H_
#define _LIBP_IO_PIPE_MISC_H_

#include "libp/io_pipe.h"
#include <stdio.h>

/*
 *	io_pipe state as bitmask
 */
static inline
uint pipe_get_state(const io_pipe * p)
{
	return ((p->ready    <<  0) & 0x01) |
	       ((p->broken   <<  1) & 0x02) |
	       ((p->readable <<  2) & 0x04) |
	       ((p->writable <<  4) & 0x08) |
	       ((p->fin_sent <<  8) & 0x10) |
	       ((p->fin_rcvd << 16) & 0x20);
}

static inline
void pipe_clone_state(io_pipe * dst, const io_pipe * src)
{
	dst->ready    = src->ready;
	dst->broken   = src->broken;
	dst->readable = src->readable;
	dst->writable = src->writable;
	dst->fin_sent = src->fin_sent;
	dst->fin_rcvd = src->fin_rcvd;
}

static inline
void pipe_tag_as_broken(io_pipe * p)
{
	p->broken = 1;

	/* be pedantic */
	p->readable = 0;
	p->writable = 0;
}

#endif
