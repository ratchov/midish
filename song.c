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

unsigned song_debug = 0;

void song_playcb(void *, struct ev *);
void song_recordcb(void *, struct ev *);
void song_idlecb(void *, struct ev *);

/*
 * allocate and initialise a song structure
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
 * initialise the given song
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
	track_init(&o->rec);
	/* runtime play record parameters */
	o->filt = NULL;
	o->tics_per_beat = o->tics_per_unit / DEFAULT_BPM;
	o->beats_per_measure = DEFAULT_BPM;
	o->tempo = TEMPO_TO_USEC24(DEFAULT_TEMPO, o->tics_per_beat);
	o->tempo_factor = 0x100;
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
 * delete a song without freeing it
 */
void
song_done(struct song *o) {
	while (o->trklist) {
		song_trkdel(o, o->trklist);
	}
	while (o->chanlist) {
		song_chandel(o, o->chanlist);
	}
	while (o->filtlist) {
		song_filtdel(o, o->filtlist);
	}
	while (o->sxlist) {
		song_sxdel(o, o->sxlist);
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

	name_add((struct name **)&o->trklist, (struct name *)t);
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
	name_remove((struct name **)&o->trklist, (struct name *)t);
	track_done(&t->track);
	name_done(&t->name);
	mem_free(t);
}

/*
 * return the track with the given name
 */
struct songtrk *
song_trklookup(struct song *o, char *name) {
	return (struct songtrk *)name_lookup((struct name **)&o->trklist, name);
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

	name_add((struct name **)&o->chanlist, (struct name *)c);
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
	for (f = o->filtlist; f != NULL; f = (struct songfilt *)f->name.next) {
		if (f->curchan == c) {
			f->curchan = NULL;
		}
	}	
	name_remove((struct name **)&o->chanlist, (struct name *)c);
	track_done(&c->conf);
	name_done(&c->name);	
	mem_free(c);
}

/*
 * return the chan with the given name
 */
struct songchan *
song_chanlookup(struct song *o, char *name) {
	return (struct songchan *)name_lookup((struct name **)&o->chanlist, name);
}

/*
 * return the chan matching the given (dev, ch) pair
 */
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

	name_add((struct name **)&o->filtlist, (struct name *)f);
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
	for (t = o->trklist; t != NULL; t = (struct songtrk *)t->name.next) {
		if (t->curfilt == f) {
			t->curfilt = NULL;
		}		
	}	
	name_remove((struct name **)&o->filtlist, (struct name *)f);
	filt_done(&f->filt);
	name_done(&f->name);	
	mem_free(f);
}

/*
 * return the filt conrresponding to the given name
 */
struct songfilt *
song_filtlookup(struct song *o, char *name) {
	return (struct songfilt *)name_lookup((struct name **)&o->filtlist, name);
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
	name_add((struct name **)&o->sxlist, (struct name *)x);
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
	name_remove((struct name **)&o->sxlist, (struct name *)x);
	sysexlist_done(&x->sx);
	name_done(&x->name);	
	mem_free(x);
}

/*
 * return the sx with the given name
 */
