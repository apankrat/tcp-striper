#include "event_loop.h"
#include "list.h"
#include "assert.h"
#include "alloc.h"
#include "macros.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>

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

	size_t       c2p_buffer;
	size_t       p2s_buffer;

	size_t       max_per_cycle;
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

	conf->c2p_buffer = 1024;
	conf->p2s_buffer = 1024;

	conf->max_per_cycle = 1024*1024;
}

/*
 *
 */
struct data_buffer
{
	size_t capacity;
	size_t size;
	char   data[1];
};

typedef struct data_buffer data_buffer;

data_buffer * alloc_data_buffer(size_t capacity)
{
	data_buffer * buf;
	size_t need;

	need = sizeof(data_buffer) - 1 + capacity;
	buf = (data_buffer *)heap_alloc(need);

	buf->capacity = capacity;
	buf->size = 0;
	return buf;
}

data_buffer * init_data_buffer(size_t capacity, void * data, size_t size)
{
	data_buffer * buf;
	
	assert(data && size && capacity >= size);

	buf = alloc_data_buffer(capacity);
	buf->size = size;
	memcpy(buf->data, data, size);

	return buf;
}

void free_data_buffer(data_buffer * buf)
{
	heap_free(buf);
}

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

typedef struct proxy_state proxy_state;

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

/*
 *
 */
struct connection
{
	struct bridge * b;
	struct connection * peer;

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
	const char  * name;
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
	proxy_config * conf = b->pxy->conf;
	int c2p = (c == &b->c2p);

	c->b = b;
	c->peer = c2p ? &b->p2s : &b->c2p;
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
	c->name = (c == &b->c2p) ? "c2p" : "p2s";
}

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
printf("Bridge %p discarded\n", b);
}

/*
 *
 */
void on_can_accept(void * ctx, uint events);
void on_p2s_connected(void * ctx, uint events);
void activate_bridge(bridge * b);

void on_conn_activity(void * ctx, uint events);
void adjust_events_mask(connection * conn);
void relay_data(connection * src, connection * dst);
void send_pending(connection * conn);
void send_fin(connection * conn);
void on_recv_failed(connection * conn);
void on_send_failed(connection * conn);
void on_conn_eof(connection * conn);

/*
 *
 */
