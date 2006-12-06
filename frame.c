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
 * a seqptr structure points to a location of the associated track and
 * can move forward, read events and write events. The location is
 * defined by the current tic and the current event within the tic. In
 * some sense a seqptr structure is for a track what is a head for a
 * tape.
 *
 * It maintains a "state list" that contains the complete state of the
 * track at the current postion: the list of all sounding notes, the
 * state of all controllers etc... This allows for instance to move to
 * an arbitrary position and to recover the complete midi state.
 *
 * Moving within the track:
 *
 *	there is no low-level primitives for moving forward, instead
 *	reading primitives should be used (and the result should be
 *	ignored). That's because the state list have to be kept up to
 *	date. Thus there is no way to go backward.
 *
 * Reading: 
 *
 *	there are 2 low-level primitives for reading: seqptr_ticskip()
 *	skips empty tics (moves forward) and seqptr_evget() reads
 *	events. There can be multiple seqptr structures reading 
 *	the same track.
 *
 * Writing:
 *
 *	there are 2 low-level routines for writing: seqptr_ticput()
 *	adds empty tics and seqptr_evput() adds an event at the
 *	current postion. The state list is updated as new events were
 *	read with seqptr_evget(). If there is a writer, there must not
 *	be any readers. In order to keep track consistency, events
 *	must only be appended at the end-of-track; indeed, if we write
 *	an arbitrary event in the middle of a track, generally it
 *	isn't possible to solve all conflicts.
 *
 * Erasing:
 *
 *	there are 2 low-level routines for erasing: seqptr_ticdel()
 *	deletes empty tics and seqptr_evdel() deletes the next event.
 *	The state list is not updated since the current position
 *	haven't changed. 
 *
 * 
 * Common errors and pitfals
 * -------------------------
 *
 * before adding new code, or changing existing code check for the
 * following errors:
 *
 *	- only call seqptr_evput() at the end of track. the only
 *	  exception of this rule, is when a track is completely
 *	  rewritten. So the following loop:
 *
 *		for (;;) {
 *			st = seqptr_evdel(&sp, &slist);
 *			seqptr_evput(&sp, &st->ev);
 *		}
 *
 *	  is ok only if _all_ events are removed.
 *
 *	- when rewriting a track one must use a separate statelist
 *	  for events being removed, so the seqptr->statelist is used for
 *	  writing new events. When starting rewriting a track on a 
 *	  given position, be sure to initialise the 'deleted events'
 *	  statelist with statelist_dup(), and not statelist_init(). 
 *	  Example:
 *
 *		seqptr_skip(&sp, pos);
 *		statelist_dup(&slist); 
 *		for (;;) {
 *			seqptr_evdel(&sp, &slist);
 *			...
 *		}
 *
 *	- when working with a statelist initialised with statlist_dup()
 *	  be aware that tags are not copied. The only fields that are copied
 *	  are those managed by statelist_update() routine. So we must first
 *	  duplicate the statelist and then tag states. Example:
 *
 *		seqptr_skip(&sp, pos);
 *		statelist_dup(&slist, &sp->slist);
 *		for (st = slist.first; st != NULL; st = st->next) {
 *			st->silent = ...
 *		}
 *
 *	  it is _not_ ok, to iterate over sp.statelist and then to dup it.
 *
 */

#include "dbg.h"
#include "track.h"
#include "default.h"
#include "frame.h"

#define SEQPTR_DEBUG
#define FRAME_DEBUG

/*
 * initialise a seqptr structure at the beginning of 
 * the given track.
 */
void
seqptr_init(struct seqptr *sp, struct track *t) {
	statelist_init(&sp->statelist);
	sp->pos = t->first;
	sp->delta = 0;
	sp->tic = 0;
}

/*
 * release the seqptr structure, free statelist etc...
 */
void
seqptr_done(struct seqptr *sp) {
	statelist_done(&sp->statelist);
	sp->pos = (void *)0xdeadbeef;
	sp->delta = 0xdeadbeef;
	sp->tic = 0;
}

/*
 * return true if the end-of-track is reached
 */
unsigned
seqptr_eot(struct seqptr *sp) {
	return sp->pos->ev.cmd == EV_NULL && sp->delta == sp->pos->delta;
}

/*
 * return true if an event is available within the current tic
 */
unsigned
seqptr_evavail(struct seqptr *sp) {
	return sp->pos->delta == sp->delta && sp->pos->ev.cmd != EV_NULL;
}

/* 
 * return the state structure of the next available event
 * or NULL if there is no next event in the current tick.
 * The state list is updated accordingly.
 */
struct state *
seqptr_evget(struct seqptr *sp) {
	struct state *st;

	if (sp->delta != sp->pos->delta || sp->pos->ev.cmd == EV_NULL) {
		return 0;
	}
	st = statelist_update(&sp->statelist, &sp->pos->ev);
	if (st->phase & EV_PHASE_FIRST) {
		st->pos = sp->pos;
		st->tic = sp->tic;
	}
	sp->pos = sp->pos->next;
	sp->delta = 0;
	return st;
}

/*
 * delete the next event from the track. If the 'slist'
 * argument is non NULL, then the state is updated as
 * it was read with seqptr_evget
 */
