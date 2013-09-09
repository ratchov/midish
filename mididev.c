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
 * mididev is a generic midi device structure it doesn't contain any
 * device-specific fields and shoud be extended by other structures
 *
 * this module also, manages a global table of generic midi
 * devices. The table is indexed by the device "unit" number, the
 * same that is stored in the event structure
 *
 * this modules converts midi bytes (ie 'unsigned char') to midi events
 * (struct ev) and calls mux_xxx callbacks to handle midi
 * input. Similarly, it converts midi events to bytes and sends them on
 * the wire
 *
 * the module provides the following methods:
 *
 * - mididev_open() to open the device and setup the parser
 *
 * - mididev_close() to release the device and the parser
 *
 * - mididev_inputcb() routine called by the lower layer when
 *   midi input has been read(), basically it decodes the midi byte
 *   stream and calls mux_xxx() routines
 *
 * - mididev_put{ev,start,stop,tic,ack}() routines send respectively
 *   a voice event, clock start, clock stop, clock tick and midi
 *   active sense.
 *
 */

#include "dbg.h"
#include "defs.h"
#include "mididev.h"
#include "pool.h"
#include "cons.h"
#include "str.h"
#include "ev.h"
#include "sysex.h"
#include "mux.h"
#include "timo.h"
#include "conv.h"

#define MIDI_SYSEXSTART	0xf0
#define MIDI_QFRAME	0xf1
#define MIDI_SYSEXSTOP	0xf7
#define MIDI_TIC	0xf8
#define MIDI_START	0xfa
#define MIDI_STOP	0xfc
#define MIDI_ACK	0xfe

#define MTC_FPS_24	0
#define MTC_FPS_25	1
#define MTC_FPS_30	3
#define MTC_FULL_LEN	10	/* size of MTC ``full frame'' sysex */

unsigned mididev_debug = 0;

unsigned mididev_evlen[] = { 2, 2, 2, 2, 1, 1, 2, 0 };
#define MIDIDEV_EVLEN(status) (mididev_evlen[((status) >> 4) & 7])

struct mididev *mididev_list, *mididev_clksrc, *mididev_mtcsrc;
struct mididev *mididev_byunit[DEFAULT_MAXNDEVS];

/*
 * initialize the mtc "parser" to a state, when a full message or 2 complete
 * frames are needed to lock to the master
 */
void
mtc_init(struct mtc *mtc)
{
	mtc->tps = 0;
	mtc->qfr = 0;
	mtc->pos = 0xdeadbeef;
	mtc->state = MTC_STOP;
	mtc->timo = 0;
};

/*
 * convert MTC-style frames per second into MTC_SEC units
 * return 0 if not supported
 */
int
mtc_setfps(struct mtc *mtc, unsigned id)
{
	switch (id) {
	case MTC_FPS_24:
		mtc->tps = MTC_SEC / (24 * 4);
		break;
	case MTC_FPS_25:
		mtc->tps = MTC_SEC / (25 * 4);
		break;
	case MTC_FPS_30:
		mtc->tps = MTC_SEC / (30 * 4);
		break;
	default:
		if (mtc->tps != 0) {
			dbg_putu(id);
			dbg_puts(": not supported MTC frame rate\n");
		}
		mtc->tps = 0;
		return 0;
	}
	return 1;
}

/*
 * called when timeout expires, ie MTC stopped
 */
void
mtc_timo(struct mtc *mtc)
{
	if (mididev_debug)
		dbg_puts("mtc_timo: stopped\n");
	mtc->state = MTC_STOP;
	mux_mtcstop();
}

/*
 * handle a quarter frame message
 */
