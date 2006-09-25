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
 * a seqptr represents the position of a cursor on the track
 * In play mode, the field 'o->pos' points to a pointer to the event
 * to be played, thus the current event is referenced by *o->pos. The 
 * field 'delta' contains the number of tics elapsed since the last
 * played event. Thus when o->delta reaches (*o->pos)->delta,
 * the event is to be played.
 *
 * In record mode, the field 'o->pos' points to a pointer
 * which is the 'next' field of the last recorded event.
 *
 * The track structure contain a cursor (a pos/delta pair) which is
 * used to walk through the track. All the track_* routines
 * maintain a consistent value of the cursor.
 *
 * track_ev* routines involve the current event,
 * When calling such a routine, the current position 
 * pointer must be an event not an empty tic (except
 * for track_evavail and track_evins). They are meant to be fast enough
 * to be used in real-time operations.
 *
 * track_tic* routines involves a single tic,
 * When calling such a routine, the current position
 * pointer must be an empty tic, not an event (except
 * for track_ticavail and track_ticins). They are meant to be fast enough
 * to be used in real-time operations.
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
	 * allocates and initialises a track structure
	 */

struct track *
track_new(void) {
	struct track *o;
	o = (struct track *)mem_alloc(sizeof(struct track));
	track_init(o);
	return o;	
}

	/*
	 * frees a track structure
	 */
	 
void
track_delete(struct track *o) {
	track_done(o);
	mem_free(o);
}

	/*
	 * initialises the track
	 */

void
track_init(struct track *o) {
	o->eot.next = NULL;
	o->eot.delta = 0;
	o->first = &o->eot;
	o->eot.ev.cmd = EV_NULL;
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
}

	/*
	 * dumps a track on stderr, for debugging purposes
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
	 * returns the number of events in the track
	 * does not change the current position pointer
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
	 * returns the number of tics in the track
	 * ie its length (eot included, of course)
	 * does not change the current position pointer
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


/* ---------------------------------------- track_seqev* routines --- */

	/*
	 * returns true if an (delta,event)
	 * pair is available
	 */
	
unsigned
track_seqevavail(struct track *o, struct seqptr *p) {
	return ((*p->pos) != &o->eot);
}

	/*
	 * inserts an event (stored in an already allocated
	 * seqev structure). The delta field of the
	 * structure will be stored as blank time
	 * before the event. Thus, in normal usage 
	 * se->delta will be nearly always zero.
	 */

void
track_seqevins(struct track *o, struct seqptr *p, struct seqev *se) {
#ifdef TRACK_DEBUG
	if (p->delta > (*p->pos)->delta) {
		dbg_puts("track_seqevput: sync. error\n");
		dbg_panic();
	}	
#endif
	se->delta += p->delta;
	(*p->pos)->delta -= p->delta;
	se->next = *p->pos;
	*p->pos = se;
}

	/*
	 * remove the next event and the blank
	 * space between the current position an
	 * the event. The seqev structure is not
	 * deleted and the delta field is set
	 * to the removed blank space.
	 */

struct seqev *
track_seqevrm(struct track *o, struct seqptr *p) {
	struct seqev *se;
#ifdef TRACK_DEBUG
	if ((*p->pos) == &o->eot) {
		dbg_puts("track_seqevrm: unexpected end of track\n");
		dbg_panic();
	}	

	if (p->delta > (*p->pos)->delta) {
		dbg_puts("track_seqevrm: sync. error\n");
		dbg_panic();
	}	
#endif
	se = *p->pos;
	se->next->delta += p->delta;
	se->delta -= p->delta;
	*p->pos = se->next;
	return se;
}


	/*
	 * moves to the next (delta,event) pair
	 * return the tics we moved
	 */

unsigned
track_seqevnext(struct track *o, struct seqptr *p) {
	unsigned tics;
#ifdef TRACK_DEBUG
	if ((*p->pos) == &o->eot) {
		dbg_puts("track_seqevnext: unexpected end of track\n");
		dbg_panic();
	}

	if (p->delta > (*p->pos)->delta) {
		dbg_puts("track_seqevnext: sync. error\n");
		dbg_panic();
	}
#endif
	tics = (*p->pos)->delta - p->delta;
	p->pos = &(*p->pos)->next;
	p->delta = 0;
	return tics;
}



/* ------------------------------------------- track_ev* routines --- */

	/*
	 * returns true if an event is immediately available
	 */
	
unsigned
track_evavail(struct track *o, struct seqptr *p) {
	return ((*p->pos) != &o->eot && p->delta == (*p->pos)->delta);
}

	/*
	 * skips the current event
	 * the current position must be an event not
	 * an empty tic
	 */

