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

#define TAG_OFF		0
#define TAG_PLAY	1
#define TAG_REC		2

unsigned song_debug = 0;
char *song_tap_modestr[3] = {"off", "start", "tempo"};

/*
 * allocate and initialize a song structure
 */
struct song *
song_new(void)
{
	struct song *o;
	o = xmalloc(sizeof(struct song), "song");
	song_init(o);
	return o;
}

/*
 * free the given song structure
 */
void
song_delete(struct song *o)
{
	song_done(o);
	xfree(o);
}

/*
 * initialize the given song
 */
void
song_init(struct song *o)
{
	struct seqev *se;

	/*
	 * song parameters
	 */
	o->mode = 0;
	o->trklist = NULL;
	o->chanlist = NULL;
	o->filtlist = NULL;
	o->sxlist = NULL;
	o->undo = NULL;
	o->undo_size = 0;
	o->tics_per_unit = DEFAULT_TPU;
	track_init(&o->meta);
	track_init(&o->clip);
	track_init(&o->rec);
	sysexlist_init(&o->recsx);

	/*
	 * runtime play record parameters
	 */
	o->tpb = o->tics_per_unit / DEFAULT_BPM;
	o->bpm = DEFAULT_BPM;
	o->tempo = TEMPO_TO_USEC24(DEFAULT_TEMPO, o->tpb);
	o->tempo_factor = 0x100;

	/*
	 * metronome
	 */
	metro_init(&o->metro);
	o->tic = o->beat = o->measure = 0;
	o->abspos = 0;

	/*
	 * defaults
	 */
	o->curtrk = NULL;
	o->curfilt = NULL;
	o->cursx = NULL;
	o->curin = NULL;
	o->curout = NULL;
	o->curpos = 0;
	o->curlen = 0;
	o->curquant = 0;
	o->loop = 0;
	evspec_reset(&o->curev);
	evspec_reset(&o->tap_evspec);
	o->tap_evspec.cmd = EVSPEC_EMPTY;
	o->tap_mode = 0;

	/*
	 * add default timesig/tempo so that setunit() works
	 */
	se = seqev_new();
	se->ev.cmd = EV_TEMPO;
	se->ev.tempo_usec24 = TEMPO_TO_USEC24(DEFAULT_TEMPO, o->tpb);
	seqev_ins(o->meta.first, se);
	se = seqev_new();
	se->ev.cmd = EV_TIMESIG;
	se->ev.timesig_beats = DEFAULT_BPM;
	se->ev.timesig_tics = o->tics_per_unit / DEFAULT_BPM;
	seqev_ins(o->meta.first, se);
}

/*
 * delete a song without freeing it
 */
void
song_done(struct song *o)
{
	if (mux_isopen) {
		song_stop(o);
	}
	undo_clear(o, &o->undo);
	while (o->trklist) {
		song_trkdel(o, (struct songtrk *)o->trklist);
	}
	while (o->chanlist) {
		song_chandel(o, (struct songchan *)o->chanlist);
	}
	while (o->filtlist) {
		song_filtdel(o, (struct songfilt *)o->filtlist);
	}
	while (o->sxlist) {
		song_sxdel(o, (struct songsx *)o->sxlist);
	}
	track_done(&o->meta);
	track_done(&o->clip);
	track_done(&o->rec);
	sysexlist_done(&o->recsx);
	metro_done(&o->metro);
	if (o->undo != NULL) {
		log_puts("undo data not freed\n");
		panic();
	}
}

/*
 * create a new track in the song
 */
struct songtrk *
song_trknew(struct song *o, char *name)
{
	struct songtrk *t;

	t = xmalloc(sizeof(struct songtrk), "songtrk");
	name_init(&t->name, name);
	track_init(&t->track);
	t->curfilt = NULL;
	t->mute = 0;

	name_add(&o->trklist, (struct name *)t);
	song_getcurfilt(o, &t->curfilt);
	song_setcurtrk(o, t);
	return t;
}

/*
 * delete the current track from the song
 */
void
song_trkdel(struct song *o, struct songtrk *t)
{
	if (o->curtrk == t) {
		o->curtrk = NULL;
	}
	name_remove(&o->trklist, (struct name *)t);
	track_done(&t->track);
	name_done(&t->name);
	xfree(t);
}

/*
 * return the track with the given name
 */
struct songtrk *
song_trklookup(struct song *o, char *name)
{
	return (struct songtrk *)name_lookup(&o->trklist, name);
}

/*
 * add a new chan to the song
 */
struct songchan *
song_channew(struct song *o, char *name, unsigned dev, unsigned ch, int input)
{
	struct songfilt *f;
	struct songchan *c, *i;
	struct evspec src, dst;

	c = xmalloc(sizeof(struct songchan), "songchan");
	name_init(&c->name, name);
	track_init(&c->conf);
	c->dev = dev;
	c->ch = ch;
	c->isinput = input;
	name_add(&o->chanlist, (struct name *)c);
	if (input)
		c->filt = NULL;
	else {
		f = song_filtlookup(o, name);
		if (f == NULL)
			f = song_filtnew(o, name);
		c->filt = f;
		evspec_reset(&src);
		evspec_reset(&dst);
		dst.dev_min = dst.dev_max = c->dev;
		dst.ch_min = dst.ch_max = c->ch;
		SONG_FOREACH_CHAN(o, i) {
			if (!i->isinput)
				continue;
			src.dev_min = src.dev_max = i->dev;
			src.ch_min = src.ch_max = i->ch;
			filt_mapnew(&c->filt->filt, &src, &dst);
		}
	}
	song_setcurchan(o, c, input);
	return c;
}

/*
 * delete the given chan from the song
 */
