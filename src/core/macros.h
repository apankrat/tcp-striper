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

#ifndef _MACROS_H_tcpstriper_
#define _MACROS_H_tcpstriper_

#include <stddef.h> /* offsetof */

/*
 *	Restore pointer to the structure by a pointer to its field
 */
#define structof(p,t,f) ((t*)(- offsetof(t,f) + (char*)(p)))

/*
 *
 */
#define sizeof_array(a) (sizeof(a) / sizeof(a[0]))

#endif

