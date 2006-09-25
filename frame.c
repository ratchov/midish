/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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
 * this module implements single frame track operations. A frame is
 * a set of events (ie a "subtrack") with a relation between them. For
 * instance: a sequence of non/noff, a bender curve etc...
 */

/*
 * TODO:
 * 	- make track_frameget() skip duplicate events within the
 *	  same tick. For instance, save the "state" and don't
 *	  put the event into the frame until the end of the tic.
 *
 *	- make track_frameget() merge adjacent frames togother,
 *	  for instance, save the "state" and dont put the event
 *	  into the frame until the end of the tic. If the tic
 *	  contains the beginning of a new frame after the last 
 *	  event of the first frame, then just update the saved
 *	  state and continue.
 *
 *	- create a new function: track_frameget2(), that does
 *	  the same thing that track_frameget(), be takes 2 input
 *	  tracks. If there is a conflict between the events of
 *	  the couple of tracks, then merge them and return a
 *	  single frame.
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
track_frameget(struct track *o, struct seqptr *p, struct track *frame) {
	struct seqptr op, fp;
	struct seqev *se;
	struct ev *state;
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
				dbg_puts("track_frameget: truncated frame\n");
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
track_frameput(struct track *o, struct seqptr *p, struct track *frame) {
	struct seqptr op, fp;
	struct seqev *se;
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
	 * same as track_frameget, but 
	 * if the same type of frame is found several times
	 * in the same tic, only the latest one is kept.
	 */

void
track_frameuniq(struct track *o, struct seqptr *p, struct track *frame) {	
	struct seqptr op, fp;
	unsigned delta;
	struct ev ev;	
	
	track_frameget(o, p, frame);
	op = *p;
	track_rew(frame, &fp);
	if (!track_evavail(frame, &fp)) {
		return;
	}
	track_evget(frame, &fp, &ev);
	for (;;) {
		delta = track_ticlast(o, &op);
		if (delta != 0 || !track_evavail(o, &op)) {
			break;
		}
		if (ev_sameclass(&ev,  &(*op.pos)->ev)) {
			track_frameget(o, &op, frame);
		} else {
			track_evnext(o, &op);
		}
	}
}


	/*
	 * make a verbatim copy of a frame
	 */

void
track_framedup(struct track *s, struct track *d) {
	struct seqptr sp, dp;
	struct ev ev;
	unsigned tics;
		
	track_rew(s, &sp);
	track_rew(d, &dp);
	for (;;) {
		tics = track_ticlast(s, &sp);
		track_seekblank(d, &dp, tics);
		if (!track_evavail(s, &sp)) {
			break;
		}
		track_evget(s, &sp, &ev);
		track_evput(d, &dp, &ev);
	}
}

	/*
	 * cut a portion of the given frame (events and blank space)
	 */
void
track_framecut(struct track *o, unsigned start, unsigned len) {
	struct seqptr op;
	struct ev st1, st2;
	unsigned tic, phase;
	
	tic = 0;
	track_rew(o, &op);

	/*
	 * if the frame is a NOTE and the first tic is to be deleted
	 * then we delete de whole note frame, else let it as-is
	 */
	if ((*op.pos)->ev.cmd == EV_NON) {
		if (start == 0) {
			track_clear(o, &op);
		}
		return;
	}
	
	st1.cmd = EV_NULL;	/* EV_NULL means that st1 isn't set */
	st2.cmd = EV_NULL;

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
	 * if the next event is the last event of a multi-event frame, 
	 * then leave it as-is because, it is necessary to close the frame.
	 */
	if (st1.cmd != EV_NULL && track_evavail(o, &op)) {
		phase = ev_phase(&(*op.pos)->ev);
		if (phase & EV_PHASE_LAST) {
			return;
		}
	}
	
	/*
	 * delete time and events during the next 'len' tics
	 */		
	for (;;) {
		len -= track_ticdelmax(o, &op, len);
		if (len == 0) {
			break;
		}
		if (!track_evavail(o, &op)) {
			if (st1.cmd != NULL && st2.cmd != EV_NULL) {
				track_evput(o, &op, &st2);
			}
			return;
		}
		while(track_evavail(o, &op)) {
			st2 = (*op.pos)->ev;
			track_evdel(o, &op);
		}
	}
	
	if (track_ticavail(o, &op)) {
		/* 
		 * if we deleted events from a multi-event frame, 
		 * then restore the state, so it can continue properly
		 */
		if (st2.cmd != EV_NULL && (st1.cmd == EV_NULL || 
		    !ev_eq(&st1, &st2))) {
			track_evput(o, &op, &st2);
		}
	} else if (track_evavail(o, &op)) {
		/*
		 * if the next event (at end position) is the last
		 * event of an entierly deleted frame, then delete if
		 */
		if (st1.cmd == EV_NULL) {
			phase = ev_phase(&(*op.pos)->ev);
			if (phase & EV_PHASE_LAST) {
				track_evdel(o, &op);
			}
		}
	}
}


	/*
	 * insert blank space in the middle of the given frame
	 */