void
song_chandel(struct song *o, struct songchan *c)
{
	if (c->isinput) {
		if (o->curin == c)
			o->curin = NULL;
	} else {
		if (o->curout == c)
			o->curout = NULL;
	}
	name_remove(&o->chanlist, (struct name *)c);
	track_done(&c->conf);
	name_done(&c->name);
	if (c->filt != NULL)
		song_filtdel(o, c->filt);
	xfree(c);
}

/*
 * return the chan with the given name
 */
struct songchan *
song_chanlookup(struct song *o, char *name, int input)
{
	struct songchan *c;

	SONG_FOREACH_CHAN(o, c) {
		if (!!c->isinput == !!input && str_eq(c->name.str, name))
			break;
	}

	return c;
}

/*
 * return the chan matching the given (dev, ch) pair
 */
struct songchan *
song_chanlookup_bynum(struct song *o, unsigned dev, unsigned ch, int input)
{
	struct songchan *c;

	SONG_FOREACH_CHAN(o, c) {
		if (!!c->isinput == !!input && c->dev == dev && c->ch == ch)
			break;
	}

	return c;
}

/*
 * add a new filt to the song
 */
struct songfilt *
song_filtnew(struct song *o, char *name)
{
	struct songfilt *f;

	f = xmalloc(sizeof(struct songfilt), "songfilt");
	name_init(&f->name, name);
	filt_init(&f->filt);
	name_add(&o->filtlist, (struct name *)f);
	song_setcurfilt(o, f);
	return f;
}

/*
 * delete the given filter from the song
 */
void
song_filtdel(struct song *o, struct songfilt *f)
{
	struct songtrk *t;

	if (o->curfilt == f) {
		song_setcurfilt(o, NULL);
	}
	SONG_FOREACH_TRK(o, t) {
		if (t->curfilt == f) {
			t->curfilt = NULL;
		}
	}
	name_remove(&o->filtlist, (struct name *)f);
	filt_done(&f->filt);
	name_done(&f->name);
	xfree(f);
}

/*
 * return the filt conrresponding to the given name
 */
struct songfilt *
song_filtlookup(struct song *o, char *name)
{
	return (struct songfilt *)name_lookup(&o->filtlist, name);
}

/*
 * add a new sx to the song
 */
struct songsx *
song_sxnew(struct song *o, char *name)
{
	struct songsx *x;

	x = xmalloc(sizeof(struct songsx), "songsx");
	name_init(&x->name, name);
	sysexlist_init(&x->sx);
	name_add(&o->sxlist, (struct name *)x);
	song_setcursx(o, x);
	return x;
}

/*
 * delete the given sysex bank from the song
 */
void
song_sxdel(struct song *o, struct songsx *x)
{
	if (o->cursx == x) {
		o->cursx = NULL;
	}
	name_remove(&o->sxlist, (struct name *)x);
	sysexlist_done(&x->sx);
	name_done(&x->name);
	xfree(x);
}

/*
 * return the sx with the given name
 */
struct songsx *
song_sxlookup(struct song *o, char *name)
{
	return (struct songsx *)name_lookup(&o->sxlist, name);
}

/*
 * the following routines are basically getters and setters for song
 * "current" parametters. They also deal with inheritancs of these
 * values
 */
void
song_getcursx(struct song *o, struct songsx **r)
{
	*r = o->cursx;
}

void
song_setcursx(struct song *o, struct songsx *x)
{
	o->cursx = x;
}

void
song_getcurtrk(struct song *o, struct songtrk **r)
{
	*r = o->curtrk;
}

void
song_setcurtrk(struct song *o, struct songtrk *t)
{
	o->curtrk = t;
}

void
song_getcurfilt(struct song *o, struct songfilt **r)
{
	*r = o->curfilt;
}

void
song_setcurfilt(struct song *o, struct songfilt *f)
{
	if (o->curfilt == f)
		return;
	if (mux_isopen)
		norm_shut();
	o->curfilt = f;
	if (o->curout && o->curout->filt != f)
		o->curout = NULL;
	if (mux_isopen)
		mux_flush();
}

void
song_getcurchan(struct song *o, struct songchan **r, int input)
{
	*r = (input) ? o->curin : o->curout;
}

void
song_setcurchan(struct song *o, struct songchan *c, int input)
{
	struct songchan **pc = input ? &o->curin : &o->curout;

	if (*pc == c)
		return;
	*pc = c;
	if (c != NULL && !input)
		song_setcurfilt(o, c->filt);
}

unsigned
song_endpos(struct song *o)
{
	struct seqptr *mp;
	struct songtrk *t;
	unsigned m, tpm, tpb, bpm, len, maxlen, delta;

	maxlen = 0;
	SONG_FOREACH_TRK(o, t) {
		len = track_numtic(&t->track);
		if (maxlen < len)
			maxlen = len;
	}
	m = 0;
	len = 0;
	mp = seqptr_new(&o->meta);
	while (len < maxlen) {
		while (seqptr_evget(mp)) {
			/* nothing */
		}
		seqptr_getsign(mp, &bpm, &tpb);
		tpm = bpm * tpb;
		delta = seqptr_skip(mp, tpm);
		if (delta > 0) {
			m += (maxlen - len + tpm - 1) / tpm;
			break;
		}
		len += tpm;
		m++;
	}
	seqptr_del(mp);
	return m;
}

void
song_playconfev(struct song *o, struct songchan *c, struct ev *in)
{
	struct ev ev = *in;

	if (!EV_ISVOICE(&ev)) {
		log_puts("song_playconfev: ");
		log_puts(c->name.str);
		log_puts(": ");
		ev_log(&ev);
		log_puts(": not a voice event");
		log_puts("\n");
		return;
	}
	ev.dev = c->dev;
	ev.ch = c->ch;
	if (!c->isinput) {
		mixout_putev(&ev, PRIO_CHAN);
	} else {
		norm_putev(&ev);
	}
}

