/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_TERMIO_H_
#define _LIBP_TERMIO_H_

int term_set_buffering(int fd, int enable);
int term_set_echo(int fd, int enable);

#endif

