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

/*
 * filt.c
 *
 * a simple midi filter. Rewrites input events according a set
 * of user-configurable rules.
 *
 * each rule determines determines how events are transformed
 * (actually "routed"). There is a source event spec and a destination
 * event spec: any event matching the source event spec is rewritten
 * to match the destination event spec. Both source and destination
 * evsepcs must describe the same king of events.
 */

/*
 * TODO
 *
 * filt_conf_xxxdrop 
 *
 *	can be made generic, just remove input mapping rules
 *	and why not just restrict them by removing a subset
 *	in 'from' ?
 *	
 * filt_conf_xxxmap
 *
 *	same remarks
 */
 
#include "dbg.h"
#include "ev.h"
#include "filt.h"
#include "pool.h"
#include "mux.h"
#include "cons.h"

unsigned filt_debug = 1;

unsigned
filt_evmap(struct evspec *from, struct evspec *to, 
	   struct ev *in, struct ev *out) 
{
	if (from->cmd == EVSPEC_ANY) {
		out->cmd = in->cmd;
		out->v0 = in->v0;
		out->v1 = in->v1;
		goto ch;
	} else if (from->cmd == EVSPEC_NOTE) {
		if (!EV_ISNOTE(in)) {
			return 0;
		}
		out->cmd = in->cmd;
		out->v1 = in->v1;
		goto v0;
	} else if (from->cmd == in->cmd) {
		out->cmd = to->cmd;
		if (in->cmd == EV_BEND || in->cmd == EV_CAT) {
			out->v0 = in->v0;
			goto ch;
		} else {
			goto v0;
		}
	} else {
		return 0;
	}
 v0:
	if (in->v0 < from->v0_min ||
	    in->v0 > from->v0_max) {
		return 0;
	}
	out->v0 = in->v0 + to->v0_min - from->v0_min;
 ch:
	if (in->dev < from->dev_min ||
	    in->dev > from->dev_max ||
	    in->ch < from->ch_min ||
	    in->ch > from->ch_max) {
		return 0;
	}
	out->dev = in->dev + to->dev_min - from->dev_min;
	out->ch = in->ch + to->ch_min - from->ch_min;
	dbg_puts("filt_evmap: ");
	ev_dbg(in);
	dbg_puts(" ->");
	ev_dbg(out);
	dbg_puts("\n");
	return 1;
}

/*
 * dump the rule on stderr (debug purposes)
 */
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

/*
 * try to apply the rule to an event; if the rule matches then
 * fill 'rev' (with EV_NULL if it's a drop rule) and return 1 
 * else return 0
 */
