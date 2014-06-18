/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_SOCKET_H_linux_
#define _LIBP_SOCKET_H_linux_

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <linux/un.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

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
 *		int sk_create(int family, int type, int protocol);
 *		int sk_shutdown(int sk);   // SD_WRITE, i.e. send FIN
 *		int sk_close(int sk);
 */

static inline
int sk_create(int family, int type, int protocol)
{
	return socket(family, type, protocol);
}

static inline
int sk_shutdown(int sk)
{
	return shutdown(sk, SHUT_WR);
}

static inline
int sk_close(int sk)
{
	return close(sk);
}

 /*
  *		int sk_bind(int sk, const sockaddr * addr, socklen_t alen);
  *
  *		int sk_connect(int sk, const sockaddr * a, socklen_t alen);
  *
  *		int sk_listen(int sk, int backlog);
  *		int sk_accept(int sk, sockaddr * addr, socklen_t * alen);
  */
static inline
int sk_bind(int sk, const sockaddr * addr, socklen_t alen)
{
	return bind(sk, addr, alen);
}

static inline
int sk_connect(int sk, const sockaddr * a, socklen_t alen)
{
	return connect(sk, a, alen);
}

static inline
int sk_listen(int sk, int backlog)
{
	return listen(sk, backlog);
}

static inline
int sk_accept(int sk, sockaddr * addr, socklen_t * alen)
{
	return accept(sk, addr, alen);
}

 /*
  *		int sk_getsockname(int sk, sockaddr * a, socklen_t * alen);
  *		int sk_getpeername(int sk, sockaddr * a, socklen_t * alen);
  */

static inline
int sk_getsockname(int sk, sockaddr * a, socklen_t * alen)
{
	return getsockname(sk, a, alen);
}

static inline
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

static inline
int sk_recvfrom(int sk, void * buf, size_t len,
                sockaddr * src, socklen_t * srclen)
{
	int r;
	do { r = recvfrom(sk, buf, len, 0, src, srclen); }
	while (r < 0 && errno == EINTR);
	return r;
}

static inline
int sk_recv(int sk, void * buf, size_t len)
{
	return sk_recvfrom(sk, buf, len, NULL, NULL);
}

static inline
int sk_sendto(int sk, const void * buf, size_t len,
              sockaddr * dst, socklen_t dstlen)
{
	int r;
	do { r = sendto(sk, buf, len, 0, dst, dstlen); }
	while (r < 0 && errno == EINTR);
	return r;
}

static inline
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

static inline
int sk_getsockopt(int sk, int level, int opt,
                  void * val, socklen_t vlen)
{
	return getsockopt(sk, level, opt, val, &vlen);
}

static inline
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

static inline
int sk_unblock(int sk)
{
	int r = fcntl(sk, F_GETFL);
	return (r < 0) ? r : fcntl(sk, F_SETFL, r | O_NONBLOCK);
}

static inline
int sk_errno()
{
	return errno;
}

static inline
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

static inline
int sk_conn_fatal(int err)
{
	return (err != EINTR && err != EINPROGRESS);
}

static inline
int sk_recv_fatal(int err)
{
	return (err != EINTR && err != EAGAIN &&
	        err != ENOMEM && err != EWOULDBLOCK);
}

static inline
int sk_send_fatal(int err)
{
	return (err != EINTR && err != EAGAIN &&
	        err != ENOMEM && err != EWOULDBLOCK);
}

static inline
int sk_conn_refused(int err)
{
	return (err == ECONNREFUSED);
}

static inline
int sk_conn_timeout(int err)
{
	return (err == ETIMEDOUT);
}

/*
 * 		void  sockaddr_in_init(sockaddr_in &);
 *
 *		SOCKADDR_IN_ADDR(sa)
 *		SOCKADDR_IN_PORT(sa)
 */
static inline
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