void
track_evnext(struct track *o, struct seqptr *p) {
#ifdef TRACK_DEBUG
	if ((*p->pos) == &o->eot) {
		dbg_puts("track_evnext: unexpected end of track\n");
		dbg_panic();
	}

	if (p->delta != (*p->pos)->delta) {
		dbg_puts("track_evnext: sync. error\n");
		dbg_panic();
	}
#endif
	p->pos = &(*p->pos)->next;
	p->delta = 0;
}

	/*
	 * moves the cursor after the last event
	 * in the current tic
	 */
	
void
track_evlast(struct track *o, struct seqptr *p) {
#ifdef TRACK_DEBUG
	if (p->delta > (*p->pos)->delta) {
		dbg_puts("track_evlast: sync. error\n");
		dbg_panic();
	}
#endif
	while (p->delta == (*p->pos)->delta && *p->pos != &o->eot) {
		p->pos = &(*p->pos)->next;
		p->delta = 0;
	}
}

	/*
	 * store the current event to the location
	 * provided by the caller and move the cursor juste after it;
	 * the current position must be an event,
	 * not an empty tic.
	 */

void
track_evget(struct track *o, struct seqptr *p, struct ev *ev) {
#ifdef TRACK_DEBUG
	if ((*p->pos) == &o->eot) {
		dbg_puts("track_evget: unexpected end of track\n");
		dbg_panic();
	}

	if (p->delta != (*p->pos)->delta) {
		dbg_puts("track_evget: sync. error\n");
		dbg_panic();
	}
#endif
	*ev = (*p->pos)->ev;
	p->pos = &(*p->pos)->next;
	p->delta = 0;
}

	/*
	 * inserts an event and puts the cursor
	 * just after it
	 */

void
track_evput(struct track *o, struct seqptr *p, struct ev *ev) {
	struct seqev *se;
#ifdef TRACK_DEBUG
	if (p->delta > (*p->pos)->delta) {
		dbg_puts("track_evput: sync. error\n");
		dbg_panic();
	}	
#endif
	se = seqev_new();
	se->ev = *ev;
	se->delta = p->delta;
	(*p->pos)->delta -= p->delta;
	se->next = *p->pos;
	*p->pos = se;
	p->pos = &se->next;
	p->delta = 0;
}

	/*
	 * deletes the follwing event
	 * the current position must be an event not
	 * an empty tic
	 */

void
track_evdel(struct track *o, struct seqptr *p) {
	struct seqev *next;
#ifdef TRACK_DEBUG
	if ((*p->pos) == &o->eot) {
		dbg_puts("track_evdel: unexpected end of track\n");
		dbg_panic();
	}	

	if (p->delta != (*p->pos)->delta) {
		dbg_puts("track_evdel: sync. error\n");
		dbg_panic();
	}	
#endif
	next = (*p->pos)->next;
	next->delta += (*p->pos)->delta;
	seqev_del(*p->pos);
	*p->pos = next;
}

	/*
	 * inserts an event at the given position, i.e.
	 * n tics after the current position
	 * for instance this is useful to implement the "echo track"
	 * WARNING: if there are already events at this offset,
	 * we put the event in the last position
	 * blank space is added at the end of track
	 * if necessary
	 */


void
track_evinsat(struct track *o, struct seqptr *p, struct ev *ev, unsigned ntics) {
	struct seqev **pos = p->pos, *targ;
	unsigned delta = p->delta, amount;
	
#ifdef TRACK_DEBUG
	if (delta > (*p->pos)->delta) {
		dbg_puts("track_evinsat: sync. error\n");
		dbg_panic();
	}
#endif
	while (ntics > 0 || (*pos)->delta == 0) {
		amount = (*pos)->delta - delta;
		if (amount == 0) {
			if (*pos == &o->eot) {
				o->eot.delta += ntics;
				delta += ntics;
				break;
			}
			pos = &(*pos)->next;
			delta = 0;
		} else {
			if (amount > ntics) amount = ntics;
			delta += amount;
			ntics -= amount;
		}
	}

	targ = seqev_new();
	targ->ev = *ev;
	targ->next = *pos;
	targ->delta = delta;
	(*pos)->delta -= delta;
	*pos = targ;
}


/* ------------------------------------------ track_tic* routines --- */

	/*
	 * returns true if a tic is available
	 */

unsigned
track_ticavail(struct track *o, struct seqptr *p) {
	return (p->delta < (*p->pos)->delta);
}

	/*
	 * skips the current tic
	 * the current position must be an empty tic, not
	 * an event.
	 */

