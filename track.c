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
 * a track (struct track *o) is a singly linked list of
 * events. Each event (struct seqev) is made by 
 *	- a midi event (struct ev)
 *	- the number of tics before the event is to be played
 *
 * Since a track can contain an amount of blank space
 * after the last event (if any), there is always an end-of-track event
 * in the list.
 *
 *	- each clock tic marks the begining of a delta
 *	- each event (struct ev) is played after delta tics
 *
 * a seqptr represents the position of a cursor on the track In play
 * mode, the field 'o->pos' points to the event to be played. The
 * 'delta' field contains the number of tics elapsed since the last
 * played event. Thus when o->delta reaches o->pos->delta, the event
 * is to be played.
 *
 * In record mode, the field 'o->pos' points to the next event that
 * the event being recorded.
 *
 * The track structure contain a cursor (a pos/delta pair) which is
 * used to walk through the track. All the track_* routines maintain a
 * consistent value of the cursor.
 *
 * track_ev* routines involve the current event, When calling such a
 * routine, the current position pointer must be an event not an empty
 * tic (except for track_evavail and track_evins). They are meant to
 * be fast enough to be used in real-time operations.
 *
 * track_tic* routines involves a single tic, When calling such a
 * routine, the current position pointer must be an empty tic, not an
 * event (except for track_ticavail and track_ticins). They are meant
 * to be fast enough to be used in real-time operations.
 */

#include "dbg.h"
#include "pool.h"
#include "track.h"

struct pool seqev_pool;

void
seqev_pool_init(unsigned size) {
	pool_init(&seqev_pool, "seqev", sizeof(struct seqev), size);
}

void
seqev_pool_done(void) {
	pool_done(&seqev_pool);
}


struct seqev *
seqev_new(void) {
	return (struct seqev *)pool_new(&seqev_pool);
}

void
seqev_del(struct seqev *se) {
	pool_del(&seqev_pool, se);
}

void
seqev_dump(struct seqev *i) {
	dbg_putu(i->delta);
	dbg_puts("\t");
	ev_dbg(&i->ev);
}

/*
 * initialise the track
 */
void
track_init(struct track *o) {
	o->eot.ev.cmd = EV_NULL;
	o->eot.delta = 0;
	o->eot.next = NULL;
	o->eot.prev = &o->first;
	o->first = &o->eot;
}

/*
 * free a track
 */
void
track_done(struct track *o) {
	struct seqev *i, *inext;
	
	for (i = o->first;  i != &o->eot;  i = inext) {
		inext = i->next;
		seqev_del(i);
	}
#ifdef TRACK_DEBUG
	o->first = (void *)0xdeadbeef;
#endif
}

/*
 * dump the track on stderr, for debugging purposes
 */
void
track_dump(struct track *o) {
	struct seqev *i;
	unsigned tic = 0, num = 0;
	
	for (i = o->first; i != NULL; i = i->next) {
		tic += i->delta;
		dbg_putu(num);
		dbg_puts("\t");
		dbg_putu(tic);
		dbg_puts("\t+");
		seqev_dump(i);
		dbg_puts("\n");
		num++;
	}
}

/*
 * return the number of events in the track
 */
unsigned
track_numev(struct track *o) {
	unsigned n;
	struct seqev *i;
	
	n = 0;
	for(i = o->first; i != &o->eot; i = i->next) n++;

	return n;
}

/*
 * return the number of tics in the track
 * ie its length (eot included, of course)
 */
unsigned
track_numtic(struct track *o) {
	unsigned ntics;
	struct seqev *i;
	ntics = 0;
	for(i = o->first; i != NULL; i = i->next) 
		ntics += i->delta;
	return ntics;
}


/*
 * remove all events from the track
 * XXX: rename this track_clear and put it in track.c,
 * (but first stop using the old track_clear())
 */
void
track_clearall(struct track *o) {
	struct seqev *i, *inext;
	
	for (i = o->first;  i != &o->eot;  i = inext) {
		inext = i->next;
		seqev_del(i);
	}
	o->eot.delta = 0;
	o->eot.prev = &o->first;
	o->first = &o->eot;
}

/*
 * clear 'dst' and attach contents of 'src' to 'dst'
 * (dont forget to copy the 'eot')
 */
void
track_moveall(struct track *dst, struct track *src) {
	track_clearall(dst);
	dst->eot.delta = src->eot.delta;
	if (src->first == &src->eot) {
		dst->first = &dst->eot;
		dst->eot.prev = &dst->first;
	} else {
		dst->first = src->first;
		dst->eot.prev = src->eot.prev;
		dst->first->prev = &dst->first;
		*dst->eot.prev = &dst->eot;
	}
	src->eot.delta = 0;
	src->eot.prev = &src->first;
	src->first = &src->eot;
}

/* ---------------------------------------- track_seqev* routines --- */