/*
 * send to the output all events from all chans
 */
void
song_playconf(struct song *o)
{
	struct songchan *i;
	struct seqptr *cp;
	struct state *st;

	SONG_FOREACH_CHAN(o, i) {
		cp = seqptr_new(&i->conf);
		for (;;) {
			st = seqptr_evget(cp);
			if (st == NULL)
				break;
			song_playconfev(o, i, &st->ev);
		}
		seqptr_del(cp);
	}
	mux_flush();
	mux_sleep(DEFAULT_CHANWAIT);
}

/*
 * send all sysex messages
 */
void
song_playsysex(struct song *o)
{
	struct songsx *l;
	struct sysex *s;
	struct chunk *c;

	SONG_FOREACH_SX(o, l) {
		for (s = l->sx.first; s != NULL; s = s->next) {
			for (c = s->first; c != NULL; c = c->next) {
				mux_sendraw(s->unit, c->data, c->used);
				mux_flush();
			}
			mux_sleep(DEFAULT_SXWAIT);
		}
	}
}

/*
 * play a meta event
 */
void
song_metaput(struct song *o, struct state *s)
{
	switch(s->ev.cmd) {
	case EV_TIMESIG:
		if ((s->flags & STATE_CHANGED) && (o->beat != 0 || o->tic != 0)) {
			/*
			 * found an incomplete measure,
			 * skip to the beggining of the next one
			 */
			o->beat = 0;
			o->tic = 0;
			o->measure++;
		}
		o->bpm = s->ev.timesig_beats;
		o->tpb = s->ev.timesig_tics;
		break;
	case EV_TEMPO:
		o->tempo = s->ev.tempo_usec24;
		if (mux_isopen)
			mux_chgtempo(o->tempo_factor * o->tempo  / 0x100);
		break;
	default:
		break;
	}
}

/*
 * save the state at the given start position, so that we can repeat
 * playback from there.
 */
void
song_loop_init(struct song *o)
{
	struct state *s, *snext;
	struct statelist *slist;
	struct songtrk *t;
	unsigned qstep;

	if (o->loop) {
		o->loop_mstart = o->curpos;
		o->loop_mend = o->curpos + o->curlen;
	} else
		o->loop_mstart = o->loop_mend = 0;

	if (o->loop_mstart == o->loop_mend)
		return;

	o->loop_tstart = track_findmeasure(&o->meta, o->loop_mstart);
	o->loop_tend = track_findmeasure(&o->meta, o->loop_mend);

	qstep = o->curquant / 2;
	if (o->loop_tstart > qstep) {
		o->loop_tstart -= qstep;
		o->loop_tend -= qstep;
	}

	o->loop_metaptr = seqptr_new(&o->meta);
	seqptr_skip(o->loop_metaptr, o->loop_tstart);

	SONG_FOREACH_TRK(o, t) {
		t->loop_trackptr = seqptr_new(&t->track);
		seqptr_skip(t->loop_trackptr, o->loop_tstart);

		/*
		 * Drop notes, as we don't restore them
		 */
		slist = &t->loop_trackptr->statelist;
		for (s = slist->first; s != NULL; s = snext) {
			snext = s->next;
			if (EV_ISNOTE(&s->ev)) {
				statelist_rm(slist, s);
				state_del(s);
			}
		}

		/*
		 * Drop terminated states
		 */
		statelist_outdate(slist);
	}
}

/*
 * free loop state
 */
void
song_loop_done(struct song *o)
{
	struct songtrk *t;

	if (o->loop_mstart == o->loop_mend)
		return;

	seqptr_del(o->loop_metaptr);
	SONG_FOREACH_TRK(o, t) {
		statelist_empty(&t->loop_trackptr->statelist);
		seqptr_del(t->loop_trackptr);
	}
}

/*
 * restore the given track (or meta-track if NULL) from its loop
 * state.
 */
void
song_loop_track(struct song *o, struct songtrk *t)
{
	struct seqptr *sp, *lp;
	struct statelist *dlist, *slist;
	struct state *s, *d, *dnext;
	struct ev re;

	if (t) {
		sp = t->trackptr;
		lp = t->loop_trackptr;
	} else {
		sp = o->metaptr;
		lp = o->loop_metaptr;
	}

	dlist = &sp->statelist;
	slist = &lp->statelist;

	/*
	 * cancel states not present in loop state (this will cancel
	 * all notes as their state is not saved)
	 */
	for (d = dlist->first; d != NULL; d = dnext) {
		dnext = d->next;
		s = statelist_lookup(slist, &d->ev);
		if (s != NULL)
			continue;
		if (!state_cancel(d, &re))
			continue;
		statelist_update(dlist, &re);
		if (d->tag) {
			if (t != NULL)
				mixout_putev(&d->ev, PRIO_TRACK);
			else
				song_metaput(o, d);
		}
	}

	/*
	 * restore states from loop start
	 */
	for (s = slist->first; s != NULL; s = s->next) {
		d = statelist_lookup(dlist, &s->ev);
		if (d != NULL && state_eq(d, &s->ev))
			continue;
		if (!state_restore(s, &re))
			continue;
		d = statelist_update(dlist, &re);
		if (d->phase & EV_PHASE_FIRST) {
			d->tag = (t != NULL) ?
			    !t->mute : EV_ISMETA(&d->ev);
		}
		if (d->tag) {
			if (t != NULL)
				mixout_putev(&d->ev, PRIO_TRACK);
			else
				song_metaput(o, d);
		}
	}

	sp->pos = lp->pos;
	sp->delta = lp->delta;
	sp->tic = lp->tic;
}

