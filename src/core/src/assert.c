/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/assert.h"

#include <stdio.h>
#include <stdlib.h>

static
void log_and_abort(const char * exp, const char * file, int line)
{
	fprintf(stderr, "assert(%s) failed in %s, line %d\n",
		exp, file, line);
	abort();
}

void (*on_assert)(const char *, const char *, int) = log_and_abort;

