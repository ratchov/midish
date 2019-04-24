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
 * a seqptr structure points to a location of the associated track and
 * can move forward, read events and write events. The location is
 * defined by the current tic and the current event within the
 * tick. In some sense a seqptr structure is for a track what is a
 * head for a tape.
 *
 * The seqptr structure contain a cursor (a pos/delta pair) which is
 * used to walk through the track as follows:
 *
 *    -	In play mode, the field 'o->pos' points to the event to be
 *	played. The 'delta' field contains the number of ticks elapsed
 *	since the last played event. Thus when o->delta reaches
 *	o->pos->delta, the event is to be played.
 *
 *    -	In record mode, the field 'o->pos' points to the next event
 *	that the event being recorded.
 *
 * It maintains a "state list" that contains the complete state of the
 * track at the current postion: the list of all sounding notes, the
 * state of all controllers etc... This allows to ensure full
 * consistency when a track is modified. So, always use the following
 * 6 primitives to modify a track.
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
 *	events. There can be multiple seqptr structures reading the
 *	same track.
 *
 * Writing:
 *
 *	there are 2 low-level routines for writing: seqptr_ticput()
 *	adds empty tics and seqptr_evput() adds an event at the
 *	current postion. The state list is updated as new events were
 *	read with seqptr_evget(). If there is a writer, there must not
 *	be readers. In order to keep track consistency, events must
 *	only be appended at the end-of-track; indeed, if we write an
 *	arbitrary event in the middle of a track, generally it isn't
 *	possible to solve all conflicts.
 *
 * Erasing:
 *
 *	there are 2 low-level routines for erasing: seqptr_ticdel()
 *	deletes empty tics and seqptr_evdel() deletes the next event.
 *	The state list is not updated since the current position
 *	haven't changed. However, to keep state of erased events both
 *	functions take as optionnal argument a state list, that is
 *	updated as events and blank space were read with
 *	seqptr_evget() and seqptr_ticskip()
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
 *			st = seqptr_evdel(sp, &slist);
 *			seqptr_evput(sp, &st->ev);
 *			...
 *		}
 *
 *	  is ok only if _all_ events are removed. This is the only
 *	  correct way of consistetly modifying a track.
 *
 *	- when rewriting a track one must use a separate statelist
 *	  for events being removed, so the seqptr->statelist is used for
 *	  writing new events. When starting rewriting a track on a
 *	  given position, be sure to initialise the 'deleted events'
 *	  statelist with statelist_dup(), and not statelist_init().
 *	  Example:
 *
 *		seqptr_skip(sp, pos);
 *		statelist_dup(&slist);
 *		for (;;) {
 *			seqptr_evdel(sp, &slist);
 *			...
 *		}
 *
 *	- when working with a statelist initialised with statlist_dup()
 *	  be aware that tags are not copied. The only fields that are copied
 *	  are those managed by statelist_update() routine. So we must first
 *	  duplicate the statelist and then tag states. Example:
 *
 *		seqptr_skip(sp, pos);
 *		statelist_dup(&slist, &sp->slist);
 *		for (st = slist.first; st != NULL; st = st->next) {
 *			st->tag = ...
 *		}
 *
 *	  it is _not_ ok, to iterate over sp->statelist and then to dup it.
 *
 *	- seqptr_tic{skip,put,del}() outdates the statelist of the
 *	  seqptr. This purges unused states and updates the
 *	  STATE_CHANGED flag. However, if we use seqptr_ticskip() with
 *	  seqptr_evdel(), we'll not outdate the right state list. So we
 *	  have to rewrite blank space too (not only events) as
 *	  follows:
 *
 *		for (;;) {
 *			delta = seqptr_ticdel(sp, &slist);
 *			seqptr_ticput(sp, delta);
 *
 *			st = seqptr_evdel(sp, &slist);
 *			seqptr_evput(sp, st->ev);
 *		}
 *
 */

/*
 * TODO:
 *
 *	- seqptr_merge1() and seqptr_merge2 are supposed to use
 *	  seqptr_restore() and seqptr_cancel()
 *
 *	- seqptr_cancel() and seqptr_restore() shouldn't
 *	  write new events if the current state is
 *	  the same as the event we write
 */

#include "utils.h"
#include "track.h"
#include "defs.h"
#include "filt.h"
#include "frame.h"
#include "pool.h"

struct pool seqptr_pool;

void
seqptr_pool_init(unsigned size)
{
	pool_init(&seqptr_pool, "seqptr", sizeof(struct seqptr), size);
}

void
seqptr_pool_done(void)
{
	pool_done(&seqptr_pool);
}

/*
 * initialize a seqptr structure at the beginning of
 * the given track.
 */
struct seqptr *
seqptr_new(struct track *t)
{
	struct seqptr *sp;

	sp = (struct seqptr *)pool_new(&seqptr_pool);
	statelist_init(&sp->statelist);
	sp->pos = t->first;
	sp->delta = 0;
	sp->tic = 0;
	return sp;
}

/*
 * release the seqptr structure, free statelist etc...
 */
void
seqptr_del(struct seqptr *sp)
{
	statelist_done(&sp->statelist);
	pool_del(&seqptr_pool, sp);
}

/*
 * return true if the end of the track is reached
 */
int
seqptr_eot(struct seqptr *sp)
{
	return sp->delta == sp->pos->delta && sp->pos->ev.cmd == EV_NULL;
}

/*
 * return the state structure of the next available event or NULL if
 * there is no next event in the current tick.  The state list is
 * updated accordingly.
 */
struct state *
seqptr_evget(struct seqptr *sp)
{
	struct state *st;

	if (sp->delta != sp->pos->delta || sp->pos->ev.cmd == EV_NULL) {
		return 0;
	}
	st = statelist_update(&sp->statelist, &sp->pos->ev);
	if (st->flags & STATE_NEW) {
		st->pos = sp->pos;
		st->tic = sp->tic;
	}
	sp->pos = sp->pos->next;
	sp->delta = 0;
	return st;
}

