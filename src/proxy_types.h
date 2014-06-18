/*
 *	This file is a part of the tcp-striper project.
 *	Copyright (c) 2014 Alex Pankratov.
 *
 *	http://github.com/apankrat/tcp-striper
 */

/*
 *	The program is distributed under terms of BSD license.
 *	You can obtain the copy of the license by visiting:
 *
 *	http://www.opensource.org/licenses/bsd-license.php
 */

#ifndef _PROXY_TYPES_H_tcpstriper_
#define _PROXY_TYPES_H_tcpstriper_

#include "types.h"
#include "list.h"
#include "event_loop.h"
#include "data_buffer.h"
#include "socket.h"

/*
 *
 */
typedef struct connection    connection;
typedef struct bridge        bridge;
typedef struct proxy_config  proxy_config;
typedef struct proxy_state   proxy_state;

/*
 *
 */
struct connection
{
	bridge      * b;
	connection  * peer;
	const char  * name;

	size_t        recv_max;

	sockaddr_in   sa_peer;
	sockaddr_in   sa_self;
	int           sk;

	int           connecting;
	int           writable;
	int           readable;
	int           fin_rcvd;     /* peer sent FIN */
	int           fin_sent;     /* we sent FIN */

	data_buffer * pending;

	uint64_t      rx, tx;
	size_t        congestions;
};

struct bridge
{
	proxy_state * pxy;

	connection    c2p;
	connection    p2s;

	hlist_item    list; /* proxy_state.bridge */
};

bridge * alloc_bridge(proxy_state * pxy);
void     discard_bridge(bridge * b);

/*
 *
 */
struct proxy_config
{
	sockaddr_in  sa_listen;
	sockaddr_in  sa_server;
	int          backlog;

	size_t       c2p_buffer;
	size_t       p2s_buffer;

	size_t       max_per_cycle;
};

void init_proxy_config(proxy_config * conf);

/*
 *
 */
struct proxy_state
{
	proxy_config * conf;
	event_loop   * evl;

	int sk;                   /* listening on pxy_addr:pxy_port */

	hlist_head     bridges;
	data_buffer  * buf;       /* shared IO buffer */
};


void init_proxy_state(proxy_state * state, proxy_config * conf);


#endif