void
track_ticnext(struct track *o, struct seqptr *p) {
#ifdef TRACK_DEBUG
	if (p->delta >= (*p->pos)->delta) {
		dbg_puts("track_ticnext: sync. error\n");
		dbg_panic();
	}
#endif
	p->delta++;
}

	/*
	 * moves the cursor to the following event in the track,
	 * returns the number of skiped tics
	 */	

unsigned
track_ticlast(struct track *o, struct seqptr *p) {
	unsigned ntics;
	
#ifdef TRACK_DEBUG
	if (p->delta > (*p->pos)->delta) {
		dbg_puts("track_ticlast: sync. error\n");
		track_dump(o);
		dbg_panic();
	}
#endif
	ntics = (*p->pos)->delta - p->delta;
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
	if (p->delta > (*p->pos)->delta) {
		dbg_puts("track_ticskipmax: sync. error\n");
		track_dump(o);
		dbg_panic();
	}
#endif
	ntics = (*p->pos)->delta - p->delta;
	if (ntics > max) {
		ntics = max;
	}
	p->delta += ntics;
	return ntics;
}

	/*
	 * delete tics until the next event, but never move
	 * more than 'max' tics. Retrun the number of deletes tics
	 */

unsigned
track_ticdelmax(struct track *o, struct seqptr *p, unsigned max) {
	unsigned ntics;
	
#ifdef TRACK_DEBUG
	if (p->delta > (*p->pos)->delta) {
		dbg_puts("track_ticdelmax: sync. error\n");
		track_dump(o);
		dbg_panic();
	}
#endif
	ntics = (*p->pos)->delta - p->delta;
	if (ntics > max) {
		ntics = max;
	}
	(*p->pos)->delta -= ntics;
	return ntics;
}

	/*
	 * insert 'max' tics on the current position
	 */

void
track_ticinsmax(struct track *o, struct seqptr *p, unsigned max) {
	(*p->pos)->delta += max;
}

	/*
	 * deletes the following tic
	 * ie shifts the rest of the track
	 */

void
track_ticdel(struct track *o, struct seqptr *p) {
#ifdef TRACK_DEBUG
	if (p->delta >= (*p->pos)->delta) {
		dbg_puts("track_ticrm: sync. error\n");
		dbg_panic();
	}	
#endif
	(*p->pos)->delta--;
}

	/*
	 * insert a tic at the current position 
	 * (will be the next to be read)
	 */
	
void
track_ticins(struct track *o, struct seqptr *p) {
	(*p->pos)->delta++;
}

/* ----------------------------------------------- misc. routines --- */

	/*
	 * frees all events in the track and
	 * sets its length to zero; 
	 * also puts the cursor at the beggining
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
	p->pos = &o->first;
	p->delta = 0;
}
	
	/*
	 * places the cursor at the beginning of the track
	 * first event, first tic
	 */

void
track_rew(struct track *o, struct seqptr *p) {
	p->pos = &o->first;
	p->delta = 0;
}
	
	/*
	 * returns true if the cursor is at the end
	 * of the track (ie the last event/tic is already retrieved)
	 */

unsigned 
track_finished(struct track *o, struct seqptr *p) {
	return (*p->pos) == &o->eot  &&  p->delta == o->eot.delta;
}

	/*
	 * moves the cursor n tics forward.
	 * if the cursor reaches the end of the track,
	 * we return the number of remaining tics
	 */

unsigned
track_seek(struct track *o, struct seqptr *p, unsigned ntics) {
	struct seqev **pos = p->pos;
	unsigned delta = p->delta, amount;
	
#ifdef TRACK_DEBUG
	if (delta > (*p->pos)->delta) {
		dbg_puts("track_seek: sync. error\n");
		dbg_panic();
	}
	
	/*
	if (ntics == 0 && !(p->pos == &o->first && p->delta == 0)) {
		dbg_puts("track_seek: 0 tics seek from the middle of the track\n");
	}
	*/
#endif
	while (ntics > 0) {
		amount = (*pos)->delta - delta;
		if (amount == 0) {
			if (*pos == &o->eot) {
				break;
			} 
			pos = &(*pos)->next;
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
	 * moves the cursor n tics forward.
	 * if the end of the thrack is reached, blank space
	 * will be added.
	 */

void
track_seekblank(struct track *o, struct seqptr *p, unsigned ntics) {
	ntics = track_seek(o, p, ntics);
	if (ntics > 0) {
		o->eot.delta += ntics;
		p->delta += ntics;
	}
}


