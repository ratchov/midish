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
 */
 
#include "dbg.h"
#include "ev.h"
#include "filt.h"
#include "pool.h"
#include "mux.h"
#include "cons.h"

unsigned filt_debug = 0;

void
rule_dbg(struct evspec *from, struct  evspec *to)
{
	evspec_dbg(from);
	dbg_puts(" > ");
	evspec_dbg(to);
}

/*
 * transform "in" matching "from" range into "out" matching "to"
 * range, this routine is supposed to be fast since it's called for
 * each incoming event.
 * 
 * from/to _must_ have the same dev/chl/v0/v1 ranges.
 */
void
filt_mapev(struct evspec *from, struct evspec *to, 
    struct ev *in, struct ev *out) 
{
	*out = *in;
	if (to->cmd != EVSPEC_ANY && to->cmd != EVSPEC_NOTE)
		out->cmd = to->cmd;
	if ((evinfo[from->cmd].flags & EV_HAS_DEV) &&
	    (evinfo[in->cmd].flags & EV_HAS_DEV)) {
		out->dev += to->dev_min - from->dev_min;
	}
	if ((evinfo[from->cmd].flags & EV_HAS_CH) &&
	    (evinfo[in->cmd].flags & EV_HAS_CH)) {
		out->ch += to->ch_min - from->ch_min;
	}
	if (evinfo[from->cmd].nranges > 0 &&
	    evinfo[in->cmd].nranges > 0) {
		out->v0 += to->v0_min - from->v0_min;
	}
	if (evinfo[from->cmd].nranges > 1 &&
	    evinfo[in->cmd].nranges > 1) {
		out->v1 += to->v1_min - from->v1_min;
	}
	if (filt_debug) {
		dbg_puts("filt_mapev: ");
		rule_dbg(from, to);
		dbg_puts(": ");
		ev_dbg(in);
		dbg_puts(" -> ");
		ev_dbg(out);
		dbg_puts("\n");
	}
}

/* 
 * initialize a filter
 */
void
filt_init(struct filt *o)
{
	o->srclist = NULL;
}

/*
 * remove all filtering rules and all states
 */
void
filt_reset(struct filt *o)
{
	struct filtsrc *s;
	struct filtdst *d;

	while (o->srclist) {
		s = o->srclist;
		while (s->dstlist) {
			d = s->dstlist;
			s->dstlist = d->next;
			mem_free(d); 
		}
		o->srclist = s->next;
		mem_free(s); 
	}
}

/*
 * destroy a filter
 */
void
filt_done(struct filt *o)
{
	filt_reset(o);
	o->srclist = (void *)0xdeadbeef;
}

/*
 * match event against all sources and for each source
 * generate output events
 */
unsigned
filt_do(struct filt *o, struct ev *in, struct ev *out)
{
	struct filtsrc *s;
	struct filtdst *d;
	unsigned nev;

	for (s = o->srclist;; s = s->next) {
		if (s == NULL)
			return 0;
		if (evspec_matchev(&s->es, in))
			break;
	}
	nev = 0;
	for (d = s->dstlist; d != NULL; d = d->next) {
		filt_mapev(&s->es, &d->es, in, out);
		nev++;
		out++;
	}
	return nev;
}

/*
 * remove all rules that are included in the from->to argument.
 */
void
filt_mapdel(struct filt *f, struct evspec *from, struct evspec *to)
{
	struct filtsrc *s, **ps;
	struct filtdst *d, **pd;

	for (ps = &f->srclist; (s = *ps) != NULL;) {
		if (evspec_in(&s->es, from)) {
			for (pd = &s->dstlist; (d = *pd) != NULL;) {
				if (evspec_in(&d->es, to)) {
					if (filt_debug) {
						dbg_puts("filt_mapdel: ");
						rule_dbg(&s->es, &d->es);
						dbg_puts(": removed\n");
					}
					*pd = d->next;
					mem_free(d);
					continue;
				}
				pd = &d->next;
			}
		}	
		if (s->dstlist == NULL) {
			if (filt_debug) {
				dbg_puts("filt_mapdel: ");
				evspec_dbg(&s->es);
				dbg_puts(": empty, removed\n");
			}
			*ps = s->next;
			mem_free(s);
			continue;
		}
		ps = &s->next;
	}
}

/* 
 * add a new rule to map events in "from" range to events in "to" range
 */
