/*
 * Copyright (c) 2003-2005 Alexandre Ratchov
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
#include "trackop.h"
#include "filt.h"
#include "song.h"
#include "cons.h"		/* cons_errxxx */
#include "default.h"

unsigned song_debug = 0;

void song_playcb(void *, struct ev_s *);
void song_recordcb(void *, struct ev_s *);
void song_idlecb(void *, struct ev_s *);

struct songtrk_s *
songtrk_new(char *name) {
	struct songtrk_s *o;
	o = (struct songtrk_s *)mem_alloc(sizeof(struct songtrk_s));
	name_init(&o->name, name);
	o->curfilt = NULL;
	track_init(&o->track);
	track_rew(&o->track, &o->trackptr);
	o->mute = 0;
	return o;
}


void
songtrk_delete(struct songtrk_s *o) {
	track_done(&o->track);
	name_done(&o->name);
	mem_free(o);
}


struct songchan_s *
songchan_new(char *name) {
	struct songchan_s *o;
	o = (struct songchan_s *)mem_alloc(sizeof(struct songchan_s));
	name_init(&o->name, name);
	track_init(&o->conf);
	o->dev = o->curinput_dev = 0;
	o->ch = o->curinput_ch = 0;
	return o;
}


void
songchan_delete(struct songchan_s *o) {
	track_done(&o->conf);
	name_done(&o->name);	
	mem_free(o);
}


struct songfilt_s *
songfilt_new(char *name) {
	struct songfilt_s *o;
	o = (struct songfilt_s *)mem_alloc(sizeof(struct songfilt_s));
	o->curchan = NULL;
	name_init(&o->name, name);
	filt_init(&o->filt);
	return o;
}


void
songfilt_delete(struct songfilt_s *o) {
	filt_done(&o->filt);
	name_done(&o->name);	
	mem_free(o);
}

struct songsx_s *
songsx_new(char *name) {
	struct songsx_s *o;
	o = (struct songsx_s *)mem_alloc(sizeof(struct songsx_s));
	name_init(&o->name, name);
	sysexlist_init(&o->sx);
	return o;
}


void
songsx_delete(struct songsx_s *o) {
	sysexlist_done(&o->sx);
	name_done(&o->name);	
	mem_free(o);
}

	/*
	 * allocates and initialises a song structure
	 */

struct song_s *
song_new(void) {
	struct song_s *o;
	o = (struct song_s *)mem_alloc(sizeof(struct song_s));
	song_init(o);
	return o;
}

	/*
	 * frees a song structure 
	 */

void
song_delete(struct song_s *o) {
	song_done(o);
	mem_free(o);
}

	/*
	 * initialises the song
	 */

void
song_init(struct song_s *o) {
	/* song parameters */
	o->trklist = NULL;
	o->chanlist = NULL;
	o->filtlist = NULL;
	o->sxlist = NULL;
	o->tics_per_unit = DEFAULT_TPU;
	track_init(&o->meta);
	track_rew(&o->meta, &o->metaptr);
	track_init(&o->rec);
	track_rew(&o->rec, &o->recptr);
	/* runtime play record parameters */
	o->filt = NULL;
	o->tics_per_beat = o->tics_per_unit / DEFAULT_BPM;
	o->beats_per_measure = DEFAULT_BPM;
	o->tempo = TEMPO_TO_USEC24(DEFAULT_TEMPO, o->tics_per_beat);
	/* metronome */
	o->tic = o->beat = o->measure = 0;
	o->metro_enabled = 1;
	o->metro_hi.cmd = EV_NON;
	o->metro_hi.data.voice.dev = 0;
	o->metro_hi.data.voice.ch = 9;
	o->metro_hi.data.voice.b0 = 67;
	o->metro_hi.data.voice.b1 = 127;
	o->metro_lo.cmd = EV_NON;
	o->metro_lo.data.voice.dev = 0;
	o->metro_lo.data.voice.ch = 9;
	o->metro_lo.data.voice.b0 = 68;
	o->metro_lo.data.voice.b1 = 90;
	/* defaults */
	o->curtrk = NULL;
	o->curfilt = NULL;
	o->cursx = NULL;
	o->curchan = NULL;
	o->curpos = 0;
	o->curlen = 0;
	o->curquant = 0;
	o->curinput_dev = 0;
	o->curinput_ch = 0;
}

	/*
	 * deletes a song without freeing it
	 */

