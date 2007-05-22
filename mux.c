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
 * the multiplexer handles :
 *	- midi input and output
 *	- internal external/timer
 *
 * the time unit is the 24th of microsecond (thus the tempo is stored
 * with the same accurancy as in standard midi files).
 *
 * the timer has the following states:
 * STOP -> STARTWAIT -> START -> FIRST_TIC -> NEXT_TIC -> STOPWAIT -> STOP
 *
 * STARTWAIT:
 *
 *	we're waiting (forever) for a "start" MIDI event; once it is
 *	received, we switch to the next state (START state). If the
 *	internal clock source is used, then we switch immediately to
 *	next state.
 *
 * START:
 *
 *	we just received the "start" MIDI event, so we wait for the
 *	first "tick" MIDI event; once it's received we switch to the
 *	next state (FIRST_TIC). If the internal clock source is used
 *	we wait MUX_START_DELAT (0.1 second) and we switch to the next
 *	state.
 *
 * FIRST_TIC:
 *
 *	we received the first "tick" event after a "start" event,
 *	the music starts now, so call appropriate call-backs to do so
 *	and wait for the next "tick" event.
 *
 * NEXT_TIC:
 *
 *	we received another "tick" event, move the music one
 *	step forward.
 *
 * STOPWAIT:
 *
 *	the music is over, so the upper layer puts this module in the
 *	STOPWAIT state. So we just wait for the "stop" MIDI event;
 *	that's necessary because other MIDI sequencers may not have
 *	finished. 
 *
 * STOP:
 *
 *	nothing to do, ignore any MIDI sync events.
 *
 */

#include "dbg.h"
#include "ev.h"

#include "default.h"
#include "mdep.h"
#include "mux.h"
#include "rmidi.h"
#include "sysex.h"
#include "timo.h"
#include "state.h"
#include "conv.h"

/* 
 * MUX_START_DELAY: 
 * delay between the START event and the first TIC
 * in 24ths of a micro second, here we use 1 tic at 30bpm
 */
#define MUX_START_DELAY	  (2000000UL)	

unsigned mux_debug = 0;
unsigned mux_ticrate;
unsigned long mux_ticlength, mux_curpos, mux_nextpos;
unsigned mux_curtic;
unsigned mux_phase;
struct sysexlist mux_sysexlist;
struct muxops *mux_ops;
void *mux_addr;

struct statelist mux_istate, mux_ostate;

void mux_dbgphase(unsigned phase);
void mux_chgphase(unsigned phase);

void
mux_init(struct muxops *ops, void *addr) {
	struct mididev *i;

	statelist_init(&mux_istate);
	statelist_init(&mux_ostate);
	
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
	for (i = mididev_list; i != NULL; i = i->next) {
		i->ticdelta = i->ticrate;
		i->isensto = 0;
		i->osensto = MIDIDEV_OSENSTO;
	}

	mux_curpos = 0;
	mux_nextpos = 0;
	mux_phase = MUX_STOP;
	mux_ops = ops;
	mux_addr = addr;
	sysexlist_init(&mux_sysexlist);
	timo_init();
	mux_mdep_init();
}