void
filt_mapnew(struct filt *f, struct evspec *from, struct evspec *to)
{
	struct filtsrc *s, **ps;
	struct filtdst *d, **pd;

	if (filt_debug) {
		dbg_puts("filt_mapnew: adding ");
		rule_dbg(from, to);
		dbg_puts("\n");
	}

	/*
	 * check if ranges are ok
	 */
	if (to->cmd != EVSPEC_EMPTY) {
		if (from->cmd != to->cmd) {
			cons_err("use the same cmd for 'from' and 'to' args");
			return;
		}
		if (evinfo[from->cmd].flags & EV_HAS_DEV &&
		    from->dev_max - from->dev_min != to->dev_max - to->dev_min) {
			cons_err("dev ranges must have the same size");
			return;
		}
		if (evinfo[from->cmd].flags & EV_HAS_CH &&
		    from->ch_max - from->ch_min != to->ch_max - to->ch_min) {
			cons_err("chan ranges must have the same size");
			return;
		}
		if (evinfo[from->cmd].nranges >= 1 &&
		    from->v0_max - from->v0_min != to->v0_max - to->v0_min) {
			cons_err("v0 ranges must have the same size");
			return;
		}
		if (evinfo[from->cmd].nranges >= 2 &&
		    from->v1_max - from->v1_min != to->v1_max - to->v1_min) {
			cons_err("v1 ranges must have the same size");
			return;
		}
	}

	/*
	 * delete existing mappings that may have conflicting sources
	 */
	for (ps = &f->srclist; (s = *ps) != NULL;) {
		if (evspec_isec(&s->es, from) &&
		    !evspec_in(&s->es, from) && 
		    !evspec_in(from, &s->es)) {
			for (pd = &s->dstlist; (d = *pd) != NULL;) {
				if (filt_debug) {
					dbg_puts("filt_mapnew: ");
					rule_dbg(&s->es, &d->es);
					dbg_puts(": src intersect\n");
				}
				*pd = d->next;
				mem_free(d);
			}
			*ps = s->next;
			mem_free(s);
			continue;
		}
		ps = &s->next;
	}

	/*
	 * find the correct place where to insert the source
	 */
	for (ps = &f->srclist;;) {
		s = *ps;
		if (s == NULL) {
			if (filt_debug)
				dbg_puts("filt_mapnew: no match\n");
			break;
		} else if (evspec_eq(from, &s->es)) {
			if (filt_debug)
				dbg_puts("filt_mapnew: exact match\n");
			goto add_dst;
		} else if (evspec_in(from, &s->es)) {
			if (filt_debug) {
				dbg_puts("filt_mapnew: ");
				evspec_dbg(from);
				dbg_puts(" in ");
				evspec_dbg(&s->es);
				dbg_puts("\n");
			}
			break;
		} else if (evspec_isec(from, &s->es)) {
			if (filt_debug) {
				dbg_puts("filt_mapnew: ");
				evspec_dbg(&s->es);
				dbg_puts(": intersection\n");
			}
			while (s->dstlist) {
				d = s->dstlist;
				s->dstlist = d->next;
				mem_free(d); 
			}
			*ps = s->next;
			mem_free(s); 
			continue;
		} else {
			if (filt_debug) {
				dbg_puts("filt_mapnew: ");
				evspec_dbg(from);
				dbg_puts(": skipped\n");
			}
		}
		ps = &s->next;
	}
	s = (struct filtsrc *)mem_alloc(sizeof(struct filtsrc));
	s->es = *from;
	s->next = *ps;
	s->dstlist = NULL;
	*ps = s;

 add_dst:
	/*
	 * remove conflicting destinations for the same source
	 */
	for (pd = &s->dstlist; (d = *pd) != NULL;) {
		if (evspec_isec(&d->es, to) ||
		    to->cmd == EVSPEC_EMPTY ||
		    d->es.cmd == EVSPEC_EMPTY) {
			if (filt_debug) {
				dbg_puts("filt_mapnew: ");
				rule_dbg(&s->es, &d->es);
				dbg_puts(": src intersect\n");
			}
			*pd = d->next;
			mem_free(d);
			continue;
		}
		pd = &d->next;
	}
	/*
	 * add the new destination to the end of the list,
	 * in order to obtain the same order as the order in
	 * which rules are added
	 */
	d = (struct filtdst *)mem_alloc(sizeof(struct filtdst));
	d->es = *to;
	for (pd = &s->dstlist; *pd != NULL; pd = &(*pd)->next) {
		/* nothing */
	}
	d->next = NULL;
	*pd = d;
}

/* 
 * configure the filt to drop events from a particular device
 */
void
filt_conf_devdrop(struct filt *o, unsigned idev)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = idev;
	to.cmd = EVSPEC_EMPTY;	
	filt_mapnew(o, &from, &to);
}

/* 
 * configure the filt not to drop events from a particular device
 */ 
void
filt_conf_nodevdrop(struct filt *o, unsigned idev)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = idev;
	to.cmd = EVSPEC_EMPTY;	
	filt_mapdel(o, &from, &to);
}

/* 
 * configure the filter to map one device to another
 */
void
filt_conf_devmap(struct filt *o, unsigned idev, unsigned odev)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	from.dev_min = from.dev_max = idev;
	to.dev_min = to.dev_max = odev;
	filt_mapnew(o, &from, &to);
}

