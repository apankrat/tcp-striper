#include "event_loop.h"
#include "list.h"
#include "assert.h"
#include "alloc.h"

#include <stdio.h>

#include "glue/socket.h"
#include "socket_utils.h"

/*
 *
 */
struct proxy_config
{
	sockaddr_in  sa_listen;
	sockaddr_in  sa_server;
	int          backlog;
};

typedef struct proxy_config proxy_config;

void init_proxy_config(proxy_config * conf)
{
	sockaddr_in_init(&conf->sa_listen);
	SOCKADDR_IN_ADDR(&conf->sa_listen) = 0;
	SOCKADDR_IN_PORT(&conf->sa_listen) = htons(55555);

	sockaddr_in_init(&conf->sa_server);
	SOCKADDR_IN_ADDR(&conf->sa_server) = inet_addr("127.0.0.1");
	SOCKADDR_IN_PORT(&conf->sa_server) = htons(22);

	conf->backlog = 8;
}

/*
 *
 */
struct proxy_state
{
	proxy_config * conf;
	event_loop   * evl;

	int sk; /* listening on pxy_addr:pxy_port */

	hlist_head  bridges;
};

typedef struct proxy_state proxy_state;

/*
 *
 */
struct connection
{
	struct bridge * b;
	
	sockaddr_in  peer;
	sockaddr_in  self;
	int          sk;

	int  connecting;
	int  writable;
	int  readable;
};

typedef struct connection connection;

/*
 *
 */
struct bridge
{
	proxy_state * pxy;

	connection    c2p;
	connection    p2s;

	hlist_item    list; /* proxy_state.bridge */
};

typedef struct bridge bridge;

/*
 *
 */
void init_connection(connection * c, struct bridge * b)
{
	c->b = b;
	sockaddr_in_init(&c->peer);
	sockaddr_in_init(&c->self);
	c->sk = -1;
	c->connecting = 0;
	c->writable = 0;
	c->readable = 0;
}

void term_connection(connection * c)
{
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
bridge * alloc_bridge()
{
	bridge * b = heap_alloc(sizeof *b);

	b->pxy = NULL;
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
void on_can_accept(void * ctx, uint events);
void on_p2s_connected(void * ctx, uint events);
void activate_bridge(bridge * b);

void on_conn_activity(void * ctx, uint events);

/*
 *
 */
void on_can_accept(void * ctx, uint events)
{
	proxy_state * pxy = (proxy_state*)ctx;
	bridge * b;

printf("Can accept\n");

	b = alloc_bridge();

	b->c2p.sk = sk_accept_ip4(pxy->sk, &b->c2p.peer);
	if (b->c2p.sk < 0)
		goto err;

printf("Accepted\n");

	b->p2s.sk = sk_create(AF_INET, SOCK_STREAM, 0);
	if (b->p2s.sk < 0)
		goto err;

	if (sk_unblock(b->p2s.sk) < 0)
		goto err;

	if (sk_connect_ip4(b->p2s.sk, &pxy->conf->sa_server) < 0 && 
	    sk_conn_fatal(sk_errno(b->p2s.sk)))
	    	goto err;

printf("Connecting p2s...\n");

	/* connected or in-progress */
	b->p2s.connecting = 1;
	
	b->pxy = pxy;
	hlist_add_front(&pxy->bridges, &b->list);

	pxy->evl->add_socket(pxy->evl, 
		b->p2s.sk, SK_EV_writable,
		on_p2s_connected, &b->p2s);

	return;

err:
	discard_bridge(b);
}

void on_p2s_connected(void * ctx, uint events)
{
	connection * conn = (connection *)ctx;
	int err;

	assert(conn->connecting);
	assert(conn == &conn->b->p2s);

	err = sk_error(conn->sk);
printf("Connection completed with %d\n", err);
	if ( (events & SK_EV_error) ||
	     (events & SK_EV_writable) && (err != 0) )
	{
		/* connection failed */
		discard_bridge(conn->b);
		return;
	}

	assert(events & SK_EV_writable);

	conn->connecting = 0;
	conn->writable = 1;
	conn->b->pxy->evl->del_socket(conn->b->pxy->evl, conn->sk);

	activate_bridge(conn->b);
}

void activate_bridge(bridge * b)
{
	b->pxy->evl->add_socket(b->pxy->evl,
		b->c2p.sk, SK_EV_readable | SK_EV_writable,
		on_conn_activity, &b->c2p);

	b->pxy->evl->add_socket(b->pxy->evl,
		b->p2s.sk, SK_EV_readable,
		on_conn_activity, &b->p2s);

printf("Bridge activated\n");
}


void on_conn_activity(void * ctx, uint events)
{
	connection * conn = (connection *)ctx;
const char * name = (conn == &conn->b->c2p) ? "c2p" : "p2s";

	if (events & SK_EV_writable)
	{
printf("%s connection is writable\n", name);
		conn->writable = 1;
		conn->b->pxy->evl->mod_socket(conn->b->pxy->evl, conn->sk, SK_EV_readable);
	}

	if (events & SK_EV_readable)
	{
printf("%s connection is readable\n", name);
		conn->readable = 1;
		conn->b->pxy->evl->mod_socket(conn->b->pxy->evl, conn->sk, 0);
	}
}



int main(int argc, char ** argv)
{
	proxy_config conf;
	proxy_state state;
	int yes = 1;

	init_proxy_config(&conf);

	state.conf = &conf;
	state.evl = new_event_loop_select();

	//
	state.sk = sk_create(AF_INET, SOCK_STREAM, 0);
	if (state.sk < 0)
		return 1;

	sk_unblock(state.sk);

	if (sk_setsockopt(state.sk, 
		SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) < 0)
		return 2;

	if (sk_bind_ip4(state.sk, &conf.sa_listen) < 0)
		return 3;
	
	if (sk_listen(state.sk, conf.backlog) < 0)
		return 4;

	state.evl->add_socket(state.evl,
		state.sk, SK_EV_readable,
		on_can_accept, &state);

	while (1)
	{
		state.evl->monitor(state.evl, 1000);
		printf("tick\n");
	}

	return 0;
}

