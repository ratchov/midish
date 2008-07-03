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
 * implements trackxxx built-in functions available through the
 * interpreter.
 *
 * for more details about what does each function, refer to
 * manual.html
 */

#include "dbg.h"
#include "default.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"

#include "frame.h"
#include "track.h"
#include "song.h"
#include "user.h"

#include "saveload.h"
#include "textio.h"



unsigned
user_func_trackmerge(struct exec *o, struct data **r)
{
	struct songtrk *src, *dst;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuptrack(o, "source", &src) ||
	    !exec_lookuptrack(o, "dest", &dst)) {
		return 0;
	}
	track_merge(&src->track, &dst->track);
	return 1;
}

unsigned
user_func_trackquant(struct exec *o, struct data **r)
{
	char *trkname;
	struct songtrk *t;
	long from, amount;
	unsigned start, len, offset;
	long quant, rate;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "trackname", &trkname) ||
	    !exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "quantum", &quant) ||
	    !exec_lookuplong(o, "rate", &rate)) {
		return 0;
	}
	t = song_trklookup(usong, trkname);
	if (t == NULL) {
		cons_err("trackquant: no such track");
		return 0;
	}
	if (rate > 100) {
		cons_err("trackquant: rate must be between 0 and 100");
		return 0;
	}

	start = track_findmeasure(&usong->meta, from);
	len = track_findmeasure(&usong->meta, from + amount) - start;

	if (quant < 0 || (unsigned)quant > usong->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}
	if (start > (unsigned)quant / 2) {
		start -= quant / 2;
		offset = quant / 2;
	} else {
		offset = 0;
		len -= quant / 2;
	}
	track_quantize(&t->track, start, len, offset, (unsigned)quant, (unsigned)rate);
	return 1;
}

unsigned
user_func_tracktransp(struct exec *o, struct data **r)
{
	struct songtrk *t;
	long from, amount, quant, halftones;
	unsigned tic, len;
	struct evspec es;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "halftones", &halftones) ||
	    !exec_lookuplong(o, "quantum", &quant) ||
	    !exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}
	tic = track_findmeasure(&usong->meta, from);
	len = track_findmeasure(&usong->meta, from + amount) - tic;

	if (quant < 0 || (unsigned)quant > usong->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}

	if (tic > (unsigned)quant/2) {
		tic -= quant / 2;
	} else {
		len -= quant / 2;
	}
	track_transpose(&t->track, tic, len, halftones);
	return 1;
}

unsigned
user_func_tracksetmute(struct exec *o, struct data **r)
{
	struct songtrk *t;
	long flag;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookupbool(o, "muteflag", &flag)) {
		return 0;
	}
	t->mute = flag;
	return 1;
}

unsigned
user_func_trackgetmute(struct exec *o, struct data **r)
{
	struct songtrk *t;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	*r = data_newlong(t->mute);
	return 1;
}

unsigned
user_func_trackchanlist(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct songchan *c;
	struct data *num;
	char map[DEFAULT_MAXNCHANS];
	unsigned i;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	*r = data_newlist(NULL);
	track_chanmap(&t->track, map);
	for (i = 0; i < DEFAULT_MAXNCHANS; i++) {
		if (map[i]) {
			c = song_chanlookup_bynum(usong, i / 16, i % 16);
			if (c != 0) {
				data_listadd(*r, data_newref(c->name.str));
			} else {
				num = data_newlist(NULL);
				data_listadd(num, data_newlong(i / 16));
				data_listadd(num, data_newlong(i % 16));
				data_listadd(*r, num);
			}
		}
	}
	return 1;
}

unsigned
user_func_trackinfo(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct seqptr mp, tp;
	struct evspec es;
	struct state *st;
	long quant;
	unsigned len, count, count_next, tpb, bpm;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookuplong(o, "quantum", &quant) ||
	    !exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}

	if (quant < 0 || (unsigned)quant > usong->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);
	textout_indent(tout);

	count_next = 0;
	seqptr_init(&tp, &t->track);
	seqptr_init(&mp, &usong->meta);
	for (;;) {
		/*
		 * scan for a time signature change
		 */
		while (seqptr_evget(&mp)) {
			/* nothing */
		}
		seqptr_getsign(&mp, &bpm, &tpb);

		/*
		 * count starting events
		 */
		len = bpm * tpb;
		count = count_next;
		count_next = 0;
		for (;;) {
			len -= seqptr_ticskip(&tp, len);
			if (len == 0)
				break;
			st = seqptr_evget(&tp);
			if (st == NULL)
				break;
			if (st->phase & EV_PHASE_FIRST) {
				if (state_inspec(st, &es)) {
					if (len >= (unsigned)quant / 2)
						count++;
					else
						count_next++;
				}
	                }
		}
		textout_putlong(tout, count);
		textout_putstr(tout, " ");
		if (len > 0) {
			if (len < (unsigned)quant / 2) {
				textout_putlong(tout, count_next);
				textout_putstr(tout, " ");
			}
			break;
		}
		(void)seqptr_skip(&mp, bpm * tpb);
	}
	seqptr_done(&mp);
	seqptr_done(&tp);

	textout_putstr(tout, "\n");
	textout_shiftleft(tout);
	textout_indent(tout);
	textout_putstr(tout, "}\n");
	return 1;
}
