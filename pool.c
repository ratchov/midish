/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * 	- Redistributions of source code must retain the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer.
 *
 * 	- Redistributions in binary form must reproduce the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer in the documentation and/or other
 * 	  materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
pool_init(struct pool *o, char *name, unsigned itemsize, unsigned itemnum) {
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
		
	o->data = (unsigned char *)mem_alloc(itemsize * itemnum);
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
pool_done(struct pool *o) {
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
pool_new(struct pool *o) {	
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
pool_del(struct pool *o, void *p) {
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

