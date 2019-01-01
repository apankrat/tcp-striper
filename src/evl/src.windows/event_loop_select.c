/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/event_loop.h"

#include "libp/assert.h"
#include "libp/macros.h"
#include "libp/alloc.h"
#include "libp/map.h"

#include "libp/socket.h"

/*
 *	Rudimentary select()-based event loop
 */
struct select_sk
{
	int            sk;
	uint           events;
	event_loop_cb  cb;
	void *         cb_context;

	map_item       by_sk;
	uint           have;
};

typedef struct select_sk  select_sk;

/*
 *
 */
struct evl_select
{
	event_loop  api;

	int         nfds;    /* highest fd in sets + 1 */
	fd_set      fds_r;
	fd_set      fds_w;
	fd_set      fds_x;

	map_head    sockets;
	int         map_touched : 1;
	int         in_callback : 1;
	int         dead : 1;
};

typedef struct evl_select  evl_select;

/*
 *
 */
static
int select_sk_comp(const map_item * a, const map_item * b)
{
	return struct_of(a, select_sk, by_sk)->sk -
	       struct_of(b, select_sk, by_sk)->sk;
}

static
select_sk * find_select_sk(evl_select * evl, int sk)
{
	map_item * mi;
	select_sk  foo;

	foo.sk = sk;
	mi = map_find(&evl->sockets, &foo.by_sk);

	return mi ? struct_of(mi, select_sk, by_sk) : NULL;
}

static
select_sk * alloc_select_sk(int sk, uint events,
                            event_loop_cb cb, void * cb_context)
{
	select_sk * foo;

	foo = heap_malloc(sizeof *foo);
	if (! foo)
		return NULL;

	foo->sk = sk;
	foo->events = events;
	foo->cb = cb;
	foo->cb_context = cb_context;

	return foo;
}

static
void evl_select_dispose(evl_select * evl)
{
	select_sk * ssk;
	map_item * mi;

	while ( (mi = map_walk(&evl->sockets, NULL)) )
	{
		ssk = struct_of(mi, select_sk, by_sk);
		map_del(&evl->sockets, mi);
		heap_free(ssk);
	}

	heap_free(evl);
}

/*
 *
 */
static
void evl_select_add_socket(event_loop * self, int sk, uint events,
                           event_loop_cb cb, void * cb_context)
{
	evl_select * evl = struct_of(self, evl_select, api);
	select_sk  * ssk;
	map_item   * mi;

	ssk = alloc_select_sk(sk, events, cb, cb_context);
	if (! ssk)
		return; /* out of memory */

	mi = map_add(&evl->sockets, &ssk->by_sk);
	assert(! mi);   /* duplicate sk */

	if (events & SK_EV_readable)
		FD_SET(sk, &evl->fds_r);

	if (events & SK_EV_writable)
		FD_SET(sk, &evl->fds_w);

	FD_SET(sk, &evl->fds_x);

	if (sk >= evl->nfds)
		evl->nfds = sk+1;

	evl->map_touched = 1;
}

static
void evl_select_mod_socket(event_loop * self, int sk, uint events)
{
	evl_select * evl = struct_of(self, evl_select, api);
	select_sk  * ssk;

	ssk = find_select_sk(evl, sk);
	assert(ssk);

	if (  (events & SK_EV_readable) &&
            ! (ssk->events & SK_EV_readable) )
	{
		FD_SET(sk, &evl->fds_r);
	}

	if (! (events & SK_EV_readable) &&
	      (ssk->events & SK_EV_readable) )
	{
		FD_CLR(sk, &evl->fds_r);
	}

	if (  (events & SK_EV_writable) &&
            ! (ssk->events & SK_EV_writable) )
	{
		FD_SET(sk, &evl->fds_w);
	}

	if (! (events & SK_EV_writable) &&
	      (ssk->events & SK_EV_writable) )
	{
		FD_CLR(sk, &evl->fds_w);
	}

	ssk->events = events;
}

