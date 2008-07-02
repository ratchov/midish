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
 * states are structures used to hold events like notes, last values
 * of controllers, the current value of the bender, etc...
 *
 * states are linked to a list (statelist structure), so that the list
 * contains the complete state of the MIDI stream (ie all sounding
 * notes, states of all controllers etc...)
 *
 * statelist structures are used in the real-time filter, so we use a
 * state pool. In a typical performace, the maximum state list length
 * is roughly equal to the maximum sounding notes; the mean list
 * length is between 2 and 3 states and the maximum is between 10 and
 * 20 states. Currently we use a singly linked list, but for
 * performance reasons we shoud use a hash table in the future.
 *
 */

#include "dbg.h"
#include "pool.h"
#include "state.h"

struct pool state_pool;
unsigned state_serial;

void
state_pool_init(unsigned size)
{
	state_serial = 0;
	pool_init(&state_pool, "state", sizeof(struct state), size);
}

void
state_pool_done(void)
{
	pool_done(&state_pool);
}

struct state *
state_new(void)
{
	return (struct state *)pool_new(&state_pool);
}

void
state_del(struct state *s)
{
	pool_del(&state_pool, s);
}

/*
 * dump the state to stderr
 */
void
state_dbg(struct state *s)
{
	ev_dbg(&s->ev);
	if (s->flags & STATE_NEW) {
		dbg_puts(" NEW");
	}
	if (s->flags & STATE_CHANGED) {
		dbg_puts(" CHANGED");
	}
	if (s->flags & STATE_BOGUS) {
		dbg_puts(" BOGUS");
	}
	if (s->flags & STATE_NESTED) {
		dbg_puts(" NESTED");
	}
	if (s->phase & EV_PHASE_FIRST) {
		dbg_puts(" FIRST");
	}
	if (s->phase & EV_PHASE_NEXT) {
		dbg_puts(" NEXT");
	}
	if (s->phase & EV_PHASE_LAST) {
		dbg_puts(" LAST");
	}
}

/*
 * copy an event into a state.
 */
void
state_copyev(struct state *st, struct ev *ev, unsigned ph)
{
	st->ev = *ev;
	st->phase = ph;
	st->flags |= STATE_CHANGED;
}

/*
 * check if the given event matches the given frame (if so, this means
 * that, iether the event is part of the frame, either there is a
 * conflict between the frame and the event)
 */