void
song_done(struct song_s *o) {
	struct songtrk_s *t, *tnext;
	struct songchan_s *i, *inext;
	struct songfilt_s *f, *fnext;
	struct songsx_s *s, *snext;
	for (t = o->trklist; t != NULL; t = tnext) {
		tnext = (struct songtrk_s *)t->name.next;
		songtrk_delete(t);
	}
	for (i = o->chanlist; i != NULL; i = inext) {
		inext = (struct songchan_s *)i->name.next;
		songchan_delete(i);
	}
	for (f = o->filtlist; f != NULL; f = fnext) {
		fnext = (struct songfilt_s *)f->name.next;
		songfilt_delete(f);
	}
	for (s = o->sxlist; s != NULL; s = snext) {
		snext = (struct songsx_s *)s->name.next;
		songsx_delete(s);
	}
	track_done(&o->meta);
}

/* -------------------------------------------------- track stuff --- */

	/*
	 * adds a new track to the song
	 */

void
song_trkadd(struct song_s *o, struct songtrk_s *t) {
	name_add((struct name_s **)&o->trklist, (struct name_s *)t);
	song_getcurfilt(o, &t->curfilt);
	song_setcurtrk(o, t);
}

	/*
	 * returns the track conrresponding to the
	 * given name
	 */

struct songtrk_s *
song_trklookup(struct song_s *o, char *name) {
	return (struct songtrk_s *)name_lookup((struct name_s **)&o->trklist, name);
}


unsigned
song_trkrm(struct song_s *o, struct songtrk_s *t) {
	struct songtrk_s **i;
	
	if (o->curtrk == t) {
		o->curtrk = NULL;
	}
	i = &o->trklist;
	while(*i != NULL) {
		if (*i == t) {
			*i = (struct songtrk_s *)t->name.next;
			break;
		}
		i = (struct songtrk_s **)&(*i)->name.next;
	}
	return 1;
}


/* -------------------------------------------------- chan stuff --- */

	/*
	 * adds a new chan to the song
	 */

void
song_chanadd(struct song_s *o, struct songchan_s *i) {
	name_add((struct name_s **)&o->chanlist, (struct name_s *)i);
	song_getcurinput(o, &i->curinput_dev, &i->curinput_ch);
	song_setcurchan(o, i);
}

	/*
	 * returns the chan conrresponding to the
	 * given name
	 */

struct songchan_s *
song_chanlookup(struct song_s *o, char *name) {
	return (struct songchan_s *)name_lookup((struct name_s **)&o->chanlist, name);
}


struct songchan_s *
song_chanlookup_bynum(struct song_s *o, unsigned dev, unsigned ch) {
	struct songchan_s *i;
	for (i = o->chanlist; i != NULL; i = (struct songchan_s *)i->name.next) {
		if (i->dev == dev && i->ch == ch) {
			return i;
		}
	}
	return 0;
}

unsigned
song_chanrm(struct song_s *o, struct songchan_s *c) {
	struct songchan_s **i;
	struct songfilt_s *f;

	if (o->curchan == c) {
		o->curchan = NULL;
	}
	for (f = o->filtlist; f != NULL; f = (struct songfilt_s *)f->name.next) {
		if (f->curchan == c) {
			f->curchan = NULL;
		}
	}	

	i = &o->chanlist;
	while(*i != NULL) {
		if (*i == c) {
			*i = (struct songchan_s *)c->name.next;
			break;
		}
		i = (struct songchan_s **)&(*i)->name.next;
	}
	return 1;
}

/* -------------------------------------------------- filt stuff --- */

	/*
	 * adds a new filt to the song
	 */

void
song_filtadd(struct song_s *o, struct songfilt_s *f) {
	name_add((struct name_s **)&o->filtlist, (struct name_s *)f);
	song_getcurchan(o, &f->curchan);
	song_setcurfilt(o, f);
}

	/*
	 * returns the filt conrresponding to the
	 * given name
	 */

struct songfilt_s *
song_filtlookup(struct song_s *o, char *name) {
	return (struct songfilt_s *)name_lookup((struct name_s **)&o->filtlist, name);
}


