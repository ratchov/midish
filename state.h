/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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

#define STATE_DEBUG
#define STATE_PROF

#include "ev.h"
#include "track.h"

struct state  {
	struct ev ev;			/* initial event */
	struct state *next, **prev;
	union {
		struct {
			unsigned nevents, keep;
		} filt;
		struct {
			unsigned tic;
			struct seqev *pos;
		} frame;
	} data;
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
void	      statelist_add(struct statelist *o, struct state *st);
void	      statelist_rm(struct statelist *o, struct state *st);
struct state *statelist_lookup(struct statelist *o, struct ev *ev);


#endif /* MIDISH_STATE_H */
