/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * filt.c
 *
 * a simple midi filter. Rewrites input events according a set
 * of user-configurable rules.
 *
 */

#include "utils.h"
#include "ev.h"
#include "filt.h"
#include "pool.h"
#include "mux.h"
#include "cons.h"

unsigned filt_debug = 0;

void
rule_log(struct evspec *from, struct  evspec *to)
{
	logx(1, "{evspec:%p} -> {evspec:%p}", from, to);
}

/*
 * allocate and insert a new leaf node at the given location
 */
struct filtnode *
filtnode_new(struct evspec *from, struct filtnode **loc)
{
	struct filtnode *s;

	s = xmalloc(sizeof(struct filtnode), "filtnode");
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
	xfree(s);
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
				logx(1, "%s: {evspec:%p}: src intersect", __func__, &s->es);
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
				logx(1, "%s: exact match", __func__);
			return s;
		}
		if (evspec_in(from, &s->es)) {
			if (filt_debug)
				logx(1, "%s: {evspec:%p} in {evspec:%p}", __func__, from, &s->es);
			break;
		}
		if (filt_debug) {
			logx(1, "%s: {evspec:%p}: skipped", __func__, from);
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
				logx(1, "%s: {evspec:%p} > {evspec:%p}: src intersect",
				    __func__, &s->es, &d->es);
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
		if (filt_debug)
			logx(1, "%s: in = {ev:%p}", __func__, in);
		if (evspec_matchev(&s->es, in)) {
			for (d = s->dstlist; d != NULL; d = d->next) {
				if (d->es.cmd == EVSPEC_EMPTY)
					continue;
				ev_map(in, &s->es, &d->es, &out[nev]);
				if (filt_debug) {
					logx(1, "%s: "
					    "{evspec:%p} > {evspec:%p}: "
					    "{ev:%p} -> {ev:%p}", __func__,
					    &s->es, &d->es, in, &out[nev]);
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
						logx(1, "%s: "
						    "{evspec:%p} > {evspec:%p}: removed",
						    __func__, &s->es, &d->es);
					}
					filtnode_del(pd);
					continue;
				}
				pd = &d->next;
			}
		}
		if (s->dstlist == NULL) {
			if (filt_debug) {
				logx(1, "%s: {evspec:%p}: empty, removed", __func__, &s->es);
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

	if (filt_debug)
		logx(1, "%s: {evspec:%p} > {evspec:%p}: added", __func__, from, to);

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
			logx(1, "%s: {evspec:%p} -> {evspec:%p}",
			    __func__, &s->es, &newspec);
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
				logx(1, "%s: {evspec:%p} -> {evspec:%p}",
				    __func__, &d->es, &newspec);
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
		logx(1, "%s: set must contain notes", __func__);
		return;
	}
	if (from->cmd == EVSPEC_NOTE &&
	    (from->v0_min != 0 || from->v0_max != EV_MAXCOARSE)) {
		logx(1, "%s: note range must be full", __func__);
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
		logx(1, "%s: set must contain notes", __func__);
		return;
	}
	s = filtnode_mksrc(&f->vcurve, from);
	s->u.vel.nweight = (64 - weight) & 0x7f;
}

unsigned
filt_evcnt(struct filt *f, unsigned cmd)
{
	struct filtnode *s;
	struct filtnode *d;
	unsigned cnt = 0;

	for (s = f->map; s != NULL; s = s->next) {
		if (s->es.cmd == cmd)
			cnt++;
		for (d = s->dstlist; d != NULL; d = d->next) {
			if (d->es.cmd == cmd)
				cnt++;
		}
	}
	for (s = f->vcurve; s != NULL; s = s->next) {
		if (s->es.cmd == cmd)
			cnt++;
	}
	for (s = f->transp; s != NULL; s = s->next) {
		if (s->es.cmd == cmd)
			cnt++;
	}

	return cnt;
}
