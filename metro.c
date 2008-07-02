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

#include "dbg.h"
#include "mux.h"
#include "metro.h"

void metro_tocb(void *);

/*
 * initialize the metronome with some defaults
 */
void
metro_init(struct metro *o) 
{
	o->enabled = 1;
	o->hi.cmd = EV_NON;
	o->hi.dev = DEFAULT_METRO_DEV;
	o->hi.ch  = DEFAULT_METRO_CHAN;
	o->hi.note_num = DEFAULT_METRO_HI_NOTE;
	o->hi.note_vel = DEFAULT_METRO_HI_VEL;
	o->lo.cmd = EV_NON;
	o->lo.dev = DEFAULT_METRO_DEV;
	o->lo.ch  = DEFAULT_METRO_CHAN;
	o->lo.note_num = DEFAULT_METRO_LO_NOTE;
	o->lo.note_vel = DEFAULT_METRO_LO_VEL;
	o->ev = NULL;
	timo_set(&o->to, metro_tocb, o);
}

/*
 * free the metronome
 */
void
metro_done(struct metro *o) 
{
	/* nothing for now */
}

/*
 * callback that sends the note-off event of the click
 */ 
void
metro_tocb(void *addr) 
{
	struct metro *o = (struct metro *)addr;
	struct ev ev;

	if (o->ev == NULL) {
		dbg_puts("metro_tocb: no click sounding\n");
		dbg_panic();
	}
	ev.cmd = EV_NOFF;
	ev.dev = o->ev->dev;
	ev.ch  = o->ev->ch;
	ev.note_num = o->ev->note_num;
	ev.note_vel = EV_NOFF_DEFAULTVEL;
	mux_putev(&ev);
	o->ev = NULL;
}

/*
 * if the position pointer is on the first tic
 * of a beat then send a "click" to the midi output
 * note: the output is not flushed
 */
void
metro_tic(struct metro *o, unsigned beat, unsigned tic) 
{
	if (o->enabled && tic == 0) {
		/*
		 * if the last metronome click is sounding
		 * abord the timeout and stop the click 
		 */
		if (o->ev) {
			dbg_puts("metro_tic: nested clicks\n");
			timo_del(&o->to);
			metro_tocb(o);
		}
		if (beat == 0) {
			o->ev = &o->hi;
		} else {
			o->ev = &o->lo;
		}
		mux_putev(o->ev);
		/*
		 * schedule the corrsponding note off in 30ms
		 */
		timo_add(&o->to, DEFAULT_METRO_CLICKLEN);
	}
}

/*
 * send the note off event of the click (if any)
 */
void
metro_shut(struct metro *o) 
{
	if (o->ev) {
		timo_del(&o->to);
		metro_tocb(o);
	}
}

