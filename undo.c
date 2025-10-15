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
	char *sep, *name;
	struct songtrk *t;
	struct undo_fdel_trk *p;
	struct sysex *x;
	struct undo *u;
	int done = 0;

	while (!done) {
		u = s->undo;
		if (u == NULL)
			return;
		s->undo = u->next;
		if (u->func) {
			/*
			 * XXX: add a proper undo_fmt() function to print
			 * the real thing
			 */
			if (u->name) {
				sep = " ";
				name = u->name;
			} else {
				sep = "";
				name = "";
			}
			logx(1, "undo: %s%s%s", u->func, sep, name);
			done = 1;
		}
		switch (u->type) {
		case UNDO_EMPTY:
			break;
		case UNDO_STR:
			str_delete(*u->u.ren.ptr);
			*u->u.ren.ptr = u->u.ren.val;
			break;
		case UNDO_UINT:
			*u->u.uint.ptr = u->u.uint.val;
			break;
		case UNDO_TRACK:
			track_undorestore(u->u.track.track, &u->u.track.data);
			break;
		case UNDO_TDEL:
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
		case UNDO_CDEL:
			name_add(&s->chanlist, &u->u.cdel.chan->name);
			if (u->u.cdel.chan->isinput) {
				if (s->curin == NULL)
					s->curin = u->u.cdel.chan;
			} else {
				if (s->curout == NULL)
					s->curout = u->u.cdel.chan;
			}
			break;
		case UNDO_CNEW:
			song_chandel(s, u->u.cdel.chan);
			break;
		case UNDO_XADD:
			x = sysexlist_rm(u->u.sysex.list,
			    u->u.sysex.data.pos);
			sysex_del(x);
			break;
		case UNDO_XRM:
			x = sysex_undorestore(&u->u.sysex.data);
			sysexlist_add(u->u.sysex.list,
			    u->u.sysex.data.pos, x);
			break;
		case UNDO_XDEL:
			name_add(&s->sxlist, &u->u.xdel.sx->name);
			if (s->cursx == NULL)
				s->cursx = u->u.xdel.sx;
			break;
		case UNDO_XNEW:
			song_sxdel(s, u->u.xdel.sx);
			break;
		case UNDO_SCALE:
			track_scale(&s->meta,
			    u->u.scale.newunit, u->u.scale.oldunit);
			SONG_FOREACH_TRK(s, t) {
				track_scale(&t->track,
				    u->u.scale.newunit, u->u.scale.oldunit);
			}
			break;
		default:
			logx(1, "%s: bad type", __func__);
			panic();
		}
		s->undo_size -= u->size;
#ifdef SONG_DEBUG
		logx(1, "%s: undo: size -> %u", __func__, s->undo_size);
#endif
		xfree(u);
	}
}

