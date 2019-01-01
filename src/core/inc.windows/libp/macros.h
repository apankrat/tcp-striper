/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_MACROS_H_windows_
#define _LIBP_MACROS_H_windows_

#include "../../inc/libp/macros.h"

/*
 *	re-define static_inline
 */
#undef  static_inline
#define static_inline  static __inline

#endif