struct state *
seqptr_evdel(struct seqptr *sp, struct statelist *slist) {
	struct state *st;
	struct seqev *next;

	if (sp->delta != sp->pos->delta || sp->pos->ev.cmd == EV_NULL) {
		return NULL;
	}
	if (slist)
		st = statelist_update(slist, &sp->pos->ev);
	next = sp->pos->next;
	next->delta += sp->pos->delta;
	/* unlink and delete sp->pos */
	*(sp->pos->prev) = next;
	next->prev = sp->pos->prev;
	seqev_del(sp->pos);
	/* fix current position */
	sp->pos = next;
	return st;
}

/*
 * insert an event and put the cursor just after it,
 * the state list is updated and the state of the 
 * new event is returned.
 */
struct state *
seqptr_evput(struct seqptr *sp, struct ev *ev) {
	struct seqev *se;
	
	se = seqev_new();
	se->ev = *ev;
	se->delta = sp->delta;
	sp->pos->delta -= sp->delta;
	/* link to the list */
	se->next = sp->pos;
	se->prev = sp->pos->prev;
	*se->prev = se;
	sp->pos->prev = &se->next;
	/* fix the position pointer and update the state */
	sp->pos = se;
	return seqptr_evget(sp);
}

/*
 * move forward until the next event, but not more than 'max'
 * tics. The number of tics we moved is returned. States of all
 * terminated events are purged
 */
unsigned
seqptr_ticskip(struct seqptr *sp, unsigned max) {
	unsigned ntics;
	
	ntics = sp->pos->delta - sp->delta;
	if (ntics > max) {
		ntics = max;
	}
	if (ntics > 0) {
		sp->delta += ntics;
		sp->tic += ntics;
		statelist_outdate(&sp->statelist);
	}
	return ntics;
}

/*
 * remove blank space at the current position, 
 * same semantics as seqptr_ticskip()
 */
unsigned
seqptr_ticdel(struct seqptr *sp, unsigned max) {
	unsigned ntics;

	ntics = sp->pos->delta - sp->delta;
	if (ntics > max) {
		ntics = max;
	}
	sp->pos->delta -= ntics;
	return ntics;
}

/*
 * insert blank space at the current position
 */
void
seqptr_ticput(struct seqptr *sp, unsigned ntics) {
	if (ntics > 0) {
		sp->pos->delta += ntics;
		sp->delta += ntics;
		sp->tic += ntics;
		statelist_outdate(&sp->statelist);
	}
}

/*
 * move forward 'ntics', if the end-of-track is reached then return
 * the number of reamaining tics. Used for reading on a track
 */
unsigned 
seqptr_skip(struct seqptr *sp, unsigned ntics) {
	for (;;) {
		if (seqptr_eot(sp) || ntics == 0)
			break;
		while (seqptr_evget(sp))
			; /* nothing */
		ntics -= seqptr_ticskip(sp, ntics);
	}
	return ntics;
}

/*
 * move forward 'ntics', if the end-of-track is reached then fill with
 * blank space. Used for writeing on a track
 */
void
seqptr_seek(struct seqptr *sp, unsigned ntics) {
	ntics = seqptr_skip(sp, ntics);
	if (ntics > 0)
		seqptr_ticput(sp, ntics);
}

/*
 * erase the event contained in the given state. Everything happens
 * as the event never existed on the track. Returns the new state, 
 * or NULL if there is no more state.
 */
struct state *
seqptr_rmlast(struct seqptr *sp, struct state *st) {
	struct seqev *i, *prev, *cur, *next;

#ifdef FRAME_DEBUG
	dbg_puts("seqptr_rmlast: ");
	ev_dbg(&st->ev);
	dbg_puts(" removing last event\n");
#endif
	/*
	 * start a the first event of the frame and iterate until the
	 * current postion. Store in 'cur' the event to delete and in
	 * 'prev' the event before 'cur' that belongs to the same
	 * frame
	 */
	i = cur = st->pos;
	prev = NULL;
	for (;;) {
		i = i->next;
		if (i == sp->pos) {
			break;
		}
		if (ev_sameclass(&st->ev, &i->ev)) {
			prev = cur;
			cur = i;
		}
	}
	/* 
	 * remove the event from the track
	 * (but not the blank space)
	 */
	next = cur->next;
	next->delta += cur->delta;
	if (next == sp->pos) {
		sp->delta += cur->delta;
	}
	next->prev = cur->prev;
	*(cur->prev) = next;
	seqev_del(cur);
	/*
	 * update the state; if we deleted the first event of the
	 * frame, the state no more exists, so purge it
	 */
	if (prev == NULL) {
		statelist_rm(&sp->statelist, st);
		state_del(st);
		return NULL;
	} else {
		st->ev = prev->ev;
		st->phase = st->pos == prev ? EV_PHASE_FIRST : EV_PHASE_NEXT;
	}
	return st;
}

/*
 * erase the frame contained in the given state until the given
 * position. Everything happens as the frame never existed on the
 * track. Returns always NULL, for consistency with seqptr_rmlast().
 */
struct state *
seqptr_rmprev(struct seqptr *sp, struct state *st) {
	struct seqev *i, *next;

#ifdef FRAME_DEBUG
	dbg_puts("seqptr_rmprev: ");
	ev_dbg(&st->ev);
	dbg_puts(" removing frame\n");
#endif
	/*
	 * start a the first event of the frame and iterate until the
	 * current postion removing all events of the frame.
	 */
	i = st->pos;
	for (;;) {
		if (ev_sameclass(&st->ev, &i->ev)) {
			/* 
			 * remove the event from the track
			 * (but not the blank space)
			 */
			next = i->next;
			next->delta += i->delta;
			if (next == sp->pos) {
				sp->delta += i->delta;
			}
			next->prev = i->prev;
			*(i->prev) = next;
			seqev_del(i);
			i = next;
		} else {
			i = i->next;
		}
		if (i == sp->pos) {
			break;
		}
	}
	statelist_rm(&sp->statelist, st);
	state_del(st);
	return NULL;
}



