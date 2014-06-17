/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_ASSERT_H_
#define _LIBP_ASSERT_H_

#define assert(exp)  if (! (exp)) on_assert( #exp, __FILE__, __LINE__ )

/*
 *	The assert() handler.
 *	Defaults to fprintf(stderr) and abort().
 */
extern void (*on_assert)(const char * exp, const char * file, int line);

#endif

