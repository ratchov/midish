/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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
 * implements high level track operations
 */

#include "dbg.h"
#include "trackop.h"
#include "track.h"
#include "default.h"
#include "frame.h"



	/*
	 * suppress orphaned NOTEOFFs and NOTEONs and nested notes
	 * if the same frame is found twice on the same tic, 
	 * only the latest is kept.
	 */

void
track_opcheck(struct track *o) {
	struct track temp, frame;
	struct seqptr op, tp, fp;
	unsigned delta;
	
	track_init(&temp);
	track_init(&frame);
	
	track_rew(o, &op);
	track_rew(&temp, &tp);
	track_rew(&frame, &fp);

	for (;;) {
		delta = track_ticlast(o, &op);
		track_seekblank(&temp, &tp, delta);
		
		if (!track_evavail(o, &op)) {
			break;
		}
		track_frameuniq(o, &op, &frame);
		track_frameput(&temp, &tp, &frame);
	}
	
	/*
	dbg_puts("track_check: the following events were dropped\n");
	track_dump(o);
	*/
	track_clear(o, &op);
	track_frameput(o, &op, &temp);

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
track_opquantise(struct track *o, unsigned start, unsigned len, 
    unsigned offset, unsigned quantum, unsigned rate) {
	struct track ctls, frame;
	struct seqptr op, cp, fp;
	unsigned tic, delta;
	unsigned remaind;
	int ofs;
	
	if (rate > 100) {
		dbg_puts("track_quantise: rate > 100\n");
	}

	track_init(&ctls);
	track_init(&frame);
	track_rew(&ctls, &cp);
	track_rew(&frame, &fp);
	track_rew(o, &op);
	delta = ofs = 0;
	tic = 0;

	/* first, move all non-quantizable frames to &ctls */
	for (;;) {
		delta = track_ticlast(o, &op);
		tic += delta;
		if (!track_evavail(o, &op)) {
			break;
		}
		track_seekblank(&ctls, &cp, delta);
		if (!EV_ISNOTE(&(*op.pos)->ev) ||  
		    tic < start || tic >= start + len) {
			track_evlast(&ctls, &cp);
			track_frameget(o, &op, &frame);
			track_frameput(&ctls, &cp, &frame);
		} else {
			track_evnext(o, &op);
		}
	}

	track_rew(o, &op);
	track_rew(&ctls, &cp);
	track_seekblank(&ctls, &cp, start);
	tic = track_seek(o, &op, start);
	delta = ofs = 0;
	
	/* now we can start */

	for (;;) {
		delta = track_ticlast(o, &op);
		tic += delta;
		delta -= ofs;
		ofs = 0;
		
		if (!track_evavail(o, &op) || tic >= start + len) {
			/* no more notes */
			break;
		}

		if (quantum != 0) {
			remaind = (tic + offset) % quantum;
		} else {
			remaind = 0;
		}
		if (remaind < quantum / 2) {
			ofs = - ((remaind * rate + 99) / 100);
		} else {
			ofs = ((quantum - remaind) * rate + 99) / 100;
		}
		if (ofs < 0 && delta < (unsigned)(-ofs)) {
			dbg_puts("track_opquantise: delta < ofs\n");
			dbg_panic();
		}
		track_seekblank(&ctls, &cp, delta + ofs);
		track_evlast(&ctls, &cp);
		track_frameget(o, &op, &frame);
		track_frameput(&ctls, &cp, &frame);
	}
	track_rew(o, &op);
	track_frameput(o, &op, &ctls);
	track_opcheck(o);
	track_done(&ctls);
}

	/* 
	 * cut a piece of the track (events and blank space)
	 */

void
track_opcut(struct track *o, unsigned start, unsigned len) {
	struct track frame, temp;
	struct seqptr op, fp, tp;
	unsigned delta, tic;
	
	track_init(&frame);
	track_init(&temp);
	
	tic = 0;
	track_rew(o, &op);
	track_rew(&frame, &fp);
	track_rew(&temp, &tp);
	
	for (;;) {
		delta = track_ticskipmax(o, &op, start - tic);
		track_seekblank(&temp, &tp, delta);
				
		tic += delta;
		if (tic == start) {
			break;
		}
		if (!track_evavail(o, &op)) {
			goto end;
		}
		track_frameget(o, &op, &frame);
		track_framecut(&frame, start - tic, len);
		track_frameput(&temp, &tp, &frame);
	}
	
	for (;;) {
		len -= track_ticdelmax(o, &op, len);
		if (len == 0) {
			break;
		}
		if (!track_evavail(o, &op)) {
			goto end;
		}
		track_frameget(o, &op, &frame);
		track_framecut(&frame, start - tic, len);
		track_frameput(&temp, &tp, &frame);
	}
	
	for (;;) {
		delta = track_ticlast(o, &op);
		track_seekblank(&temp, &tp, delta);
		
		tic += delta;
		if (!track_evavail(o, &op)) {
			goto end;
		}
		track_frameget(o, &op, &frame);
		track_frameput(&temp, &tp, &frame);
	}
	
end:
	track_rew(o, &op);
	track_frameput(o, &op, &temp);
	track_done(&frame);
	track_done(&temp);
}

	/*
	 * insert blank space at the given position
	 */

void
track_opinsert(struct track *o, unsigned start, unsigned len) {
	struct track frame, temp;
	struct seqptr op, tp, fp;
	unsigned delta, tic;
	
	track_init(&temp);
	track_init(&frame);
	
	tic = 0;
	track_rew(o, &op);
	track_rew(&frame, &fp);
	track_rew(&temp, &tp);
		
	for (;;) {
		delta = track_ticskipmax(o, &op, start - tic);
		track_seekblank(&temp, &tp, delta);
				
		tic += delta;
		if (tic == start) {
			break;
		}
		if (!track_evavail(o, &op)) {
			goto end;
		}
		track_frameget(o, &op, &frame);
		track_frameins(&frame, start - tic, len);
		track_frameput(&temp, &tp, &frame);
	}
		
	tic += len;
	track_seekblank(&temp, &tp, len);

	for (;;) {
		delta = track_ticlast(o, &op);
		track_seekblank(&temp, &tp, delta);
		track_evlast(&temp, &tp);
		tic += delta;

		if (!track_evavail(o, &op)) {
			break;
		}
	
		track_frameget(o, &op, &frame);
		track_frameput(&temp, &tp, &frame);
	}
	
end:	
	track_rew(o, &op);
	track_frameput(o, &op, &temp);
	track_done(&frame);
	track_done(&temp);
}

	/* 
	 * blank a piece of the track (remove events but not blank space)
	 */
	 
void
track_opblank(struct track *o, unsigned start, unsigned len, 
   struct evspec *es) {
	struct track frame, backup;
	struct seqptr op, bp;
	unsigned delta, tic;
	
	track_init(&frame);
	track_init(&backup);
	
	tic = 0;
	track_rew(o, &op);
	track_rew(&backup, &bp);
		
	for (;;) {
		delta = track_ticskipmax(o, &op, start - tic);
		track_seekblank(&backup, &bp, delta);

		tic += delta;
		if (tic == start) {
			break;
		}
		if (!track_evavail(o, &op)) {
			goto end;
		}
		track_frameget(o, &op, &frame);
		if (track_framematch(&frame, es)) {
			track_frameblank(&frame, start - tic, len);
		}
		track_frameput(&backup, &bp, &frame);
	}
		
	for (;;) {
		delta = track_ticskipmax(o, &op, len);
		track_seekblank(&backup, &bp, delta);
		
		tic   += delta;
		start += delta;
		len   -= delta;
		if (len == 0) {
			break;
		}
		if (!track_evavail(o, &op)) {
			goto end;
		}
		track_frameget(o, &op, &frame);
		if (track_framematch(&frame, es)) {
			track_frameblank(&frame, start - tic, len);
		}
		track_frameput(&backup, &bp, &frame);
	}

end:	track_rew(o, &op);
	track_frameput(o, &op, &backup);
	track_done(&frame);
}


	/* 
	 * copy a piece of the track into another track
	 */
	 
void
track_opcopy(struct track *o, unsigned start, unsigned len, 
   struct evspec *es, struct track *targ) {
	struct track frame, backup, copy;
	struct seqptr op, bp, tp;
	unsigned delta, tic;
	
	track_init(&frame);
	track_init(&backup);
	track_init(&copy);
	
	tic = 0;
	track_rew(o, &op);
	track_rew(&backup, &bp);
	track_clear(targ, &tp);
		
	for (;;) {
		delta = track_ticskipmax(o, &op, start - tic);
		track_seekblank(&backup, &bp, delta);

		tic += delta;
		if (tic == start) {
			break;
		}
		if (!track_evavail(o, &op)) {
			goto end;
		}
		track_frameget(o, &op, &frame);
		if (track_framematch(&frame, es)) {
			track_framecopy(&frame, start - tic, len, &copy);
			track_frameput(targ, &tp, &copy);	
		}
		track_frameput(&backup, &bp, &frame);
	}
		
	for (;;) {
		delta = track_ticskipmax(o, &op, len);
		track_seekblank(&backup, &bp, delta);
		track_seekblank(targ, &tp, delta);

		tic   += delta;
		start += delta;
		len   -= delta;
		if (len == 0) {
			break;
		}
		if (!track_evavail(o, &op)) {
			goto end;
		}
		track_frameget(o, &op, &frame);
		if (track_framematch(&frame, es)) {
			track_framecopy(&frame, start - tic, len, &copy);
			track_frameput(targ, &tp, &copy);	
		}
		track_frameput(&backup, &bp, &frame);
	}

end:	track_rew(o, &op);
	track_frameput(o, &op, &backup);

	track_done(&frame);
	track_done(&copy);
}



void
track_optransp(struct track *o, struct seqptr *p, unsigned len, 
    int halftones, struct evspec *es) {
	struct track frame, temp;
	struct seqptr op, tp;
	unsigned delta, tic;
	
	track_init(&temp);
	track_init(&frame);

	tic = 0;
	op = *p;
	track_rew(&temp, &tp);
		
	for (;;) {
		delta = track_ticlast(o, &op);
		track_seekblank(&temp, &tp, delta);
		track_evlast(&temp, &tp);
		tic += delta;

		if (!track_evavail(o, &op)) {
			break;
		}
	
		if (evspec_matchev(es, &(*op.pos)->ev)) {
			track_frameget(o, &op, &frame);
			track_frametransp(&frame, halftones);
			track_frameput(&temp, &tp, &frame);
		} else {
			track_evnext(o, &op);
		}
	}
	op = *p;
	track_frameput(o, &op, &temp);
	track_done(&frame);
	track_done(&temp);
	track_opcheck(o);
}

	/*
	 * set the chan (dev/midichan pair) of
	 * all voice events
	 */

void
track_opsetchan(struct track *o, unsigned dev, unsigned ch) {
	struct seqptr op;
	track_rew(o, &op);
	for (;;) {
		if (!track_seqevavail(o, &op)) {
			break;
		}
		if (EV_ISVOICE(&(*op.pos)->ev)) {
			(*op.pos)->ev.data.voice.dev = dev;
			(*op.pos)->ev.data.voice.ch = ch;
		}
		track_seqevnext(o, &op);
	}
}


	/*
	 * takes a measure and returns the corresponding tic
	 */

unsigned
track_opfindtic(struct track *o, unsigned m0) {
	struct seqptr op;
	unsigned m, dm, tpb, bpm, pos, delta;
	struct ev ev;
	
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
					/* 
					 * XXX: may happen with bogus midi 
					 * files 
					 */
					dbg_puts("track_opfindtic: EV_TIMESIG in a measure\n");
					dbg_panic();
				}
				bpm = ev.data.sign.beats;
				tpb = ev.data.sign.tics;
			}
		}
	}
	return pos;
}

	/*
	 * determine the tempo and the
	 * time signature at a given position (in tics)
	 * (the whole tic is scanned)
	 */

