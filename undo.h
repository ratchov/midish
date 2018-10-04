/*
 * Copyright (c) 2018 Alexandre Ratchov <alex@caoua.org>
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

#ifndef MIDISH_UNDO_H
#define MIDISH_UNDO_H

#include "track.h"
#include "sysex.h"

struct songtrk;
struct songchan;
struct songfilt;
struct songsx;

enum {
	UNDO_TRACK,
	UNDO_TREN,
	UNDO_TDEL,
	UNDO_TNEW,
	UNDO_FILT,
	UNDO_FREN,
	UNDO_FDEL,
	UNDO_FNEW,
	UNDO_CREN,
	UNDO_CDEL,
	UNDO_CNEW,
	UNDO_XADD,
	UNDO_XRM,
	UNDO_XDEL,
	UNDO_XNEW,
	UNDO_XREN,
	UNDO_XSETD,
	UNDO_CSET
};

struct undo {
	struct undo *next;
	int type;
	char *func;
	char *name;
	unsigned size;
	union {
		struct undo_track {
			struct track *track;
			struct track_data data;
		} track;
		struct undo_tdel {
			struct song *song;
			struct songtrk *trk;
			struct track_data data;
		} tdel;
		struct undo_tren {
			struct songtrk *trk;
			char *name;
		} tren;
		struct undo_filt {
			struct filt *filt;
			struct filt data;
		} filt;
		struct undo_fdel {
			struct song *song;
			struct songfilt *filt;
			struct undo_fdel_trk {
				struct undo_fdel_trk *next;
				struct songtrk *trk;
			} *trks;
		} fdel;
		struct undo_fren {
			struct songfilt *filt;
			char *name;
		} fren;
		struct undo_cdel {
			struct song *song;
			struct songchan *chan;
			struct track_data data;
		} cdel;
		struct undo_cren {
			struct songchan *chan;
			char *name;
		} cren;
		struct undo_cset {
			struct songchan *chan;
			unsigned int dev, ch;
		} cset;
		struct undo_sysex {
			struct sysexlist *list;
			struct sysex_data data;
		} sysex;
		struct undo_xdel {
			struct song *song;
			struct songsx *sx;
		} xdel;
		struct undo_xren {
			struct songsx *sx;
			char *name;
		} xren;
	} u;
};

void undo_pop(struct song *);
void undo_push(struct song *, struct undo *);
void undo_clear(struct song *, struct undo **);
void undo_track_save(struct song *, struct track *, char *, char *);
void undo_track_diff(struct song *);
void undo_tren_do(struct song *, struct songtrk *, char *, char *);
void undo_tdel_do(struct song *, struct songtrk *, char *);
struct songtrk *undo_tnew_do(struct song *, char *, char *);
void undo_filt_save(struct song *, struct filt *, char *, char *);

void undo_fren_do(struct song *s, struct songfilt *, char *, char *);
void undo_fdel_do(struct song *, struct songfilt *, char *);
struct songfilt *undo_fnew_do(struct song *, char *, char *);

void undo_cren_do(struct song *, struct songchan *, char *, char *);
struct songchan *undo_cnew_do(struct song *, unsigned int, unsigned int, int,
	char *, char *);
void undo_cdel_do(struct song *, struct songchan *, char *);
void undo_cset_do(struct song *, struct songchan *, char *,
	unsigned int, unsigned int);

void undo_xadd_do(struct song *, char *, struct songsx *, struct sysex *);
void undo_xrm_do(struct song *, char *, struct songsx *, unsigned int);
void undo_xdel_do(struct song *, char *, struct songsx *);
struct songsx *undo_xnew_do(struct song *, char *, char *);
void undo_xren_do(struct song *, struct songsx *, char *, char *);
void undo_xsetd_do(struct song *, char *, struct songsx *, 
	unsigned int, unsigned int);

#endif /* MIDISH_UNDO_H */