void
mtc_tick(struct mtc *mtc, unsigned data)
{
	unsigned pos;
	int delta;

	if (mtc->state == MTC_STOP)
		return;
	if (data >> 4 != mtc->qfr) {
		if (mididev_debug)
			dbg_puts("mtc sync err\n");
		return;
	}
	if (mtc->state == MTC_RUN) {
		mtc->pos += mtc->tps;
		if (mtc->pos >= MTC_PERIOD)
			mtc->pos -= MTC_PERIOD;
		mux_mtctick(mtc->tps * (24000000 / MTC_SEC));
	} else {
		mtc->state = MTC_RUN;
		mux_mtctick(0);
	}
	mtc->nibble[mtc->qfr++] = data & 0xf;
	if (mtc->qfr < 8)
		return;
	mtc->timo = 24000000 / 4;
	pos = mtc->tps * 4 * (mtc->nibble[0] +  (mtc->nibble[1]      << 4)) +
	    MTC_SEC *        (mtc->nibble[2] +  (mtc->nibble[3]      << 4)) +
	    MTC_SEC * 60 *   (mtc->nibble[4] +  (mtc->nibble[5]      << 4)) +
	    MTC_SEC * 3600 * (mtc->nibble[6] + ((mtc->nibble[7] & 1) << 4));
	pos += 7 * mtc->tps;
	if (pos >= MTC_PERIOD)
		pos -= MTC_PERIOD;
	if (pos != mtc->pos) {
		delta = (int)pos - (int)mtc->pos;
		if (delta < MTC_PERIOD / 2)
			delta += MTC_PERIOD;
		if (delta >= MTC_PERIOD / 2)
			delta -= MTC_PERIOD;
		dbg_puts("mtc_tick: went off by ");
		dbg_puti(delta);
		dbg_puts(" ticks\n");
		if (delta > 0 && delta < MTC_SEC / 6) {
			mux_mtctick(delta);
			mtc->pos = pos;
		} else {
			mtc->state = MTC_STOP;
			mux_mtcstop();
		}
	}
	mtc->qfr = 0;
}

/*
 * handle a full frame message
 */
void
mtc_full(struct mtc *mtc, struct sysex *x)
{
	unsigned char *data;

	if (x->first == NULL ||
	    x->first->next != NULL ||
	    x->first->used != 10)
		return;
	data = x->first->data;
	if (data[1] != 0x7f ||
	    data[2] != 0x7f ||
	    data[3] != 0x01 ||
	    data[4] != 0x01)
		return;
	if (!mtc_setfps(mtc, data[5] >> 5))
		return;
	mtc->qfr = 0;
	mtc->pos = MTC_SEC * 3600 * (data[5] & 0x1f) +
	    MTC_SEC * 60 * data[6] +
	    MTC_SEC * data[7] +
	    mtc->tps * 4 * data[8];
	mtc->state = MTC_START;
	if (mididev_debug) {
		dbg_puts("mtc_full: start at ");
		dbg_putu(mtc->pos);
		dbg_puts("\n");
	}
	mux_mtcstart(mtc->pos);
}

/*
 * initialize the device independent part of the device structure
 */
void
mididev_init(struct mididev *o, struct devops *ops, unsigned mode)
{
	/*
	 * by default we don't transmit realtime information
	 * (midi_tic, midi_start, midi_stop etc...)
	 */
	o->ops = ops;
	o->sendclk = 0;
	o->sendmmc = 1;
	o->ticrate = DEFAULT_TPU;
	o->ticdelta = 0xdeadbeef;
	o->mode = mode;
	o->ixctlset = 0;	/* all input controllers are 7bit */
	o->oxctlset = 0;
	o->ievset = CONV_XPC | CONV_NRPN | CONV_RPN;
	o->oevset = CONV_XPC | CONV_NRPN | CONV_RPN;
	o->eof = 1;

	/*
	 * reset parser
	 */
	o->oused = 0;
	o->istatus = o->ostatus = 0;
	o->isysex = NULL;
	o->runst = 1;
	o->sync = 0;
}

/*
 * release the device independent part of the device structure: for
 * future use
 */
void
mididev_done(struct mididev *o)
{
	if (mux_isopen)
		mididev_close(o);
}

/*
 * open the device and initialize the parser
 */
