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

/*
 * filt.c
 *
 * a stateful midi filter. It's used to drop/rewrite some events, 
 * transpose notes etc... 
 *
 */
 
#include "dbg.h"
#include "ev.h"
#include "filt.h"
#include "pool.h"

#define FILT_DEBUG

unsigned filt_debug = 0;

/* ------------------------------------------------------------------ */

	/*
	 * states are structures used to hold the notes
	 * which are "on", the values of used controllers
	 * the bender etc...
	 *
	 * since for each midi event, a state can be created
	 * in realtime, we use a pool.
	 *
	 */

struct pool_s state_pool;

void
state_pool_init(unsigned size) {
	pool_init(&state_pool, "state", sizeof(struct state_s), size);
}

void
state_pool_done(void) {
	pool_done(&state_pool);
}

struct state_s *
state_new(void) {
	return (struct state_s *)pool_new(&state_pool);
}

void
state_del(struct state_s *s) {
	pool_del(&state_pool, s);
}

/* ------------------------------------------------------------------ */

unsigned char filt_curve_id[128] = {
	0,	1,	2,	3,	4,	5,	6,	7,
	8,	9,	10,	11,	12,	13,	14,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31,
	32,	33,	34,	35,	36,	37,	38,	39,
	40,	41,	42,	43,	44,	45,	46,	47,
	48,	49,	50,	51,	52,	53,	54,	55,
	56,	57,	58,	59,	60,	61,	62,	63,
	64,	65,	66,	67,	68,	69,	70,	71,
	72,	73,	74,	75,	76,	77,	78,	79,
	80,	81,	82,	83,	84,	85,	86,	87,
	88,	89,	90,	91,	92,	93,	94,	95,
	96,	97,	98,	99,	100,	101,	102,	103,
	104,	105,	106,	107,	108,	109,	110,	111,
	112,	113,	114,	115,	116,	117,	118,	119,
	120,	121,	122,	123,	124,	125,	126,	127
};

unsigned char filt_curve_inv[128] = {
	127,	126,	125,	124,	123,	122,	121,	120,
	119,	118,	117,	116,	115,	114,	113,	112,
	111,	110,	109,	108,	107,	106,	105,	104,
	103,	102,	101,	100,	99,	98,	97,	96,
	95,	94,	93,	92,	91,	90,	89,	88,
	87,	86,	85,	84,	83,	82,	81,	80,
	79,	78,	77,	76,	75,	74,	73,	72,
	71,	70,	69,	68,	67,	66,	65,	64,
	63,	62,	61,	60,	59,	58,	57,	56,
	55,	54,	53,	52,	51,	50,	49,	48,
	47,	46,	45,	44,	43,	42,	41,	40,
	39,	38,	37,	36,	35,	34,	33,	32,
	31,	30,	29,	28,	27,	26,	25,	24,
	23,	22,	21,	20,	19,	18,	17,	16,
	15,	14,	13,	12,	11,	10,	9,	8,
	7,	6,	5,	4,	3,	2,	1,	0
};


void
rule_swapichan(struct rule_s *o, unsigned olddev, unsigned oldch, 
    unsigned newdev, unsigned newch) {
	switch(o->type) {
	case RULE_CHANDROP:
	case RULE_CHANMAP:
	case RULE_KEYDROP:
	case RULE_KEYMAP:
	case RULE_CTLDROP:
	case RULE_CTLMAP:
		if (o->idev == newdev && o->ich == newch) {
			o->ich = oldch;			
			o->idev = olddev;			
		} else if (o->idev == olddev && o->ich == oldch) {
			o->idev = newdev;
			o->ich = newch;
		}
		break;
	default:
		break;
	}
}


