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

#ifndef _ALLOC_H_tcpstriper_
#define _ALLOC_H_tcpstriper_

#include <stdlib.h>

void * heap_alloc(size_t size);
void * heap_realloc(void * ptr, size_t size);
void   heap_free(void * ptr);

#endif
