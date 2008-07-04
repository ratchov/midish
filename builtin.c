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
#include "default.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"
#include "frame.h"
#include "song.h"
#include "user.h"
#include "smf.h"
#include "saveload.h"
#include "textio.h"
#include "mux.h"
#include "rmidi.h"
#include "builtin.h"

unsigned
blt_panic(struct exec *o, struct data **r)
{
	dbg_panic();
	/* not reached */
	return 0;
}

unsigned
blt_debug(struct exec *o, struct data **r)
{
	char *flag;
	long value;

	if (!exec_lookupname(o, "flag", &flag) ||
	    !exec_lookuplong(o, "value", &value)) {
		return 0;
	}
	if (str_eq(flag, "rmidi")) {
		rmidi_debug = value;
	} else if (str_eq(flag, "filt")) {
		filt_debug = value;
	} else if (str_eq(flag, "song")) {
		song_debug = value;
	} else if (str_eq(flag, "mem")) {
		mem_debug = value;
	} else {
		cons_err("debug: unknuwn debug-flag");
		return 0;
	}
	return 1;
}

unsigned
blt_exec(struct exec *o, struct data **r)
{
	char *filename;

	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	return exec_runfile(o, filename);
}

unsigned
blt_print(struct exec *o, struct data **r)
{
	struct var *arg;

	arg = exec_varlookup(o, "value");
	if (!arg) {
		dbg_puts("blt_print: 'value': no such param\n");
		return 0;
	}
	data_print(arg->data);
	textout_putstr(tout, "\n");
	*r = data_newnil();
	return 1;
}

unsigned
blt_err(struct exec *o, struct data **r)
{
	char *msg;

	if (!exec_lookupstring(o, "message", &msg)) {
		return 0;
	}
	cons_err(msg);
	return 0;
}

unsigned
blt_ev(struct exec *o, struct data **r)
{
	struct evspec es;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}
	usong->curev = es;
	return 1;
}

unsigned
blt_cc(struct exec *o, struct data **r)
{
	struct songchan *t;
	struct var *arg;

	if (!song_try(usong)) {
		return 0;
	}
	arg = exec_varlookup(o, "channame");
	if (!arg) {
		dbg_puts("blt_setc: channame: no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcurchan(usong, NULL);
		return 1;
	}
	if (!exec_lookupchan_getref(o, "channame", &t)) {
		return 0;
	}
	song_setcurchan(usong, t);
	return 1;
}

unsigned
blt_getc(struct exec *o, struct data **r)
{
	struct songchan *cur;

	if (!song_try(usong)) {
		return 0;
	}
	song_getcurchan(usong, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
blt_cx(struct exec *o, struct data **r)
{
	struct songsx *t;
	struct var *arg;

	if (!song_try(usong)) {
		return 0;
	}
	arg = exec_varlookup(o, "sysexname");
	if (!arg) {
		dbg_puts("blt_setx: 'sysexname': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcursx(usong, NULL);
		return 1;
	}
	if (!exec_lookupsx(o, "sysexname", &t)) {
		return 0;
	}
	song_setcursx(usong, t);
	return 1;
}

unsigned
blt_getx(struct exec *o, struct data **r)
{
	struct songsx *cur;

	if (!song_try(usong)) {
		return 0;
	}
	song_getcursx(usong, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
blt_setunit(struct exec *o, struct data **r)
{
	long tpu;
	struct songtrk *t;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "tics_per_unit", &tpu)) {
		return 0;
	}
	if ((tpu % DEFAULT_TPU) != 0 || tpu < DEFAULT_TPU) {
		cons_err("setunit: unit must be multiple of 96 tics");
		return 0;
	}
	SONG_FOREACH_TRK(usong, t) {
		track_scale(&t->track, usong->tics_per_unit, tpu);
	}
	track_scale(&usong->meta, usong->tics_per_unit, tpu);
	usong->curquant = usong->curquant * tpu / usong->tics_per_unit;
	usong->tics_per_unit = tpu;
	return 1;
}

unsigned
blt_getunit(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	*r = data_newlong(usong->tics_per_unit);
	return 1;
}

unsigned
blt_goto(struct exec *o, struct data **r)
{
	long measure;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "measure", &measure)) {
		return 0;
	}
	if (measure < 0) {
		cons_err("measure cant be negative");
		return 0;
	}
	usong->curpos = measure;
	cons_putpos(measure, 0, 0);
	return 1;
}

unsigned
blt_getpos(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	*r = data_newlong(usong->curpos);
	return 1;
}

unsigned
blt_sel(struct exec *o, struct data **r)
{
	long len;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "length", &len)) {
		return 0;
	}
	if (len < 0) {
		cons_err("'measures' parameter cant be negative");
		return 0;
	}
	usong->curlen = len;
	return 1;
}

unsigned
blt_getlen(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	*r = data_newlong(usong->curlen);
	return 1;
}