struct songsx *
song_sxlookup(struct song *o, char *name) {
	return (struct songsx *)name_lookup((struct name **)&o->sxlist, name);
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
 * return true if each track in the song is finished and false if at
 * least one track is not finished
 */
unsigned
song_finished(struct song *o) {
	struct songtrk *i;
	
	for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
		if (!seqptr_eot(&i->trackptr)) {
			return 0;
		}
	}
	return 1;
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
	
	for (i = o->chanlist; i != NULL; i = (struct songchan *)i->name.next) {
		seqptr_init(&cp, &i->conf);
		for (;;) {
			st = seqptr_evget(&cp);
			if (st == NULL)
				break;
			ev = st->ev;
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

/*
 * callback called by the filter (if any) that will
 * call the realtimecb of the song (depending of
 * the selected mode (play/record/idle)
 */
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

/*
 * play a meta event
 */
void
song_metaput(struct song *o, struct ev *ev) {
	switch(ev->cmd) {
	case EV_TIMESIG:
		o->beats_per_measure = ev->data.sign.beats;
		o->tics_per_beat = ev->data.sign.tics;
		break;
	case EV_TEMPO:
		o->tempo = ev->data.tempo.usec24;
		mux_chgtempo(o->tempo_factor * o->tempo  / 0x100);	
		break;
	default:
		break;
	}
}

/*
 * play the data corresponding to a tic
 * used by playcb and recordcb
 */
void
song_playtic(struct song *o) {
	struct songtrk *i;
	struct state *st;
	struct ev ev;
	unsigned phase;
	
	phase = mux_getphase();
	
	if (phase == MUX_NEXT) {
		/* 
		 * tempo_track 
		 */
		seqptr_ticskip(&o->metaptr, 1);
		o->tic++;
		if (o->tic >= o->tics_per_beat) {
			o->tic = 0;
			o->beat++;
			if (o->beat >= o->beats_per_measure) {
				o->beat = 0;
				o->measure++;
			}
		}
		for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
			seqptr_ticskip(&i->trackptr, 1);
		}
	}
	
	if (phase == MUX_NEXT || phase == MUX_FIRST) {
		while ((st = seqptr_evget(&o->metaptr))) {
			song_metaput(o, &st->ev);
		}
		metro_tic(&o->metro, o->beat, o->tic);
		for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
			while ((st = seqptr_evget(&i->trackptr))) {
				ev = st->ev;
				if (EV_ISVOICE(&ev)) {
					if (st->phase & EV_PHASE_FIRST)
						st->tag = 1;
					if (!i->mute) {
						if (st->tag) {
							mux_putev(&ev);
						} else if (song_debug) {
							dbg_puts("song_playtic: ");
							ev_dbg(&ev);
							dbg_puts(": no tag\n");
						}
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

#if 0
/*
 * restore the state of a track (all controllers, program changes,
 * etc..
 */
void
song_trkrestore(struct song *o, struct songtrk *t) {
	
		for (s = t->trackptr.statelist.first; s != NULL; s = snext) {
			snext = s->next;
			if (ev_restore(&s->ev, &re)) {
				if (song_debug) {
					dbg_puts("song_start: ");
					ev_dbg(&s->ev);
					dbg_puts(": restored -> ");
					ev_dbg(&re);
					dbg_puts("\n");
				}
				mux_putev(&re);
				s->tag = 1;
			} else {
				if (song_debug) {
					dbg_puts("song_start: ");
					ev_dbg(&s->ev);
					dbg_puts(": not restored (no tag)\n");
				}
				s->tag = 0;
			}
		}
}
#endif

/*
 * setup everything to start play/record: the current filter,
 * go to the current position, etc... must be called with the
 * mux initialised. The 'cb' parameter is a function that
 * will be called for each input event. 
 */
void
song_start(struct song *o, 
    void (*cb)(void *, struct ev *), unsigned countdown) {
	struct songfilt *f;
	struct songtrk *i;
	unsigned tic;
	struct ev re;
	struct state *s, *snext;

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
	o->beats_per_measure = DEFAULT_BPM;
	o->tics_per_beat = DEFAULT_TPB;
	o->tempo = TEMPO_TO_USEC24(DEFAULT_TEMPO, DEFAULT_TPB);
	tic = track_findmeasure(&o->meta, o->measure);
	if (o->measure > 0 && tic > o->curquant / 2 && o->curquant / 2 > 0) {
		tic -= o->curquant / 2;
		o->tic = o->beats_per_measure * o->tics_per_beat - o->curquant / 2;
		o->beat = o->beats_per_measure - 1;
		o->measure--;
	}
	
	/*
	 * move all tracks to the current position
	 */
	for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
		seqptr_init(&i->trackptr, &i->track);
		seqptr_skip(&i->trackptr, tic);
	}
	seqptr_init(&o->metaptr, &o->meta);
	seqptr_skip(&o->metaptr, tic);
	
	seqptr_init(&o->recptr, &o->rec);
	seqptr_seek(&o->recptr, tic);

	o->realtimecb = cb;
	if (o->filt) {
		filt_start(o->filt, cb, o);
	}

	mux_init(song_inputcb, o);
	/*
	 * send sysex messages and channel config messages
	 */
	song_playsysex(o);
	song_playconf(o);

	/*
	 * restore track states
	 */
	for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
		for (s = i->trackptr.statelist.first; s != NULL; s = snext) {
			snext = s->next;
			if (ev_restore(&s->ev, &re)) {
				if (song_debug) {
					dbg_puts("song_start: ");
					ev_dbg(&s->ev);
					dbg_puts(": restored -> ");
					ev_dbg(&re);
					dbg_puts("\n");
				}
				mux_putev(&re);
				s->tag = 1;
			} else {
				if (song_debug) {
					dbg_puts("song_start: ");
					ev_dbg(&s->ev);
					dbg_puts(": not restored (no tag)\n");
				}
				s->tag = 0;
			}
		}
	}
	for (s = o->metaptr.statelist.first; s != NULL; s = snext) {
		snext = s->next;
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
				dbg_puts(": not restored (no tag)\n");
			}
			s->tag = 0;
		}
	}
	mux_flush();	
}

/*
 * stop play/record: undo song_start and things
 * done during the play/record process. Must be called
 * with the mux initialised
 */
void
song_stop(struct song *o) {
	struct songtrk *i;
	struct state *s, *snext;
	struct ev ca;

	if (o->filt) {
		filt_shut(o->filt);
		filt_stop(o->filt);
	}

	metro_shut(&o->metro);	

	/*
	 * stop sounding notes
	 */
	for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
		for (s = i->trackptr.statelist.first; s != NULL; s = snext) {
			snext = s->next;
			if (s->tag && ev_cancel(&s->ev, &ca)) {
				if (song_debug) {
					dbg_puts("song_stop: ");
					ev_dbg(&s->ev);
					dbg_puts(": canceled -> ");
					ev_dbg(&ca);
					dbg_puts("\n");
				}
				mux_putev(&ca);

			}
			statelist_rm(&i->trackptr.statelist, s);
			state_del(s);
		}
	}
	mux_flush();	
	mux_done();

	for (i = o->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
		seqptr_done(&i->trackptr);
	}
	seqptr_done(&o->recptr);
	seqptr_done(&o->metaptr);
}

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
		mux_chgtempo(o->tempo_factor * o->tempo / 0x100);
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
 * play the song initialise the midi/timer and start the event loop
 */
void
song_play(struct song *o) {
	song_start(o, song_playcb, 0);
	
	if (song_debug) {
		dbg_puts("song_play: starting loop, waiting for a start event...\n");
	}
	mux_startwait();
	mux_run();
	
	song_stop(o);
}

/*
 * record callback is automatically called when an event is received
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
			while (seqptr_evget(&o->recptr))
				; /* nothing */
			if (seqptr_eot(&o->recptr)) {
				seqptr_ticput(&o->recptr, 1);
			} else {
				seqptr_ticskip(&o->recptr, 1);
			}

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
			(void)seqptr_evput(&o->recptr, ev);
		}
		mux_putev(ev);
	}
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

	song_start(o, song_recordcb, 1);
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
 * idle callback: is automatically called when an event is received
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
	song_start(o, song_idlecb, 0);
	
	if (song_debug) {
		dbg_puts("song_idle: started loop...\n");
	}
	mux_run();	
	song_stop(o);
}