/* 
 * configure the filter not to map 
 * any devices to the given one
 */
void
filt_conf_nodevmap(struct filt *o, unsigned odev)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	to.dev_min = to.dev_max = odev;
	filt_mapdel(o, &from, &to);
}

/*
 * configure the filter to drop a particular channel
 */
void
filt_conf_chandrop(struct filt *o, unsigned idev, unsigned ich)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	to.cmd = EVSPEC_EMPTY;	
	filt_mapnew(o, &from, &to);
}

/*
 * configure the filter to drop a particular channel
 */
void
filt_conf_nochandrop(struct filt *o, unsigned idev, unsigned ich)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	to.cmd = EVSPEC_EMPTY;	
	filt_mapdel(o, &from, &to);
}

void
filt_conf_chanmap(struct filt *o, unsigned idev, unsigned ich,
    unsigned odev, unsigned och)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	filt_mapnew(o, &from, &to);
}

void
filt_conf_nochanmap(struct filt *o, unsigned odev, unsigned och)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	filt_mapdel(o, &from, &to);
}


void
filt_conf_ctldrop(struct filt *o, unsigned idev, unsigned ich, unsigned ictl)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.cmd = EVSPEC_XCTL;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = from.v0_max = ictl;
	to.cmd = EVSPEC_EMPTY;	
	filt_mapnew(o, &from, &to);
}

void
filt_conf_noctldrop(struct filt *o, unsigned idev, unsigned ich, unsigned ictl)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.cmd = EVSPEC_XCTL;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = from.v0_max = ictl;
	to.cmd = EVSPEC_EMPTY;	
	filt_mapdel(o, &from, &to);
}

void
filt_conf_ctlmap(struct filt *o, 
    unsigned idev, unsigned ich, unsigned odev, unsigned och, 
    unsigned ictl, unsigned octl)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	from.cmd = EVSPEC_CTL;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = from.v0_max = ictl;
	to.cmd = EVSPEC_CTL;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	to.v0_min = to.v0_max = octl;
	filt_mapnew(o, &from, &to);
}

void
filt_conf_noctlmap(struct filt *o, 
    unsigned odev, unsigned och, unsigned octl)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	to.cmd = EVSPEC_CTL;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	to.v0_min = to.v0_max = octl;
	filt_mapdel(o, &from, &to);
}

void
filt_conf_keydrop(struct filt *o, unsigned idev, unsigned ich, 
    unsigned keylo, unsigned keyhi)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.cmd = EVSPEC_NOTE;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = keylo;
	from.v0_max = keyhi;
	to.cmd = EVSPEC_EMPTY;	
	filt_mapnew(o, &from, &to);
}

void
filt_conf_nokeydrop(struct filt *o, unsigned idev, unsigned ich, 
    unsigned keylo, unsigned keyhi)
{
	struct evspec from, to;

	evspec_reset(&from);
	from.cmd = EVSPEC_NOTE;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = keylo;
	from.v0_max = keyhi;
	to.cmd = EVSPEC_EMPTY;	
	filt_mapdel(o, &from, &to);
}


void
filt_conf_keymap(struct filt *o, 
    unsigned idev, unsigned ich, unsigned odev, unsigned och, 
    unsigned keylo, unsigned keyhi, int keyplus)
{
	struct evspec from, to;

	if ((int)keyhi + keyplus > EV_MAXCOARSE)
		keyhi = EV_MAXCOARSE - keyplus;
	if ((int)keylo + keyplus < 0)
		keylo = -keyplus;
	if (keylo >= keyhi)
		return;

	evspec_reset(&from);
	evspec_reset(&to);
	from.cmd = EVSPEC_NOTE;
	from.dev_min = from.dev_max = idev;
	from.ch_min = from.ch_max = ich;
	from.v0_min = keylo;
	from.v0_max = keyhi;
	to.cmd = EVSPEC_NOTE;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	to.v0_min = keylo + keyplus;
	to.v0_max = keyhi + keyplus;
	filt_mapnew(o, &from, &to);
}

void
filt_conf_nokeymap(struct filt *o, unsigned odev, unsigned och, 
    unsigned keylo, unsigned keyhi)
{
	struct evspec from, to;

	evspec_reset(&from);
	evspec_reset(&to);
	to.cmd = EVSPEC_NOTE;
	to.dev_min = to.dev_max = odev;
	to.ch_min = to.ch_max = och;
	to.v0_min = keylo;
	to.v0_max = keyhi;
	filt_mapdel(o, &from, &to);
}