unsigned
blt_setq(struct exec *o, struct data **r)
{
	long quantum;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "quantum", &quantum)) {
		return 0;
	}
	if (quantum < 0 || (unsigned)quantum > usong->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}
	usong->curquant = quantum;
	return 1;
}

unsigned
blt_getq(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	*r = data_newlong(usong->curquant);
	return 1;
}

unsigned
blt_fac(struct exec *o, struct data **r)
{
	long tpu;

	if (!exec_lookuplong(o, "tempo_factor", &tpu)) {
		return 0;
	}
	if (tpu < 50 || tpu > 200) {
		cons_err("fac: factor must be between 50 and 200");
		return 0;
	}
	usong->tempo_factor = 0x100 * 100 / tpu;
	return 1;
}

unsigned
blt_getfac(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	*r = data_newlong(usong->tempo_factor);
	return 1;
}

unsigned
blt_ci(struct exec *o, struct data **r)
{
	unsigned dev, ch;
	struct data *l;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplist(o, "inputchan", &l)) {
		return 0;
	}
	if (!data_num2chan(l, &dev, &ch)) {
		return 0;
	}
	song_setcurinput(usong, dev, ch);
	return 1;
}

unsigned
blt_geti(struct exec *o, struct data **r)
{
	unsigned dev, ch;

	if (!song_try(usong)) {
		return 0;
	}
	song_getcurinput(usong, &dev, &ch);
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(dev));
	data_listadd(*r, data_newlong(ch));
	return 1;
}

