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
 * a track (struct track *o) is a linked list of
 * events. Each event (struct seqev) is made by
 *	- a midi event (struct ev)
 *	- the number of tics before the event is to be played
 *
 * Since a track can contain an amount of blank space
 * after the last event (if any), there is always an end-of-track event
 * in the list.
 *
 *	- each clock tick marks the begining of a delta
 *	- each event (struct ev) is played after delta ticks
 *
 */

#include "utils.h"
#include "pool.h"
#include "track.h"

struct pool seqev_pool;

void
seqev_pool_init(unsigned size)
{
	pool_init(&seqev_pool, "seqev", sizeof(struct seqev), size);
}

void
seqev_pool_done(void)
{
	pool_done(&seqev_pool);
}


struct seqev *
seqev_new(void)
{
	return (struct seqev *)pool_new(&seqev_pool);
}

void
seqev_del(struct seqev *se)
{
	pool_del(&seqev_pool, se);
}

void
seqev_dump(struct seqev *i)
{
	log_putu(i->delta);
	log_puts("\t");
	ev_log(&i->ev);
}

/*
 * initialise the track
 */
void
track_init(struct track *o)
{
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
track_done(struct track *o)
{
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
track_dump(struct track *o)
{
	struct seqev *i;
	unsigned tic = 0, num = 0;

	for (i = o->first; i != NULL; i = i->next) {
		tic += i->delta;
		log_putu(num);
		log_puts("\t");
		log_putu(tic);
		log_puts("\t+");
		seqev_dump(i);
		log_puts("\n");
		num++;
	}
}

/*
 * return true if the track is empty
 */
unsigned
track_isempty(struct track *o)
{
	return o->first->ev.cmd == EV_NULL && o->first->delta == 0;
}

/*
 * remove trailing blank space
 */
void
track_chomp(struct track *o)
{
	o->eot.delta = 0;
}

/*
 * shift the track origin forward
 */
void
track_shift(struct track *o, unsigned ntics)
{
	o->first->delta += ntics;
}

/*
 * swap contents of two tracks
 */
void
track_swap(struct track *t1, struct track *t2)
{
	struct seqev *se, eot;

	/* swap list of events */
	se = t1->first;
	t1->first = t2->first;
	t2->first = se;
	t1->first->prev = &t1->first;
	t2->first->prev = &t2->first;

	/* swap eot events (as they are part of the track) */
	eot = t1->eot;
	t1->eot = t2->eot;
	t2->eot = eot;

	/* fix references to eot events */
	*t1->eot.prev = &t1->eot;
	*t2->eot.prev = &t2->eot;
}

/*
 * return true if an event is available on the track
 */
unsigned
seqev_avail(struct seqev *pos)
{
	return (pos->ev.cmd != EV_NULL);
}

/*
 * insert an event (stored in an already allocated seqev structure)
 * just before the event of the given position (the delta field of the
 * given event is ignored)
 */
void
seqev_ins(struct seqev *pos, struct seqev *se)
{
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
seqev_rm(struct seqev *pos)
{
#ifdef TRACK_DEBUG
	if (pos->ev.cmd == EV_NULL) {
		log_puts("seqev_rm: unexpected end of track\n");
		panic();
	}
#endif
	pos->next->delta += pos->delta;
	pos->delta = 0;
	/* since se != &eot, next is never NULL */
	*pos->prev = pos->next;
	pos->next->prev = pos->prev;
}

/*
 * return the number of events in the track
 */
unsigned
track_numev(struct track *o)
{
	unsigned n;
	struct seqev *i;

	n = 0;
	for (i = o->first; i != NULL; i = i->next)
		n++;
	return n;
}

/*
 * return the number of ticks in the track
 * ie its length (eot included, of course)
 */
unsigned
track_numtic(struct track *o)
{
	unsigned ntics;
	struct seqev *i;

	ntics = 0;
	for(i = o->first; i != NULL; i = i->next)
		ntics += i->delta;
	return ntics;
}


/*
 * remove all events from the track
 */
void
track_clear(struct track *o)
{
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
 * set the chan (dev/midichan pair) of
 * all voice events
 */
void
track_setchan(struct track *src, unsigned dev, unsigned ch)
{
	struct seqev *i;

	for (i = src->first; i != NULL; i = i->next) {
		if (EV_ISVOICE(&i->ev)) {
			i->ev.dev = dev;
			i->ev.ch = ch;
		}
	}
}

/*
 * fill a map of used channels/devices
 */
void
track_chanmap(struct track *o, char *map)
{
	struct seqev *se;
	unsigned dev, ch, i;

	for (i = 0; i < DEFAULT_MAXNCHANS; i++) {
		map[i] = 0;
	}

	for (se = o->first; se != NULL; se = se->next) {
		if (EV_ISVOICE(&se->ev)) {
			dev = se->ev.dev;
			ch  = se->ev.ch;
			if (dev >= DEFAULT_MAXNDEVS || ch >= 16) {
				log_puts("track_chanmap: bogus dev/ch pair, stopping\n");
				break;
			}
			map[dev * 16 + ch] = 1;
		}
	}
}

/*
 * return the number of events of the given type
 */
unsigned
track_evcnt(struct track *o, unsigned cmd)
{
	struct seqev *se;
	unsigned cnt = 0;

	for (se = o->first; se != NULL; se = se->next) {
		if (se->ev.cmd == cmd)
			cnt++;
	}
	return cnt;
}

unsigned
track_undosave(struct track *t, struct track_undo *u)
{
	struct seqev *i;
	struct track_undoev *e;
	unsigned size;

	u->nins = track_numev(t);
	size = sizeof(struct track_undoev) * u->nins;
	u->evs = xmalloc(size, "track_undo");
	e = u->evs;
	for (i = t->first; i != NULL; i = i->next) {
		e->delta = i->delta;
		e->ev = i->ev;
		e++;
	}
	u->pos = 0;
	u->nrm = u->nins;
	return size;
}

void
track_diff(struct track_undo *u1, struct track_undo *u2,
	unsigned *pos, unsigned *nrm, unsigned *nins)
{
	unsigned start, end1, end2;

	start = 0;
	while (1) {
		if (start == u1->nins || start == u2->nins)
			break;
		if (u1->evs[start].delta != u2->evs[start].delta)
			break;
		if (!ev_eq(&u1->evs[start].ev, &u2->evs[start].ev))
			break;
		start++;
	}

	end1 = u1->nins;
	end2 = u2->nins;
	while (1) {
		if (end1 == start || end2 == start)
			break;
		if (u1->evs[end1 - 1].delta != u2->evs[end2 - 1].delta)
			break;
		if (!ev_eq(&u1->evs[end1 - 1].ev, &u2->evs[end2 - 1].ev))
			break;
		end1--;
		end2--;
	}
	*pos = start;
	*nrm = end1 - start;
	*nins = end2 - start;
}

void
track_undodiff(struct track *t, struct track_undo *orig)
{
	struct track_undo mod;
	unsigned int i, pos, nrm, nins;
	struct track_undoev *evs;

	track_undosave(t, &mod);
	track_diff(orig, &mod, &pos, &nrm, &nins);

	evs = xmalloc(sizeof(struct track_undoev) * nrm, "track_diff");
	for (i = 0; i < nrm; i++)
		evs[i] = orig->evs[pos + i];
	xfree(mod.evs);
	xfree(orig->evs);
	orig->evs = evs;
	orig->nins = nins;
	orig->nrm = nrm;
	orig->pos = pos;
}

void
track_undorestore(struct track *t, struct track_undo *u)
{
	unsigned n;
	struct seqev *pos, *se;
	struct track_undoev *e;

	/* go to pos */
	pos = t->first;
	for (n = u->pos; n > 0; n--)
		pos = pos->next;

	/* remove events that were inserted */
	for (n = u->nins; n > 0; n--) {
		if (pos->ev.cmd == EV_NULL) {
			if (n != 1) {
				log_puts("can't remove eot event\n");
				panic();
			}
			pos->delta = 0;
			break;
		}
		se = pos;
		pos = se->next;

		/* remove seqev */
		*se->prev = pos;
		pos->prev = se->prev;
		seqev_del(se);
	}

	/* insert events that were removed */
	e = u->evs;
	for (n = u->nrm; n > 0; n--) {
		if (e->ev.cmd == EV_NULL) {
			if (n != 1) {
				log_puts("can't insert eot event\n");
				panic();
			}
			t->eot.delta = e->delta;
			break;
		}
		se = seqev_new();
		se->ev = e->ev;
		se->delta = e->delta;
		e++;

		/* insert seqev */
		se->next = pos;
		se->prev = pos->prev;
		*(se->prev) = se;
		pos->prev = &se->next;
	}
	xfree(u->evs);
}