void
song_loop_rec(struct song *o)
{
	if (o->playptr != NULL)
		return;

	o->playptr = seqptr_new(&o->rec);
	seqptr_link(o->playptr, o->recptr);

	if (seqptr_ticdel(o->playptr, o->loop_tstart,
		&o->rec_replay) != o->loop_tstart) {
		log_puts("song_loop_rec: events in the way\n");
		panic();
	}
	seqptr_ticput(o->playptr, o->loop_tstart);

	if (song_debug)
		log_puts("song_loop_rec: starting replay\n");
}

/*
 * continue playback from the loop start position
 */
int
song_loop_repeat(struct song *o)
{
	struct songtrk *t;

	if (o->loop_mstart == o->loop_mend || o->abspos != o->loop_tend)
		return 0;

	o->abspos = o->loop_tstart;
	o->measure -= o->loop_mend - o->loop_mstart;

	SONG_FOREACH_TRK(o, t) {
		song_loop_track(o, t);
	}

	song_loop_track(o, NULL);

	if (o->mode >= SONG_REC)
		song_loop_rec(o);

	return 1;
}

/*
 * move all track pointers 1 tick forward. Return true if at least one
 * track moved forward and 0 if no track moved forward (ie end of the
 * song was track reached)
 *
 * Note that must be no events available on any track, in other words,
 * this routine must be called after song_ticplay()
 */
void
song_ticskip(struct song *o)
{
	struct ev ev;
	struct songtrk *i;
	struct state *s;
	unsigned neot;
	unsigned period;

	/*
	 * tempo_track
	 */
	neot = seqptr_ticskip(o->metaptr, 1);
	o->tic++;
	if (o->tic >= o->tpb) {
		o->tic = 0;
		o->beat++;
		if (o->beat >= o->bpm) {
			o->beat = 0;
			o->measure++;
		}
	}
	o->abspos++;
	SONG_FOREACH_TRK(o, i) {
		neot |= seqptr_ticskip(i->trackptr, 1);
	}
	if (o->mode >= SONG_REC) {
		if (o->playptr) {
			seqptr_ticdel(o->playptr, 1, &o->rec_replay);
			seqptr_ticput(o->playptr, 1);
		}
		seqptr_ticput(o->recptr, 1);
		statelist_outdate(&o->rec_input);

		/*
		 * increment length and terminate notes longer than
		 * the loop period
		 */
		period = o->loop_tend - o->loop_tstart;
		for (s = o->rec_input.first; s != NULL; s = s->next) {
			if (s->tag != TAG_REC)
				continue;
			if (++s->tic != period)
				continue;
			if (state_cancel(s, &ev)) {
				seqptr_evmerge2(o->recptr,
				    &o->rec_replay, &ev, NULL);
				mixout_putev(&ev, 0);
			}
			s->tag = TAG_OFF;
		}
	}
	if (song_loop_repeat(o))
		return;
	if (neot == 0 && !o->complete) {
		cons_puttag("complete");
		o->complete = 1;
	}
}

/*
 * play data corresponding to the current tick used by playcb and
 * recordcb.
 */
void
song_ticplay(struct song *o)
{
	struct songtrk *i;
	struct state *st, *sr;
	unsigned id;

	while ((st = seqptr_evget(o->metaptr)))
		song_metaput(o, st);

	if (o->tic == 0) {
		cons_putpos(o->measure, o->beat, o->tic);
	}
	metro_tic(&o->metro, o->beat, o->tic);
	SONG_FOREACH_TRK(o, i) {
		id = i->trackptr->statelist.serial;
		while ((st = seqptr_evget(i->trackptr))) {
			if (st->phase & EV_PHASE_FIRST)
				st->tag = i->mute ? 0 : 1;
			if (st->tag)
				mixout_putev(&st->ev, PRIO_TRACK);
		}
	}

	if (o->mode >= SONG_REC) {
		/*
		 * remove all events from rec track and put them back
		 * on it by merging them with the input states. The
		 * played states table is updated so it always contain
		 * the exact state of the original track.
		 */
		if (o->playptr != NULL) {
			for(;;) {
				st = seqptr_evdel(o->playptr, &o->rec_replay);
				if (st == NULL)
					break;
				st->tag = 1;
				sr = seqptr_evmerge1(o->recptr, st);
				if (sr != NULL)
					mixout_putev(&st->ev, 0);
			}
		}

	}
}

/*
 * restore all frames in the given statelist. First, restore events
 * that have context, then the rest.
 */
void
song_confrestore(struct statelist *slist, int all, unsigned prio)
{
	struct state *s;
	struct ev re;

	for (s = slist->first; s != NULL; s = s->next) {
		if (EV_ISNOTE(&s->ev))
			continue;
		if (s->tag) {
			if (song_debug) {
				log_puts("song_strestore: ");
				ev_log(&s->ev);
				log_puts(": not restored (tagged)\n");
			}
			continue;
		}
		if (!(s->phase & EV_PHASE_LAST) && !all) {
			if (song_debug) {
				log_puts("song_strestore: ");
				ev_log(&s->ev);
				log_puts(": not restored (unterminated)\n");
			}
			continue;
		}
		if (state_restore(s, &re)) {
			if (song_debug) {
				log_puts("song_strestore: ");
				ev_log(&s->ev);
				log_puts(": restored -> ");
				ev_log(&re);
				log_puts("\n");
			}
			mixout_putev(&re, prio);
		}
		s->tag = 1;
	}
}

/*
 * cancel all frames in the given state list
 */
void
song_confcancel(struct statelist *slist, unsigned prio)
{
	struct state *s;
	struct ev ca;

	for (s = slist->first; s != NULL; s = s->next) {
		if (s->tag) {
			if (state_cancel(s, &ca)) {
				if (song_debug) {
					log_puts("song_stcancel: ");
					ev_log(&s->ev);
					log_puts(": canceled -> ");
					ev_log(&ca);
					log_puts("\n");
				}
				mixout_putev(&ca, prio);
			}
			s->tag = 0;
		} else {
			if (song_debug) {
				log_puts("song_stcancel: ");
				ev_log(&s->ev);
				log_puts(": not canceled (no tag)\n");
			}
		}
	}
}