unsigned
song_filtrm(struct song_s *o, struct songfilt_s *f) {
	struct songtrk_s *t;
	struct songfilt_s **i;

	if (o->curfilt == f) {
		o->curfilt = NULL;
	}
	for (t = o->trklist; t != NULL; t = (struct songtrk_s *)t->name.next) {
		if (t->curfilt == f) {
			t->curfilt = NULL;
		}		
	}	
	i = &o->filtlist;
	while(*i != NULL) {
		if (*i == f) {
			*i = (struct songfilt_s *)f->name.next;
			break;
		}
		i = (struct songfilt_s **)&(*i)->name.next;
	}
	return 1;
}

/* -------------------------------------------------- sx stuff --- */

	/*
	 * adds a new sx to the song
	 */

void
song_sxadd(struct song_s *o, struct songsx_s *x) {
	name_add((struct name_s **)&o->sxlist, (struct name_s *)x);
	song_setcursx(o, x);
}

	/*
	 * returns the sx conrresponding to the
	 * given name
	 */

struct songsx_s *
song_sxlookup(struct song_s *o, char *name) {
	return (struct songsx_s *)name_lookup((struct name_s **)&o->sxlist, name);
}


unsigned
song_sxrm(struct song_s *o, struct songsx_s *f) {
	struct songsx_s **i;

	if (o->cursx == f) {
		o->cursx = NULL;
	}
	i = &o->sxlist;
	while(*i != NULL) {
		if (*i == f) {
			*i = (struct songsx_s *)f->name.next;
			break;
		}
		i = (struct songsx_s **)&(*i)->name.next;
	}
	return 1;
}


/* ------------------------------------------------- {get,set}xxx --- */

void
song_getcursx(struct song_s *o, struct songsx_s **r) {
	*r = o->cursx;
}

void
song_setcursx(struct song_s *o, struct songsx_s *x) {
	o->cursx = x;
}

void
song_getcurtrk(struct song_s *o, struct songtrk_s **r) {
	*r = o->curtrk;
}

void
song_setcurtrk(struct song_s *o, struct songtrk_s *t) {
	o->curtrk = t;
}

void
song_getcurfilt(struct song_s *o, struct songfilt_s **r) {
	struct songtrk_s *t;
	song_getcurtrk(o, &t);
	if (t) {
		*r = t->curfilt;
	} else {
		*r = o->curfilt;
	}
}

void
song_setcurfilt(struct song_s *o, struct songfilt_s *f) {
	struct songtrk_s *t;
	o->curfilt = f;
	song_getcurtrk(o, &t);
	if (t) {
		t->curfilt = f;
	}
}

void
song_getcurchan(struct song_s *o, struct songchan_s **r) {
	struct songfilt_s *f;
	song_getcurfilt(o, &f);
	if (f) {
		*r = f->curchan;
	} else {
		*r = o->curchan;
	}
}

void
song_setcurchan(struct song_s *o, struct songchan_s *c) {
	struct songfilt_s *f;
	song_getcurfilt(o, &f);
	if (f) {
		f->curchan = c;
	} else {
		o->curchan = c;
	}
}