unsigned
rule_do(struct rule *r, struct ev *in, struct ev *out) {
	switch(r->type) {
	case RULE_DEVDROP:
		if (in->dev == r->idev) {
			goto match_drop;
		}
		break;
	case RULE_DEVMAP:
		if (in->dev == r->idev) {
			*out = *in;
			out->dev = r->odev;
			goto match_pass;
		}
		break;
	case RULE_CHANDROP:
		if (in->dev == r->idev &&
		    in->ch == r->ich) {
			goto match_drop;
		}
		break;
	case RULE_CHANMAP:
		if (in->dev == r->idev && 
		    in->ch == r->ich) {
			*out = *in;
			out->dev = r->odev;
			out->ch = r->och;
			goto match_pass;
		}
		break;
	case RULE_KEYDROP:
		if (EV_ISNOTE(in) && 
		    in->dev == r->idev && 
		    in->ch == r->ich && 
		    in->note_num >= r->keylo &&
		    in->note_num <= r->keyhi) {
			goto match_drop;
		}
		break;
	case RULE_KEYMAP:
		if (EV_ISNOTE(in) && 
		    in->dev == r->idev && 
		    in->ch == r->ich && 
		    in->note_num >= r->keylo &&
		    in->note_num <= r->keyhi) {
			*out = *in;
			out->dev = r->odev;
			out->ch = r->och;
			out->note_num += r->keyplus;
			out->note_num &= 0x7f;
			goto match_pass;
		}
		break;
	case RULE_CTLDROP:
		if (in->cmd == EV_XCTL &&
		    in->dev == r->idev &&
		    in->ch == r->ich &&
		    in->ctl_num == r->ictl) {
			goto match_drop;
		}
		break;
	case RULE_CTLMAP:
		if (in->cmd == EV_XCTL &&
		    in->dev == r->idev &&
		    in->ch == r->ich &&
		    in->ctl_num == r->ictl) {
			*out = *in;
			out->dev = r->odev;
			out->ch = r->och;
			out->ctl_num = r->octl;
			goto match_pass;
		}
		break;
	}
	return 0;

match_drop:
	out->cmd = EV_NULL;
match_pass:
	if (filt_debug) {
		dbg_puts("rule_apply: ");
		rule_dbg(r);
		dbg_puts(": ");
		ev_dbg(in);
		dbg_puts(" -> ");		
		ev_dbg(out);
		dbg_puts("\n");
	}
	return 1;
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
		if (o->idev == olddev && o->ich == oldch) {
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
		if (o->odev == olddev && o->och == oldch) {
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

void filt_timocb(void *);

/* 
 * initialise a filter
 */
void
filt_init(struct filt *o) {
	o->voice_rules = NULL;
	o->chan_rules = NULL;
	o->dev_rules = NULL;
}

/*
 * removes all filtering rules and all states
 */
void
filt_reset(struct filt *o) {
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
}

/*
 * destroy a filter
 */
void
filt_done(struct filt *o) {
	filt_reset(o);
}


/*
 * match event against all rules, return the number of events
 * filled
 */
unsigned
filt_do(struct filt *o, struct ev *in, struct ev *out) {
	unsigned match;
	struct rule *i;

	match = 0;
	for (i = o->voice_rules; i != NULL; i = i->next) {
		if (match >= FILT_MAXNRULES) {
			return match;
		}
		match += rule_do(i, in, out + match);
	}
	if (!match) {
		for (i = o->chan_rules; i != NULL; i = i->next) {
			if (match >= FILT_MAXNRULES) {
				return match;
			}
			match += rule_do(i, in, out + match);
		}
		if (!match) {
			for (i = o->dev_rules; i != NULL; i = i->next) {
				if (match >= FILT_MAXNRULES) {
					return match;
				}
				match += rule_do(i, in, out + match);
			}
			if (!match) {
				*out = *in;
				if (filt_debug) {
					dbg_puts("filt_do: default > ");
					ev_dbg(out);
					dbg_puts("\n");
				}
				return 1;
			}
		}
	}
	return match;
}


/*
 * return the number of rules on the filter
 */
unsigned
filt_nrules(struct filt *o) {
	struct rule *r;
	unsigned n = 0;

	for (r = o->voice_rules; r != NULL; r = r->next) {
		n++;
	}
	for (r = o->chan_rules; r != NULL; r = r->next) {
		n++;
	}
	for (r = o->dev_rules; r != NULL; r = r->next) {
		n++;
	}
	return n;
}

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
	if (filt_nrules(o) >= FILT_MAXNRULES) {
		return;
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
	if (filt_nrules(o) >= FILT_MAXNRULES) {
		return;
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
	if (filt_nrules(o) >= FILT_MAXNRULES) {
		return;
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
	if (filt_nrules(o) >= FILT_MAXNRULES) {
		return;
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
	if (filt_nrules(o) >= FILT_MAXNRULES) {
		return;
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
	if (filt_nrules(o) >= FILT_MAXNRULES) {
		return;
	}
	r = (struct rule *)mem_alloc(sizeof(struct rule));
	r->type = RULE_CTLMAP;
	r->idev = idev;
	r->ich = ich;
	r->odev = odev;
	r->och = och;
	r->ictl = ictl;
	r->octl = octl;
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
	if (filt_nrules(o) >= FILT_MAXNRULES) {
		return;
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
	if (filt_nrules(o) >= FILT_MAXNRULES) {
		return;
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