unsigned
blt_ct(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct var *arg;

	if (!song_try(usong)) {
		return 0;
	}
	arg = exec_varlookup(o, "trackname");
	if (!arg) {
		dbg_puts("blt_sett: 'trackname': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcurtrk(usong, NULL);
		return 1;
	}
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	song_setcurtrk(usong, t);
	return 1;
}

unsigned
blt_gett(struct exec *o, struct data **r)
{
	struct songtrk *cur;

	if (!song_try(usong)) {
		return 0;
	}
	song_getcurtrk(usong, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
blt_cf(struct exec *o, struct data **r)
{
	struct songfilt *f;
	struct var *arg;

	if (!song_try(usong)) {
		return 0;
	}
	arg = exec_varlookup(o, "filtname");
	if (!arg) {
		dbg_puts("blt_setf: filtname: no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcurfilt(usong, NULL);
		return 1;
	} else if (arg->data->type == DATA_REF) {
		f = song_filtlookup(usong, arg->data->val.ref);
		if (!f) {
			cons_err("no such filt");
			return 0;
		}
		song_setcurfilt(usong, f);
		return 1;
	}
	return 0;
}

unsigned
blt_getf(struct exec *o, struct data **r)
{
	struct songfilt *cur;

	if (!song_try(usong)) {
		return 0;
	}
	song_getcurfilt(usong, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
blt_mutexxx(struct exec *o, struct data **r, int flag)
{
	struct songtrk *t;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	t->mute = flag;
	return 1;
}

unsigned
blt_mute(struct exec *o, struct data **r)
{
	return blt_mutexxx(o, r, 1);
}

unsigned
blt_unmute(struct exec *o, struct data **r)
{
	return blt_mutexxx(o, r, 1);
}

unsigned
blt_getmute(struct exec *o, struct data **r)
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
blt_ls(struct exec *o, struct data **r)
{
	char map[DEFAULT_MAXNCHANS];
	struct songtrk *t;
	struct songchan *c;
	struct songfilt *f;
	struct songsx *s;
	struct sysex *x;
	unsigned i, count;
	unsigned dev, ch;

	if (!song_try(usong)) {
		return 0;
	}
	/*
	 * print info about channels
	 */
	textout_putstr(tout, "chanlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# chan_name,  {devicenum, midichan}, default_input\n");
	SONG_FOREACH_CHAN(usong, c) {
		textout_indent(tout);
		textout_putstr(tout, c->name.str);
		textout_putstr(tout, "\t");
		textout_putstr(tout, "{");
		textout_putlong(tout, c->dev);
		textout_putstr(tout, " ");
		textout_putlong(tout, c->ch);
		textout_putstr(tout, "}");
		textout_putstr(tout, "\t");
		textout_putstr(tout, "{");
		textout_putlong(tout, c->curinput_dev);
		textout_putstr(tout, " ");
		textout_putlong(tout, c->curinput_ch);
		textout_putstr(tout, "}");
		textout_putstr(tout, "\n");

	}
	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");

	/*
	 * print info about filters
	 */
	textout_putstr(tout, "filtlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# filter_name,  default_channel\n");
	SONG_FOREACH_FILT(usong, f) {
		textout_indent(tout);
		textout_putstr(tout, f->name.str);
		textout_putstr(tout, "\t");
		if (f->curchan != NULL) {
			textout_putstr(tout, f->curchan->name.str);
		} else {
			textout_putstr(tout, "nil");
		}
		textout_putstr(tout, "\n");

	}
	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");

	/*
	 * print info about tracks
	 */
	textout_putstr(tout, "tracklist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# track_name,  default_filter,  used_channels,  flags\n");
	SONG_FOREACH_TRK(usong, t) {
		textout_indent(tout);
		textout_putstr(tout, t->name.str);
		textout_putstr(tout, "\t");
		if (t->curfilt != NULL) {
			textout_putstr(tout, t->curfilt->name.str);
		} else {
			textout_putstr(tout, "nil");
		}
		textout_putstr(tout, "\t{");
		track_chanmap(&t->track, map);
		for (i = 0, count = 0; i < DEFAULT_MAXNCHANS; i++) {
			if (map[i]) {
				if (count) {
					textout_putstr(tout, " ");
				}
				c = song_chanlookup_bynum(usong, i / 16, i % 16);
				if (c) {
					textout_putstr(tout, c->name.str);
				} else {
					textout_putstr(tout, "{");
					textout_putlong(tout, i / 16);
					textout_putstr(tout, " ");
					textout_putlong(tout, i % 16);
					textout_putstr(tout, "}");
				}
				count++;
			}
		}
		textout_putstr(tout, "}");
		if (t->mute) {
			textout_putstr(tout, " mute");
		}
		textout_putstr(tout, "\n");

	}
	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");

	/*
	 * print info about sysex banks
	 */
	textout_putstr(tout, "sysexlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# sysex_name,  number_messages\n");
	SONG_FOREACH_SX(usong, s) {
		textout_indent(tout);
		textout_putstr(tout, s->name.str);
		textout_putstr(tout, "\t");
		i = 0;
		for (x = s->sx.first; x != NULL; x = x->next) {
			i++;
		}
		textout_putlong(tout, i);
		textout_putstr(tout, "\n");

	}
	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");

	/*
	 * print current values
	 */
	textout_putstr(tout, "curchan ");
	song_getcurchan(usong, &c);
	if (c) {
		textout_putstr(tout, c->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");

	textout_putstr(tout, "curfilt ");
	song_getcurfilt(usong, &f);
	if (f) {
		textout_putstr(tout, f->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");

	textout_putstr(tout, "curtrack ");
	song_getcurtrk(usong, &t);
	if (t) {
		textout_putstr(tout, t->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");

	textout_putstr(tout, "cursysex ");
	song_getcursx(usong, &s);
	if (s) {
		textout_putstr(tout, s->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");

	textout_putstr(tout, "curquant ");
	textout_putlong(tout, usong->curquant);
	textout_putstr(tout, "\n");
	textout_putstr(tout, "curev ");
	evspec_output(&usong->curev, tout);
	textout_putstr(tout, "\n");
	textout_putstr(tout, "curpos ");
	textout_putlong(tout, usong->curpos);
	textout_putstr(tout, "\n");
	textout_putstr(tout, "curlen ");
	textout_putlong(tout, usong->curlen);
	textout_putstr(tout, "\n");

	textout_indent(tout);
	textout_putstr(tout, "curinput {");
	song_getcurinput(usong, &dev, &ch);
	textout_putlong(tout, dev);
	textout_putstr(tout, " ");
	textout_putlong(tout, ch);
	textout_putstr(tout, "}\n");
	return 1;
}

unsigned
blt_save(struct exec *o, struct data **r)
{
	char *filename;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	song_save(usong, filename);
	return 1;
}

unsigned
blt_load(struct exec *o, struct data **r)
{
	char *filename;
	unsigned res;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	song_done(usong);
	song_init(usong);
	res = song_load(usong, filename);
	cons_putpos(usong->curpos, 0, 0);
	return res;
}

unsigned
blt_reset(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	song_done(usong);
	song_init(usong);
	cons_putpos(usong->curpos, 0, 0);
	return 1;
}

unsigned
blt_export(struct exec *o, struct data **r)
{
	char *filename;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	return song_exportsmf(usong, filename);
}

unsigned
blt_import(struct exec *o, struct data **r)
{
	char *filename;
	struct song *sng;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	sng = song_importsmf(filename);
	if (sng == NULL) {
		return 0;
	}
	song_delete(usong);
	usong = sng;
	cons_putpos(usong->curpos, 0, 0);
	return 1;
}

unsigned
blt_idle(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	song_idle(usong);
	return 1;
}

unsigned
blt_play(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	song_play(usong);
	return 1;
}

unsigned
blt_rec(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	song_record(usong);
	return 1;
}

unsigned
blt_stop(struct exec *o, struct data **r)
{
	if (!mux_isopen) {
		cons_err("nothing to stop, ignored");
		return 1;
	}
	song_stop(usong);
	cons_putpos(usong->curpos, 0, 0);
	return 1;
}

unsigned
blt_tempo(struct exec *o, struct data **r)
{
	long tempo;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "beats_per_minute", &tempo)) {
		return 0;
	}
	if (tempo < 40 || tempo > 240) {
		cons_err("tempo must be between 40 and 240 beats per measure");
		return 0;
	}
	track_settempo(&usong->meta, usong->curpos, tempo);
	return 1;
}

unsigned
blt_mins(struct exec *o, struct data **r)
{
	long num, den, amount;
	unsigned tic, len, tpm;
	unsigned long usec24;
	struct seqptr sp;
	struct track t1, t2, tn;
	struct ev ev;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "numerator", &num) ||
	    !exec_lookuplong(o, "denominator", &den)) {
		return 0;
	}
	if (den != 1 && den != 2 && den != 4 && den != 8) {
		cons_err("only 1, 2, 4 and 8 are supported as denominator");
		return 0;
	}

	tpm = usong->tics_per_unit * num / den;
	track_timeinfo(&usong->meta, usong->curpos, &tic, &usec24, NULL, NULL);
	len = amount * tpm;

	track_init(&tn);
	seqptr_init(&sp, &tn);
	seqptr_ticput(&sp, tic);
	ev.cmd = EV_TIMESIG;
	ev.timesig_beats = num;
	ev.timesig_tics = usong->tics_per_unit / den;
	seqptr_evput(&sp, &ev);
	ev.cmd = EV_TEMPO;
	ev.tempo_usec24 = usec24;
	seqptr_evput(&sp, &ev);
	seqptr_ticput(&sp, len);
	seqptr_done(&sp);

	track_init(&t1);
	track_init(&t2);
	track_move(&usong->meta, 0,   tic, NULL, &t1, 1, 1);
	track_move(&usong->meta, tic, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, tic + len);
	track_clear(&usong->meta);
	track_merge(&usong->meta, &t1);
	track_merge(&usong->meta, &tn);
	track_merge(&usong->meta, &t2);
	track_done(&t1);
	track_done(&t2);
	track_done(&tn);
	return 1;
}

unsigned
blt_mdel(struct exec *o, struct data **r)
{
	long amount;
	unsigned tic, len;
	struct track t1, t2;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "amount", &amount)) {
		return 0;
	}
	tic = track_findmeasure(&usong->meta, usong->curpos);
	len = track_findmeasure(&usong->meta, usong->curpos + amount) - tic;

	track_init(&t1);
	track_init(&t2);
	track_move(&usong->meta, 0,         tic, NULL, &t1, 1, 1);
	track_move(&usong->meta, tic + len, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, tic);
	track_clear(&usong->meta);
	track_merge(&usong->meta, &t1);
	if (!track_isempty(&t2)) {
		track_merge(&usong->meta, &t2);
	}
	track_done(&t1);
	track_done(&t2);
	return 1;
}

unsigned
blt_minfo(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	track_output(&usong->meta, tout);
	textout_putstr(tout, "\n");
	return 1;
}


unsigned
blt_mtempo(struct exec *o, struct data **r)
{
	long from;
	unsigned tic, bpm, tpb;
	unsigned long usec24;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "from", &from)) {
		return 0;
	}
	track_timeinfo(&usong->meta, from, &tic, &usec24, &bpm, &tpb);
	*r = data_newlong(60L * 24000000L / (usec24 * tpb));
	return 1;
}

unsigned
blt_msig(struct exec *o, struct data **r)
{
	long from;
	unsigned tic, bpm, tpb;
	unsigned long usec24;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "from", &from)) {
		return 0;
	}
	track_timeinfo(&usong->meta, from, &tic, &usec24, &bpm, &tpb);
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(bpm));
	data_listadd(*r, data_newlong(usong->tics_per_unit / tpb));
	return 1;
}


unsigned
blt_ctlconf_any(struct exec *o, struct data **r, int isfine)
{
	char *name;
	unsigned num, old, val;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "name", &name) ||
	    !exec_lookupctl(o, "ctl", &num) ||
	    !exec_lookupval(o, "defval", isfine, &val)) {
		return 0;
	}
	if (evctl_lookup(name, &old)) {
		evctl_unconf(old);
	}
	evctl_unconf(num);
	if (!isfine)
		val <<= 7;
	evctl_conf(num, name, val);
	return 1;
}

unsigned
blt_ctlconf(struct exec *o, struct data **r)
{
	return blt_ctlconf_any(o, r, 0);
}

unsigned
blt_ctlconfx(struct exec *o, struct data **r)
{
	return blt_ctlconf_any(o, r, 1);
}

unsigned
blt_ctlunconf(struct exec *o, struct data **r)
{
	char *name;
	unsigned num;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "name", &name)) {
		return 0;
	}
	if (!evctl_lookup(name, &num)) {
		cons_errs(name, "no such controller");
		return 0;
	}
	evctl_unconf(num);
	return 1;
}