/*
 * mute the given track by canceling all states
 */
void
song_trkmute(struct song *s, struct songtrk *t)
{
	if (s->mode >= SONG_PLAY)
		song_confcancel(&t->trackptr->statelist, PRIO_TRACK);
	t->mute = 1;
}

/*
 * unmute the given track by restoring all states
 */
void
song_trkunmute(struct song *s, struct songtrk *t)
{
	if (s->mode >= SONG_PLAY)
		song_confrestore(&t->trackptr->statelist, 1, PRIO_TRACK);
	t->mute = 0;
}

/*
 * merge recorded track into current track
 */
void
song_mergerec(struct song *o)
{
	struct state *st;
	struct track loop;
	struct seqptr *lp;
	struct songtrk *t;
	struct songsx *x;
	struct sysex *e;
	struct state *s;
	struct ev ev;
	unsigned period, offset;

	/*
	 * if there is no filter for recording there may be
	 * unterminated frames, so finalize them.
	 */
	for (s = o->rec_input.first; s != NULL; s = s->next) {
		if (s->tag != TAG_REC && s->tag != TAG_PLAY)
			continue;
		if (state_cancel(s, &ev)) {
			if (s->tag == TAG_REC) {
				seqptr_evmerge2(o->recptr,
				    &o->rec_replay, &ev, NULL);
			}
			mixout_putev(&ev, 0);
		}
		s->tag = TAG_OFF;
	}

	/*
	 * cancel any remaining frames (replayed events in loop mode)
	 */
	for (s = o->recptr->statelist.first; s != NULL; s = s->next) {
		if (!state_cancel(s, &ev))
			continue;
		mixout_putev(&ev, 0);
	}

	if (o->playptr != NULL) {
		period = o->loop_tend - o->loop_tstart;
		offset = (o->playptr->tic - o->loop_tstart) % period;

		/*
		 * advance until loop end
		 */
		while (offset++ != period) {
			seqptr_ticdel(o->playptr, 1, &o->rec_replay);
			seqptr_ticput(o->playptr, 1);
			seqptr_ticput(o->recptr, 1);
			for(;;) {
				st = seqptr_evdel(o->playptr, &o->rec_replay);
				if (st == NULL)
					break;
				st->tag = 1;
				seqptr_evmerge1(o->recptr, st);
			}
		}

		track_init(&loop);
		lp = seqptr_new(&loop);
		seqptr_ticput(lp, o->loop_tstart);

		/*
		 * unroll loop into a new 'loop' track
		 */
		while (o->rec.first->ev.cmd != EV_NULL) {
			seqptr_ticdel(o->playptr, 1, &o->rec_replay);
			seqptr_ticput(o->playptr, 1);
			seqptr_ticput(o->recptr, 1);
			seqptr_ticput(lp, 1);
			for(;;) {
				st = seqptr_evdel(o->playptr, &o->rec_replay);
				if (st == NULL)
					break;
				if (st->phase & EV_PHASE_FIRST)
					st->tag = 0;
				if (st->tag)
					seqptr_evmerge1(o->recptr, st);
				else
					seqptr_evput(lp, &st->ev);
			}
		}

		seqptr_del(lp);
		track_swap(&o->rec, &loop);
		track_done(&loop);
	}

	song_getcurtrk(o, &t);
	if (t) {
		undo_track_save(o, &t->track, "record", t->name.str);
		track_merge(&o->curtrk->track, &o->rec);
		undo_track_diff(o);
	}
	track_clear(&o->rec);

	song_getcursx(o, &x);
	if (x) {
		for (;;) {
			e = sysexlist_get(&o->recsx);
			if (e == NULL)
				break;
			sysexlist_put(&x->sx, e);
		}
	}
	sysexlist_clear(&o->recsx);
}

/*
 * call-back called when the first midi tick arrives
 */
void
song_startcb(struct song *o)
{

	if (song_debug) {
		log_puts("song_startcb:\n");
	}
	if (o->mode >= SONG_PLAY) {
		song_ticplay(o);
		mux_flush();
	}
	o->started = 1;
}

/*
 * call-back called when the midi clock stopped
 */
void
song_stopcb(struct song *o)
{
	struct songtrk *t;

	if (song_debug)
		log_puts("song_stopcb:\n");

	/*
	 * stop all sounding notes
	 */
	SONG_FOREACH_TRK(o, t) {
		song_confcancel(&t->trackptr->statelist, PRIO_TRACK);
	}
}

/*
 * call-back, called when the midi clock moved one tick forward
 */
void
song_movecb(struct song *o)
{
	if (o->mode >= SONG_PLAY) {
		(void)song_ticskip(o);
		song_ticplay(o);
	}
	mux_flush();
}

/*
 * call-back called when a midi event arrives
 */