void
song_getcurinput(struct song_s *o, unsigned *dev, unsigned *ch) {
	struct songchan_s *c;
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
song_setcurinput(struct song_s *o, unsigned dev, unsigned ch) {
	struct songchan_s *c;
	song_getcurchan(o, &c);
	if (c) {
		c->curinput_dev = dev;
		c->curinput_ch = ch;
	} else {
		o->curinput_dev = dev;
		o->curinput_ch = ch;
	}
}

/* ------------------------------------------------- global stuff --- */

	/*
	 * convert a measure to a number of tics
	 */

unsigned
song_measuretotic(struct song_s *o, unsigned measure) {
	return track_opfindtic(&o->meta, measure);
}

	/*
	 * if the position pointer is on the first tic
	 * of a beat then send a metronome "tic" to the midiout
	 * note: the output is not flushed
	 */

void
song_metrotic(struct song_s *o) {
	struct ev_s ev;
	if (o->metro_enabled && o->tic == 0) {
		if (o->beat == 0) {
			ev = o->metro_hi;
		} else {
			ev = o->metro_lo;
		}
		mux_putev(&ev);
		ev.cmd = EV_NOFF;
		ev.data.voice.b1 = EV_NOFF_DEFAULTVEL;
		mux_putev(&ev);
	}
}

	/*
	 * returns true if each track in the song is finished
	 * and false if at least one track is not finished
	 */

unsigned
song_finished(struct song_s *o) {
	struct songtrk_s *i;
	
	for (i = o->trklist; i != NULL; i = (struct songtrk_s *)i->name.next) {
		if (!track_finished(&i->track, &i->trackptr)) {
			return 0;
		}
	}
	return 1;
}


void
song_playconf(struct song_s *o) {
	struct songchan_s *i;
	struct seqptr_s cp;
	struct ev_s ev;
	
	for (i = o->chanlist; i != NULL; i = (struct songchan_s *)i->name.next) {
		track_rew(&i->conf, &cp);
		while (track_evavail(&i->conf, &cp) ) {
			track_evget(&i->conf, &cp, &ev);
			if (EV_ISVOICE(&ev)) {
				ev.data.voice.dev = i->dev;
				ev.data.voice.ch = i->ch;
				mux_putev(&ev);
			} else {
				dbg_puts("song_playconf: event not implemented : ");
				dbg_putx(ev.cmd);
				dbg_puts("\n");
			}
		}
	}
	mux_flush();
	mux_sleep(DEFAULT_CHANWAIT);
}


void
song_playsysex(struct song_s *o) {
	struct songsx_s *l;
	struct sysex_s *s;
	struct chunk_s *c;
	
	for (l = o->sxlist; l != NULL; l = (struct songsx_s *)l->name.next) {
		for (s = l->sx.first; s != NULL; s = s->next) {
			for (c = s->first; c != NULL; c = c->next) {
				mux_sendraw(s->unit, c->data, c->used);
				mux_flush();
			}
			mux_sleep(DEFAULT_SXWAIT);
		}
	}
}


void
song_nexttic(struct song_s *o) {
	o->tic++;
	if (o->tic >= o->tics_per_beat) {
		o->tic = 0;
		o->beat++;
		if (o->beat >= o->beats_per_measure) {
			o->beat = 0;
			o->measure++;
		}
	}
}

	/*
	 * play the data corresponding to a tic
	 * used by playcb and recordcb
	 */

void
song_playtic(struct song_s *o) {
	struct songtrk_s *i;
	struct ev_s ev;
	unsigned phase;
	
	phase = mux_getphase();
	
	if (phase == MUX_NEXT) {
		/* tempo_track */
		if (track_ticavail(&o->meta, &o->metaptr)) {
			track_ticnext(&o->meta, &o->metaptr);
		}
		song_nexttic(o);
		for (i = o->trklist; i != NULL; i = (struct songtrk_s *)i->name.next) {
			if (track_ticavail(&i->track, &i->trackptr)) {
				track_ticnext(&i->track, &i->trackptr);
			}
		}
	}
	
	if (phase == MUX_NEXT || phase == MUX_FIRST) {
		/* tempo_track */
		while (track_evavail(&o->meta, &o->metaptr)) {
			track_evget(&o->meta, &o->metaptr, &ev);		
			if (o->measure < o->curpos) {
				break;
			}
			switch(ev.cmd) {
			case EV_TIMESIG:
				o->beats_per_measure = ev.data.sign.beats;
				o->tics_per_beat = ev.data.sign.tics;
				break;
			case EV_TEMPO:
				o->tempo = ev.data.tempo.usec24;
				mux_chgtempo(o->tempo);	
				break;
			default:
				break;
			}
		}
		song_metrotic(o);
		for (i = o->trklist; i != NULL; i = (struct songtrk_s *)i->name.next) {
			while (track_evavail(&i->track, &i->trackptr)) {
				track_evget(&i->track, &i->trackptr, &ev);
				if (EV_ISVOICE(&ev)) {
					if (!i->mute) {
						mux_putev(&ev);
					}
				} else {
					dbg_puts("song_playtic: event not implemented : ");
					dbg_putx(ev.cmd);
					dbg_puts("\n");
				}
			}
		}	
	}
}

void
song_rt_setup(struct song_s *o) {
	struct songfilt_s *f;
	song_getcurfilt(o, &f);
	if (f) {
		o->filt = &f->filt;	
	} else {
		o->filt = NULL;
	}
}


	/*
	 * go to a position in the song
	 * the position pointer of each track is set to
	 * the given tic. If the track is not long enough
	 * the pointer is moved to the end of track
	 */

void
song_rt_seek(struct song_s *o, unsigned rewind) {
	struct songtrk_s *i;
	unsigned tic, tics_per_measure, rew_tics;
	
	tic = track_opfindtic(&o->meta, o->curpos);
	track_optimeinfo(&o->meta, tic, &o->tempo, &o->beats_per_measure, &o->tics_per_beat);
	o->measure = o->curpos;
	o->beat = 0;
	o->tic = 0;
	
	if (rewind) {
		tics_per_measure = o->beats_per_measure * o->tics_per_beat;

		if (tic >= tics_per_measure) {
			rew_tics = tics_per_measure;
		} else {
			rew_tics = tic;
		}
	
		if (rew_tics != 0) {
			o->measure -= 1;
			o->beat = (tics_per_measure - rew_tics) / o->tics_per_beat;
			o->tic =  (tics_per_measure - rew_tics) % o->tics_per_beat;
			tic -= rew_tics;
		}
	}
	
	for (i = o->trklist; i != NULL; i = (struct songtrk_s *)i->name.next) {
		track_rew(&i->track, &i->trackptr);
		track_seek(&i->track, &i->trackptr, tic);
	}
	track_rew(&o->meta, &o->metaptr);
	track_seek(&o->meta, &o->metaptr, tic);
	track_clear(&o->rec, &o->recptr);
	track_seekblank(&o->rec, &o->recptr, tic);
}

/* ---------------------------------------------- input filtering --- */

void
song_inputcb(void *addr, struct ev_s *ev) {
	struct song_s *o = (struct song_s *)addr;
	
	if (!EV_ISVOICE(ev)) {
		if (o->filt && ev->cmd == EV_TIC) {
			filt_timercb(o->filt);
		}
		o->realtimecb(o, ev);
		return;
	}
	if (o->filt) {
		filt_evcb(o->filt, ev);
	} else {
		o->realtimecb(o, ev);
	}
	mux_flush();
}

void
song_inputstart(struct song_s *o, void (*cb)(void *, struct ev_s *)) {
	o->realtimecb = cb;
	if (o->filt) {
		filt_start(o->filt, cb, o);
	}
}

	/*
	 * shutdowns all input notes (using the statelist)
	 */

void
song_inputshut(struct song_s *o) {
	if (o->filt) {
		filt_shut(o->filt);
	}
}


void
song_inputstop(struct song_s *o) {
	if (o->filt) {
		filt_stop(o->filt);
	}
}

/* --------------------------------------------- output filtering --- */

	/*
	 * XXX: normally, we shoud define a stateful filter (as
	 * for the input) attached only to the output of 
	 * the player (not to the input). In that way
	 * outputshut would shutsown only notes played from 
	 * tracks. But for now, we reset everything
	 */

void
song_outputshut(struct song_s *o) {
	unsigned i;
	struct ev_s ev;
	struct mididev_s *dev;
	
	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		ev.cmd = EV_CTL;		
		ev.data.voice.dev = dev->unit;
		ev.data.voice.b0 = 121;		/* all note off */
		ev.data.voice.b1 = 0;
		for (i = 0; i <= EV_MAXCH; i++) {
			ev.data.voice.ch = i;
			mux_putev(&ev);
		}
		ev.data.voice.dev = dev->unit;
		ev.data.voice.b0 = 123;		/* all ctl reset */
		ev.data.voice.b1 = 0;
		for (i = 0; i <= EV_MAXCH; i++) {
			ev.data.voice.ch = i;
			mux_putev(&ev);
		}
	}
	mux_flush();	
}