unsigned
blt_ctlinfo(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	evctltab_output(evctl_tab, tout);
	return 1;
}

unsigned
blt_metro(struct exec *o, struct data **r)
{
	char *mstr;
	unsigned mask;

	if (!exec_lookupname(o, "onoff", &mstr)) {
		return 0;
	}
	if (!metro_str2mask(&usong->metro, mstr, &mask)) {
		cons_err("m: metro must be 'on', 'off' or 'rec'");
		return 0;
	}
	metro_setmask(&usong->metro, mask);
	return 1;
}

unsigned
blt_metrocf(struct exec *o, struct data **r)
{
	struct ev evhi, evlo;

	if (!exec_lookupev(o, "eventhi", &evhi) ||
	    !exec_lookupev(o, "eventlo", &evlo)) {
		return 0;
	}
	if (evhi.cmd != EV_NON && evlo.cmd != EV_NON) {
		cons_err("note-on event expected");
		return 0;
	}
	metro_shut(&usong->metro);
	usong->metro.hi = evhi;
	usong->metro.lo = evlo;
	return 1;
}

unsigned
blt_tlist(struct exec *o, struct data **r)
{
	struct data *d, *n;
	struct songtrk *i;

	if (!song_try(usong)) {
		return 0;
	}
	d = data_newlist(NULL);
	SONG_FOREACH_TRK(usong, i) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
blt_tnew(struct exec *o, struct data **r)
{
	char *trkname;
	struct songtrk *t;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "trackname", &trkname)) {
		return 0;
	}
	t = song_trklookup(usong, trkname);
	if (t != NULL) {
		cons_err("tnew: track already exists");
		return 0;
	}
	t = song_trknew(usong, trkname);
	return 1;
}

