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
 * 	  materials  provided with the distribution.
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

#ifndef SEQ_FILT_H
#define SEQ_FILT_H

#include "ev.h"

#define FILT_NOTE		0x01
#define FILT_BENDER		0x02
#define FILT_KEYTOUCH		0x04
#define FILT_CHANTOUCH		0x08
#define FILT_CTL		0x10
#define FILT_PROGCHANGE		0x20

struct state_s {
	struct state_s *next;
	struct ev_s ev;			/* initial event */
};

#define RULE_CTLMAP	1
#define RULE_KEYMAP	2
#define RULE_CHANMAP	3
#define RULE_DEVMAP	4

struct rule_s {
	struct rule_s *next;
	unsigned type;
	unsigned ichan, ochan;
	unsigned key_start, key_end;
	int key_plus;
	unsigned ictl, octl;
	unsigned char *curve;
};

struct filt_s {
	/* config */
	unsigned ichan, ochan;
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
void filt_run(struct filt_s *o, struct ev_s *ev);

struct state_s  *state_new(void);
void             state_del(struct state_s *s);

void             filt_pass(struct filt_s *o, struct ev_s *ev);
struct state_s **filt_statenew(struct filt_s *o, struct ev_s *ev);
void             filt_statedel(struct filt_s *o, struct state_s **p);
struct state_s **filt_statelookup(struct filt_s *o, struct ev_s *ev);
void             filt_stateshut(struct filt_s *o, struct state_s **p);

void filt_new_keymap(struct filt_s *o, unsigned ichan, unsigned ochan, unsigned key_start, unsigned key_end, int key_plus);
void filt_new_ctlmap(struct filt_s *o, unsigned ichan, unsigned ochan, unsigned ictl, unsigned octl);
void filt_new_chanmap(struct filt_s *o, unsigned ichan, unsigned ochan);
void filt_new_devmap(struct filt_s *o, unsigned idev, unsigned odev);
void filt_changein(struct filt_s *o, unsigned oldc, unsigned newc);

extern unsigned filt_debug;

#endif /* SEQ_FILT_H */
