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
 * 	  materials  provided with the distribution.
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
 * implements high level track operations
 */

#include "dbg.h"
#include "trackop.h"
#include "track.h"
#include "default.h"

	/*
	 * removes the frame at the current position from the track.
	 * If the frame is incomplete, the returned
	 * frame is empty.
	 */
void
track_framerm(struct track_s *o, struct seqptr_s *p, struct track_s *frame) {
	struct seqptr_s op, fp;
	struct seqev_s *se;
	unsigned tics;
	unsigned key, chan;
	
	op = *p;
	track_clear(frame, &fp);
	tics = 0;
	
	if (!track_evavail(o, &op)) {
		dbg_puts("track_framerm: bad position\n");
		dbg_panic();
	}
	
	if (!EV_ISNOTE(&(*op.pos)->ev)) {
		se = track_seqevrm(o, &op);
		track_seqevins(frame, &fp, se);
		return;
	}
	
	if ((*op.pos)->ev.cmd != EV_NON) {
		dbg_puts("track_framerm: not a noteon\n");
		dbg_panic();
	}
	
	/* move the NOTEON */
	se = track_seqevrm(o, &op);
	track_seqevins(frame, &fp, se);
	track_seqevnext(frame, &fp);
	key = EV_GETNOTE(&se->ev);
	chan = EV_GETCHAN(&se->ev);

	/* move all related note events */
	for (;;) {
		tics += track_ticlast(o, &op);
		
		/* check for end of track */
		if (!track_evavail(o, &op)) {
			dbg_puts("track_framerm: orphaned noteon, ignored\n");
			track_clear(frame, &fp);
			break;

		/* check for nested NOTEON */
		} else if ((*op.pos)->ev.cmd == EV_NON &&
		    EV_GETNOTE(&(*op.pos)->ev) == key &&
		    EV_GETCHAN(&(*op.pos)->ev) == chan) {
			dbg_puts("track_framerm: nested noteon, skiped\n");
			track_evdel(o, &op);
			continue;
		
		/* is it a note event of the frame */
		} else if (EV_ISNOTE(&(*op.pos)->ev) && 
		    EV_GETNOTE(&(*op.pos)->ev) == key &&
		    EV_GETCHAN(&(*op.pos)->ev) == chan) {
			/* found the corresponding NOTE event */
			track_seekblank(frame, &fp, tics);
			track_evlast(frame, &fp);

			/* move the event */ 
			se = track_seqevrm(o, &op);
			track_seqevins(frame, &fp, se);
			track_seqevnext(frame, &fp);
			
			/* stop if it is the last event */
			if (se->ev.cmd == EV_NOFF)
				break;
				
			tics = 0;
			continue;
		}
		track_evnext(o, &op);
	}
}

	/*
	 * merges the given frame into the track
	 * WARNING: there is no checking for
	 * nested notes.
	 */

