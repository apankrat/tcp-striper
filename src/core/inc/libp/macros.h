/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_MACROS_H_
#define _LIBP_MACROS_H_

#include <stddef.h> /* offsetof */

/*
 *	Restore pointer to the structure by a pointer to its field
 */
#define struct_of(p,t,f) ((t*)(- offsetof(t,f) + (char*)(p)))

/*
 *
 */
#define sizeof_array(a) (sizeof(a) / sizeof(a[0]))

#endif

