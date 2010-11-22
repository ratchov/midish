/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
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
 * allocate and insert a new leaf node at the given location
 */
struct filtnode *
filtnode_new(struct evspec *from, struct filtnode **loc)
{	
	struct filtnode *s;

	s = mem_alloc(sizeof(struct filtnode), "filtnode");
	s->dstlist = NULL;
	s->es = *from;
	s->next = *loc;
	*loc = s;
	return s;
}

/*
 * delete the branch at the given location
 */
void
filtnode_del(struct filtnode **loc)
{
	struct filtnode *s = *loc;

	while (s->dstlist)
		filtnode_del(&s->dstlist);
	*loc = s->next;
	mem_free(s);
}

/*
 * find a node (or create one) such that the given evspec includes the previous
 * nodes and is included in the next nodes. Remove nodes that cause conflicts.
 */
struct filtnode *
filtnode_mksrc(struct filtnode **root, struct evspec *from)
{
	struct filtnode **ps, *s;

	/*
	 * delete nodes causing conflicts
	 */
	for (ps = root; (s = *ps) != NULL;) {
		if (evspec_isec(&s->es, from) && !evspec_in(from, &s->es)) {
			if (filt_debug) {
				dbg_puts("filtnode_mksrc: ");
				evspec_dbg(&s->es);
				dbg_puts(": src intersect\n");
			}
			filtnode_del(ps);
			continue;
		}
		ps = &s->next;
	}

	/*
	 * find the correct place where to insert the source
	 */
	for (ps = root; (s = *ps) != NULL;) {
		if (evspec_eq(from, &s->es)) {
			if (filt_debug)
				dbg_puts("filtnode_mksrc: exact match\n");
			return s;
		}
		if (evspec_in(from, &s->es)) {
			if (filt_debug) {
				dbg_puts("filtnode_mksrc: ");
				evspec_dbg(from);
				dbg_puts(" in ");
				evspec_dbg(&s->es);
				dbg_puts("\n");
			}
			break;
		}
		if (filt_debug) {
			dbg_puts("filtnode_mksrc: ");
			evspec_dbg(from);
			dbg_puts(": skipped\n");
		}
		ps = &s->next;
	}
	return filtnode_new(from, ps);
}

/*
 * find a node (or create one) such that the given evspec has no intersection
 * with the nodes on the list. Remove nodes that may cause this.
 */
struct filtnode *
filtnode_mkdst(struct filtnode *s, struct evspec *to)
{
	struct filtnode *d, **pd;

	/*
	 * remove conflicting destinations
	 */
	for (pd = &s->dstlist; (d = *pd) != NULL;) {
		if (evspec_eq(&d->es, to))
			return d;
		if (evspec_isec(&d->es, to) ||
		    to->cmd == EVSPEC_EMPTY ||
		    d->es.cmd == EVSPEC_EMPTY) {
			if (filt_debug) {
				dbg_puts("filtnode_mkdst: ");
				rule_dbg(&s->es, &d->es);
				dbg_puts(": src intersect\n");
			}
			filtnode_del(pd);
			continue;
		}
		pd = &d->next;
	}
	return filtnode_new(to, pd);
}


/*
 * initialize a filter
 */
void
filt_init(struct filt *o)
{
	o->map = NULL;
	o->vcurve = NULL;
	o->transp = NULL;
}

/*
 * remove all filtering rules and all states
 */
void
filt_reset(struct filt *o)
{
	while (o->map)
		filtnode_del(&o->map);
	while (o->transp)
		filtnode_del(&o->transp);
	while (o->vcurve)
		filtnode_del(&o->vcurve);
}

/*
 * destroy a filter
 */
void
filt_done(struct filt *o)
{
	filt_reset(o);
	o->map = o->transp = o->vcurve = (void *)0xdeadbeef;
}

/*
 * return velocity adjusted by curve with the given weight.
 * the weight must be in the 1..127 range, 64 means neutral
 */
unsigned
vcurve(unsigned nweight, unsigned x)
{
	if (x == 0)
		return 0;
	nweight--;
	if (x <= nweight) {
		if (nweight == 0)
			return 127;
		else
			return 1 + (126 - nweight) * (x - 1) / nweight;
	} else {
		if (nweight == 126)
			return 1;
		else
			return 127 - nweight * (127 - x) / (126 - nweight);
	}
}

/*
 * match event against all sources and for each source
 * generate output events
 */
