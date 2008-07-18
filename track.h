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

#ifndef MIDISH_TRACK_H
#define MIDISH_TRACK_H

#include "ev.h"

struct seqev {
	unsigned delta;
	struct ev ev;
	struct seqev *next, **prev;
};

struct track {
	struct seqev eot;		/* end-of-track event */
	struct seqev *first;		/* head of the event list */
};

void	      seqev_pool_init(unsigned);
void	      seqev_pool_done(void);
struct seqev *seqev_new(void);
void	      seqev_del(struct seqev *);
void	      seqev_dump(struct seqev *);

void	      track_init(struct track *);
void	      track_done(struct track *);
void	      track_dump(struct track *);
unsigned      track_numev(struct track *);
unsigned      track_numtic(struct track *);
void	      track_clear(struct track *);
unsigned      track_isempty(struct track *);
void	      track_chomp(struct track *);
void	      track_shift(struct track *, unsigned);

unsigned      seqev_avail(struct seqev *);
void	      seqev_ins(struct seqev *, struct seqev *);
void	      seqev_rm(struct seqev *);

void	      track_setchan(struct track *, unsigned, unsigned);
void	      track_chanmap(struct track *, char *);

#endif /* MIDISH_TRACK_H */