unsigned
blt_tdel(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	if (usong->curtrk == NULL) {
		cons_err("tdel: no current track");
		return 0;
	}
	song_trkdel(usong, usong->curtrk);
	return 1;
}

unsigned
blt_tren(struct exec *o, struct data **r)
{
	char *name;

	if (!song_try(usong)) {
		return 0;
	}
	if (usong->curtrk == NULL) {
		cons_err("tren: no current track");
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_trklookup(usong, name)) {
		cons_err("tren: name already used by another track");
		return 0;
	}
	str_delete(usong->curtrk->name.str);
	usong->curtrk->name.str = str_new(name);
	return 1;
}

unsigned
blt_texists(struct exec *o, struct data **r)
{
	char *name;
	struct songtrk *t;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "trackname", &name)) {
		return 0;
	}
	t = song_trklookup(usong, name);
	*r = data_newlong(t != NULL ? 1 : 0);
	return 1;
}

unsigned
blt_taddev(struct exec *o, struct data **r)
{
	long measure, beat, tic;
	struct ev ev;
	struct seqptr tp;
	struct songtrk *t;
	unsigned pos, bpm, tpb;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "measure", &measure) ||
	    !exec_lookuplong(o, "beat", &beat) ||
	    !exec_lookuplong(o, "tic", &tic) ||
	    !exec_lookupev(o, "event", &ev)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("taddev: no current track");
		return 0;
	}
	track_timeinfo(&usong->meta, measure, &pos, NULL, &bpm, &tpb);

	if (beat < 0 || (unsigned)beat >= bpm ||
	    tic  < 0 || (unsigned)tic  >= tpb) {
		cons_err("taddev: beat/tick must fit in the measure");
		return 0;
	}
	pos += beat * tpb + tic;
	seqptr_init(&tp, &t->track);
	seqptr_seek(&tp, pos);
	seqptr_evput(&tp, &ev);
	seqptr_done(&tp);
	return 1;
}

unsigned
blt_tsetf(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct songfilt *f;
	struct var *arg;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tsetf: no current track");
		return 0;
	}
	arg = exec_varlookup(o, "filtname");
	if (!arg) {
		dbg_puts("blt_tsetf: filtname: no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		t->curfilt = NULL;
		return 1;
	} else if (arg->data->type == DATA_REF) {
		f = song_filtlookup(usong, arg->data->val.ref);
		if (!f) {
			cons_err("tsetf: no such filt");
			return 0;
		}
		t->curfilt = f;
		return 1;
	}
	return 0;
}

unsigned
blt_tgetf(struct exec *o, struct data **r)
{
	struct songtrk *t;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tgetf: no current track");
		return 0;
	}
	if (t->curfilt) {
		*r = data_newref(t->curfilt->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
blt_tcheck(struct exec *o, struct data **r)
{
	struct songtrk *t;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tgetf: no current track");
		return 0;
	}
	track_check(&t->track);
	return 1;
}

unsigned
blt_tcut(struct exec *o, struct data **r)
{
	struct songtrk *t;
	unsigned qstep, stic, etic;
	struct track t1, t2;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tcut: no current track");
		return 0;
	}
	stic = track_findmeasure(&usong->meta, usong->curpos);
	etic = track_findmeasure(&usong->meta, usong->curpos + usong->curlen);
	qstep = usong->curquant / 2;
	if (stic > qstep) {
		stic -= qstep;
	}	
	if (etic > qstep) {
		etic -= qstep;
	}	
	track_init(&t1);
	track_init(&t2);
	track_move(&t->track, 0,   stic, NULL, &t1, 1, 1);
	track_move(&t->track, etic, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, stic);
	track_clear(&t->track);
	track_merge(&t->track, &t1);
	if (!track_isempty(&t2)) {
		track_merge(&t->track, &t2);
	}
	track_done(&t1);
	track_done(&t2);
	usong->curlen = 0;
	return 1;
}

