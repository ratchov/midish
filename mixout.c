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
 * mixout.c
 *
 * mixes multiple sources and send the result tout the midi output, a
 * state list is maintained to avoid conflict.
 *
 * each source has it's ID, if there are conflicting events from
 * different sources (differents IDs), then the lower ID may cancel
 * the higher ID source.
 *
 * For continuous controllers terminated states are kept for around 1
 * second, this will block events from other lower prority
 * sources. For instance, this can be used to adjust a controller in
 * realtime while a track using the same controller is playing (input
 * ID is zero, and has precedence over tracks).
 *
 */

#include "utils.h"
#include "ev.h"
#include "filt.h"
#include "pool.h"
#include "mux.h"
#include "timo.h"
#include "state.h"

#define MIXOUT_TIMO (1000000UL)
#define MIXOUT_MAXTICS 24

void mixout_timocb(void *);

struct statelist mixout_slist;
struct timo mixout_timo;
unsigned mixout_debug = 0;

void
mixout_start(void)
{
	statelist_init(&mixout_slist);
	timo_set(&mixout_timo, mixout_timocb, NULL);
	timo_add(&mixout_timo, MIXOUT_TIMO);
	if (mixout_debug)
		logx(1, "%s", __func__);
}

void
mixout_stop(void)
{
	if (mixout_debug)
		logx(1, "%s", __func__);

	timo_del(&mixout_timo);
	statelist_done(&mixout_slist);
}

void
mixout_putev(struct ev *ev, unsigned id)
{
	struct state *os;
	struct ev ca;

	if (mixout_debug >= 3)
		logx(1, "%s: {ev:%p} (%u)", __func__, ev, id);

	os = statelist_lookup(&mixout_slist, ev);
	if (os != NULL && os->tag != id) {
		if (os->tag < id) {
			if (mixout_debug) {
				logx(1, "%s: {ev:%p} (%d): ignored after {ev:%p} (%d)",
				    __func__, ev, id, &os->ev, os->tag);
			}
			return;
		}
		if (state_cancel(os, &ca)) {
			if (mixout_debug) {
				logx(1, "%s: {ev:%p} (%d): will kick older {ev:%p} (%d)",
				    __func__, ev, id, &os->ev, os->tag);
			}
			statelist_update(&mixout_slist, &ca);
			mux_putev(&ca);
		}
		if (mixout_debug)
			logx(1, "%s: {ev:%p}: won", __func__, ev);
	}
	os = statelist_update(&mixout_slist, ev);
	os->tag = id;
	os->tic = 0;
	if ((os->flags & (STATE_BOGUS | STATE_NESTED)) == 0)
		mux_putev(ev);
	else {
		if (mixout_debug)
			logx(1, "%s: {ev:%p} nested or bogus", __func__, ev);
	}
}

void
mixout_timocb(void *addr)
{
	struct state *i, *inext;

	for (i = mixout_slist.first; i != NULL; i = inext) {
		inext = i->next;
		/*
		 * purge states that are no more used
		 */
		if (i->phase == EV_PHASE_LAST) {
			statelist_rm(&mixout_slist, i);
			state_del(i);
		} else if (i->phase == (EV_PHASE_FIRST | EV_PHASE_LAST)) {
			if (i->tic >= MIXOUT_MAXTICS) {
				if (mixout_debug >= 2)
					logx(1, "%s: {state:%p}: timed out", __func__, i);

				statelist_rm(&mixout_slist, i);
				state_del(i);
			} else {
				i->flags &= ~STATE_CHANGED;
				i->tic++;
			}
		}
	}
	timo_add(&mixout_timo, MIXOUT_TIMO);
}
