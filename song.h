/* $Id: song.h,v 1.18 2006/02/17 13:18:06 alex Exp $ */
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

#ifndef MIDISH_SONG_H
#define MIDISH_SONG_H

#define SONG_DEFAULT_BPM	4
#define SONG_DEFAULT_TPB	24
#define SONG_DEFAULT_TEMPO	60

#include "name.h"
#include "track.h"
#include "filt.h"
#include "sysex.h"

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
	struct name name;			/* identifier + list entry */
	struct track track;			/* actual data */
	struct seqptr trackptr;		/* track pointer for RT */
	struct songfilt *curfilt;		/* source and dest. channel */
	unsigned mute;
};

struct songsx {
	struct name name;
	struct sysexlist sx;
};

struct songtrk *songtrk_new(char *);
void songtrk_delete(struct songtrk *);

struct songchan *songchan_new(char *);
void songchan_delete(struct songchan *o);

struct songfilt *songfilt_new(char *);
void songfilt_delete(struct songfilt *o);

struct songsx *songsx_new(char *);
void songsx_delete(struct songsx *o);

struct song {
	/* music-related fields, should be saved */
	struct track meta;			/* tempo track */
	struct seqptr metaptr;
	struct songtrk *trklist;
	struct songchan *chanlist;
	struct songfilt *filtlist;
	struct songsx *sxlist;
	unsigned tics_per_unit;			/* global time resulution */
	/* real-time parameters */
	unsigned long tempo;			/* 24th of usec per tic */
	unsigned beats_per_measure, tics_per_beat;
	struct track rec;
	struct seqptr recptr;
	struct filt *filt;
	void (*realtimecb)(void *addr, struct ev *ev);
	/* metronome stuff */
	unsigned tic, beat, measure;
	unsigned metro_enabled;
	struct ev metro_hi, metro_lo;
	/* defautls */
	struct songtrk *curtrk;
	struct songfilt *curfilt;
	struct songchan *curchan;
	struct songsx *cursx;
	unsigned curpos;
	unsigned curquant, curlen;
	unsigned curinput_dev, curinput_ch;		
};

struct song *song_new(void);
void song_delete(struct song *o);
void song_init(struct song *o);
void song_done(struct song *o);

void song_trkadd(struct song *o, struct songtrk *t);
struct songtrk *song_trklookup(struct song *o, char *name);
unsigned song_trkrm(struct song *o, struct songtrk *t);

void song_chanadd(struct song *o, struct songchan *);
struct songchan *song_chanlookup(struct song *o, char *name);
struct songchan *song_chanlookup_bynum(struct song *o, unsigned dev, unsigned ch);
unsigned song_chanrm(struct song *o, struct songchan *c);

void song_filtadd(struct song *o, struct songfilt *i);
struct songfilt *song_filtlookup(struct song *o, char *name);
unsigned song_filtrm(struct song *o, struct songfilt *f);

void song_sxadd(struct song *o, struct songsx *t);
struct songsx *song_sxlookup(struct song *o, char *name);
unsigned song_sxrm(struct song *o, struct songsx *t);

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

void song_metrotic(struct song *o);
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