/*
 * merge "low priority" event: 
 * check that the event of state "s1" doen't conflict with
 * event in thate "s2". If so, then it is put in the
 * track. Else "s1" is tagged as silent so a next call
 * to this routine will just skip it
 */
void
seqptr_evmerge1(struct seqptr *pd, struct state *s1, struct state *s2) {
	if (s1->phase & EV_PHASE_FIRST) {
		s1->silent = (s2 && s2->phase != EV_PHASE_LAST) ? 1 : 0;
#ifdef FRAME_DEBUG
		if (s1->silent) {
			dbg_puts("seqptr_evmerge1: ");
			ev_dbg(&s1->ev);
			dbg_puts(" started in silent state\n");
		}
#endif
	}
	if (!s1->silent) {
		(void)seqptr_evput(pd, &s1->ev);
	}
}

/*
 * merge "high priority" event:
 * check that the event of state "s2" doesn't conflict with
 * events of state "s1". If so, then put it on the track. If
 * there is conflict then discard events related to "s1" and
 * put "s2"
 */
void
seqptr_evmerge2(struct seqptr *pd, struct state *s1, struct state *s2) {
	struct state *sd;

	if (s1 == NULL || (s1->phase == EV_PHASE_LAST && !(s1->flags & STATE_CHANGED))) {
		(void)seqptr_evput(pd, &s2->ev);
		return;
	}
	if (!(s2->phase & EV_PHASE_LAST) || s1->phase & EV_PHASE_LAST) {
		sd = statelist_lookup(&pd->statelist, &s2->ev);
		if (sd == NULL) {
			dbg_puts("seqptr_merge2: no conflicting events\n");
			dbg_panic();
		}
		if (EV_ISNOTE(&s2->ev)) {
			if (!(s1->phase & EV_PHASE_LAST)) 
				sd = seqptr_rmprev(pd, sd);
		} else {
			if (s1->flags & STATE_CHANGED)
				sd = seqptr_rmlast(pd, sd);
		}
		if (sd == NULL || !ev_eq(&sd->ev, &s2->ev)) {
			(void)seqptr_evput(pd, &s2->ev);
		}
		s1->silent = 1;
	} else {
		if (EV_ISNOTE(&s2->ev)) {
			(void)seqptr_evput(pd, &s2->ev);
		} else {
			sd = statelist_lookup(&pd->statelist, &s2->ev);
			if (sd == NULL) {
				dbg_puts("seqptr_merge2: no state for silent ctl\n");
				dbg_panic();
			}
			if (!s1->silent) {
				dbg_puts("seqptr_merge2: ctl to restore not silent\n");
				dbg_panic();
			}
			if (!(s1->flags & STATE_CHANGED) && !ev_eq(&sd->ev, &s1->ev)) {
				(void)seqptr_evput(pd, &s1->ev);
				s1->silent = 0;
			}
		}
	}
}

/*
 * Merge track "src" (high priority) in track "dst" (low prority)
 * resolving all conflicts, so that "dst" is consistent.
 */	
void
track_merge(struct track *dst, struct track *src) {
	struct state *s1, *s2;
	struct seqptr p2, pd;
	struct statelist orglist;
	unsigned delta1, delta2, deltad;

	seqptr_init(&pd, dst);
	seqptr_init(&p2, src);
	statelist_init(&orglist);

	for (;;) {
		if (seqptr_eot(&pd) && seqptr_eot(&p2))
			break;
	
		/*
		 * remove all events from 'dst' and put them back on
		 * on 'dst' by merging them with the state table of
		 * 'src'. The 'orglist' state table is updated so it
		 * always contain the exact state of the original
		 * 'dst' track.
		 */
		for(;;) {
			s1 = seqptr_evdel(&pd, &orglist);
			if (s1 == NULL)
				break;
			s2 = statelist_lookup(&p2.statelist, &s1->ev);
			seqptr_evmerge1(&pd, s1, s2);
		}

		/*
		 * move all events from 'src' to 'dst' by merging them
		 * with the original state of 'dst'.
		 */
		for (;;) {
			s2 = seqptr_evget(&p2);
			if (s2 == NULL)
				break;
			s1 = statelist_lookup(&orglist, &s2->ev);
			seqptr_evmerge2(&pd, s1, s2);
		}
		
		/*
		 * move to the next non empty tick
		 * XXX: simplify
		 */
		delta1 = pd.pos->delta - pd.delta;
		delta2 = p2.pos->delta - p2.delta;
		if (delta1 > delta2 && delta2 > 0) {
			delta1 = delta2;
		} else if (delta2 > delta1 && delta1 > 0) {
			delta2 = delta1;
		}
		deltad = delta1 > delta2 ? delta1 : delta2;
		seqptr_ticskip(&p2, delta2);
		deltad -= seqptr_ticskip(&pd, deltad);
		if (deltad) {
			seqptr_ticput(&pd, deltad);
		}
		statelist_outdate(&orglist);
	}

	statelist_done(&orglist);
	seqptr_done(&p2);
	seqptr_done(&pd);
}

