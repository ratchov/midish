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
undo_new(struct song *s, int type, char *func, char *name)
{
	struct undo *u;

	u = xmalloc(sizeof(struct undo), "undo");
	u->type = type;
	u->func = func;
	u->name = name;
	u->size = 0;
	return u;
}

void
undo_pop(struct song *s)
{
	struct undo_fdel_trk *p;
	struct undo *u;
	int done = 0;

	while (!done) {
		u = s->undo;
		if (u == NULL)
			return;
		s->undo = u->next;
		if (u->func) {
			log_puts("undo:");
			log_puts(" ");
			log_puts(u->func);
			if (u->name) {
				log_puts(" ");
				log_puts(u->name);
			}
			log_puts("\n");
			done = 1;
		}
		switch (u->type) {
		case UNDO_TRACK:
			track_undorestore(u->u.track.track, &u->u.track.data);
			break;
		case UNDO_TREN:
			str_delete(u->u.tren.trk->name.str);
			u->u.tren.trk->name.str = u->u.tren.name;
			break;
		case UNDO_TDEL:
			track_undorestore(&u->u.tdel.trk->track, &u->u.tdel.data);
			name_add(&s->trklist, &u->u.tdel.trk->name);
			if (s->curtrk == NULL)
				s->curtrk = u->u.tdel.trk;
			break;
		case UNDO_TNEW:
			song_trkdel(s, u->u.tdel.trk);
			break;
		case UNDO_FILT:
			filt_reset(u->u.filt.filt);
			*u->u.filt.filt = u->u.filt.data;
			break;
		case UNDO_FREN:
			str_delete(u->u.fren.filt->name.str);
			u->u.fren.filt->name.str = u->u.fren.name;
			break;
		case UNDO_FDEL:
			name_add(&s->filtlist, &u->u.fdel.filt->name);
			if (s->curfilt == NULL)
				s->curfilt = u->u.fdel.filt;
			while ((p = u->u.fdel.trks) != NULL) {
				p->trk->curfilt = u->u.fdel.filt;
				u->u.fdel.trks = p->next;
				xfree(p);
			}
			break;
		case UNDO_FNEW:
			song_filtdel(s, u->u.fdel.filt);
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
}

void
undo_clear(struct song *s, struct undo **pos)
{
	struct undo *u;

	while ((u = *pos) != NULL) {
		*pos = u->next;
		switch (u->type) {
		case UNDO_TRACK:
			xfree(u->u.track.data.evs);
			break;
		case UNDO_TREN:
			str_delete(u->u.tren.name);
			break;
		case UNDO_TDEL:
			xfree(u->u.tdel.data.evs);
			track_done(&u->u.tdel.trk->track);
			name_done(&u->u.tdel.trk->name);
			xfree(u->u.tdel.trk);
			break;
		case UNDO_TNEW:
			break;
		case UNDO_FILT:
			filt_reset(&u->u.filt.data);
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
	u->nrm = 0;
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
undo_track_save(struct song *s, struct track *t, char *func, char *name)
{
	struct undo *u;

	u = undo_new(s, UNDO_TRACK, func, name);
	u->u.track.track = t;
	u->size = track_undosave(t, &u->u.track.data);
	undo_push(s, u);
}

void
undo_track_diff(struct song *s)
{
	struct undo *u = s->undo;
	unsigned size;

	if (u == NULL || u->type != UNDO_TRACK) {
		log_puts("undo_track_diff: no data to diff\n");
		return;
	}
	size = track_undodiff(u->u.track.track, &u->u.track.data);
	s->undo_size += size - u->size;
	u->size = size;
}

void
undo_tren_do(struct song *s, struct songtrk *t, char *name, char *func)
{
	struct undo *u;

	u = undo_new(s, UNDO_TREN, func, t->name.str);
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
	u = undo_new(s, UNDO_TDEL, func, t->name.str);
	u->u.tdel.song = s;
	u->u.tdel.trk = t;
	u->size = track_undosave(&t->track, &u->u.tdel.data);
	name_remove(&s->trklist, &t->name);
	undo_push(s, u);
}

struct songtrk *
undo_tnew_do(struct song *s, char *func, char *name)
{
	struct undo *u;
	struct songtrk *t;

	t = song_trknew(s, name);
	u = undo_new(s, UNDO_TNEW, func, t->name.str);
	u->u.tdel.song = s;
	u->u.tdel.trk = t;
	undo_push(s, u);
	return t;
}

int
filtnode_size(struct filtnode **sloc)
{
	struct filtnode *s;
	int size = 0;

	for (s = *sloc; s != NULL; s = s->next)
		size += sizeof(struct filtnode) +
		    filtnode_size(&s->dstlist);
	return size;
}

void
filtnode_dup(struct filtnode **dloc, struct filtnode **sloc)
{
	struct filtnode *s, *d;

	s = *sloc;
	while (s != NULL) {
		d = filtnode_new(&s->es, dloc);
		filtnode_dup(&d->dstlist, &s->dstlist);
		dloc = &d->next;
		s = s->next;
	}
}

int
filt_size(struct filt *f)
{
	return filtnode_size(&f->map) +
	    filtnode_size(&f->vcurve) +
	    filtnode_size(&f->transp);

}

void
filt_undosave(struct filt *f, struct filt *data)
{
	filt_init(data);
	filtnode_dup(&data->map, &f->map);
	filtnode_dup(&data->vcurve, &f->vcurve);
	filtnode_dup(&data->transp, &f->transp);
}

void
undo_filt_save(struct song *s, struct filt *f, char *func, char *name)
{
	struct undo *u;

	u = undo_new(s, UNDO_FILT, func, name);
	u->u.filt.filt = f;
	filt_undosave(f, &u->u.filt.data);
	u->size += filt_size(&u->u.filt.data);
	undo_push(s, u);
}

void
undo_fren_do(struct song *s, struct songfilt *t, char *name, char *func)
{
	struct undo *u;

	u = undo_new(s, UNDO_FREN, func, t->name.str);
	u->u.fren.filt = t;
	u->u.fren.name = t->name.str;
	t->name.str = str_new(name);
	undo_push(s, u);
}

void
undo_fdel_do(struct song *s, struct songfilt *f, char *func)
{
	struct undo *u;
	struct songtrk *t;
	struct undo_fdel_trk *p;

	u = undo_new(s, UNDO_FDEL, func, f->name.str);
	u->u.fdel.song = s;
	u->u.fdel.filt = f;
	u->u.fdel.trks = NULL;

	SONG_FOREACH_TRK(s, t) {
		if (t->curfilt != f)
			continue;
		p = xmalloc(sizeof(struct undo_fdel_trk), "fdel_trk");
		u->size += sizeof(struct undo_fdel_trk);
		p->trk = t;
		p->next = u->u.fdel.trks;
		u->u.fdel.trks = p;
		t->curfilt = NULL;
	}

	if (s->curfilt == f)
		song_setcurfilt(s, NULL);

	name_remove(&s->filtlist, &f->name);

	undo_push(s, u);
}

struct songfilt *
undo_fnew_do(struct song *s, char *func, char *name)
{
	struct undo *u;
	struct songfilt *t;

	t = song_filtnew(s, name);
	u = undo_new(s, UNDO_FNEW, func, t->name.str);
	u->u.fdel.song = s;
	u->u.fdel.filt = t;
	u->u.fdel.trks = NULL;
	undo_push(s, u);
	return t;
}