void
rule_swapidev(struct rule_s *o, unsigned olddev, unsigned newdev) {
	switch(o->type) {
	case RULE_CHANDROP:
	case RULE_CHANMAP:
	case RULE_KEYDROP:
	case RULE_KEYMAP:
	case RULE_CTLDROP:
	case RULE_CTLMAP:
	case RULE_DEVDROP:
	case RULE_DEVMAP:
		if (o->idev == olddev) {
			o->idev = newdev;
		} else if (o->idev == newdev) {
			o->idev = olddev;
		}
		break;
	default:
		break;
	}
}


/* --------------------------------------------------------------------- */


	/* 
	 * initialise a filter
	 */
	 
void
filt_init(struct filt_s *o) {
	o->cb = 0;
	o->addr = 0;
		
	o->statelist = 0;
	o->active = 1;
	o->voice_rules = 0;
	o->chan_rules = 0;
	o->dev_rules = 0;
}

	/*
	 * removes all filtering rules and all states
	 */

void
filt_reset(struct filt_s *o) {
	struct state_s *i;
	struct rule_s *r;
	while (o->voice_rules) {
		r = o->voice_rules;
		o->voice_rules = r->next;
		mem_free(r); 
	}
	while (o->chan_rules) {
		r = o->chan_rules;
		o->chan_rules = r->next;
		mem_free(r); 
	}
	while (o->dev_rules) {
		r = o->dev_rules;
		o->dev_rules = r->next;
		mem_free(r); 
	}
	while (o->statelist) {
		i = o->statelist;
		o->statelist = i->next;
		state_del(i);
	}
}

	/*
	 * destroy a filter
	 */

void
filt_done(struct filt_s *o) {
#ifdef FILT_DEBUG
	if (o->cb != 0 || o->addr != 0) {
		dbg_puts("filt_done: call filt_stop first.\n");
	}
	if (o->statelist != 0) {
		dbg_puts("filt_done: statelist not empty\n");
	}
#endif
	filt_reset(o);
}


	/*
	 * attach a filter to the owener
	 * events are passed to the given callback (see filt_pass)
	 */

void
filt_start(struct filt_s *o, void (*cb)(void *, struct ev_s *), void *addr) {
	/*
	XXX: why ?
	o->active = 0; 
	*/
	o->addr = addr;
	o->cb = cb;
}


	/*
	 * detaches the filter from the ower
	 */

void
filt_stop(struct filt_s *o) {
	o->cb = 0;
	o->addr = 0;	
}


	/*
	 * shuts all notes and restores the default values
	 * of the modified controllers, the bender etc...
	 */

void
filt_shut(struct filt_s *o) {
	while (o->statelist) {
		filt_stateshut(o, &o->statelist);
	}
}


/* ------------------------------------- configuration management --- */

	/* 
	 * configure the filt to drop 
	 * events from a particular device 
	 * - if the same drop-rule exists then do nothing
	 * - remove map-rules with the same idev
	 */
	 
