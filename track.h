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

struct track_data {
	struct seqev_data {
		unsigned delta;
		struct ev ev;
	} *evs;
	unsigned int pos, nrm, nins;
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
void	      track_swap(struct track *, struct track *);

unsigned      seqev_avail(struct seqev *);
void	      seqev_ins(struct seqev *, struct seqev *);
void	      seqev_rm(struct seqev *);

void	      track_setchan(struct track *, unsigned, unsigned);
void	      track_chanmap(struct track *, char *);
unsigned      track_evcnt(struct track *, unsigned);

unsigned track_undosave(struct track *, struct track_data *);
unsigned track_undodiff(struct track *, struct track_data *);
void track_undorestore(struct track *, struct track_data *);

#endif /* MIDISH_TRACK_H */