/* ------------------------------------------------------- player --- */

	/*
	 * play callback:
	 * is automatically called when an event is received
	 * (tic, start, midiev, etc)
	 */

void
song_playcb(void *addr, struct ev_s *ev) {
	struct song_s *o = (struct song_s *)addr;
	unsigned phase;
	
	phase = mux_getphase();
	switch (ev->cmd) {
	case EV_START:
		if (song_debug) {
			dbg_puts("song_play: got a start\n");
		}
		mux_chgtempo(o->tempo);
		break;
	case EV_STOP:
		if (song_debug) {
			dbg_puts("song_play: got a stop\n");
		}
		break;
	case EV_TIC:
		if (song_finished(o) && phase != MUX_STOP) {
			mux_stopwait();
			break;
		} else {
			song_playtic(o);
		}
		mux_flush();
		break;
	default:		
		if (!EV_ISVOICE(ev)) {
			break;
		}
		mux_putev(ev);
		break;
	}
}

	/*
	 * play the song initialises the
	 * midi/timer and starts an event loop
	 */

void
song_play(struct song_s *o) {
	song_rt_setup(o);
	song_rt_seek(o, 0);

	song_inputstart(o, song_playcb);
	mux_init(song_inputcb, o);
	
	song_playsysex(o);
	song_playconf(o);
	mux_chgtempo(o->tempo);
	mux_chgticrate(o->tics_per_unit);
	
	if (song_debug) {
		dbg_puts("song_play: starting loop, waiting for a start event...\n");
	}
	mux_startwait();
	mux_run();
	
	song_inputshut(o);
	song_outputshut(o);
	
	mux_done();
	song_inputstop(o);
}

	/*
	 * record callback
	 * is automatically called when an event is received
	 * (tic, start, midiev, etc)
	 */