/*
 * return true if an (delta, event) pair is available
 */
unsigned
track_seqevavail(struct track *o, struct seqptr *p) {
	return (p->pos != &o->eot);
}

/*
 * insert an event (stored in an already allocated
 * seqev structure). The delta field of the
 * structure will be stored as blank time
 * before the event. Thus, in normal usage 
 * se->delta will be nearly always zero.
 */
void
track_seqevins(struct track *o, struct seqptr *p, struct seqev *se) {
#ifdef TRACK_DEBUG
	if (p->delta > p->pos->delta) {
		dbg_puts("track_seqevins: sync. error\n");
		dbg_panic();
	}	
#endif
	se->delta += p->delta;
	p->pos->delta -= p->delta;
	/* link to the list */
	se->next = p->pos;
	se->prev = p->pos->prev;
	*(se->prev) = se;
	p->pos->prev = &se->next;
	/* fix current position */
	p->pos = se;
}

/*
 * remove the next event and the blank space between the current
 * position an the event. The seqev structure is not deleted and the
 * delta field is set to the removed blank space.
 */
struct seqev *
track_seqevrm(struct track *o, struct seqptr *p) {
	struct seqev *se;
#ifdef TRACK_DEBUG
	if (p->pos == &o->eot) {
		dbg_puts("track_seqevrm: unexpected end of track\n");
		dbg_panic();
	}	

	if (p->delta > p->pos->delta) {
		dbg_puts("track_seqevrm: sync. error\n");
		dbg_panic();
	}	
#endif
	se = p->pos;
	se->next->delta += p->delta;
	se->delta -= p->delta;
	/* since se != &eot, next is never NULL */
	*se->prev = se->next;
	se->next->prev = se->prev;
	/* fix current position */
	p->pos = se->next;
	return se;
}


/*
 * move to the next (delta, event) pair
 * return the tics we moved
 */
unsigned
track_seqevnext(struct track *o, struct seqptr *p) {
	unsigned tics;
#ifdef TRACK_DEBUG
	if (p->pos == &o->eot) {
		dbg_puts("track_seqevnext: unexpected end of track\n");
		dbg_panic();
	}

	if (p->delta > p->pos->delta) {
		dbg_puts("track_seqevnext: sync. error\n");
		dbg_panic();
	}
#endif
	tics = p->pos->delta - p->delta;
	p->pos = p->pos->next;
	p->delta = 0;
	return tics;
}

/* ------------------------------------------- seqev_xxx routines --- */

/*
 * return true if an event is available on the track
 */
unsigned
seqev_avail(struct seqev *pos) {
	return (pos->ev.cmd != EV_NULL);
}

/*
 * insert an event (stored in an already allocated seqev
 * structure) just before the event of the given position
 * (the delta field of the given event is ignored)
 */
void
seqev_ins(struct seqev *pos, struct seqev *se) {
	se->delta = pos->delta;
	pos->delta = 0;
	/* link to the list */
	se->next = pos;
	se->prev = pos->prev;
	*(se->prev) = se;
	pos->prev = &se->next;
}

/*
 * remove the event (but not blank space) on the given position
 */
void
seqev_rm(struct seqev *pos) {
#ifdef TRACK_DEBUG
	if (pos->ev.cmd == EV_NULL) {
		dbg_puts("seqev_rm: unexpected end of track\n");
		dbg_panic();
	}	
#endif
	pos->next->delta += pos->delta;
	pos->delta = 0;
	/* since se != &eot, next is never NULL */
	*pos->prev = pos->next;
	pos->next->prev = pos->prev;
}


/* ------------------------------------------- track_ev* routines --- */

/*
 * return true if an event is immediately available
 */
unsigned
track_evavail(struct track *o, struct seqptr *p) {
	return (p->pos != &o->eot && p->delta == p->pos->delta);
}

/*
 * skip the current event the current position must be an event not an
 * empty tic
 */

void
track_evnext(struct track *o, struct seqptr *p) {
#ifdef TRACK_DEBUG
	if (p->pos == &o->eot) {
		dbg_puts("track_evnext: unexpected end of track\n");
		dbg_panic();
	}

	if (p->delta != p->pos->delta) {
		dbg_puts("track_evnext: sync. error\n");
		dbg_panic();
	}
#endif
	p->pos = p->pos->next;
	p->delta = 0;
}

/*
 * move the cursor after the last event in the current tic
 */
void
track_evlast(struct track *o, struct seqptr *p) {
#ifdef TRACK_DEBUG
	if (p->delta > p->pos->delta) {
		dbg_puts("track_evlast: sync. error\n");
		dbg_panic();
	}
#endif
	while (p->delta == p->pos->delta && p->pos != &o->eot) {
		p->pos = p->pos->next;
		p->delta = 0;
	}
}

