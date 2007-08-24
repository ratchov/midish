/*
 * Copyright (c) 2003-2007 Alexandre Ratchov <alex@caoua.org>
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

#include "dbg.h"
#include "mididev.h"
#include "mux.h"
#include "track.h"
#include "frame.h"
#include "filt.h"
#include "song.h"
#include "cons.h"		/* cons_errxxx */
#include "metro.h"
#include "default.h"

unsigned song_debug = 1;

/*
 * allocate and initialize a song structure
 */
struct song *
song_new(void) {
	struct song *o;
	o = (struct song *)mem_alloc(sizeof(struct song));
	song_init(o);
	return o;
}

/*
 * free the given song structure
 */
void
song_delete(struct song *o) {
	song_done(o);
	mem_free(o);
}

/*
 * initialize the given song
 */
void
song_init(struct song *o) {
	struct seqev *se;

	/* 
	 * song parameters 
	 */
	o->trklist = NULL;
	o->chanlist = NULL;
	o->filtlist = NULL;
	o->sxlist = NULL;
	o->tics_per_unit = DEFAULT_TPU;
	track_init(&o->meta);
	track_init(&o->rec);

	/* 
	 * runtime play record parameters 
	 */
	o->filt = NULL;
	o->tpb = o->tics_per_unit / DEFAULT_BPM;
	o->bpm = DEFAULT_BPM;
	o->tempo = TEMPO_TO_USEC24(DEFAULT_TEMPO, o->tpb);
	o->tempo_factor = 0x100;

	/* 
	 * metronome 
	 */
	metro_init(&o->metro);
	o->tic = o->beat = o->measure = 0;
	
	/* 
	 * defaults 
	 */
	o->curtrk = NULL;
	o->curfilt = NULL;
	o->cursx = NULL;
	o->curchan = NULL;
	o->curpos = 0;
	o->curlen = 0;
	o->curquant = 0;
	o->curinput_dev = 0;
	o->curinput_ch = 0;

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
song_done(struct song *o) {
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
	track_done(&o->rec);
	metro_done(&o->metro);
}

/*
 * create a new track in the song
 */
struct songtrk *
song_trknew(struct song *o, char *name) {
	struct songtrk *t;

	t = (struct songtrk *)mem_alloc(sizeof(struct songtrk));
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
song_trkdel(struct song *o, struct songtrk *t) {
	if (o->curtrk == t) {
		o->curtrk = NULL;
	}
	name_remove(&o->trklist, (struct name *)t);
	track_done(&t->track);
	name_done(&t->name);
	mem_free(t);
}

/*
 * return the track with the given name
 */
struct songtrk *
song_trklookup(struct song *o, char *name) {
	return (struct songtrk *)name_lookup(&o->trklist, name);
}

/*
 * add a new chan to the song
 */
struct songchan *
song_channew(struct song *o, char *name, unsigned dev, unsigned ch) {
	struct songchan *c;

	c = (struct songchan *)mem_alloc(sizeof(struct songchan));
	name_init(&c->name, name);
	track_init(&c->conf);
	c->dev = c->curinput_dev = dev;
	c->ch = c->curinput_ch = ch;

	name_add(&o->chanlist, (struct name *)c);
	song_getcurinput(o, &c->curinput_dev, &c->curinput_ch);
	song_setcurchan(o, c);
	return c;
}

/*
 * delete the given chan from the song
 */
void
song_chandel(struct song *o, struct songchan *c) {
	struct songfilt *f;

	if (o->curchan == c) {
		o->curchan = NULL;
	}
	SONG_FOREACH_FILT(o, f) {
		if (f->curchan == c) {
			f->curchan = NULL;
		}
	}
	name_remove(&o->chanlist, (struct name *)c);
	track_done(&c->conf);
	name_done(&c->name);	
	mem_free(c);
}

/*
 * return the chan with the given name
 */
struct songchan *
song_chanlookup(struct song *o, char *name) {
	return (struct songchan *)name_lookup(&o->chanlist, name);
}

/*
 * return the chan matching the given (dev, ch) pair
 */
struct songchan *
song_chanlookup_bynum(struct song *o, unsigned dev, unsigned ch) {
	struct songchan *i;

	SONG_FOREACH_CHAN(o, i) {
		if (i->dev == dev && i->ch == ch) {
			return i;
		}
	}
	return 0;
}

/*
 * add a new filt to the song
 */
struct songfilt *
song_filtnew(struct song *o, char *name) {
	struct songfilt *f;

	f = (struct songfilt *)mem_alloc(sizeof(struct songfilt));
	f->curchan = NULL;
	name_init(&f->name, name);
	filt_init(&f->filt);

	name_add(&o->filtlist, (struct name *)f);
	song_getcurchan(o, &f->curchan);
	song_setcurfilt(o, f);
	return f;
}

/*
 * delete the given filter from the song
 */
void
song_filtdel(struct song *o, struct songfilt *f) {
	struct songtrk *t;

	if (o->curfilt == f) {
		o->curfilt = NULL;
	}
	SONG_FOREACH_TRK(o, t) {
		if (t->curfilt == f) {
			t->curfilt = NULL;
		}		
	}	
	name_remove(&o->filtlist, (struct name *)f);
	filt_done(&f->filt);
	name_done(&f->name);	
	mem_free(f);
}

/*
 * return the filt conrresponding to the given name
 */
struct songfilt *
song_filtlookup(struct song *o, char *name) {
	return (struct songfilt *)name_lookup(&o->filtlist, name);
}

/*
 * add a new sx to the song
 */
struct songsx *
song_sxnew(struct song *o, char *name) {
	struct songsx *x;

	x = (struct songsx *)mem_alloc(sizeof(struct songsx));
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
song_sxdel(struct song *o, struct songsx *x) {
	if (o->cursx == x) {
		o->cursx = NULL;
	}
	name_remove(&o->sxlist, (struct name *)x);
	sysexlist_done(&x->sx);
	name_done(&x->name);	
	mem_free(x);
}

/*
 * return the sx with the given name
 */
struct songsx *
song_sxlookup(struct song *o, char *name) {
	return (struct songsx *)name_lookup(&o->sxlist, name);
}

/*
 * the following routines are basically getters and setters for song
 * "current" parametters. They also deal with inheritancs of these
 * values
 */
void
song_getcursx(struct song *o, struct songsx **r) {
	*r = o->cursx;
}

void
song_setcursx(struct song *o, struct songsx *x) {
	o->cursx = x;
}

void
song_getcurtrk(struct song *o, struct songtrk **r) {
	*r = o->curtrk;
}

void
song_setcurtrk(struct song *o, struct songtrk *t) {
	o->curtrk = t;
}

void
song_getcurfilt(struct song *o, struct songfilt **r) {
	struct songtrk *t;
	song_getcurtrk(o, &t);
	if (t) {
		*r = t->curfilt;
	} else {
		*r = o->curfilt;
	}
}

void
song_setcurfilt(struct song *o, struct songfilt *f) {
	struct songtrk *t;
	o->curfilt = f;
	song_getcurtrk(o, &t);
	if (t) {
		t->curfilt = f;
	}
}

void
song_getcurchan(struct song *o, struct songchan **r) {
	struct songfilt *f;
	song_getcurfilt(o, &f);
	if (f) {
		*r = f->curchan;
	} else {
		*r = o->curchan;
	}
}

void
song_setcurchan(struct song *o, struct songchan *c) {
	struct songfilt *f;
	song_getcurfilt(o, &f);
	if (f) {
		f->curchan = c;
	} else {
		o->curchan = c;
	}
}

void
song_getcurinput(struct song *o, unsigned *dev, unsigned *ch) {
	struct songchan *c;
	song_getcurchan(o, &c);
	if (c) {
		*dev = c->curinput_dev;
		*ch = c->curinput_ch;
	} else {
		*dev = o->curinput_dev;
		*ch = o->curinput_ch;
	}
}

void
song_setcurinput(struct song *o, unsigned dev, unsigned ch) {
	struct songchan *c;
	song_getcurchan(o, &c);
	if (c) {
		c->curinput_dev = dev;
		c->curinput_ch = ch;
	} else {
		o->curinput_dev = dev;
		o->curinput_ch = ch;
	}
}

/*
 * send to the output all events from all chans
 */
void
song_playconf(struct song *o) {
	struct songchan *i;
	struct seqptr cp;
	struct state *st;
	struct ev ev;
	
	SONG_FOREACH_CHAN(o, i) {
		seqptr_init(&cp, &i->conf);
		for (;;) {
			st = seqptr_evget(&cp);
			if (st == NULL)
				break;
			ev = st->ev;
			if (EV_ISVOICE(&ev)) {
				ev.dev = i->dev;
				ev.ch = i->ch;
				mux_putev(&ev);
			} else {
				dbg_puts("song_playconf: event not implemented : ");
				dbg_putx(ev.cmd);
				dbg_puts("\n");
			}
		}
		seqptr_done(&cp);
	}
	mux_flush();
	mux_sleep(DEFAULT_CHANWAIT);
}

/*
 * send all sysex messages
 */
void
song_playsysex(struct song *o) {
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
song_metaput(struct song *o, struct ev *ev) {
	switch(ev->cmd) {
	case EV_TIMESIG:
		o->bpm = ev->timesig_beats;
		o->tpb = ev->timesig_tics;
		break;
	case EV_TEMPO:
		o->tempo = ev->tempo_usec24;
		mux_chgtempo(o->tempo_factor * o->tempo  / 0x100);	
		break;
	default:
		break;
	}
}

/*
 * move all track pointers 1 tick forward. Return true if at least one
 * track moved forward and 0 if no track moved forward (ie end of the
 * song was track reached)
 *
 * Note that must be no events available on any track, in other words,
 * this routine must be called after song_ticplay()
 */
unsigned
song_ticskip(struct song *o) {
	unsigned tic;
	struct songtrk *i;

	/* 
	 * tempo_track 
	 */
	(void)seqptr_ticskip(&o->metaptr, 1);
	o->tic++;
	if (o->tic >= o->tpb) {
		o->tic = 0;
		o->beat++;
		if (o->beat >= o->bpm) {
			o->beat = 0;
			o->measure++;
		}
	}
#if 0
	if (song_debug) {
		dbg_puts("song_ticskip: ");
		dbg_putu(o->measure);
		dbg_puts(":");
		dbg_putu(o->beat);
		dbg_puts(":");
		dbg_putu(o->tic);
		dbg_puts("\n");
	}
#endif
	tic = 1;
	SONG_FOREACH_TRK(o, i) {
		tic += seqptr_ticskip(&i->trackptr, 1);
	}
	return tic;
}

/*
 * play data corresponding to the current tick used by playcb and
 * recordcb.
 */
void
song_ticplay(struct song *o) {
	struct songtrk *i;
	struct state *st;
	struct ev ev;
	
	while ((st = seqptr_evget(&o->metaptr))) {
		song_metaput(o, &st->ev);
	}
	metro_tic(&o->metro, o->beat, o->tic);
	SONG_FOREACH_TRK(o, i) {
		while ((st = seqptr_evget(&i->trackptr))) {
			ev = st->ev;
			if (EV_ISVOICE(&ev)) {
				if (st->phase & EV_PHASE_FIRST)
					st->tag = 1;
				if (!i->mute) {
					if (st->tag) {
						mux_putev(&ev);
					} else if (song_debug) {
						dbg_puts("song_ticplay: ");
						ev_dbg(&ev);
						dbg_puts(": no tag\n");
					}
				}
			} else {
				dbg_puts("song_ticplay: event not implemented : ");
				dbg_putx(ev.cmd);
				dbg_puts("\n");
			}
		}
	}
}


/*
 * restore the given frame and tag it appropriately
 */
void
song_strestore(struct state *s) {
	struct ev re;
	
	if (!EV_ISNOTE(&s->ev)) {
		if (state_restore(s, &re)) {
			if (song_debug) {
				dbg_puts("song_strestore: ");
				ev_dbg(&s->ev);
				dbg_puts(": restored -> ");
				ev_dbg(&re);
				dbg_puts("\n");
			}
			mux_putev(&re);
		}
		s->tag = 1;
	} else {
		if (song_debug) {
			dbg_puts("song_strestore: ");
			ev_dbg(&s->ev);
			dbg_puts(": not restored (not tagged)\n");
		}
		s->tag = 0;
	}
}

/*
 * restore all frames in the given statelist. First, restore events
 * that have context, then the rest.
 */
void
song_confrestore(struct statelist *slist) {
	struct state *s;

	for (s = slist->first; s != NULL; s = s->next) {
		song_strestore(s);
	}
}

/*
 * cancel the given frame
 */
void
song_stcancel(struct state *s) {
	struct ev ca;

	if (s->tag) {
		if (state_cancel(s, &ca)) {
			if (song_debug) {
				dbg_puts("song_stcancel: ");
				ev_dbg(&s->ev);
				dbg_puts(": canceled -> ");
				ev_dbg(&ca);
				dbg_puts("\n");
			}
			mux_putev(&ca);
		}
		s->tag = 0;
	} else {
		if (song_debug) {
			dbg_puts("song_stcancel: ");
			ev_dbg(&s->ev);
			dbg_puts(": not canceled (no tag)\n");
		}
	}
}


/*
 * cancel all frames in the given state list
 */
void
song_confcancel(struct statelist *slist) {
	struct state *s;

	for (s = slist->first; s != NULL; s = s->next) {
		song_stcancel(s);
	}
}


/* --------------------------------------------------------------------- */

/*
 * call-back called when the first midi tick arrives
 */
void
song_startcb(struct song *o) {	
	if (song_debug) {
		dbg_puts("song_startcb:\n");
	}
	if (o->mode & SONG_PLAY) {
		song_ticplay(o);
		mux_flush();
	}
}

/*
 * call-back called when the midi clock stopped
 */
void
song_stopcb(struct song *o) {	
	if (song_debug) {
		dbg_puts("song_rec_stop:\n");
	}
}

/*
 * call-back, called when the midi clock moved one tick forward
 */
void
song_movecb(struct song *o) {
	unsigned delta;

	if (o->mode & SONG_REC) {
		while (seqptr_evget(&o->recptr))
			; /* nothing */
		delta = seqptr_ticskip(&o->recptr, 1);
		if (delta == 0)
			seqptr_ticput(&o->recptr, 1);
	}
	if (o->mode & SONG_PLAY) {
		(void)song_ticskip(o);
		song_ticplay(o);
	}
	mux_flush();
}

/*
 * call-back called when a midi event arrives
 */
void
song_evcb(struct song *o, struct ev *ev) {
	struct ev filtout[FILT_MAXNRULES];
	unsigned i, nev;

	if (!EV_ISVOICE(ev)) {
		return;
	}
	if (o->filt) {
		nev = filt_do(o->filt, ev, filtout);
	} else {
		filtout[0] = *ev;
		nev = 1;
	}
	if (o->mode & SONG_REC) {
		if (mux_getphase() >= MUX_START) {
			for (i = 0; i < nev; i++) {
				if (filtout[i].cmd == EV_NULL) {
					continue;
				}
				(void)seqptr_evput(&o->recptr, &filtout[i]);
			}
		}
	}
	for (i = 0; i < nev; i++) {
		if (filtout[i].cmd == EV_NULL) {
			continue;
		}
		mux_putev(&filtout[i]);
	}
}

/*
 * call-back called when a sysex message arrives
 */
void
song_sysexcb(struct song *o, struct sysex *sx) {
	if (song_debug) {
		dbg_puts("song_sysexcb:\n");
	}
	if (o->mode & SONG_REC) {
		if (o->cursx) {
			if (sx == NULL) {
				dbg_puts("got null sx\n");
			} else {
				sysexlist_put(&o->cursx->sx, sx);
			}
		}
	}
}

/*
 * setup everything to start play/record: the current filter, go to
 * the current position, etc... must be called with the mux
 * initialized. The 'cb' parameter is a function that will be called
 * for each input event.
 */
void
song_start(struct song *o, unsigned mode, unsigned countdown) {
	struct songfilt *f;
	struct songtrk *t;
	unsigned tic, off;
	struct state *s;

	o->mode = mode;

	/*
	 * check for the current filter
	 */
	song_getcurfilt(o, &f);
	if (f) {
		o->filt = &f->filt;	
	} else {
		o->filt = NULL;
	}

	/*
	 * set the current position and the default
	 * signature and tempo
	 */
	o->measure = o->curpos >= countdown ? o->curpos - countdown : 0;
	o->beat = 0;
	o->tic = 0;	
	o->bpm = DEFAULT_BPM;
	o->tpb = DEFAULT_TPB;
	o->tempo = TEMPO_TO_USEC24(DEFAULT_TEMPO, DEFAULT_TPB);

	/*
	 * slightly shift start position, so notes
	 * within the same quantum are played
	 */
	tic = track_findmeasure(&o->meta, o->measure);
	off = o->curquant / 2;
	if (off > o->tpb) 
		off = o->tpb;
	if (off > tic)
		off = tic;
	if (o->measure > 0 && off > 0) {
		tic -= off;
		o->tic = o->tpb - off;
		o->beat = o->bpm - 1;
		o->measure--;
	}
	
	/*
	 * move all tracks to the current position
	 */
	SONG_FOREACH_TRK(o, t) {
		seqptr_init(&t->trackptr, &t->track);
		seqptr_skip(&t->trackptr, tic);
	}
	seqptr_init(&o->metaptr, &o->meta);
	seqptr_skip(&o->metaptr, tic);
	
	seqptr_init(&o->recptr, &o->rec);
	seqptr_seek(&o->recptr, tic);

	mux_open();

	/*
	 * send sysex messages and channel config messages
	 */
	song_playsysex(o);
	song_playconf(o);

	/*
	 * restore track states
	 */
	SONG_FOREACH_TRK(o, t) {
		song_confrestore(&t->trackptr.statelist);
	}
	for (s = o->metaptr.statelist.first; s != NULL; s = s->next) {
		if (EV_ISMETA(&s->ev)) {
			if (song_debug) {
				dbg_puts("song_start: ");
				ev_dbg(&s->ev);
				dbg_puts(": restoring meta-event\n");
			}
			song_metaput(o, &s->ev);
			s->tag = 1;
		} else {
			if (song_debug) {
				dbg_puts("song_start: ");
				ev_dbg(&s->ev);
				dbg_puts(": not restored (not tagged)\n");
			}
			s->tag = 0;
		}
	}
	mux_flush();	
}

/*
 * stop play/record: undo song_start and things done during the
 * play/record process. Must be called with the mux initialised
 */
void
song_stop(struct song *o) {
	struct songtrk *t;
	struct state *st;
	struct ev ev;

	metro_shut(&o->metro);	

	/*
	 * stop sounding notes
	 */
	SONG_FOREACH_TRK(o, t) {
		song_confcancel(&t->trackptr.statelist);
	}
	mux_flush();	
	mux_close();

	SONG_FOREACH_TRK(o, t) {
		statelist_empty(&t->trackptr.statelist);
		seqptr_done(&t->trackptr);
	}

	/*
	 * if there is no filter for recording there may be
	 * unterminated frames, so finalize them.
	 */
	for (st = o->recptr.statelist.first; st != NULL; st = st->next) {
		if (!(st->phase & EV_PHASE_LAST) && state_cancel(st, &ev)) {
			seqptr_evput(&o->recptr, &ev);
		}
	}
	seqptr_done(&o->recptr);
	seqptr_done(&o->metaptr);
}

/*
 * play the song initialize the midi/timer and start the event loop
 */
void
song_play(struct song *o) {
	song_start(o, SONG_PLAY, 0);
	
	if (song_debug) {
		dbg_puts("song_play: starting loop, waiting for a start event...\n");
	}
	mux_startwait();
	mux_run();
	
	song_stop(o);
}

/*
 * record the current track: initialise the midi/timer and start the
 * event loop
 */
void
song_record(struct song *o) {
	struct songtrk *t;
	
	song_getcurtrk(o, &t);
	if (!t || t->mute) {
		dbg_puts("song_record: no current track or current track is muted\n");
	}

	song_start(o, SONG_PLAY | SONG_REC, 1);
	if (song_debug) {
		dbg_puts("song_record: started loop, waiting for a start event...\n");
	}
	mux_startwait();
	mux_run();
	song_stop(o);
	
	if (t) {
		track_merge(&o->curtrk->track, &o->rec);
	}
	track_clear(&o->rec);
}

/*
 * move input events directly to the output
 */
void
song_idle(struct song *o) {
	song_start(o, 0, 0);
	
	if (song_debug) {
		dbg_puts("song_idle: started loop...\n");
	}
	mux_run();	
	song_stop(o);
}
