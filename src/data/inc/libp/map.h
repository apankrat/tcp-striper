/*
 *	The code is distributed under terms of the BSD license.
 *	Copyright (c) 2014 Alex Pankratov. All rights reserved.
 *
 *	http://swapped.cc/bsd-license
 */
#ifndef _LIBP_MAP_H_
#define _LIBP_MAP_H_

/*
 *	map_item goes into an item that is being kept on the map.
 *
 *	map_head is the handle of the map and the rest of the API
 *	binds map_items to map_head to form ... duh ... a map.
 *
 *	The principal point is that the API doesn't perform any 
 *	allocation, it merely weaves items and the head together.
 *
 *	The app is expected to restore a pointer to its own item
 *	type from a pointer to map_item via container_of()
 *	macros, e.g.
 *
 *		struct foo
 *		{
 *			...
 *			map_item  index;
 *			... 
 *		};
 *
 *		void process_foo(map_item * p)
 *		{
 *			foo * f = container_of(p, foo, index);
 *			assert(p == &f->index);
 *			...
 *		}
 *
 *	Unless explicitly stated, the app should treat contents 
 *	of map_item and map_head as opaque and never operate 
 *	with them directly.
 */

/*
 *	This is a rudimentary (unbalanced) binary tree map.
 *	For better alternatives see map_avl.h & co.
 */
typedef struct map_item  map_item;
typedef struct map_head  map_head;

typedef int (* map_compare)(const map_item * a, const map_item * b);

//
struct map_item
{
	map_item * l;
	map_item * r;
	map_item * p;
};


struct map_head
{
	map_compare  comp;
	map_item   * root;
};

/*
 *
 */
void map_init(map_head * head, map_compare comp);

/*
 *	add() returns existing item on conflict, NULL otherwise
 *	del() returns removed item, NULL otherwise
 */
map_item * map_add(map_head * head, map_item * item);
map_item * map_del(map_head * head, map_item * item);

/*
 *	find() - self-explanatory
 */
map_item * map_find(map_head * head, map_item * key);

/*
 *	walk() is an iterator, start the walk with NULL
 */
map_item * map_walk(map_head * head, map_item * now);

#endif

