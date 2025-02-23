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
 * norm.c
 *
 * a stateful midi normalizer. It's used to normalize/sanitize midi
 * input
 *
 */

#include "utils.h"
#include "ev.h"
#include "norm.h"
#include "pool.h"
#include "mux.h"
#include "filt.h"
#include "mixout.h"

struct song;

#define TAG_PASS 1
#define TAG_PENDING 2

/*
 * timeout for throtteling: 1 tick at 60 bpm
 */
#define NORM_TIMO TEMPO_TO_USEC24(120,24)

unsigned norm_debug = 0;
struct statelist norm_slist;		/* state of the normilizer */
struct timo norm_timo;			/* for throtteling */

/* --------------------------------------------------------------------- */

void norm_timocb(void *);

/*
 * inject an event
 */
void
norm_putev(struct ev *ev)
{
	if (!EV_ISVOICE(ev) && !EV_ISSX(ev))
		return;
	song_evcb(usong, ev);
	mux_flush();
}

/*
 * configure the normalizer so that output events are passed to the
 * given callback
 */
void
norm_start(void)
{
	statelist_init(&norm_slist);
	timo_set(&norm_timo, norm_timocb, NULL);
	timo_add(&norm_timo, NORM_TIMO);
	if (norm_debug) {
		log_puts("norm_start()\n");
	}
}

/*
 * unconfigure the normalizer
 */
void
norm_stop(void)
{
	struct state *s, *snext;
	struct ev ca;

	if (norm_debug) {
		log_puts("norm_stop()\n");
	}
	for (s = norm_slist.first; s != NULL; s = snext) {
		snext = s->next;
		if (state_cancel(s, &ca)) {
			if (norm_debug) {
				log_puts("norm_stop: ");
				ev_log(&s->ev);
				log_puts(": cancelled by: ");
				ev_log(&ca);
				log_puts("\n");
			}
			s = statelist_update(&norm_slist, &ca);
			norm_putev(&s->ev);
		}
	}
	timo_del(&norm_timo);
	statelist_done(&norm_slist);
}

/*
 * shuts all notes and restores the default values
 * of the modified controllers, the bender etc...
 */
void
norm_shut(void)
{
	struct state *s, *snext;
	struct ev ca;

	for (s = norm_slist.first; s != NULL; s = snext) {
		snext = s->next;
		if (!(s->tag & TAG_PASS))
			continue;
		if (state_cancel(s, &ca)) {
			if (norm_debug) {
				log_puts("norm_shut: ");
				ev_log(&s->ev);
				log_puts(": cancelled by: ");
				ev_log(&ca);
				log_puts("\n");
			}
			s = statelist_update(&norm_slist, &ca);
			norm_putev(&s->ev);
		}
		s->tag &= ~TAG_PASS;
	}
}

/*
 * kill all tagged frames matching the given event (because a bogus
 * event was received)
 */
void
norm_kill(struct ev *ev)
{
	struct state *st, *stnext;
	struct ev ca;

	for (st = norm_slist.first; st != NULL; st = stnext) {
		stnext = st->next;
		if (!state_match(st, ev) ||
		    !(st->tag & TAG_PASS) ||
		    st->phase & EV_PHASE_LAST) {
			continue;
		}
		/*
		 * cancel/untag the frame and change the phase to
		 * EV_PHASE_LAST, so the state can be deleted if
		 * necessary
		 */
		if (state_cancel(st, &ca)) {
			st = statelist_update(&norm_slist, &ca);
			norm_putev(&st->ev);
		}
		st->tag &= ~TAG_PASS;
		log_puts("norm_kill: ");
		ev_log(&st->ev);
		log_puts(": killed\n");
	}
}

/*
 * give an event to the normalizer for processing
 */
void
norm_evcb(struct ev *ev)
{
	struct state *st;

	if (norm_debug) {
		log_puts("norm_run: ");
		ev_log(ev);
		log_puts("\n");
	}
#ifdef NORM_DEBUG
	if (!EV_ISVOICE(ev) && !EV_ISSX(ev)) {
		log_puts("norm_evcb: only voice events allowed\n");
		panic();
	}
#endif

	/*
	 * create/update state for this event
	 */
	st = statelist_update(&norm_slist, ev);
	if (st->phase & EV_PHASE_FIRST) {
		if (st->flags & STATE_NEW)
			st->nevents = 0;

		if (st->flags & (STATE_BOGUS | STATE_NESTED)) {
			st->tag = 0;
			if (norm_debug) {
				log_puts("norm_evcb: ");
				ev_log(ev);
				log_puts(": bogus/nested frame\n");
			}
			norm_kill(ev);
		} else
			st->tag = TAG_PASS;
	}

	/*
	 * nothing to do with silent (not selected) frames
	 */
	if (!(st->tag & TAG_PASS))
		return;

	/*
	 * throttling: if we played more than MAXEV
	 * events skip this event only if it doesnt change the
	 * phase of the frame
	 */
	if (st->nevents > NORM_MAXEV &&
	    (st->phase == EV_PHASE_NEXT ||
	     st->phase == (EV_PHASE_FIRST | EV_PHASE_LAST))) {
		st->tag |= TAG_PENDING;
		return;
	}

	norm_putev(&st->ev);
	st->nevents++;
}

/*
 * timeout: outdate all events, so throttling will enable
 * output again.
 */
void
norm_timocb(void *addr)
{
	struct state *i;

	statelist_outdate(&norm_slist);
	for (i = norm_slist.first; i != NULL; i = i->next) {
		i->nevents = 0;
		if (i->tag & TAG_PENDING) {
			i->tag &= ~TAG_PENDING;
			norm_putev(&i->ev);
			i->nevents++;
		}
	}
	timo_add(&norm_timo, NORM_TIMO);
}
