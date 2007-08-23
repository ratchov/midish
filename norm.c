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
 * norm.c
 *
 * a stateful midi normalizer. It's used to normalize/sanitize midi
 * input
 *
 */
 
#include "dbg.h"
#include "ev.h"
#include "norm.h"
#include "pool.h"
#include "mux.h"

struct song;
extern struct song *user_song;

#define TAG_PASS 1
#define TAG_PENDING 2

/*
 * timeout for throtteling: 1 tick at 60 bpm
 */
#define NORM_TIMO TEMPO_TO_USEC24(120,24)

unsigned norm_debug = 0;

/* --------------------------------------------------------------------- */

void norm_timocb(void *);


/*
 * configure the normalizer so that output events are passed to the
 * given callback
 */
void
norm_start(struct norm *o) {
	statelist_init(&o->statelist);
	timo_set(&o->timo, norm_timocb, o);
	timo_add(&o->timo, NORM_TIMO);
	if (norm_debug) {
		dbg_puts("norm_start()\n");
	}
}

/*
 * unconfigure the normalizer
 */
void
norm_stop(struct norm *o) {
	if (norm_debug) {
		dbg_puts("norm_stop()\n");
	}
	timo_del(&o->timo);
	statelist_done(&o->statelist);
}

/*
 * shuts all notes and restores the default values
 * of the modified controllers, the bender etc...
 */
void
norm_shut(struct norm *o) {
	struct state *s, *snext;
	struct ev ca;
	
	for (s = o->statelist.first; s != NULL; s = snext) {
		snext = s->next;
		if (state_cancel(s, &ca)) {
			if (norm_debug) {
				dbg_puts("norm_shut: ");
				ev_dbg(&s->ev);
				dbg_puts(": cancelled by: ");
				ev_dbg(&ca);
				dbg_puts("\n");
			}
			s = statelist_update(&o->statelist, &ca);
			song_evcb(user_song, &s->ev);
		}
	}
}

/*
 * kill all tagged frames matching the given event (because a bogus
 * event was received)
 */
void
norm_kill(struct norm *o, struct ev *ev) {
	struct state *st, *stnext;
	struct ev ca;

	for (st = o->statelist.first; st != NULL; st = stnext) {
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
			st = statelist_update(&o->statelist, &ca);
			song_evcb(user_song, &st->ev);
		}
		st->tag &= ~TAG_PASS;
		dbg_puts("norm_kill: ");
		ev_dbg(&st->ev);
		dbg_puts(": killed\n");
	}
}

/*
 * give an event to the normalizer for processing
 */
void
norm_evcb(struct norm *o, struct ev *ev) {
	struct state *st;
	
	if (norm_debug) {
		dbg_puts("norm_run: ");
		ev_dbg(ev);
		dbg_puts("\n");
	}
#ifdef NORM_DEBUG
	if (o->cb == NULL) {
		dbg_puts("norm_evcb: cb = NULL, bad initialisation\n");
		dbg_panic();
	}
	if (!EV_ISVOICE(ev)) {
		dbg_puts("norm_evcb: only voice events allowed\n");
		dbg_panic();
	}
#endif

	/*
	 * create/update state for this event
	 */
	st = statelist_update(&o->statelist, ev);
	if (st->phase & EV_PHASE_FIRST) {
		if (st->flags & STATE_NEW)
			st->nevents = 0;

		st->tag = TAG_PASS;
		if (st->flags & (STATE_BOGUS | STATE_NESTED)) {
			if (norm_debug) {
				dbg_puts("norm_evcb: ");
				ev_dbg(ev);
				dbg_puts(": bogus/nested frame\n");
			}
			norm_kill(o, ev);
		}
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

	song_evcb(user_song, &st->ev);
	st->nevents++;
}

/*
 * timeout: outdate all events, so throttling will enable
 * output again.
 */
void
norm_timocb(void *addr) {
	struct norm *o = (struct norm *)addr;
	struct state *i;

	statelist_outdate(&o->statelist);
	for (i = o->statelist.first; i != NULL; i = i->next) {
		i->nevents = 0;
		if (i->tag & TAG_PENDING) {
			i->tag &= ~TAG_PENDING;
			song_evcb(user_song, &i->ev);
			i->nevents++;
		}
	}
	timo_add(&o->timo, NORM_TIMO);
}
