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

#ifndef _SOCKET_UTILS_H_tcpstriper_
#define _SOCKET_UTILS_H_tcpstriper_

#include "socket.h"

/*
 *	Some inlines so not to carry around
 *	
 *		... (sockaddr*)addr, sizeof(*addr)); 
 *
 *	when calling bind/connect/accept/etc.
 */
static inline
int sk_bind_ip4(int sk, const sockaddr_in * addr)
{
	return sk_bind(sk, (sockaddr*)addr, sizeof(*addr));
}

static inline
int sk_connect_ip4(int sk, const sockaddr_in * addr)
{
	return sk_connect(sk, (sockaddr*)addr, sizeof(*addr));
}

static inline
int sk_accept_ip4(int sk, sockaddr_in * addr)
{
	socklen_t alen = sizeof(*addr);
	return sk_accept(sk, (sockaddr*)addr, &alen);
}

static inline
int sk_getsockname_ip4(int sk, sockaddr_in * addr)
{
	socklen_t alen = sizeof(*addr);
	return sk_getsockname(sk, (sockaddr*)addr, &alen);
}

static inline
int sk_getpeername_ip4(int sk, sockaddr_in * addr)
{
	socklen_t alen = sizeof(*addr);
	return getpeername(sk, (sockaddr*)addr, &alen);
}

/*
 *
 */
static inline
int sk_no_delay(int sk)
{
	static const int yes = 1;
	return sk_setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
}

/*
 *
 */
const char * sa_to_str(const sockaddr_in * sa, char * buf, size_t max);


#endif