void
track_optimeinfo(struct track *o, unsigned pos, unsigned long *usec24, unsigned *bpm, unsigned *tpb) {
	unsigned tic;
	struct ev ev;
	struct seqptr op;
	
	tic = 0;
	track_rew(o, &op);
	
	/* use default midi settings */
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
}


	/*
	 * determine the (measure, beat, tic)
	 * corresponding to a absolut tic
	 * (the whole tic is scanned)
	 */

void
track_opgetmeasure(struct track *o, unsigned pos,
    unsigned *measure, unsigned *beat, unsigned *tic) {
	unsigned delta, abstic, tpb, bpm;
	struct ev ev;
	struct seqptr op;
	
	abstic = 0;
	track_rew(o, &op);
	
	/* use default midi settings */
	tpb = DEFAULT_TPB;
	bpm = DEFAULT_BPM;
	*measure = 0;
	*beat = 0;
	*tic = 0;
	
	for (;;) {
		delta = track_ticlast(o, &op);
		if (abstic + delta >= pos) {
			delta = pos - abstic;
		}
		abstic += delta;
		*tic += delta;
		*measure += *tic / (tpb * bpm);
		*tic =      *tic % (tpb * bpm);
		*beat +=    *tic / tpb;	
		*tic =      *tic % tpb;
		*measure += *beat / bpm;
		*beat =     *beat % bpm;
		if (abstic >= pos || !track_evavail(o, &op)) {
			break;
		}
		while (track_evavail(o, &op)) {
			track_evget(o, &op, &ev);
			switch(ev.cmd) {
			case EV_TIMESIG:
				bpm = ev.data.sign.beats;
				tpb = ev.data.sign.tics;
				break;
			default:
				break;
			}
		}
	}
}


