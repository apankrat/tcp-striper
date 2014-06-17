/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#include "libp/map.h"

#include "libp/assert.h"
#include "libp/macros.h"

/*
 *	internal
 */
static
map_item ** _map_find(map_head * head, map_item * key, map_item ** parent)
{
	map_item ** p;
	map_item * q;
	int r;

	p = &head->root;
	q = NULL;

	while (*p)
	{
		r = head->comp(key, *p);
		if (! r)
			return p; /* dupe */
	
		q = *p;
		p = (r < 0) ? &(*p)->l : &(*p)->r;
	}

	// p is either &head->root, q is NULL or
	// p = &q->l or 
	// p = &q->r
	if (parent)
		*parent = q;

	return p;
}

/*
 *	API
 */
void map_init(map_head * head, map_compare comp)
{
	head->comp = comp;
	head->root = NULL;
}

map_item * map_add(map_head * head, map_item * item)
{
	map_item ** pp, * p;
	
	pp = _map_find(head, item, &p);
	if (*pp)
		return *pp; /* dupe */

	*pp = item;
	item->p = p;
	item->l = NULL;
	item->r = NULL;

	return NULL;
}

map_item * map_del(map_head * head, map_item * item)
{
	map_item ** p;
	map_item * q;

	p = _map_find(head, item, NULL);
	if (*p != item)
		return NULL;

	if (item->l && item->r)
	{
		q = item->l;
		while (q->r)
			q = q->r;

		q->r = item->r;
		item->p = q;

		*p = item->l;
		item->l->p = item->p;
	}
	else
	if (item->l)
	{
		*p = item->l;
		item->l->p = item->p;
	}
	else
	if (item->r)
	{
		*p = item->r;
		item->r->p = item->p;
	}
	else
	{
		*p = NULL;
	}

	item->l = NULL;
	item->r = NULL;
	item->p = NULL;

	return item;
}

map_item * map_find(map_head * head, map_item * key)
{
	map_item ** p;

	p = _map_find( (map_head*)head, (map_item*)key, NULL );
	return *p;
}

map_item * map_walk(map_head * head, map_item * now)
{
	map_item * p;

	if (! now)
	{
		if (! head->root)
			return NULL;

		p = head->root;
		while (p->l)
			p = p->l;
	}
	else
	if (now->r)
	{
		p = now->r;
		while (p->l)
			p = p->l;
	}
	else
	{
		for (;;)
		{
			p = now->p;
			if (! p || p->l == now)
				break;
			now = p;
		}
	}

	return p;
}