unsigned
filt_do(struct filt *o, struct ev *in, struct ev *out)
{
	struct ev *ev;
	struct filtnode *s;
	struct filtnode *d;
	unsigned nev, i;

	nev = 0;
	for (s = o->map;; s = s->next) {
		if (s == NULL)
			break;
		if (evspec_matchev(&s->es, in)) {
			for (d = s->dstlist; d != NULL; d = d->next) {
				if (d->es.cmd == EVSPEC_EMPTY)
					continue;
				ev_map(in, &s->es, &d->es, &out[nev]);
				if (filt_debug) {
					dbg_puts("filt_do: (");
					rule_dbg(&s->es, &d->es);
					dbg_puts("): ");
					ev_dbg(in);
					dbg_puts(" -> ");
					ev_dbg(&out[nev]);
					dbg_puts("\n");
				}
				nev++;
			}
			break;
		}
	}
	if (!EV_ISNOTE(in))
		return nev;
	for (i = 0, ev = out; i < nev; i++, ev++) {
		for (d = o->vcurve; d != NULL; d = d->next) {
			if (!evspec_matchev(&d->es, ev))
				continue;
			ev->note_vel = vcurve(d->u.vel.nweight, ev->note_vel);
			break;
		}
		for (d = o->transp; d != NULL; d = d->next) {
			if (!evspec_matchev(&d->es, ev))
				continue;
			ev->note_num += d->u.transp.plus;
			ev->note_num &= 0x7f;
			break;
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
	struct filtnode *s, **ps;
	struct filtnode *d, **pd;

	for (ps = &f->map; (s = *ps) != NULL;) {
		if (evspec_in(&s->es, from)) {
			for (pd = &s->dstlist; (d = *pd) != NULL;) {
				if (evspec_in(&d->es, to)) {
					if (filt_debug) {
						dbg_puts("filt_mapdel: ");
						rule_dbg(&s->es, &d->es);
						dbg_puts(": removed\n");
					}
					filtnode_del(pd);
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
			filtnode_del(ps);
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
	struct filtnode *s;

	if (filt_debug) {
		dbg_puts("filt_mapnew: adding ");
		rule_dbg(from, to);
		dbg_puts("\n");
	}

	/*
	 * check if ranges are ok, do nothing if they are not
	 */
	if (to->cmd != EVSPEC_EMPTY && !evspec_isamap(from, to)) 
		return;

	s = filtnode_mksrc(&f->map, from);
	filtnode_mkdst(s, to);
}

struct filtnode *
filt_detach(struct filt *o)
{
	struct filtnode *list, *s;

	for (list = NULL; (s = o->map) != NULL;) {
		o->map = s->next;
		s->next = list;
		list = s;
	}
	return list;
}

void
filt_chgin(struct filt *o, struct evspec *from, struct evspec *to, int swap)
{
	struct evspec newspec;
	struct filtnode *s, *list;
	struct filtnode *d;

	list = filt_detach(o);
	while ((s = list) != NULL) {
		if (evspec_in(&s->es, from)) {
			evspec_map(&s->es, from, to, &newspec);
		} else if (swap && evspec_in(&s->es, to)) {
			evspec_map(&s->es, to, from, &newspec);
		} else {
			newspec = s->es;
		}
		if (filt_debug) {
			dbg_puts("filt_chgin: ");
			evspec_dbg(&s->es);
			dbg_puts(" -> ");
			evspec_dbg(&newspec);
			dbg_puts("\n");
		}
		while ((d = s->dstlist) != NULL) {
			filt_mapnew(o, &newspec, &d->es);
			filtnode_del(&s->dstlist);
		}
		filtnode_del(&list);
	}
}

void
filt_chgout(struct filt *o, struct evspec *from, struct evspec *to, int swap)
{
	struct evspec newspec;
	struct filtnode *s, *list;
	struct filtnode *d;

	list = filt_detach(o);
	while ((s = list) != NULL) {
		while ((d = s->dstlist) != NULL) {
			if (evspec_in(&d->es, from)) {
				evspec_map(&d->es, from, to, &newspec);
			} else if (swap && evspec_in(&d->es, to)) {
				evspec_map(&d->es, to, from, &newspec);
			} else {
				newspec = d->es;
			}
			if (filt_debug) {
				dbg_puts("filt_chgout: ");
				evspec_dbg(&d->es);
				dbg_puts(" -> ");
				evspec_dbg(&newspec);
				dbg_puts("\n");
			}
			filt_mapnew(o, &s->es, &newspec);
			filtnode_del(&s->dstlist);
		}
		filtnode_del(&list);
	}
}

void
filt_transp(struct filt *f, struct evspec *from, int plus)
{
	struct filtnode *s;

	if (from->cmd != EVSPEC_ANY && from->cmd != EVSPEC_NOTE) {
		dbg_puts("filt_transp: set must contain notes\n");
		return;
	}
	if (from->cmd == EVSPEC_NOTE && 
	    (from->v0_min != 0 || from->v0_max != EV_MAXCOARSE)) {
		dbg_puts("filt_transp: note range must be full\n");
		return;
	}

	s = filtnode_mksrc(&f->transp, from);
	s->u.transp.plus = plus & 0x7f;
}

void
filt_vcurve(struct filt *f, struct evspec *from, int weight)
{
	struct filtnode *s;

	if (from->cmd != EVSPEC_ANY && from->cmd != EVSPEC_NOTE) {
		dbg_puts("filt_vcurve: set must contain notes\n");
		return;
	}
	s = filtnode_mksrc(&f->vcurve, from);
	s->u.vel.nweight = (64 - weight) & 0x7f;
}
