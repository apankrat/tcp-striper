/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/event_loop.h"
#include "libp/io_pipe.h"
#include "libp/io_bridge.h"
#include "libp/socket.h"
#include "libp/socket_utils.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#include "libp/termio.h"

/*
 *
 */
int enough = 0;

void on_bridge_down(void * context, int graceful)
{
	printf("\n-- bridge_down, graceful = %d --\n", graceful);
	enough = 1;
}

int main(int argc, char ** argv)
{
	event_loop * evl;
	io_pipe * io_c2p;
	io_pipe * io_p2s;
	io_bridge * br;
	sockaddr_in sa;
	char buf[128];
	int sk, c2p, p2s;
	int yes = 1;

	//
	signal(SIGPIPE, SIG_IGN);

	//
	evl = new_event_loop_select();

	//
	sk = sk_create(AF_INET, SOCK_STREAM, 0);
	if (sk < 0)
		return 1;

	sockaddr_in_init(&sa);
	SOCKADDR_IN_PORT(&sa) = htons(55555);

	if (sk_setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) < 0 ||
	    sk_bind_ip4(sk, &sa) < 0 ||
	    sk_listen(sk, 8) < 0)
		return 2;

	printf("listening on %s ...\n", sa_to_str(&sa, buf, sizeof buf));

	//
	c2p = sk_accept_ip4(sk, &sa);
	if (c2p < 0)
		return 3;
	
	printf("accepted\n");
	sk_unblock(c2p);

	//
	SOCKADDR_IN_ADDR(&sa) = inet_addr(argc > 1 ? argv[1] : "127.0.0.1");
	SOCKADDR_IN_PORT(&sa) = htons( (argc > 2) ? atoi(argv[2]) : 22 );
	
	p2s = sk_create(AF_INET, SOCK_STREAM, 0);
	if (p2s < 0)
		return 4;

	if (sk_unblock(p2s) < 0)
		return 5;
	
	printf("connecting to %s ...\n", sa_to_str(&sa, buf, sizeof buf));

	if (sk_connect_ip4(p2s, &sa) < 0 &&
	    sk_conn_fatal(sk_errno(p2s)))
	    	return 6;

	//
	io_c2p = new_tcp_pipe(c2p);
	io_p2s = new_tcp_pipe(p2s);

io_c2p = new_atx_pipe(io_c2p);
io_p2s = new_atx_pipe(io_p2s);

	br = new_io_bridge(io_c2p, io_p2s);
	br->on_shutdown = on_bridge_down;

	br->init(br, evl);

	while (! enough)
	{
		static uint tick = 0;
		evl->monitor(evl, 100);
		
		if ( (tick++ % 17) && ! enough )
			continue;

		printf("\r%u  |  [%c%c %10llu %10llu %4u] [%c%c %10llu %10llu %4u]", 
			tick++, 
			br->l->pipe->writable ? 'w' :
			br->l->pipe->fin_sent ? 'x' : '-',
			br->l->pipe->readable ? 'r' : 
			br->l->pipe->fin_rcvd ? 'x' : '-',
			br->l->tx, br->l->rx, br->l->congestions,
			br->r->pipe->writable ? 'w' : 
			br->r->pipe->fin_sent ? 'x' : '-',
			br->r->pipe->readable ? 'r' : 
			br->r->pipe->fin_rcvd ? 'x' : '-',
			br->r->tx, br->r->rx, br->r->congestions);

		fflush(stdout);
	}

	printf("\n");

	return 0;
}