/*
 * store the current event to the location provided by the caller and
 * move the cursor just after it; the current position must be an
 * event, not an empty tic.
 */
void
track_evget(struct track *o, struct seqptr *p, struct ev *ev) {
#ifdef TRACK_DEBUG
	if (p->pos == &o->eot) {
		dbg_puts("track_evget: unexpected end of track\n");
		dbg_panic();
	}

	if (p->delta != p->pos->delta) {
		dbg_puts("track_evget: sync. error\n");
		dbg_panic();
	}
#endif
	*ev = p->pos->ev;
	p->pos = p->pos->next;
	p->delta = 0;
}

/*
 * insert an event and put the cursor just after it
 */
void
track_evput(struct track *o, struct seqptr *p, struct ev *ev) {
	struct seqev *se;
#ifdef TRACK_DEBUG
	if (p->delta > p->pos->delta) {
		dbg_puts("track_evput: sync. error\n");
		dbg_panic();
	}	
#endif
	se = seqev_new();
	se->ev = *ev;
	se->delta = p->delta;
	p->pos->delta -= p->delta;
	p->delta = 0;
	/* link to the list */
	se->next = p->pos;
	se->prev = p->pos->prev;
	*se->prev = se;
	p->pos->prev = &se->next;
}

/*
 * delete the event following the current position; the current
 * position must be an event not an empty tic
 */
void
track_evdel(struct track *o, struct seqptr *p) {
	struct seqev *next;
#ifdef TRACK_DEBUG
	if (p->pos == &o->eot) {
		dbg_puts("track_evdel: unexpected end of track\n");
		dbg_panic();
	}	

	if (p->delta != p->pos->delta) {
		dbg_puts("track_evdel: sync. error\n");
		dbg_panic();
	}	
#endif
	next = p->pos->next;
	next->delta += p->pos->delta;
	/* unlink and delete p->pos */
	*(p->pos->prev) = next;
	next->prev = p->pos->prev;
	seqev_del(p->pos);
	/* fix current position */
	p->pos = next;
}

/*
 * insert an event at the given position, i.e.  n tics after the
 * current position for instance this is useful to implement the "echo
 * track" 
 * WARNING: if there are already events at this offset, we put
 * the event in the last position blank space is added at the end of
 * track if necessary
 */
void
track_evinsat(struct track *o, struct seqptr *p, struct ev *ev, unsigned ntics) {
	struct seqev *pos = p->pos, *targ;
	unsigned delta = p->delta, amount;
	
#ifdef TRACK_DEBUG
	if (delta > p->pos->delta) {
		dbg_puts("track_evinsat: sync. error\n");
		dbg_panic();
	}
#endif
	while (ntics > 0 || pos->delta == 0) {
		amount = pos->delta - delta;
		if (amount == 0) {
			if (pos == &o->eot) {
				o->eot.delta += ntics;
				delta += ntics;
				break;
			}
			pos = pos->next;
			delta = 0;
		} else {
			if (amount > ntics)
				amount = ntics;
			delta += amount;
			ntics -= amount;
		}
	}
	targ = seqev_new();
	targ->ev = *ev;
	targ->delta = delta;
	/* link to the list */
	targ->next = pos;
	targ->prev = pos->prev;
	*(pos->prev) = targ;
	pos->prev = &targ->next;
	/* fix current position */
	p->pos = targ;
}


/* ------------------------------------------ track_tic* routines --- */

/*
 * return true if a tic is available
 */
unsigned
track_ticavail(struct track *o, struct seqptr *p) {
	return (p->delta < p->pos->delta);
}

/*
 * skip the current tic the current position must be an empty tic, not
 * an event.
 */
void
track_ticnext(struct track *o, struct seqptr *p) {
#ifdef TRACK_DEBUG
	if (p->delta >= p->pos->delta) {
		dbg_puts("track_ticnext: sync. error\n");
		dbg_panic();
	}
#endif
	p->delta++;
}

/*
 * move the cursor to the following event in the track, return the
 * number of skiped tics
 */	
unsigned
track_ticlast(struct track *o, struct seqptr *p) {
	unsigned ntics;
	
#ifdef TRACK_DEBUG
	if (p->delta > p->pos->delta) {
		dbg_puts("track_ticlast: sync. error\n");
		track_dump(o);
		dbg_panic();
	}
#endif
	ntics = p->pos->delta - p->delta;
	p->delta += ntics;
	return ntics;
}

/*
 * try to move to the next event, but never move
 * more than 'max' tics. Retrun the number of skipped tics
 */
unsigned
track_ticskipmax(struct track *o, struct seqptr *p, unsigned max) {
	unsigned ntics;
	
#ifdef TRACK_DEBUG
	if (p->delta > p->pos->delta) {
		dbg_puts("track_ticskipmax: sync. error\n");
		track_dump(o);
		dbg_panic();
	}
#endif
	ntics = p->pos->delta - p->delta;
	if (ntics > max) {
		ntics = max;
	}
	p->delta += ntics;
	return ntics;
}

