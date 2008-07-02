/*
 * Copyright (c) 2003-2007 Alexandre Ratchov <alex@caoua.org>
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
 * system exclusive (sysex) message management.
 *
 * A sysex message is a long byte string whose size is not know in
 * advance. So we preallocate a large pool of 256 byte chunks and we
 * represent a sysex message as a list of chunks. Since there may be
 * several sysex messages we use a pool for the sysex messages
 * themselves.
 *
 * the song contains a list of sysex message, so we group them in a
 * list.
 */

#include "dbg.h"
#include "sysex.h"
#include "default.h"
#include "pool.h"

/* ------------------------------------------ sysex pool routines --- */

struct pool chunk_pool;
struct pool sysex_pool;

void
chunk_pool_init(unsigned size) 
{
	pool_init(&chunk_pool, "chunk", sizeof(struct chunk), size);
}

void
chunk_pool_done(void) 
{
	pool_done(&chunk_pool);
}

struct chunk *
chunk_new(void) 
{
	struct chunk *o;
	o = (struct chunk *)pool_new(&chunk_pool);
	o->next = NULL;
	o->used = 0;
	return o;
}

void
chunk_del(struct chunk *o) 
{
	pool_del(&chunk_pool, o);
}

void
sysex_pool_init(unsigned size) 
{
	pool_init(&sysex_pool, "sysex", sizeof(struct sysex), size);
}

void
sysex_pool_done(void) 
{
	pool_done(&sysex_pool);
}

/*
 * create an empty sysex message
 */
struct sysex *
sysex_new(unsigned unit) 
{
	struct sysex *o;
	o = (struct sysex *)pool_new(&sysex_pool);
	o->next = NULL;
	o->unit = unit;
	o->first = o->last = NULL;
	return o;
}

/*
 * free all chunks of a sysex message, and the message
 * itself
 */
void
sysex_del(struct sysex *o) 
{
	struct chunk *i, *inext;
	for (i = o->first; i != NULL; i = inext) {
		inext = i->next;
		chunk_del(i);
	}
	pool_del(&sysex_pool, o);
}

/*
 * add a byte to the message
 */
void
sysex_add(struct sysex *o, unsigned data) 
{
	struct chunk *ck;
		
	ck = o->last;
	if (!ck) {
		ck = o->first = o->last = chunk_new();
	}
	if (ck->used >= CHUNK_SIZE) {
		ck->next = chunk_new();
		ck = ck->next;
		o->last = ck;
	}
	ck->data[ck->used++] = data;
}

/*
 * dump the sysex message on stderr
 */
void
sysex_dbg(struct sysex *o) 
{
	struct chunk *ck;
	unsigned i;
	dbg_puts("unit = ");
	dbg_putx(o->unit);
	dbg_puts(", data = { ");
	for (ck = o->first; ck != NULL; ck = ck->next) {
		for (i = 0; i < ck->used; i++) {
			dbg_putx(ck->data[i]);
			dbg_puts(" ");
		}
	}
	dbg_puts("}");
}

/*
 * check that the sysex message (1) starts with 0xf0, (2) ends with
 * 0xf7 and (3) doesn't contain any status bytes.
 */
unsigned
sysex_check(struct sysex *o) 
{
	unsigned status, data;	
	struct chunk *ck;
	unsigned i;
	
	
	status = 0; 
	for (ck = o->first; ck != NULL; ck = ck->next) {
		for (i = 0; i < ck->used; i++) {
			data = ck->data[i];
			if (data == 0xf0) { 		/* sysex start */
				if (status != 0) {
					return 0;
				}
				status = data;
			} else if (data == 0xf7) {
				if (status != 0xf0) {
					return 0;
				}
				status = data;
			} else if (data > 0x7f) {
				return 0;
			} else {
				if (status != 0xf0) {
					return 0;
				}
			}
		}
	}
	if (status != 0xf7) {
		return 0;
	}
	return 1;
}

/*
 * initialize a list of sysex messages
 */
void
sysexlist_init(struct sysexlist *o) 
{
	o->first = NULL;
	o->lastptr = &o->first;
}

/*
 * destroy the list
 */
void
sysexlist_done(struct sysexlist *o) 
{
	struct sysex *i, *inext;
	
	for (i = o->first; i != NULL; i = inext) {
		inext = i->next;
		sysex_del(i);
	}
	o->first = NULL;
	o->lastptr = &o->first;
}

/*
 * put a sysex message at the end of the list
 */
void
sysexlist_put(struct sysexlist *o, struct sysex *e) 
{
	e->next = NULL;
	*o->lastptr = e;
	o->lastptr = &e->next;
}

/*
 * detach the first sysex message on the list
 */
struct sysex *
sysexlist_get(struct sysexlist *o) 
{
	struct sysex *e;
	if (o->first) {
		e = o->first;
		o->first = e->next;
		if (e->next == NULL) {
			o->lastptr = &o->first;
		}
		return e;
	}
	return 0;		
}

/*
 * dump a sysex list on stderr
 */
void
sysexlist_dbg(struct sysexlist *o) 
{
	struct sysex *e;
	unsigned i;
	dbg_puts("sysex_dbg:\n");
	for (e = o->first; e != NULL; e = e->next) {
		dbg_puts("unit = ");
		dbg_putx(e->unit);
		dbg_puts(", data = { ");
		if (e->first) {
			for (i = 0; i < e->first->used; i++) {
				if (i > 16) {
					dbg_puts("... ");
					break;
				}
				dbg_putx(e->first->data[i]);
				dbg_puts(" ");
			}
		}
		dbg_puts("}\n");
	}
}
