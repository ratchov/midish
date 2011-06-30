/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * a pool is a large memory block (the pool) that is split into
 * small blocks of equal size (pools entries). Its used for
 * fast allocation of pool entries. Free enties are on a singly
 * linked list
 */

#include "dbg.h"
#include "pool.h"

unsigned pool_debug = 0;

/*
 * initialises a pool of "itemnum" elements of size "itemsize"
 */
void
pool_init(struct pool *o, char *name, unsigned itemsize, unsigned itemnum)
{
	unsigned i;
	unsigned char *p;

	/*
	 * round item size to sizeof unsigned
	 */
	if (itemsize < sizeof(struct poolent)) {
		itemsize = sizeof(struct poolent);
	}
	itemsize += sizeof(unsigned) - 1;
	itemsize &= ~(sizeof(unsigned) - 1);

	o->data = mem_alloc(itemsize * itemnum, "pool");
	if (!o->data) {
		dbg_puts("pool_init(");
		dbg_puts(name);
		dbg_puts("): out of memory\n");
		dbg_panic();
	}
	o->first = NULL;
	o->itemsize = itemsize;
	o->itemnum = itemnum;
	o->name = name;
#ifdef POOL_DEBUG
	o->maxused = 0;
	o->used = 0;
	o->newcnt = 0;
#endif

	/*
	 * create a linked list of all entries
	 */
	p = o->data;
	for (i = itemnum; i != 0; i--) {
		((struct poolent *)p)->next = o->first;
		o->first = (struct poolent *)p;
		p += itemsize;
		o->itemnum++;
	}
}


/*
 * free the given pool
 */
void
pool_done(struct pool *o)
{
	mem_free(o->data);
#ifdef POOL_DEBUG
	if (o->used != 0) {
		dbg_puts("pool_done(");
		dbg_puts(o->name);
		dbg_puts("): WARNING ");
		dbg_putu(o->used);
		dbg_puts(" items still allocated\n");
	}
	if (pool_debug) {
		dbg_puts("pool_done(");
		dbg_puts(o->name);
		dbg_puts("): using ");
		dbg_putu((1023 + o->itemnum * o->itemsize) / 1024);
		dbg_puts("kB maxused = ");
		dbg_putu(100 * o->maxused / o->itemnum);
		dbg_puts("% allocs = ");
		dbg_putu(100 * o->newcnt / o->itemnum);
		dbg_puts("%\n");
	}
#endif
}

/*
 * allocate an entry from the pool: just unlink
 * it from the free list and return the pointer
 */
void *
pool_new(struct pool *o)
{

	struct poolent *e;

	if (!o->first) {
		dbg_puts("pool_new(");
		dbg_puts(o->name);
		dbg_puts("): pool is empty\n");
		dbg_panic();
	}

	/*
	 * unlink from the free list
	 */
	e = o->first;
	o->first = e->next;

#ifdef POOL_DEBUG
	o->newcnt++;
	o->used++;
	if (o->used > o->maxused)
		o->maxused = o->used;
#endif
	return e;
}

/*
 * free an entry: just link it again on the free list
 */
void
pool_del(struct pool *o, void *p)
{
	struct poolent *e = (struct poolent *)p;
#ifdef POOL_DEBUG
	unsigned i, n;
	unsigned *buf;

	/*
	 * check if we aren't trying to free more
	 * entries than the poll size
	 */
	if (o->used == 0) {
		dbg_puts("pool_del(");
		dbg_puts(o->name);
		dbg_puts("): pool is full\n");
		dbg_panic();
	}
	o->used--;

	/*
	 * overwrite the entry with garbage so any attempt to use a
	 * free entry will probably segfault
	 */
	buf = (unsigned *)e;
	n = o->itemsize / sizeof(unsigned);
	for (i = 0; i < n; i++)
		*(buf++) = mem_rnd();
#endif
	/*
	 * link on the free list
	 */
	e->next = o->first;
	o->first = e;
}
