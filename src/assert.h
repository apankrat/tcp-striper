#ifndef _ASSERT_H_tcpstriper_
#define _ASSERT_H_tcpstriper_

#define assert(exp)  if (! (exp)) on_assert( #exp, __FILE__, __LINE__ )

void on_assert(const char * exp, const char * file, int line);

#endif
