/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_LIST_H_
#define _LIBP_LIST_H_

/*
 *	slist, qlist, hlist, dlist containers
 *	as per http://swapped.cc/on-linked-lists
 */

/*
 *	hlist
 */
typedef struct hlist_head hlist_head;
typedef struct hlist_item hlist_item;

struct hlist_head
{
	hlist_item * first;
};

struct hlist_item
{
	hlist_item ** pprev;
	hlist_item * next;
};

/*
 *
 */
static inline
void hlist_init(hlist_head * h)
{
	h->first = NULL;
}

static inline
void hlist_init_item(hlist_item * i)
{
	i->pprev = NULL;
	i->next = NULL;
}

static inline
void hlist_add_front(hlist_head * h, hlist_item * i)
{
	i->next = h->first;
	if (i->next)
		i->next->pprev = &i->next;
	i->pprev = &h->first;
	h->first = i;
}

static inline
void hlist_del(hlist_item * i)
{
	if (! i->pprev)
		return;

	*i->pprev = i->next;
	if (i->next)
		i->next->pprev = i->pprev;

	hlist_init_item(i);
}

static inline
hlist_item * hlist_walk(hlist_head * head, hlist_item * now)
{
	return now ? now->next : head->first;
}

#endif

