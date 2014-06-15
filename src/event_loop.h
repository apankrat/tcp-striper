#ifndef _EVENT_LOOP_H_tcpstriper_
#define _EVENT_LOOP_H_tcpstriper_

#include "types.h"

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
 *
 */
typedef void (* event_loop_cb)(void * context, uint events);

typedef struct event_loop  event_loop;

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
 *
 */
event_loop * new_event_loop_select();

#endif

