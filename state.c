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
 * states are structures used to hold the notes
 * which are "on", the values of used controllers
 * the bender etc...
 *
 * state lists are used in the real-time filter, so we use a state pool. In
 * a typical performace, the maximum state list length is roughly equal to
 * the maximum sounding notes; the mean list length is between 2 and 3
 * states and the maximum is between 10 and 20 states. Currently we use a
 * singly linked list, but for performance reasons we shoud use a hash table
 * in the future.
 */
 
#include "dbg.h"
#include "pool.h"
#include "state.h"

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
	statelist_empty(o);
}

/*
 * create a new statelist by duplicating another one
 */
void
statelist_dup(struct statelist *o, struct statelist *src) {
	struct state *i, *n;

	statelist_init(o);
	for (i = src->first; i != NULL; i = i->next) {
		n = state_new();
		n->ev = i->ev;
		n->phase = i->phase;
		n->flags = i->flags;
		statelist_add(o, n);
	}
}

/*
 * remove and free all states from the state list
 */
void
statelist_empty(struct statelist *o) {
	struct state *i, *inext;

	for (i = o->first; i != NULL; i = inext) {
		if (!(i->phase & EV_PHASE_LAST)) {
			dbg_puts("statelist_empty: ");
			ev_dbg(&i->ev);
			dbg_puts(": unterminated frame\n");
		}
		inext = i->next;
		statelist_rm(o, i);
		state_del(i);
	}
}

/*
 * add a state to the state list
 */ 
void
statelist_add(struct statelist *o, struct state *st) {
	st->next = o->first;
	st->prev = &o->first;
	if (o->first)
		o->first->prev = &st->next;
	o->first = st;
}

/*
 * remove a state from the state list, the state
 * isn't freed
 */
void
statelist_rm(struct statelist *o, struct state *st) {
	*st->prev = st->next;
	if (st->next)
		st->next->prev = st->prev;
}

/*
 * find the first state that matches the given event
 * return NULL if not found
 */
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

/*
 * create a state for the given event and return the state.
 * if there is already a state for a related event, then
 * update the existing state.
 *
 * Note: it would be nice if to reuse existing states instead of
 * purging them and allocating new ones
 */
struct state *
statelist_update(struct statelist *statelist, struct ev *ev) {
	struct state *st;
	unsigned phase, flags, nevents;

	phase = ev_phase(ev);
	st = statelist_lookup(statelist, ev);

	/*
	 * purge an unused state (state of terminated frame) that we
	 * need. This will also purge bogus frames.
	 */
	if (st != NULL && st->phase & EV_PHASE_LAST) {
#ifdef STATE_DEBUG
		dbg_puts("statelist_update: ");
		ev_dbg(&st->ev);
		dbg_puts(": purged\n");
#endif
		nevents = st->nevents;
		statelist_rm(statelist, st);
		state_del(st);
		st = NULL;
	} else {
		nevents = 0;
	}

	/*
	 * if one of the following are true
	 *	- there is no state because this is the first
	 *	  event of a new frame, or this is a bogus next event
	 *        (the beginning is missing)
	 *	- there is a state, but this is for sure the
	 *	  first event of a new frame (thus this is 
	 *	  de beginning of a nested frame)
	 * then create a new state.
	 */
	if (st == NULL || phase == EV_PHASE_FIRST) {
		if (!(phase & EV_PHASE_FIRST)) {
			flags = STATE_BOGUS | STATE_NEW;
			phase = EV_PHASE_FIRST | EV_PHASE_LAST;
#ifdef STATE_DEBUG
			dbg_puts("statelist_update: ");
			ev_dbg(ev);
			dbg_puts(": not first and no state\n");
#endif
		} else if (st != NULL) {
			flags = STATE_NESTED | STATE_NEW;
#ifdef STATE_DEBUG
			ev_dbg(ev);
			dbg_puts(": nested events, stacked\n");
#endif
		} else {
			flags = STATE_NEW;
			phase &= ~EV_PHASE_NEXT;
		}
		st = state_new();
		statelist_add(statelist, st);
		st->phase = phase;
		st->flags = flags;
		st->nevents = nevents;
		st->ev = *ev;
#ifdef STATE_DEBUG
		dbg_puts("statelist_update: ");
		ev_dbg(ev);
		dbg_puts(": new state\n");
#endif
	} else {
		st->ev = *ev;
		st->phase = phase & ~EV_PHASE_FIRST;
		st->flags &= ~STATE_NEW;
#ifdef STATE_DEBUG
		dbg_puts("statelist_update: ");
		ev_dbg(ev);
		dbg_puts(": updated\n");
#endif
	}
	st->flags |= STATE_CHANGED;
	st->nevents++;
	return st;
}

/*
 * mark all states as not changed. This routine is called at the
 * beginning of a tick (track editting) or after a timeout (real-time
 * filter).
 */
void
statelist_outdate(struct statelist *o) {
	struct state *i, *inext;

	for (i = o->first; i != NULL; i = inext) {
		inext = i->next;
		/*
		 * we purge states that are terminated, but we keep states
		 * of unknown controllers, tempo changes etc... these
		 * states have both FIRST and LAST bits set
		 */
		if (i->phase == EV_PHASE_LAST) {
			statelist_rm(o, i);
			state_del(i);
		} else {
			i->flags &= ~STATE_CHANGED;
			i->nevents = 0;
		}
	}
}

