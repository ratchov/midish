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
	struct ev ev;			/* initial event */
	struct state *next, **prev;	/* for statelist */
#define STATE_NEW	1		/* just created, never updated */
#define STATE_TOUCHED	2		/* updated within the current tick */
#define STATE_BOGUS	4		/* frame detected as bogus */
	unsigned flags;
	unsigned tic;			/* absolute tic of the FIRST event */
	struct seqev *pos;		/* pointer to the FIRST event */
	unsigned phase;			/* current phase (of the 'ev' field) */

	unsigned silent;		/* events shoudn't be played */
	unsigned keep;			/* shouldn't be purged */
	unsigned nevents;		/* number of events before timeout */
};

struct statelist {
	/* 
	 * XXX: use a hash table instead of simple list
	 */
	struct state *first;
#ifdef STATE_PROF
	unsigned lookup_n, lookup_max, lookup_time;
#endif
};

void	      state_pool_init(unsigned size);
void	      state_pool_done(void);
struct state *state_new(void);
void	      state_del(struct state *s);
void	      statelist_init(struct statelist *o);
void	      statelist_done(struct statelist *o);
void	      statelist_dup(struct statelist *o, struct statelist *src);
void	      statelist_add(struct statelist *o, struct state *st);
void	      statelist_rm(struct statelist *o, struct state *st);
void	      statelist_empty(struct statelist *o);
struct state *statelist_lookup(struct statelist *o, struct ev *ev);
struct state *statelist_update(struct statelist *statelist, struct ev *ev);
void	      statelist_keep(struct statelist *o);
void	      statelist_diff(struct statelist *nl, struct statelist *ol);


#endif /* MIDISH_STATE_H */
