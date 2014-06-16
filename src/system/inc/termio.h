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

#ifndef _TERMIO_H_tcpstriper_
#define _TERMIO_H_tcpstriper_

int term_set_buffering(int fd, int enable);
int term_set_echo(int fd, int enable);

#endif