void
song_evcb(struct song *o, struct ev *ev)
{
	struct ev filtout[FILT_MAXNRULES], rev;
	struct state *s;
	unsigned i, nev;
	unsigned usec24;

	if (o->tap_mode != SONG_TAP_OFF &&
	    evspec_matchev(&o->tap_evspec, ev)) {
		if (!(ev_phase(ev) & EV_PHASE_FIRST))
			return;
		if (o->started)
			return;
		if (o->tap_cnt == 0) {
			if (o->tap_mode == SONG_TAP_START) {
				log_puts("start triggered\n");
				o->tap_cnt = -1;
				mux_ticcb();
			} else {
				log_puts("measuring tempo...\n");
				o->tap_time = mux_wallclock;
			}
		} else if (o->tap_mode == SONG_TAP_TEMPO && o->tap_cnt == 1) {
			usec24 = (long)(mux_wallclock - o->tap_time) / o->tpb;
			log_puts("start triggered, tempo = ");
			log_putu(60 * 24000000 / o->tpb / usec24);
			log_puts("\n");
			if (usec24 < TEMPO_MIN || usec24 > TEMPO_MAX) {
				log_puts("tempo out of range, aborted\n");
				o->tap_cnt = 0;
				return;
			}
			o->tempo = usec24;
			mux_chgtempo(o->tempo);
			mux_ticcb();
			o->tap_cnt = -1;
		}
		o->tap_cnt++;
		return;
	}

	/*
	 * apply filter, if any
	 */
	if (o->curfilt) {
		nev = filt_do(&o->curfilt->filt, ev, filtout);
	} else {
		filtout[0] = *ev;
		nev = 1;
	}

	/*
	 * output and/or record resulting events
	 */
	ev = filtout;
	for (i = 0; i < nev; i++) {
		if (o->mode >= SONG_REC) {
			s = statelist_update(&o->rec_input, ev);
			if (s->phase & EV_PHASE_FIRST) {
				s->tic = 0;
				if (s->flags & (STATE_BOGUS | STATE_NESTED)) {
					state_log(s);
					log_puts(": norm filter failed\n");
					panic();
				}
				if ((mux_getphase() >= MUX_START) &&
				    (o->loop_mstart == o->loop_mend ||
					o->abspos >= o->loop_tstart))
					s->tag = TAG_REC;
				else
					s->tag = TAG_PLAY;
			}

			if (s->tag == TAG_REC) {
				if (seqptr_evmerge2(o->recptr,
					&o->rec_replay, ev, &rev))
					mixout_putev(&rev, 0);
			}
			if (s->tag == TAG_REC || s->tag == TAG_PLAY)
				mixout_putev(ev, 0);

		} else
			mixout_putev(ev, 0);
		ev++;
	}
}

/*
 * call-back called when a sysex message arrives
 */
void
song_sysexcb(struct song *o, struct sysex *sx)
{
	if (song_debug)
		log_puts("song_sysexcb:\n");
	if (sx == NULL) {
		log_puts("got null sx\n");
		return;
	}
	if (o->mode >= SONG_REC)
		sysexlist_put(&o->recsx, sx);
	else
		sysex_del(sx);
}

/*
 * cancel the current state, and restore the state of
 * the new postition, must be in idle mode
 *
 * return the absolute position as an ``MTC position'',
 * suitable for a MMC relocate message.
 */
unsigned
song_loc(struct song *o, unsigned where, unsigned how)
{
	struct state *s;
	struct songtrk *t;
	unsigned maxdelta, delta;
	unsigned bpm, tpb, offs;
	unsigned long long pos, endpos;
	unsigned long usec24;

	seqptr_del(o->metaptr);
	o->metaptr = seqptr_new(&o->meta);

	/* please gcc */
	endpos = 0xdeadbeef;
	offs = 0xdeadbeef;
	maxdelta = 0xdeadbeef;

	/*
	 * XXX: when not in LOC_MEAS and LOC_SPP modes, the MTC position
	 * can overflow. Limit song_loc() to 24 hours
	 */

	switch (how) {
	case SONG_LOC_MEAS:
		offs = (!mux_manualstart && o->mode >= SONG_PLAY) ?
		    o->curquant / 2 : 0;
		break;
	case SONG_LOC_MTC:
		endpos = (unsigned long long)where * (24000000 / MTC_SEC);
		offs = 0;
		break;
	case SONG_LOC_SPP:
		where *= o->tics_per_unit / 16;
		offs = 0;
		break;
	default:
		log_puts("song_loc: bad argument\n");
		panic();
	}
	pos = 0;
	o->abspos = 0;
	o->measure = o->beat = o->tic = 0;

	for (;;) {
		seqptr_getsign(o->metaptr, &bpm, &tpb);
		seqptr_gettempo(o->metaptr, &usec24);
		switch (how) {
		case SONG_LOC_MEAS:
			maxdelta = (where - o->measure) * bpm * tpb
			    - o->beat * tpb
			    - o->tic;
			break;
		case SONG_LOC_MTC:
			maxdelta = (endpos - pos) / usec24;
			break;
		case SONG_LOC_SPP:
			maxdelta = where - o->abspos;
			break;
		}
		if (maxdelta <= offs)
			break;
		maxdelta -= offs;
		delta = seqptr_ticskip(o->metaptr, maxdelta);
		s = seqptr_evget(o->metaptr);
		if (s == NULL) {
			/* XXX: integrate this in ticskip() & co */
			statelist_outdate(&o->metaptr->statelist);
			delta = maxdelta;
		}
		o->tic += delta;
		o->beat += o->tic / tpb;
		o->tic = o->tic % tpb;
		o->measure += o->beat / bpm;
		o->beat = o->beat % bpm;
		o->abspos += delta;
		pos += (unsigned long long)delta * usec24;
	}

	/*
	 * process all meta events of the current tick,
	 * so we get the time signature and the tempo of
	 * the next tick
	 */
	while (seqptr_evget(o->metaptr))
		; /* nothing */

	o->complete = !seqptr_eot(o->metaptr);

	/*
	 * move all tracks to the current position
	 */
	SONG_FOREACH_TRK(o, t) {
		/*
		 * cancel and free old states
		 */
		song_confcancel(&t->trackptr->statelist, PRIO_TRACK);
		statelist_empty(&t->trackptr->statelist);
		seqptr_del(t->trackptr);

		/*
		 * allocate and restore new states
		 */
		t->trackptr = seqptr_new(&t->track);
		seqptr_skip(t->trackptr, o->abspos);
		for (s = t->trackptr->statelist.first; s != NULL; s = s->next)
			s->tag = 0;
		song_confrestore(&t->trackptr->statelist,
		    o->mode >= SONG_PLAY, PRIO_TRACK);

		/*
		 * check if we reached the end-of-track
		 */
		if (!seqptr_eot(t->trackptr))
			o->complete = 0;
	}

	if (o->mode >= SONG_REC)
		track_clear(&o->rec);
#ifdef SONG_DEBUG
	if (!track_isempty(&o->rec)) {
		log_puts("song_loc: rec track not empty\n");
		panic();
	}
#endif
	/*
	 * at this point track is cleared, song->rec_xxx lists are
	 * empty
	 */
	if (o->playptr)
		seqptr_del(o->playptr);
	seqptr_del(o->recptr);
	o->recptr = seqptr_new(&o->rec);
	o->playptr = NULL;
	if (o->mode >= SONG_REC) {
		seqptr_seek(o->recptr, o->abspos);
	}
	statelist_empty(&o->rec_input);
	statelist_empty(&o->rec_replay);

	for (s = o->metaptr->statelist.first; s != NULL; s = s->next) {
		if (EV_ISMETA(&s->ev)) {
			if (song_debug) {
				log_puts("song_loc: ");
				ev_log(&s->ev);
				log_puts(": restoring meta-event\n");
			}
			song_metaput(o, s);
			s->tag = 1;
		} else {
			if (song_debug) {
				log_puts("song_loc: ");
				ev_log(&s->ev);
				log_puts(": not restored (not tagged)\n");
			}
			s->tag = 0;
		}
	}

	if (o->complete)
		cons_puttag("complete");

	switch (how) {
	case SONG_LOC_MEAS:
	case SONG_LOC_SPP:
		pos /= 24000000 / MTC_SEC;
		if (song_debug) {
			log_puts("song_loc: measure/spp ");
			log_putu(where);
			log_puts(" -> ");
			log_putu(o->measure);
			log_puts(":");
			log_putu(o->beat);
			log_puts(":");
			log_putu(o->tic);
			log_puts("/");
			log_putu(o->abspos);
			log_puts(", mtc = ");
			log_putu(pos);
			log_puts("\n");
		}
		break;
	case SONG_LOC_MTC:
		pos = endpos - pos;
		if (song_debug) {
			log_puts("song_loc: mtc ");
			log_putu(where);
			log_puts(" -> +");
			log_putu(o->measure);
			log_puts(":");
			log_putu(o->beat);
			log_puts(":");
			log_putu(o->tic);
			log_puts("/");
			log_putu(o->abspos);
			log_puts(" +");
			log_putu(pos);
			log_puts("/");
			log_putu(usec24);
			log_puts("\n");
		}
		break;
	default:
		log_puts("song_loc: bad argument\n");
		panic();
	}

	/*
	 * we've the tempo to be set, as in the LOC_MTC case, the
	 * return value is a franction of the tick length. The tempo
	 * is set, either by mux_open() or by above song_metaput()
	 * calls.
	 */
	return pos;
}

