/*
 * Copyright (c) 2003-2006 Alexandre Ratchov
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
#include "trackop.h"
#include "filt.h"
#include "song.h"
#include "cons.h"		/* cons_errxxx */
#include "metro.h"
#include "default.h"

unsigned song_debug = 0;

void song_playcb(void *, struct ev *);
void song_recordcb(void *, struct ev *);
void song_idlecb(void *, struct ev *);

struct songtrk *
songtrk_new(char *name) {
	struct songtrk *o;
	o = (struct songtrk *)mem_alloc(sizeof(struct songtrk));
	name_init(&o->name, name);
	o->curfilt = NULL;
	track_init(&o->track);
	track_rew(&o->track, &o->trackptr);
	o->mute = 0;
	return o;
}


void
songtrk_delete(struct songtrk *o) {
	track_done(&o->track);
	name_done(&o->name);
	mem_free(o);
}


struct songchan *
songchan_new(char *name) {
	struct songchan *o;
	o = (struct songchan *)mem_alloc(sizeof(struct songchan));
	name_init(&o->name, name);
	track_init(&o->conf);
	o->dev = o->curinput_dev = 0;
	o->ch = o->curinput_ch = 0;
	return o;
}


void
songchan_delete(struct songchan *o) {
	track_done(&o->conf);
	name_done(&o->name);	
	mem_free(o);
}


struct songfilt *
songfilt_new(char *name) {
	struct songfilt *o;
	o = (struct songfilt *)mem_alloc(sizeof(struct songfilt));
	o->curchan = NULL;
	name_init(&o->name, name);
	filt_init(&o->filt);
	return o;
}


void
songfilt_delete(struct songfilt *o) {
	filt_done(&o->filt);
	name_done(&o->name);	
	mem_free(o);
}

struct songsx *
songsx_new(char *name) {
	struct songsx *o;
	o = (struct songsx *)mem_alloc(sizeof(struct songsx));
	name_init(&o->name, name);
	sysexlist_init(&o->sx);
	return o;
}


void
songsx_delete(struct songsx *o) {
	sysexlist_done(&o->sx);
	name_done(&o->name);	
	mem_free(o);
}

	/*
	 * allocates and initialises a song structure
	 */

struct song *
song_new(void) {
	struct song *o;
	o = (struct song *)mem_alloc(sizeof(struct song));
	song_init(o);
	return o;
}

	/*
	 * frees a song structure 
	 */

void
song_delete(struct song *o) {
	song_done(o);
	mem_free(o);
}

	/*
	 * initialises the song
	 */