void
mux_done(void) {
	struct mididev *i;
	mux_flush();
	for (i = mididev_list; i != NULL; i = i->next) {
		if (RMIDI(i)->isysex) {
			dbg_puts("lost incomplete sysex\n");
			sysex_del(RMIDI(i)->isysex);
		}
	}
	mux_mdep_done();
	timo_done();
	sysexlist_done(&mux_sysexlist);
	statelist_done(&mux_ostate);
	statelist_done(&mux_istate);
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
 * Send a TIC to all devices that transmit
 * real-time events. The tic is only send if
 * the device tic_per_unit permits it.
 */
void
mux_sendtic(void) {
	struct mididev *i;

	for (i = mididev_list; i != NULL; i = i->next) {
		if (i->sendrt && i != mididev_master) {
			while (i->ticdelta >= mux_ticrate) {
				rmidi_puttic(RMIDI(i));
				i->ticdelta -= mux_ticrate;
			}
			i->ticdelta += i->ticrate;
		}
	}
}

/*
 * Similar to sendtic, but sends a START event
 */
void
mux_sendstart(void) {
	struct mididev *i;

	for (i = mididev_list; i != NULL; i = i->next) {
		if (i->sendrt && i != mididev_master) {
			i->ticdelta = i->ticrate;
			/*
			 * send a tic just before the start event
			 * in order to notify that we are the master
			 */
			rmidi_puttic(RMIDI(i));
			rmidi_putstart(RMIDI(i));
		}
	}
}

/*
 * similar to sendtic, but sends a STOP event
 */
void
mux_sendstop(void) {
	struct mididev *i;

	for (i = mididev_list; i != NULL; i = i->next) {
		if (i->sendrt && i != mididev_master) {
			rmidi_putstop(RMIDI(i));
		}
	}
}

void
mux_putev(struct ev *ev) {
	unsigned unit;
	struct mididev *dev;
	struct ev rev[CONV_NUMREV];
	unsigned i, nev;
	
#ifdef MUX_DEBUG
	dbg_puts("mux_putev: ");
	ev_dbg(ev);
	dbg_puts("\n");
#endif

	if (EV_ISVOICE(ev)) {
		unit = ev->dev;
		if (unit < DEFAULT_MAXNDEVS) {
			dev = mididev_byunit[unit];
			if (dev != NULL) {
				nev = conv_unpackev(&mux_ostate, ev, rev);
				for (i = 0; i < nev; i++) {
					rmidi_putev(RMIDI(dev), &rev[i]);
				}
			}
		}
	} else {
		dbg_puts("mux_putev: only voice events allowed\n");
		dbg_panic();
	}
}

void
mux_sendraw(unsigned unit, unsigned char *buf, unsigned len) {
	struct mididev *dev;
	if (unit >= DEFAULT_MAXNDEVS) {
		return;
	}
	if (len == 0) {
		return;
	}
	dev = mididev_byunit[unit];
	if (dev == NULL) {
		return;
	}
	rmidi_sendraw(RMIDI(dev), buf, len);
}

void
mux_timercb(unsigned long delta) {
	struct mididev *dev;
	mux_curpos += delta;
	
	/*
	 * run expired timeouts
	 */
	timo_update(delta);

	/*
	 * handle active sensing
	 */
	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		if (dev->isensto) {
			if (dev->isensto <= delta) {
				dev->isensto = 0;
				dbg_putu(dev->unit);
				dbg_puts(": sensing timeout, disabled\n");
			} else {
				dev->isensto -= delta;
			}
		}
		if (dev->osensto) {
			if (dev->osensto <= delta) {
				rmidi_putack(RMIDI(dev));
				rmidi_flush(RMIDI(dev));
				dev->osensto = MIDIDEV_OSENSTO;
			} else {
				dev->osensto -= delta;
			}
		}
	}
	
	/*
	 * update clocks, when necessary
	 */
	if (!mididev_master) {	
		/* 
		 * internal clock source (no master) 
		 */
		if (mux_phase == MUX_STARTWAIT) {
			mux_chgphase(MUX_START);
			mux_sendstart();
			mux_curpos = 0;
			mux_nextpos = MUX_START_DELAY;
			if (mux_debug) {
				dbg_puts("mux_timercb: generated start\n");
			}
			mux_flush();
		}

		if (mux_phase == MUX_STOPWAIT) {
			mux_chgphase(MUX_STOP);
			if (mux_debug) {
				dbg_puts("mux_timercb: generated stop\n");
			}
			mux_ops->stop(mux_addr);
			mux_sendstop();
			mux_flush();
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
			if (mux_phase == MUX_NEXT) {
				mux_curtic++;
				mux_ops->move(mux_addr);
			} else if (mux_phase == MUX_FIRST) {
				mux_curtic = 0;
				mux_ops->start(mux_addr);
			}
			mux_flush();	
		}
	}
}

/*
 * called when a MIDI TICK is received
 */