/*
 * copy a portion of the given track. The copy is consistent, no
 * frames are copied from the midle: notes are always completely copied,
 * and controllers (and other) are restored
 */
void
track_copy(struct track *src, unsigned start, unsigned len, struct track *dst) {
	unsigned delta, amount;
	struct seqptr sp, dp;
	struct state *st;
	struct ev ev;

	track_clearall(dst);
	if (len == 0)
		return;
	seqptr_init(&sp, src);
	seqptr_init(&dp, dst);

	/*
	 * stage 1: go the the start position and tag all events as
	 * silent
	 */
	(void)seqptr_skip(&sp, start);
	for (st = sp.statelist.first; st != NULL; st = st->next) {
		st->silent = 1;
	}

	/*
	 * stage 2: move the first tic accepting starting frames; this
	 * is the last chance for open frames to terminate and to avoid
	 * to be restored
	 */
	while (seqptr_evavail(&sp)) {
		st = seqptr_evget(&sp);
		if ((EV_ISNOTE(&st->ev) && (st->phase & EV_PHASE_FIRST)) ||
		    (!EV_ISNOTE(&st->ev) && (st->phase & (EV_PHASE_FIRST | EV_PHASE_NEXT))))
			st->silent = 0;
		if (!st->silent) {
			seqptr_evput(&dp, &st->ev);
		}
	}

	/*
	 * stage 3: restore all states that weren't updated by the
	 * first tic
	 */
	for (st = sp.statelist.first; st != NULL; st = st->next) {
		if (EV_ISNOTE(&st->ev))
			continue;
		if (!(st->flags & STATE_CHANGED) && !(st->phase & EV_PHASE_LAST)) {
			if (ev_restore(&st->ev, &ev)) {
       				seqptr_evput(&dp, &ev);
				st->silent = 0;
			}
		}
	}

	/*
	 * stage 4: go ahead and copy all events during 'len' tics
	 */
	amount = 0;
	for (;;) {
		delta = seqptr_ticskip(&sp, len);
		amount += delta;
		len -= delta;
		if (len == 0)
			break;
		
		if (!seqptr_evavail(&sp))
			break;
		st = seqptr_evget(&sp);
		if (st->phase & EV_PHASE_FIRST)
			st->silent = 0;
		if (!st->silent) {
			seqptr_seek(&dp, amount);
			seqptr_evput(&dp, &st->ev);
			amount = 0;
		}
	}
	

	/*
	 * stage 5: cancel all states (that arent silent) on the 'dst'
	 * track, states that are canceled are tagged as silent, so we
	 * can continue copying selected events in the next stage
	 */
	for (st = sp.statelist.first; st != NULL; st = st->next) {
		if (EV_ISNOTE(&st->ev))
			continue;
		if (!(st->phase & EV_PHASE_LAST)) {
			if (ev_cancel(&st->ev, &ev)) { 
				seqptr_seek(&dp, amount);
				seqptr_evput(&dp, &ev);
				amount = 0;
			}
			st->silent = 1;
		}
	}

	/*
	 * state 6: copy all event for whose state couldn't be
	 * canceled (note events)
	 */
	for (;;) {
		delta = seqptr_ticskip(&sp, ~0U);
		amount += delta;
		
		if (!seqptr_evavail(&sp))
			break;
		st = seqptr_evget(&sp);
		if (st->phase & EV_PHASE_FIRST)
			st->silent = 1;
		if (!st->silent) {
			seqptr_seek(&dp, amount);
			seqptr_evput(&dp, &st->ev);
			amount = 0;
		}
	}
}


/*
 * blank a portion of the given track. The copy is consistent, no
 * frames are copied from the midle: notes are always completely copied,
 * and controllers (and other) are restored
 */
void
track_blank(struct track *src, unsigned start, unsigned len) {
	struct seqptr sp;
	struct statelist slist;
	struct state *st;
	struct ev ev;

	if (len == 0)
		return;
	seqptr_init(&sp, src);

	/*
	 * stage 1: go the the start position and tag all events as
	 * not silent
	 */
	(void)seqptr_skip(&sp, start);
	statelist_dup(&slist, &sp.statelist);
	for (st = slist.first; st != NULL; st = st->next) {
		st->silent = 0;
	}

	/*
	 * stage 2: cancel all states that will be erased from the selection
	 */
	for (st = slist.first; st != NULL; st = st->next) {
		if (EV_ISNOTE(&st->ev))
			continue;
		if (!(st->phase & EV_PHASE_LAST)) {
			if (ev_cancel(&st->ev, &ev)) {
				seqptr_evput(&sp, &ev);
				st->silent = 1;
			}
		}
	}

	/*
	 * stage 3: go ahead and erase events of selected frames
	 * during 'len' tics
	 */
	for (;;) {
		len -= seqptr_ticskip(&sp, len);
		if (len == 0)
			break;
		st = seqptr_evdel(&sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST)
			st->silent = 1;
		dbg_puts("track_blank: ");
		ev_dbg(&st->ev);
		dbg_puts(" silent=");
		dbg_putu(st->silent);
		dbg_puts("\n");
		if (!st->silent)
			seqptr_evput(&sp, &st->ev);
	}
	
	/*
	 * stage 4: move the first tic of the 'end' boundary,
	 * accepting starting frames; this is the last chance for
	 * opened frames to terminate and to avoid to be restored
	 */
	while (seqptr_evavail(&sp)) {
		st = seqptr_evdel(&sp, &slist);
		if ((EV_ISNOTE(&st->ev) && (st->phase & EV_PHASE_FIRST)) ||
		    (!EV_ISNOTE(&st->ev) && (st->phase & (EV_PHASE_FIRST | EV_PHASE_NEXT))))
			st->silent = 0;
		if (!st->silent) {
			seqptr_evput(&sp, &st->ev);
		}
	}

	/*
	 * stage 5: retore all states that arent silent. states that
	 * are retored are tagged as not silent, so we can continue
	 * copying selected events in the next stage
	 */
	for (st = slist.first; st != NULL; st = st->next) {
		if (EV_ISNOTE(&st->ev) || !st->silent)
			continue;
		if (!(st->phase & EV_PHASE_LAST)) {
			if (ev_restore(&st->ev, &ev)) { 
				seqptr_evput(&sp, &ev);
				st->silent = 0;
			}
		}
	}

	/*
	 * state 6: copy all events for not silent frames
	 */
	for (;;) {
		(void)seqptr_ticskip(&sp, ~0U);
		if (!seqptr_evavail(&sp))
			break;
		st = seqptr_evdel(&sp, &slist);
		if (st->phase & EV_PHASE_FIRST)
			st->silent = 0;
		if (!st->silent)
			seqptr_evput(&sp, &st->ev);
	}
}