void
filt_conf_devdrop(struct filt_s *o, unsigned idev) {
	struct rule_s **i, *r;
	
	i = &o->dev_rules;
	while (*i) {
		if (((*i)->type == RULE_DEVDROP || 
		     (*i)->type == RULE_DEVMAP) &&
		    (*i)->idev == idev) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
	r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
	r->type = RULE_DEVDROP;
	r->idev = idev;
	r->next = o->dev_rules;
	o->dev_rules = r;
}

	/* 
	 * configure the filt not to drop 
	 * events from a particular device 
	 */ 

void
filt_conf_nodevdrop(struct filt_s *o, unsigned idev) {
	struct rule_s **i, *r;
	
	i = &o->dev_rules;
	while (*i) {
		if ((*i)->type == RULE_DEVDROP &&
		    (*i)->idev == idev) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
}

	/* 
	 * configure the filter to map one device to another 
	 * - if the same map-rule exists dont add new one
	 * - remove maps with the same output dev
	 * - remove drops with the same input
	 */

void
filt_conf_devmap(struct filt_s *o,
    unsigned idev, unsigned odev) {
	struct rule_s **i, *r;
	
	i = &o->dev_rules;
	while (*i != 0) {
		if (((*i)->type == RULE_DEVMAP && 
		     (*i)->odev == odev) ||
		    ((*i)->type == RULE_DEVDROP && 
		     (*i)->idev == idev)) {
		  	r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
	r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
	r->type = RULE_DEVMAP;
	r->idev = idev;
	r->odev = odev;
	r->next = o->dev_rules;
	o->dev_rules = r;
}

	/* 
	 * configure the filter not to map 
	 * any devices to the given one
	 */

void
filt_conf_nodevmap(struct filt_s *o, unsigned odev) {
	struct rule_s **i, *r;
	
	i = &o->dev_rules;
	while (*i != 0) {
		if ((*i)->type == RULE_DEVMAP && 
		    (*i)->odev == odev) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
}

	/*
	 * configure the filter to drop a particular 
	 * channel
	 */

void
filt_conf_chandrop(struct filt_s *o, unsigned idev, unsigned ich) {
	struct rule_s **i, *r;
	
	i = &o->chan_rules;
	while (*i) {
		if (((*i)->type == RULE_CHANDROP && 
		     (*i)->idev == idev &&
		     (*i)->ich == ich) ||
		    ((*i)->type == RULE_CHANMAP && 
		     (*i)->idev == idev &&
		     (*i)->ich == ich)) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
	r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
	r->type = RULE_CHANDROP;
	r->idev = idev;
	r->ich = ich;
	r->next = o->chan_rules;
	o->chan_rules = r;	
}

	/*
	 * configure the filter to drop a particular 
	 * channel
	 */
	 
void
filt_conf_nochandrop(struct filt_s *o, unsigned idev, unsigned ich) {
	struct rule_s **i, *r;
	
	i = &o->chan_rules;
	while (*i) {
		if ((*i)->type == RULE_CHANDROP && 
		    (*i)->idev == idev &&
		    (*i)->ich == ich) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
}

void
filt_conf_chanmap(struct filt_s *o, unsigned idev, unsigned ich,
    unsigned odev, unsigned och) {
	struct rule_s **i, *r;
	
	i = &o->chan_rules;
	while (*i != 0) {
		if (((*i)->type == RULE_CHANMAP && 
		     (*i)->odev == odev &&
		     (*i)->och == och) ||
		    ((*i)->type == RULE_CHANDROP && 
		     (*i)->idev == idev &&
		     (*i)->ich == ich)) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
	r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
	r->type = RULE_CHANMAP;
	r->idev = idev;
	r->ich = ich;
	r->odev = odev;
	r->och = och;
	r->next = o->chan_rules;
	o->chan_rules = r;
}

void
filt_conf_nochanmap(struct filt_s *o, unsigned odev, unsigned och ) {
	struct rule_s **i, *r;
	
	i = &o->chan_rules;
	while (*i != 0) {
		if ((*i)->type == RULE_CHANMAP && 
		    (*i)->odev == odev &&
		    (*i)->och == och) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
}


void
filt_conf_ctldrop(struct filt_s *o, 
    unsigned idev, unsigned ich, unsigned ictl) {
	struct rule_s **i, *r;
	
	i = &o->voice_rules;
	while (*i) {
		if (((*i)->type == RULE_CTLDROP && 
		     (*i)->idev == idev && 
		     (*i)->ich == ich && 
		     (*i)->ictl == ictl) 
		     ||
		    ((*i)->type == RULE_CTLMAP && 
		     (*i)->idev == idev && 
		     (*i)->ich == ich && 
		     (*i)->ictl == ictl)) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else{
			i = &(*i)->next;
		}
	}
	r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
	r->type = RULE_CTLDROP;
	r->idev = idev;
	r->ich = ich;
	r->ictl = ictl;
	r->next = o->voice_rules;
	o->voice_rules = r;
}

void
filt_conf_noctldrop(struct filt_s *o, 
    unsigned idev, unsigned ich, unsigned ictl) {
	struct rule_s **i, *r;
	
	i = &o->voice_rules;
	while (*i) {
		if ((*i)->type == RULE_CTLDROP && 
		    (*i)->idev == idev && 
		    (*i)->ich == ich && 
		    (*i)->ictl == ictl) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
}

void
filt_conf_ctlmap(struct filt_s *o, 
    unsigned idev, unsigned ich, unsigned odev, unsigned och, 
    unsigned ictl, unsigned octl) {
	struct rule_s **i, *r;
	
	i = &o->voice_rules;
	while (*i != 0) {
		if (((*i)->type == RULE_CTLMAP && 
		     (*i)->odev == odev && 
		     (*i)->och == och && 
		     (*i)->octl == octl) 
		     ||
		    ((*i)->type == RULE_CTLDROP &&
		     (*i)->idev == idev && 
		     (*i)->ich == ich && 
		     (*i)->ictl == ictl)) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
	r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
	r->type = RULE_CTLMAP;
	r->idev = idev;
	r->ich = ich;
	r->odev = odev;
	r->och = och;
	r->ictl = ictl;
	r->octl = octl;
	r->curve = filt_curve_id;
	r->next = o->voice_rules;
	o->voice_rules = r;
}

void
filt_conf_noctlmap(struct filt_s *o, 
    unsigned odev, unsigned och, unsigned octl) {
	struct rule_s **i, *r;
	
	i = &o->voice_rules;
	while (*i != 0) {
		if ((*i)->type == RULE_CTLMAP && 
		    (*i)->odev == odev && 
		    (*i)->och == och && 
		    (*i)->octl == octl) {
			r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
}

void
filt_conf_keydrop(struct filt_s *o, unsigned idev, unsigned ich, 
    unsigned keylo, unsigned keyhi) {
	struct rule_s **i, *r;
	
	i = &o->voice_rules;
	while (*i) {
		if (((*i)->type == RULE_KEYDROP && 
		     (*i)->idev == idev && 
		     (*i)->ich == ich && 
		     keyhi >= (*i)->keylo && 
		     keylo <= (*i)->keyhi) 
		     ||
		    ((*i)->type == RULE_KEYMAP && 
		     (*i)->idev == idev && 
		     (*i)->ich == ich && 
		     keyhi >= (*i)->keylo && 
		     keylo <= (*i)->keyhi)) {
		    	r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
	r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
	r->type = RULE_KEYDROP;
	r->idev = idev;
	r->ich = ich;
	r->keylo = keylo;
	r->keyhi = keyhi;
	r->next = o->voice_rules;
	o->voice_rules = r;
}


	/*
	 * if there is intersection with existing rules
	 * then remove them
	 */

void
filt_conf_nokeydrop(struct filt_s *o, unsigned idev, unsigned ich, 
    unsigned keylo, unsigned keyhi) {
	struct rule_s **i, *r;
	
	i = &o->voice_rules;
	while (*i) {
		if ((*i)->type == RULE_KEYDROP && 
		    (*i)->idev == idev && 
		    (*i)->ich == ich && 
		    keyhi >= (*i)->keylo && 
		    keylo <= (*i)->keyhi) {
		    	r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
}


void
filt_conf_keymap(struct filt_s *o, 
    unsigned idev, unsigned ich, unsigned odev, unsigned och, 
    unsigned keylo, unsigned keyhi, int keyplus) {
	struct rule_s **i, *r;
	
	i = &o->voice_rules;
	while (*i) {
		if (((*i)->type == RULE_KEYMAP && 
		     (*i)->odev == odev && 
		     (*i)->och == och && 
		     keyhi >= (*i)->keylo && 
		     keylo <= (*i)->keyhi)
		     ||
		    ((*i)->type == RULE_KEYDROP && 
		     (*i)->idev == idev && 
		     (*i)->ich == ich && 
		     keyhi >= (*i)->keylo && 
		     keylo <= (*i)->keyhi)) {
		    	r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
	r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
	r->type = RULE_KEYMAP;
	r->idev = idev;
	r->ich = ich;
	r->odev = odev;
	r->och = och;
	r->keylo = keylo;
	r->keyhi = keyhi;
	r->keyplus = keyplus;
	r->curve = filt_curve_id;
	r->next = o->voice_rules;
	o->voice_rules = r;
}


void
filt_conf_nokeymap(struct filt_s *o, unsigned odev, unsigned och, 
    unsigned keylo, unsigned keyhi) {
	struct rule_s **i, *r;
	
	i = &o->voice_rules;
	while (*i) {
		if ((*i)->type == RULE_KEYMAP && 
		    (*i)->odev == odev && 
		    (*i)->och == och && 
		    keyhi >= (*i)->keylo && 
		    keylo <= (*i)->keyhi) {
		    	r = *i;
			*i = r->next;
			mem_free(r);
		} else {
			i = &(*i)->next;
		}
	}
}


void
filt_conf_swapichan(struct filt_s *o, 
    unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch) {
	struct rule_s *i;
	
	for (i = o->voice_rules; i != 0; i = i->next) {
		rule_swapichan(i, olddev, oldch, newdev, newch);
	}
	for (i = o->chan_rules; i != 0; i = i->next) {
		rule_swapichan(i, olddev, oldch, newdev, newch);
	}
}


void
filt_conf_swapidev(struct filt_s *o, unsigned olddev, unsigned newdev) {
	struct rule_s *i;
	
	for (i = o->voice_rules; i != 0; i = i->next) {
		rule_swapidev(i, olddev, newdev);
	}
	for (i = o->chan_rules; i != 0; i = i->next) {
		rule_swapidev(i, olddev, newdev);
	}
	for (i = o->dev_rules; i != 0; i = i->next) {
		rule_swapidev(i, olddev, newdev);
	}
}



/* ----------------------------------------------- real-time part --- */

	/*
	 * when called in realtime, this method passes
	 * the event to the event handler of the 
	 * filter owner
	 */

void
filt_pass(struct filt_s *o, struct ev_s *ev) {
#ifdef FILT_DEBUG
	if (filt_debug) {
		dbg_puts("filt_pass: ");
		ev_dbg(ev);
		dbg_puts("\n");
	}
#endif
	if (o->cb) {
		o->cb(o->addr, ev);
	}
}

	/*
	 * create a state, copy the given event in it
	 * and insert it into the list of states 
	 *
	 * return a poiter to the next-field of
	 * the previous state in the liste, so
	 * the return value can be used to delete
	 * the state without any lookup in the list.
	 */


struct state_s **
filt_statenew(struct filt_s *o, struct ev_s *ev) {
	struct state_s *s;

#ifdef FILT_DEBUG
	if (filt_debug) {
		dbg_puts("filt_statenew: ");
		ev_dbg(ev);
		dbg_puts("\n");
	}
#endif
	s = state_new();
	s->ev = *ev;
	s->next = o->statelist;
	o->statelist = s;
	return &o->statelist;
}

	/*
	 * remove the given state from the list
	 * and destroy the state.
	 *
	 * 'p' is a ponter returned by filt_statenew
	 * or filt_statelookup
	 */

void
filt_statedel(struct filt_s *o, struct state_s **p) {
	struct state_s *s;

	s = *p;
#ifdef FILT_DEBUG
	if (filt_debug) {
		dbg_puts("filt_statedel: ");
		ev_dbg(&s->ev);
		dbg_puts("\n");
	}
#endif
	*p = s->next;
	state_del(s);
}

	/*
	 * lookup in the state list for a state
	 * corresponding to the give event. Ie a
	 * state with the same note, the same controller,
	 * bender etc...
	 */


struct state_s **
filt_statelookup(struct filt_s *o, struct ev_s *ev) {
	struct state_s **i;
	
	if (EV_ISNOTE(ev)) {
		for (i = &o->statelist; *i != 0; i = &(*i)->next) {
			if ((*i)->ev.cmd == EV_NON && 
			    (*i)->ev.data.voice.dev == ev->data.voice.dev && 
			    (*i)->ev.data.voice.ch == ev->data.voice.ch && 
			    (*i)->ev.data.voice.b0 == ev->data.voice.b0) {
				return i;
			}
		}	
	} else if (ev->cmd == EV_CTL) {
		for (i = &o->statelist; *i != 0; i = &(*i)->next) {
			if ((*i)->ev.cmd == EV_CTL && 
			    (*i)->ev.data.voice.dev == ev->data.voice.dev && 
			    (*i)->ev.data.voice.ch == ev->data.voice.ch && 
			    (*i)->ev.data.voice.b0 == ev->data.voice.b0) {
				return i;
			}
		}	
	} else if (ev->cmd == EV_BEND) {
		for (i = &o->statelist; *i != 0; i = &(*i)->next) {
			if ((*i)->ev.cmd == EV_BEND &&
			    (*i)->ev.data.voice.dev == ev->data.voice.dev && 
			    (*i)->ev.data.voice.ch == ev->data.voice.ch) {
				return i;
			}
		}	
	}
	return 0;
}


	/*
	 * for the given state (returned by filt_statelookup
	 * or filt_statenew), generates and passes an event
	 * restoring the default midi state. I.e. generate
	 * NOTEOFFs for NOTEON-states etc...
	 * 	
	 */


void
filt_stateshut(struct filt_s *o, struct state_s **p) {
	struct ev_s *ev = &(*p)->ev;
	
	if (ev->cmd == EV_NON) {
		ev->cmd = EV_NOFF;
		ev->data.voice.b1 = EV_NOFF_DEFAULTVEL;
		filt_pass(o, ev);
	} else if (ev->cmd == EV_BEND) {
		ev->data.voice.b0 = EV_BEND_DEFAULTLO;
		ev->data.voice.b1 = EV_BEND_DEFAULTHI;
		filt_pass(o, ev);
	}
	filt_statedel(o, p);
}

	/* 
	 * executes the given rule; if the rule matches
	 * the event then return 1, else retrun 0
	 */
	 	
extern void *user_stdout;
		
unsigned
filt_matchrule(struct filt_s *o, struct rule_s *r, struct ev_s *ev) {
	struct ev_s te;
		
	switch(r->type) {
	case RULE_DEVDROP:
		if (ev->data.voice.dev == r->idev) {
			return 1;
		}
		break;	
	case RULE_DEVMAP:
		if (ev->data.voice.dev == r->idev) {
			te = *ev;
			te.data.voice.dev &= r->odev;
			filt_pass(o, &te);
			return 1;
		}
		break;
	case RULE_CHANDROP:
		if (ev->data.voice.dev == r->idev &&
		    ev->data.voice.ch == r->ich) {
			return 1;
		}
		break;
	case RULE_CHANMAP:
		if (ev->data.voice.dev == r->idev && 
		    ev->data.voice.ch == r->ich) {
			te = *ev;
			te.data.voice.dev = r->odev;
			te.data.voice.ch = r->och;
			filt_pass(o, &te);
			return 1;
		}
		break;
	case RULE_KEYDROP:
		if (EV_ISNOTE(ev) && 
		    ev->data.voice.dev == r->idev && 
		    ev->data.voice.ch == r->ich && 
		    ev->data.voice.b0 >= r->keylo &&
		    ev->data.voice.b0 <= r->keyhi) {
			return 1;
		}
		break;
	case RULE_KEYMAP:
		if (EV_ISNOTE(ev) && 
		    ev->data.voice.dev == r->idev && 
		    ev->data.voice.ch == r->ich && 
		    ev->data.voice.b0 >= r->keylo &&
		    ev->data.voice.b0 <= r->keyhi) {
			te = *ev;
			te.data.voice.dev = r->odev;
			te.data.voice.ch = r->och;
			te.data.voice.b0 += r->keyplus;
			te.data.voice.b0 &= 0x7f;
			te.data.voice.b1 = r->curve[te.data.voice.b1];
			filt_pass(o, &te);
			return 1;
		}
		break;
	case RULE_CTLDROP:
		if (ev->cmd == EV_CTL &&
		    ev->data.voice.dev == r->idev &&
		    ev->data.voice.ch == r->ich &&
		    ev->data.voice.b0 == r->ictl) {
			return 1;
		}
		break;
	case RULE_CTLMAP:
		if (ev->cmd == EV_CTL &&
		    ev->data.voice.dev == r->idev &&
		    ev->data.voice.ch == r->ich &&
		    ev->data.voice.b0 == r->ictl) {
			te = *ev;
			te.data.voice.dev = r->odev;
			te.data.voice.ch = r->och;
			te.data.voice.b0 = r->octl;
			te.data.voice.b1 = r->curve[te.data.voice.b1];
			filt_pass(o, &te);
			return 1;
		}
		break;
	}			
	return 0;
}

	/*
	 * give an event to the filter for processing
	 */

void
filt_run(struct filt_s *o, struct ev_s *ev) {
	struct state_s **p;
	struct rule_s *i;
	unsigned ret;
	
#ifdef FILT_DEBUG
	if (filt_debug) {
		dbg_puts("filt_run: ");
		ev_dbg(ev);
		dbg_puts("\n");
	}
	if (o->cb == 0) {
		dbg_puts("filt_run: cb = 0, bad initialisation\n");
		dbg_panic();
	}
	if (!EV_ISVOICE(ev)) {
		dbg_puts("filt_run: only voice events allowed\n");
		dbg_panic();
	}
#endif

	/* 
	 * first, sanitize the input; keep state for
	 * NOTEs and BENDs
	 */

	if (EV_ISNOTE(ev)) {
		p = filt_statelookup(o, ev);
		if (ev->cmd == EV_NON) {
			if (!o->active) {
				return;
			}
			if (p) {
				dbg_puts("noteon: state exists, removed\n");
				filt_stateshut(o, p);
			}
			p = filt_statenew(o, ev);
			goto pass;
		} else if (ev->cmd == EV_NOFF) {
			if (p) {
				filt_statedel(o, p);
				goto pass;
			} else {
				if (!o->active) {
					return;
				}
				dbg_puts("noteoff: state doesn't exist, dropped\n");
				goto quit;
			}
		} else {
			if (p) {
				goto pass;
			}
			goto quit;
		}
	} else if (ev->cmd == EV_BEND) {
		p = filt_statelookup(o, ev);
		if (ev->data.voice.b0 == EV_BEND_DEFAULTLO && 
		    ev->data.voice.b1 == EV_BEND_DEFAULTHI) {
			if (p) {
				filt_statedel(o, p);
			}
		} else {
			if (p) {
				(*p)->ev.data.voice.b0 = ev->data.voice.b0;
				(*p)->ev.data.voice.b1 = ev->data.voice.b1;
			} else {
				if (!o->active) {
					return;
				}			
				p = filt_statenew(o, ev);
			}			
		}		
		goto pass;
	} else if (ev->cmd == EV_CTL) {
		if (!o->active) {
			goto quit;
		}
		goto pass;
	} else if (ev->cmd == EV_CAT) {
		if (!o->active) {
			goto quit;
		}
		goto pass;
	}
	
	/* 
	 * match events agants rules
	 */

pass:
	ret = 0;
	for (i = o->voice_rules; i != 0; i = i->next) {
		ret |= filt_matchrule(o, i, ev);
	}
	if (!ret) {
		for (i = o->chan_rules; i != 0; i = i->next) {
			ret |= filt_matchrule(o, i, ev);
		}
		if (!ret) {
			for (i = o->dev_rules; i != 0; i = i->next) {
				ret |= filt_matchrule(o, i, ev);
			}
			if (!ret) {
				filt_pass(o, ev);
			}
		}
	}
quit:
	return;
}

