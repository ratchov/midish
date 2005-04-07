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
 * the multiplexer handles :
 *	- midi input and output
 *	- internal external/timer
 *
 * the unit of time is the 24th of microsecond, thus
 * the tempo is stored we the same accurancy than a
 * midi file.
 */

#include "dbg.h"
#include "ev.h"

#include "default.h"
#include "mdep.h"
#include "mux.h"
#include "rmidi.h"
	/* 
	 * MUX_START_DELAY: 
	 * delay between the START event and the first TIC
	 * in 24ths of a micro second, 
	 * here we use 1 tic at 30bpm
	 */
#define MUX_START_DELAY	  (2000000UL)	
#undef MUX_DEBUG

unsigned mux_ticrate;
unsigned long mux_ticlength, mux_curpos, mux_nextpos;
unsigned mux_curtic;
unsigned mux_phase;
void (*mux_cb)(void *, struct ev_s *);
void *mux_addr;

void mux_dbgphase(unsigned phase);
void mux_chgphase(unsigned phase);

void
mux_init(void (*cb)(void *, struct ev_s *), void *addr) {
	struct mididev_s *i;
	/* 
	 * default tempo is 120 beats per minutes
	 * with 24 tics per beat
	 * (time unit = 24th of microsecond)
	 */
	mux_ticlength = TEMPO_TO_USEC24(DEFAULT_TEMPO, DEFAULT_TPB);
	
	/* 
	 * default tics per second = 96
	 */
	mux_ticrate = DEFAULT_TPU;
	
	/*
	 * reset tic counters of devices 
	 */
	for (i = mididev_list; i != 0; i = i->next) {
		i->ticdelta = i->ticrate;
	}

	mux_curpos = 0;
	mux_nextpos = 0;
	mux_phase = MUX_STOP;
	mux_cb = cb;
	mux_addr = addr;
	mux_mdep_init();
}

void
mux_done(void) {
	mux_flush();
	mux_mdep_done();
}

#ifdef MUX_DEBUG
void
mux_dbgphase(unsigned phase) {
	switch(phase) {
	case MUX_STARTWAIT:
		dbg_puts("STARTWAIT");
		break;
	case MUX_START:
		dbg_puts("START");
		break;
	case MUX_FIRST:
		dbg_puts("FIRST");
		break;
	case MUX_NEXT:
		dbg_puts("NEXT");
		break;
	case MUX_STOPWAIT:
		dbg_puts("STOPWAIT");
		break;
	case MUX_STOP:
		dbg_puts("STOP");
		break;
	default:
		dbg_puts("unknown");
		break;
	}
}
#endif

void
mux_chgphase(unsigned phase) {
#ifdef MUX_DEBUG
	dbg_puts("mux_phase: ");
	mux_dbgphase(mux_phase);
	dbg_puts(" -> ");
	mux_dbgphase(phase);
	dbg_puts("\n");
#endif
	mux_phase = phase;
}

	/*
	 * try send a TIC event to all devices the transmid
	 * real-time events, the tic is only send if
	 * the device tic_per_unit permits it.
	 */

void
mux_sendtic(void) {
	struct ev_s ev;
	struct mididev_s *i;

	ev.cmd = EV_TIC;
	for (i = mididev_list; i != 0; i = i->next) {
		if (i->sendrt && i != mididev_master) {
			while (i->ticdelta >= i->ticrate) {
				rmidi_putev(RMIDI(i), &ev);
				i->ticdelta -= mux_ticrate;
			}
			i->ticdelta += mux_ticrate;
		}
	}
}

	/*
	 * similar to sendtic, but sends a START event
	 */


void
mux_sendstart(void) {
	struct ev_s tic, start;
	struct mididev_s *i;

	tic.cmd = EV_TIC;
	start.cmd = EV_START;
	for (i = mididev_list; i != 0; i = i->next) {
		if (i->sendrt && i != mididev_master) {
			i->ticdelta = i->ticrate;
			/*
			 * send a tic just before the start event
			 * in order to notify that we are the master
			 */
			rmidi_putev(RMIDI(i), &tic);
			rmidi_putev(RMIDI(i), &start);
		}
	}
}

	/*
	 * similar to sendtic, but sends a STOP event
	 */

void
mux_sendstop(void) {
	struct ev_s ev;
	struct mididev_s *i;

	ev.cmd = EV_STOP;
	for (i = mididev_list; i != 0; i = i->next) {
		if (i->sendrt && i != mididev_master) {
			rmidi_putev(RMIDI(i), &ev);
		}
	}
}

