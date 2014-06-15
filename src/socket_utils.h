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

inline
int sk_recv_ip4(int sk, void * buf, size_t len, sockaddr_in * src)
{
	socklen_t alen = sizeof(*src);
	return sk_recv(sk, buf, len, (sockaddr*)src, &alen);
}

inline
int sk_send_ip4(int sk, void * buf, size_t len, sockaddr_in * dst)
{
	return sk_send(sk, buf, len, (sockaddr*)dst, sizeof(*dst));
}

#endif