void
mididev_open(struct mididev *o)
{
	o->eof = 0;
	o->oused = 0;
	o->istatus = o->ostatus = 0;
	o->isysex = NULL;
	mtc_init(&o->imtc);
	o->ops->open(o);
}

/*
 * close the device
 */
void
mididev_close(struct mididev *o)
{
	o->ops->close(o);
	if (o->oused) {
		/*
		 * XXX: should we flush instead of printing error ?
		 */
		dbg_puts("mididev_close: device not flushed\n");
	}
	o->eof = 1;
}

/*
 * flush the given midi device
 */
void
mididev_flush(struct mididev *o)
{
	unsigned count, todo;
	unsigned char *buf;
	unsigned i;

	if (!o->eof) {
		if (mididev_debug && o->oused > 0) {
			dbg_puts("mididev_flush: ");
			dbg_putu(timo_abstime / 24);
			dbg_puts(": dev ");
			dbg_putu(o->unit);
			dbg_puts(":");
			for (i = 0; i < o->oused; i++) {
				dbg_puts(" ");
				dbg_putx(o->obuf[i]);
			}
			dbg_puts("\n");
		}
		todo = o->oused;
		buf = o->obuf;
		while (todo > 0) {
			count = o->ops->write(o, buf, todo);
			if (o->eof)
				break;
			todo -= count;
			buf += count;
		}
		if (o->oused)
			o->osensto = MIDIDEV_OSENSTO;
	}
	o->oused = 0;
}

/*
 * mididev_inputcb is called when midi data becomes available
 * it calls mux_evcb
 */
void
mididev_inputcb(struct mididev *o, unsigned char *buf, unsigned count)
{
	struct ev ev;
	unsigned i, data;

	if (!(o->mode & MIDIDEV_MODE_IN)) {
		dbg_puts("received data from output only device\n");
		return;
	}
	if (mididev_debug) {
		dbg_puts("mididev_inputcb: ");
		dbg_putu(timo_abstime / 24);
		dbg_puts(": dev ");
		dbg_putu(o->unit);
		dbg_puts(":");
		for (i = 0; i < count; i++) {
			dbg_puts(" ");
			dbg_putx(buf[i]);
		}
		dbg_puts("\n");
	}
	while (count != 0) {
		data = *buf;
		count--;
		buf++;
		if (data >= 0xf8) {
			switch(data) {
			case MIDI_TIC:
				if (o == mididev_clksrc)
					mux_ticcb();
				break;
			case MIDI_START:
				if (o == mididev_clksrc) {
					o->ticdelta = o->ticrate;
					mux_startcb();
				}
				break;
			case MIDI_STOP:
				if (o == mididev_clksrc)
					mux_stopcb();
				break;
			case MIDI_ACK:
				mux_ackcb(o->unit);
				break;
			default:
				if (mididev_debug) {
					dbg_puts("mididev_inputcb: ");
					dbg_putx(data);
					dbg_puts(" : skipped unimplemented message\n");
				}
				break;
			}
		} else if (data >= 0x80) {
			if (mididev_debug &&
			    o->istatus >= 0x80 &&  o->icount > 0 &&
			    o->icount < MIDIDEV_EVLEN(o->istatus)) {
				/*
				 * midi spec says messages can be aborted
				 * by status byte, so don't trigger an error
				 */
				dbg_puts("mididev_inputcb: ");
				dbg_putx(o->istatus);
				dbg_puts(": skipped aborted message\n");
			}
			o->istatus = data;
			o->icount = 0;
			switch(data) {
			case MIDI_SYSEXSTART:
				if (o->isysex) {
					if (mididev_debug)
						dbg_puts("mididev_inputcb: previous sysex aborted\n");
					sysex_del(o->isysex);
				}
				o->isysex = sysex_new(o->unit);
				sysex_add(o->isysex, data);
				break;
			case MIDI_SYSEXSTOP:
				if (o->isysex) {
					sysex_add(o->isysex, data);
					if (o == mididev_mtcsrc)
						mtc_full(&o->imtc, o->isysex);
					mux_sysexcb(o->unit, o->isysex);
					o->isysex = NULL;
				}
				o->istatus = 0;
				break;
			default:
				/*
				 * sysex message without the stop byte
				 * is considered as aborted.
				 */
				if (o->isysex) {
					if (mididev_debug)
						dbg_puts("mididev_inputcb: current sysex aborted\n");
					sysex_del(o->isysex);
					o->isysex = NULL;
				}
				break;
			}
		} else if (o->istatus >= 0x80 && o->istatus < 0xf0) {
			o->idata[o->icount] = (unsigned char)data;
			o->icount++;

			if (o->icount == MIDIDEV_EVLEN(o->istatus)) {
				o->icount = 0;
				ev.cmd = o->istatus >> 4;
				ev.dev = o->unit;
				ev.ch = o->istatus & 0x0f;
				if (ev.cmd == EV_NON && o->idata[1] == 0) {
					ev.cmd = EV_NOFF;
					ev.note_num = o->idata[0];
					ev.note_vel = EV_NOFF_DEFAULTVEL;
				} else if (ev.cmd == EV_BEND) {
					ev.bend_val = ((unsigned)o->idata[1] << 7) + o->idata[0];
				} else {
					ev.v0 = o->idata[0];
					ev.v1 = o->idata[1];
				}
				mux_evcb(o->unit, &ev);
			}
		} else if (o->istatus == MIDI_SYSEXSTART) {
			sysex_add(o->isysex, data);
		} else if (o->istatus == MIDI_QFRAME) {
			/*
			 * NOTE: MIDI uses running status only for voice events
			 *	 so, if you add new system common messages
			 *       here don't forget to reset the running status
			 */
			if (o == mididev_mtcsrc)
				mtc_tick(&o->imtc, data);
			o->istatus = 0;
		}
	}
}