/*
 * calculate the offset that the givent tic must be 
 * moved so it is quantized.
 *
 * XXX: this is only used once in track_quantise.
 *	cut&paste the code there and remove this routine
 */
int
quantize_ofs(unsigned tic, unsigned quantum, unsigned rate) {
	unsigned remaind;
	int ofs;
	
	if (quantum != 0) {
		remaind = tic % quantum;
	} else {
		remaind = 0;
	}
	if (remaind <= quantum / 2) {
		ofs = - ((remaind * rate + 99) / 100);
	} else {
		ofs = ((quantum - remaind) * rate + 99) / 100;
	}
	return ofs;
}

/*
 * quantize the given track
 */
void
track_quantize(struct track *src, unsigned start, unsigned len,
    unsigned offset, unsigned quantum, unsigned rate) {
	unsigned delta, tic;
	struct track qt;
	struct seqptr sp, qp;
	struct state *st;
	struct statelist slist;
	int ofs;

	track_init(&qt);
	seqptr_init(&sp, src);
	seqptr_init(&qp, &qt);

	/*
	 * go to t start position and tag all events as silent
	 * (silent = not quantisable)
	 */
	(void)seqptr_skip(&sp, start);
	statelist_dup(&slist, &sp.statelist);
	for (st = sp.statelist.first; st != NULL; st = st->next) {
		st->silent = 1;
	}
	seqptr_seek(&qp, start);
	tic = start;
	ofs = 0;

	/*
	 * go ahead and copy all events to quantize during 'len' tics,
	 * while stretching the time scale in the destination track
	 */
	for (;;) {
		delta = seqptr_ticskip(&sp, len);
		tic += delta;
	
		if (tic >= start + len || !seqptr_evavail(&sp))
			break;
		
		delta -= ofs;
		ofs = quantize_ofs(tic - start + offset, quantum, rate);
#ifdef FRAME_DEBUG
		if (ofs < 0 && delta < (unsigned)(-ofs)) {
			dbg_puts("track_quantize: delta < -ofs\n");
			dbg_panic();
		}
#endif
		delta += ofs;
		seqptr_seek(&qp, delta);

		st = seqptr_evdel(&sp, &slist);
		if (st->phase & EV_PHASE_FIRST) {
			st->silent = !EV_ISNOTE(&st->ev) ? 1 : 0;
		}
		if (st->silent) {
			seqptr_evput(&sp, &st->ev);
		} else {
			seqptr_evput(&qp, &st->ev);
		}
		if (st->ev.cmd == EV_NON) {
			ev_dbg(&st->ev);
#ifdef FRAME_DEBUG
			dbg_puts(": tic=");
			dbg_putu(tic % quantum);
			dbg_puts(", ofs=");
			dbg_puts(ofs < 0 ? "-" : "+");
			dbg_putu(ofs < 0 ? -ofs : ofs);
			dbg_puts("\n");
#endif
		}
	}	    

	/*
	 * finish quantised (not silent) events
	 */
	for (;;) {
		delta = seqptr_ticskip(&sp, ~0U);
		if (!seqptr_evavail(&sp))
			break;
		st = seqptr_evdel(&sp, &slist);
		if (st->phase & EV_PHASE_FIRST)
			st->silent = 1;
		seqptr_seek(&qp, delta);
		if (st->silent) {
			seqptr_evput(&sp, &st->ev);
		} else {
			seqptr_evput(&qp, &st->ev);
		}
	}
	track_merge(src, &qt);
	statelist_done(&slist);
	track_done(&qt);
}

/*
 * transpose the given track
 */
