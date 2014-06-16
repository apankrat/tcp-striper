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

#include "proxy_types.h"

#include "assert.h"
#include "alloc.h"

/*
 *
 */
static
void init_connection(connection * c, bridge * b)
{
	proxy_config * conf = b->pxy->conf;
	int c2p = (c == &b->c2p);

	c->b = b;
	c->peer = c2p ? &b->p2s : &b->c2p;
	c->name = (c == &b->c2p) ? "c2p" : "p2s";

	c->recv_max = c2p ? conf->c2p_buffer : conf->p2s_buffer;
	sockaddr_in_init(&c->sa_peer);
	sockaddr_in_init(&c->sa_self);
	c->sk = -1;
	c->connecting = 0;
	c->writable = 0;
	c->readable = 0;
	c->fin_rcvd = 0;
	c->fin_sent = 0;
	c->pending = NULL;

	c->rx = 0;
	c->tx = 0;
	c->congestions = 0;
}

static
void term_connection(connection * c)
{
	if (c->pending)
		free_data_buffer(c->pending);

	if (c->sk == -1)
		return;

	assert(c->b);

	if (c->b->pxy && c->b->pxy->evl)
		c->b->pxy->evl->del_socket(c->b->pxy->evl, c->sk);

	sk_close(c->sk);
	c->sk = -1;
}

/*
 *
 */
bridge * alloc_bridge(proxy_state * pxy)
{
	bridge * b = heap_alloc(sizeof *b);

	b->pxy = pxy;
	
	init_connection(&b->c2p, b);
	init_connection(&b->p2s, b);

	hlist_init_item(&b->list);

	return b;
}

void discard_bridge(bridge * b)
{
	term_connection(&b->c2p);
	term_connection(&b->p2s);
	hlist_del(&b->list);
	heap_free(b);
}

/*
 *
 */
void init_proxy_config(proxy_config * conf)
{
	sockaddr_in_init(&conf->sa_listen);
	SOCKADDR_IN_ADDR(&conf->sa_listen) = 0;
	SOCKADDR_IN_PORT(&conf->sa_listen) = htons(55555);

	sockaddr_in_init(&conf->sa_server);
	SOCKADDR_IN_ADDR(&conf->sa_server) = inet_addr("127.0.0.1");
	SOCKADDR_IN_PORT(&conf->sa_server) = htons(22);

	conf->backlog = 8;

	conf->c2p_buffer = 1024*1024;
	conf->p2s_buffer = 1024*1024;

	conf->max_per_cycle = 16*1024*1024;
}

/*
 *
 */
void init_proxy_state(proxy_state * state, proxy_config * conf)
{
	size_t capacity;
	
	capacity = (conf->c2p_buffer < conf->p2s_buffer) ? 
		conf->p2s_buffer : conf->c2p_buffer;
	
	state->conf = conf;
	state->evl = new_event_loop_select();
	state->buf = alloc_data_buffer(capacity);

	hlist_init(&state->bridges);
}


