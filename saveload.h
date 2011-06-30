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

#ifndef MIDISH_SAVELOAD_H
#define MIDISH_SAVELOAD_H

struct textout;
struct ev;
struct evctl;
struct track;
struct filt;
struct rule;
struct songchan;
struct songtrk;
struct songfilt;
struct songsx;
struct song;

void ev_output(struct ev *, struct textout *);
void evspec_output(struct evspec *, struct textout *);
void track_output(struct track *, struct textout *);
void rule_output(struct rule *, struct textout *);
void filt_output(struct filt *, struct textout *);
void songtrk_output(struct songtrk *, struct textout *);
void songchan_output(struct songchan *, struct textout *);
void songfilt_output(struct songfilt *, struct textout *);
void songsx_output(struct songsx *, struct textout *);
void evctltab_output(struct evctl *, struct textout *);
void song_output(struct song *, struct textout *);

void track_save(struct track *, char *);
unsigned track_load(struct track *, char *);

void song_save(struct song *, char *);
unsigned song_load(struct song *, char *);


#endif /* MIDISH_SAVELOAD_H */
