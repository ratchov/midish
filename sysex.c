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

#include "utils.h"
#include "sysex.h"
#include "defs.h"
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
sysex_log(struct sysex *o)
{
	struct chunk *ck;
	unsigned count;

	for (ck = o->first; ck != NULL; ck = ck->next) {
		count = ck->used;
		if (count > 16)
			count = 16;
		logx(1, "unit = %x, data = {{hexdump:%p,%u}%s}",
		    o->unit, ck->data, count, (count < ck->used) ? " ..." : "");
	}
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
	sysexlist_clear(o);
	o->first = (void *)0xdeadbeef;
	o->lastptr = (void *)0xdeadbeef;
}

/*
 * clear the sysex list
 */
void
sysexlist_clear(struct sysexlist *o)
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
 * detach the first sysex message from the list
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
 * put a sysex message at the given position
 */
void
sysexlist_add(struct sysexlist *l, unsigned int pos, struct sysex *e)
{
	struct sysex **pe;

	pe = &l->first;
	while (pos > 0) {
		pe = &(*pe)->next;
		pos--;
	}

	e->next = *pe;
	*pe = e;
	if (e->next == NULL)
		l->lastptr = &e->next;
}

/*
 * detach the sysex message at the given position from the list
 */
struct sysex *
sysexlist_rm(struct sysexlist *l, unsigned int pos)
{
	struct sysex *e, **pe;

	pe = &l->first;
	while (pos > 0) {
		pe = &(*pe)->next;
		pos--;
	}

	e = *pe;
	*pe = e->next;
	if (*pe == NULL)
		l->lastptr = pe;
	return e;
}

/*
 * dump a sysex list on stderr
 */
void
sysexlist_log(struct sysexlist *o)
{
	struct sysex *e;

	for (e = o->first; e != NULL; e = e->next) {
		if (e->first)
			sysex_log(e);
	}
}
