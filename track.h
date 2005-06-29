/*
 * Copyright (c) 2003-2005 Alexandre Ratchov
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

#ifndef MIDISH_TRACK_H
#define MIDISH_TRACK_H

#define	TRACK_DEBUG

#include "ev.h"

struct seqev_s {
	unsigned delta;
	struct ev_s ev;
	struct seqev_s *next;
};

struct track_s {
	unsigned flags;			/* cyclic, ... */
	struct seqev_s *first;		/* head of the list */
	struct seqev_s eot;		/* the end-of-track event */
};

struct seqptr_s {
	struct seqev_s **pos;		/* ptr to a ptr to the next event */
	unsigned delta;
};

void		seqev_pool_init(unsigned size);
void		seqev_pool_done(void);
struct seqev_s *seqev_new(void);
void		seqev_del(struct seqev_s *p);	
void		seqev_dump(struct seqev_s *i);

struct track_s *track_new(void);
void		track_delete(struct track_s *o);
void		track_init(struct track_s *o);
void		track_done(struct track_s *o);
void		track_dump(struct track_s *o);
unsigned	track_numev(struct track_s *o);
unsigned	track_numtic(struct track_s *o);

void		track_seqevins(struct track_s *o, struct seqptr_s *p, struct seqev_s *se);
struct seqev_s *track_seqevrm(struct track_s *o, struct seqptr_s *p);
unsigned	track_seqevnext(struct track_s *o, struct seqptr_s *p);
unsigned	track_seqevavail(struct track_s *o, struct seqptr_s *p);

unsigned	track_evavail(struct track_s *o, struct seqptr_s *p);
void		track_evnext(struct track_s *o, struct seqptr_s *p);
void		track_evlast(struct track_s *o, struct seqptr_s *p);
void		track_evget(struct track_s *o, struct seqptr_s *p, struct ev_s *ev);
void		track_evput(struct track_s *o, struct seqptr_s *p, struct ev_s *ev);
void		track_evdel(struct track_s *o, struct seqptr_s *p);
void		track_evinsat(struct track_s *o, struct seqptr_s *p, struct ev_s *ev, unsigned ntics);

unsigned	track_ticavail(struct track_s *o, struct seqptr_s *p);
void		track_ticnext(struct track_s *o, struct seqptr_s *p);
unsigned	track_ticlast(struct track_s *o, struct seqptr_s *p);
void		track_ticdel(struct track_s *o, struct seqptr_s *p);
void		track_ticins(struct track_s *o, struct seqptr_s *p);

void		track_clear(struct track_s *o, struct seqptr_s *p);
void		track_rew(struct track_s *o, struct seqptr_s *p);
unsigned	track_finished(struct track_s *o, struct seqptr_s *p);
unsigned	track_seek(struct track_s *o, struct seqptr_s *p, unsigned ntics);
void		track_seekblank(struct track_s *o, struct seqptr_s *p, unsigned ntics);
unsigned	track_filt(struct track_s *o, struct seqptr_s *p, unsigned ntics,
			unsigned (*func)(void *, struct ev_s *), void *addr);

#endif /* MIDISH_TRACK_H */
