#ifndef _SOCKET_UTILS_H_tcpstriper_
#define _SOCKET_UTILS_H_tcpstriper_

#include "glue/socket.h"

/*
 *	Some inlines so not to carry around
 *	
 *		... (sockaddr*)addr, sizeof(*addr)); 
 *
 *	when calling bind/connect/accept/etc.
 */
inline
int sk_bind_ip4(int sk, const sockaddr_in * addr)
{
	return sk_bind(sk, (sockaddr*)addr, sizeof(*addr));
}

inline
int sk_connect_ip4(int sk, const sockaddr_in * addr)
{
	return sk_connect(sk, (sockaddr*)addr, sizeof(*addr));
}

inline
int sk_accept_ip4(int sk, sockaddr_in * addr)
{
	socklen_t alen = sizeof(*addr);
	return sk_accept(sk, (sockaddr*)addr, &alen);
}

inline
int sk_getsockname_ip4(int sk, sockaddr_in * addr)
{
	socklen_t alen = sizeof(*addr);
	return sk_getsockname(sk, (sockaddr*)addr, &alen);
}

inline
int sk_getpeername_ip4(int sk, sockaddr_in * addr)
{
	socklen_t alen = sizeof(*addr);
	return getpeername(sk, (sockaddr*)addr, &alen);
}

/*
 *
 */
int sk_no_delay(int sk)
{
	static const int yes = 1;
	return sk_setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
}

#endif

