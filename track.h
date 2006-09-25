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

#ifndef MIDISH_TRACK_H
#define MIDISH_TRACK_H

#define	TRACK_DEBUG

#include "ev.h"

struct seqev {
	unsigned delta;
	struct ev ev;
	struct seqev *next;
};

struct track {
	struct seqev *first;		/* head of the list */
	struct seqev eot;		/* the end-of-track event */
};

struct seqptr {
	struct seqev **pos;		/* ptr to a ptr to the next event */
	unsigned delta;
};

void		seqev_pool_init(unsigned size);
void		seqev_pool_done(void);
struct seqev *seqev_new(void);
void		seqev_del(struct seqev *p);	
void		seqev_dump(struct seqev *i);

struct track *track_new(void);
void		track_delete(struct track *o);
void		track_init(struct track *o);
void		track_done(struct track *o);
void		track_dump(struct track *o);
unsigned	track_numev(struct track *o);
unsigned	track_numtic(struct track *o);

void		track_seqevins(struct track *o, struct seqptr *p, struct seqev *se);
struct seqev *track_seqevrm(struct track *o, struct seqptr *p);
unsigned	track_seqevnext(struct track *o, struct seqptr *p);
unsigned	track_seqevavail(struct track *o, struct seqptr *p);

unsigned	track_evavail(struct track *o, struct seqptr *p);
void		track_evnext(struct track *o, struct seqptr *p);
void		track_evlast(struct track *o, struct seqptr *p);
void		track_evget(struct track *o, struct seqptr *p, struct ev *ev);
void		track_evput(struct track *o, struct seqptr *p, struct ev *ev);
void		track_evdel(struct track *o, struct seqptr *p);
void		track_evinsat(struct track *o, struct seqptr *p, struct ev *ev, unsigned ntics);

unsigned	track_ticavail(struct track *o, struct seqptr *p);
void		track_ticnext(struct track *o, struct seqptr *p);
unsigned	track_ticlast(struct track *o, struct seqptr *p);
unsigned	track_ticskipmax(struct track *o, struct seqptr *p, unsigned max);
unsigned	track_ticdelmax(struct track *o, struct seqptr *p, unsigned max);
void	        track_ticinsmax(struct track *o, struct seqptr *p, unsigned max);
void		track_ticdel(struct track *o, struct seqptr *p);
void		track_ticins(struct track *o, struct seqptr *p);

void		track_clear(struct track *o, struct seqptr *p);
void		track_rew(struct track *o, struct seqptr *p);
unsigned	track_finished(struct track *o, struct seqptr *p);
unsigned	track_seek(struct track *o, struct seqptr *p, unsigned ntics);
void		track_seekblank(struct track *o, struct seqptr *p, unsigned ntics);
unsigned	track_filt(struct track *o, struct seqptr *p, unsigned ntics,
			unsigned (*func)(void *, struct ev *), void *addr);

#endif /* MIDISH_TRACK_H */