void
undo_clear(struct song *s, struct undo **pos)
{
	struct undo *u;
	struct undo_fdel_trk *p;

	while ((u = *pos) != NULL) {
		*pos = u->next;
#ifdef SONG_DEBUG
		logx(1, "%s: %s: freeed", __func__,
		    u->func ? u->func : "null");
#endif
		switch (u->type) {
		case UNDO_EMPTY:
			break;
		case UNDO_STR:
			str_delete(u->u.ren.val);
			break;
		case UNDO_UINT:
			break;
		case UNDO_TRACK:
			xfree(u->u.track.data.evs);
			break;
		case UNDO_TDEL:
			track_done(&u->u.tdel.trk->track);
			name_done(&u->u.tdel.trk->name);
			xfree(u->u.tdel.trk);
			break;
		case UNDO_TNEW:
			break;
		case UNDO_FILT:
			filt_reset(&u->u.filt.data);
			break;
		case UNDO_FDEL:
			filt_reset(&u->u.fdel.filt->filt);
			name_done(&u->u.fdel.filt->name);
			while ((p = u->u.fdel.trks) != NULL) {
				u->u.fdel.trks = p->next;
				xfree(p);
			}
			xfree(u->u.fdel.filt);
			break;
		case UNDO_FNEW:
			break;
		case UNDO_CDEL:
			track_done(&u->u.cdel.chan->conf);
			name_done(&u->u.cdel.chan->name);
			xfree(u->u.cdel.chan);
			break;
		case UNDO_CNEW:
			break;
		case UNDO_XADD:
			break;
		case UNDO_XRM:
			xfree(u->u.sysex.data.data);
			break;
		case UNDO_XDEL:
			name_done(&u->u.xdel.sx->name);
			xfree(u->u.xdel.sx);
			break;
		case UNDO_XNEW:
			break;
		case UNDO_SCALE:
			break;
		default:
			logx(1, "%s: bad type", __func__);
			panic();
		}
		s->undo_size -= u->size;
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
	logx(1, "%s: %s, size -> %d", __func__,
	    u->func ? u->func : "null", s->undo_size);
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

void
undo_start(struct song *s, char *func, char *tag)
{
	struct undo *u;

	u = undo_new(s, UNDO_EMPTY, func, tag);
	undo_push(s, u);
}

void
undo_setstr(struct song *s, char *func, char **ptr, char *val)
{
	struct undo *u;

	u = undo_new(s, UNDO_STR, func, *ptr);
	u->u.ren.ptr = ptr;
	u->u.ren.val = *ptr;
	*ptr = str_new(val);
	undo_push(s, u);
}

void
undo_setuint(struct song *s, char *func, char *tag,
	unsigned int *ptr, unsigned int val)
{
	struct undo *u;

	u = undo_new(s, UNDO_UINT, func, tag);
	u->u.uint.ptr = ptr;
	u->u.uint.val = *ptr;
	*ptr = val;
	undo_push(s, u);
}

void
undo_scale(struct song *s, char *func, char *tag,
	unsigned int oldunit, unsigned int newunit)
{
	struct undo *u;
	struct songtrk *t;

	u = undo_new(s, UNDO_SCALE, func, tag);
	u->u.scale.oldunit = oldunit;
	u->u.scale.newunit = newunit;

	track_scale(&s->meta, oldunit, newunit);
	SONG_FOREACH_TRK(s, t) {
		track_scale(&t->track, oldunit, newunit);
	}

	undo_push(s, u);
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
				logx(1, "%s: can't remove eot event", __func__);
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
				logx(1, "%s: can't insert eot event", __func__);
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
		logx(1, "%s: no data to diff", __func__);
		return;
	}
	size = track_undodiff(u->u.track.track, &u->u.track.data);
	s->undo_size += size - u->size;
	u->size = size;
}

void
undo_tdel_do(struct song *s, struct songtrk *t, char *func)
{
	struct undo *u;

	if (s->curtrk == t)
		s->curtrk = NULL;
	undo_track_save(s, &t->track, func, t->name.str);
	track_clear(&t->track);
	undo_track_diff(s);
	u = undo_new(s, UNDO_TDEL, NULL, NULL);
	u->u.tdel.trk = t;
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
undo_fdel_do(struct song *s, struct songfilt *f, char *func)
{
	struct undo *u;
	struct songtrk *t;
	struct undo_fdel_trk *p;

	u = undo_new(s, UNDO_FDEL, func, f->name.str);
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
	u->u.fdel.filt = t;
	u->u.fdel.trks = NULL;
	undo_push(s, u);
	return t;
}

struct songchan *
undo_cnew_do(struct song *s, unsigned int dev, unsigned int ch, int input,
	char *func, char *name)
{
	struct undo *u;
	struct songchan *c;

	c = song_channew(s, name, dev, ch, input);
	u = undo_new(s, UNDO_CNEW, func, c->name.str);
	u->u.cdel.chan = c;
	undo_push(s, u);
	return c;
}

void
undo_cdel_do(struct song *s, struct songchan *c, char *func)
{
	struct undo *u;

	if (c->isinput) {
		if (s->curin == c)
			s->curin = NULL;
	} else {
		if (s->curout == c)
			s->curout = NULL;
	}

	undo_track_save(s, &c->conf, func, c->name.str);
	track_clear(&c->conf);
	undo_track_diff(s);

	u = undo_new(s, UNDO_CDEL, NULL, NULL);
	u->u.cdel.chan = c;
	name_remove(&s->chanlist, &c->name);
	undo_push(s, u);
	if (c->filt)
		undo_fdel_do(s, c->filt, NULL);
}

unsigned int
sysex_undosave(struct sysex *x, struct sysex_data *data)
{
	struct chunk *ck;
	unsigned char *p;
	unsigned int i;

	data->unit = x->unit;

	data->size = 0;
	for (ck = x->first; ck != NULL; ck = ck->next)
		data->size += ck->used;

	data->data = xmalloc(data->size, "undo_sysex");

	p = data->data;
	for (ck = x->first; ck != NULL; ck = ck->next) {
		for (i = 0; i < ck->used; i++)
			*p++ = ck->data[i];
	}

	return data->size;
}

struct sysex *
sysex_undorestore(struct sysex_data *data)
{
	struct sysex *x;
	unsigned int i;

	x = sysex_new(data->unit);
	for (i = 0; i < data->size; i++)
		sysex_add(x, data->data[i]);
	xfree(data->data);
	return x;
}

void
undo_xadd_do(struct song *s, char *func, struct songsx *sx, struct sysex *x)
{
	struct undo *u;
	struct sysex *xi;
	unsigned int pos;

	pos = 0;
	for (xi = sx->sx.first; xi != NULL; xi = xi->next)
		pos++;

	u = undo_new(s, UNDO_XADD, func, sx->name.str);
	u->u.sysex.list = &sx->sx;
	u->u.sysex.data.unit = 0;
	u->u.sysex.data.data = NULL;
	u->u.sysex.data.pos = pos;
	u->size = 0;
	undo_push(s, u);

	sysexlist_put(&sx->sx, x);
}

void
undo_xrm_do(struct song *s, char *func, struct songsx *sx, unsigned int pos)
{
	struct undo *u;
	struct sysex *x;

	x = sysexlist_rm(&sx->sx, pos);

	u = undo_new(s, UNDO_XRM, func, sx->name.str);
	u->u.sysex.list = &sx->sx;
	u->u.sysex.data.pos = pos;
	u->size = sysex_undosave(x, &u->u.sysex.data);
	undo_push(s, u);

	sysex_del(x);
}

void
undo_xdel_do(struct song *s, char *func, struct songsx *sx)
{
	struct undo *u;

	if (s->cursx == sx)
		s->cursx = NULL;
	u = undo_new(s, UNDO_XDEL, func, sx->name.str);
	u->u.xdel.sx = sx;
	undo_push(s, u);
	while (sx->sx.first)
		undo_xrm_do(s, NULL, sx, 0);
	name_remove(&s->sxlist, &sx->name);
}

struct songsx *
undo_xnew_do(struct song *s, char *func, char *name)
{
	struct undo *u;
	struct songsx *sx;

	sx = song_sxnew(s, name);
	u = undo_new(s, UNDO_XNEW, func, sx->name.str);
	u->u.xdel.sx = sx;
	undo_push(s, u);
	return sx;
}
