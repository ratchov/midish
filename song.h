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

#ifndef SEQ_SONG_H
#define SEQ_SONG_H

#define SONG_DEFAULT_BPM	4
#define SONG_DEFAULT_TPB	24
#define SONG_DEFAULT_TEMPO	60

#include "name.h"
#include "track.h"
#include "filt.h"

struct songchan_s {
	struct name_s name;
	struct track_s conf;
	unsigned dev, ch;
};

struct songfilt_s {
	struct name_s name;
	struct filt_s filt;
};

struct songtrk_s {
	struct name_s name;			/* identifier + list entry */
	struct track_s track;			/* actual data */
	struct seqptr_s trackptr;		/* track pointer for RT */
	struct songfilt_s *curfilt;		/* source and dest. channel */
	unsigned mute;
};

struct songtrk_s *songtrk_new(char *);
void songtrk_delete(struct songtrk_s *);

struct songchan_s *songchan_new(char *);
void songchan_delete(struct songchan_s *o);

struct songfilt_s *songfilt_new(char *);
void songfilt_delete(struct songfilt_s *o);

struct song_s {
	/* music-related fields, should be saved */
	struct track_s meta;			/* tempo track */
	struct seqptr_s metaptr;
	struct songtrk_s *trklist;
	struct songchan_s *chanlist;
	struct songfilt_s *filtlist;
	unsigned tics_per_unit;			/* global time resulution */
	/* real-time parameters */
	unsigned long tempo;			/* 24th of usec per tic */
	unsigned beats_per_measure, tics_per_beat;
	struct track_s rec;
	struct seqptr_s recptr;
	struct filt_s *filt;
	void (*realtimecb)(void *addr, struct ev_s *ev);
	/* metronome stuff */
	unsigned tic, beat, measure;
	unsigned metro_enabled;
	struct ev_s metro_hi, metro_lo;
	/* defautls */
	struct songtrk_s *curtrk;
	struct songfilt_s *curfilt;
	unsigned curpos;
	unsigned curquant;
};

struct song_s *song_new(void);
void song_delete(struct song_s *o);
void song_init(struct song_s *o);
void song_done(struct song_s *o);

void song_trkadd(struct song_s *o, struct songtrk_s *t);
struct songtrk_s *song_trklookup(struct song_s *o, char *name);
unsigned song_trkrm(struct song_s *o, struct songtrk_s *t);

void song_chanadd(struct song_s *o, struct songchan_s *);
struct songchan_s *song_chanlookup(struct song_s *o, char *name);
struct songchan_s *song_chanlookup_bynum(struct song_s *o, unsigned dev, unsigned ch);
unsigned song_chanrm(struct song_s *o, struct songchan_s *c);


void song_filtadd(struct song_s *o, struct songfilt_s *i);
struct songfilt_s *song_filtlookup(struct song_s *o, char *name);
unsigned song_filtrm(struct song_s *o, struct songfilt_s *f);

unsigned song_measuretotic(struct song_s *o, unsigned);

void song_metrotic(struct song_s *o);
void song_playconf(struct song_s *o);
void song_nexttic(struct song_s *o);
void song_playtic(struct song_s *o);
unsigned song_finished(struct song_s *o);
void song_record(struct song_s *o);
void song_play(struct song_s *o);
void song_idle(struct song_s *o);

void song_rt_setup(struct song_s *o);
void song_rt_seek(struct song_s *o);

#endif /* SEQ_SONG_H */
