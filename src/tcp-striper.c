#include "map.h"
#include "macros.h"
#include <stdio.h>

struct foo
{
	int       val;
	map_item  m;
};

typedef struct foo foo;

int foo_comp(const map_item * a, const map_item * b)
{
	return structof(a, foo, m)->val - structof(b, foo, m)->val;
}

int dump_foo_map(map_head * map)
{
	map_item * i = NULL;
	int total;

	printf("[");
	for (total = 0; (i = map_walk(map, i)); total++)
	{
		foo * f = structof(i, foo, m);
		printf("%d ", f->val);
	}
	printf("]\n");
	return total;
}

int dump_map(map_item * item, char what, int level)
{
	if (! item)
	{
		printf("%*c: null\n", 4*level, what);
		return 0;
	}
	printf("%*c: %p %p [%d]\n", 4*level, what, item, item->p, structof(item,foo,m)->val);
	return 1 + 
	       dump_map(item->l, 'l', level+1) +
	       dump_map(item->r, 'r', level+1);
}

int main(int argc, char ** argv)
{
	map_head map;
	foo x[16];
	int i, n, total;

	map_init(&map, foo_comp);

	for (i=0; i<sizeof_array(x); i++)
	{
		x[i].val = (i & 1) ? i : -i; // rand() & 0xff;
		map_add(&map, &x[i].m);
	}

	total = dump_map(map.root, 'x', 0);
	printf("total = %d\n", total);
	total = dump_foo_map(&map);
	printf("total = %d\n", total);

	for (i=0, n=sizeof_array(x); i<n; i++)
	{
		map_del(&map, &x[i].m);
	}
	
	total = dump_map(map.root, 'x', 0);
	printf("total = %d\n", total);
	total = dump_foo_map(&map);
	printf("total = %d\n", total);

	return 0;
}