void
song_init(struct song *o) {
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
	metro_init(&o->metro);
	o->tic = o->beat = o->measure = 0;
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
song_done(struct song *o) {
	struct songtrk *t, *tnext;
	struct songchan *i, *inext;
	struct songfilt *f, *fnext;
	struct songsx *s, *snext;
	for (t = o->trklist; t != NULL; t = tnext) {
		tnext = (struct songtrk *)t->name.next;
		songtrk_delete(t);
	}
	for (i = o->chanlist; i != NULL; i = inext) {
		inext = (struct songchan *)i->name.next;
		songchan_delete(i);
	}
	for (f = o->filtlist; f != NULL; f = fnext) {
		fnext = (struct songfilt *)f->name.next;
		songfilt_delete(f);
	}
	for (s = o->sxlist; s != NULL; s = snext) {
		snext = (struct songsx *)s->name.next;
		songsx_delete(s);
	}
	track_done(&o->meta);
	metro_done(&o->metro);
}

/* -------------------------------------------------- track stuff --- */

	/*
	 * adds a new track to the song
	 */

void
song_trkadd(struct song *o, struct songtrk *t) {
	name_add((struct name **)&o->trklist, (struct name *)t);
	song_getcurfilt(o, &t->curfilt);
	song_setcurtrk(o, t);
}

	/*
	 * returns the track conrresponding to the
	 * given name
	 */

struct songtrk *
song_trklookup(struct song *o, char *name) {
	return (struct songtrk *)name_lookup((struct name **)&o->trklist, name);
}


unsigned
song_trkrm(struct song *o, struct songtrk *t) {
	struct songtrk **i;
	
	if (o->curtrk == t) {
		o->curtrk = NULL;
	}
	i = &o->trklist;
	while(*i != NULL) {
		if (*i == t) {
			*i = (struct songtrk *)t->name.next;
			break;
		}
		i = (struct songtrk **)&(*i)->name.next;
	}
	return 1;
}


/* -------------------------------------------------- chan stuff --- */

	/*
	 * adds a new chan to the song
	 */

void
song_chanadd(struct song *o, struct songchan *i) {
	name_add((struct name **)&o->chanlist, (struct name *)i);
	song_getcurinput(o, &i->curinput_dev, &i->curinput_ch);
	song_setcurchan(o, i);
}

	/*
	 * returns the chan conrresponding to the
	 * given name
	 */

struct songchan *
song_chanlookup(struct song *o, char *name) {
	return (struct songchan *)name_lookup((struct name **)&o->chanlist, name);
}


struct songchan *
song_chanlookup_bynum(struct song *o, unsigned dev, unsigned ch) {
	struct songchan *i;
	for (i = o->chanlist; i != NULL; i = (struct songchan *)i->name.next) {
		if (i->dev == dev && i->ch == ch) {
			return i;
		}
	}
	return 0;
}

unsigned
song_chanrm(struct song *o, struct songchan *c) {
	struct songchan **i;
	struct songfilt *f;

	if (o->curchan == c) {
		o->curchan = NULL;
	}
	for (f = o->filtlist; f != NULL; f = (struct songfilt *)f->name.next) {
		if (f->curchan == c) {
			f->curchan = NULL;
		}
	}	

	i = &o->chanlist;
	while(*i != NULL) {
		if (*i == c) {
			*i = (struct songchan *)c->name.next;
			break;
		}
		i = (struct songchan **)&(*i)->name.next;
	}
	return 1;
}

/* -------------------------------------------------- filt stuff --- */

	/*
	 * adds a new filt to the song
	 */

void
song_filtadd(struct song *o, struct songfilt *f) {
	name_add((struct name **)&o->filtlist, (struct name *)f);
	song_getcurchan(o, &f->curchan);
	song_setcurfilt(o, f);
}

	/*
	 * returns the filt conrresponding to the
	 * given name
	 */

struct songfilt *
song_filtlookup(struct song *o, char *name) {
	return (struct songfilt *)name_lookup((struct name **)&o->filtlist, name);
}


unsigned
song_filtrm(struct song *o, struct songfilt *f) {
	struct songtrk *t;
	struct songfilt **i;

	if (o->curfilt == f) {
		o->curfilt = NULL;
	}
	for (t = o->trklist; t != NULL; t = (struct songtrk *)t->name.next) {
		if (t->curfilt == f) {
			t->curfilt = NULL;
		}		
	}	
	i = &o->filtlist;
	while(*i != NULL) {
		if (*i == f) {
			*i = (struct songfilt *)f->name.next;
			break;
		}
		i = (struct songfilt **)&(*i)->name.next;
	}
	return 1;
}

/* -------------------------------------------------- sx stuff --- */

	/*
	 * adds a new sx to the song
	 */

void
song_sxadd(struct song *o, struct songsx *x) {
	name_add((struct name **)&o->sxlist, (struct name *)x);
	song_setcursx(o, x);
}

	/*
	 * returns the sx conrresponding to the
	 * given name
	 */

struct songsx *
song_sxlookup(struct song *o, char *name) {
	return (struct songsx *)name_lookup((struct name **)&o->sxlist, name);
}


unsigned
song_sxrm(struct song *o, struct songsx *f) {
	struct songsx **i;

	if (o->cursx == f) {
		o->cursx = NULL;
	}
	i = &o->sxlist;
	while(*i != NULL) {
		if (*i == f) {
			*i = (struct songsx *)f->name.next;
			break;
		}
		i = (struct songsx **)&(*i)->name.next;
	}
	return 1;
}


/* ------------------------------------------------- {get,set}xxx --- */

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
	 * convert a measure to a number of tics
	 */

unsigned
song_measuretotic(struct song *o, unsigned measure) {
	return track_opfindtic(&o->meta, measure);
}


	/*
	 * returns true if each track in the song is finished
	 * and false if at least one track is not finished
	 */

unsigned
song_finished(struct song *o) {
	struct songtrk *i;
	
	for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
		if (!track_finished(&i->track, &i->trackptr)) {
			return 0;
		}
	}
	return 1;
}


