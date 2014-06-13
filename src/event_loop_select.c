#include "event_loop.h"

#include <sys/select.h>

#include "assert.h"
#include "macros.h"
#include "alloc.h"
#include "map.h"

/*
 *	rudimentary select()-based event loop
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
	event_loop api;

	int    nfds;    /* highest fd in sets + 1 */
	fd_set fds_r;
	fd_set fds_w;
	fd_set fds_x;

	map_head  sockets;
	int       map_touched;
};

typedef struct evl_select  evl_select;

/*
 *
 */
static
int _select_sk_comp(const map_item * a, const map_item * b)
{
	return structof(a, select_sk, by_sk)->sk - 
	       structof(b, select_sk, by_sk)->sk;
}

static
select_sk * _find_select_sk(evl_select * evl, int sk)
{
	map_item * mi;
	select_sk  foo;
	
	foo.sk = sk;
	mi = map_find(&evl->sockets, &foo.by_sk);

	return mi ? structof(mi, select_sk, by_sk) : NULL;
}

static
select_sk * _alloc_select_sk(int sk, uint events,
                            event_loop_cb cb, void * cb_context)
{
	select_sk * foo;

	foo = heap_alloc(sizeof *foo);
	if (! foo)
		return NULL;

	foo->sk = sk;
	foo->events = events;
	foo->cb = cb;
	foo->cb_context = cb_context;

	return foo;
}

/*
 *
 */
static
void _evl_select_add_socket(event_loop * self, int sk, uint events,
	                   event_loop_cb cb, void * cb_context)
{
	evl_select * evl = structof(self, evl_select, api);
	select_sk  * ssk;
	map_item   * mi;

	ssk = _alloc_select_sk(sk, events, cb, cb_context);
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
void _evl_select_mod_socket(event_loop * self, int sk, uint events)
{
	evl_select * evl = structof(self, evl_select, api);
	select_sk  * ssk;

	ssk = _find_select_sk(evl, sk);
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
void _evl_select_del_socket(event_loop * self, int sk)
{
	evl_select * evl = structof(self, evl_select, api);
	select_sk  * ssk;

	ssk = _find_select_sk(evl, sk);
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
			
			foo = structof(mi, select_sk, by_sk);
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
int _evl_select_monitor(event_loop * self, size_t timeout_ms)
{
	evl_select * evl = structof(self, evl_select, api);
	fd_set fds_r, fds_w, fds_x;
	struct timeval tv;
	int r;
	map_item  * mi;
	int active;

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

	active = 0;

	mi = NULL;
	while ( (mi = map_walk(&evl->sockets, mi)) )
	{
		select_sk * ssk = structof(mi, select_sk, by_sk);

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

again:
	evl->map_touched = 0;

	mi = NULL;
	while ( ( mi = map_walk(&evl->sockets, mi)) )
	{
		select_sk * ssk = structof(mi, select_sk, by_sk);
		uint have = ssk->have;
		
		if (! have)
			continue;

		ssk->have = 0;
		ssk->cb(ssk->cb_context, have);

		if (! --active)
			break;

		if (evl->map_touched)
			goto again;
	}

	return 0;
}

/*
 *
 */
event_loop * new_event_loop_select()
{
	evl_select * evl;

	evl = heap_alloc(sizeof *evl);
	if (! evl)
		return NULL;

	evl->api.add_socket = _evl_select_add_socket;
	evl->api.mod_socket = _evl_select_mod_socket;
	evl->api.del_socket = _evl_select_del_socket;
	evl->api.monitor    = _evl_select_monitor;
	evl->api.discard    = NULL; // _evl_select_discard

	evl->nfds = 0;
	FD_ZERO(&evl->fds_r);
	FD_ZERO(&evl->fds_w);
	FD_ZERO(&evl->fds_x);

	map_init(&evl->sockets, _select_sk_comp);
	evl->map_touched = 0;

	return &evl->api;
}

#if 0

	void (* discard)(event_loop * self);



 int select(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout);

       void FD_CLR(int fd, fd_set *set);
       int  FD_ISSET(int fd, fd_set *set);
       void FD_SET(int fd, fd_set *set);
       void FD_ZERO(fd_set *set);

#endif

