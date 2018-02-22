/*
 * Copyright (c) 2018 Alexandre Ratchov <alex@caoua.org>
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

#include "utils.h"
#include "mididev.h"
#include "mux.h"
#include "track.h"
#include "frame.h"
#include "filt.h"
#include "song.h"
#include "cons.h"
#include "metro.h"
#include "defs.h"
#include "mixout.h"
#include "norm.h"
#include "undo.h"


struct undo *
undo_new(struct song *s, int type, char *func)
{
	struct undo *u;

	u = xmalloc(sizeof(struct undo), "undo");
	u->type = type;
	u->func = func;
	u->size = 0;
	return u;
}

void
undo_pop(struct song *s)
{
	struct undo *u;

	u = s->undo;
	if (u == NULL)
		return;
	s->undo = u->next;
	log_puts("undo: ");
	log_puts(u->func);
	log_puts("\n");
	switch (u->type) {
	case UNDO_TDATA:
		track_undorestore(&u->u.tdata.trk->track, &u->u.tdata.data);
		break;
	case UNDO_TREN:
		str_delete(u->u.tren.trk->name.str);
		u->u.tren.trk->name.str = u->u.tren.name;
		break;
	case UNDO_TDEL:
		track_undorestore(&u->u.tdata.trk->track, &u->u.tdata.data);
		name_add(&s->trklist, &u->u.tdata.trk->name);
		break;
	case UNDO_TNEW:
		song_trkdel(s, u->u.tdata.trk);
		break;
	case UNDO_CDATA:
		track_undorestore(&u->u.cdata.chan->conf, &u->u.cdata.data);
		break;
	default:
		log_puts("undo_pop: bad type\n");
		panic();
	}
	s->undo_size -= u->size;
#ifdef SONG_DEBUG
	log_puts("undo: size -> ");
	log_puti(s->undo_size);
	log_puts("\n");
#endif
	xfree(u);
}

void
undo_clear(struct song *s, struct undo **pos)
{
	struct undo *u;

	while ((u = *pos) != NULL) {
		*pos = u->next;
		switch (u->type) {
		case UNDO_TDATA:
			xfree(u->u.tdata.data.evs);
			break;
		case UNDO_TREN:
			str_delete(u->u.tren.name);
			break;
		case UNDO_TDEL:
			xfree(u->u.tdata.data.evs);
			break;
		case UNDO_TNEW:
			break;
		case UNDO_CDATA:
			xfree(u->u.cdata.data.evs);
			break;
		default:
			log_puts("undo_clear: bad type\n");
			panic();
		}
		s->undo_size -= u->size;
#ifdef SONG_DEBUG
		log_puts("undo: freed ");
		log_puts(u->func);
		log_puts("\n");
#endif
		xfree(u);
	}
}

void
undo_push(struct song *s, struct undo *u)
{
	struct undo **pu;
	size_t size;

	u->next = s->undo;
	s->undo = u;
	s->undo_size += u->size;
#ifdef SONG_DEBUG
	log_puts("undo: ");
	log_puts(u->func);
	log_puts(", size -> ");
	log_puti(s->undo_size);
	log_puts("\n");
#endif

	/*
	 * free old entries exceeding memory usage limit
	 */
	size = 0;
	pu = &s->undo;
	while (1) {
		u = *pu;
		if (u == NULL)
			return;
		size += u->size;
		if (size > UNDO_MAXSIZE)
			break;
		pu = &u->next;
	}

	undo_clear(s, pu);
}

unsigned
track_undosave(struct track *t, struct track_data *u)
{
	struct seqev *i;
	struct seqev_data *e;
	unsigned size;

	u->nins = track_numev(t);
	size = sizeof(struct seqev_data) * u->nins;
	u->evs = xmalloc(size, "track_data");
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
track_diff(struct track_data *u1, struct track_data *u2,
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

unsigned
track_undodiff(struct track *t, struct track_data *orig)
{
	struct track_data mod;
	unsigned int i, pos, nrm, nins;
	struct seqev_data *evs;

	track_undosave(t, &mod);
	track_diff(orig, &mod, &pos, &nrm, &nins);

	evs = xmalloc(sizeof(struct seqev_data) * nrm, "track_diff");
	for (i = 0; i < nrm; i++)
		evs[i] = orig->evs[pos + i];
	xfree(mod.evs);
	xfree(orig->evs);
	orig->evs = evs;
	orig->nins = nins;
	orig->nrm = nrm;
	orig->pos = pos;
	return sizeof(struct seqev_data) * nrm;
}

void
track_undorestore(struct track *t, struct track_data *u)
{
	unsigned n;
	struct seqev *pos, *se;
	struct seqev_data *e;

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

void
undo_tdata_save(struct song *s, struct songtrk *t, char *func)
{
	struct undo *u;

	u = undo_new(s, UNDO_TDATA, func);
	u->u.tdata.trk = t;
	u->size = track_undosave(&t->track, &u->u.tdata.data);
	undo_push(s, u);
}

void
undo_tdata_diff(struct song *s)
{
	struct undo *u = s->undo;
	unsigned size;

	if (u == NULL || u->type != UNDO_TDATA) {
		log_puts("undo_tdata_diff: no data to diff\n");
		return;
	}
	size = track_undodiff(&u->u.tdata.trk->track, &u->u.tdata.data);
	s->undo_size += size - u->size;
	u->size = size;
}

void
undo_tren_do(struct song *s, struct songtrk *t, char *name, char *func)
{
	struct undo *u;

	u = undo_new(s, UNDO_TREN, func);
	u->u.tren.trk = t;
	u->u.tren.name = t->name.str;
	t->name.str = str_new(name);
	undo_push(s, u);
}

void
undo_tdel_do(struct song *s, struct songtrk *t, char *func)
{
	struct undo *u;

	if (s->curtrk == t)
		s->curtrk = NULL;
	u = undo_new(s, UNDO_TDEL, func);
	u->u.tdata.trk = t;
	u->size = track_undosave(&t->track, &u->u.tdata.data);
	name_remove(&s->trklist, &t->name);
	undo_push(s, u);
}

struct songtrk *
undo_tnew_do(struct song *s, char *name, char *func)
{
	struct undo *u;
	struct songtrk *t;

	t = song_trknew(s, name);
	u = undo_new(s, UNDO_TNEW, func);
	u->u.tdata.trk = t;
	undo_push(s, u);
	return t;
}

void
undo_cdata_save(struct song *s, struct songchan *c, char *func)
{
	struct undo *u;

	u = undo_new(s, UNDO_CDATA, func);
	u->u.cdata.chan = c;
	u->size = track_undosave(&c->conf, &u->u.cdata.data);
	undo_push(s, u);
}