/*
 * write a single midi byte to the output buffer, if
 * it is full, flush it. Shouldn't we inline it?
 */
void
mididev_out(struct mididev *o, unsigned data)
{
	if (!(o->mode & MIDIDEV_MODE_OUT)) {
		return;
	}
	if (o->oused == MIDIDEV_BUFLEN) {
		mididev_flush(o);
	}
	o->obuf[o->oused] = (unsigned char)data;
	o->oused++;
}

void
mididev_putstart(struct mididev *o)
{
	mididev_out(o, MIDI_START);
	if (o->sync)
		mididev_flush(o);
}

void
mididev_putstop(struct mididev *o)
{
	mididev_out(o, MIDI_STOP);
	if (o->sync)
		mididev_flush(o);
}

void
mididev_puttic(struct mididev *o)
{
	mididev_out(o, MIDI_TIC);
	if (o->sync)
		mididev_flush(o);
}

void
mididev_putack(struct mididev *o)
{
	mididev_out(o, MIDI_ACK);
	if (o->sync)
		mididev_flush(o);
}

/*
 * convert a voice event to byte stream and queue
 * it for sending
 */
void
mididev_putev(struct mididev *o, struct ev *ev)
{
	unsigned char *p;
	unsigned s;

	if (EV_ISSX(ev)) {
		p = evinfo[ev->cmd].pattern;
		for (;;) {
			switch (*p) {
			case EV_PATV0_HI:
				mididev_out(o, ev->v0 >> 7);
				break;
			case EV_PATV0_LO:
				mididev_out(o, ev->v0 & 0x7f);
				break;
			case EV_PATV1_HI:
				mididev_out(o, ev->v1 >> 7);
				break;
			case EV_PATV1_LO:
				mididev_out(o, ev->v1 & 0x7f);
				break;
			default:
				mididev_out(o, *p);
				if (*p == 0xf7)
					goto end;
			}
			p++;
		}
		o->runst = 0;
	}
	if (!EV_ISVOICE(ev)) {
		return;
	}
	if (ev->cmd == EV_NOFF) {
		s = ev->ch + (EV_NON << 4);
		if (!o->runst || s != o->ostatus) {
			o->ostatus = s;
			mididev_out(o, s);
		}
		mididev_out(o, ev->note_num);
		mididev_out(o, 0);
	} else if (ev->cmd == EV_BEND) {
		s = ev->ch + (EV_BEND << 4);
		if (!o->runst || s != o->ostatus) {
			o->ostatus = s;
			mididev_out(o, s);
		}
		mididev_out(o, ev->bend_val & 0x7f);
		mididev_out(o, ev->bend_val >> 7);
	} else {
		s = ev->ch + (ev->cmd << 4);
		if (!o->runst || s != o->ostatus) {
			o->ostatus = s;
			mididev_out(o, s);
		}
		mididev_out(o, ev->v0);
		if (MIDIDEV_EVLEN(s) == 2) {
			mididev_out(o, ev->v1);
		}
	}
end:
	if (o->sync)
		mididev_flush(o);
}