void
song_playconf(struct song *o) {
	struct songchan *i;
	struct seqptr cp;
	struct ev ev;
	
	for (i = o->chanlist; i != NULL; i = (struct songchan *)i->name.next) {
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
song_playsysex(struct song *o) {
	struct songsx *l;
	struct sysex *s;
	struct chunk *c;
	
	for (l = o->sxlist; l != NULL; l = (struct songsx *)l->name.next) {
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
song_nexttic(struct song *o) {
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
song_playtic(struct song *o) {
	struct songtrk *i;
	struct ev ev;
	unsigned phase;
	
	phase = mux_getphase();
	
	if (phase == MUX_NEXT) {
		/* tempo_track */
		if (track_ticavail(&o->meta, &o->metaptr)) {
			track_ticnext(&o->meta, &o->metaptr);
		}
		song_nexttic(o);
		for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
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
		metro_tic(&o->metro, o->beat, o->tic);
		for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
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
song_rt_setup(struct song *o) {
	struct songfilt *f;
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
song_rt_seek(struct song *o, unsigned rewind) {
	struct songtrk *i;
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
		
		/* 
		 * XXX: make multi-measure countdown possible
		 *	rew_meas = (rew + tpm - 1) / tpm;
		 *	rew_beat = ((tic % tpm) + tpb - 1) / tpb
		 *	rew_tic  = (tic % tpb)
		 *	meas -= rew_meas
		 *	beat  = (beat + bpm - rew_beat) % bpm;
		 *	tic   = (tic + tpb - rew_tic) % tpb;
		 */	
		if (rew_tics != 0) {
			o->measure -= 1;
			o->beat = (tics_per_measure - rew_tics) / o->tics_per_beat;
			o->tic =  (tics_per_measure - rew_tics) % o->tics_per_beat;
			tic -= rew_tics;
		}
	}
	
	for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
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
song_inputcb(void *addr, struct ev *ev) {
	struct song *o = (struct song *)addr;
	
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
song_inputstart(struct song *o, void (*cb)(void *, struct ev *)) {
	o->realtimecb = cb;
	if (o->filt) {
		filt_start(o->filt, cb, o);
	}
}

	/*
	 * shutdowns all input notes (using the statelist)
	 */

void
song_inputshut(struct song *o) {
	if (o->filt) {
		filt_shut(o->filt);
	}
}


void
song_inputstop(struct song *o) {
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
song_outputshut(struct song *o) {
	unsigned i;
	struct ev ev;
	struct mididev *dev;

	metro_shut(&o->metro);	
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
song_playcb(void *addr, struct ev *ev) {
	struct song *o = (struct song *)addr;
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
song_play(struct song *o) {
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
song_recordcb(void *addr, struct ev *ev) {
	struct song *o = (struct song *)addr;
	struct sysex *sx;
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
song_record(struct song *o) {
	struct seqptr cp;
	struct songtrk *t;
	
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
		track_frameput(&o->curtrk->track, &cp, &o->rec);
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
song_idlecb(void *addr, struct ev *ev) {
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
song_idle(struct song *o) {
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