int sk_no_delay(int sk)
{
	static const int yes = 1;
	return sk_setsockopt(sk, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
}

void on_can_accept(void * ctx, uint events)
{
	proxy_state * pxy = (proxy_state*)ctx;
	bridge * b;

printf("Can accept\n");

	b = alloc_bridge(pxy);

	b->c2p.sk = sk_accept_ip4(pxy->sk, &b->c2p.sa_peer);
	if (b->c2p.sk < 0)
		goto err;
	
	if (sk_unblock(b->c2p.sk) < 0)
		goto err;

	if (sk_no_delay(b->c2p.sk) < 0)
		goto err;

printf("Accepted\n");

	b->p2s.sk = sk_create(AF_INET, SOCK_STREAM, 0);
	if (b->p2s.sk < 0)
		goto err;

	if (sk_unblock(b->p2s.sk) < 0)
		goto err;

	if (sk_no_delay(b->p2s.sk) < 0)
		goto err;

	if (sk_connect_ip4(b->p2s.sk, &pxy->conf->sa_server) < 0 && 
	    sk_conn_fatal(sk_errno(b->p2s.sk)))
	    	goto err;

printf("Connecting p2s...\n");

	/* connected or in-progress */
	b->p2s.connecting = 1;
	
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
	if (  (events & SK_EV_error) ||
	     ((events & SK_EV_writable) && (err != 0)) )
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

	if (events & SK_EV_writable)
	{
printf("%s: writable\n", conn->name);
		if (! conn->pending)
		{
			conn->writable = 1;
			adjust_events_mask(conn);
		}
		else
		{
			send_pending(conn);
		}

		if (conn->writable && conn->peer->readable)
			relay_data(conn->peer, conn);
	}

	if (events & SK_EV_readable)
	{
printf("%s: readable\n", conn->name);
		conn->readable = 1;
		adjust_events_mask(conn);

		if (conn->peer->writable)
			relay_data(conn, conn->peer);
	}
}

void adjust_events_mask(connection * conn)
{
	event_loop * evl = conn->b->pxy->evl;
	uint mask = 0;

	if (! conn->readable && ! conn->fin_rcvd)
		mask |= SK_EV_readable;

	if (! conn->writable && ! conn->fin_sent)
		mask |= SK_EV_writable;

	evl->mod_socket(evl, conn->sk, mask);

printf("%s: have %c%c, monitoring %c%c\n", conn->name,
	conn->readable ? 'R' : '-',
	conn->writable ? 'W' : '-',
	(mask & SK_EV_readable) ? 'R' : '-',
	(mask & SK_EV_writable) ? 'W' : '-');
}

void send_pending(connection * conn)
{
	data_buffer * buf = conn->pending;
	int sent, left;

	assert(conn->pending && ! conn->writable);

	sent = sk_send_ip4(conn->sk, buf->data, buf->size, NULL);
	if (sent == buf->size)
	{
printf("%s: flushed %d bytes\n", conn->name, buf->size);
		free_data_buffer(conn->pending);
		conn->pending = NULL;
		conn->writable = 1;
		/* now writable again */

		/* ... unless EOF is pending */
		if (conn->peer->fin_rcvd)
			send_fin(conn);
		
		adjust_events_mask(conn);
	}
	else
	if (sent < 0)
	{
printf("%s: flush of %d bytes failed with %d\n", conn->name, buf->size, sk_errno(conn->sk));
		if (sk_send_fatal(sk_errno(conn->sk)))
		{
			on_send_failed(conn);
			return;
		}

		/* this was a no-op */
	}
	else
	{
		left = buf->size - sent;
printf("%s: flushed %d bytes, %d remains\n", conn->name, sent, left);
		memmove(buf->data, buf->data+sent, left);
		buf->size = left;

		/* still ! writable */
	}
}

void relay_data(connection * src, connection * dst)
{
	data_buffer * buf = src->b->pxy->buf;
	int rcvd, sent, left;
	size_t sofar = 0;

	assert(src->readable && dst->writable && ! dst->pending);
	assert(src->recv_max <= buf->capacity);

	for (;;)
	{
		rcvd = sk_recv_ip4(src->sk, buf->data, src->recv_max, NULL);
		if (rcvd == 0)
		{
			on_conn_eof(src);
			break;
		}

		if (rcvd < 0)
		{
printf("%s: read failed with %d\n", src->name, sk_errno(src->sk));
			if (sk_recv_fatal(sk_errno(src->sk)))
			{
				on_recv_failed(src);
			}
			else
			{
				src->readable = 0;
				adjust_events_mask(src);
			}
			break;
		}

		/* read something */
		assert(rcvd > 0);

printf("%s: read %d bytes\n", src->name, rcvd);

		sent = sk_send_ip4(dst->sk, buf->data, rcvd, NULL);
		if (sent == rcvd)
		{
printf("%s: sent\n", dst->name);
			sofar += sent;
			if (sofar > src->b->pxy->conf->max_per_cycle)
				break;
			continue;
		}

		if (sent < 0)
		{
printf("%s: send failed with %d\n", dst->name, sk_errno(dst->sk));
			if (sk_send_fatal(sk_errno(dst->sk)))
			{
				on_send_failed(dst);
				break;
			}

			sent = 0;
		}

		/* partial send */
		assert(sent < rcvd);

		left = rcvd - sent;
printf("%s: partial send, %d bytes queued\n", dst->name, left);
		dst->pending = init_data_buffer(left, buf->data+sent, left);

		dst->writable = 0;
		adjust_events_mask(dst);
		break;
	}
}

void send_fin(connection * conn)
{
	assert(! conn->fin_sent);

	sk_shutdown(conn->sk);

	conn->fin_sent = 1;
	conn->writable = 0;
	adjust_events_mask(conn);

	if (conn->peer->fin_sent)
		discard_bridge(conn->b);
}

void on_recv_failed(connection * conn)
{
printf("%s: recv failed\n", conn->name);
	discard_bridge(conn->b);
}

void on_send_failed(connection * conn)
{
printf("%s: send failed\n", conn->name);
	discard_bridge(conn->b);
}

void on_conn_eof(connection * conn)
{
printf("%s: got FIN\n", conn->name);
	conn->fin_rcvd = 1;
	conn->readable = 0;
	adjust_events_mask(conn);

	if (! conn->peer->pending)
		send_fin(conn->peer);
}

/*
 *
 */
const char * sa_to_str(const sockaddr_in * sa, char * buf, size_t max)
{
	snprintf(buf, max, "%u.%u.%u.%u:%u",
		(SOCKADDR_IN_ADDR(sa) >> 24) & 0xFF,
		(SOCKADDR_IN_ADDR(sa) >> 16) & 0xFF,
		(SOCKADDR_IN_ADDR(sa) >>  8) & 0xFF,
		(SOCKADDR_IN_ADDR(sa) >>  0) & 0xFF,
		htons(SOCKADDR_IN_PORT(sa)));

	return buf;
}

void dump_connection(connection * conn)
{
	printf("%s  %c%c%c  fin:%c%c", conn->name,
		conn->connecting ? 'C' : '-',
		conn->writable ? 'W' : '-',
		conn->readable ? 'R' : '-',
		conn->fin_rcvd ? 'R' : '-',
		conn->fin_sent ? 'S' : '-');

	if (conn->pending)
		printf(", %u pending", conn->pending->size);
}

void dump_pxy_state(proxy_state * pxy)
{
	char buf[64];
	hlist_item * i = NULL;

	printf("-- proxy state --\n");

	printf("listening  : %s\n", sa_to_str(&pxy->conf->sa_listen, buf, sizeof buf));
	printf("  backlog  : %d\n", pxy->conf->backlog);
	printf("connecting : %s\n", sa_to_str(&pxy->conf->sa_server, buf, sizeof buf));
	printf("c2p buffer : %u\n", pxy->conf->c2p_buffer);
	printf("p2s buffer : %u\n", pxy->conf->p2s_buffer);
	printf("max read   : %u /cycle\n", pxy->conf->max_per_cycle);
	printf("---- bridges ----\n");

	while ( (i = hlist_walk(&pxy->bridges, i)) )
	{
		bridge * b = structof(i, bridge, list);
		
		printf("%p: [", b);
		dump_connection(&b->c2p);
		printf("] [");
		dump_connection(&b->p2s);
		printf("]\n");
	}

	printf("-----------------\n");
}

/*
 *
 */
int dump_status = 0;

void on_signal(int sig)
{
	dump_status = 1;
}

int main(int argc, char ** argv)
{
	proxy_config conf;
	proxy_state state;
	int yes = 1;

	//
	signal(SIGUSR1, on_signal);

	//
	init_proxy_config(&conf);
	init_proxy_state(&state, &conf);

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
		static const char ticker[] = "bdqp";
		static int phase = 0;

		state.evl->monitor(state.evl, 150);
		printf("   %c\r", ticker[ phase++ % (sizeof(ticker)-1) ]);
		fflush(stdout);

		if (dump_status)
		{
			dump_status = 0;
			dump_pxy_state(&state);
		}
	}

	return 0;
}

