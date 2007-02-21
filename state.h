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

#ifndef MIDISH_STATE_H
#define MIDISH_STATE_H

#undef STATE_DEBUG
#undef STATE_PROF

#include "ev.h"

struct seqev;

struct state  {
	struct state *next, **prev;	/* for statelist */
	struct ev ev;			/* last event */
	unsigned phase;			/* current phase (of the 'ev' field) */
	unsigned char b2;		/* 7bit "hi" bits of 14bit CTLs */

	/*
	 * the state may contain a copy of the last bank
	 * controller (for prog-change states) and the last
	 * RPN/NRPN controller (for data entries).
	 */
	unsigned char ctx_b0;		/* a copy of ev.b0 of the context */
	unsigned char ctx_b1;		/* a copy of ev.b1 of the context */
	unsigned char ctx_b2;		/* a copy of b2 of the context */

	/*
	 * the following flags are set by statelist_update() and
	 * statelist_outdate() and can be read by other routines,
	 * but shouldn't be changed
	 */
#define STATE_NEW	1		/* just created, never updated */
#define STATE_CHANGED	2		/* updated within the current tick */
#define STATE_BOGUS	4		/* frame detected as bogus */
#define STATE_NESTED	8		/* nested frame */
	unsigned flags;			/* bitmap of above */
	unsigned nevents;		/* number of events before timeout */

	/*
	 * the following are general purpose fields that are ignored
	 * by state_xxx() and statelist_xxx() routines. Other
	 * subsystems (seqptr, filt, ...) use them for various
	 * purposes
	 */
	unsigned tag;			/* user-defined tag */
	unsigned tic;			/* absolute tic of the FIRST event */
	struct seqev *pos;		/* pointer to the FIRST event */
};

/*
 * return true if the state depends on any context (ex: PCs may depend
 * on bank changes, lo/hi nibbles of 14bit controllers depend on each
 * other).
 */
#define STATE_HASCTX(s) ((s)->ctx_b0 != EV_CTL_UNKNOWN)

struct statelist {
	/* 
	 * instead of a simple list, we should use a hash table here,
	 * but statistics on real-life cases seem to show that lookups
	 * are very fast thanks to the state ordering (avg lookup
	 * time is around 1-2 iterations)
	 */
	struct state *first;	/* head of the state list */
	unsigned changed;	/* if changed within this tick */
#ifdef STATE_PROF
	unsigned lookup_n;	/* number of lookups */
	unsigned lookup_max;	/* max lookup time */
	unsigned lookup_time;	/* total lookup time */
#endif
};

#define STATE_REVMAX 4		/* num events state_cancel() & co return */

void	      state_pool_init(unsigned size);
void	      state_pool_done(void);
struct state *state_new(void);
void	      state_del(struct state *s);
void	      state_dbg(struct state *s);
void	      state_copyev(struct state *s, struct ev *ev, 
			   unsigned phase, struct state *ctx);
unsigned      state_match(struct state *s, struct ev *ev, struct state *st);
unsigned      state_eq(struct state *s, struct ev *ev);
unsigned      state_cancel(struct state *st, struct ev *rev);
unsigned      state_restore(struct state *st, struct ev *rev);

void	      statelist_init(struct statelist *o);
void	      statelist_done(struct statelist *o);
void	      statelist_dump(struct statelist *o);
void	      statelist_dup(struct statelist *o, struct statelist *src);
void	      statelist_add(struct statelist *o, struct state *st);
void	      statelist_rm(struct statelist *o, struct state *st);
void	      statelist_empty(struct statelist *o);
struct state *statelist_lookup(struct statelist *o, struct ev *ev);
struct state *statelist_update(struct statelist *statelist, struct ev *ev);
void	      statelist_outdate(struct statelist *o);

#endif /* MIDISH_STATE_H */
