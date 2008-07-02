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

struct songtrk;
struct songchan;
struct songfilt;
struct songsx;

struct songtrk {
	struct name name;		/* identifier + list entry */
	struct track track;		/* actual data */
	struct seqptr trackptr;		/* track pointer for RT */
	struct songfilt *curfilt;	/* source and dest. channel */
	unsigned mute;
};

struct songchan {
	struct name name;		/* identifier + list entry */
	struct track conf;		/* data to send on initialization */
	unsigned dev, ch;		/* dev/chan of the chan */
	unsigned curinput_dev;		/* defaults for filter creation */
	unsigned curinput_ch;
};

struct songfilt {
	struct name name;		/* identifier + list entry */
	struct filt filt;		/* filter rules */
	struct songchan *curchan;	/* defaults for new rules */
};

struct songsx {
	struct name name;		/* identifier + list entry */
	struct sysexlist sx;		/* list of sysex messages */
};

struct song {
	/*
	 * music-related fields that should be saved
	 */
	struct track meta;		/* tempo track */
	struct name *trklist;		/* list of tracks */
	struct name *chanlist;		/* list of channels */
	struct name *filtlist;		/* list of fiters */
	struct name *sxlist;		/* list of system exclive banks */
	unsigned tics_per_unit;		/* number of tics in an unit note */
	unsigned tempo_factor;		/* tempo := tempo * factor / 256 */
	struct songtrk *curtrk;		/* default track */
	struct songfilt *curfilt;	/* default filter */
	struct songchan *curchan;	/* default channel */
	struct songsx *cursx;		/* default sysex bank */
	unsigned curpos;		/* default position (in measures) */
	unsigned curquant;		/* default quantization step */
	unsigned curlen;		/* selection length */
	unsigned curinput_dev;		/* default input device */
	unsigned curinput_ch;		/* default midi channel */
	struct evspec curev;		/* evspec for track editing */
	struct metro metro;		/* metonome conf. */

	/*
	 * temporary variables used in real-time operations
	 */
	struct seqptr metaptr;		/* cur. pos in meta track */
	unsigned long tempo;		/* cur tempo in 24th of usec per tic */
	unsigned bpm, tpb;		/* cur time signature */
	struct track rec;		/* track being recorded */
	struct seqptr recptr;		/* cur position in 'rec' track */
	struct filt *filt;		/* cur filter */
	struct muxops *ops;		/* cur real-time operation */
	unsigned measure, beat, tic;	/* cur position (for metronome) */
#define SONG_PLAY	1
#define SONG_REC	2
	unsigned mode;			/* real-time "mode" */
	unsigned metro_mask;		/* if enable = (mask | mode) */
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
void song_sxdel(struct song *o, struct songsx *t);

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

void song_playconf(struct song *o);
void song_record(struct song *o);
void song_play(struct song *o);
void song_idle(struct song *o);
void song_stop(struct song *o);
unsigned song_try(struct song *o);

extern unsigned song_debug;

#endif /* MIDISH_SONG_H */
