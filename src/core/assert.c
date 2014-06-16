#include "assert.h"
#include <stdio.h>
#include <stdlib.h>

void on_assert(const char * exp, const char * file, int line)
{
	fprintf(stderr, "assert(%s) failed in %s, line %d\n",
		exp, file, line);
	abort();
}
