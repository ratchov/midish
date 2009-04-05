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

void	      seqptr_pool_init(unsigned);
void	      seqptr_pool_done(void);
struct seqptr *seqptr_new(struct track *);
void	      seqptr_del(struct seqptr *);
struct state *seqptr_evget(struct seqptr *);
struct state *seqptr_evdel(struct seqptr *, struct statelist *);
struct state *seqptr_evput(struct seqptr *, struct ev *);
unsigned      seqptr_ticskip(struct seqptr *, unsigned);
unsigned      seqptr_ticdel(struct seqptr *, unsigned,
			    struct statelist *);
void	      seqptr_ticput(struct seqptr *, unsigned);
unsigned      seqptr_skip(struct seqptr *, unsigned);
void	      seqptr_seek(struct seqptr *, unsigned);
struct state *seqptr_getsign(struct seqptr *, unsigned *, unsigned *);
struct state *seqptr_gettempo(struct seqptr *, unsigned long *);
unsigned      seqptr_skipmeasure(struct seqptr *, unsigned);

void	 track_merge(struct track *, struct track *);
unsigned track_findmeasure(struct track *, unsigned);
void	 track_timeinfo(struct track *, unsigned, unsigned *,
			unsigned long *, unsigned *, unsigned *);
void     track_settempo(struct track *, unsigned, unsigned);
void     track_move(struct track *, unsigned, unsigned,
		    struct evspec *, struct track *,
		    unsigned, unsigned);
void     track_quantize(struct track *, unsigned, unsigned,
			unsigned, unsigned, unsigned);
void     track_scale(struct track *, unsigned, unsigned);
void     track_transpose(struct track *,
			 unsigned, unsigned, struct evspec *, int);
void	 track_check(struct track *);
void     track_confev(struct track *, struct ev *);
void	 track_unconfev(struct track *, struct evspec *);
void	 track_ins(struct track *, unsigned, unsigned);
void	 track_cut(struct track *, unsigned, unsigned);

#endif /* MIDISH_FRAME_H */
