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

#ifndef MIDISH_FILT_H
#define MIDISH_FILT_H

#include "ev.h"

/*
 * destination to which the output event is mapped
 */
struct filtdst {
	struct evspec es;
	struct filtdst *next;
};

/*
 * source against which the input event is matched
 */
struct filtsrc {
	struct evspec es;		/* events by this entry */
	struct filtdst *dstlist;	/* destinations for this source */
	struct filtsrc *next;		/* next source in the list */
};

#define FILT_MAXNRULES 32

struct filt {
	struct filtsrc *srclist;
};

void filt_init(struct filt *);
void filt_done(struct filt *);
void filt_reset(struct filt *);
unsigned filt_do(struct filt *, struct ev *, struct ev *);
void filt_mapnew(struct filt *, struct evspec *, struct  evspec *);
void filt_mapdel(struct filt *, struct evspec *, struct  evspec *);
void filt_chgin(struct filt *, struct evspec *, struct evspec *, int);
void filt_chgout(struct filt *, struct evspec *, struct evspec *, int);

extern unsigned filt_debug;

void filt_conf_devdrop(struct filt *, unsigned);
void filt_conf_nodevdrop(struct filt *, unsigned);
void filt_conf_devmap(struct filt *, unsigned, unsigned);
void filt_conf_nodevmap(struct filt *, unsigned);
void filt_conf_chandrop(struct filt *, unsigned, unsigned);
void filt_conf_nochandrop(struct filt *, unsigned, unsigned);
void filt_conf_chanmap(struct filt *, unsigned, unsigned, unsigned, unsigned);
void filt_conf_nochanmap(struct filt *, unsigned, unsigned );
void filt_conf_ctldrop(struct filt *, unsigned, unsigned, unsigned);
void filt_conf_noctldrop(struct filt *, unsigned, unsigned, unsigned);
void filt_conf_ctlmap(struct filt *, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
void filt_conf_noctlmap(struct filt *, unsigned, unsigned, unsigned);
void filt_conf_keydrop(struct filt *, unsigned, unsigned, unsigned, unsigned);
void filt_conf_nokeydrop(struct filt *, unsigned, unsigned, unsigned, unsigned);
void filt_conf_keymap(struct filt *, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, int);
void filt_conf_nokeymap(struct filt *, unsigned, unsigned, unsigned, unsigned);
void filt_conf_chgich(struct filt *, unsigned, unsigned, unsigned, unsigned);
void filt_conf_chgidev(struct filt *, unsigned, unsigned);
void filt_conf_swapich(struct filt *, unsigned, unsigned, unsigned, unsigned);
void filt_conf_swapidev(struct filt *, unsigned, unsigned);
void filt_conf_chgoch(struct filt *, unsigned, unsigned, unsigned, unsigned);
void filt_conf_chgodev(struct filt *, unsigned, unsigned);
void filt_conf_swapoch(struct filt *, unsigned, unsigned, unsigned, unsigned);
void filt_conf_swapodev(struct filt *, unsigned, unsigned);

#endif /* MIDISH_FILT_H */
