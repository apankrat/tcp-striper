/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/event_loop.h"
#include "libp/io_pipe.h"
#include "libp/socket.h"
#include "libp/socket_utils.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "libp/termio.h"

/*
 *
 */
int enough = 0;

void on_pipe_event(void * context, uint events)
{
	printf("pipe event = %c%c%c\n",
		(events & SK_EV_readable) ? 'R' : '-',
		(events & SK_EV_writable) ? 'W' : '-',
		(events & SK_EV_error)    ? 'E' : '-');
	enough = 1;
}

int main(int argc, char ** argv)
{
	event_loop * evl;
	io_pipe * io;
	sockaddr_in sa;
	int sk;

	//
	evl = new_event_loop_select();

	//
	sk = sk_create(AF_INET, SOCK_STREAM, 0);
	if (sk < 0)
		return 1;

	sockaddr_in_init(&sa);
	SOCKADDR_IN_ADDR(&sa) = inet_addr("64.34.106.10");
//	SOCKADDR_IN_ADDR(&sa) = inet_addr("127.0.0.1");
	SOCKADDR_IN_PORT(&sa) = htons(22);

	sk_unblock(sk);

	if (sk_connect_ip4(sk, &sa) < 0 &&
	    sk_conn_fatal(sk_errno(sk)))
	    	return 2;

	//
	io = new_tcp_pipe(sk);

	io->on_activity = on_pipe_event;
	io->on_context = io;

	io->init(io, evl);

	while (! enough)
	{
		evl->monitor(evl, 1000);
		printf("tick\n");
	}

	return 0;
}