unsigned
blt_tins(struct exec *o, struct data **r)
{
	struct songtrk *t;
	long amount;
	unsigned stic, etic, qstep;
	struct track t1, t2;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tins: no current track");
		return 0;
	}
	if (!exec_lookuplong(o, "amount", &amount)) {
		return 0;
	}
	stic = track_findmeasure(&usong->meta, usong->curpos);
	etic = track_findmeasure(&usong->meta, usong->curpos + amount);
	qstep = usong->curquant / 2;
	if (stic > qstep) {
		stic -= qstep;
	}	
	if (etic > qstep) {
		etic -= qstep;
	}	
	track_init(&t1);
	track_init(&t2);
	track_move(&t->track, 0 ,  stic, NULL, &t1, 1, 1);
	track_move(&t->track, stic, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, etic);
	track_clear(&t->track);
	track_merge(&t->track, &t1);
	if (!track_isempty(&t2)) {
		track_merge(&t->track, &t2);
	}
	track_done(&t1);
	track_done(&t2);
	usong->curlen += amount;
	return 1;
}

unsigned
blt_tclr(struct exec *o, struct data **r)
{
	struct songtrk *t;
	unsigned stic, etic, qstep;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tins: no current track");
		return 0;
	}
	stic = track_findmeasure(&usong->meta, usong->curpos);
	etic = track_findmeasure(&usong->meta, usong->curpos + usong->curlen);
	qstep = usong->curquant / 2;
	if (stic > qstep) {
		stic -= qstep;
	}	
	if (etic > qstep) {
		etic -= qstep;
	}	
	track_move(&t->track, stic, etic - stic, &usong->curev, NULL, 0, 1);
	return 1;
}

unsigned
blt_tcopy(struct exec *o, struct data **r)
{
	struct songtrk *t, *t2;
	struct track copy;
	long where;
	unsigned stic, etic, stic2, qstep;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tcopy: no current track");
		return 0;
	}
	if (!exec_lookuptrack(o, "trackname2", &t2) ||
	    !exec_lookuplong(o, "where", &where)) {
		return 0;
	}
	stic = track_findmeasure(&usong->meta, usong->curpos);
	etic = track_findmeasure(&usong->meta, usong->curpos + usong->curlen);
	stic2 = track_findmeasure(&usong->meta, where);
	qstep = usong->curquant / 2;
	if (stic > qstep && stic2 > qstep) {
		stic -= qstep;
		stic2 -= qstep;
	}
	if (etic > qstep) {
		etic -= qstep;
	}
	track_init(&copy);
	track_move(&t->track, stic, etic - stic, &usong->curev, &copy, 1, 0);
	if (!track_isempty(&copy)) {
		copy.first->delta += stic2;
		track_merge(&t2->track, &copy);
	}
	track_done(&copy);
	return 1;
}

unsigned
blt_tmerge(struct exec *o, struct data **r)
{
	struct songtrk *src, *dst;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuptrack(o, "source", &src)) {
		return 0;
	}
	if ((dst = usong->curtrk) == NULL) {
		cons_err("tmerge: no current track");
		return 0;
	}
	track_merge(&src->track, &dst->track);
	return 1;
}

unsigned
blt_tquant(struct exec *o, struct data **r)
{
	struct songtrk *t;
	unsigned stic, etic, offset, qstep;
	long rate;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tquant: no current track");
		return 0;
	}
	if (!exec_lookuplong(o, "rate", &rate)) {
		return 0;
	}
	if (rate > 100) {
		cons_err("tquant: rate must be between 0 and 100");
		return 0;
	}
	stic = track_findmeasure(&usong->meta, usong->curpos);
	etic = track_findmeasure(&usong->meta, usong->curpos + usong->curlen);
	qstep = usong->curquant / 2;
	if (stic > qstep) {
		stic -= qstep;
		offset = qstep;
	} else
		offset = 0;
	if (etic > qstep)
		etic -= qstep;
	track_quantize(&t->track, stic, etic - stic, offset, 2 * qstep, rate);
	return 1;
}

unsigned
blt_ttransp(struct exec *o, struct data **r)
{
	struct songtrk *t;
	long halftones;
	unsigned stic, etic, qstep;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("ttransp: no current track");
		return 0;
	}
	if (!exec_lookuplong(o, "halftones", &halftones)) {
		return 0;
	}
	stic = track_findmeasure(&usong->meta, usong->curpos);
	etic = track_findmeasure(&usong->meta, usong->curpos + usong->curlen);
	qstep = usong->curquant / 2;
	if (stic > qstep) {
		stic -= qstep;
	}
	if (etic > qstep) {
		etic -= qstep;
	}
	track_transpose(&t->track, stic, etic - stic, halftones);
	return 1;
}

