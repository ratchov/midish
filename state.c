/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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
 * states are structures used to hold the notes
 * which are "on", the values of used controllers
 * the bender etc...
 *
 * since for each midi event, a state can be created
 * in realtime, we use a pool.
 *
 */
 
#include "dbg.h"
#include "pool.h"
#include "state.h"

unsigned statelist_debug = 0;

struct pool state_pool;

void
state_pool_init(unsigned size) {
	pool_init(&state_pool, "state", sizeof(struct state), size);
}

void
state_pool_done(void) {
	pool_done(&state_pool);
}

struct state *
state_new(void) {
	return (struct state *)pool_new(&state_pool);
}

void
state_del(struct state *s) {
	pool_del(&state_pool, s);
}

void
statelist_init(struct statelist *o) {
	o->first = NULL;
#ifdef STATE_PROF
	o->lookup_n = 0;
	o->lookup_time = 0;
	o->lookup_max = 0;
#endif
}

void
statelist_done(struct statelist *o) {
#ifdef STATE_PROF
	unsigned mean;
	dbg_puts("statelist_done: lookup: num=");
	dbg_putu(o->lookup_n);
	if (o->lookup_n != 0) {
		mean = 100 * o->lookup_time / o->lookup_n;
		dbg_puts(", max=");
		dbg_putu(o->lookup_max);
		dbg_puts(", mean=");
		dbg_putu(mean / 100);
		dbg_puts(".");
		dbg_putu(mean % 100);
	}
	dbg_puts("\n");
#endif
	if (o->first != NULL) {
		dbg_puts("statelist_done: list not empty\n");
		dbg_panic();
	}
}

void
statelist_add(struct statelist *o, struct state *st) {
	st->next = o->first;
	st->prev = &o->first;
	if (o->first)
		o->first->prev = &st->next;
	o->first = st;
}

void
statelist_rm(struct statelist *o, struct state *st) {
	*st->prev = st->next;
	if (st->next)
		st->next->prev = st->prev;
}

struct state *
statelist_lookup(struct statelist *o, struct ev *ev) {
	struct state *i;
#ifdef STATE_PROF
	unsigned time = 0;
#endif

	for (i = o->first; i != NULL; i = i->next) {
#ifdef STATE_PROF
		time++;
#endif
		if (ev_sameclass(&i->ev, ev)) { 
			break;
		}
	}
#ifdef STATE_PROF
	o->lookup_n++;
	if (o->lookup_max < time) {
		o->lookup_max = time;
	}
	o->lookup_time += time;
#endif
	return i;
}

