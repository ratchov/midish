/* $Id$ */
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

#ifndef MIDISH_FILT_H
#define MIDISH_FILT_H

#define FILT_DEBUG

#include "ev.h"

struct state_s {
	struct ev_s ev;			/* initial event */
	unsigned nevents, keep;
	struct state_s *next;
};

#define RULE_CTLMAP	1
#define RULE_CTLDROP	2
#define RULE_KEYMAP	3
#define RULE_KEYDROP	4
#define RULE_CHANMAP	5
#define RULE_CHANDROP	6
#define RULE_DEVMAP	7
#define RULE_DEVDROP	8

struct rule_s {
	struct rule_s *next;
	unsigned type;
	unsigned idev, odev;
	unsigned ich, och;
	unsigned ictl, octl;
	unsigned keylo, keyhi;
	int keyplus;
	unsigned char *curve;
};

struct filt_s {
	/* config */
	struct rule_s *voice_rules, *chan_rules, *dev_rules;
	
	/* real-time stuff*/
	unsigned active;
	void (*cb)(void *, struct ev_s *);
	void *addr;
	struct state_s *statelist;
	
};

void state_pool_init(unsigned size);
void state_pool_done(void);

void filt_init(struct filt_s *);
void filt_done(struct filt_s *);
void filt_reset(struct filt_s *);
void filt_start(struct filt_s *, void (*)(void *, struct ev_s *), void *);
void filt_shut(struct filt_s *);
void filt_stop(struct filt_s *);

void filt_evcb(struct filt_s *o, struct ev_s *ev);
void filt_timercb(struct filt_s *o);

struct state_s  *state_new(void);
void             state_del(struct state_s *s);

void		 filt_processev(struct filt_s *o, struct ev_s *ev);

struct state_s **filt_statecreate(struct filt_s *o, struct ev_s *ev, unsigned keep);
struct state_s **filt_statelookup(struct filt_s *o, struct ev_s *ev);
struct state_s **filt_stateshut(struct filt_s *o, struct state_s **p);
struct state_s **filt_stateupdate(struct filt_s *o, struct state_s **p, struct ev_s *ev, unsigned keep);

void filt_conf_devdrop(struct filt_s *o, unsigned idev);
void filt_conf_nodevdrop(struct filt_s *o, unsigned idev);
void filt_conf_devmap(struct filt_s *o, unsigned idev, unsigned odev);
void filt_conf_nodevmap(struct filt_s *o, unsigned odev);
void filt_conf_chandrop(struct filt_s *o, unsigned idev, unsigned ich);
void filt_conf_nochandrop(struct filt_s *o, unsigned idev, unsigned ich);
void filt_conf_chanmap(struct filt_s *o, unsigned idev, unsigned ich, unsigned odev, unsigned och);
void filt_conf_nochanmap(struct filt_s *o, unsigned odev, unsigned och );
void filt_conf_ctldrop(struct filt_s *o, unsigned idev, unsigned ich, unsigned ictl);
void filt_conf_noctldrop(struct filt_s *o, unsigned idev, unsigned ich, unsigned ictl);
void filt_conf_ctlmap(struct filt_s *o, unsigned idev, unsigned ich, unsigned odev, unsigned och, unsigned ictl, unsigned octl);
void filt_conf_noctlmap(struct filt_s *o, unsigned odev, unsigned och, unsigned octl);
void filt_conf_keydrop(struct filt_s *o, unsigned idev, unsigned ich, unsigned keylo, unsigned keyhi);
void filt_conf_nokeydrop(struct filt_s *o, unsigned idev, unsigned ich, unsigned keylo, unsigned keyhi);
void filt_conf_keymap(struct filt_s *o, unsigned idev, unsigned ich, unsigned odev, unsigned och, unsigned keylo, unsigned keyhi, int keyplus);
void filt_conf_nokeymap(struct filt_s *o, unsigned odev, unsigned och, unsigned keylo, unsigned keyhi);
void filt_conf_chgich(struct filt_s *o, unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch);
void filt_conf_chgidev(struct filt_s *o, unsigned olddev, unsigned newdev);
void filt_conf_swapich(struct filt_s *o, unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch);
void filt_conf_swapidev(struct filt_s *o, unsigned olddev, unsigned newdev);
void filt_conf_chgoch(struct filt_s *o, unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch);
void filt_conf_chgodev(struct filt_s *o, unsigned olddev, unsigned newdev);
void filt_conf_swapoch(struct filt_s *o, unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch);
void filt_conf_swapodev(struct filt_s *o, unsigned olddev, unsigned newdev);

extern unsigned filt_debug;

#endif /* MIDISH_FILT_H */
