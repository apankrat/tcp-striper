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

#include "alloc.h"
#include <stdlib.h>

void * heap_alloc(size_t n)
{
	return malloc(n);
}

void heap_free(void * p)
{
	free(p);
}

