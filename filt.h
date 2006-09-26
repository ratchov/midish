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

#ifndef MIDISH_FILT_H
#define MIDISH_FILT_H

#include "state.h"

#define FILT_DEBUG

#define RULE_CTLMAP	1
#define RULE_CTLDROP	2
#define RULE_KEYMAP	3
#define RULE_KEYDROP	4
#define RULE_CHANMAP	5
#define RULE_CHANDROP	6
#define RULE_DEVMAP	7
#define RULE_DEVDROP	8

struct rule {
	struct rule *next;
	unsigned type;
	unsigned idev, odev;
	unsigned ich, och;
	unsigned ictl, octl;
	unsigned keylo, keyhi;
	int keyplus;
	unsigned char *curve;
};

struct filt {
	/* config */
	struct rule *voice_rules, *chan_rules, *dev_rules;
	
	/* real-time stuff*/
	unsigned active;
	void (*cb)(void *, struct ev *);
	void *addr;
	struct statelist statelist;
	
};

void filt_init(struct filt *);
void filt_done(struct filt *);
void filt_reset(struct filt *);
void filt_start(struct filt *, void (*)(void *, struct ev *), void *);
void filt_shut(struct filt *);
void filt_stop(struct filt *);
void filt_evcb(struct filt *o, struct ev *ev);
void filt_timercb(struct filt *o);
void filt_processev(struct filt *o, struct ev *ev);

struct state *filt_statecreate(struct filt *o, struct ev *ev, unsigned keep);
struct state *filt_statelookup(struct filt *o, struct ev *ev);
void filt_stateshut(struct filt *o, struct state *p);
void filt_stateupdate(struct filt *o, struct state *p, struct ev *ev, unsigned keep);

void filt_conf_devdrop(struct filt *o, unsigned idev);
void filt_conf_nodevdrop(struct filt *o, unsigned idev);
void filt_conf_devmap(struct filt *o, unsigned idev, unsigned odev);
void filt_conf_nodevmap(struct filt *o, unsigned odev);
void filt_conf_chandrop(struct filt *o, unsigned idev, unsigned ich);
void filt_conf_nochandrop(struct filt *o, unsigned idev, unsigned ich);
void filt_conf_chanmap(struct filt *o, unsigned idev, unsigned ich, unsigned odev, unsigned och);
void filt_conf_nochanmap(struct filt *o, unsigned odev, unsigned och );
void filt_conf_ctldrop(struct filt *o, unsigned idev, unsigned ich, unsigned ictl);
void filt_conf_noctldrop(struct filt *o, unsigned idev, unsigned ich, unsigned ictl);
void filt_conf_ctlmap(struct filt *o, unsigned idev, unsigned ich, unsigned odev, unsigned och, unsigned ictl, unsigned octl);
void filt_conf_noctlmap(struct filt *o, unsigned odev, unsigned och, unsigned octl);
void filt_conf_keydrop(struct filt *o, unsigned idev, unsigned ich, unsigned keylo, unsigned keyhi);
void filt_conf_nokeydrop(struct filt *o, unsigned idev, unsigned ich, unsigned keylo, unsigned keyhi);
void filt_conf_keymap(struct filt *o, unsigned idev, unsigned ich, unsigned odev, unsigned och, unsigned keylo, unsigned keyhi, int keyplus);
void filt_conf_nokeymap(struct filt *o, unsigned odev, unsigned och, unsigned keylo, unsigned keyhi);
void filt_conf_chgich(struct filt *o, unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch);
void filt_conf_chgidev(struct filt *o, unsigned olddev, unsigned newdev);
void filt_conf_swapich(struct filt *o, unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch);
void filt_conf_swapidev(struct filt *o, unsigned olddev, unsigned newdev);
void filt_conf_chgoch(struct filt *o, unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch);
void filt_conf_chgodev(struct filt *o, unsigned olddev, unsigned newdev);
void filt_conf_swapoch(struct filt *o, unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch);
void filt_conf_swapodev(struct filt *o, unsigned olddev, unsigned newdev);

extern unsigned filt_debug;

#endif /* MIDISH_FILT_H */