void
track_transpose(struct track *src, unsigned start, unsigned len, int halftones) {
	unsigned delta, tic;
	struct track qt;
	struct seqptr sp, qp;
	struct state *st;
	struct statelist slist;
	struct ev ev;

	track_init(&qt);
	seqptr_init(&sp, src);
	seqptr_init(&qp, &qt);

	/*
	 * go to t start position and tag all events as silent
	 * (silent = not quantisable)
	 */
	(void)seqptr_skip(&sp, start);
	statelist_dup(&slist, &sp.statelist);
	for (st = sp.statelist.first; st != NULL; st = st->next) {
		st->silent = 1;
	}
	seqptr_seek(&qp, start);
	tic = start;

	/*
	 * go ahead and copy all events to transpose during 'len' tics,
	 */
	for (;;) {
		delta = seqptr_ticskip(&sp, len);
		seqptr_seek(&qp, delta);
		tic += delta;
	
		if (tic >= start + len || !seqptr_evavail(&sp))
			break;
		
		st = seqptr_evdel(&sp, &slist);
		if (st->phase & EV_PHASE_FIRST) {
			st->silent = !EV_ISNOTE(&st->ev) ? 1 : 0;
		}
		if (st->silent) {
			seqptr_evput(&sp, &st->ev);
		} else {
			ev = st->ev;
			ev.data.voice.b0 += halftones;
			ev.data.voice.b0 &= 0x7f;
			seqptr_evput(&qp, &ev);
		}
	}	    

	/*
	 * finish transposed (not silent) events
	 */
	for (;;) {
		delta = seqptr_ticskip(&sp, ~0U);
		seqptr_seek(&qp, delta);
		if (!seqptr_evavail(&sp))
			break;
		st = seqptr_evdel(&sp, &slist);
		if (st->phase & EV_PHASE_FIRST)
			st->silent = 1;
		if (st->silent) {
			seqptr_evput(&sp, &st->ev);
		} else {
			ev = st->ev;
			ev.data.voice.b0 += halftones;
			ev.data.voice.b0 &= 0x7f;
			seqptr_evput(&qp, &ev);
		}
	}
	track_merge(src, &qt);
	statelist_done(&slist);
	track_done(&qt);
}

/*
 * check (and fix) the given track for inconsistencies
 */
void
track_check(struct track *src) {
	struct seqptr sp;
	struct state *st, *stnext;
	struct statelist slist;

	seqptr_init(&sp, src);
	statelist_init(&slist);

	/*
	 * reconstruct the track skipping bogus events,
	 * see statelist_update() for definition of bogus
	 */
	for (;;) {
		(void)seqptr_ticskip(&sp, ~0U);
		st = seqptr_evdel(&sp, &slist);
		if (st == NULL)
			break;
		if (st->flags & STATE_NEW) {
			st->silent = st->flags & STATE_BOGUS ? 1: 0;
			if (st->flags & STATE_BOGUS) {
				ev_dbg(&st->ev);
				dbg_puts(": bogus\n");
			}
		}
		if (!st->silent) {
			seqptr_evput(&sp, &st->ev);
		}	
	}

	/*
	 * undo (erase) all unterminated frames 
	 */
	for (st = sp.statelist.first; st != NULL; st = stnext) {
		stnext = st->next;
		if (!(st->phase & EV_PHASE_LAST)) {
			ev_dbg(&st->ev);
			dbg_puts(": unterminated\n");
			(void)seqptr_rmprev(&sp, st);
		}
	}

	statelist_done(&slist);
	seqptr_done(&sp);
}

/*
 * get the current tempo (at the current position)
 */
struct state *
seqptr_getsign(struct seqptr *sp, unsigned *bpm, unsigned *tpb) {
	struct ev ev;
	struct state *st;

	ev.cmd = EV_TIMESIG;
	st = statelist_lookup(&sp->statelist, &ev);
	if (bpm) 
		*bpm = (st == NULL) ? DEFAULT_BPM : st->ev.data.sign.beats;
	if (tpb)
		*tpb = (st == NULL) ? DEFAULT_TPB : st->ev.data.sign.tics;
	return st;
}

/*
 * look forward for a TIMESIG event return 1 if there is one in the
 * current tic else return 0.
 * 
 * XXX: this is too ugly, see comments in seqptr_skipmeasure()
 */
unsigned
seqptr_issign(struct seqptr *sp) {
	struct seqev *se;
	unsigned delta;

	se = sp->pos;
	delta = sp->delta;
	for (;;) {
		if (delta < se->delta || se->ev.cmd == NULL)
			return 0;
		if (se->ev.cmd == EV_TIMESIG)
			return 1;
		delta -= se->delta;
		se = se->next;
	}
}

/*
 * get the current tempo (at the current position)
 */
struct state *
seqptr_gettempo(struct seqptr *sp, unsigned long *usec24) {
	struct ev ev;
	struct state *st;

	ev.cmd = EV_TEMPO;
	st = statelist_lookup(&sp->statelist, &ev);
	if (usec24)
		*usec24 = (st == NULL) ? DEFAULT_USEC24 : st->ev.data.tempo.usec24;
	return st;
}

/*
 * try to move 'm0' measures forward; the current postition MUST be
 * the beginning of a measure and the state table must be up to date.
 * Return the number of tics remaining until the requested measure
 * (only on premature end-of-track)
 */
