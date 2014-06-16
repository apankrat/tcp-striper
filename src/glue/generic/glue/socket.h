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

#ifndef _SOCKET_H_tcpstriper_
#define _SOCKET_H_tcpstriper_

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
 *
 *		ip4_addr_t
 *
 * 		-- functions --
 *
 *		int sk_create(int family, int type, int protocol);
 *		int sk_shutdown(int sk);   // SD_WRITE, i.e. send FIN
 *		int sk_close(int sk);
 *
 *		int sk_bind(int sk, const sockaddr * addr, socklen_t alen);
 *
 *		int sk_connect(int sk, const sockaddr * a, socklen_t alen); 
 *
 *		int sk_listen(int sk, int backlog);
 *		int sk_accept(int sk, sockaddr * addr, socklen_t * alen);
 *
 *		int sk_getsockname(int sk, sockaddr * a, socklen_t * alen);
 *		int sk_getpeername(int sk, sockaddr * a, socklen_t * alen);
 *		
 *		int sk_recv(int sk, void * p, size_t n);
 *		int sk_recvfrom(int sk, void * p, size_t n, 
 *		                sockaddr * src, socklen_t * alen);
 *
 *		int sk_send(int sk, const void * p, size_t n);
 *		int sk_sendto(int sk, const void * p, size_t n, 
 *		              sockaddr * dst, socklen_t alen);
 *
 *		int sk_getsockopt(int sk, int level, int opt, 
 *		                  void * val, socklen_t vlen);
 *		int sk_setsockopt(int sk, int level, int opt, 
 *		                  const void * val, socklen_t vlen);
 *
 * 	Non-Posix extensions:
 *
 *		int sk_unblock(int sk);
 *
 *		int sk_errno();       // aka "fast", errno 
 *		int sk_error(int sk); // aka "slow", getsockopt(so_error)
 *
 * 		int sk_conn_fatal(int err); // boolean return
 * 		int sk_recv_fatal(int err);
 * 		int sk_send_fatal(int err);
 *
 *		int sk_conn_refused(int err);
 *		int sk_conn_timeout(int err);
 *
 * 		void  sockaddr_in_init(sockaddr_in &);
 *
 *		SOCKADDR_IN_ADDR(sa)
 *		SOCKADDR_IN_PORT(sa)
 *
 * 		htons, ntohs, htons_const
 * 		htonl, ntohl, htonl_const
 *
 *	Notes:
 *
 *		On windows - if the connection is aborted by peer (e.g. 
 *		via linger/close), and there's unread data in a socket 
 *		buffer, the socket will show as 'readable' when polled 
 *		for status, but recv() will fail with ECONNRESET.
 *
 *		So for the sake of portability, it should be assumed that 
 *		if a socket enters a 'reset' (or 'error') state, all its 
 *		buffers - both inbound and outbound - are cleared by the OS.
 */

#error Set your Include paths to use platform-specific version of this file

#endif

