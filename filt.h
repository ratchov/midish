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

#ifndef MIDISH_FILT_H
#define MIDISH_FILT_H

#include "ev.h"

/*
 * source against which the input event is matched
 */
struct filtnode {
	struct evspec es;		/* events handled by this branch */
	struct filtnode *dstlist;	/* destinations for this source */
	struct filtnode *next;		/* next source in the list */
	union {
		struct {
			unsigned nweight;
		} vel;
		struct {
			int plus;
		} transp;
	} u;
};

#define FILT_MAXNRULES 32

struct filt {
	struct filtnode *map;		/* root of map rules */
	struct filtnode *vcurve;	/* root of vcurve rules */
	struct filtnode *transp;	/* root of transp rules */
};

void filt_init(struct filt *);
void filt_done(struct filt *);
void filt_reset(struct filt *);
unsigned filt_do(struct filt *, struct ev *, struct ev *);
void filt_mapnew(struct filt *, struct evspec *, struct  evspec *);
void filt_mapdel(struct filt *, struct evspec *, struct  evspec *);
void filt_chgin(struct filt *, struct evspec *, struct evspec *, int);
void filt_chgout(struct filt *, struct evspec *, struct evspec *, int);
void filt_transp(struct filt *, struct evspec *, int);
void filt_vcurve(struct filt *, struct evspec *, int);
unsigned filt_evcnt(struct filt *, unsigned);

extern unsigned filt_debug;

#endif /* MIDISH_FILT_H */
