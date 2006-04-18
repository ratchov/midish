/*
 * Copyright (c) 2003-2006 Alexandre Ratchov
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

#include "dbg.h"
#include "pool.h"


	/*
	 * initialises a pool of "itemnum" elements of size "itemsize"
	 */

void
pool_init(struct pool *o, char *name, unsigned itemsize, unsigned itemnum) {
	unsigned i;
	unsigned char *p;
	
	/* round item size */
	
	if (itemsize < sizeof(struct poolentry)) {
		itemsize = sizeof(struct poolentry);
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
	dbg_puts("pool_init(");
	dbg_puts(o->name);
	dbg_puts("): using ");
	dbg_putu((1023 + o->itemnum * o->itemsize)/1024);
	dbg_puts(" Kbytes\n");	
#endif	
	p = o->data;
	for (i = itemnum; i != 0; i--) {
		((struct poolentry *)p)->next = o->first;
		o->first = (struct poolentry *)p;
		p += itemsize;
		o->itemnum++;		
	}
}


	/*
	 * frees a pool
	 */

void
pool_done(struct pool *o) {
	mem_free(o->data);
#ifdef POOL_DEBUG
	if (o->used != 0) {
		dbg_puts("pool_done(");
		dbg_puts(o->name);
		dbg_puts("): warning ");
		dbg_putu(o->used);
		dbg_puts(" items stil allocated\n");
	}

	dbg_puts("pool_done(");
	dbg_puts(o->name);
	dbg_puts("): maxused = ");
	dbg_putu(100 * o->maxused / o->itemnum);
	dbg_puts("% allocs = ");
	dbg_putu(100 * o->newcnt / o->itemnum);
	dbg_puts("%\n");
#endif
}

	/*
	 * allocate a item in the pool
	 */

void *
pool_new(struct pool *o) {	
	struct poolentry *i;
	
	if (!o->first) {
		dbg_puts("pool_new(");
		dbg_puts(o->name);
		dbg_puts("): pool is empty\n");
		dbg_panic();
	}
	
	i = o->first;
	o->first = i->next;

#ifdef POOL_DEBUG
	o->newcnt++;
	o->used++;
	if (o->used > o->maxused)
		o->maxused = o->used;
#endif
	return i;
}

	/*
	 * free a item to the pool
	 */

void
pool_del(struct pool *o, void *p) {
	struct poolentry *i = (struct poolentry *)p;
	
	i->next = o->first;
	o->first = i;

#ifdef POOL_DEBUG
	if (o->used == 0) {
		dbg_puts("pool_del(");
		dbg_puts(o->name);
		dbg_puts("): pool is full\n");
		dbg_panic();
	}
	o->used--;
#endif
}

