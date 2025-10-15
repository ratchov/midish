/*
 * Copyright (c) 2003-2019 Alexandre Ratchov <alex@caoua.org>
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

#include <stdio.h>
#include "utils.h"
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

size_t
state_fmt(char *buf, size_t bufsz, struct state *s)
{
	char *p = buf, *end = buf + bufsz;

	p += ev_fmt(buf, bufsz, &s->ev);

	if (s->flags & STATE_NEW)
		p += snprintf(p, p < end ? end - p : 0, " NEW");

	if (s->flags & STATE_CHANGED)
		p += snprintf(p, p < end ? end - p : 0, " CHANGED");

	if (s->flags & STATE_BOGUS)
		p += snprintf(p, p < end ? end - p : 0, " BOGUS");

	if (s->flags & STATE_NESTED)
		p += snprintf(p, p < end ? end - p : 0, " NESTED");

	if (s->phase & EV_PHASE_FIRST)
		p += snprintf(p, p < end ? end - p : 0, " FIRST");

	if (s->phase & EV_PHASE_NEXT)
		p += snprintf(p, p < end ? end - p : 0, " NEXT");

	if (s->phase & EV_PHASE_LAST)
		p += snprintf(p, p < end ? end - p : 0, " LAST");

	return p - buf;
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
	unsigned res;

	res = ev_match(&st->ev, ev);
#ifdef STATE_DEBUG
	if (res)
		logx(1, "%s: {ev:%p}: matched", __func__, &st->ev);
#endif
	return res;
}

/*
 * check if the given state belongs to the event spec
 */
unsigned
state_inspec(struct state *st, struct evspec *spec)
{
	struct evinfo *ei;

	if (spec == NULL) {
		return 1;
	}
	ei = evinfo + st->ev.cmd;
	switch(spec->cmd) {
	case EVSPEC_EMPTY:
		return 0;
	case EVSPEC_ANY:
		goto ch;
	case EVSPEC_NOTE:
		if (!EV_ISNOTE(&st->ev))
			return 0;
		break;
	default:
		if (st->ev.cmd != spec->cmd)
			return 0;
	}
	if (ei->nparams >= 1) {
		if (st->ev.v0 < spec->v0_min ||
		    st->ev.v0 > spec->v0_max)
			return 0;
	}
	if (ei->nparams >= 2) {
		if (st->ev.v1 < spec->v1_min ||
		    st->ev.v1 > spec->v1_max)
			return 0;
	}
ch:
	if (ei->flags & EV_HAS_DEV) {
		if (st->ev.dev < spec->dev_min ||
		    st->ev.dev > spec->dev_max)
			return 0;
	}
	if (ei->flags & EV_HAS_CH) {
		if (st->ev.ch < spec->ch_min ||
		    st->ev.ch > spec->ch_max)
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
	struct evinfo *ei;

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
	} else if (EV_ISSX(&st->ev)) {
		if (st->ev.cmd != ev->cmd)
			return 0;
		ei = evinfo + st->ev.cmd;
		if ((ei->nparams >= 1 && st->ev.v0 != ev->v0) ||
		    (ei->nparams >= 2 && st->ev.v1 != ev->v1))
			return 0;
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
		logx(1, "%s: not defined", __func__);
		panic();
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
		logx(1, "%s: unknown event type", __func__);
		panic();
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
		logx(1, "%s: can't restore note events", __func__);
		panic();
	}

	/*
	 * don't restore last event of terminated frames
	 */
	if ((st->phase & EV_PHASE_LAST) && !(st->phase & EV_PHASE_FIRST))
		return 0;

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

	/*
	 * free all states
	 */
	for (i = o->first; i != NULL; i = inext) {
		/*
		 * check that we didn't forgot to cancel some states
		 * the EV_CTL case is here for conv_xxx() functions
		 */
		if (!(i->phase & EV_PHASE_LAST) && i->ev.cmd != EV_CTL) {
			logx(1, "%s: {ev:%p}: unterminated frame", __func__, &i->ev);
		}
		inext = i->next;
		statelist_rm(o, i);
		state_del(i);
	}

}

void
statelist_dump(struct statelist *o)
{
	struct state *i;

	logx(1, "%s:", __func__);

	for (i = o->first; i != NULL; i = i->next)
		logx(1, "{ev:%p}", &i->ev);
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
	for (i = o->first; i != NULL; i = i->next) {
		if (state_match(i, ev)) {
			break;
		}
	}
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

	phase = ev_phase(ev);

	st = statelist->first;
	for (;;) {
		if (st == NULL) {
			st = state_new();
			st->flags = STATE_NEW;
			statelist_add(statelist, st);
			break;
		}

		stnext = st->next;

		if (state_match(st, ev)) {
			if (!(st->phase == EV_PHASE_LAST) &&
			    !(st->flags & STATE_BOGUS)) {
				st->flags &= ~STATE_NEW;
				break;
			}

			statelist_rm(statelist, st);
			state_del(st);
		}
		st = stnext;
	}

	switch (phase) {
	case EV_PHASE_FIRST:
		if (st->flags != STATE_NEW) {
			st = state_new();
			st->flags = STATE_NEW | STATE_NESTED;
			statelist_add(statelist, st);
#ifdef STATE_DEBUG
			logx(1, "%s: {ev:%p}: stacked (nested)", __func__, ev);
#endif
		}
		break;
	case EV_PHASE_NEXT:
	case EV_PHASE_LAST:
		if (st->flags == STATE_NEW) {
			st->flags |= STATE_BOGUS;
			phase |= EV_PHASE_FIRST;
			phase &= ~EV_PHASE_NEXT;
#ifdef STATE_DEBUG
			logx(1, "%s: {ev:%p}: first missing", __func__, ev);
#endif
		}
		break;
	case EV_PHASE_FIRST | EV_PHASE_NEXT:
		phase &= (st->flags == STATE_NEW) ?
		    ~EV_PHASE_NEXT : ~EV_PHASE_FIRST;
		break;
	case EV_PHASE_FIRST | EV_PHASE_LAST:
		/* nothing */
		break;
	default:
		logx(1, "%s: bad phase", __func__);
		panic();
	}

	state_copyev(st, ev, phase);
	statelist->changed = 1;
#ifdef STATE_DEBUG
	logx(1, "%s: updated: {state:%p}", __func__, st);
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
#ifdef STATE_DEBUG
			logx(1, "%s: {state:%p}: removed", __func__, i);
#endif
			statelist_rm(o, i);
			state_del(i);
		} else {
			i->flags &= ~STATE_CHANGED;
		}
	}
}

