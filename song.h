/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
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
struct undo;

struct songtrk {
	struct name name;		/* identifier + list entry */
	struct track track;		/* actual data */
	struct seqptr *trackptr;	/* track pointer for RT */
	struct songfilt *curfilt;	/* source and dest. channel */
	unsigned mute;
};

struct songchan {
	struct name name;		/* identifier + list entry */
	struct track conf;		/* data to send on initialization */
	unsigned dev, ch;		/* dev/chan of the chan */
	int isinput;
	struct songfilt *filt;		/* default filter (output only) */
};

struct songfilt {
	struct name name;		/* identifier + list entry */
	struct filt filt;		/* filter rules */
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
	struct undo *undo;		/* list of operation to undo */
	unsigned undo_size;		/* size of all undo buffers */
	unsigned tics_per_unit;		/* number of tics in an unit note */
	unsigned tempo_factor;		/* tempo := tempo * factor / 256 */
	struct songtrk *curtrk;		/* default track */
	struct songfilt *curfilt;	/* default filter */
	struct songchan *curout;	/* default output channel */
	struct songchan *curin;		/* default input channel */
	struct songsx *cursx;		/* default sysex bank */
	unsigned curpos;		/* default position (in measures) */
	unsigned curlen;		/* selection length */
	unsigned curquant;		/* default quantization step */
	struct evspec curev;		/* evspec for track editing */
	struct metro metro;		/* metonome conf. */
	struct evspec tap_evspec;	/* event to trigger start manually */
	unsigned long tap_time;		/* timestamp of first tap */
#define SONG_TAP_OFF	0
#define SONG_TAP_START	1
#define SONG_TAP_TEMPO	2
	int tap_mode;			/* one of above */
	int tap_cnt;			/* number of taps, -1 means done */

	/*
	 * clipboard
	 */
#define CLIP_OFFS	(256 * 96)
	struct track clip;		/* tmp track for copy & paste */

	/*
	 * temporary variables used in real-time operations
	 */
	struct seqptr *metaptr;		/* cur. pos in meta track */
	unsigned long tempo;		/* cur tempo in 24th of usec per tic */
	unsigned bpm, tpb;		/* cur time signature */
	struct track rec;		/* track being recorded */
	struct seqptr *recptr;		/* cur position in 'rec' track */
	unsigned measure, beat, tic;	/* cur position (for metronome) */
#define SONG_IDLE	1		/* filter running */
#define SONG_PLAY	2		/* above + playback */
#define SONG_REC	3		/* above + recording */
	unsigned mode;			/* real-time "mode" */
	unsigned started;		/* playback started */
	unsigned complete;		/* playback completed */
	unsigned metro_mask;		/* if enable = (mask | mode) */
};

extern char *song_tap_modestr[3];

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

/*
 * how to relocate, used by song_loc() & friends
 */
#define SONG_LOC_MEAS 	0	/* measure number */
#define SONG_LOC_MTC 	1	/* MTC/MMC absolute time */
#define SONG_LOC_SPP 	2	/* MIDI song position pointer */

struct song *song_new(void);
void song_delete(struct song *);
void song_init(struct song *);
void song_done(struct song *);

struct songtrk *song_trknew(struct song *, char *);
struct songtrk *song_trklookup(struct song *, char *);
void song_trkdel(struct song *, struct songtrk *);
void song_trkmute(struct song *, struct songtrk *);
void song_trkunmute(struct song *, struct songtrk *);

struct songchan *song_channew(struct song *, char *, unsigned, unsigned, int);
struct songchan *song_chanlookup(struct song *, char *, int);
struct songchan *song_chanlookup_bynum(struct song *, unsigned, unsigned, int);
void song_confev(struct song *, struct songchan *, struct ev *);
void song_unconfev(struct song *, struct songchan *, struct evspec *);
void song_chandel(struct song *, struct songchan *);

struct songfilt *song_filtnew(struct song *, char *);
struct songfilt *song_filtlookup(struct song *, char *);
void song_filtdel(struct song *, struct songfilt *);

struct songsx *song_sxnew(struct song *, char *);
struct songsx *song_sxlookup(struct song *, char *);
void song_sxdel(struct song *, struct songsx *);

void song_getcursx(struct song *, struct songsx **);
void song_setcursx(struct song *, struct songsx *);
void song_getcurtrk(struct song *, struct songtrk **);
void song_setcurtrk(struct song *, struct songtrk *);
void song_getcurfilt(struct song *, struct songfilt **);
void song_setcurfilt(struct song *, struct songfilt *);
void song_getcurchan(struct song *, struct songchan **, int);
void song_setcurchan(struct song *, struct songchan *, int);
void song_setunit(struct song *, unsigned);
unsigned song_endpos(struct song *);
void song_playconf(struct song *);

unsigned song_loc(struct song *, unsigned, unsigned);
void song_setmode(struct song *, unsigned);
void song_goto(struct song *, unsigned);
void song_record(struct song *);
void song_play(struct song *);
void song_idle(struct song *);
void song_stop(struct song *);

unsigned song_try_mode(struct song *, unsigned);
unsigned song_try_curev(struct song *);
unsigned song_try_curpos(struct song *);
unsigned song_try_curlen(struct song *);
unsigned song_try_curquant(struct song *);
unsigned song_try_curtrk(struct song *);
unsigned song_try_curchan(struct song *, int);
unsigned song_try_curfilt(struct song *);
unsigned song_try_cursx(struct song *);
unsigned song_try_trk(struct song *, struct songtrk *);
unsigned song_try_chan(struct song *, struct songchan *, int);
unsigned song_try_filt(struct song *, struct songfilt *);
unsigned song_try_sx(struct song *, struct songsx *);
unsigned song_try_meta(struct song *);
unsigned song_try_ev(struct song *, unsigned);

extern unsigned song_debug;

#endif /* MIDISH_SONG_H */