/*
 * relocate requested from a device. Move the song to the
 * tick just before the given MTC position, and return
 * the time (24-th of us) between the requested position
 * and the current tick. This way the mux module will "skip"
 * this duration to ensure we're perfectly in sync.
 */
unsigned
song_gotocb(struct song *o, unsigned mtcpos)
{
	unsigned nextpos;

	nextpos = song_loc(o, mtcpos, SONG_LOC_MTC);
	cons_putpos(o->measure, o->beat, o->tic);

	return nextpos;
}

/*
 * set the current mode
 */
void
song_setmode(struct song *o, unsigned newmode)
{
	struct songtrk *t;
	unsigned oldmode;

	oldmode = o->mode;
	o->mode = newmode;
	if (oldmode >= SONG_PLAY) {
		mux_stopreq();
	}
	if (newmode < oldmode)
		metro_setmode(&o->metro, newmode);
	if (oldmode >= SONG_REC && newmode < SONG_REC)
		song_mergerec(o);
	if (oldmode >= SONG_PLAY && newmode < SONG_PLAY)
		song_loop_done(o);
	if (oldmode >= SONG_IDLE && newmode < SONG_IDLE) {
		/*
		 * cancel and free states
		 */
		SONG_FOREACH_TRK(o, t) {
			song_confcancel(&t->trackptr->statelist, PRIO_TRACK);
			statelist_empty(&t->trackptr->statelist);
			seqptr_del(t->trackptr);
		}
		if (o->playptr)
			seqptr_del(o->playptr);
		statelist_empty(&o->rec_input);
		statelist_done(&o->rec_input);
		statelist_empty(&o->rec_replay);
		statelist_done(&o->rec_replay);
		seqptr_del(o->recptr);
		seqptr_del(o->metaptr);
		norm_shut();
		mux_flush();
		mux_close();
	}
	if (oldmode < SONG_PLAY && newmode >= SONG_PLAY) {
		o->tap_cnt = 0;
		o->complete = 0;
		song_loop_init(o);
	}
	if (oldmode < SONG_IDLE && newmode >= SONG_IDLE) {
		o->abspos = 0;
		o->measure = 0;
		o->beat = 0;
		o->tic = 0;

		/*
		 * get empty states
		 */
		SONG_FOREACH_TRK(o, t) {
			t->trackptr = seqptr_new(&t->track);
		}
		o->metaptr = seqptr_new(&o->meta);
		o->recptr = seqptr_new(&o->rec);
		o->playptr = NULL;
		statelist_init(&o->rec_replay);
		statelist_init(&o->rec_input);

		mux_open();

		/*
		 * send sysex messages and channel config messages
		 */
		song_playsysex(o);
		song_playconf(o);
		mux_flush();
	}
	if (newmode > oldmode)
		metro_setmode(&o->metro, newmode);
}

