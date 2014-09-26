/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_MACROS_H_linux_
#define _LIBP_MACROS_H_linux_

/*
 *	Borrow everything from the generic version ...
 */
#include "../../inc/libp/macros.h"

/*
 *	... except for this
 */
#undef  __max
#define __max(a, b)                  \
	({                           \
		typeof(a) aa = (a);  \
		typeof(b) bb = (b);  \
		(aa < bb) ? bb : aa  \
	})

#endif

