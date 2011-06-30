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
void     track_transpose(struct track *, unsigned, unsigned,
	 struct evspec *, int);
void	 track_evmap(struct track *, unsigned, unsigned, 
	 struct evspec *, struct evspec *, struct evspec *);
void	 track_check(struct track *);
void     track_confev(struct track *, struct ev *);
void	 track_unconfev(struct track *, struct evspec *);
void	 track_ins(struct track *, unsigned, unsigned);
void	 track_cut(struct track *, unsigned, unsigned);

#endif /* MIDISH_FRAME_H */