void
track_opchaninfo(struct track *o, char *map) {
	unsigned i, ch, dev;
	struct seqptr p;
	
	for (i = 0; i < DEFAULT_MAXNCHANS; i++) {
		map[i] = 0;
	}
	track_rew(o, &p);
	
	while (track_seqevavail(o, &p)) {
		if (EV_ISVOICE(&(*p.pos)->ev)) {
			dev = (*p.pos)->ev.data.voice.dev;
			ch = (*p.pos)->ev.data.voice.ch;
			i = dev * 16 + ch;
			if (dev >= DEFAULT_MAXNDEVS || ch >= 16) {
				dbg_puts("track_opchaninfo: bogus dev/ch pair, stopping\n");
				break;
			}
			map[dev * 16 + ch] = 1;
		}
		track_seqevnext(o, &p);
	}
}

	/*
	 * add an event to the track (config track)
	 * in an ordered way
	 */

void
track_opconfev(struct track *o, struct ev *ev) {
	struct seqptr p;
	
	track_rew(o, &p);	
	while(track_seqevavail(o, &p)) {
		if (ev_sameclass(&(*p.pos)->ev, ev)) {
			(*p.pos)->ev = *ev;
			return;
		}
		track_seqevnext(o, &p);
	}

	track_rew(o, &p);	
	while(track_seqevavail(o, &p)) {
		if (ev_ordered(ev, &(*p.pos)->ev)) {
			track_evput(o, &p, ev);
			return;
		}
		track_seqevnext(o, &p);
	}
	track_evput(o, &p, ev);
}
