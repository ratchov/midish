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
rule_changein(struct rule_s *o, unsigned oldc, unsigned newc) {
	switch(o->type) {
	case RULE_CHANMAP:
	case RULE_KEYMAP:
	case RULE_CTLMAP:
		if (o->ichan == oldc) {
			o->ichan = newc;
		}
		break;
	case RULE_DEVMAP:
	default:
		break;
	}
}

#if 0

unsigned
rule_isused(struct rule_s *o) {
	switch(o->type) {
	case RULE_DEVMAP:
		if (o->ichan != o->ochan) {
			return 1;
		} 
		break;
	case RULE_CHANMAP:
		if (o->ichan != o->ochan) {
			return 1;
		} 
		break;
	case RULE_KEYMAP:
		if (o->ichan != o->ochan ||
		    o->key_plus != 0 ||
		    o->curve != filt_curve_id) {
			return 1;
		}
		break;
	case RULE_CTLMAP:
		if (o->ichan != o->ochan ||
		    o->ictl != o->octl ||
		    o->curve != filt_curve_id) {
			return 1;
		}
		break;
	default:
		dbg_puts("rule_checkused: unknown rule\n");
		dbg_panic();
	}
	return 0;
}
	
unsigned
rule_checkchan(struct rule_s *o) {
	if (o->ichan > EV_MAXCHAN ||
	    o->ochan > EV_MAXCHAN) {
		return 0;
	}
	return 1;
}

unsigned
rule_checkkey(struct rule_s *o) {
	if (!rule_checkchan(o) ||
	    o->key_start > o->key_end ||
	    o->key_start + o->key_plus < 0 ||
	    o->key_start + o->key_plus > EV_MAXB0 ||
	    o->key_end + o->key_plus < 0 ||
	    o->key_end + o->key_plus > EV_MAXB0) {
		return 0;
	}
	return 1;
}

unsigned
rule_checkctl(struct rule_s *o) {
	if (!rule_checkchan(o) ||
	    o->ictl > EV_MAXB0 ||
	    o->octl > EV_MAXB0) {
		return 0;
	}
	return 1;
}


#endif

void
filt_new_devmap(struct filt_s *o,
    unsigned ichan, unsigned ochan) {
	struct rule_s **i, *found;
	struct rule_s *r;
	
	i = &o->dev_rules;
	while(*i != 0) {
		if ((*i)->type == RULE_DEVMAP && 
		    (*i)->ochan == ochan) {
			found = *i;
			*i = found->next;
			mem_free(found);
		} else {
			i = &(*i)->next;
		}
	}
	
	if (ichan != ochan) {
		r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
		r->type = RULE_DEVMAP;
		r->ichan = ichan;
		r->ochan = ochan;
		r->next = o->dev_rules;
		o->dev_rules = r;
	}
}


void
filt_new_chanmap(struct filt_s *o,
    unsigned ichan, unsigned ochan) {
	struct rule_s **i, *found;
	struct rule_s *r;
	
	i = &o->chan_rules;
	while(*i != 0) {
		if ((*i)->type == RULE_CHANMAP && 
		    (*i)->ochan == ochan) {
			found = *i;
			*i = found->next;
			mem_free(found);
		} else {
			i = &(*i)->next;
		}
	}
	
	if (ichan != ochan) {
		r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
		r->type = RULE_CHANMAP;
		r->ichan = ichan;
		r->ochan = ochan;
		r->next = o->chan_rules;
		o->chan_rules = r;
	}
}



void
filt_new_keymap(struct filt_s *o, unsigned ichan, unsigned ochan,
    unsigned key_start, unsigned key_end, int key_plus) {
	struct rule_s **i, *found;
	struct rule_s *r;
	
	i = &o->voice_rules;
	while(*i != 0) {
		if ((*i)->type == RULE_KEYMAP && 
		    (*i)->ochan == ochan &&
		    (*i)->key_plus == key_plus) {
			found = *i;
			*i = found->next;
			mem_free(found);
		} else {
			i = &(*i)->next;
		}
	}
	
	if (ichan != ochan || key_plus != 0) {
		r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
		r->type = RULE_KEYMAP;
		r->ichan = ichan;
		r->ochan = ochan;
		r->key_start = key_start;
		r->key_end = key_end;
		r->key_plus = key_plus;
		r->curve = filt_curve_id;
		r->next = o->voice_rules;
		o->voice_rules = r;
	}
}


void
filt_new_ctlmap(struct filt_s *o,
    unsigned ichan, unsigned ochan, unsigned ictl, unsigned octl) {
	struct rule_s **i, *found;
	struct rule_s *r;
	
	i = &o->voice_rules;
	while(*i != 0) {
		if ((*i)->type == RULE_CTLMAP && 
		    (*i)->ochan == ochan && 
		    (*i)->octl == octl) {
			found = *i;
			*i = found->next;
			mem_free(found);
		} else {
			i = &(*i)->next;
		}
	}
	
	if (ichan != ochan || ictl != octl) {
		r = (struct rule_s *)mem_alloc(sizeof(struct rule_s));
		r->type = RULE_CTLMAP;
		r->ichan = ichan;
		r->ochan = ochan;
		r->ictl = ictl;
		r->octl = octl;
		r->curve = filt_curve_id;
		r->next = o->voice_rules;
		o->voice_rules = r;
	}
}



