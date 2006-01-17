/*
 * Copyright (c) 2003-2005 Alexandre Ratchov
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
 * implements single frame track operations. A frame is
 * a set of events (ie a "subtrack") with a relation between them.
 * Example a sequence of non/noff, a bender curve etc...
 */

#include "dbg.h"
#include "track.h"
#include "default.h"
#include "frame.h"

	/*
	 * extract the frame at the current position from the given track.
	 * If the frame is incomplete, the returned frame is empty.
	 */
void
track_frameget(struct track_s *o, struct seqptr_s *p, struct track_s *frame) {
	struct seqptr_s op, fp;
	struct seqev_s *se;
	struct ev_s *state;
	unsigned tics;
	unsigned phase;
	
	op = *p;
	track_clear(frame, &fp);
	tics = 0;

	if (!track_evavail(o, &op)) {
		dbg_puts("track_frameget: bad position\n");
		dbg_panic();
	}

	state = &(*op.pos)->ev;
	phase = ev_phase(state);
	if (!(phase & EV_PHASE_FIRST)) {
		dbg_puts("track_frameget: not first:");
		ev_dbg(&(*op.pos)->ev);
		dbg_puts("\n");
		se = track_seqevrm(o, &op);
		seqev_del(se);		
		return;	
	}

	/* move the event */ 
	se = track_seqevrm(o, &op);
	track_seqevins(frame, &fp, se);
	track_seqevnext(frame, &fp);

	while (!(phase & EV_PHASE_LAST)) {
		
		for (;;) {
			tics += track_ticlast(o, &op);
			/* check for end of track */
			if (!track_evavail(o, &op)) {
				dbg_puts("track_frameget: truncated frame, ignored\n");
				track_clear(frame, &fp);
				return;
			}
			if (ev_sameclass(&(*op.pos)->ev, state)) {
				break;
			}
			track_evnext(o, &op);
		}

		state = &(*op.pos)->ev;
		phase = ev_phase(state);
		if (!(phase & EV_PHASE_NEXT || phase & EV_PHASE_LAST)) {
			dbg_puts("track_frameget: nested frames, skiped: ");
			ev_dbg(&(*op.pos)->ev);
			dbg_puts("\n");
			track_evdel(o, &op);
			continue;
		}
		
		/* move the event */ 
		track_seekblank(frame, &fp, tics);
		track_evlast(frame, &fp);
		se = track_seqevrm(o, &op);
		track_seqevins(frame, &fp, se);
		track_seqevnext(frame, &fp);
		tics = 0;
	}
}


	/*
	 * merges the given frame into the track
	 *
	 * Warning 1: there is no checking for
	 * 	nested notes.
	 *
	 * Warning 2: 
	 *	currently frameput just copies events one-by one
	 *	without considering the frame structure. If you
	 *	change this behaviour in the future, please verify
	 *	that frameput isn't used elsewhere for copying
	 *	tracks (ex: in song_record)
	 */

void
track_frameput(struct track_s *o, struct seqptr_s *p, struct track_s *frame) {
	struct seqptr_s op, fp;
	struct seqev_s *se;
	unsigned tics;
	
	op = *p;
	track_rew(frame, &fp);

	for (;;) {
		tics = track_ticlast(frame, &fp);
		track_seekblank(o, &op, tics);
		
		if (!track_evavail(frame, &fp)) {
			break;
		}
		/* move to the last ev. in the current position */
		track_evlast(o, &op);
		/* move the event */ 
		se = track_seqevrm(frame, &fp);
		track_seqevins(o, &op, se);
		track_seqevnext(o, &op);
	}
	track_clear(frame, &fp);
}


	/*
	 * cut a portion of the given frame (events and blank space)
	 * the frame must not start after the start of the
	 * position to cut.
	 */
void
track_framecut(struct track_s *o, unsigned tic, unsigned start, unsigned len) {
	struct seqptr_s op;
	struct ev_s st1, st2;	
	
	if (tic > start) {
		dbg_puts("track_framecut: missed the start tic\n");
		dbg_panic();
	}
	st1.cmd = EV_NULL;
	st2.cmd = EV_NULL;
	track_rew(o, &op);
	
	/*
	 * move to the begging of the start position
	 */	
	for (;;) {
		tic += track_ticskipmax(o, &op, start - tic);
		if (tic == start) {
			break;
		}
		if (!track_evavail(o, &op)) {
			return;
		}
		while(track_evavail(o, &op)) {
			st1 = (*op.pos)->ev;
			track_evnext(o, &op);
		}
	}

	/*
	 * delete time and events during next 'len' tics
	 */		
	for (;;) {
		len -= track_ticdelmax(o, &op, len);
		if (len == 0) {
			break;
		}
		if (!track_evavail(o, &op)) {
			goto restore;
		}
		while(track_evavail(o, &op)) {
			st2 = (*op.pos)->ev;
			track_evdel(o, &op);
		}
	}
	
	/*
	 * if there is no event available, restore the state
	 */
	if (track_ticavail(o, &op) || !track_evavail(o, &op)) {
	restore:
		if ((st2.cmd != EV_NULL) && 
		    (st1.cmd != EV_NULL || st1.cmd != st2.cmd)) {
			track_evput(o, &op, &st2);
		}
	}
}