void
mux_ticcb(unsigned unit) {
	struct mididev *dev = mididev_byunit[unit];

	if (!mididev_master || dev != mididev_master) {	
		return;
	}
	while (mididev_master->ticdelta >= mididev_master->ticrate) {
		if (mux_phase == MUX_FIRST) {
			mux_chgphase(MUX_NEXT);
		} else if (mux_phase == MUX_START) {
			mux_chgphase(MUX_FIRST);
		}
		if (mux_phase == MUX_NEXT) {
			mux_curtic++;
			mux_ops->move(mux_addr);
		} else if (mux_phase == MUX_FIRST) {
			mux_curtic = 0;
			mux_ops->start(mux_addr);
		}
		/* XXX: where is the flush ?! */
		mididev_master->ticdelta -= mididev_master->ticrate;
	}
	mididev_master->ticdelta += mux_ticrate;
}

/*
 * called when a MIDI START event is received
 */
void
mux_startcb(unsigned unit) {
	struct mididev *dev = mididev_byunit[unit];

	if (!mididev_master || dev != mididev_master) {	
		return;
	}
	mux_chgphase(MUX_START);
	if (mux_debug) {
		dbg_puts("mux_startcb: got start event\n");
	}
}

/*
 * called when a MIDI STOP event is received
 */
void
mux_stopcb(unsigned unit) {
	struct mididev *dev = mididev_byunit[unit];

	if (!mididev_master || dev != mididev_master) {	
		return;
	}
	mux_chgphase(MUX_STOP);
	if (mux_debug) {
		dbg_puts("mux_startcb: got stop\n");
	}
	mux_ops->stop(mux_addr);
}

/*
 * called when a MIDI Active-sensing is received
 */
void
mux_ackcb(unsigned unit) {
	struct mididev *dev = mididev_byunit[unit];

	if (dev->isensto == 0) {
		dbg_putu(dev->unit);
		dbg_puts(": sensing enabled\n");
		dev->isensto = MIDIDEV_ISENSTO;
	}
}

/*
 * called when a MIDI voice event is received
 */
void
mux_evcb(unsigned unit, struct ev *ev) {
	struct ev rev;
#ifdef MUX_DEBUG
	dbg_puts("mux_evcb: ");
	ev_dbg(ev);
	dbg_puts("\n");
#endif
	if (conv_packev(&mux_istate, ev, &rev)) {
		mux_ops->ev(mux_addr, &rev);
	}
}

/*
 * called if an error is detected.
 * currently we send an all note off and all ctls reset
 *
 * XXX: these controllers should be handled by the filter
 *	they should shutdown recorded states following filtering
 *	rules.
 */
void
mux_errorcb(unsigned unit) {
	unsigned i;
	struct ev ev;
	
	/* 
	 * send all sound off and ctls reset
	 */
	ev.cmd = EV_XCTL;
	ev.dev = unit;
	ev.ctl_num = 120;		/* all sound off */
	ev.ctl_val = 0;
	for (i = 0; i <= EV_MAXCH; i++) {
		ev.ch = i;
		mux_ops->ev(mux_addr, &ev);
	}
	ev.ctl_num = 121;		/* all ctl reset */
	ev.ctl_val = 0;
	for (i = 0; i <= EV_MAXCH; i++) {
		ev.ch = i;
		mux_ops->ev(mux_addr, &ev);
	}
	mux_flush();
}

void
mux_run(void) {
	mux_mdep_run();
	
	if (!mididev_master) {
		if (mux_phase > MUX_START && mux_phase < MUX_STOP) {
			mux_chgphase(MUX_STOP);
			mux_ops->stop(mux_addr);
			mux_sendstop();
			mux_flush();
		}
	}
}


/*
 * called when an sysex has been received
 */
void
mux_sysexcb(unsigned unit, struct sysex *sysex) {
	/* XXX: remove list put()/get() */
	sysexlist_put(&mux_sysexlist, sysex);
	mux_ops->sysex(mux_addr, sysex);
}


/* -------------------------------------- user "public" functions --- */

struct sysex *
mux_getsysex(void) {
	return sysexlist_get(&mux_sysexlist);
}

void
mux_flush(void) {
	struct mididev *dev;
	for (dev = mididev_list; dev != NULL; dev = dev->next) {
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
