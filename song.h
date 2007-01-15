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

#ifndef MIDISH_SONG_H
#define MIDISH_SONG_H

#define SONG_DEFAULT_BPM	4
#define SONG_DEFAULT_TPB	24
#define SONG_DEFAULT_TEMPO	60

#include "name.h"
#include "track.h"
#include "frame.h"
#include "filt.h"
#include "sysex.h"
#include "metro.h"

struct songchan {
	struct name name;
	struct track conf;
	/* device and midichan of the donc chan */
	unsigned dev, ch;
	/* default source dev/chan */
	unsigned curinput_dev, curinput_ch;		
};

struct songfilt {
	struct name name;
	struct filt filt;
	struct songchan *curchan;
};

struct songtrk {
	struct name name;		/* identifier + list entry */
	struct track track;		/* actual data */
	struct seqptr trackptr;		/* track pointer for RT */
	struct songfilt *curfilt;	/* source and dest. channel */
	unsigned mute;
};

struct songsx {
	struct name name;
	struct sysexlist sx;
};

struct song {
	/* music-related fields, should be saved */
	struct track meta;			/* tempo track */
	struct seqptr metaptr;
	struct name *trklist;
	struct name *chanlist;
	struct name *filtlist;
	struct name *sxlist;
	unsigned tics_per_unit;			/* global time resulution */
	/* real-time parameters */
	unsigned long tempo;			/* 24th of usec per tic */
	unsigned beats_per_measure, tics_per_beat;
	unsigned tempo_factor;			/* tempo = tempo * factor / 200 */
	struct track rec;
	struct seqptr recptr;
	struct filt *filt;
	void (*realtimecb)(void *addr, struct ev *ev);
	/* metronome stuff */
	struct metro metro;
	unsigned measure, beat, tic;
	/* defautls */
	struct songtrk *curtrk;
	struct songfilt *curfilt;
	struct songchan *curchan;
	struct songsx *cursx;
	unsigned curpos;
	unsigned curquant, curlen;
	unsigned curinput_dev, curinput_ch;		
};

#define SONG_FOREACH_TRK(s, i)				\
	for (i = (struct songtrk *)(s)->trklist;	\
	     i != NULL;					\
	     i = (struct songtrk *)i->name.next)

#define SONG_FOREACH_CHAN(s, i)				\
	for (i = (struct songchan *)(s)->chanlist;	\
	     i != NULL;					\
	     i = (struct songchan *)i->name.next)

#define SONG_FOREACH_FILT(s, i)				\
	for (i = (struct songfilt *)(s)->filtlist;	\
	     i != NULL;					\
	     i = (struct songfilt *)i->name.next)

#define SONG_FOREACH_SX(s, i)				\
	for (i = (struct songsx *)(s)->sxlist;		\
	     i != NULL;					\
	     i = (struct songsx *)i->name.next)

struct song *song_new(void);
void song_delete(struct song *o);
void song_init(struct song *o);
void song_done(struct song *o);

struct songtrk *song_trknew(struct song *o, char *name);
struct songtrk *song_trklookup(struct song *o, char *name);
void song_trkdel(struct song *o, struct songtrk *t);

struct songchan *song_channew(struct song *o, char *name, unsigned dev, unsigned ch);
struct songchan *song_chanlookup(struct song *o, char *name);
struct songchan *song_chanlookup_bynum(struct song *o, unsigned dev, unsigned ch);
void song_chandel(struct song *o, struct songchan *c);

struct songfilt *song_filtnew(struct song *o, char *name);
struct songfilt *song_filtlookup(struct song *o, char *name);
void song_filtdel(struct song *o, struct songfilt *f);

struct songsx *song_sxnew(struct song *o, char *name);
struct songsx *song_sxlookup(struct song *o, char *name);
void           song_sxdel(struct song *o, struct songsx *t);

void song_getcursx(struct song *o, struct songsx **r);
void song_setcursx(struct song *o, struct songsx *x);
void song_getcurtrk(struct song *o, struct songtrk **r);
void song_setcurtrk(struct song *o, struct songtrk *t);
void song_getcurfilt(struct song *o, struct songfilt **r);
void song_setcurfilt(struct song *o, struct songfilt *f);
void song_getcurchan(struct song *o, struct songchan **r);
void song_setcurchan(struct song *o, struct songchan *c);
void song_getcurinput(struct song *o, unsigned *dev, unsigned *ch);
void song_setcurinput(struct song *o, unsigned dev, unsigned ch);

unsigned song_measuretotic(struct song *o, unsigned);

void song_playconf(struct song *o);
void song_nexttic(struct song *o);
void song_playtic(struct song *o);
unsigned song_finished(struct song *o);
void song_record(struct song *o);
void song_play(struct song *o);
void song_idle(struct song *o);

void song_rt_setup(struct song *o);
void song_rt_seek(struct song *o, unsigned rewind);

extern unsigned song_debug;

#endif /* MIDISH_SONG_H */