unsigned
seqptr_skipmeasure(struct seqptr *sp, unsigned m0) {
	unsigned ticrel, ticabs, m, dm, tics_per_meas, bpm, tpb;

	/* 
	 * XXX: remove the ugly seqptr_issign() routine, and
	 * do something simpler here:
	 *
	 *	while (m > 0) {
	 *		seqptr_getsign();
	 *		seqptr_skip(tics_per_meas);
	 *	}
	 */

	m = ticabs = ticrel = 0;
	for (;;) {
		seqptr_getsign(sp, &bpm, &tpb);
		tics_per_meas = bpm * tpb;
		if (seqptr_eot(sp)) {
			return (m0 - m) * tics_per_meas - ticrel;
		}

		/* 
		 * move forward to the next event, but not
		 * beyound the requested measure
		 */
		ticrel += seqptr_ticskip(sp, (m0 - m) * tics_per_meas - ticrel);
		dm      = ticrel / tics_per_meas;
		ticrel  = ticrel % tics_per_meas;
		ticabs += dm * tics_per_meas;
		m      += dm;
		if (m == m0)
			return 0;

		/*
		 * we have to deal with short measures; so we check
		 * without moving forward nor updating the state list
		 * if there is a time signature change; if so we
		 * terminate the current measure (considered short)
		 */
		if (ticrel != 0 && seqptr_issign(sp)) {
			m++;
			dbg_puts("track_findmeasure: measure #");
			dbg_putu(m);
			dbg_puts("is short\n");
			if (m == m0)
				return 0;
			ticabs += ticrel;
			ticrel = 0;
		}

		/*
		 * move to the end of the tic
		 */
		while (seqptr_evget(sp))
			; /* nothing */
	}
	

}

/*
 * convert a measure number to a tic number using
 * meta-events from the given track
 */
unsigned
track_findmeasure(struct track *t, unsigned m) {
	struct seqptr sp;
	unsigned tic;

	seqptr_init(&sp, t);
	tic  = seqptr_skipmeasure(&sp, m);
	tic += sp.tic;
	seqptr_done(&sp);

#ifdef FRAME_DEBUG
	dbg_puts("track_findmeasure: ");
	dbg_putu(m);
	dbg_puts(" -> ");
	dbg_putu(tic);
	dbg_puts("\n");
#endif

	return tic;
}

/*
 * return the tempo and time signature corresponding to the given tic
 */
void
track_timeinfo(struct track *t, unsigned tic, 
    unsigned long *usec24, unsigned *bpm, unsigned *tpb) {
	struct seqptr sp;

	seqptr_init(&sp, t);
	(void)seqptr_skip(&sp, tic);
	seqptr_getsign(&sp, bpm, tpb);
	seqptr_gettempo(&sp, usec24);
	seqptr_done(&sp);
}

/*
 * go to the given measure and set the tempo
 */
void
track_settempo(struct track *t, unsigned measure, unsigned tempo) {
	struct seqptr sp;
	struct ev ev;
	unsigned long usec24, old_usec24;
	unsigned tic, bpm, tpb;

	/*
	 * go to the requested position, insert blank if necessary
	 */
	seqptr_init(&sp, t);
	tic = seqptr_skipmeasure(&sp, measure);
	if (tic) {
		seqptr_ticput(&sp, tic);
	}
	/*
	 * remove all tempo events at the current tic
	 */
	while (seqptr_evavail(&sp)) {
		if (sp.pos->ev.cmd == EV_TEMPO)
			seqptr_evdel(&sp, NULL);
		else 
			seqptr_evget(&sp);
	}

	/*
	 * if needed, insert a new tempo event
	 */
	seqptr_getsign(&sp, &bpm, &tpb);
	usec24 = TEMPO_TO_USEC24(tempo, tpb);
	seqptr_gettempo(&sp, &old_usec24);
	if (usec24 != old_usec24) {
		ev.cmd = EV_TEMPO;
		ev.data.tempo.usec24 = usec24;
		seqptr_evput(&sp, &ev);
	}
	/*
	 * remove the next tempo event if it has the current value
	 */
	for (;;) {
		seqptr_ticskip(&sp, ~0);
		if (!seqptr_evavail(&sp))
			break;
		if (sp.pos->ev.cmd == EV_TEMPO) {
			if (sp.pos->ev.data.tempo.usec24 == usec24)
				seqptr_evdel(&sp, NULL);
			break;
		}
		seqptr_evget(&sp);
	}
	seqptr_done(&sp);
}


/*
 * insert measures in the given tempo track
 */
void
track_timeins(struct track *t, unsigned measure, unsigned amount,
    unsigned bpm, unsigned tpb) {
	struct seqptr sp;
	struct state *sign;
	struct ev ev;
	unsigned tic, save_bpm, save_tpb;

	/*
	 * go to the requested position, insert blank if necessary
	 */
	seqptr_init(&sp, t);
	tic = seqptr_skipmeasure(&sp, measure);
	if (tic) {
		seqptr_ticput(&sp, tic);
	}
	sign = seqptr_getsign(&sp, &save_bpm, &save_tpb);

	if (bpm != save_bpm || tpb != save_tpb) {
		ev.cmd = EV_TIMESIG;
		ev.data.sign.beats = bpm;
		ev.data.sign.tics = tpb;
		sign = seqptr_evput(&sp, &ev);
	}
	seqptr_ticput(&sp, bpm * tpb * amount);

	/* 
	 * restore the time signature
	 */
	if (bpm != save_bpm || tpb != save_tpb) {
		ev.cmd = EV_TIMESIG;
		ev.data.sign.beats = save_bpm;
		ev.data.sign.tics = save_tpb;
		sign = seqptr_evput(&sp, &ev);
	}
	for (;;) {
		if (!seqptr_evavail(&sp))
			break;
		if (sign != NULL && sp.pos->ev.cmd == EV_TIMESIG) {
			sign = seqptr_rmlast(&sp, sign);
			break;
		}
		seqptr_evget(&sp);
	}
	
	/*
	 * remove the next timesig if it has the current value
	 */
	for (;;) {
		seqptr_ticskip(&sp, ~0);
		if (!seqptr_evavail(&sp))
			break;
		if (sp.pos->ev.cmd == EV_TIMESIG) {
			if (sp.pos->ev.data.sign.beats == bpm &&
			    sp.pos->ev.data.sign.tics == tpb)
				seqptr_evdel(&sp, NULL);
			break;
		}
		seqptr_evget(&sp);
	}
	seqptr_done(&sp);
}