void
track_frameins(struct track_s *o, struct seqptr_s *p, struct track_s *frame) {
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
	 * set the current position to the
	 * following frame. if no frame is available
	 * the current position is 'o->eot'
	 */

unsigned
track_framefind(struct track_s *o, struct seqptr_s *p) {
	unsigned tics;
	tics = 0;
	for (;;) {
		tics += track_ticlast(o, p);
		if ((*p->pos)->ev.cmd == EV_NON ||
		    !EV_ISNOTE(&(*p->pos)->ev)) {
		    	break;
		}
		track_evnext(o, p);
	}
	return tics;
}

	/*
	 * same as track_framerm, but 
	 * if the same type of frame is found several times
	 * in the same tic, only the latest one is kept.
	 */	 

void
track_frameuniq(struct track_s *o, struct seqptr_s *p, struct track_s *frame) {	
	struct seqptr_s op, fp;
	unsigned delta;
	struct ev_s ev;	
	
	track_framerm(o, p, frame);
	op = *p;
	track_rew(frame, &fp);
	if (!track_evavail(frame, &fp)) {
		return;
	}
	track_evget(frame, &fp, &ev);
	for (;;) {
		delta = track_framefind(o, &op);
		if (delta != 0 || !track_evavail(o, &op)) {
			break;
		}
		if (ev_sameclass(&ev,  &(*op.pos)->ev)) {
			track_framerm(o, &op, frame);
		} else {
			track_evnext(o, &op);
		}
	}
}


void
track_framecp(struct track_s *s, struct track_s *d) {
	struct seqptr_s sp, dp;
	struct ev_s ev;
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
	 * suppress orphaned NOTEOFFs and NOTEONs and nested notes
	 * if the same frame is found twice on the same tic, 
	 * only the latest is kept.
	 */

void
track_opcheck(struct track_s *o) {
	struct track_s temp, frame;
	struct seqptr_s op, tp, fp;
	unsigned delta;
	
	track_init(&temp);
	track_init(&frame);
	
	track_rew(o, &op);
	track_rew(&temp, &tp);
	track_rew(&frame, &fp);

	for (;;) {
		delta = track_framefind(o, &op);
		track_seekblank(&temp, &tp, delta);
		
		if (!track_evavail(o, &op)) {
			break;
		}
		track_frameuniq(o, &op, &frame);
		track_frameins(&temp, &tp, &frame);
	}
	
	/*
	dbg_puts("track_check: the following events were dropped\n");
	track_dump(o);
	*/
	track_clear(o, &op);
	track_frameins(o, &op, &temp);

	track_done(&temp);
	track_done(&frame);
}



	/*
	 * quantise and (suppress orphaned NOTEOFFs and NOTEONs)
	 * arguments
	 *	first - the number of the curpos tic
	 *	len - the number of tic to quantise
	 *	quantum - number of tics to round to note-on positions
	 *	rate - 0 =  dont quantise, 100 = full quantisation
	 */

void
track_opquantise(struct track_s *o, struct seqptr_s *p, 
    unsigned first, unsigned len, unsigned quantum, unsigned rate) {
	struct track_s ctls, frame;
	struct seqptr_s op, cp, fp;
	unsigned tic, delta;
	unsigned remaind;
	int ofs;
	
	if (rate > 100) {
		dbg_puts("track_quantise: rate > 100\n");
	}

	len += first;
	track_init(&ctls);
	track_init(&frame);
	op = *p;
	track_rew(&ctls, &cp);
	track_rew(&frame, &fp);
	delta = ofs = 0;
	tic = first;
	
	
	/* first, move all non-quantizable frames to &ctls */	
	for (;;) {
		delta = track_framefind(o, &op);
		tic += delta;
		if (!track_evavail(o, &op) || tic >= len) {
			break; 	
		
		} 
		if ((*op.pos)->ev.cmd != EV_NON) {
			track_seekblank(&ctls, &cp, delta);
			track_evlast(&ctls, &cp);
			track_framerm(o, &op, &frame);
			track_frameins(&ctls, &cp, &frame);
		} else {
			track_evnext(o, &op);
		}
	}
	op = *p;
	track_rew(&ctls, &cp);	
	delta = ofs = 0;	
	tic = first;
	
	/* now we can start */

	for (;;) {
		delta = track_framefind(o, &op);
		tic += delta;
		delta -= ofs;
		ofs = 0;
		
		if (!track_evavail(o, &op) || tic >= len) {	/* no more notes? */
			break;
		} 
			
		if (quantum != 0) {
			remaind = tic % quantum;
		} else {
			remaind = 0;
		}
		if (remaind < quantum / 2) {
			ofs = - ((remaind * rate + 99) / 100);
		} else {
			ofs = ((quantum - remaind) * rate + 99) / 100;
		}
		if (delta + ofs < 0) {
			dbg_puts("track_opquantise: delta + ofs < 0\n");
			dbg_panic();
		}
		track_seekblank(&ctls, &cp, delta + ofs);
		track_evlast(&ctls, &cp);
		track_framerm(o, &op, &frame);
		track_frameins(&ctls, &cp, &frame);
	}
	op = *p;
	track_frameins(o, &op, &ctls);
	track_opcheck(o);
	track_done(&ctls);
}

	/*
	 * extracts all frames from a track
	 * begging at the current position during
	 * 'len' tics.
	 */

void
track_opextract(struct track_s *o, struct seqptr_s *p, 
    unsigned len, struct track_s *targ) {
	struct track_s frame;
	struct seqptr_s op, tp;
	unsigned delta, tic;
	
	tic = 0;
	op = *p;
	track_init(&frame);
	track_clear(targ, &tp);
	
	for (;;) {
		delta = track_framefind(o, &op);
		track_seekblank(targ, &tp, delta);
		track_evlast(targ, &tp);
		tic += delta;

		if (!track_evavail(o, &op) || tic >= len) {
			break;
		}
		
		track_framerm(o, &op, &frame);
		track_frameins(targ, &tp, &frame);
	}	
	track_done(&frame);
}


	/* 
	 * cuts a piece of the track (events + blank space)
	 */
	

void
track_opdelete(struct track_s *o, struct seqptr_s *p, unsigned len) {
	struct track_s frame, temp;
	struct seqptr_s op, fp, tp;
	unsigned delta, tic;
	
	track_init(&frame);
	track_init(&temp);
	
	tic = 0;
	op = *p;
	track_rew(&frame, &fp);
	track_rew(&temp, &tp);
	
	for (;;) {
		delta = track_framefind(o, &op);
		
		if (tic + delta >= len) {
			if (tic < len) {
				track_seekblank(&temp, &tp, tic + delta - len);
			} else {
				track_seekblank(&temp, &tp, delta);
			}
		}
		track_evlast(&temp, &tp);
		tic += delta;
		
		if (!track_evavail(o, &op)) {
			break;
		}

		if (tic >= len) {
			track_framerm(o, &op, &frame);
			track_frameins(&temp, &tp, &frame);
		} else {
			track_framerm(o, &op, &frame);
			track_clear(&frame, &fp);
		}
	}	
	op = *p;
	track_frameins(o, &op, &temp);
	track_done(&frame);
	track_done(&temp);
	track_opcheck(o);
}

	/*
	 * insert blank space at the giver position
	 */

void
track_opinsert(struct track_s *o, struct seqptr_s *p, unsigned len) {
	struct track_s frame, temp;
	struct seqptr_s op, tp;
	unsigned delta, tic;
	
	track_init(&temp);
	track_init(&frame);

	tic = 0;
	op = *p;
	track_rew(&temp, &tp);
	track_seekblank(&temp, &tp, len);
		
	for (;;) {
		delta = track_framefind(o, &op);
		track_seekblank(&temp, &tp, delta);
		track_evlast(&temp, &tp);
		tic += delta;

		if (!track_evavail(o, &op)) {
			break;
		}
	
		track_framerm(o, &op, &frame);
		track_frameins(&temp, &tp, &frame);
	}
	op = *p;
	track_frameins(o, &op, &temp);
	track_done(&frame);
	track_done(&temp);
	track_opcheck(o);
}


void
track_opsetchan(struct track_s *o, unsigned chan) {
	struct seqptr_s op;
	track_rew(o, &op);
	for (;;) {
		if (!track_seqevavail(o, &op)) {
			break;
		}
		if (EV_ISVOICE(&(*op.pos)->ev)) {
			(*op.pos)->ev.data.voice.chan = chan;
		}
		track_seqevnext(o, &op);
	}
}


	/*
	 * takes a measure and returns the corresponding tic
	 */


unsigned
track_opfindtic(struct track_s *o, unsigned m0) {
	struct seqptr_s op;
	unsigned m, dm, tpb, bpm, pos, delta;
	struct ev_s ev;
	
	/* default to 24 tics per beat, 4 beats per measure */
	tpb = DEFAULT_TPB;
	bpm = DEFAULT_BPM;
	
	/* go to the beggining */
	track_rew(o, &op);
	m = 0;
	delta = pos = 0;
	
	/* seek the wanted measure */
	for (;;) {
		delta += track_ticlast(o, &op);
		dm = delta / (bpm * tpb);
		if (dm >= m0 - m) {		/* dont seek beyound m0 */
			dm = m0 - m;
		}
		m     += dm;
		delta -= dm * (bpm * tpb);
		pos   += dm * (bpm * tpb);
		if (m == m0) {
			break;
		}
		/* check for premature end of track */
		if (!track_evavail(o, &op)) {
			/* complete as we had blank space */
			pos += (m0 - m) * bpm * tpb;
			m = m0;
			break;
		}
		/* scan for timesign changes */
		while (track_evavail(o, &op)) {
			track_evget(o, &op, &ev);
			if (ev.cmd == EV_TIMESIG) {
				if (delta != 0) {
					dbg_puts("track_opfindtic: EV_TIMESIG in a measure\n");
					dbg_panic();
				}
				bpm = ev.data.sign.beats;
				tpb = ev.data.sign.tics;
			}
		}
	}
	/*
	dbg_puts("track_opfindtic: pos=");
	dbg_putu(pos);
	dbg_puts("\n");
	*/
	return pos;
}

	/*
	 * determines the tempo and the
	 * time signature at a givent position (in tics)
	 * (the whole tic is scanned)
	 */


void
track_optimeinfo(struct track_s *o, unsigned pos, unsigned long *usec24, unsigned *bpm, unsigned *tpb) {
	unsigned tic;
	struct ev_s ev;
	struct seqptr_s op;
	
	tic = 0;
	track_rew(o, &op);
	
	*tpb = DEFAULT_TPB;
	*bpm = DEFAULT_BPM;
	*usec24 = TEMPO_TO_USEC24(DEFAULT_TEMPO, *tpb);
	
	for (;;) {
		tic += track_ticlast(o, &op);
		if (tic > pos || !track_evavail(o, &op)) {
			break;
		}
		while (track_evavail(o, &op)) {
			track_evget(o, &op, &ev);
			switch(ev.cmd) {
			case EV_TEMPO:
				*usec24 = ev.data.tempo.usec24;
				break;
			case EV_TIMESIG:
				*bpm = ev.data.sign.beats;
				*tpb = ev.data.sign.tics;
				break;
			default:
				break;
			}
		}
	}
	/*
	dbg_puts("track_optimeinfo: ");
	dbg_putu(tic);
	dbg_puts(" tempo=");
	dbg_putu(*usec24);
	dbg_puts(" bpm=");
	dbg_putu(*bpm);
	dbg_puts(" tpb=");
	dbg_putu(*tpb);
	dbg_puts("\n");
	*/
}