/*
 * delete tics until the next event, but never move more than 'max'
 * tics. Retrun the number of deletes tics
 */
unsigned
track_ticdelmax(struct track *o, struct seqptr *p, unsigned max) {
	unsigned ntics;
	
#ifdef TRACK_DEBUG
	if (p->delta > p->pos->delta) {
		dbg_puts("track_ticdelmax: sync. error\n");
		track_dump(o);
		dbg_panic();
	}
#endif
	ntics = p->pos->delta - p->delta;
	if (ntics > max) {
		ntics = max;
	}
	p->pos->delta -= ntics;
	return ntics;
}

/*
 * insert 'max' tics on the current position
 */
void
track_ticinsmax(struct track *o, struct seqptr *p, unsigned max) {
	p->pos->delta += max;
}

/*
 * delete the following tic ie shifts the rest of the track
 */
void
track_ticdel(struct track *o, struct seqptr *p) {
#ifdef TRACK_DEBUG
	if (p->delta >= p->pos->delta) {
		dbg_puts("track_ticdel: sync. error\n");
		dbg_panic();
	}	
#endif
	p->pos->delta--;
}

/*
 * insert a tic at the current position (will be the next to be read)
 */
void
track_ticins(struct track *o, struct seqptr *p) {
	p->pos->delta++;
}

/* ----------------------------------------------- misc. routines --- */

/*
 * free all events in the track and set its length to zero; also put
 * the cursor at the beggining
 */
void
track_clear(struct track *o, struct seqptr *p) {
	struct seqev *i, *inext;
	
	for (i = o->first;  i != &o->eot;  i = inext) {
		inext = i->next;
		seqev_del(i);
	}
	o->eot.delta = 0;
	o->first = &o->eot;
	o->eot.prev = &o->first;
	p->pos = o->first;
	p->delta = 0;
}
	
/*
 * place the cursor at the beginning of the track
 * first event, first tic
 */
void
track_rew(struct track *o, struct seqptr *p) {
	p->pos = o->first;
	p->delta = 0;
}

/*
 * return true if the cursor is at the end
 * of the track (ie the last event/tic is already retrieved)
 */
unsigned 
track_finished(struct track *o, struct seqptr *p) {
	return p->pos == &o->eot && p->delta == o->eot.delta;
}

/*
 * move the cursor n tics forward. if the cursor reaches the end of
 * the track, return the number of remaining tics
 */
unsigned
track_seek(struct track *o, struct seqptr *p, unsigned ntics) {
	struct seqev *pos = p->pos;
	unsigned delta = p->delta, amount;
	
#ifdef TRACK_DEBUG
	if (delta > p->pos->delta) {
		dbg_puts("track_seek: sync. error\n");
		dbg_panic();
	}
#endif
	while (ntics > 0) {
		amount = pos->delta - delta;
		if (amount == 0) {
			if (pos == &o->eot) {
				break;
			} 
			pos = pos->next;
			delta = 0;
		} else {
			if (amount > ntics) amount = ntics;
			delta += amount;
			ntics -= amount;
		}
	}
	p->pos = pos;
	p->delta = delta;
	return ntics;
}

/*
 * move the cursor n tics forward.  if the end of the track is
 * reached, blank space will be added.
 */
void
track_seekblank(struct track *o, struct seqptr *p, unsigned ntics) {
	ntics = track_seek(o, p, ntics);
	if (ntics > 0) {
		o->eot.delta += ntics;
		p->delta += ntics;
	}
}

/*
 * set the chan (dev/midichan pair) of
 * all voice events
 */
void
track_setchan(struct track *src, unsigned dev, unsigned ch) {
	struct seqev *i;

	for (i = src->first; i != NULL; i = i->next) {
		if (EV_ISVOICE(&i->ev)) {
			i->ev.data.voice.dev = dev;
			i->ev.data.voice.ch = ch;
		}
	}
}

/*
 * fill a map of used channels/devices
 */
void
track_chanmap(struct track *o, char *map) {
	struct seqev *se;
	unsigned dev, ch, i;
	
	for (i = 0; i < DEFAULT_MAXNCHANS; i++) {
		map[i] = 0;
	}

	for (se = o->first; se != NULL; se = se->next) {
		if (EV_ISVOICE(&se->ev)) {
			dev = se->ev.data.voice.dev;
			ch  = se->ev.data.voice.ch;
			if (dev >= DEFAULT_MAXNDEVS || ch >= 16) {
				dbg_puts("track_chanmap: bogus dev/ch pair, stopping\n");
				break;
			}
			map[dev * 16 + ch] = 1;
		}
	}
}
