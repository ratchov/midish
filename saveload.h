/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
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