void
filt_conf_chgich(struct filt *o, unsigned olddev, unsigned oldch,
    unsigned newdev, unsigned newch)
{
	struct filtsrc *i;
	
	for (i = o->srclist; i != NULL; i = i->next) {
		if (i->es.dev_min != i->es.dev_max ||
		    i->es.ch_min != i->es.ch_max) {
			continue;
		}
		if (i->es.dev_min == olddev &&
		    i->es.ch_min == oldch) {
			i->es.dev_min = i->es.dev_max = newdev;
			i->es.ch_min = i->es.ch_max = newch;
		}
	}
}


void
filt_conf_chgidev(struct filt *o, unsigned olddev, unsigned newdev)
{
	struct filtsrc *i;
	
	for (i = o->srclist; i != NULL; i = i->next) {
		if (i->es.dev_min != i->es.dev_max) {
			continue;
		}
		if (i->es.dev_min == olddev) {
			i->es.dev_min = i->es.dev_max = newdev;
		}
	}
}


void
filt_conf_swapich(struct filt *o, unsigned olddev, unsigned oldch,
    unsigned newdev, unsigned newch)
{
	struct filtsrc *i;
	
	for (i = o->srclist; i != NULL; i = i->next) {
		if (i->es.dev_min != i->es.dev_max ||
		    i->es.ch_min != i->es.ch_max) {
			continue;
		}
		if (i->es.dev_min == newdev && i->es.ch_min == newch) {
			i->es.dev_min = i->es.dev_max = olddev;
			i->es.ch_min = i->es.ch_max = oldch;
		} else if (i->es.dev_min == olddev && i->es.ch_min == oldch) {
			i->es.dev_min = i->es.dev_max = newdev;
			i->es.ch_min = i->es.ch_max = newch;
		}
	}
}


void
filt_conf_swapidev(struct filt *o, unsigned olddev, unsigned newdev)
{
	struct filtsrc *i;
	
	for (i = o->srclist; i != NULL; i = i->next) {
		if (i->es.dev_min != i->es.dev_max) {
			continue;
		}
		if (i->es.dev_min == newdev) {
			i->es.dev_min = i->es.dev_max = olddev;
		} else if (i->es.dev_min == olddev) {
			i->es.dev_min = i->es.dev_max = newdev;
		}
	}
}

void
filt_conf_chgoch(struct filt *o, unsigned olddev, unsigned oldch,
    unsigned newdev, unsigned newch)
{
	struct filtsrc *s;
	struct filtdst *d;
	
	for (s = o->srclist; s != NULL; s = s->next) {
		for (d = s->dstlist; d != NULL; d = d->next) {
			if (d->es.dev_min != d->es.dev_max ||
			    d->es.ch_min != d->es.ch_max) {
				continue;
			}
			if (d->es.dev_min == olddev && d->es.ch_min == oldch) {
				d->es.dev_min = d->es.dev_max = newdev;
				d->es.ch_min = d->es.ch_max = newch;
			}
		}
	}
}


void
filt_conf_chgodev(struct filt *o, unsigned olddev, unsigned newdev)
{
	struct filtsrc *s;
	struct filtdst *d;
	
	for (s = o->srclist; s != NULL; s = s->next) {
		for (d = s->dstlist; d != NULL; d = d->next) {
			if (d->es.dev_min != d->es.dev_max) {
				continue;
			}
			if (d->es.dev_min == olddev) {
				d->es.dev_min = d->es.dev_max = newdev;
			}
		}
	}
}


void
filt_conf_swapoch(struct filt *o, unsigned olddev, unsigned oldch,
    unsigned newdev, unsigned newch)
{
	struct filtsrc *s;
	struct filtdst *d;
	
	for (s = o->srclist; s != NULL; s = s->next) {
		for (d = s->dstlist; d != NULL; d = d->next) {
			if (d->es.dev_min != d->es.dev_max ||
			    d->es.ch_min != d->es.ch_max) {
				continue;
			}
			if (d->es.dev_min == newdev &&
			    d->es.ch_min == newch) {
				d->es.dev_min = d->es.dev_max = olddev;
				d->es.ch_min = d->es.ch_max = oldch;
			} else if (d->es.dev_min == olddev && 
			    d->es.ch_min == oldch) {
				d->es.dev_min = d->es.dev_max = newdev;
				d->es.ch_min = d->es.ch_max = newch;
			}
		}
	}
}

void
filt_conf_swapodev(struct filt *o, unsigned olddev, unsigned newdev)
{
	struct filtsrc *s;
	struct filtdst *d;
	
	for (s = o->srclist; s != NULL; s = s->next) {
		for (d = s->dstlist; d != NULL; d = d->next) {
			if (d->es.dev_min != d->es.dev_max) {
				continue;
			}
			if (d->es.dev_min == newdev) {
				d->es.dev_min = d->es.dev_max = olddev;
			} else if (d->es.dev_min == olddev) {
				d->es.dev_min = d->es.dev_max = newdev;
			}
		}
	}
}