void
mux_putev(struct ev_s *ev) {
	unsigned unit;
	struct mididev_s *dev;
	
	if (EV_ISVOICE(ev)) {
		unit = ev->data.voice.chan >> 4;
		if (unit < DEFAULT_MAXNDEVS) {
			dev = mididev_byunit[unit];
			if (dev != 0) {
				rmidi_putev(RMIDI(dev), ev);
			}
		}
	} else {
		dbg_puts("mux_putev: only voice events allowed\n");
		dbg_panic();
	}
}

void
mux_sendraw(unsigned unit, unsigned char *buf, unsigned len) {
	struct mididev_s *dev;
	if (unit >= DEFAULT_MAXNDEVS) {
		return;
	}
	dev = mididev_byunit[unit];
	if (dev == 0) {
		return;
	}
	while(len) {
		rmidi_out(RMIDI(dev), *buf);
		buf++;
		len--;
	}
}

void
mux_timercb(unsigned long delta) {
	struct ev_s ev;
	mux_curpos += delta;

	if (!mididev_master) {	/* if internal clock source */
		if (mux_phase == MUX_STARTWAIT) {
			mux_chgphase(MUX_START);
			mux_sendstart();
			mux_flush();
			mux_curpos = 0;
			mux_nextpos = MUX_START_DELAY;
			if (mux_cb) {
				ev.cmd = EV_START;
				mux_cb(mux_addr, &ev);
			}
		}

		if (mux_phase == MUX_STOPWAIT) {
			mux_chgphase(MUX_STOP);
			mux_sendstop();
			mux_flush();
			if (mux_cb) {
				ev.cmd = EV_STOP;
				mux_cb(mux_addr, &ev);
			}
		}
		
		while (mux_curpos >= mux_nextpos) {
			mux_curpos -= mux_nextpos;
			mux_nextpos = mux_ticlength;
			if (mux_phase == MUX_FIRST) {
				mux_chgphase(MUX_NEXT);
			} else if (mux_phase == MUX_START) {
				mux_chgphase(MUX_FIRST);
			}
			mux_sendtic();
			if (mux_cb) {
				ev.cmd = EV_TIC;
				mux_cb(mux_addr, &ev);
			}
			mux_flush();	
			if (mux_phase == MUX_NEXT) {
				mux_curtic++;
			} else if (mux_phase == MUX_FIRST) {
				mux_curtic = 0;
			}
		}
	}
}

void
mux_evcb(unsigned unit, struct ev_s *ev) {
	switch(ev->cmd) {
	case EV_TIC:
		if (mididev_master && 				/* if external clock */
		    mididev_byunit[unit] == mididev_master) {	
			if (mux_phase == MUX_FIRST) {
				mux_chgphase(MUX_NEXT);
			} else if (mux_phase == MUX_START) {
				mux_chgphase(MUX_FIRST);
			}
			if (mux_phase == MUX_NEXT) {
				mux_curtic++;
			} else if (mux_phase == MUX_FIRST) {
				mux_curtic = 0;
			}
			if (mux_cb) {
				mux_cb(mux_addr, ev);
			}
		}
		break;
	case EV_START:
		if (mididev_master && 				/* if external clock */
		    mididev_byunit[unit] == mididev_master) {	
			mux_chgphase(MUX_START);
			if (mux_cb) {
				mux_cb(mux_addr, ev);
			}
		}
		break;
	case EV_STOP:
		if (mididev_master && 				/* if external clock */
		    mididev_byunit[unit] == mididev_master) {	
			mux_chgphase(MUX_STOP);
			if (mux_cb) {
				mux_cb(mux_addr, ev);
			}
		}
		break;
	default:
		if (mux_cb) {
			mux_cb(mux_addr, ev);
		}
	}
}

/* -------------------------------------- user "public" functions --- */

void
mux_flush(void) {
	struct mididev_s *dev;
	for (dev = mididev_list; dev != 0; dev = dev->next) {
		rmidi_flush(RMIDI(dev));
	}
}


unsigned
mux_getphase(void) {
	return mux_phase;
}

void
mux_chgtempo(unsigned long ticlength) {
	if (mux_phase == MUX_FIRST || mux_phase == MUX_NEXT) {
		mux_nextpos += ticlength;
		mux_nextpos -= mux_ticlength;
	}
	mux_ticlength = ticlength;
}

void
mux_chgticrate(unsigned tpu) {
	mux_ticrate = tpu;
}

void
mux_startwait(void) {
	mux_chgphase(MUX_STARTWAIT);
}

void
mux_stopwait(void) {
	mux_chgphase(MUX_STOPWAIT);
}
