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
 * rmidi_s extends mididev_s and imprlements
 * raw midi devices (à la {Open,Net}BSD and OSS ones)
 *
 * converts midi bytes (ie unsigned char *) 
 * to midi events (struct ev_s) and calls mux_evcb on the input
 *
 * converts midi events into bytes and sends them on the wire
 */

#include "dbg.h"
#include "default.h"
#include "mdep.h"
#include "ev.h"
#include "mux.h"
#include "rmidi.h"

#define MIDI_TIC	0xf8
#define MIDI_START	0xfa
#define MIDI_STOP	0xfc
#define MIDI_ACK	0xfe

struct rmidi_s *
rmidi_new(void) {
	struct rmidi_s *o;
	o = (struct rmidi_s *)mem_alloc(sizeof(struct rmidi_s));
	rmidi_init(o);
	return o;
}

void
rmidi_delete(struct rmidi_s *o) {
	rmidi_done(o);
	mem_free(o);
}

void 
rmidi_init(struct rmidi_s *o) {	
	o->oused = 0;
	o->istatus = o->ostatus = 0;
	mididev_init(&o->mididev);
	rmidi_mdep_init(o);
}

void 
rmidi_done(struct rmidi_s *o) {
	if (o->oused != 0) {
		dbg_puts("rmidi_done: output buffer is not empty, continuing...\n");
	}
	rmidi_mdep_done(o);
	mididev_done(&o->mididev);
}


unsigned rmidi_evlen[] = { 2, 2, 2, 2, 1, 1, 2, 0 };
#define RMIDI_EVLEN(status) (rmidi_evlen[((status) >> 4) & 7])

	/*
	 * rmidi_inputcb is called when midi data becomes available
	 * it calls the mux_evcb
	 */

void
rmidi_inputcb(struct rmidi_s *o, unsigned char *buf, unsigned count) {
	struct ev_s ev;
	unsigned data;

	while (count != 0) {
		data = *buf;
		count--;
		buf++;
		
		if (data >= 0xf8) {
			switch(data) {
			case MIDI_TIC:
				ev.cmd = EV_TIC;
				mux_evcb(o->mididev.unit, &ev);
				break;
			case MIDI_START:
				ev.cmd = EV_START;
				mux_evcb(o->mididev.unit, &ev);
				break;
			case MIDI_STOP:
				ev.cmd = EV_STOP;
				mux_evcb(o->mididev.unit, &ev);
				break;
			case MIDI_ACK:
				/* for now, ignore midi ACKs */
				break;
			default:
				dbg_puts("rmidi_inputcb: ");
				dbg_putx(data);
				dbg_puts(" : unimplemented midi status, skipped\n");
				break;
			}
		} else if (data >= 0x80) {
			o->istatus = data;
			o->icount = 0;
		} else {
			if (o->istatus >= 0x80 && o->istatus < 0xf0) {
				o->idata[o->icount] = (unsigned char)data;
				o->icount++;

				if (o->icount == RMIDI_EVLEN(o->istatus)) { 
					o->icount = 0;
					ev.cmd = o->istatus >> 4;
					ev.data.voice.b0 = o->idata[0];
					ev.data.voice.b1 = o->idata[1];
					ev.data.voice.chan = (o->istatus & 0x0f) + (o->mididev.unit << 4);
					if (ev.cmd == EV_NON && ev.data.voice.b1 == 0) {
						ev.cmd = EV_NOFF;
						ev.data.voice.b1 = EV_NOFF_DEFAULTVEL;
					}
					mux_evcb(o->mididev.unit, &ev);
				}
			}
		}
	}
}

	/*
	 * writes a midi byte to the output buffer, non blocking
	 */

void
rmidi_out(struct rmidi_s *o, unsigned data) {
	if (o->oused == RMIDI_BUFLEN) {
		dbg_puts("rmidi_out: buffer overflow\n");
		dbg_panic();
	}
	o->obuf[o->oused] = (unsigned char)data;
	o->oused++;
}
	/*
	 * stores an event in the output buffer
	 */

void
rmidi_putev(struct rmidi_s *o, struct ev_s *ev) {
	unsigned s;
	
	switch (ev->cmd) {
	case EV_TIC:
		rmidi_out(o, MIDI_TIC);
		break;
	
	case EV_START:
		rmidi_out(o, MIDI_START);
		break;
	
	case EV_STOP:
		rmidi_out(o, MIDI_STOP);
		break;
	
	default:
		if (ev->cmd == EV_NOFF) {
			ev->cmd = EV_NON;
			ev->data.voice.b1 = 0;
		}
		if (!EV_ISVOICE(ev)) {
			break;
		}
		s = (ev->data.voice.chan & 0x0f) + ((ev->cmd & 0x0f) << 4);
		if (s != o->ostatus) {
			o->ostatus = s;
			rmidi_out(o, s);
		}

		rmidi_out(o, ev->data.voice.b0);
		if (RMIDI_EVLEN(s) == 2) {
			rmidi_out(o, ev->data.voice.b1);
		}
		break;
	}
}