/*
 * queue raw data for sending
 */
void
mididev_sendraw(struct mididev *o, unsigned char *buf, unsigned len)
{
	if (!(o->mode & MIDIDEV_MODE_OUT)) {
		return;
	}
	while (len > 0) {
		if (o->oused == MIDIDEV_BUFLEN) {
			mididev_flush(o);
		}
		o->obuf[o->oused] = *buf;
		o->oused++;
		len--;
		buf++;
	}
	/*
	 * since we don't parse the buffer, reset running status
	 */
	o->ostatus = 0;
	if (o->sync)
		mididev_flush(o);
}

/*
 * initialize the device table
 */
void
mididev_listinit(void)
{
	unsigned i;
	for (i = 0; i < DEFAULT_MAXNDEVS; i++) {
		mididev_byunit[i] = NULL;
	}
	mididev_list = NULL;
	mididev_mtcsrc = NULL;	/* no external timer, use internal timer */
	mididev_clksrc = NULL;	/* no clock source, use internal clock */
}

/*
 * unregister all entries of the device table
 */
void
mididev_listdone(void)
{
	unsigned i;
	struct mididev *dev;

	for (i = 0; i < DEFAULT_MAXNDEVS; i++) {
		dev = mididev_byunit[i];
		if (dev != NULL) {
			dev->ops->del(dev);
			mididev_byunit[i] = NULL;
		}
	}
	mididev_clksrc = NULL;
	mididev_list = NULL;
}

/*
 * register a new device number (ie "unit")
 */
unsigned
mididev_attach(unsigned unit, char *path, unsigned mode)
{
	struct mididev *dev;

	if (unit >= DEFAULT_MAXNDEVS) {
		cons_err("given unit is too large");
		return 0;
	}
	if (mididev_byunit[unit] != NULL) {
		cons_err("device already exists");
		return 0;
	}
#if defined(USE_SNDIO)
	dev = sndio_new(path, mode);
#elif defined(USE_ALSA)
	dev = alsa_new(path, mode);
#else
	dev = raw_new(path, mode);
#endif
	if (dev == NULL)
		return 0;
	dev->next = mididev_list;
	mididev_list = dev;
	mididev_byunit[unit] = dev;
	dev->unit = unit;
	return 1;
}

/*
 * unregister the given device number
 */
unsigned
mididev_detach(unsigned unit)
{
	struct mididev **i, *dev;

	if (unit >= DEFAULT_MAXNDEVS || mididev_byunit[unit] == NULL) {
		cons_err("no such device");
		return 0;
	}

	if (mididev_byunit[unit] == mididev_clksrc ||
	    mididev_byunit[unit] == mididev_mtcsrc) {
		cons_err("cant detach master device");
		return 0;
	}

	for (i = &mididev_list; *i != NULL; i = &(*i)->next) {
		dev = *i;
		if (dev->unit == unit) {
			*i = dev->next;
			dev->ops->del(dev);
			mididev_byunit[unit] = NULL;
			return 1;
		}
	}
	dbg_puts("mididev_detach: the device is not in the list\n");
	dbg_panic();
	return 0;
}