/*
 * cancel the current state, and restore the state of
 * the given position
 */
void
song_goto(struct song *o, unsigned measure)
{
	unsigned mmcpos;

	if (o->mode >= SONG_IDLE) {
		/*
		 * 1 measure of count-down for recording
		 */
		if (o->mode >= SONG_REC && measure > 0)
			measure--;
		o->started = 0;

		/*
		 * move all tracks to given measure
		 */
		mmcpos = song_loc(o, measure, SONG_LOC_MEAS);
		mux_gotoreq(mmcpos);
		mux_flush();

		/*
		 * display initial position
		 */
		cons_putpos(o->measure, o->beat, o->tic);
	} else
		cons_putpos(measure, 0, 0);
}

/*
 * stop play/record: undo song_start and things done during the
 * play/record process. Must be called with the mux initialised
 */
void
song_stop(struct song *o)
{
	song_setmode(o, 0);
	cons_putpos(o->curpos, 0, 0);
}

/*
 * play the song initialize the midi/timer and start the event loop
 */
void
song_play(struct song *o)
{
	unsigned m;

	m = (o->mode >= SONG_IDLE) ? o->measure : o->curpos;
	song_setmode(o, SONG_PLAY);
	song_goto(o, m);
	mux_startreq(o->tap_mode != SONG_TAP_OFF);
	mux_flush();

	if (song_debug) {
		log_puts("song_play: waiting for a start event...\n");
	}
}

/*
 * record the current track: initialise the midi/timer and start the
 * event loop
 */
void
song_record(struct song *o)
{
	struct songtrk *t;
	unsigned m;

	song_getcurtrk(o, &t);
	if (!t || t->mute) {
		log_puts("song_record: no current track (or muted)\n");
	}

	m = (o->mode >= SONG_IDLE) ? o->measure : o->curpos;
	song_setmode(o, SONG_REC);
	song_goto(o, m);
	mux_startreq(o->tap_mode != SONG_TAP_OFF);
	mux_flush();
	if (song_debug) {
		log_puts("song_record: waiting for a start event...\n");
	}
}

/*
 * move input events directly to the output
 */
void
song_idle(struct song *o)
{
	unsigned m;

	m = (o->mode >= SONG_IDLE) ? o->measure : o->curpos;
	song_setmode(o, SONG_IDLE);
	song_goto(o,  m);
	mux_flush();

	if (song_debug) {
		log_puts("song_idle: started loop...\n");
	}
}


/*
 * the song_try_xxx() routines return 1 if we can have exclusive write
 * access to the corresponding object, 0 otherwise.
 *
 * builting functions that cannot handle properly concurent access to
 * the object, must call the corresponding song_try_xxx(), and fail if
 * they cannot get exclusive access to it.
 */

unsigned
song_try_mode(struct song *o, unsigned reqmode)
{
	if (o->mode > reqmode) {
		cons_err("song in use, use ``s'' to release it");
		return 0;
	}
	return 1;
}

unsigned
song_try_curev(struct song *o)
{
	return 1;
}

unsigned
song_try_curpos(struct song *o)
{
	return 1;
}

unsigned
song_try_curlen(struct song *o)
{
	return 1;
}

unsigned
song_try_curquant(struct song *o)
{
	return 1;
}

unsigned
song_try_curtrk(struct song *o)
{
	return 1;
}

unsigned
song_try_curchan(struct song *o, int input)
{
	return 1;
}

unsigned
song_try_curfilt(struct song *o)
{
	return song_try_mode(o, 0);
}

unsigned
song_try_cursx(struct song *o)
{
	return song_try_mode(o, 0);
}

unsigned
song_try_trk(struct song *o, struct songtrk *f)
{
	if (o->mode >= SONG_PLAY) {
		cons_err("track in use, use ``s'' or ``i'' to release it");
		return 0;
	}
	return 1;
}

unsigned
song_try_chan(struct song *o, struct songchan *f, int input)
{
	return song_try_mode(o, 0);
}

unsigned
song_try_filt(struct song *o, struct songfilt *f)
{
	return song_try_mode(o, 0);
}

unsigned
song_try_sx(struct song *o, struct songsx *x)
{
	if (o->mode >= SONG_REC && (o->cursx == x)) {
		cons_err("sysex in use, use ``s'' or ``i'' to stop recording");
		return 0;
	}
	return 1;
}

unsigned
song_try_meta(struct song *o)
{
	if (o->mode >= SONG_PLAY) {
		cons_err("meta track in use, use ``s'' or ``i'' to release it");
		return 0;
	}
	return 1;
}

unsigned
song_try_ev(struct song *o, unsigned cmd)
{
	struct songfilt *f;
	struct songchan *c;
	struct songtrk *t;

	SONG_FOREACH_TRK(o, t) {
		if (track_evcnt(&t->track, cmd)) {
			cons_errs(t->name.str, "event in use by track");
			return 0;
		}
	}
	SONG_FOREACH_CHAN(o, c) {
		if (track_evcnt(&c->conf, cmd)) {
			cons_errs(c->name.str, "event in use by input");
			return 0;
		}
	}
	SONG_FOREACH_FILT(o, f) {
		if (filt_evcnt(&f->filt, cmd)) {
			cons_errs(f->name.str, "event in use by filter");
			return 0;
		}
	}
	return 1;
}

void
song_confev(struct song *o, struct songchan *c, struct ev *ev)
{
	track_confev(&c->conf, ev);
	if (mux_isopen) {
		song_playconfev(o, c, ev);
		mux_flush();
	}
}

void
song_unconfev(struct song *o, struct songchan *c, struct evspec *es)
{
	track_unconfev(&c->conf, es);
}