void
track_frameins(struct track *o, unsigned start, unsigned len) {
	struct seqptr op;
	struct ev st1, st2, ca;	
	unsigned tic, phase;
	
	tic = 0;
	track_rew(o, &op);

	/*
	 * don't change note lengths
	 */
	if ((*op.pos)->ev.cmd == EV_NON) {
		if (start == 0) {
			track_ticinsmax(o, &op, len);
		}
		return;
	}
	
	st1.cmd = EV_NULL;	/* EV_NULL means that st1 isn't set */
	st2.cmd = EV_NULL;

	/*
	 * move to the begging of the 'start' position
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
	 * if the last event of a multi-event frame follows, 
	 * then leave it as-is and return
	 */
	if (st1.cmd != EV_NULL && track_evavail(o, &op)) {
		phase = ev_phase(&(*op.pos)->ev);
		if (phase & EV_PHASE_LAST) {
			return;
		}
	}
	
	/*
	 * terminate the frame at the start position
	 * and insert some blank space
	 */
	if (st1.cmd != EV_NULL && ev_cancel(&st1, &ca)) {
		track_evput(o, &op, &ca);
	}
	track_ticinsmax(o, &op, len);
	track_ticskipmax(o, &op, len);
	tic += len;
		
	/*
	 * if there is no event available, restore the state
	 */
	if (track_ticavail(o, &op) && st1.cmd != EV_NULL) {
		track_evput(o, &op, &st1);
	}
}

	/*
	 * blank (erase events but not time) a window in a frame
	 *
	 * Note: this function very similar to track_frameins(), so if
	 *	 you make changes here, check also track_frameins()
	 */

void
track_frameblank(struct track *o, unsigned start, unsigned len) {
	struct seqptr op;
	struct ev st1, st2, ca;	
	unsigned tic, phase;

	tic = 0;
	track_rew(o, &op);

	/*
	 * if the frame is a NOTE and the first tic is to be deleted
	 * then we delete de whole note frame, else we let it as-is
	 */
	if ((*op.pos)->ev.cmd == EV_NON) {
		if (start == 0) {
			track_clear(o, &op);
		}
		return;
	}
	 	
	st1.cmd = EV_NULL;	/* EV_NULL means that st1 isn't set */
	st2.cmd = EV_NULL;

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
	 * if the last event of a multi-event frame follows, 
	 * then leave it as-is and return
	 */
	if (st1.cmd != EV_NULL && track_evavail(o, &op)) {
		phase = ev_phase(&(*op.pos)->ev);
		if (phase & EV_PHASE_LAST) {
			return;
		}
	}

	/*
	 * terminate the frame at the start position of the window
	 */
	if (st1.cmd != EV_NULL && ev_cancel(&st1, &ca)) {
		track_evput(o, &op, &ca);
	}

	/*
	 * delete events during next 'len' tics
	 */		
	for (;;) {
		tic += track_ticskipmax(o, &op, start + len - tic);
		if (tic == start + len) {
			break;
		}
		if (!track_evavail(o, &op)) {
			return;
		}
		while(track_evavail(o, &op)) {
			st2 = (*op.pos)->ev;
			track_evdel(o, &op);
		}
	}
	
	if (track_ticavail(o, &op)) {
		/* 
		 * if we deleted events from a multi-event frame, 
		 * then restore the state, so it can continue properly
		 */
		if (st2.cmd != EV_NULL) {
			track_evput(o, &op, &st2);
		} else if (st1.cmd != EV_NULL) {
			track_evput(o, &op, &st1);
		}
	} else if (track_evavail(o, &op)) {
		/*
		 * if the next event (at end position) is the last
		 * event of an a partially deleted frame, then delete it
		 */
		if (st1.cmd == EV_NULL) {
			phase = ev_phase(&(*op.pos)->ev);
			if (phase & EV_PHASE_LAST) {
				track_evdel(o, &op);
			}
		}
	}
}

	/*
	 * partialy copy a frame
	 *
	 * Note: this function very similar to track_frameblank(), so if
	 *	 you make changes here, check also track_frameblank()
	 */