unsigned
state_match(struct state *st, struct ev *ev)
{
	switch (st->ev.cmd) {
	case EV_NON:
	case EV_NOFF:
	case EV_KAT:
		if (!EV_ISNOTE(ev) ||
		    st->ev.note_num != ev->note_num ||
		    st->ev.ch != ev->ch ||
		    st->ev.dev != ev->dev) {
			return 0;
		}
		break;
	case EV_XCTL:
	case EV_NRPN:
	case EV_RPN:
		if (st->ev.cmd != ev->cmd ||
		    st->ev.dev != ev->dev ||
		    st->ev.ch != ev->ch ||
		    st->ev.v0 != ev->v0) {
			return 0;
		}
		break;
	case EV_BEND:
	case EV_CAT:
	case EV_XPC:
		if (st->ev.cmd != ev->cmd ||
		    st->ev.dev != ev->dev ||
		    st->ev.ch != ev->ch) {
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
		dbg_puts("state_match: ");
		state_dbg(st);
		dbg_puts(": bad event type\n");
		dbg_panic();
		break;
	}
#ifdef STATE_DEBUG
	dbg_puts("state_match: ");
	ev_dbg(&st->ev);
	dbg_puts(": ok\n");
#endif
	return 1;
}

/*
 * check if the given state belongs to the event spec
 */
unsigned
state_inspec(struct state *st, struct evspec *spec)
{
	if (spec == NULL) {
		return 1;
	}
	switch(spec->cmd) {
	case EVSPEC_EMPTY:
		return 0;
	case EVSPEC_ANY:
		goto ch;
	case EVSPEC_NOTE:
		if (!(st->phase & EV_PHASE_FIRST)) {
			dbg_puts("state_inspec: not first\n");
			dbg_panic();
		}
		if (st->ev.cmd == EV_NON) {
			goto v0;
		}
		break;
	case EVSPEC_CAT:
		if (st->ev.cmd == EV_CAT) {
			goto ch;
		}
		break;
	case EVSPEC_BEND:
		if (st->ev.cmd == EV_BEND) {
			goto ch;
		}
		break;
	case EVSPEC_XPC:
		if (st->ev.cmd == EV_XPC) {
			goto v1;
		}
		break;
	case EVSPEC_NRPN:
		if (st->ev.cmd == EV_NRPN) {
			goto v1;
		}
		break;
	case EVSPEC_RPN:
		if (st->ev.cmd == EV_RPN) {
			goto v1;
		}
		break;
	case EVSPEC_XCTL:
		if (st->ev.cmd == EV_XCTL) {
			goto v0;
		}
		break;
	default:
		break;
	}
	return 0;

v1:	if (st->ev.v1 < spec->v1_min ||
	    st->ev.v1 > spec->v1_max) {
		return 0;
	}
v0:	if (st->ev.v0 < spec->v0_min ||
	    st->ev.v0 > spec->v0_max) {
		return 0;
	}
ch:	if (st->ev.dev < spec->dev_min ||
	    st->ev.dev > spec->dev_max ||
	    st->ev.ch < spec->ch_min ||
	    st->ev.ch > spec->ch_max) {
		return 0;
	}
	return 1;
}

/*
 * compare a state to a matching event (ie one for which
 * state_match() returns 1)
 */
unsigned
state_eq(struct state *st, struct ev *ev)
{
	if (EV_ISVOICE(&st->ev)) {
		switch(st->ev.cmd) {
		case EV_CAT:
		case EV_BEND:
			if (st->ev.v0 != ev->v0)
				return 0;
			break;
		default:
			if (st->ev.cmd != ev->cmd ||
			    st->ev.v0 != ev->v0 ||
			    st->ev.v1 != ev->v1)
				return 0;
			break;
		}
	} else if (st->ev.cmd == EV_TEMPO) {
		if (st->ev.tempo_usec24 != ev->tempo_usec24) {
			return 0;
		}
	} else if (st->ev.cmd == EV_TIMESIG) {
		if (st->ev.timesig_beats != ev->timesig_beats ||
		    st->ev.timesig_tics != ev->timesig_tics) {
			return 0;
		}
	} else {
		dbg_puts("state_eq: not defined\n");
		dbg_panic();
	}
	return 1;
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
state_cancel(struct state *st, struct ev *rev)
{
	if (st->phase & EV_PHASE_LAST)
		return 0;
	switch(st->ev.cmd) {
	case EV_NON:
	case EV_KAT:
		rev->cmd = EV_NOFF;
		rev->note_num = st->ev.note_num;
		rev->note_vel = EV_NOFF_DEFAULTVEL;
		rev->dev = st->ev.dev;
		rev->ch = st->ev.ch;
		break;
	case EV_CAT:
		rev->cmd = EV_CAT;
		rev->cat_val = EV_CAT_DEFAULT;
		rev->dev = st->ev.dev;
		rev->ch  = st->ev.ch;
		break;
	case EV_XCTL:
		rev->cmd = EV_XCTL;
		rev->ctl_num = st->ev.ctl_num;
		rev->ctl_val = EV_CTL_DEFVAL(&st->ev);
		rev->dev = st->ev.dev;
		rev->ch = st->ev.ch;
		break;
	case EV_BEND:
		rev->cmd = EV_BEND;
		rev->bend_val = EV_BEND_DEFAULT;
		rev->dev = st->ev.dev;
		rev->ch = st->ev.ch;
		break;
	default:
		/*
		 * other events have their EV_PHASE_LAST bit set, so
		 * we never come here
		 */
		dbg_puts("state_cancel: unknown event type\n");
		dbg_panic();
	}
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
state_restore(struct state *st, struct ev *rev)
{
	if (st->flags & STATE_BOGUS)
		return 0;

	if (EV_ISNOTE(&st->ev)) {
		/*
		 * we never use this function for NOTE events, so
		 * if we're here, there is problem somewhere...
		 */
		dbg_puts("state_restore: can't restore note events\n");
		dbg_panic();
	}
	if ((st->phase & EV_PHASE_LAST) && !(st->phase & EV_PHASE_FIRST)) {
		dbg_puts("state_restore: ");
		state_dbg(st);
		dbg_puts(": called for last event!\n");
		return 0;
	}
	*rev = st->ev;
	return 1;
}


/*
 * initialize an empty state list
 */
void
statelist_init(struct statelist *o)
{
	o->first = NULL;
	o->changed = 0;
	o->serial = state_serial++;
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
statelist_done(struct statelist *o)
{
	struct state *i, *inext;
#ifdef STATE_PROF
	unsigned mean;
#endif

	/*
	 * free all states
	 */
	for (i = o->first; i != NULL; i = inext) {
		/*
		 * check that we didn't forgot to cancel some states
		 * the EV_CTL case is here for conv_xxx() functions
		 */
		if (!(i->phase & EV_PHASE_LAST) && i->ev.cmd != EV_CTL) {
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
	dbg_puts("statelist_done: serial=");
	dbg_putu(o->serial);
	dbg_puts(" lookup: num=");
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
statelist_dump(struct statelist *o)
{
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
statelist_dup(struct statelist *o, struct statelist *src)
{
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
statelist_empty(struct statelist *o)
{
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
statelist_add(struct statelist *o, struct state *st)
{
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
statelist_rm(struct statelist *o, struct state *st)
{
	*st->prev = st->next;
	if (st->next)
		st->next->prev = st->prev;
}

/*
 * find the first state that matches the given event
 * return NULL if not found
 */
struct state *
statelist_lookup(struct statelist *o, struct ev *ev)
{
	struct state *i;
#ifdef STATE_PROF
	unsigned time = 0;
#endif
	for (i = o->first; i != NULL; i = i->next) {
#ifdef STATE_PROF
		time++;
#endif
		if (state_match(i, ev)) {
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
 * we dont reuse existing states, but instead we purge them and we
 * allocate new ones, so that states that are often updated go to the
 * beginning of the list.
 */
struct state *
statelist_update(struct statelist *statelist, struct ev *ev)
{
	struct state *st, *stnext;
	unsigned phase;
#ifdef STATE_PROF
	unsigned time = 0;
#endif
	/*
	 * we scan for a matching state, if it exists but is
	 * terminated (phase = EV_PHASE_LAST) we purge it in order to
	 * reuse the list entry. We cant just use statelist_lookup(),
	 * because this will not work with nested frames (eg. if the
	 * "top" state is purged but not the other one). So here we
	 * inline a kind of 'lookup_for_write()' routine:
	 */
	st = statelist->first;
	for (;;) {
#ifdef STATE_PROF
		time++;
#endif
		if (st == NULL) {
			st = state_new();
			statelist_add(statelist, st);
			st->flags = STATE_NEW;
			break;
		}
		stnext = st->next;
		if (state_match(st, ev)) {
			/*
			 * found a matching state
			 */
			if (st->phase & EV_PHASE_LAST) {
				/*
				 * if the event is not tagged as
				 * nested, we reached the deepest
				 * state, so stop iterating here
				 * else continue purging states
				 */
				if (!(st->flags & STATE_NESTED)) {
					st->flags = 0;
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
	if (st->flags & STATE_NEW || st->phase & EV_PHASE_LAST) {
		/*
		 * this is new state or a terminated frame that we are
		 * reusing
		 */
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
	} else if (phase == EV_PHASE_FIRST) {
		/*
		 * this frame is not yet terminated. the incoming
		 * event starts a new one that's conflicting
		 */
#ifdef STATE_DEBUG
		dbg_puts("statelist_update: ");
		ev_dbg(ev);
		dbg_puts(": nested events, stacked\n");
#endif
		st = state_new();
		statelist_add(statelist, st);
		st->flags = STATE_NESTED | STATE_NEW;
	} else {
		/*
		 * this frame is not yet terminated, the incoming
		 * event belongs to it
		 */
		phase &=  ~EV_PHASE_FIRST;
	}
	state_copyev(st, ev, phase);
	statelist->changed = 1;
#ifdef STATE_DEBUG
	dbg_puts("statelist_update: updated: ");
	state_dbg(st);
	dbg_puts("\n");
#endif
	return st;
}

/*
 * mark all states as not changed. This routine is called at the
 * beginning of a tick (track editting) or after a timeout (real-time
 * filter).
 */
void
statelist_outdate(struct statelist *o)
{
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
		}
	}
}

