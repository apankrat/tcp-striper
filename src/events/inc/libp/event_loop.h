/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_EVENT_LOOP_H_
#define _LIBP_EVENT_LOOP_H_

#include "libp/types.h"

/*
 *	For listening sockets:
 *
 *		SK_EV_readable - new incoming connection,
 *		                 sk_accept() can be called
 *
 *	For connecting sockets:
 *
 *		Windows: connected -> SK_EV_writable
 *		         failed    -> SK_EV_error, SO_ERROR is the reason
 *
 *		Linux:   connected -> SK_EV_writable, SO_ERROR is 0
 *		         connected -> SK_EV_writable, SO_ERROR is not 0
 *
 *	For connected sockets:
 *
 *		SK_EV_readable - new inbound data/eof,
 *		                 sk_recv() can be called
 *		SK_EV_writable - outbound buffer has space,
 *		                 sk_send() can be called
 *		SK_EV_error    - connection is gone due to
 *		                 timeout, reset, etc.
 *
 *	SK_EV_error is non-maskable, it's always monitored
 *	and reported.
 */
enum socket_event
{
	SK_EV_readable = 0x01,
	SK_EV_writable = 0x02,
	SK_EV_error    = 0x04
};

/*
 *	Your good old event loop.
 *
 *	Pass it sockets to be monitored for r/w/x events with add(), 
 *	then pull on monitor() to do the actual monitor for up to
 *	timeout_ms milliseconds and it will get you a callback if
 *	an event happens on a socket.
 *
 *	Use mod() to change the monitored event mask.
 *
 *	Use del() to remove the socket from the loop.
 */
typedef void (* event_loop_cb)(void * context, uint events);

typedef struct event_loop event_loop;

struct event_loop
{
	void (* add_socket)(event_loop * self, int sk, uint events,
	                    event_loop_cb cb, void * cb_context);

	void (* mod_socket)(event_loop * self, int sk, uint events);

	void (* del_socket)(event_loop * self, int sk);

	int  (* monitor)(event_loop * self, size_t timeout_ms);

	void (* discard)(event_loop * self);
};

/*
 *	Rudimentary select()-based implementation for now,
 *	epoll()-based one is coming up
 */
event_loop * new_event_loop_select();

#endif