void
track_framecopy(struct track *o, unsigned start, unsigned len, struct track *frame) {
	struct seqptr op, fp;
	struct ev st1, st2, ca;	
	unsigned tic, delta, phase;

	tic = 0;
	track_rew(o, &op);
	track_clear(frame, &fp);
	
	/*
	 * if the frame is a NOTE and the first tic is to be copied
	 * then copy the whole note frame, else do nothing
	 */
	if ((*op.pos)->ev.cmd == EV_NON) {
		if (start == 0) {			
			track_framedup(o, frame);
		}
		return;
	}

	 	
	st1.cmd = EV_NULL;	/* EV_NULL means that st1 isn't set */
	st2.cmd = EV_NULL;

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
	 * if the last event of a multi-event frame follows, 
	 * then leave it as-is and return
	 */
	if (st1.cmd != EV_NULL && track_evavail(o, &op)) {
		phase = ev_phase(&(*op.pos)->ev);
		if (phase & EV_PHASE_LAST) {
			return;
		}
	}

	/*
	 * open the new frame at the start position
	 */
	if (st1.cmd != EV_NULL) {
		track_evput(frame, &fp, &st1);
	}

	/*
	 * delete events during next 'len' tics
	 */		
	for (;;) {
		delta = track_ticskipmax(o, &op, start + len - tic);
		track_seekblank(frame, &fp, delta);
		tic += delta;
		if (tic == start + len) {
			break;
		}
		if (!track_evavail(o, &op)) {
			return;
		}
		while(track_evavail(o, &op)) {
			track_evget(o, &op, &st2);
			track_evput(frame, &fp, &st2);
		}
	}
	
	/*
	 * close the event
	 */
	if (st2.cmd != EV_NULL) {
		if (ev_cancel(&st2, &ca)) {
			track_evput(frame, &fp, &ca);
		}
	} else if (st1.cmd != EV_NULL) {
		if (ev_cancel(&st1, &ca)) {
			track_evput(frame, &fp, &ca);
		}
	}
}

	/*
	 * retrun 1 if at least one event match the given event range
	 * and zero otherwise
	 */

unsigned
track_framematch(struct track *s, struct evspec *e) {
	struct seqptr sp;
	track_rew(s, &sp);
	for (;;) {
		if (!track_seqevavail(s, &sp)) {
			break;
		}
		if (evspec_matchev(e, &(*sp.pos)->ev)) {
			return 1;
		}
		track_seqevnext(s, &sp);
	}
	return 0;
}


void
track_frametransp(struct track *o, int halftones) {
	struct seqptr op;
	
	track_rew(o, &op);	
	for (;;) {
		if (!track_seqevavail(o, &op)) {
			break;
		}
		if (EV_ISNOTE(&(*op.pos)->ev)) {
			(*op.pos)->ev.data.voice.b0 += halftones;
			(*op.pos)->ev.data.voice.b0 &= 0x7f;
		}
		track_seqevnext(o, &op);
	}
}
