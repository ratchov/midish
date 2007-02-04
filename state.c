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

/*
 * initialise an empty state list
 */
void
statelist_init(struct statelist *o) {
	o->first = NULL;
	o->changed = 0;
#ifdef STATE_PROF
	o->lookup_n = 0;
	o->lookup_time = 0;
	o->lookup_max = 0;
#endif
}

/*
 * destroy a statelist. All states are deleted, but if there are
 * states corresponding to unterminated frames, then a warning is
 * issued, since this probably is due to track inconsistencies
 */
void
statelist_done(struct statelist *o) {
	struct state *i, *inext;
#ifdef STATE_PROF
	unsigned mean;
#endif

	/*
	 * free all states
	 */
	for (i = o->first; i != NULL; i = inext) {
		if (!(i->phase & EV_PHASE_LAST)) {
			dbg_puts("statelist_done: ");
			ev_dbg(&i->ev);
			dbg_puts(": unterminated frame\n");
		}
		inext = i->next;
		statelist_rm(o, i);
		state_del(i);
	}

#ifdef STATE_PROF
	/*
	 * display profiling statistics
	 */
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
}

void
statelist_dump(struct statelist *o) {
	struct state *i;

	dbg_puts("statelist_dump:\n");
	for (i = o->first; i != NULL; i = i->next) {
		ev_dbg(&i->ev);
		dbg_puts("\n");
	}
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
		n->ctx_hi = i->ctx_hi;
		n->ctx_lo = i->ctx_lo;
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
 * check if the given event matches the given frame (if it matches,
 * then iether the event is part of the frame, either there is a
 * conflict between the frame and the event)
 */
unsigned
statelist_match(struct statelist *o, struct state *st, struct ev *ev) {
	switch (st->ev.cmd) {
	case EV_NON:
	case EV_NOFF:
	case EV_KAT:
		if (!EV_ISNOTE(ev) ||
		    st->ev.data.voice.b0 != ev->data.voice.b0 ||
		    st->ev.data.voice.ch != ev->data.voice.ch ||
		    st->ev.data.voice.dev != ev->data.voice.dev) {
			return 0;
		}
		break;		
	case EV_CTL:
		if (st->ev.cmd != ev->cmd ||
		    st->ev.data.voice.dev != ev->data.voice.dev ||
		    st->ev.data.voice.ch != ev->data.voice.ch) {
			return 0;
		}
		if (st->ev.data.voice.b0 != ev->data.voice.b0) {
			if (EVCTL_IS7BIT(st->ev.data.voice.b0) || 
			    EVCTL_IS7BIT(ev->data.voice.b0)) {
				return 0;
			} else {
				if (EVCTL_HI(st->ev.data.voice.b0) != ev->data.voice.b0 &&
				    EVCTL_HI(ev->data.voice.b0) != st->ev.data.voice.b0)
					return 0;
			}
		}
		break;
	case EV_BEND:
	case EV_CAT:
	case EV_PC:
		if (st->ev.cmd != ev->cmd ||
		    st->ev.data.voice.dev != ev->data.voice.dev ||
		    st->ev.data.voice.ch != ev->data.voice.ch) {
			return 0;
		}
		break;
	case EV_TEMPO:
	case EV_TIMESIG:
		if (st->ev.cmd != ev->cmd) {
			return 0;
		}
		break;
	default:
		dbg_puts("statelist_match: bad event type\n");
		dbg_panic();
		break;
	}
	return 1;
}


/*
 * copy event content into an existing state. It isn't enough to just
 * copy the event; special care must be taken for controller events to
 * handle contexts for 14bit controllers and reference to RPN/NRPN for
 * data-entry controllers
 */
void
statelist_copyev(struct statelist *slist, struct ev *ev, struct state *st) {
	struct state *ctxst;
	struct ev ctxev;

	st->ev = *ev;
	switch(ev->cmd) {
	case EV_CTL:
		if (EVCTL_IS7BIT(ev->data.voice.b0))
			break;
		if (EVCTL_ISHI(ev->data.voice.b0)) {
			st->ctx_hi = ev->data.voice.b1;
		} else {
			st->ctx_lo = ev->data.voice.b1;
		}
		break;
	case EV_PC:
		ctxev.cmd = EV_CTL;
		ctxev.data.voice.b0 = 0; /* bank msb, b1 unused */
		ctxev.data.voice.dev = ev->data.voice.dev;
		ctxev.data.voice.ch = ev->data.voice.ch;		
		ctxst = statelist_lookup(slist, &ctxev);
		if (ctxst) {
			st->ctx_hi = ctxst->ctx_hi;
			st->ctx_lo = ctxst->ctx_lo;
		} else {
			st->ctx_hi = EVCTL_UNDEF;
			st->ctx_lo = EVCTL_UNDEF;
		}
		break;
	}
#ifdef STATE_DEBUG
	dbg_puts("statelist_copyev: ");
	ev_dbg(ev);
	dbg_puts(": hi = ");
	dbg_putu(st->ctx_hi);
	dbg_puts(", lo = ");
	dbg_putu(st->ctx_lo);
	dbg_puts("\n");
#endif
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
		if (statelist_match(o, i, ev)) { 
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
 * update the state of a frame when a new event is received. If this
 * is the first event of the frame, then create a new state.
 *
 * XXX: try to reuse existing states instead of purging them and
 * allocating new ones
 */
struct state *
statelist_update(struct statelist *statelist, struct ev *ev) {
	struct state *st, *stnext;
	unsigned phase, flags, nevents;
#ifdef STATE_PROF
	unsigned time = 0;
#endif
	
	/*
	 * we scan for a matching state, if it exists but is
	 * terminated (phase = EV_PHASE_LAST) we purge it in order to
	 * ruse the list entry. We cant just use statelist_lookup(),
	 * because this will not work with nested frames (eg. if the
	 * "top" state is purged but not the other one). So here we
	 * inline a kind of 'lookup_for_write()' routine here:
	 */
	nevents = 0;
	st = statelist->first;
	for (;;) {
#ifdef STATE_PROF
		time++;
#endif
		if (st == NULL) {
			st = state_new();
			statelist_add(statelist, st);
			st->flags = STATE_NEW;
			st->nevents = 0;
			st->ctx_hi = st->ctx_lo = EVCTL_UNDEF;
			break;
		}
		stnext = st->next;
		if (statelist_match(statelist, st, ev)) {
			/*
			 * found a matching state
			 */
			phase = st->phase;
			flags = st->flags;
			if (phase & EV_PHASE_LAST) {
#ifdef STATE_DEBUG
				dbg_puts("statelist_update: ");
				ev_dbg(&st->ev);
				dbg_puts(": purged\n");
#endif
				/*
				 * if the event is not tagged as 
				 * nested, we reached the deepest
				 * state, so stop iterating here
				 * else continue purging states
				 */
				if (!(flags & STATE_NESTED)) {
					st->flags = STATE_NEW;
					break;
				} else {
					statelist_rm(statelist, st);
					state_del(st);
				}
			} else {
				st->flags &= ~STATE_NEW;
				break;
			}
		}
		st = stnext;
	}
#ifdef STATE_PROF
	statelist->lookup_n++;
	if (statelist->lookup_max < time) {
		statelist->lookup_max = time;
	}
	statelist->lookup_time += time;
#endif

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
	phase = ev_phase(ev);
	if (st->flags & STATE_NEW || phase == EV_PHASE_FIRST) {
		if (st->flags & STATE_NEW) {
			if (!(phase & EV_PHASE_FIRST)) {
				phase = EV_PHASE_FIRST | EV_PHASE_LAST;
				st->flags |= STATE_BOGUS;
#ifdef STATE_DEBUG
				dbg_puts("statelist_update: ");
				ev_dbg(ev);
				dbg_puts(": not first and no state\n");
#endif
			} else {
				phase &= ~EV_PHASE_NEXT;
			}
		} else {
#ifdef STATE_DEBUG
			dbg_puts("statelist_update: ");
			ev_dbg(ev);
			dbg_puts(": nested events, stacked\n");
#endif
			st = state_new();
			statelist_add(statelist, st);
			st->flags = STATE_NESTED | STATE_NEW;
			st->nevents = 0;
		}
		st->phase = phase;
		statelist_copyev(statelist, ev, st);
#ifdef STATE_DEBUG
		dbg_puts("statelist_update: ");
		ev_dbg(ev);
		dbg_puts(": new state\n");
#endif
	} else {
		statelist_copyev(statelist, ev, st);
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
	statelist->changed = 1;
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

	if (!o->changed)
		return;

	o->changed = 0;
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

/*
 * generate an array of events that can be played in order to cancel
 * the given state (ie restore all parameters related to the frame
 * state as the frame never existed). Return the number of generated
 * events
 *
 * note: if zero is returned, that doesn't mean that the frame
 * couldn't be canceled, that just means no events are needed (btw
 * currently this never happens...)
 */
unsigned
statelist_cancel(struct statelist *slist, struct state *st, struct ev *rev) {
	if (st->phase & EV_PHASE_LAST)
		return 0;
	switch(st->ev.cmd) {
	case EV_NON:
	case EV_KAT:
		rev->cmd = EV_NOFF;
		rev->data.voice.b0  = st->ev.data.voice.b0;
		rev->data.voice.b1  = EV_NOFF_DEFAULTVEL;
		break;
	case EV_CAT:
		rev->cmd = EV_CAT;
		rev->data.voice.b0  = EV_CAT_DEFAULT;
		break;
	case EV_CTL:
		if (EVCTL_IS7BIT(st->ev.data.voice.b0)) {
			rev->cmd = EV_CTL;
			rev->data.voice.b0 = st->ev.data.voice.b0;
			rev->data.voice.b1 = EVCTL_DEFAULT(st->ev.data.voice.b0);
			return 1;
		} else {
			rev->cmd = EV_CTL;
			rev->data.voice.b0 = EVCTL_HI(st->ev.data.voice.b0);
			rev->data.voice.b1 = EVCTL_DEFAULT(rev->data.voice.b0);
			rev->data.voice.dev = st->ev.data.voice.dev;
			rev->data.voice.ch  = st->ev.data.voice.ch;
			rev++;
			rev->cmd = EV_CTL;
			rev->data.voice.b0 = EVCTL_LO(st->ev.data.voice.b0);
			rev->data.voice.b1 = EVCTL_DEFAULT(rev->data.voice.b0);
			rev->data.voice.dev = st->ev.data.voice.dev;
			rev->data.voice.ch  = st->ev.data.voice.ch;
			return 2;
		}
		/* not reached */
	case EV_BEND:
		rev->cmd = EV_BEND;
		rev->data.voice.b0 = EV_BEND_DEFAULTLO;
		rev->data.voice.b1 = EV_BEND_DEFAULTHI;
		break;
	default:
		/* 
		 * other events have their EV_PHASE_LAST bit set, so
		 * we never come here
		 */
		dbg_puts("statelist_cancel: unknown event type\n");
		dbg_panic();
	}
	rev->data.voice.dev = st->ev.data.voice.dev;
	rev->data.voice.ch  = st->ev.data.voice.ch;
	return 1;
}


/*
 * generate an array of events that will restore the given state
 * return the number of generated events.
 *
 * note: if zero is returned, that doesn't mean that the frame
 * couldn't be canceled, that just means no events are needed (btw
 * currently this never happens...)
 */
unsigned
statelist_restore(struct statelist *slist, struct state *st, struct ev *rev) {
	unsigned num = 0;
	if (EV_ISNOTE(&st->ev)) {
		/*
		 * we never use this function for NOTE events, so
		 * if we're here, there is problem somewhere...
		 */
		dbg_puts("statelist_restore: can't restore note events\n");
		dbg_panic();
	}
	if ((st->phase & EV_PHASE_LAST) && !(st->phase & EV_PHASE_FIRST)) {
		dbg_puts("statelist_restore: WARNING: called for last event\n");
		return 0;
	}
	switch(st->ev.cmd) {
	case EV_CTL:
		if (EVCTL_IS7BIT(st->ev.data.voice.b0)) {
			*rev = st->ev;
			return 1;
		}
		if (st->ctx_hi == EVCTL_UNDEF) {
			/*
			 * we do nothig because lsb without
			 * msb is considered bogus
			 */
			return 0;
		}
		rev->cmd = EV_CTL;
		rev->data.voice.b0 = EVCTL_HI(st->ev.data.voice.b0);
		rev->data.voice.b1 = st->ctx_hi;
		rev->data.voice.dev = st->ev.data.voice.dev;
		rev->data.voice.ch  = st->ev.data.voice.ch;
		if (st->ctx_lo != EVCTL_UNDEF) {
			rev++;
			rev->cmd = EV_CTL;
			rev->data.voice.b0 = EVCTL_LO(st->ev.data.voice.b0);
			rev->data.voice.b1 = st->ctx_lo;
			rev->data.voice.dev = st->ev.data.voice.dev;
			rev->data.voice.ch  = st->ev.data.voice.ch;
			return 2;
		} else {
			return 1;
		}
		/* not reached */
	case EV_PC:
		if (st->ctx_hi != EVCTL_UNDEF) {
			rev->cmd = EV_CTL;
			rev->data.voice.b0 = 0; /* BANK HI */
			rev->data.voice.b1 = st->ctx_hi;
			rev->data.voice.dev = st->ev.data.voice.dev;
			rev->data.voice.ch  = st->ev.data.voice.ch;
			rev++;
			num++;
			if (st->ctx_lo != EVCTL_UNDEF) {
				rev->cmd = EV_CTL;
				rev->data.voice.b0 = 32; /* BANK LO */
				rev->data.voice.b1 = st->ctx_lo;
				rev->data.voice.dev = st->ev.data.voice.dev;
				rev->data.voice.ch  = st->ev.data.voice.ch;
				rev++;
				num++;
			}
		}
		*rev = st->ev;
		num++;
		return num;			
	default:
		*rev = st->ev;
		return 1;
	}
	/* not reached */
}

