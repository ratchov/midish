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

#include "dbg.h"
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
	if (mixout_debug) {
		dbg_puts("mixout_start()\n");
	}
}

void
mixout_stop(void)
{
	if (mixout_debug) {
		dbg_puts("mixout_stop()\n");
	}
	timo_del(&mixout_timo);
	statelist_done(&mixout_slist);
}

void
mixout_putev(struct ev *ev, unsigned id)
{
	struct state *os;
	struct ev ca;

	os = statelist_lookup(&mixout_slist, ev);
	if (os != NULL && os->tag != id) {
		if (os->tag < id) {
			if (mixout_debug) {
				dbg_puts("mixout_putev: [");
				dbg_putu(id);
				dbg_puts("] ");
				ev_dbg(ev);
				dbg_puts(" ignored\n");
			}
			return;
		}
		if (state_cancel(os, &ca)) {
			if (mixout_debug) {
				dbg_puts("mixout_putev: [");
				dbg_putu(os->tag);
				dbg_puts("] ");
				ev_dbg(ev);
				dbg_puts(" kicked\n");
			}
			os = statelist_update(&mixout_slist, &ca);
			mux_putev(&ca);
		}
		if (mixout_debug) {
			dbg_puts("mixout_putev: ");
			ev_dbg(ev);
			dbg_puts(" won\n");
		}
	}
	os = statelist_update(&mixout_slist, ev);
	os->tag = id;
	os->tic = 0;
	mux_putev(ev);
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
				if (mixout_debug) {
					dbg_puts("mixout_timo: ");
					state_dbg(i);
					dbg_puts(": timed out\n");
				}
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