static
void evl_select_del_socket(event_loop * self, int sk)
{
	evl_select * evl = struct_of(self, evl_select, api);
	select_sk  * ssk;

	ssk = find_select_sk(evl, sk);
	assert(ssk);

	if (ssk->events & SK_EV_readable)
		FD_CLR(sk, &evl->fds_r);

	if (ssk->events & SK_EV_writable)
		FD_CLR(sk, &evl->fds_w);

	FD_CLR(sk, &evl->fds_x);

	map_del(&evl->sockets, &ssk->by_sk);

	heap_free(ssk);

	/*
	 *	recalculate evl->nfds
	 */
	if (evl->nfds == sk+1)
	{
		map_item  * mi = NULL;
		int max_sk = -1;

		while ( (mi = map_walk(&evl->sockets, mi)) )
		{
			select_sk * foo;

			foo = struct_of(mi, select_sk, by_sk);
			if (foo->sk < max_sk)
				continue;

			max_sk = foo->sk;
			if (max_sk == sk-1)
				break; /* as sk was the largest */
		}

		evl->nfds = max_sk+1;
	}

	evl->map_touched = 1;
}

static
int evl_select_monitor(event_loop * self, size_t timeout_ms)
{
	evl_select * evl = struct_of(self, evl_select, api);
	fd_set fds_r, fds_w, fds_x;
	struct timeval tv;
	int r;
	map_item  * mi;
	int active;

	/*
	 *	Don't recurse, i.e. don't call evl->select() from
	 *	a callback dispatched from another call to evl->select()
	 */
	assert(! evl->in_callback);
	if (evl->in_callback)
		return -1;

	/*
	 *	OK, select
	 */
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = 1000 * (timeout_ms % 1000);

	if (! evl->nfds)
	{
		r = select(0, NULL, NULL, NULL, &tv);
	}
	else
	{
		fds_r = evl->fds_r;
		fds_w = evl->fds_w;
		fds_x = evl->fds_x;

		r = select(evl->nfds, &fds_r, &fds_w, &fds_x, &tv);
	}

	if (r < 0)
		return -1;

	if (r == 0)
		return 0;

	/*
	 *	Got some activity
	 */
	active = 0;
	mi = NULL;
	while ( (mi = map_walk(&evl->sockets, mi)) )
	{
		select_sk * ssk = struct_of(mi, select_sk, by_sk);

		ssk->have = 0;

		if (FD_ISSET(ssk->sk, &fds_r))
			ssk->have |= SK_EV_readable;

		if (FD_ISSET(ssk->sk, &fds_w))
			ssk->have |= SK_EV_writable;

		if (FD_ISSET(ssk->sk, &fds_x))
			ssk->have |= SK_EV_error;

		if (ssk->have)
			active++;
	}

	assert(active); /* otherwise r should've been 0 */

	/*
	 *	Dispatch callbacks
	 */
again:
	evl->map_touched = 0;

	mi = NULL;
	while ( ( mi = map_walk(&evl->sockets, mi)) )
	{
		select_sk * ssk = struct_of(mi, select_sk, by_sk);
		uint have = ssk->have;

		if (! have)
			continue;

		ssk->have = 0;

		evl->in_callback = 1;
		ssk->cb(ssk->cb_context, have);
		evl->in_callback = 0;

		if (evl->dead)
		{
			/* discard() was called from the callback */
			evl_select_dispose(evl);
			return -1;
		}

		if (! --active)
			break;

		if (evl->map_touched)
			goto again;
	}

	return 0;
}

static
void evl_select_discard(event_loop * self)
{
	evl_select * evl = struct_of(self, evl_select, api);

	assert(! evl->dead); /* don't discard more than once */
	evl->dead = 1;

	if (evl->in_callback)
		/* Got here through ssk->cb() from select(), so
		   select() will do the actual 'evl' disposal */
		return;

	evl_select_dispose(evl);
}

/*
 *
 */
event_loop * new_event_loop_select()
{
	evl_select * evl;

	evl = heap_malloc(sizeof *evl);
	if (! evl)
		return NULL;

	evl->api.add_socket = evl_select_add_socket;
	evl->api.mod_socket = evl_select_mod_socket;
	evl->api.del_socket = evl_select_del_socket;
	evl->api.monitor    = evl_select_monitor;
	evl->api.discard    = evl_select_discard;

	evl->nfds = 0;
	FD_ZERO(&evl->fds_r);
	FD_ZERO(&evl->fds_w);
	FD_ZERO(&evl->fds_x);

	map_init(&evl->sockets, select_sk_comp);
	evl->map_touched = 0;

	return &evl->api;
}