void
song_recordcb(void *addr, struct ev_s *ev) {
	struct song_s *o = (struct song_s *)addr;
	struct sysex_s *sx;
	unsigned phase;
	
	phase = mux_getphase();
	switch (ev->cmd) {
	case EV_START:
		if (song_debug) {
			dbg_puts("song_record: got a start\n");
		}
		break;
	case EV_STOP:
		if (song_debug) {
			dbg_puts("song_record: got a stop\n");
		}
		break;
	case EV_TIC:
		if (phase == MUX_NEXT) {
			if (track_finished(&o->rec, &o->recptr)) {
				track_ticins(&o->rec, &o->recptr);
			}
			track_evlast(&o->rec, &o->recptr);
			track_ticnext(&o->rec, &o->recptr);
		}
		song_playtic(o);
		mux_flush();
		break;
	case EV_SYSEX:
		if (o->cursx) {
			sx = mux_getsysex();
			if (sx == NULL) {
				dbg_puts("got null sx\n");
			} else {
				sysexlist_put(&o->cursx->sx, sx);
			}
		}
		break;
	default:
		if (!EV_ISVOICE(ev)) {
			break;
		}
		if (phase == MUX_START || phase == MUX_NEXT || phase == MUX_FIRST) {
			track_evput(&o->rec, &o->recptr, ev);
		}
		mux_putev(ev);
	}
}

void
song_record(struct song_s *o) {
	struct seqptr_s cp;
	struct songtrk_s *t;
	
	song_getcurtrk(o, &t);
	if (!t || t->mute) {
		dbg_puts("song_record: no current track or current track is muted\n");
	}

	song_rt_setup(o);
	song_rt_seek(o, 1);
	
	song_inputstart(o, song_recordcb);
	mux_init(song_inputcb, o);
	
	song_playsysex(o);
	song_playconf(o);
	mux_chgtempo(o->tempo);
	mux_chgticrate(o->tics_per_unit);
	
	if (song_debug) {
		dbg_puts("song_record: started loop, waiting for a start event...\n");
	}
	mux_startwait();
	mux_run();
	
	song_inputshut(o);
	song_outputshut(o);
	
	mux_done();
	song_inputstop(o);
	
	track_opcheck(&o->rec); 	
	if (t) {
		track_rew(&o->curtrk->track, &cp);
		track_frameins(&o->curtrk->track, &cp, &o->rec);
		track_opcheck(&o->curtrk->track); 
	} else {
		track_clear(&o->rec, &o->recptr);
	}
}

	/*
	 * idle callback:
	 * is automatically called when an event is received
	 * (tic, start, midiev, etc)
	 */

void
song_idlecb(void *addr, struct ev_s *ev) {
	switch (ev->cmd) {
	case EV_START:
		if (song_debug) {
			dbg_puts("song_idle: got a start\n");
		}
		break;
	case EV_STOP:
		if (song_debug) {
			dbg_puts("song_idle: got a stop\n");
		}
		break;
	case EV_TIC:
		break;
	default:
		if (!EV_ISVOICE(ev)) {
			break;
		}
		mux_putev(ev);
		break;
	}
}

	/*
	 * moves inputs events directly to the output
	 */

void
song_idle(struct song_s *o) {
	song_rt_setup(o);
	song_rt_seek(o, 0);

	song_inputstart(o, song_idlecb);
	mux_init(song_inputcb, o);
	
	song_playsysex(o);
	song_playconf(o);
	mux_chgtempo(o->tempo);
	mux_chgticrate(o->tics_per_unit);

	if (song_debug) {
		dbg_puts("song_idle: started loop...\n");
	}
	mux_run();
	
	song_inputshut(o);
	song_outputshut(o);
	
	mux_done();
	song_inputstop(o);
}

