/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
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

extern unsigned filt_debug;

#endif /* MIDISH_FILT_H */
