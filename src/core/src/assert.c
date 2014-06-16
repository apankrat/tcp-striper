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

#include "assert.h"

#include <stdio.h>
#include <stdlib.h>

static
void log_and_die(const char * exp, const char * file, int line)
{
	fprintf(stderr, "assert(%s) failed in %s, line %d\n",
		exp, file, line);
	abort();
}

void (*on_assert)(const char *, const char *, int) = log_and_die;
