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

#ifndef MIDISH_STATE_H
#define MIDISH_STATE_H

#include "ev.h"
#include "utils.h"

struct seqev;
struct statelist;

struct state  {
	struct state *next, **prev;	/* for statelist */
	struct ev ev;			/* last event */
	unsigned phase;			/* current phase (of the 'ev' field) */
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
	 * subsystems (seqptr, filt, ...) use them privately for
	 * various purposes. See specific modules to get their various
	 * significances.
	 */
	unsigned tag;			/* user-defined tag */
	unsigned tic;			/* absolute tic of the FIRST event */
	struct seqev *pos;		/* pointer to the FIRST event */
};

struct statelist {
	/*
	 * instead of a simple list, we should use a hash table here,
	 * but statistics on real-life cases seem to show that lookups
	 * are very fast thanks to the state ordering (average lookup
	 * time is around 1-2 iterations for a common MIDI file), so
	 * we keep using a simple list
	 */
	struct state *first;	/* head of the state list */
	unsigned changed;	/* if changed within this tick */
	unsigned serial;	/* unique ID */
#ifdef STATE_PROF
	struct prof prof;
#endif
};

void	      state_pool_init(unsigned);
void	      state_pool_done(void);
struct state *state_new(void);
void	      state_del(struct state *);
size_t	      state_fmt(char *, size_t, struct state *);
void	      state_copyev(struct state *, struct ev *, unsigned);
unsigned      state_match(struct state *, struct ev *);
unsigned      state_inspec(struct state *, struct evspec *);
unsigned      state_eq(struct state *, struct ev *);
unsigned      state_cancel(struct state *, struct ev *);
unsigned      state_restore(struct state *, struct ev *);

void	      statelist_init(struct statelist *);
void	      statelist_done(struct statelist *);
void	      statelist_dump(struct statelist *);
void	      statelist_dup(struct statelist *, struct statelist *);
void	      statelist_empty(struct statelist *);
void	      statelist_add(struct statelist *, struct state *);
void	      statelist_rm(struct statelist *, struct state *);
struct state *statelist_lookup(struct statelist *, struct ev *);
struct state *statelist_update(struct statelist *, struct ev *);
void	      statelist_outdate(struct statelist *);

#endif /* MIDISH_STATE_H */
