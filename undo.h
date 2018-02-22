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

struct songtrk;
struct songchan;
struct songfilt;
struct songsx;

struct undo {
	struct undo *next;
#define UNDO_TDATA	1
#define UNDO_TREN	2
#define UNDO_TDEL	3
#define UNDO_TNEW	4
#define UNDO_CDATA	5
	int type;
	char *func;
	char *name;
	unsigned size;
	union {
		struct undo_tdata {
			struct songtrk *trk;
			struct track_data data;
		} tdata;
		struct undo_tren {
			struct songtrk *trk;
			char *name;
		} tren;
		struct undo_chan {
			struct songchan *chan;
			struct track_data data;
		} cdata;
	} u;
};

void undo_pop(struct song *);
void undo_push(struct song *, struct undo *);
void undo_clear(struct song *, struct undo **);
void undo_tdata_save(struct song *, struct songtrk *, char *);
void undo_tdata_diff(struct song *);
void undo_cdata_save(struct song *, struct songchan *, char *);
void undo_tren_do(struct song *, struct songtrk *, char *, char *);
void undo_tdel_do(struct song *, struct songtrk *, char *);
struct songtrk *undo_tnew_do(struct song *, char *, char *);

#endif /* MIDISH_UNDO_H */
