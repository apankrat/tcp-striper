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
 *	-- In short --
 *
 *	io_pipe is a duplex, reliable, ordered communication
 *	channel based on the TCP socket semantics.
 *
 *	-- Sending/receiving --
 *
 *	Each pipe has two buffers, one for the inbound data (RX) 
 *	and another for the outbound data (TX).
 *
 *	When there's data in the RX buffer, pipe->readable is
 *	set to 1 and recv() can be used to retrieve the data.
 *	When the buffer is exhausted, recv() will return -1 
 *	and 'readable' will be cleared.
 *
 *	When there's space in the TX buffer, pipe->writable is 
 *	set to 1 and send() is likely to send at least some 
 *	of the data passed to it. When TX buffer gets full,
 *	send() will return either -1 or less than it was asked 
 *	to send and 'writable' will be cleared.
 *
 *	To tell apart fatal and EAGAIN failures with send/recv,
 *	calling code should look at the pipe->broken flag as 
 *	described below.
 *
 *	-- Events --
 *
 *	When a TX buffer congestion clears or new data arrives
 *	in the RX buffer, pipe sets respectively 'writable' or
 *	'readable' flag and issues the on_activity() callback
 *	to the application.
 *
 *	The way the pipe itself learns about these events is
 *	through a callback from an event_loop or through an
 *	on_activity() callback from another pipe that it may
 *	be using for the actual I/O.
 *
 *	In practice it means that if for example the pipe got 
 *	congested, the app should simply be ticking its event 
 *	loop and it will receive on_activity() callback with
 *	IO_EV_writable once the pipe is writable again.
 *
 *	-- FIN --
 *
 *	Pipes support FIN/EOF semantics of TCP sockets. That is
 *	calling send_fin() will close the pipe for writing and
 *	will cause recv() on the peer's end return with 0 after
 *	it retrieves all other incoming data.
 *
 *	When FIN is retrieved with recv(), the 'readable' bit is 
 *	cleared and the 'fin_rcvd' bit is set.
 *
 *	Similarly, calling send_fin() clears 'writable' bit and 
 *	raises the 'fin_sent' bit.
 *
 *	HOWEVER! send_fin() may complete asynchronously if FIN
 *	cannot be sent right away. In this case send_fin() will 
 *	return -1 and the app will get on_activity() callback
 *	with IO_EV_fin_sent once the FIN does go out.
 *
 *	-- Ready state  --
 *
 *	Lastly, there are two bits that describe general state
 *	of the pipe - 'ready' and 'broken'.
 *
 *	Each pipe starts its life with both bits cleared. Once
 *	it gets into a functional state by completing, for 
 *	example, its connection handshake, it sets the 'ready' 
 *	flag and issues on_activity() callback with IO_EV_ready. 
 *
 *	-- Error state --
 *
 *	If pipe encounters a fatal error, it sets 'broken' flag.
 *
 *	If this happens while executing recv/send/send_fin call, 
 *	it simply sets 'broken' flag and returns -1.
 *	
 *	If this happens when processing an event loop callback, 
 *	pipe will set the flag and issue on_activity() callback
 *	with IO_EV_broken.
 *
 *	-- The callback --
 *
 *	To recap - the on_activity() callback is issued by the 
 *	pipe when a respective state bit is changed from 0 to 1. 
 *
 */
typedef struct io_pipe io_pipe;

enum io_event
{
	IO_EV_ready    = 0x01,
	IO_EV_broken   = 0x02,
	IO_EV_readable = 0x04,
	IO_EV_writable = 0x08,
	IO_EV_fin_sent = 0x10
};

struct io_pipe
{
	/* General state */
	int  ready    : 1;
	int  broken   : 1;

	/* RX state */
	int  readable : 1;
	int  fin_rcvd : 1;

	/* TX state */
	int  writable : 1;
	int  fin_sent : 1;

	/* The API */
	void (* init)(io_pipe * p, event_loop * evl);

	int  (* recv)(io_pipe * p, void * buf, size_t len);
	int  (* send)(io_pipe * p, const void * buf, size_t len);
	int  (* send_fin)(io_pipe * p);

	void (* discard)(io_pipe * p);

	/* The callback */
	void (* on_activity)(void * context, uint io_event_mask);
	void  * on_context;
};

/*
 *	TCP socket wrapper
 */
io_pipe * new_tcp_pipe(int sk);

/*
 *	Atomic-send pipe
 *
 *	send() either accepts whole packet or fails with -1
 */
io_pipe * new_atx_pipe(io_pipe * io);

/*
 *	Datagram pipe
 *
 *	send() expects each blob of data passed to it to be 
 *	in <size><payload> format, whereby <size> is packed 
 *	with io_store_size()
 *
 *	recv() received and parses <size> first, validates 
 *	it against maximum datagram size and then proceeds
 *	to reassemble the datagram and return it to the 
 *	calling code (as <size><payload>).
 *
 *	If reassembled packet is too big to fit into the 
 *	buffer passed to recv(), it's a fatal error.
 */
io_pipe * new_dgm_pipe(io_pipe * io, size_t max_size);

#endif

