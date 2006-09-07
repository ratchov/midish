/*
 * Copyright (c) 2003-2006 Alexandre Ratchov
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

unsigned filt_debug = 0;

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
rule_dbg(struct rule *o) {
	switch(o->type) {
	case RULE_DEVDROP:
		dbg_puts("devdrop");
		break;
	case RULE_DEVMAP:
		dbg_puts("devmap");
		break;
	case RULE_CHANDROP:
		dbg_puts("chandrop");
		break;
	case RULE_CHANMAP:
		dbg_puts("chanmap");
		break;
	case RULE_KEYDROP:
		dbg_puts("keydrop");
		break;
	case RULE_KEYMAP:
		dbg_puts("keymap");
		break;
	case RULE_CTLDROP:
		dbg_puts("ctldrop");
		break;
	case RULE_CTLMAP:
		dbg_puts("ctlmap");
		break;
	default:
		dbg_puts("unknown");
		break;
	}
	dbg_puts(" idev=");
	dbg_putu(o->idev);	
	dbg_puts(" odev=");
	dbg_putu(o->odev);
	if (o->type != RULE_DEVDROP && o->type != RULE_DEVMAP) {
		dbg_puts(" ich=");
		dbg_putu(o->ich);
		dbg_puts(" och=");
		dbg_putu(o->och);
	}
	if (o->type == RULE_CTLDROP || o->type == RULE_CTLMAP) {
		dbg_puts(" ictl=");
		dbg_putu(o->ictl);
		dbg_puts(" octl=");
		dbg_putu(o->octl);
	}
	if (o->type == RULE_KEYDROP || o->type == RULE_KEYMAP) {
		dbg_puts(" keyhi=");
		dbg_putu(o->keyhi);
		dbg_puts(" keylo=");
		dbg_putu(o->keylo);
		dbg_puts(" keyplus=");
		if (o->keyplus < 0) {
			dbg_puts("-");
			dbg_putu(-o->keyplus);
		} else {
			dbg_putu(o->keyplus);
		}
	}
}

void
rule_chgich(struct rule *o, unsigned olddev, unsigned oldch, 
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
rule_chgidev(struct rule *o, unsigned olddev, unsigned newdev) {
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

void
rule_swapich(struct rule *o, unsigned olddev, unsigned oldch, 
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
rule_swapidev(struct rule *o, unsigned olddev, unsigned newdev) {
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


void
rule_chgoch(struct rule *o, unsigned olddev, unsigned oldch, 
    unsigned newdev, unsigned newch) {
	switch(o->type) {
	case RULE_CHANMAP:
	case RULE_KEYMAP:
	case RULE_CTLMAP:
		if (o->odev == newdev && o->och == newch) {
			o->och = oldch;			
			o->odev = olddev;			
		} else if (o->odev == olddev && o->och == oldch) {
			o->odev = newdev;
			o->och = newch;
		}
		break;
	default:
		break;
	}
}


void
rule_chgodev(struct rule *o, unsigned olddev, unsigned newdev) {
	switch(o->type) {
	case RULE_CHANMAP:
	case RULE_KEYMAP:
	case RULE_CTLMAP:
	case RULE_DEVMAP:
		if (o->odev == olddev) {
			o->odev = newdev;
		} else if (o->odev == newdev) {
			o->odev = olddev;
		}
		break;
	default:
		break;
	}
}

void
rule_swapoch(struct rule *o, unsigned olddev, unsigned oldch, 
    unsigned newdev, unsigned newch) {
	switch(o->type) {
	case RULE_CHANMAP:
	case RULE_KEYMAP:
	case RULE_CTLMAP:
		if (o->odev == newdev && o->och == newch) {
			o->och = oldch;			
			o->odev = olddev;			
		} else if (o->odev == olddev && o->och == oldch) {
			o->odev = newdev;
			o->och = newch;
		}
		break;
	default:
		break;
	}
}


void
rule_swapodev(struct rule *o, unsigned olddev, unsigned newdev) {
	switch(o->type) {
	case RULE_CHANMAP:
	case RULE_KEYMAP:
	case RULE_CTLMAP:
	case RULE_DEVMAP:
		if (o->odev == olddev) {
			o->odev = newdev;
		} else if (o->odev == newdev) {
			o->odev = olddev;
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
filt_init(struct filt *o) {
	o->cb = NULL;
	o->addr = NULL;	
	o->active = 1;
	o->voice_rules = NULL;
	o->chan_rules = NULL;
	o->dev_rules = NULL;
	statelist_init(&o->statelist);	
}

	/*
	 * removes all filtering rules and all states
	 */

