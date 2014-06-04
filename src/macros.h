#ifndef _MACROS_H_tcpstriper_
#define _MACROS_H_tcpstriper_

#include <stddef.h> /* offsetof */

/*
 *	Restore pointer to the structure 
 *	by a pointer to its field
 */
#define structof(p,t,f) ((t*)(- offsetof(t,f) + (char*)(p)))

/*
 *
 */
#define sizeof_array(a) (sizeof(a) / sizeof(a[0]))

#endif