unsigned
blt_tclist(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct songchan *c;
	struct data *num;
	char map[DEFAULT_MAXNCHANS];
	unsigned i;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tclist: no current track");
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
blt_tinfo(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct seqptr mp, tp;
	struct state *st;
	unsigned len, count, count_next, tpb, bpm;

	if (!song_try(usong)) {
		return 0;
	}
	if ((t = usong->curtrk) == NULL) {
		cons_err("tinfo: no current track");
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
				if (state_inspec(st, &usong->curev)) {
					if (len >= usong->curquant / 2)
						count++;
					else
						count_next++;
				}
	                }
		}
		textout_putlong(tout, count);
		textout_putstr(tout, " ");
		if (len > 0) {
			if (len < usong->curquant / 2) {
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

unsigned
blt_clist(struct exec *o, struct data **r)
{
	struct data *d, *n;
	struct songchan *i;

	if (!song_try(usong)) {
		return 0;
	}
	d = data_newlist(NULL);
	SONG_FOREACH_CHAN(usong, i) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
blt_cexists(struct exec *o, struct data **r)
{
	struct songchan *i;
	unsigned dev, ch;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupchan_getnum(o, "channame", &dev, &ch)) {
		return 0;
	}
	i = song_chanlookup_bynum(usong, dev, ch);
	*r = data_newlong(i != NULL ? 1 : 0);
	return 1;
}

unsigned
blt_cnew(struct exec *o, struct data **r)
{
	char *name;
	struct songchan *i;
	unsigned dev, ch;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "channame", &name) ||
	    !exec_lookupchan_getnum(o, "channum", &dev, &ch)) {
		return 0;
	}
	i = song_chanlookup(usong, name);
	if (i != NULL) {
		cons_err("channew: chan already exists");
		return 0;
	}
	i = song_chanlookup_bynum(usong, dev, ch);
	if (i != NULL) {
		cons_errs(i->name.str, "dev/chan number already used");
		return 0;
	}
	if (dev > EV_MAXDEV || ch > EV_MAXCH) {
		cons_err("channew: dev/chan number out of bounds");
		return 0;
	}
	i = song_channew(usong, name, dev, ch);
	return 1;
}

unsigned
blt_cdel(struct exec *o, struct data **r)
{
	struct songchan *c;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("cren: no current chan");
		return 0;
	}
	song_chandel(usong, c);
	return 1;
}

unsigned
blt_cren(struct exec *o, struct data **r)
{
	struct songchan *c;
	char *name;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("cren: no current chan");
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_chanlookup(usong, name)) {
		cons_err("name already used by another chan");
		return 0;
	}
	str_delete(c->name.str);
	c->name.str = str_new(name);
	return 1;
}

unsigned
blt_cset(struct exec *o, struct data **r)
{
	struct songchan *c, *i;
	unsigned dev, ch;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("cset: no current chan");
		return 0;
	}
	if (!exec_lookupchan_getnum(o, "channum", &dev, &ch)) {
		return 0;
	}
	i = song_chanlookup_bynum(usong, dev, ch);
	if (i != NULL) {
		cons_errs(i->name.str, "dev/chan number already used");
		return 0;
	}
	c->dev = dev;
	c->ch = ch;
	track_setchan(&c->conf, dev, ch);
	return 1;
}

unsigned
blt_cgetc(struct exec *o, struct data **r)
{
	struct songchan *c;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("cgetc: no current chan");
		return 0;
	}
	*r = data_newlong(c->ch);
	return 1;
}

unsigned
blt_cgetd(struct exec *o, struct data **r)
{
	struct songchan *c;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("cgetd: no current chan");
		return 0;
	}
	*r = data_newlong(c->dev);
	return 1;
}

unsigned
blt_caddev(struct exec *o, struct data **r)
{
	struct songchan *c;
	struct ev ev;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("caddev: no current chan");
		return 0;
	}
	if (!exec_lookupev(o, "event", &ev)) {
		return 0;
	}
	if (ev.ch != c->ch || ev.dev != c->dev) {
		cons_err("chanconfev: dev/chan mismatch in event spec");
		return 0;
	}
	track_confev(&c->conf, &ev);
	return 1;
}

unsigned
blt_crmev(struct exec *o, struct data **r)
{
	struct songchan *c;
	struct evspec es;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("crmev: no current chan");
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}
	track_unconfev(&c->conf, &es);
	return 1;
}

unsigned
blt_cinfo(struct exec *o, struct data **r)
{
	struct songchan *c;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("cinfo: no current chan");
		return 0;
	}
	track_output(&c->conf, tout);
	textout_putstr(tout, "\n");
	return 1;
}

unsigned
blt_cseti(struct exec *o, struct data **r)
{
	unsigned dev, ch;
	struct songchan *c;
	struct data *l;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("cseti: no current chan");
		return 0;
	}
	if (!exec_lookuplist(o, "inputchan", &l)) {
		return 0;
	}
	if (!data_num2chan(l, &dev, &ch)) {
		return 0;
	}
	c->curinput_dev = dev;
	c->curinput_ch = ch;
	return 1;
}


