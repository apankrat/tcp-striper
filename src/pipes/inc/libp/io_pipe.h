/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_IO_PIPE_H_
#define _LIBP_IO_PIPE_H_

#include "libp/event_loop.h"

/*
 *	io_pipe is an abstraction of TCP connection that follows 
 *	its send/recv/shutdown semantics. It is used to extend 
 *	send/recv behavior and transparently implement things 
 *	like atomic send, datagram framing, stream encryption,
 *	compression and so on.
 *
 *	The best part is that io_pipes are stackable, meaning
 *	that it's possible to create compressed datagram TCP
 *	pipe by chaining 'datagram', 'compress' and 'tcp'
 *	pipes together.
 *
 *	--
 *
 *	Each pipe instance has 4 API functions - send, recv,
 *	shutdown and discard. The app uses these to ... well
 *	... write/read data to/from the pipe, send an "EOF"
 *	to the peer and to dispose of the pipe instance.
 *
 *	If a pipe operates with actual network sockets, it
 *	is attached to an event_loop instance at creation.
 *	As such, when the event_loop detects activity on
 *	pipe's sockets, pipe will get a callback, which it
 *	will process and then pass on to the app in a form
 *	on on_activity() or on_connected() callback.
 *
 *	--
 *
 *	on_activity() callback is issued as follows:
 *
 *	   SK_EV_readable - if there's pending data to be
 *	                    read or a pending EOF.
 *
 *	   SK_EV_writable - if it's possible to write into
 *	                    the pipe, including completion
 *	                    of a pending connect() call
 *
 *	   SK_EV_error    - if the pipe becomes broken and
 *	                    can no longer be used for I/O,
 *	                    or if pending connect() failed
 *
 *	These events are level-triggered, meaning that being 
 *	"readable" is a pipe *state* rather than a one-off 
 *	event. Monitoring for readability of an already 
 *	readable pipe will result in on_activity() callback 
 *	on every event_loop cycle.
 *
 *	--
 *
 *	In many cases a pipe may NOT have an actual socket.
 *	Instead it will use another pipe to send/recv data
 *	to augment IO semantics in some way. The on_activity
 *	callbacks from this kind of pipe will be driven by
 *	on_activity callbacks issued by the underlying pipe
 *	rather than the event_loop directly.
 *
 *	--
 *
 *	It is guaranteed that callbacks are NEVER invoked
 *	from a pipe's API call. In other words, the io_pipe
 *	instance is guaranteed to still exist after a call 
 *	to send/recv/shutdown returns. These calls do not
 *	recurse back into the app code via callbacks.
 *
 *	--
 *
 *	The status flags are managed by the pipe instance
 *	and are meant for read-only consumption by the app.
 *
 *	When a send() call encounters a failure or a partial 
 *	write due to congestion, the pipe will clear 'writable' 
 *	and start monitoring connection for becoming writable 
 *	again. When it happens, the pipe will set 'writable' 
 *	flag and issue on_activity(SK_EV_writable) callback.
 *
 *	Similar logic applies to recv() calls that cannot 
 *	fetch any data because the data is not there. The
 *	pipe will clear 'readable' flag and start monitoring
 *	for incoming data. If it receives data, it sets the
 *	'readable' flag and issues on_activity(SK_EV_readable).
 *
 *	It may also receive an EOF (end of file) if its peer
 *	issues 'shutdown' call. In this case the pipe will
 *	set 'fin_rcvd', issue on_activity(SK_EV_readable) and
 *	the next call to recv() will return 0, which is
 *	consistent with standard POSIX recv() semantics.
 *
 */
typedef struct io_pipe io_pipe;

struct io_pipe
{
	/* state */
	int  writable : 1;
	int  readable : 1;
	int  fin_rcvd : 1; /* got FIN from the peer */
	int  fin_sent : 1; /* sent FIN to the peer  */
	
	/* api */
	void (* init)(io_pipe * self, event_loop * evl);

	int  (* recv)(io_pipe * self, void * buf, size_t len, int * fatal);
	int  (* send)(io_pipe * self, const void * buf, size_t len, int * fatal);
	int  (* shutdown)(io_pipe * self);
	
	void (* discard)(io_pipe * self);

	/* callbacks */
	void (* on_activity) (void * context, uint events);
	void  * on_context;
};

/*
 *
 */
io_pipe * new_tcp_pipe(int sk);

#endif