void
filt_changein(struct filt_s *o, unsigned oldc, unsigned newc) {
	struct rule_s *i;
	
	for (i = o->voice_rules; i != 0; i = i->next) {
		rule_changein(i, oldc, newc);
	}
	for (i = o->chan_rules; i != 0; i = i->next) {
		rule_changein(i, oldc, newc);
	}
	if (o->chan_rules == 0) {
		filt_new_chanmap(o, newc, oldc);
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
	o->ichan = o->ochan = 0;
		
	o->statelist = 0;
	o->active = 1;
	o->voice_rules = o->chan_rules = o->dev_rules = 0;
	
/*	filt_new_chanmap(o, 16, 0);
	filt_new_ctlmap(o, 16, 0, 7, 11);
	filt_new_keymap(o,  16, 0, 0, 127, -12);
*/
}


void
filt_reset(struct filt_s *o) {
	struct rule_s *r;
	struct state_s *i;

	while(o->voice_rules) {
		r = o->voice_rules;
		o->voice_rules = r->next;
		mem_free(r); 
	}
	while(o->chan_rules) {
		r = o->chan_rules;
		o->chan_rules = r->next;
		mem_free(r); 
	}
	while(o->dev_rules) {
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
			    (*i)->ev.data.voice.b0 == ev->data.voice.b0) {
				return i;
			}
		}	
	} else if (ev->cmd == EV_CTL) {
		for (i = &o->statelist; *i != 0; i = &(*i)->next) {
			if ((*i)->ev.cmd == EV_CTL && 
			    (*i)->ev.data.voice.b0 == ev->data.voice.b0) {
				return i;
			}
		}	
	} else if (ev->cmd == EV_BEND) {
		for (i = &o->statelist; *i != 0; i = &(*i)->next) {
			if ((*i)->ev.cmd == EV_BEND) {
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
		ev->data.voice.b1 = 100;
		filt_pass(o, ev);
	} else if (ev->cmd == EV_BEND) {
		ev->data.voice.b0 = 0;
		ev->data.voice.b1 = 0x40;
		filt_pass(o, ev);
	}
	filt_statedel(o, p);
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


/* --------------------------------------------------------------------- */

	/* 
	 * executes the given rule; if the rule matches
	 * the event then return 1, else retrun 0
	 */
	 	
extern void *user_stdout;
		
unsigned
filt_matchrule(struct filt_s *o, struct rule_s *r, struct ev_s *ev) {
	struct ev_s te;
		
	switch(r->type) {
	case RULE_DEVMAP:
		if ((ev->data.voice.chan & 0xf0) == r->ichan) {
			te = *ev;
			te.data.voice.chan &= 0x0f;
			te.data.voice.chan |= r->ochan;
			filt_pass(o, &te);
			return 1;
		}
		break;
	case RULE_CHANMAP:
		if (ev->data.voice.chan == r->ichan) {
			te = *ev;
			te.data.voice.chan = r->ochan;
			filt_pass(o, &te);
			return 1;
		}
		break;
	case RULE_KEYMAP:
		if (EV_ISNOTE(ev) && 
		    ev->data.voice.chan == r->ichan && 
		    ev->data.voice.b0 >= r->key_start &&
		    ev->data.voice.b0 <= r->key_end) {
			te = *ev;
			te.data.voice.chan = r->ochan;
			te.data.voice.b0 += r->key_plus;
			te.data.voice.b0 &= 0x7f;
			te.data.voice.b1 = r->curve[te.data.voice.b1];
			filt_pass(o, &te);
			return 1;
		}
		break;
	case RULE_CTLMAP:
		if (ev->cmd == EV_CTL &&
		    ev->data.voice.chan == r->ichan &&
		    ev->data.voice.b0 == r->ictl) {
			te = *ev;
			te.data.voice.chan = r->ochan;
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
				goto drop;
			}
		} else {
			if (p) {
				goto pass;
			}
			goto drop;
		}
	} else if (ev->cmd == EV_BEND) {
		p = filt_statelookup(o, ev);
		if (ev->data.voice.b0 == 0 && ev->data.voice.b1 == 0x40) {
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
			goto drop;
		}
		goto pass;
	} else if (ev->cmd == EV_CAT) {
		if (!o->active) {
			goto drop;
		}
		goto pass;
	}
	
pass:
	ret = 0;
	for (i = o->voice_rules; i != 0; i = i->next) {
		ret |= filt_matchrule(o, i, ev);
	}
	if (!ret) {
		for (i = o->chan_rules; i != 0; i = i->next) {
			ret |= filt_matchrule(o, i, ev);
		}
	}
	if (!ret) {
		for (i = o->dev_rules; i != 0; i = i->next) {
			ret |= filt_matchrule(o, i, ev);
		}
	}
drop:
	return;
}