unsigned
blt_cgeti(struct exec *o, struct data **r)
{
	struct songchan *c;

	if (!song_try(usong)) {
		return 0;
	}
	if ((c = usong->curchan) == NULL) {
		cons_err("cgeti: no current chan");
		return 0;
	}
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(c->curinput_dev));
	data_listadd(*r, data_newlong(c->curinput_ch));
	return 1;
}

unsigned
blt_flist(struct exec *o, struct data **r)
{
	struct data *d, *n;
	struct songfilt *i;

	if (!song_try(usong)) {
		return 0;
	}
	d = data_newlist(NULL);
	SONG_FOREACH_FILT(usong, i) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
blt_fnew(struct exec *o, struct data **r)
{
	char *name;
	struct songfilt *i;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookupname(o, "filtname", &name)) {
		return 0;
	}
	i = song_filtlookup(usong, name);
	if (i != NULL) {
		cons_err("filtnew: filt already exists");
		return 0;
	}
	i = song_filtnew(usong, name);
	return 1;
}

unsigned
blt_fdel(struct exec *o, struct data **r)
{
	struct songfilt *f;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	song_filtdel(usong, f);
	return 1;
}

unsigned
blt_fren(struct exec *o, struct data **r)
{
	struct songfilt *f;
	char *name;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_filtlookup(usong, name)) {
		cons_err("name already used by another filt");
		return 0;
	}
	str_delete(f->name.str);
	f->name.str = str_new(name);
	return 1;
}

unsigned
blt_fexists(struct exec *o, struct data **r)
{
	char *name;
	struct songfilt *f;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	if (!exec_lookupname(o, "filtname", &name)) {
		return 0;
	}
	f = song_filtlookup(usong, name);
	*r = data_newlong(f != NULL ? 1 : 0);
	return 1;
}

unsigned
blt_finfo(struct exec *o, struct data **r)
{
	struct songfilt *f;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	filt_output(&f->filt, tout);
	textout_putstr(tout, "\n");
	return 1;
}

unsigned
blt_freset(struct exec *o, struct data **r)
{
	struct songfilt *f;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	filt_reset(&f->filt);
	return 1;
}

unsigned
blt_fmap(struct exec *o, struct data **r)
{
	struct songfilt *f;
	struct evspec from, to;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from) ||
	    !exec_lookupevspec(o, "to", &to)) {
		return 0;
	}
	filt_mapnew(&f->filt, &from, &to);
	return 1;
}

unsigned
blt_funmap(struct exec *o, struct data **r)
{
	struct songfilt *f;
	struct evspec from, to;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from) ||
	    !exec_lookupevspec(o, "to", &to)) {
		return 0;
	}
	filt_mapdel(&f->filt, &from, &to);
	return 1;
}

unsigned
blt_fchgxxx(struct exec *o, struct data **r, int input, int swap)
{
	struct songfilt *f;
	struct evspec from, to;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from) ||
	    !exec_lookupevspec(o, "to", &to)) {
		return 0;
	}
	if (evspec_isec(&from, &to)) {
		cons_err("\"from\" and \"to\" event ranges must be disjoint");
	}
	if (input)
		filt_chgin(&f->filt, &from, &to, swap);
	else
		filt_chgout(&f->filt, &from, &to, swap);
	return 1;
}

unsigned
blt_fchgin(struct exec *o, struct data **r)
{
	return blt_fchgxxx(o, r, 1, 0);
}

unsigned
blt_fchgout(struct exec *o, struct data **r)
{
	return blt_fchgxxx(o, r, 0, 0);
}

unsigned
blt_fswapin(struct exec *o, struct data **r)
{
	return blt_fchgxxx(o, r, 1, 1);
}

unsigned
blt_fswapout(struct exec *o, struct data **r)
{
	return blt_fchgxxx(o, r, 0, 1);
}

unsigned
blt_fsetc(struct exec *o, struct data **r)
{
	struct songfilt *f;
	struct songchan *c;
	struct var *arg;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	arg = exec_varlookup(o, "channame");
	if (!arg) {
		dbg_puts("user_func_filtsetcurchan: 'channame': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		f->curchan = NULL;
		return 1;
	} else if (arg->data->type == DATA_REF) {
		c = song_chanlookup(usong, arg->data->val.ref);
		if (!c) {
			cons_err("no such chan");
			return 0;
		}
		f->curchan = c;
		return 1;
	}
	return 0;
}

unsigned
blt_fgetc(struct exec *o, struct data **r)
{
	struct songfilt *f;

	if (!song_try(usong)) {
		return 0;
	}
	if ((f = usong->curfilt) == NULL) {
		cons_err("fdel: no current filt");
		return 0;
	}
	if (f->curchan) {
		*r = data_newref(f->curchan->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}
