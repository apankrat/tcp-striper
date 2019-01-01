/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_SOCKET_H_windows_
#define _LIBP_SOCKET_H_windows_

#include "libp/types.h"
#include "libp/macros.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#define SHUT_RD    SD_RECEIVE  /* 0 */
#define SHUT_WR    SD_SEND     /* 1 */
#define SHUT_RDWR  SD_BOTH     /* 2 */

/*
 *	Posix-ish Socket API
 *
 * 		-- constants --
 *
 * 		AF_INET, SOCK_STREAM, SOCK_DGRAM
 *
 * 		SOL_SOCKET
 * 			SO_ERROR         int
 * 			SO_RCVBUF        int
 * 			SO_SNDBUF        int
 * 			SO_REUSEADDR     int
 *			SO_LINGER        linger
 *
 * 		IPPROTO_IP
 * 			IP_TTL           int
 *
 * 		IPPROTO_TCP
 * 			TCP_NODELAY      int
 *
 * 		IPPROTO_UDP
 * 			<none>
 *
 * 		-- types --
 *
 *		int (not socket_t) for a socket descriptor
 *
 *		socklen_t
 * 		sockaddr
 * 		sockaddr_in
 *		linger
 */
typedef struct sockaddr  sockaddr;
typedef struct sockaddr_in  sockaddr_in;

/*		ip4_addr_t
 *
 * 		-- functions --
 *
 *		int sk_init(); // platform-specific network init
 */
static_inline
int sk_init()
{
	static WSADATA wsa = { 0 };

	if (wsa.wVersion)
		return 0;

	return WSAStartup(MAKEWORD(2,0), &wsa);
}

/*
 *		int sk_create(int family, int type, int protocol);
 *		int sk_shutdown(int sk);   // SD_WRITE, i.e. send FIN
 *		int sk_close(int sk);
 */

static_inline
int sk_create(int family, int type, int protocol)
{
	return socket(family, type, protocol);
}

static_inline
int sk_shutdown(int sk)
{
	return shutdown(sk, SHUT_WR);
}

static_inline
int sk_close(int sk)
{
	return closesocket(sk); /* win32-specific */
}

 /*
  *		int sk_bind(int sk, const sockaddr * addr, socklen_t alen);
  *
  *		int sk_connect(int sk, const sockaddr * a, socklen_t alen);
  *
  *		int sk_listen(int sk, int backlog);
  *		int sk_accept(int sk, sockaddr * addr, socklen_t * alen);
  */
static_inline
int sk_bind(int sk, const sockaddr * addr, socklen_t alen)
{
	return bind(sk, addr, alen);
}

static_inline
int sk_connect(int sk, const sockaddr * a, socklen_t alen)
{
	return connect(sk, a, alen);
}

static_inline
int sk_listen(int sk, int backlog)
{
	return listen(sk, backlog);
}

static_inline
int sk_accept(int sk, sockaddr * addr, socklen_t * alen)
{
	return accept(sk, addr, alen);
}

 /*
  *		int sk_getsockname(int sk, sockaddr * a, socklen_t * alen);
  *		int sk_getpeername(int sk, sockaddr * a, socklen_t * alen);
  */

static_inline
int sk_getsockname(int sk, sockaddr * a, socklen_t * alen)
{
	return getsockname(sk, a, alen);
}

static_inline
int sk_getpeername(int sk, sockaddr * a, socklen_t * alen)
{
	return getpeername(sk, a, alen);
}

/*
 *		int sk_recv(int sk, void * p, size_t n);
 *
 *		int sk_recvfrom(int sk, void * p, size_t n,
 *		                sockaddr * src, socklen_t * srclen);
 *
 *		int sk_send(int sk, const void * p, size_t n);
 *
 *		int sk_sendto(int sk, const void * p, size_t n,
 *		              sockaddr * dst, socklen_t dstlen);
 */

static_inline
int sk_recvfrom(int sk, void * buf, size_t len,
                sockaddr * src, socklen_t * srclen)
{
	return recvfrom(sk, buf, len, 0, src, srclen);
}

static_inline
int sk_recv(int sk, void * buf, size_t len)
{
	return sk_recvfrom(sk, buf, len, NULL, NULL);
}

static_inline
int sk_sendto(int sk, const void * buf, size_t len,
              sockaddr * dst, socklen_t dstlen)
{
	return sendto(sk, buf, len, 0, dst, dstlen);
}

static_inline
int sk_send(int sk, const void * buf, size_t len)
{
	return sk_sendto(sk, buf, len, NULL, 0);
}

/*
 *		int sk_getsockopt(int sk, int level, int opt,
 *		                  void * val, socklen_t vlen);
 *		int sk_setsockopt(int sk, int level, int opt,
 *		                  const void * val, socklen_t vlen);
 */

static_inline
int sk_getsockopt(int sk, int level, int opt,
                  void * val, socklen_t vlen)
{
	return getsockopt(sk, level, opt, val, &vlen);
}

static_inline
int sk_setsockopt(int sk, int level, int opt,
                  const void * val, socklen_t vlen)
{
	return setsockopt(sk, level, opt, val, vlen);
}

/*
 * 	Non-Posix extensions:
 *
 *		int sk_unblock(int sk);
 *
 *		int sk_errno();       // aka "fast", errno
 *		int sk_error(int sk); // aka "slow", getsockopt(so_error)
 */

static_inline
int sk_unblock(int sk)
{
	static u_long unblock = 1;
	return ioctlsocket(sk, FIONBIO, &unblock);
}

static_inline
int sk_errno()
{
	return WSAGetLastError();
}

static_inline
int sk_error(int sk)
{
	int e;
	return (sk_getsockopt(sk, SOL_SOCKET, SO_ERROR, &e, sizeof e) < 0) ? -1 : e;
}

/*
 * 		int sk_conn_fatal(int err);   // boolean return
 * 		int sk_recv_fatal(int err);
 * 		int sk_send_fatal(int err);
 *
 *		int sk_conn_refused(int err);
 *		int sk_conn_timeout(int err);
 */

static_inline
int sk_conn_fatal(int err)
{
	return (err != WSAEINPROGRESS); // (err != EINTR && err != EINPROGRESS);
}

static_inline
int sk_recv_fatal(int err)
{
	return (err != WSAEWOULDBLOCK);
}

static_inline
int sk_send_fatal(int err)
{
	return (err != WSAEWOULDBLOCK);
}

static_inline
int sk_conn_refused(int err)
{
	return (err == WSAECONNREFUSED);
}

static_inline
int sk_conn_timeout(int err)
{
	return (err == WSAETIMEDOUT);
}

/*
 * 		void  sockaddr_in_init(sockaddr_in &);
 *
 *		SOCKADDR_IN_ADDR(sa)
 *		SOCKADDR_IN_PORT(sa)
 */
static_inline
void sockaddr_in_init(sockaddr_in * sa)
{
	static sockaddr_in sa_zero = { 0 };
	*sa = sa_zero;
	sa->sin_family = AF_INET;
}

#define SOCKADDR_IN_ADDR(sa) ( (sa)->sin_addr.s_addr )
#define SOCKADDR_IN_PORT(sa) ( (sa)->sin_port )

/*
 * 		htons, ntohs, htons_const
 * 		htonl, ntohl, htonl_const
 */

#endif

