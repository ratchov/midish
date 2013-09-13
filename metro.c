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

#include "dbg.h"
#include "mux.h"
#include "metro.h"
#include "song.h"

void metro_tocb(void *);

/*
 * initialize the metronome with some defaults
 */
void
metro_init(struct metro *o)
{
	o->mode = 0;
	o->mask = (1 << SONG_REC);
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
		log_puts("metro_tocb: no click sounding\n");
		panic();
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
	if ((o->mask & (1 << o->mode)) && tic == 0) {
		/*
		 * if the last metronome click is sounding
		 * abord the timeout and stop the click
		 */
		if (o->ev) {
			log_puts("metro_tic: nested clicks\n");
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

/*
 * set the mode of the metronome
 */
void
metro_setmode(struct metro *o, unsigned mode)
{
	if ((o->mask & (1 << o->mode)) && !(o->mask & (1 << mode)))
		metro_shut(o);
	o->mode = mode;
}

/*
 * change the current mask
 */
void
metro_setmask(struct metro *o, unsigned mask)
{
	if ((o->mask & (1 << o->mode)) && !(mask & (1 << o->mode)))
		metro_shut(o);
	o->mask = mask;
}

unsigned
metro_str2mask(struct metro *o, char *mstr, unsigned *rmask)
{
	unsigned mask;

	if (str_eq(mstr, "on")) {
		mask = (1 << SONG_PLAY) + (1 << SONG_REC);
	} else if (str_eq(mstr, "rec")) {
		mask = (1 << SONG_REC);
	} else if (str_eq(mstr, "off")) {
		mask = 0;
	} else {
		return 0;
	}
	*rmask = mask;
	return 1;
}

