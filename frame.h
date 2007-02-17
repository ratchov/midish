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

#ifndef MIDISH_FRAME_H
#define MIDISH_FRAME_H

#include "state.h"

struct seqptr {
	struct statelist statelist;
	struct seqev *pos;		/* next event (current position) */
	unsigned delta;			/* tics until the next event */	 
	unsigned tic;			/* absolute tic of the current pos */
};

struct track;
struct evspec;

void	      seqptr_init(struct seqptr *sp, struct track *t);
void	      seqptr_done(struct seqptr *sp);
struct state *seqptr_evget(struct seqptr *sp);
struct state *seqptr_evdel(struct seqptr *sp, struct statelist *slist);
struct state *seqptr_evput(struct seqptr *sp, struct ev *ev);
unsigned      seqptr_ticskip(struct seqptr *sp, unsigned max);
unsigned      seqptr_ticdel(struct seqptr *sp, unsigned max, 
			    struct statelist *slist);
void	      seqptr_ticput(struct seqptr *sp, unsigned ntics);
unsigned      seqptr_skip(struct seqptr *sp, unsigned ntics);
void	      seqptr_seek(struct seqptr *sp, unsigned ntics);
struct state *seqptr_getsign(struct seqptr *sp, unsigned *bpm, unsigned *tpb);
struct state *seqptr_gettempo(struct seqptr *sp, unsigned long *usec24);

unsigned track_findmeasure(struct track *t, unsigned m0);
void	 track_timeinfo(struct track *t, unsigned meas, unsigned *tic, 
			unsigned long *usec24, unsigned *bpm, unsigned *tpb);
void	 track_merge(struct track *dst, struct track *src);
void     track_settempo(struct track *t, unsigned measure, unsigned tempo);
void     track_timeins(struct track *t, unsigned measure, unsigned amount,
		       unsigned bpm, unsigned tpb);
void     track_timerm(struct track *t, unsigned measure, unsigned amount);
void	 track_insert(struct track *t, unsigned start, unsigned len);
void	 track_copy(struct track *src, unsigned start, unsigned len, 
		    struct evspec *es, struct track *dst);
void	 track_blank(struct track *src, unsigned start, 
		     unsigned len, struct evspec *es);
void     track_move(struct track *src, unsigned start, unsigned len, 
		    struct evspec *evspec, struct track *dst, 
		    unsigned copy, unsigned blank);
void     track_quantize(struct track *src, unsigned start, unsigned len, 
			unsigned offset, unsigned quantum, unsigned rate);
void     track_transpose(struct track *src, unsigned start, unsigned len, 
			 int halftones);
void	 track_check(struct track *src);
void     track_confev(struct track *src, struct ev *ev);

#endif /* MIDISH_FRAME_H */