/*
 * delete the next event from the track. If the 'slist' argument is
 * not NULL, then the state is updated as it was read with
 * seqptr_evget
 */
struct state *
seqptr_evdel(struct seqptr *sp, struct statelist *slist)
{
	struct state *st;
	struct seqev *next;

	if (sp->delta != sp->pos->delta || sp->pos->ev.cmd == EV_NULL) {
		return NULL;
	}
	if (slist)
		st = statelist_update(slist, &sp->pos->ev);
	else
		st = NULL;
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
 * insert an event and put the cursor just after it, the state list is
 * updated and the state of the new event is returned.
 */
struct state *
seqptr_evput(struct seqptr *sp, struct ev *ev)
{
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
seqptr_ticskip(struct seqptr *sp, unsigned max)
{
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
 * remove blank space at the current position, same semantics as
 * seqptr_ticskip()
 */
unsigned
seqptr_ticdel(struct seqptr *sp, unsigned max, struct statelist *slist)
{
	unsigned ntics;

	ntics = sp->pos->delta - sp->delta;
	if (ntics > max) {
		ntics = max;
	}
	sp->pos->delta -= ntics;
	if (slist != NULL && max > 0) {
		statelist_outdate(slist);
	}
	return ntics;
}

/*
 * insert blank space at the current position
 */
void
seqptr_ticput(struct seqptr *sp, unsigned ntics)
{
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
seqptr_skip(struct seqptr *sp, unsigned ntics)
{
	unsigned delta;
	while (ntics > 0) {
		while (seqptr_evget(sp))
			; /* nothing */
		delta = seqptr_ticskip(sp, ntics);
		/*
		 * check if the end of the track was reached
		 */
		if (delta == 0)
			break;
		ntics -= delta;
	}
	return ntics;
}

/*
 * move forward 'ntics', if the end-of-track is reached then fill with
 * blank space. Used for writing on a track
 */
void
seqptr_seek(struct seqptr *sp, unsigned ntics)
{
	ntics = seqptr_skip(sp, ntics);
	if (ntics > 0)
		seqptr_ticput(sp, ntics);
}


/*
 * move the next frame of the current tick to the given track. Must
 * not be called on a non-frame-starting event.
 */
void
seqptr_framerm(struct seqptr *sp, struct track *f)
{
	struct seqev **save_pos, *se, *spos, *fpos;
	unsigned save_delta, sdelta, phase;

	if (sp->delta != sp->pos->delta) {
		log_puts("seqptr_framerm: not called at event position\n");
		panic();
	}
	if (sp->pos->ev.cmd == EV_NULL) {
		log_puts("seqptr_framerm: no event to remove\n");
		panic();
	}

	track_clear(f);
	fpos = f->first;

	/*
	 * Save current postition.
	 */
	save_pos = sp->pos->prev;
	save_delta = sp->delta;

	/*
	 * Move all events that belong to the frame.
	 */

	spos = sp->pos;
	sdelta = sp->delta;

	phase = ev_phase(&spos->ev);

	/* move event to frame track */
	se = spos;
	spos = se->next;
	seqev_rm(se);
	seqev_ins(fpos, se);

	for (;;) {
		if (phase & EV_PHASE_LAST)
			break;
		if (spos->ev.cmd == EV_NULL) {
			/*
			 * XXX: this can't happen, call panic() here ?
			 */
			log_puts("seqptr_framerm: unterminated frame\n");
			track_clear(f);
			return;
		}

		/* move to next event */
		fpos->delta += spos->delta - sdelta;
		sdelta = spos->delta;

		/* process next event */
		if (ev_match(&f->first->ev, &spos->ev)) {
			phase = ev_phase(&spos->ev);

			/* move event to frame track */
			se = spos;
			spos = se->next;
			seqev_rm(se);
			seqev_ins(fpos, se);
		} else {
			/* skip event */
			spos = spos->next;
			sdelta = 0;
		}
	}

	/*
	 * restore position.
	 */
	sp->pos = *save_pos;
	sp->delta = save_delta;
}

/*
 * insert the given frame at the end of the current tick. As the first
 * event of the frame is not seen yet, the state list is not
 * affected. As this is a stateless operation, it's the caller
 * responsability to avoid conflicts.
 */
void
seqptr_frameadd(struct seqptr *sp, struct track *f)
{
	struct seqev *se, *spos, **save_pos;
	unsigned ntics, offs, sdelta, save_delta;

	/*
	 * Save current postition.
	 */
	save_pos = sp->pos->prev;
	save_delta = sp->delta;

	spos = sp->pos;
	sdelta = sp->delta;
	for (;;) {
		if (f->first->ev.cmd == EV_NULL)
			break;
		se = f->first;
		offs = se->delta;
		se->delta = 0;
		seqev_rm(se);

		/*
		 * move forward offs ticks, possibly inserting
		 * empty space
		 */
		while (1) {
			ntics = spos->delta - sdelta;
			if (ntics > offs)
				ntics = offs;
			sdelta += ntics;
			offs -= ntics;
			if (offs == 0)
				break;

			/* if reached the end, append space */
			if (spos->ev.cmd == EV_NULL) {
				spos->delta += offs;
				sdelta += offs;
				offs = 0;
				break;
			}

			spos = spos->next;
			sdelta = 0;
		}

		/* make the event the last of the tick */
		while (sdelta == spos->delta && spos->ev.cmd != EV_NULL) {
			sdelta = 0;
			spos = spos->next;
		}

		se->delta = sdelta;
		spos->delta -= sdelta;
		sdelta = 0;
		/* link to the list */
		se->next = spos;
		se->prev = spos->prev;
		*se->prev = se;
		spos->prev = &se->next;
	}

	/*
	 * Restore current position.
	 */
	sp->pos = *save_pos;
	sp->delta = save_delta;
}

/*
 * generate an event that will suspend the frame of the given state;
 * the state is unchanged and may belong to any statelist.  Return 1
 * if an event was generated.
 */
unsigned
seqptr_cancel(struct seqptr *sp, struct state *st)
{
	struct ev ev;

	if (!EV_ISNOTE(&st->ev) && !(st->phase & EV_PHASE_LAST)) {
		if (state_cancel(st, &ev)) {
			seqptr_evput(sp, &ev);
		}
		return 1;
	}
	return 0;
}

/*
 * generate an event that will restore the frame of the given
 * state; the state is unchanged and may belong to any statelist.
 * Return 1 if an event was generated.
 */
unsigned
seqptr_restore(struct seqptr *sp, struct state *st)
{
	struct ev ev;

	if (!EV_ISNOTE(&st->ev) && !(st->phase & EV_PHASE_LAST)) {
		if (state_restore(st, &ev)) {
			seqptr_evput(sp, &ev);
		}
		return 1;
	}
	return 0;
}


/*
 * erase the event contained in the given state. Everything happens as
 * the event never existed on the track. Returns the new state, or
 * NULL if there is no more state.
 */
struct state *
seqptr_rmlast(struct seqptr *sp, struct state *st)
{
	struct seqev *i, *prev, *cur, *next;

#ifdef FRAME_DEBUG
	log_puts("seqptr_rmlast: ");
	ev_log(&st->ev);
	log_puts(" removing last event\n");
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
		if (state_match(st, &i->ev)) {
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
seqptr_rmprev(struct seqptr *sp, struct state *st)
{
	struct seqev *i, *next;

#ifdef FRAME_DEBUG
	log_puts("seqptr_rmprev: ");
	ev_log(&st->ev);
	log_puts(" removing whole frame\n");
#endif
	/*
	 * start a the first event of the frame and iterate until the
	 * current postion removing all events of the frame.
	 */
	i = st->pos;
	for (;;) {
		if (state_match(st, &i->ev)) {
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
 * merge low priority event: if s1 doen't conflict with high priority
 * events in the track, then store it in the track. Else drop it. This
 * routine must be called before seqptr_evmerge2() within the current
 * tick.
 */
struct state *
seqptr_evmerge1(struct seqptr *pd, struct state *s1)
{
	struct state *sd;

	/*
	 * ignore bogus events
	 */
	if (s1->flags & (STATE_BOGUS | STATE_NESTED))
		return NULL;

	sd = statelist_lookup(&pd->statelist, &s1->ev);

	if (sd != NULL) {
		if (sd->tag == 0 && !(sd->phase & EV_PHASE_LAST))
			return NULL;
	} else if (!(s1->phase & EV_PHASE_FIRST))
		return NULL;

	if (sd == NULL || !state_eq(sd, &s1->ev))
		sd = seqptr_evput(pd, &s1->ev);
	sd->tag = 1;

	return sd;
}

/*
 * merge high priority event: if s2 conflicts with low priority events
 * in the track, discard them and store s2. This routine must be
 * called once seqptr_evmerge1() was called for all low prio events
 * within the current tick.
 */
struct state *
seqptr_evmerge2(struct seqptr *pd, struct state *s1, struct state *s2)
{
	struct state *sd;

	/*
	 * ignore bogus events
	 */
	if (s2->flags & (STATE_BOGUS | STATE_NESTED))
		return NULL;
	if (s1 != NULL && s1->flags & (STATE_BOGUS | STATE_NESTED))
		s1 = NULL;

	sd = statelist_lookup(&pd->statelist, &s2->ev);

	if (sd != NULL) {
		if (sd->tag == 0) {
			if (s2->phase == EV_PHASE_LAST &&
			    s1 != NULL && s1->phase != EV_PHASE_LAST &&
			    !EV_ISNOTE(&sd->ev)) {
				if (!state_eq(s2, &s1->ev))
					sd = seqptr_evput(pd, &s1->ev);
				sd->tag = 1;
				return sd;
			}
		} else {
			if (s2->phase & EV_PHASE_FIRST) {
				if (EV_ISNOTE(&sd->ev)) {
					if (sd->phase != EV_PHASE_LAST)
						sd = seqptr_rmprev(pd, sd);
				} else if (sd->flags & STATE_CHANGED) {
					sd = seqptr_rmlast(pd, sd);
				}
			}
		}
	} else if (!(s2->phase & EV_PHASE_FIRST))
		return NULL;

	if (sd == NULL || !state_eq(sd, &s2->ev))
		sd = seqptr_evput(pd, &s2->ev);
	sd->tag = 0;

	return sd;
}

/*
 * Merge track "src" (high priority) in track "dst" (low prority)
 * resolving all conflicts, so that "dst" is consistent.
 */
void
track_merge(struct track *dst, struct track *src)
{
	struct state *s1, *s2;
	struct seqptr *p2, *pd;
	struct statelist orglist;
	unsigned delta1, delta2, deltad;

	pd = seqptr_new(dst);
	p2 = seqptr_new(src);
	statelist_init(&orglist);

	for (;;) {
		/*
		 * remove all events from 'dst' and put them back on
		 * on 'dst' by merging them with the state table of
		 * 'src'. The 'orglist' state table is updated so it
		 * always contain the exact state of the original
		 * 'dst' track.
		 */
		for(;;) {
			s1 = seqptr_evdel(pd, &orglist);
			if (s1 == NULL)
				break;
			seqptr_evmerge1(pd, s1);
		}

		/*
		 * move all events from 'src' to 'dst' by merging them
		 * with the original state of 'dst'.
		 */
		for (;;) {
			s2 = seqptr_evget(p2);
			if (s2 == NULL)
				break;
			s1 = statelist_lookup(&orglist, &s2->ev);
			seqptr_evmerge2(pd, s1, s2);
		}

		/*
		 * move to the next non empty tick: the next tic is the
		 * smaller position of the next event of each track
		 */
		delta1 = pd->pos->delta - pd->delta;
		delta2 = p2->pos->delta - p2->delta;
		if (delta1 > 0) {
			deltad = delta1;
			if (delta2 > 0 && delta2 < deltad)
				deltad = delta2;
		} else if (delta2 > 0) {
			deltad = delta2;
		} else {
			/* both delta1 and delta2 are zero */
			break;
		}
		(void)seqptr_ticskip(p2, deltad);
		(void)seqptr_ticdel(pd, deltad, &orglist);
		seqptr_ticput(pd, deltad);
	}

	statelist_done(&orglist);
	seqptr_del(p2);
	seqptr_del(pd);
	track_chomp(dst);
}

/*
 * move/copy/blank a portion of the given track. All operations are
 * consistent: notes are always completely copied/moved/erased and
 * controllers (and other) are cut when necessary
 *
 * if the 'copy' flag is set, then the selection is copied in
 * track 'dst'. If 'blank' flag is set, then the selection is
 * cleanly removed from the 'src' track
 */
void
track_move(struct track *src, unsigned start, unsigned len,
    struct evspec *es, struct track *dst, unsigned copy, unsigned blank) {
	unsigned delta;
	struct seqptr *sp, *dp;		/* current src & dst track states */
	struct statelist slist;		/* original src track state */
	struct state *st;

#define TAG_KEEP	1		/* frame is not erased */
#define TAG_COPY	2		/* frame is copied */

	/* please gcc */
	sp = dp = NULL;

	if (len == 0)
		return;
	if (copy) {
		track_clear(dst);
		dp = seqptr_new(dst);
	}
	sp = seqptr_new(src);

	/*
	 * go to the start position and tag all frames as
	 * not being copied and not being erased
	 */
	(void)seqptr_skip(sp, start);
	statelist_dup(&slist, &sp->statelist);
	for (st = slist.first; st != NULL; st = st->next) {
       		st->tag = TAG_KEEP;
	}

	/*
	 * cancel/tag frames that will be erased (blank only)
	 */
	if (blank) {
		for (st = slist.first; st != NULL; st = st->next) {
			if (!EV_ISNOTE(&st->ev) && state_inspec(st, es) &&
			    seqptr_cancel(sp, st))
				st->tag &= ~TAG_KEEP;
		}
	}

	/*
	 * copy the first tic: tag/copy/erase new frames. This is the
	 * last chance for already tagged frames to terminate and to
	 * avoid being restored in the copy
	 *
	 */
	for (;;) {
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if ((st->phase & EV_PHASE_FIRST) ||
		    (st->phase & EV_PHASE_NEXT && !EV_ISNOTE(&st->ev))) {
			st->tag &= ~TAG_COPY;
			if (state_inspec(st, es))
				st->tag |= TAG_COPY;
		}
		if (st->phase & EV_PHASE_FIRST) {
			st->tag &= ~TAG_KEEP;
		}
		if (copy && (st->tag & TAG_COPY))
			seqptr_evput(dp, &st->ev);
		if (!blank || (st->tag & TAG_KEEP)) {
			seqptr_evput(sp, &st->ev);
		}
	}

	/*
	 * in the copy, restore frames that weren't updated by the
	 * first tic.
	 */
	if (copy) {
		for (st = slist.first; st != NULL; st = st->next) {
			if (EV_ISNOTE(&st->ev) || !state_inspec(st, es))
				continue;
			if (!(st->tag & TAG_COPY) && seqptr_restore(dp, st)) {
				st->tag |= TAG_COPY;
			}
		}
	}

	/*
	 * tag/copy/erase frames during 'len' tics
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, len, &slist);
		if (copy)
			seqptr_ticput(dp, delta);
		seqptr_ticput(sp, delta);
		len -= delta;
		if (len == 0)
			break;
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST) {
			st->tag = state_inspec(st, es) ? TAG_COPY : TAG_KEEP;
		}
		if (copy && (st->tag & TAG_COPY)) {
			seqptr_evput(dp, &st->ev);
		}
		if (!blank || (st->tag & TAG_KEEP)) {
			seqptr_evput(sp, &st->ev);
		}
	}

	/*
	 * cancel all copied frames (that are tagged).
	 * cancelled frames are untagged, so they will stop
	 * being copied
	 */
	if (copy) {
		for (st = slist.first; st != NULL; st = st->next) {
			if (seqptr_cancel(dp, st))
				st->tag &= ~TAG_COPY;
		}
	}

	/*
	 * move the first tic of the 'end' boundary. New frames are
	 * tagged as "not to erase". This is the last chance for
	 * untagged frames (those being erased) to terminate and to
	 * avoid being restored
	 */
	for (;;) {
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if ((st->phase & EV_PHASE_FIRST) ||
		    (st->phase & EV_PHASE_NEXT && !EV_ISNOTE(&st->ev))) {
			st->tag |= TAG_KEEP;
		}
		if (st->phase & EV_PHASE_FIRST)
			st->tag &= ~TAG_COPY;
		if (copy && (st->tag & TAG_COPY)) {
			seqptr_evput(dp, &st->ev);
		}
		if (!blank || (st->tag & TAG_KEEP)) {
			seqptr_evput(sp, &st->ev);
		}

	}

	/*
	 * retore/tag frames that are not tagged.
	 */
	for (st = slist.first; st != NULL; st = st->next) {
		if (!(st->tag & TAG_KEEP) && seqptr_restore(sp, st)) {
			st->tag |= TAG_KEEP;
		}
	}

	/*
	 * copy frames for whose state couldn't be
	 * canceled (note events)
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist);
		if (copy)
			seqptr_ticput(dp, delta);
		seqptr_ticput(sp, delta);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST) {
			st->tag &= ~TAG_COPY;
			st->tag |= TAG_KEEP;
		}
		if (copy && (st->tag & TAG_COPY)) {
			seqptr_evput(dp, &st->ev);
		}
		if (!blank || (st->tag & TAG_KEEP)) {
			seqptr_evput(sp, &st->ev);
		}
	}

	statelist_done(&slist);
	seqptr_del(sp);
	if (copy) {
		seqptr_del(dp);
		track_chomp(dst);
	}
	if (blank)
		track_chomp(src);
#undef TAG_BLANK
#undef TAG_COPY
}

/*
 * quantize the given track
 */
void
track_quantize(struct track *src, struct evspec *es,
    unsigned start, unsigned len,
    unsigned offset, unsigned quant, unsigned rate) {
	unsigned tic, qtic;
	struct track qt;
	struct seqptr *sp, *qp;
	struct state *st;
	struct statelist slist;
	unsigned remaind;
	unsigned fluct, notes;
	int ofs, delta;

	track_init(&qt);
	sp = seqptr_new(src);
	qp = seqptr_new(&qt);

	/*
	 * go to start position and untag all events
	 * (tagged = will be quantized)
	 */
	(void)seqptr_skip(sp, start);
	statelist_dup(&slist, &sp->statelist);
	for (st = slist.first; st != NULL; st = st->next) {
		st->tag = 0;
	}
	seqptr_seek(qp, start);
	tic = qtic = start;
	ofs = 0;

	/*
	 * go ahead and copy all events to quantize during 'len' tics,
	 * while stretching the time scale in the destination track
	 */
	fluct = 0;
	notes = 0;
	for (;;) {
		delta = seqptr_ticdel(sp, start + len - tic, &slist);
		seqptr_ticput(sp, delta);
		tic += delta;
		if (tic >= start + len)
			break;
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;

		remaind = quant != 0 ? (tic - start + offset) % quant : 0;
		if (remaind < quant / 2) {
			ofs = - ((remaind * rate + 99) / 100);
		} else {
			ofs = ((quant - remaind) * rate + 99) / 100;
		}

		delta = tic + ofs - qtic;
#ifdef FRAME_DEBUG
		if (delta < 0) {
			log_puts("track_quantize: delta < 0\n");
			panic();
		}
#endif
		seqptr_ticput(qp, delta);
		qtic += delta;

		if (st->phase & EV_PHASE_FIRST) {
			if (evspec_matchev(es, &st->ev)) {
				st->tag = 1;
				if (EV_ISNOTE(&st->ev)) {
					fluct += (ofs < 0) ? -ofs : ofs;
					notes++;
				}
			} else
				st->tag = 0;
		}
		if (st->tag) {
			seqptr_evput(qp, &st->ev);
		} else {
			seqptr_evput(sp, &st->ev);
		}
	}

	/*
	 * finish quantised (tagged) events
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist);
		seqptr_ticput(sp, delta);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST)
			st->tag = 0;
		seqptr_ticput(qp, delta);
		if (st->tag) {
			seqptr_evput(qp, &st->ev);
		} else {
			seqptr_evput(sp, &st->ev);
		}
	}
	track_merge(src, &qt);
	statelist_done(&slist);
	seqptr_del(sp);
	seqptr_del(qp);
	track_done(&qt);
	if (notes > 0) {
		log_puts("quantize: ");
		log_putu(notes);
		log_puts(" notes, fluctuation = ");
		log_putu(100 * fluct / notes);
		log_puts("% of a tick\n");
	}
}

/*
 * quantize the given track by moving frames
 */
void
track_quantize_frame(struct track *src, struct evspec *es,
    unsigned start, unsigned len,
    unsigned offset, unsigned quant, unsigned rate) {
	unsigned tic, qtic;
	struct track qt, frame;
	struct seqptr *sp, *qp;
	struct state *st;
	unsigned remaind;
	unsigned fluct, notes;
	int ofs, delta;

	sp = seqptr_new(src);

	/*
	 * go to start position
	 */
	if (seqptr_skip(sp, start) > 0) {
		seqptr_del(sp);
		return;
	}

	track_init(&qt);
	qp = seqptr_new(&qt);
	seqptr_seek(qp, start);

	track_init(&frame);
	tic = qtic = start;
	ofs = 0;

	/*
	 * go ahead and copy all events to quantize during 'len' tics,
	 * while stretching the time scale in the destination track
	 */
	fluct = 0;
	notes = 0;
	for (;;) {
		delta = seqptr_ticskip(sp, ~0U);
		tic += delta;
		if (tic >= start + len)
			break;

		remaind = quant != 0 ? (tic - start + offset) % quant : 0;
		if (remaind < quant / 2) {
			ofs = - ((remaind * rate + 99) / 100);
		} else {
			ofs = ((quant - remaind) * rate + 99) / 100;
		}

		delta = tic + ofs - qtic;
#ifdef FRAME_DEBUG
		if (delta < 0) {
			log_puts("track_quantize_frame: delta < 0\n");
			panic();
		}
#endif
		seqptr_seek(qp, delta);
		qtic += delta;

		if (seqptr_eot(sp))
			break;

		st = statelist_lookup(&sp->statelist, &sp->pos->ev);
		if (st != NULL && !(st->phase & EV_PHASE_LAST)) {
			/*
			 * There's a state for this event in the
			 * source track, which means the frame
			 * is not being quantized
			 */
#ifdef FRAME_DEBUG
			if (sp->pos->ev.cmd != EV_NULL) {
				ev_log(&sp->pos->ev);
				log_puts(": skipped (not ours)\n");
			}
#endif
			seqptr_evget(sp);
			continue;
		}

		st = statelist_lookup(&qp->statelist, &sp->pos->ev);
		if (st != NULL && !(st->phase & EV_PHASE_LAST)) {
			/*
			 * There's as state for this event, which is
			 * part of a conflicting frame. Just skip it.
			 */
#ifdef FRAME_DEBUG
			if (sp->pos->ev.cmd != EV_NULL) {
				ev_log(&sp->pos->ev);
				log_puts(": skipped (conflict)\n");
			}
#endif
			seqptr_evget(sp);
			continue;
		}

		if (!evspec_matchev(es, &sp->pos->ev)) {
			/*
			 * Doesn't match selection, Skip this event.
			 */
			seqptr_evget(sp);
			continue;
		}

		seqptr_framerm(sp, &frame);
		if (EV_ISNOTE(&frame.first->ev)) {
			fluct += (ofs < 0) ? -ofs : ofs;
			notes++;
		}
		seqptr_frameadd(qp, &frame);
		while (seqptr_evget(qp))
			;
	}

	statelist_empty(&sp->statelist);
	statelist_empty(&qp->statelist);
	seqptr_del(sp);
	seqptr_del(qp);
	track_done(&frame);

	track_merge(src, &qt);
	track_done(&qt);

	if (notes > 0) {
		log_puts("quantize: ");
		log_putu(notes);
		log_puts(" notes, fluctuation = ");
		log_putu(100 * fluct / notes);
		log_puts("% of a tick\n");
	}
}

/*
 * First step to time-scale of the given track: round event positions
 * and convert tempo/signature events without scaling the track.
 */
void
track_prescale(struct track *t, unsigned oldunit, unsigned newunit)
{
	struct seqptr *sp;
	struct statelist slist;
	struct state *st;
	struct ev ev;
	unsigned delta, round, err;

	round = oldunit / newunit;
	if (round == 0)
		round = 1;

	err = 0;
	sp = seqptr_new(t);
	statelist_init(&slist);
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist) + err;
		err = delta % round;
		delta -= err;
		seqptr_ticput(sp, delta);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL) {
			break;
		}
		switch (st->ev.cmd) {
		case EV_TEMPO:
			ev.cmd = st->ev.cmd;
			ev.tempo_usec24 = st->ev.tempo_usec24 * oldunit / newunit;
			seqptr_evput(sp, &ev);
			break;
		case EV_TIMESIG:
			ev.cmd = st->ev.cmd;
			ev.timesig_beats = st->ev.timesig_beats;
			ev.timesig_tics = st->ev.timesig_tics * newunit / oldunit;
			seqptr_evput(sp, &ev);
			break;
		default:
			seqptr_evput(sp, &st->ev);
			break;
		}
	}
	statelist_done(&slist);
	seqptr_del(sp);
}

/*
 * Finalize time-scaling the given track in such a way that 'oldunit'
 * ticks will correspond to 'newunit'.
 */
void
track_scale(struct track *t, unsigned oldunit, unsigned newunit)
{
	struct seqptr *sp;
	struct statelist slist;
	struct state *st;
	unsigned delta;

	sp = seqptr_new(t);
	statelist_init(&slist);
	for (;;) {
		delta = newunit * seqptr_ticdel(sp, ~0U, &slist);
		seqptr_ticput(sp, delta / oldunit);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		seqptr_evput(sp, &st->ev);
	}
	statelist_done(&slist);
	seqptr_del(sp);
}

/*
 * transpose the given track
 */
void
track_transpose(struct track *src, unsigned start, unsigned len,
    struct evspec *es, int halftones)
{
	unsigned delta, tic;
	struct track qt;
	struct seqptr *sp, *qp;
	struct state *st;
	struct statelist slist;
	struct ev ev;

	track_init(&qt);
	sp = seqptr_new(src);
	qp = seqptr_new(&qt);

	/*
	 * go to t start position and untag all frames
	 * (tagged = will be transposed)
	 */
	(void)seqptr_skip(sp, start);
	statelist_dup(&slist, &sp->statelist);
	for (st = slist.first; st != NULL; st = st->next) {
		st->tag = 0;
	}
	seqptr_seek(qp, start);
	tic = start;

	/*
	 * go ahead and copy all events to transpose during 'len' tics,
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, len, &slist);
		seqptr_ticput(sp, delta);
		seqptr_ticput(qp, delta);
		tic += delta;
		if (tic >= start + len)
			break;
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST) {
			if (EV_ISNOTE(&st->ev) && state_inspec(st, es))
				st->tag = 1;
			else
				st->tag = 0;
		}
		if (st->tag) {
			ev = st->ev;
			ev.note_num += (128 + halftones);
			ev.note_num &= 0x7f;
			seqptr_evput(qp, &ev);
		} else {
			seqptr_evput(sp, &st->ev);
		}
	}

	/*
	 * finish transposed (tagged) frames
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist);
		seqptr_ticput(sp, delta);
		seqptr_ticput(qp, delta);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST)
			st->tag = 0;
		if (st->tag) {
			ev = st->ev;
			ev.note_num += (128 + halftones);
			ev.note_num &= 0x7f;
			seqptr_evput(qp, &ev);
		} else {
			seqptr_evput(sp, &st->ev);
		}
	}
	track_merge(src, &qt);
	statelist_done(&slist);
	seqptr_del(sp);
	seqptr_del(qp);
	track_done(&qt);
}

/*
 * apply velocity curve to given track
 */
void
track_vcurve(struct track *src, unsigned start, unsigned len,
    struct evspec *es, int weight)
{
	unsigned delta, tic;
	struct seqptr *sp;
	struct state *st;
	struct statelist slist;
	struct ev ev;

	/* put weight from -63:63 to 1:127 range */
	weight = (64 - weight) & 0x7f;

	sp = seqptr_new(src);
	statelist_dup(&slist, &sp->statelist);
	tic = 0;

	/*
	 * rewrite all events, modifying selected ones
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist);
		seqptr_ticput(sp, delta);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		tic += delta;
		if ((st->phase & EV_PHASE_FIRST) &&
		    tic >= start && tic < start + len &&
		    EV_ISNOTE(&st->ev) && state_inspec(st, es)) {
			ev = st->ev;
			ev.note_vel = vcurve(weight, ev.note_vel);
			seqptr_evput(sp, &ev);
		} else {
			seqptr_evput(sp, &st->ev);
		}
	}

	statelist_done(&slist);
	seqptr_del(sp);
}

/*
 * rewrite the track frame-by-frame
 */
void
track_rewrite(struct track *src)
{
	struct statelist slist;
	struct track dst, frame;
	struct seqptr *sp, *dp;
	unsigned delta;

	track_check(src);

	track_init(&frame);

	sp = seqptr_new(src);
	statelist_init(&slist);

	track_init(&dst);
	dp = seqptr_new(&dst);

	/*
	 * reconstruct frame-by-frame
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist);
		seqptr_seek(dp, delta);

		if (seqptr_eot(sp))
			break;

		seqptr_framerm(sp, &frame);
		seqptr_frameadd(dp, &frame);
	}

	statelist_empty(&dp->statelist);
	seqptr_del(dp);
	statelist_done(&slist);
	seqptr_del(sp);

	track_swap(src, &dst);
	track_done(&dst);
}

/*
 * check (and fix) the given track for inconsistencies
 */
void
track_check(struct track *src)
{
	struct seqptr *sp;
	struct state *dst, *st, *stnext;
	struct statelist slist;
	unsigned delta;

	sp = seqptr_new(src);
	statelist_init(&slist);

	/*
	 * reconstruct the track skipping bogus events,
	 * see statelist_update() for definition of bogus
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist);
		seqptr_ticput(sp, delta);

		st = seqptr_evdel(sp, &slist);
		if (st == NULL) {
			break;
		}
		if (st->phase & EV_PHASE_FIRST) {
			if (st->flags & STATE_BOGUS) {
				log_puts("track_check: ");
				ev_log(&st->ev);
				log_puts(": bogus\n");
				st->tag = 0;
			} else if (st->flags & STATE_NESTED) {
				log_puts("track_check: ");
				ev_log(&st->ev);
				log_puts(": nested\n");
				st->tag = 0;
			} else {
				st->tag = 1;
			}
		}
		if (st->tag) {
			/*
			 * dont duplicate events
			 */
			dst = statelist_lookup(&sp->statelist, &st->ev);
			if (dst == NULL || !state_eq(dst, &st->ev)) {
				seqptr_evput(sp, &st->ev);
			} else {
				log_puts("track_check: ");
				ev_log(&st->ev);
				log_puts(": duplicated\n");
			}
		}
	}

	/*
	 * undo (erase) all unterminated frames
	 */
	for (st = sp->statelist.first; st != NULL; st = stnext) {
		stnext = st->next;
		if (!(st->phase & EV_PHASE_LAST)) {
			log_puts("track_check: ");
			ev_log(&st->ev);
			log_puts(": unterminated\n");
			(void)seqptr_rmprev(sp, st);
		}
	}

	/*
	 * statelist_done() will complain about bogus frames.  Since
	 * bugs are fixed in the track, we empty slist to avoid
	 * warning messages
	 */
	statelist_empty(&slist);
	statelist_done(&slist);
	seqptr_del(sp);
}

/*
 * get the current tempo (at the current position)
 */
struct state *
seqptr_getsign(struct seqptr *sp, unsigned *bpm, unsigned *tpb)
{
	struct ev ev;
	struct state *st;

	ev.cmd = EV_TIMESIG;
	st = statelist_lookup(&sp->statelist, &ev);
	if (bpm)
		*bpm = (st == NULL) ? DEFAULT_BPM : st->ev.timesig_beats;
	if (tpb)
		*tpb = (st == NULL) ? DEFAULT_TPB : st->ev.timesig_tics;
	return st;
}

/*
 * get the current tempo (at the current position)
 */
struct state *
seqptr_gettempo(struct seqptr *sp, unsigned long *usec24)
{
	struct ev ev;
	struct state *st;

	ev.cmd = EV_TEMPO;
	st = statelist_lookup(&sp->statelist, &ev);
	if (usec24)
		*usec24 = (st == NULL) ? DEFAULT_USEC24 : st->ev.tempo_usec24;
	return st;
}

/*
 * try to move 'm0' measures forward; the current postition MUST be
 * the beginning of a measure and the state table must be up to date.
 * Return the number of tics remaining until the requested measure
 * (only on premature end-of-track)
 */
unsigned
seqptr_skipmeasure(struct seqptr *sp, unsigned meas)
{
	unsigned m, bpm, tpb, tics_per_meas, delta;

	for (m = 0; m < meas; m++) {
		while (seqptr_evget(sp)) {
			/* nothing */
		}
		seqptr_getsign(sp, &bpm, &tpb);
		tics_per_meas = bpm * tpb;
		delta = seqptr_skip(sp, tics_per_meas);
		if (delta > 0)
			return (meas - m - 1) * tics_per_meas + delta;
	}
	return 0;
}

/*
 * convert a measure number to a tic number using
 * meta-events from the given track
 */
unsigned
track_findmeasure(struct track *t, unsigned m)
{
	struct seqptr *sp;
	unsigned tic;

	sp = seqptr_new(t);
	tic  = seqptr_skipmeasure(sp, m);
	tic += sp->tic;
	seqptr_del(sp);

#ifdef FRAME_DEBUG
	log_puts("track_findmeasure: ");
	log_putu(m);
	log_puts(" -> ");
	log_putu(tic);
	log_puts("\n");
#endif

	return tic;
}

/*
 * return the absolute tic, the tempo and the time signature
 * corresponding to the given measure number
 */
void
track_timeinfo(struct track *t, unsigned meas, unsigned *abs,
    unsigned long *usec24, unsigned *bpm, unsigned *tpb)
{
	struct seqptr *sp;
	unsigned tic;

	sp = seqptr_new(t);
	tic  = seqptr_skipmeasure(sp, meas);
	tic += sp->tic;

	/*
	 * move to the last event, so all meta events enter the
	 * state list
	 */
	while (seqptr_evget(sp)) {
		/* nothing */
	}
	if (abs) {
		*abs = tic;
	}
	seqptr_getsign(sp, bpm, tpb);
	seqptr_gettempo(sp, usec24);
	seqptr_del(sp);
}

/*
 * go to the given measure and set the tempo
 */
void
track_settempo(struct track *t, unsigned measure, unsigned tempo)
{
	struct seqptr *sp;
	struct state *st;
	struct statelist slist;
	struct ev ev;
	unsigned long usec24, old_usec24;
	unsigned tic, bpm, tpb;
	unsigned delta;

	/*
	 * go to the requested position, insert blank if necessary
	 */
	sp = seqptr_new(t);
	tic = seqptr_skipmeasure(sp, measure);
	if (tic) {
		seqptr_ticput(sp, tic);
	}
	statelist_dup(&slist, &sp->statelist);

	/*
	 * remove tempo events at the current tic
	 */
	for (;;) {
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->ev.cmd != EV_TEMPO)
			seqptr_evput(sp, &st->ev);
	}

	/*
	 * if needed, insert a new tempo event
	 */
	seqptr_getsign(sp, &bpm, &tpb);
	usec24 = TEMPO_TO_USEC24(tempo, tpb);
	seqptr_gettempo(sp, &old_usec24);
	if (usec24 != old_usec24) {
		ev.cmd = EV_TEMPO;
		ev.tempo_usec24 = usec24;
		seqptr_evput(sp, &ev);
	}

	/*
	 * move next events, skipping duplicate tempos
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist);
		seqptr_ticput(sp, delta);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->ev.cmd != EV_TEMPO ||
		    st->ev.tempo_usec24 != usec24) {
			usec24 = st->ev.tempo_usec24;
			seqptr_evput(sp, &st->ev);
		}
	}
	seqptr_del(sp);
	statelist_done(&slist);
}

/*
 * add an event to the first given "config" track, if there is such an
 * event, then replace it.
 */
void
track_confev(struct track *src, struct ev *ev)
{
	struct seqptr *sp;
	struct statelist slist;
	struct state *st;

	if (ev_phase(ev) != (EV_PHASE_FIRST | EV_PHASE_LAST)) {
		log_puts("track_confev: ");
		ev_log(ev);
		log_puts(": bad phase, ignored");
		log_puts("\n");
		return;
	}

	sp = seqptr_new(src);
	statelist_init(&slist);

	/*
	 * rewrite the track, removing frames matching the event
	 */
	for (;;) {
		(void)seqptr_ticdel(sp, ~0U, &slist);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST) {
			st->tag = state_match(st, ev) ? 0 : 1;
		}
		if (st->tag) {
			seqptr_evput(sp, &st->ev);
		}

	}
	seqptr_evput(sp, ev);
	statelist_done(&slist);
	seqptr_del(sp);
}

/*
 * remove a set of events from the "config" track
 */
void
track_unconfev(struct track *src, struct evspec *es)
{
	struct statelist slist;
	struct seqptr *sp;
	struct state *st;

	sp = seqptr_new(src);
	statelist_init(&slist);

	/*
	 * rewrite the track, removing frames matching the spec
	 */
	for (;;) {
		(void)seqptr_ticdel(sp, ~0U, &slist);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST) {
			st->tag = state_inspec(st, es) ? 0 : 1;
		}
		if (st->tag) {
			seqptr_evput(sp, &st->ev);
		}

	}
	statelist_done(&slist);
	seqptr_del(sp);
}

/*
 * insert the given amount of blank space at the given position
 */
void
track_ins(struct track *t, unsigned stic, unsigned len)
{
	struct track t1, t2;

	track_init(&t1);
	track_init(&t2);
	track_move(t, 0 ,  stic, NULL, &t1, 1, 1);
	track_move(t, stic, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, stic + len);
	track_clear(t);
	track_merge(t, &t1);
	if (!track_isempty(&t2)) {
		track_merge(t, &t2);
	}
	track_done(&t1);
	track_done(&t2);
}

/*
 * cut the given portion of the track
 */
void
track_cut(struct track *t, unsigned stic, unsigned len)
{
	struct track t1, t2;

	track_init(&t1);
	track_init(&t2);
	track_move(t, 0,	 stic, NULL, &t1, 1, 1);
	track_move(t, stic + len, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, stic);
	track_clear(t);
	track_merge(t, &t1);
	if (!track_isempty(&t2)) {
		track_merge(t, &t2);
	}
	track_done(&t1);
	track_done(&t2);
}

/*
 * map current selection to the given evspec
 */
void
track_evmap(struct track *src, unsigned start, unsigned len,
    struct evspec *es, struct evspec *from, struct evspec *to)
{
	unsigned delta, tic;
	struct track qt;
	struct seqptr *sp, *qp;
	struct state *st;
	struct statelist slist;
	struct ev ev;

	if (!evspec_isamap(from, to))
		return;

	track_init(&qt);
	sp = seqptr_new(src);
	qp = seqptr_new(&qt);

	/*
	 * go to t start position and untag all frames
	 * (tagged = will be mapped)
	 */
	(void)seqptr_skip(sp, start);
	statelist_dup(&slist, &sp->statelist);
	for (st = slist.first; st != NULL; st = st->next) {
		st->tag = 0;
	}
	seqptr_seek(qp, start);
	tic = start;

	/*
	 * go ahead and copy all events to map during 'len' tics,
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, len, &slist);
		seqptr_ticput(sp, delta);
		seqptr_ticput(qp, delta);
		tic += delta;
		if (tic >= start + len)
			break;
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST) {
			if (state_inspec(st, es) && state_inspec(st, from))
				st->tag = 1;
			else
				st->tag = 0;
		}
		if (st->tag) {
			ev_map(&st->ev, from, to, &ev);
			seqptr_evput(qp, &ev);
		} else {
			seqptr_evput(sp, &st->ev);
		}
	}

	/*
	 * finish mapped (tagged) frames
	 */
	for (;;) {
		delta = seqptr_ticdel(sp, ~0U, &slist);
		seqptr_ticput(sp, delta);
		seqptr_ticput(qp, delta);
		st = seqptr_evdel(sp, &slist);
		if (st == NULL)
			break;
		if (st->phase & EV_PHASE_FIRST)
			st->tag = 0;
		if (st->tag) {
			ev_map(&st->ev, from, to, &ev);
			seqptr_evput(qp, &ev);
		} else {
			seqptr_evput(sp, &st->ev);
		}
	}
	track_merge(src, &qt);
	statelist_done(&slist);
	seqptr_del(sp);
	seqptr_del(qp);
	track_done(&qt);
}
