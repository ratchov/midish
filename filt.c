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
 * transform "in" (matching "from" spec) into "out" so it matches "to"
 * spec. The "from" and "to" specs _must_ have the same dev, ch, v0 and
 * v1 ranges (to ensure the mapping must be bijective).  This routine is
 * supposed to be fast since it's called for each incoming event.
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
		dbg_puts("filt_mapev: (");
		rule_dbg(from, to);
		dbg_puts("): ");
		ev_dbg(in);
		dbg_puts(" -> ");
		ev_dbg(out);
		dbg_puts("\n");
	}
}

/*
 * transform "in" spec (included in "from" spec) into "out" spec
 * (included in "to" spec). This routine works in exactly the same way
 * as filt_evmap() but for specs instead of events; so it has the same
 * semantics and constraints.
 */
void
filt_mapspec(struct evspec *from, struct evspec *to,
    struct evspec *in, struct evspec *out)
{
	*out = *in;
	if (to->cmd != EVSPEC_ANY && to->cmd != EVSPEC_NOTE)
		out->cmd = to->cmd;
	if ((evinfo[from->cmd].flags & EV_HAS_DEV) &&
	    (evinfo[in->cmd].flags & EV_HAS_DEV)) {
		out->dev_min += to->dev_min - from->dev_min;
		out->dev_max += to->dev_min - from->dev_min;
	}
	if ((evinfo[from->cmd].flags & EV_HAS_CH) &&
	    (evinfo[in->cmd].flags & EV_HAS_CH)) {
		out->ch_min += to->ch_min - from->ch_min;
		out->ch_max += to->ch_min - from->ch_min;
	}
	if (evinfo[from->cmd].nranges > 0 &&
	    evinfo[in->cmd].nranges > 0) {
		out->v0_min += to->v0_min - from->v0_min;
		out->v0_max += to->v0_min - from->v0_min;
	}
	if (evinfo[from->cmd].nranges > 1 &&
	    evinfo[in->cmd].nranges > 1) {
		out->v1_min += to->v1_min - from->v1_min;
		out->v1_max += to->v1_min - from->v1_min;
	}
	if (filt_debug) {
		dbg_puts("filt_mapspec: (");
		rule_dbg(from, to);
		dbg_puts("): ");
		evspec_dbg(in);
		dbg_puts(" -> ");
		evspec_dbg(out);
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
	o->vcurve = NULL;
	o->transp = NULL;
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
	struct ev *ev;
	struct filtsrc *s;
	struct filtdst *d;
	unsigned nev, i;

	nev = 0;
	for (s = o->srclist;; s = s->next) {
		if (s == NULL)
			break;
		if (evspec_matchev(&s->es, in)) {
			for (d = s->dstlist; d != NULL; d = d->next) {
				filt_mapev(&s->es, &d->es, in, &out[nev]);
				nev++;
			}
			break;
		}
	}
	for (d = o->transp; d != NULL; d = d->next) {
		for (i = 0; i < nev; i++) {
			ev = &out[i];
			if (EV_ISNOTE(ev) && evspec_matchev(&d->es, ev)) {
				ev->note_num += d->u.transp.plus;
				ev->note_num &= 0x7f;
			}
		}
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

struct filtsrc *
filt_movelist(struct filt *o)
{
	struct filtsrc *list, *s;

	for (list = NULL; (s = o->srclist) != NULL;) {
		o->srclist = s->next;
		s->next = list;
		list = s;
	}
	return list;
}

void
filt_chgin(struct filt *o, struct evspec *from, struct evspec *to, int swap)
{
	struct evspec newspec;
	struct filtsrc *s, *list;
	struct filtdst *d;

	list = filt_movelist(o);
	while ((s = list) != NULL) {
		if (evspec_in(&s->es, from)) {
			filt_mapspec(from, to, &s->es, &newspec);
		} else if (swap && evspec_in(&s->es, to)) {
			filt_mapspec(to, from, &s->es, &newspec);
		} else {
			newspec = s->es;
		}
		while ((d = s->dstlist) != NULL) {
			filt_mapnew(o, &newspec, &d->es);
			s->dstlist = d->next;
			mem_free(d);
		}
		list = s->next;
		mem_free(s);
	}
}

void
filt_chgout(struct filt *o, struct evspec *from, struct evspec *to, int swap)
{
	struct evspec newspec;
	struct filtsrc *s, *list;
	struct filtdst *d;

	list = filt_movelist(o);
	while ((s = list) != NULL) {
		while ((d = s->dstlist) != NULL) {
			if (evspec_in(&d->es, from)) {
				filt_mapspec(from, to, &d->es, &newspec);
			} else if (swap && evspec_in(&d->es, to)) {
				filt_mapspec(to, from, &d->es, &newspec);
			} else {
				newspec = d->es;
			}
			filt_mapnew(o, &s->es, &newspec);
			s->dstlist = d->next;
			mem_free(d);
		}
		list = s->next;
		mem_free(s);
	}
}

void
filt_transp(struct filt *f, struct evspec *to, int plus)
{
	struct filtdst *d, **pd;

	if (filt_debug) {
		dbg_puts("filt_transp:  ");
		evspec_dbg(to);
		dbg_puts(" ");
		dbg_putu(plus & 0x7f);
		dbg_puts("\n");		
	}
	if (to->cmd != EVSPEC_ANY && to->cmd != EVSPEC_NOTE) {
		dbg_puts("filt_transp: set must contain notes\n");
		return;
	}
	if (to->cmd == EVSPEC_NOTE && 
	    (to->v0_min != 0 || to->v0_max != EV_MAXCOARSE)) {
		dbg_puts("filt_transp: note range must be full\n");
		return;
	}
	for (pd = &f->transp; (d = *pd) != NULL;) {
		if (evspec_isec(&d->es, to)) {
			if (filt_debug) {
				dbg_puts("filt_transp: ");
				evspec_dbg(&d->es);
				dbg_puts(": dst intersect\n");
			}
			*pd = d->next;
			mem_free(d);
			continue;
		}
		pd = &d->next;
	}
	if (plus == 0)
		return;

	/*
	 * add the new destination to the end of the list,
	 * in order to obtain the same order as the order in
	 * which rules are added
	 */
	d = (struct filtdst *)mem_alloc(sizeof(struct filtdst));
	d->es = *to;
	d->u.transp.plus = plus & 0x7f;
	for (pd = &f->transp; *pd != NULL; pd = &(*pd)->next) {
		/* nothing */
	}
	d->next = NULL;
	*pd = d;
}