void
filt_reset(struct filt *o) {
	struct state *i;
	struct rule *r;
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
	while (o->statelist.first) {
		i = o->statelist.first;
		statelist_rm(&o->statelist, i);
		state_del(i);
	}
}

	/*
	 * destroy a filter
	 */

void
filt_done(struct filt *o) {
	statelist_done(&o->statelist);	
#ifdef FILT_DEBUG
	if (o->cb != NULL) {
		dbg_puts("filt_done: call filt_stop first.\n");
	}
#endif
	filt_reset(o);
}


	/*
	 * attach a filter to the owener
	 * events are passed to the given callback (see filt_pass)
	 */

void
filt_start(struct filt *o, void (*cb)(void *, struct ev *), void *addr) {
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
filt_stop(struct filt *o) {
	o->cb = NULL;
	o->addr = NULL;	
#ifdef FILT_DEBUG
	if (o->statelist.first) {	/* XXX: use statelist macro */
		dbg_puts("filt_stop: state list isn't empty\n");
	}
#endif
}


/* ------------------------------------- configuration management --- */

	/* 
	 * configure the filt to drop 
	 * events from a particular device 
	 * - if the same drop-rule exists then do nothing
	 * - remove map-rules with the same idev
	 */
	 
void
filt_conf_devdrop(struct filt *o, unsigned idev) {
	struct rule **i, *r;
	
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
	r = (struct rule *)mem_alloc(sizeof(struct rule));
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
filt_conf_nodevdrop(struct filt *o, unsigned idev) {
	struct rule **i, *r;
	
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
filt_conf_devmap(struct filt *o,
    unsigned idev, unsigned odev) {
	struct rule **i, *r;
	
	i = &o->dev_rules;
	while (*i != NULL) {
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
	r = (struct rule *)mem_alloc(sizeof(struct rule));
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
filt_conf_nodevmap(struct filt *o, unsigned odev) {
	struct rule **i, *r;
	
	i = &o->dev_rules;
	while (*i != NULL) {
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
filt_conf_chandrop(struct filt *o, unsigned idev, unsigned ich) {
	struct rule **i, *r;
	
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
	r = (struct rule *)mem_alloc(sizeof(struct rule));
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
filt_conf_nochandrop(struct filt *o, unsigned idev, unsigned ich) {
	struct rule **i, *r;
	
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
filt_conf_chanmap(struct filt *o, unsigned idev, unsigned ich,
    unsigned odev, unsigned och) {
	struct rule **i, *r;
	
	i = &o->chan_rules;
	while (*i != NULL) {
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
	r = (struct rule *)mem_alloc(sizeof(struct rule));
	r->type = RULE_CHANMAP;
	r->idev = idev;
	r->ich = ich;
	r->odev = odev;
	r->och = och;
	r->next = o->chan_rules;
	o->chan_rules = r;
}

void
filt_conf_nochanmap(struct filt *o, unsigned odev, unsigned och ) {
	struct rule **i, *r;
	
	i = &o->chan_rules;
	while (*i != NULL) {
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
filt_conf_ctldrop(struct filt *o, 
    unsigned idev, unsigned ich, unsigned ictl) {
	struct rule **i, *r;
	
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
	r = (struct rule *)mem_alloc(sizeof(struct rule));
	r->type = RULE_CTLDROP;
	r->idev = idev;
	r->ich = ich;
	r->ictl = ictl;
	r->next = o->voice_rules;
	o->voice_rules = r;
}

void
filt_conf_noctldrop(struct filt *o, 
    unsigned idev, unsigned ich, unsigned ictl) {
	struct rule **i, *r;
	
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
filt_conf_ctlmap(struct filt *o, 
    unsigned idev, unsigned ich, unsigned odev, unsigned och, 
    unsigned ictl, unsigned octl) {
	struct rule **i, *r;
	
	i = &o->voice_rules;
	while (*i != NULL) {
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
	r = (struct rule *)mem_alloc(sizeof(struct rule));
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
filt_conf_noctlmap(struct filt *o, 
    unsigned odev, unsigned och, unsigned octl) {
	struct rule **i, *r;
	
	i = &o->voice_rules;
	while (*i != NULL) {
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
filt_conf_keydrop(struct filt *o, unsigned idev, unsigned ich, 
    unsigned keylo, unsigned keyhi) {
	struct rule **i, *r;
	
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
	r = (struct rule *)mem_alloc(sizeof(struct rule));
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
filt_conf_nokeydrop(struct filt *o, unsigned idev, unsigned ich, 
    unsigned keylo, unsigned keyhi) {
	struct rule **i, *r;
	
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
filt_conf_keymap(struct filt *o, 
    unsigned idev, unsigned ich, unsigned odev, unsigned och, 
    unsigned keylo, unsigned keyhi, int keyplus) {
	struct rule **i, *r;
	
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
	r = (struct rule *)mem_alloc(sizeof(struct rule));
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
filt_conf_nokeymap(struct filt *o, unsigned odev, unsigned och, 
    unsigned keylo, unsigned keyhi) {
	struct rule **i, *r;
	
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
filt_conf_chgich(struct filt *o, 
    unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch) {
	struct rule *i;
	
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_chgich(i, olddev, oldch, newdev, newch);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_chgich(i, olddev, oldch, newdev, newch);
	}
}


void
filt_conf_chgidev(struct filt *o, unsigned olddev, unsigned newdev) {
	struct rule *i;
	
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_chgidev(i, olddev, newdev);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_chgidev(i, olddev, newdev);
	}
	for (i = o->dev_rules; i != NULL; i = i->next) {
		rule_chgidev(i, olddev, newdev);
	}
}


void
filt_conf_swapich(struct filt *o, 
    unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch) {
	struct rule *i;
	
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_swapich(i, olddev, oldch, newdev, newch);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_swapich(i, olddev, oldch, newdev, newch);
	}
}


void
filt_conf_swapidev(struct filt *o, unsigned olddev, unsigned newdev) {
	struct rule *i;
	
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_swapidev(i, olddev, newdev);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_swapidev(i, olddev, newdev);
	}
	for (i = o->dev_rules; i != NULL; i = i->next) {
		rule_swapidev(i, olddev, newdev);
	}
}

void
filt_conf_chgoch(struct filt *o, 
    unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch) {
	struct rule *i;
	
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_chgoch(i, olddev, oldch, newdev, newch);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_chgoch(i, olddev, oldch, newdev, newch);
	}
}


void
filt_conf_chgodev(struct filt *o, unsigned olddev, unsigned newdev) {
	struct rule *i;
	
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_chgodev(i, olddev, newdev);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_chgodev(i, olddev, newdev);
	}
	for (i = o->dev_rules; i != NULL; i = i->next) {
		rule_chgodev(i, olddev, newdev);
	}
}


void
filt_conf_swapoch(struct filt *o, 
    unsigned olddev, unsigned oldch, unsigned newdev, unsigned newch) {
	struct rule *i;
	
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_swapoch(i, olddev, oldch, newdev, newch);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_swapoch(i, olddev, oldch, newdev, newch);
	}
}


void
filt_conf_swapodev(struct filt *o, unsigned olddev, unsigned newdev) {
	struct rule *i;
	
	for (i = o->voice_rules; i != NULL; i = i->next) {
		rule_swapodev(i, olddev, newdev);
	}
	for (i = o->chan_rules; i != NULL; i = i->next) {
		rule_swapodev(i, olddev, newdev);
	}
	for (i = o->dev_rules; i != NULL; i = i->next) {
		rule_swapodev(i, olddev, newdev);
	}
}

/* ----------------------------------------------- real-time part --- */

	/* 
	 * executes the given rule; if event matches the rule then
	 * pass it and return 1, else retrun 0
	 */
	 	
		
unsigned
filt_matchrule(struct filt *o, struct rule *r, struct ev *ev) {
	struct ev te;
		
	switch(r->type) {
	case RULE_DEVDROP:
		if (ev->data.voice.dev == r->idev) {
			goto match_drop;
		}
		break;	
	case RULE_DEVMAP:
		if (ev->data.voice.dev == r->idev) {
			te = *ev;
			te.data.voice.dev = r->odev;
			goto match_pass;
		}
		break;
	case RULE_CHANDROP:
		if (ev->data.voice.dev == r->idev &&
		    ev->data.voice.ch == r->ich) {
			goto match_drop;
		}
		break;
	case RULE_CHANMAP:
		if (ev->data.voice.dev == r->idev && 
		    ev->data.voice.ch == r->ich) {
			te = *ev;
			te.data.voice.dev = r->odev;
			te.data.voice.ch = r->och;
			goto match_pass;
		}
		break;
	case RULE_KEYDROP:
		if (EV_ISNOTE(ev) && 
		    ev->data.voice.dev == r->idev && 
		    ev->data.voice.ch == r->ich && 
		    ev->data.voice.b0 >= r->keylo &&
		    ev->data.voice.b0 <= r->keyhi) {
			goto match_drop;
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
			goto match_pass;
		}
		break;
	case RULE_CTLDROP:
		if (ev->cmd == EV_CTL &&
		    ev->data.voice.dev == r->idev &&
		    ev->data.voice.ch == r->ich &&
		    ev->data.voice.b0 == r->ictl) {
			goto match_drop;
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
			goto match_pass;
		}
		break;
	}			
	return 0;

match_pass:
	if (filt_debug) {
		dbg_puts("filt_matchrule: ");
		rule_dbg(r);
		dbg_puts(" > ");		
		ev_dbg(&te);
		dbg_puts("\n");
	}
	if (o->cb) {
		o->cb(o->addr, &te);
	}
	return 1;
match_drop:
	if (filt_debug) {
		dbg_puts("filt_matchrule: ");
		rule_dbg(r);
		dbg_puts("\n");
	}
	return 1;
}


	/*
	 * match event against all rules
	 */

void
filt_processev(struct filt *o, struct ev *ev) {
	unsigned match;
	struct rule *i;

	match = 0;
	for (i = o->voice_rules; i != NULL; i = i->next) {
		match |= filt_matchrule(o, i, ev);
	}
	if (!match) {
		for (i = o->chan_rules; i != NULL; i = i->next) {
			match |= filt_matchrule(o, i, ev);
		}
		if (!match) {
			for (i = o->dev_rules; i != NULL; i = i->next) {
				match |= filt_matchrule(o, i, ev);
			}
			if (!match) {
				if (filt_debug) {
					dbg_puts("filt_processev: default > ");
					ev_dbg(ev);
					dbg_puts("\n");
				}
				if (o->cb) {
					o->cb(o->addr, ev);
				}
			}
		}
	}
}

	/*
	 * send state event and delete
	 * the state if it is no more used
	 */

void
filt_staterun(struct filt *o, struct state *s) {
	if (s->data.filt.nevents == 0) {
		filt_processev(o, &s->ev);
		if (!s->data.filt.keep) {
#ifdef FILT_DEBUG
			if (filt_debug) {
				dbg_puts("filt_staterun: deleting: ");
				ev_dbg(&s->ev);
				dbg_puts("\n");
			}
#endif
			statelist_rm(&o->statelist, s);
			state_del(s);
			return;
		}
	}
	s->data.filt.nevents++;
}

	/*
	 * create and run a new state and return pointer
	 * to the next runable state
	 */

struct state *
filt_statecreate(struct filt *o, struct ev *ev, unsigned keep) {
	struct state *s;
	
#ifdef FILT_DEBUG
	if (filt_debug) {
		dbg_puts("filt_statecreate: ");
		ev_dbg(ev);
		dbg_puts("\n");
	}
#endif
	s = state_new();
	s->ev = *ev;
	s->data.filt.nevents = 0;
	s->data.filt.keep = keep;
	statelist_add(&o->statelist, s);
	filt_staterun(o, s);
	return s;
}

	/*
	 * update the event and the keep-flag of the given state
	 * try to send it
	 */

void
filt_stateupdate(struct filt *o, struct state *s, struct ev *ev, unsigned keep) {
	s->ev = *ev;
	s->data.filt.keep = keep;
	filt_staterun(o, s);
}

	/*
	 * lookup in the state list for a state
	 * corresponding to the give event. Ie a
	 * state with the same note, the same controller,
	 * bender etc...
	 */

struct state *
filt_statelookup(struct filt *o, struct ev *ev) {
	return statelist_lookup(&o->statelist, ev);
}


	/*
	 * for the given state (returned by filt_statelookup
	 * or filt_statenew), generates and passes an event
	 * restoring the default midi state. I.e. generate
	 * NOTEOFFs for NOTEON-states etc...
	 * 	
	 */

void
filt_stateshut(struct filt *o, struct state *s) {
	struct ev *ev = &s->ev;

	if (s->ev.cmd == EV_NON || s->ev.cmd == EV_KAT) {
		ev->cmd = EV_NOFF;
		ev->data.voice.b0 = s->ev.data.voice.b0;
		ev->data.voice.b1 = EV_NOFF_DEFAULTVEL;
		s->data.filt.keep = 0;
		filt_staterun(o, s);
	} else if (s->ev.cmd == EV_BEND) {
		ev->data.voice.b0 = EV_BEND_DEFAULTLO;
		ev->data.voice.b1 = EV_BEND_DEFAULTHI;
		s->data.filt.keep = 0;
		filt_staterun(o, s);
	} else if (s->ev.cmd == EV_CAT) {
		ev->data.voice.b0 = EV_CAT_DEFAULT;
		s->data.filt.keep = 0;
		filt_staterun(o, s);
	} else {
#ifdef FILT_DEBUG
		if (filt_debug) {
			dbg_puts("filt_stateshut: deleting: ");
			ev_dbg(ev);
			dbg_puts("\n");
		}
#endif
		statelist_rm(&o->statelist, s);
		state_del(s);
	}
}


	/*
	 * shuts all notes and restores the default values
	 * of the modified controllers, the bender etc...
	 */

void
filt_shut(struct filt *o) {
	struct state *s, *snext;
	
	for (s = o->statelist.first; s != NULL; s = snext) {
		snext = s->next;
		filt_stateshut(o, s);
	}
}

	/*
	 * give an event to the filter for processing
	 */

void
filt_evcb(struct filt *o, struct ev *ev) {
	struct state *s;
	unsigned keep;
	
	if (filt_debug) {
		dbg_puts("filt_run: ");
		ev_dbg(ev);
		dbg_puts("\n");
	}
#ifdef FILT_DEBUG
	if (o->cb == NULL) {
		dbg_puts("filt_evcb: cb = NULL, bad initialisation\n");
		dbg_panic();
	}
	if (!EV_ISVOICE(ev)) {
		dbg_puts("filt_evcb: only voice events allowed\n");
		dbg_panic();
	}
#endif

	/* 
	 * first, sanitize the input; keep state for
	 * NOTEs and BENDs
	 */

	if (EV_ISNOTE(ev)) {
		s = filt_statelookup(o, ev);
		if (ev->cmd == EV_NON) {
			if (s) {
				dbg_puts("filt_evcb: noteon: state exists, ignored\n");
				return;
			}
			if (!o->active) {
				return;
			}
			filt_statecreate(o, ev, 1);
		} else if (ev->cmd == EV_NOFF) {
			if (s) {
				filt_stateupdate(o, s, ev, 0);
			} else {
				if (!o->active) {
					return;
				}
				dbg_puts("filt_evcb: noteoff: state doesn't exist, event ignored\n");
			}
		} else {
			filt_stateupdate(o, s, ev, 1);
		}
	} else if (ev->cmd == EV_BEND) {
		keep = (ev->data.voice.b0 != EV_BEND_DEFAULTLO || 
			ev->data.voice.b1 != EV_BEND_DEFAULTHI) ? 1 : 0;
		s = filt_statelookup(o, ev);
		if (!s || 
		    ev->data.voice.b0 != s->ev.data.voice.b0 ||
		    ev->data.voice.b1 != s->ev.data.voice.b1) {
			if (s) {
				filt_stateupdate(o, s, ev, keep);
			} else {
				if (!o->active) {
					return;
				}
				filt_statecreate(o, ev, keep);
			}
		}
	} else if (ev->cmd == EV_CTL) {
		keep = 1;
		s = filt_statelookup(o, ev);
		if (!s) {
			if (!o->active) {
				return;
			}
			filt_statecreate(o, ev, keep);
		} else if (ev->data.voice.b1 != s->ev.data.voice.b1) {
			filt_stateupdate(o, s, ev, keep);
		}
	} else if (ev->cmd == EV_CAT) {
		/* 
		XXX: not tested => set keep to zero 
		keep = ev->data.voice.b0 != 0 ? 1 : 0;
		*/
		keep = 0;		
		s = filt_statelookup(o, ev);
		if (!s) {
			if (!o->active) {
				return;
			}
			filt_statecreate(o, ev, keep);
		} else if (ev->data.voice.b0 != s->ev.data.voice.b0) {
			filt_stateupdate(o, s, ev, keep);
		}
	} else {
		filt_processev(o, ev);
	}
}

void
filt_timercb(struct filt *o) {
	struct state *s, *snext;
	unsigned nevents;

	for (s = o->statelist.first; s != NULL; s = snext) {
		snext = s->next;
		nevents = s->data.filt.nevents;
		s->data.filt.nevents = 0;
		if (nevents > 1) {
			filt_staterun(o, s);
		}
	}
}
