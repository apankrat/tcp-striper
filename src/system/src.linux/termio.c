/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/termio.h"
#include <termios.h>

/*
 *
 */
static
int term_mod_lflag(int fd, int mask, int bits)
{
	struct termios tio;

	if (tcgetattr(fd, &tio) < 0)
		return -1;

	tio.c_lflag &= ~mask;
	tio.c_lflag |= bits;

	return tcsetattr(fd, TCSANOW, &tio);
}

/*
 *
 */
int term_set_buffering(int fd, int enable)
{
	return term_mod_lflag(fd, ICANON, enable ? ICANON : 0);
}

int term_set_echo(int fd, int enable)
{
	return term_mod_lflag(fd, ECHO, enable ? ECHO : 0);
}

