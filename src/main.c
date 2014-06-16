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

#include "assert.h"
#include "macros.h"
#include "alloc.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "socket.h"
#include "socket_utils.h"
#include "termio.h"

#include "data_buffer.h"
#include "proxy_types.h"

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
void on_recv_failed(connection * conn, int err);
void on_send_failed(connection * conn, int err);
void on_conn_eof(connection * conn);

/*
 *
 */
void on_can_accept(void * ctx, uint events)
{
	proxy_state * pxy = (proxy_state*)ctx;
	bridge * b;

	b = alloc_bridge(pxy);

	/* accept the connection */
	b->c2p.sk = sk_accept_ip4(pxy->sk, &b->c2p.sa_peer);
	if (b->c2p.sk < 0)
		goto err;

	if (sk_unblock(b->c2p.sk) < 0)
		goto err;

	if (sk_no_delay(b->c2p.sk) < 0)
		goto err;

	/* connect to the server */
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
	bridge     * b = conn->b;
	event_loop * evl = b->pxy->evl;
	int err;

	assert(conn->connecting);
	assert(conn == &conn->b->p2s);

	err = sk_error(conn->sk);
	if ( (events & SK_EV_error) ||
	    ((events & SK_EV_writable) && (err != 0)) )
	{
		/* connection failed */
		discard_bridge(conn->b);
		return;
	}

	assert(events & SK_EV_writable);

	/* connected to the server */
	conn->connecting = 0;
	conn->writable = 1;
	evl->del_socket(evl, conn->sk);

	/* activate bridge */
	evl->add_socket(evl,
		b->c2p.sk, SK_EV_readable | SK_EV_writable,
		on_conn_activity, &b->c2p);

	evl->add_socket(evl,
		conn->b->p2s.sk, SK_EV_readable,
		on_conn_activity, &b->p2s);
}


void on_conn_activity(void * ctx, uint events)
{
	connection * conn = (connection *)ctx;

	if (events & SK_EV_writable)
	{
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
		conn->readable = 1;
		adjust_events_mask(conn);

		if (conn->peer->writable)
			relay_data(conn, conn->peer);
	}

	if (events & SK_EV_error)
	{
		discard_bridge(conn->b);
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
}

void send_pending(connection * conn)
{
	data_buffer * buf = conn->pending;
	int sent, left, err;

	assert(conn->pending && ! conn->writable);

	sent = sk_send(conn->sk, buf->data, buf->size);
	if (sent == buf->size)
	{
		conn->tx += sent;

		free_data_buffer(conn->pending);
		conn->pending = NULL;
		conn->writable = 1;

		/* now writable again    *
		 * unless EOF is pending */
		if (conn->peer->fin_rcvd)
			send_fin(conn);

		adjust_events_mask(conn);
	}
	else
	if (sent < 0)
	{
		err = sk_errno(conn->sk);
		if (sk_send_fatal(err))
		{
			on_send_failed(conn, err);
			return;
		}

		/* this was a no-op */
	}
	else
	{
		/* recongested */

		conn->tx += sent;
		conn->congestions++;

		left = buf->size - sent;
		memmove(buf->data, buf->data+sent, left);
		buf->size = left;

		/* still ! writable */
	}
}

void relay_data(connection * src, connection * dst)
{
	data_buffer * buf = src->b->pxy->buf;
	int rcvd, sent, left, err;
	size_t sofar = 0;

	assert(src->readable && dst->writable && ! dst->pending);
	assert(src->recv_max <= buf->capacity);

	for (;;)
	{
		rcvd = sk_recv(src->sk, buf->data, src->recv_max);
		if (rcvd == 0)
		{
			on_conn_eof(src);
			break;
		}

		if (rcvd < 0)
		{
			err = sk_errno(src->sk);
			if (sk_recv_fatal(err))
			{
				on_recv_failed(src, err);
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

		src->rx += rcvd;

		sent = sk_send(dst->sk, buf->data, rcvd);
		if (sent == rcvd)
		{
			dst->tx += sent;

			sofar += sent;
			if (sofar > src->b->pxy->conf->max_per_cycle)
				break;

			continue;
		}

		if (sent < 0)
		{
			err = sk_errno(src->sk);
			if (sk_send_fatal(err))
			{
				on_send_failed(dst, err);
				break;
			}

			sent = 0;
		}

		/* partial send */
		assert(sent < rcvd);

		dst->tx += sent;
		dst->congestions++;

		left = rcvd - sent;
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

void on_recv_failed(connection * conn, int err)
{
	discard_bridge(conn->b);
}

void on_send_failed(connection * conn, int err)
{
	discard_bridge(conn->b);
}

void on_conn_eof(connection * conn)
{
	conn->fin_rcvd = 1;
	conn->readable = 0;
	adjust_events_mask(conn);

	if (! conn->peer->pending)
		send_fin(conn->peer);
}

/*
 *
 */
void dump_connection(connection * conn)
{
	printf("%s  %c%c%c  fin:%c%c  tx/rx %llu/%llu  cong %u", conn->name,
		conn->connecting ? 'C' : '-',
		conn->writable ? 'W' : '-',
		conn->readable ? 'R' : '-',
		conn->fin_rcvd ? 'R' : '-',
		conn->fin_sent ? 'S' : '-',
		conn->tx, conn->rx, 
		conn->congestions);

	if (conn->pending)
		printf(", %u pending", conn->pending->size);
}

void dump_proxy_state(proxy_state * pxy)
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

	printf("\n");
}

/*
 *
 */
int dump_status = 0;

void on_stdin_event(void * ctx, uint events)
{
	dump_status |= (getchar() == 0x1B);
}

int main(int argc, char ** argv)
{
	proxy_config conf;
	proxy_state state;
	int yes = 1;

	//
	init_proxy_config(&conf);
	init_proxy_state(&state, &conf);

	//
	term_set_buffering(STDIN_FILENO, 0);
	term_set_echo(STDIN_FILENO, 0);

	state.evl->add_socket(state.evl, 1, SK_EV_readable, on_stdin_event, NULL);

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
		state.evl->monitor(state.evl, 100);

		{{
			static const char ticker[] = "bdqp";
			static int phase = 0;
			static time_t last = 0;
			time_t now = time(NULL);

			if (now != last)
			{
				printf("   %c\r", ticker[ phase++ % (sizeof(ticker)-1) ]);
				fflush(stdout);
				last = now;
			}
		}}

		if (dump_status)
		{
			dump_status = 0;
			dump_proxy_state(&state);
		}
	}

	return 0;
}