/*
 * remove measures from the given tempo track
 */
void
track_timerm(struct track *t, unsigned measure, unsigned amount) {
	struct seqptr sp;
	struct statelist slist;
	struct state *st, *ost;
	unsigned tic, len;
	struct ev ev;

	/*
	 * go to the requested position and determine the number
	 * of tics to delete;
	 */
	seqptr_init(&sp, t);
	len = seqptr_skipmeasure(&sp, measure);
	if (len != 0)
		return;
	tic = sp.tic;
	(void)seqptr_skipmeasure(&sp, amount);
	len = sp.tic - tic;
	seqptr_done(&sp);
	
#ifdef FRAME_DEBUG
	dbg_puts("track_timerm: ");
	dbg_putu(tic);
	dbg_puts(" / ");
	dbg_putu(len);
	dbg_puts("\n");
#endif
	/*
	 * go to the start position; tag all frames as not silent
	 */
	seqptr_init(&sp, t);
	(void)seqptr_skip(&sp, tic);
	statelist_dup(&slist, &sp.statelist);
	for (st = slist.first; st != NULL; st = st->next) {
		st->silent = 0;
	}

	/*
	 * remove everything during 'len' tics
	 */
	for (;;) {
		len -= seqptr_ticdel(&sp, len);
		if (len == 0)
			break;
		if (!seqptr_evavail(&sp))
			break;
		st = seqptr_evdel(&sp, &slist);
		st->silent = 1;
	}
	
	/*
	 * process the next tic; this will give a chance to some
	 * frames to be restored by themselves (before trying to
	 * restore them "by hand")
	 */
	while (seqptr_evavail(&sp)) {
		st = seqptr_evdel(&sp, &slist);
		ost = statelist_lookup(&sp.statelist, &st->ev);
		if (!ost || !ev_eq(&st->ev, &ost->ev))
			seqptr_evput(&sp, &st->ev);
		st->silent = 0;
	}

	/*
	 * retore all states that arent silent. states that
	 * are restored are tagged as not silent, so we can continue
	 * copying selected events in the next stage
	 */
	for (st = slist.first; st != NULL; st = st->next) {
		if (st->silent && ev_restore(&st->ev, &ev)) {
			ost = statelist_lookup(&sp.statelist, &ev);
			if (!ost || !ev_eq(&ev, &ost->ev))
				seqptr_evput(&sp, &ev);
			st->silent = 0;
		}
	}
	
	/*
	 * state 6: copy all events for not silent frames
	 */
	for (;;) {
		(void)seqptr_ticskip(&sp, ~0U);
		if (!seqptr_evavail(&sp))
			break;
		st = seqptr_evdel(&sp, &slist);
		st->silent = 0;
		ost = statelist_lookup(&sp.statelist, &ev);
		if (!ost || !ev_eq(&st->ev, &ost->ev))
			seqptr_evput(&sp, &st->ev);
	}
}


/*
 * add an event to the first event of a track (config track)
 * in an ordered way
 */
void
track_confev(struct track *src, struct ev *ev) {
	struct seqptr sp;
	struct seqev *se;
	struct statelist slist;
	struct state *i, *inext, *st;
	unsigned prio;

	seqptr_init(&sp, src);
	statelist_init(&slist);

	/*
	 * load all states of the first tick, until the first note
	 */
	for (;;) {
		if (!seqptr_evavail(&sp) || EV_ISNOTE(&sp.pos->ev))
			break;
		seqptr_evdel(&sp, &slist);
	}

	/*
	 * update the state for 'ev'
	 */
	if (ev) {
		st = statelist_lookup(&slist, ev);
		if (st == NULL) {
			st = state_new();
			statelist_add(&slist, st);
			st->flags = STATE_NEW;
		}
		st->ev = *ev;
		st->phase = ev_phase(ev);
		if (st->phase != (EV_PHASE_FIRST | EV_PHASE_LAST)) {
			dbg_puts("track_confev: ");
			ev_dbg(&st->ev);
			dbg_puts(": bad phase");
			dbg_puts("\n");
		}
	}
		
	/*
	 * dump events, order them by priority
	 */
	for (prio = EV_PRIO_ANY; prio <= EV_PRIO_MAX; prio++) {
		for (i = slist.first; i != NULL; i = inext) {
			inext = i->next;
			if (ev_prio(&i->ev) == prio) {
				se = seqev_new();
				se->delta = 0;
				se->ev = i->ev;
				/* insert the event */
				se->next = src->first;
				src->first->prev = &se->next;
				src->first = se;
				se->prev = &src->first;
				/* remove the state */
				statelist_rm(&slist, i);
				state_del(i);
			}
		}
	}

	statelist_done(&slist);
	seqptr_done(&sp);
}
