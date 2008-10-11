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
#include "mididev.h"
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
	if (str_eq(flag, "mididev")) {
		mididev_debug = value;
	} else if (str_eq(flag, "filt")) {
		filt_debug = value;
	} else if (str_eq(flag, "song")) {
		song_debug = value;
	} else if (str_eq(flag, "mem")) {
		mem_debug = value;
	} else {
		cons_errs(o->procname, "unknuwn debug-flag");
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
blt_h(struct exec *o, struct data **r)
{
	char *name;
	struct proc *proc;
	struct name *arg;

	if (!exec_lookupname(o, "function", &name)) {
		return 0;
	}
	proc = exec_proclookup(o, name);
	if (proc == NULL) {
		cons_errss(o->procname, name, "no such proc");
		return 0;
	}
	textout_putstr(tout, name);
	for (arg = proc->args; arg != NULL; arg = arg->next) {
		textout_putstr(tout, " ");
		textout_putstr(tout, arg->str);
	}      
	textout_putstr(tout, "\n");
	return 1;
}

unsigned
blt_ev(struct exec *o, struct data **r)
{
	struct evspec es;

	if (!exec_lookupevspec(o, "evspec", &es, 0)) {
		return 0;
	}
	if (!song_try_curev(usong)) {
		return 0;
	}
	usong->curev = es;
	return 1;
}

unsigned
blt_cc(struct exec *o, struct data **r, int input)
{
	struct songchan *t;
	struct var *arg;

	arg = exec_varlookup(o, "channame");
	if (!arg) {
		dbg_puts("blt_cc: channame: no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcurchan(usong, NULL, input);
		return 1;
	}
	if (!exec_lookupchan_getref(o, "channame", &t, input)) {
		return 0;
	}
	if (!song_try_curchan(usong, input)) {
		return 0;
	}
	song_setcurchan(usong, t, input);
	return 1;
}

unsigned
blt_ci(struct exec *o, struct data **r)
{
	return blt_cc(o, r, 1);
}

unsigned
blt_co(struct exec *o, struct data **r)
{
	return blt_cc(o, r, 0);
}

unsigned
blt_getc(struct exec *o, struct data **r, int input)
{
	struct songchan *cur;

	song_getcurchan(usong, &cur, input);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
blt_geti(struct exec *o, struct data **r)
{
	return blt_getc(o, r, 1);
}

unsigned
blt_geto(struct exec *o, struct data **r)
{
	return blt_getc(o, r, 0);
}

unsigned
blt_cx(struct exec *o, struct data **r)
{
	struct songsx *t;
	struct var *arg;

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
	if (!song_try_cursx(usong)) {
		return 0;
	}
	song_setcursx(usong, t);
	return 1;
}

unsigned
blt_getx(struct exec *o, struct data **r)
{
	struct songsx *cur;

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

	if (!exec_lookuplong(o, "tics_per_unit", &tpu)) {
		return 0;
	}
	if ((tpu % DEFAULT_TPU) != 0 || tpu < DEFAULT_TPU) {
		cons_errs(o->procname, "tpu must be multiple of 96 tics");
		return 0;
	}
	if (tpu > TPU_MAX) {
		cons_errs(o->procname, "tpu too large");
		return 0;
	}
	if (!song_try(usong)) {
		return 0;
	}
	song_setunit(usong, tpu);
	return 1;
}

unsigned
blt_getunit(struct exec *o, struct data **r)
{
	*r = data_newlong(usong->tics_per_unit);
	return 1;
}

unsigned
blt_goto(struct exec *o, struct data **r)
{
	long measure;

	if (!exec_lookuplong(o, "measure", &measure)) {
		return 0;
	}
	if (measure < 0) {
		cons_errs(o->procname, "measure cant be negative");
		return 0;
	}
	if (!song_try_curpos(usong)) {
		return 0;
	}
	usong->curpos = measure;
	cons_putpos(measure, 0, 0);
	return 1;
}

unsigned
blt_getpos(struct exec *o, struct data **r)
{
	*r = data_newlong(usong->curpos);
	return 1;
}

unsigned
blt_sel(struct exec *o, struct data **r)
{
	long len;

	if (!exec_lookuplong(o, "length", &len)) {
		return 0;
	}
	if (len < 0) {
		cons_errs(o->procname, "'measures' parameter cant be negative");
		return 0;
	}
	if (!song_try_curlen(usong)) {
		return 0;
	}
	usong->curlen = len;
	return 1;
}

unsigned
blt_getlen(struct exec *o, struct data **r)
{
	*r = data_newlong(usong->curlen);
	return 1;
}

unsigned
blt_setq(struct exec *o, struct data **r)
{
	long step;
	struct var *arg;

	arg = exec_varlookup(o, "step");
	if (!arg) {
		dbg_puts("blt_setq: step: no such param\n");
		return 0;
	}
	if (!song_try_curquant(usong)) {
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		usong->curquant = 0;
		return 1;
	} else if (arg->data->type == DATA_LONG) {
		step = arg->data->val.num;
		if (step < 1 || step > 96) {
			cons_errs(o->procname,
			    "the step must be in the 1..96 range");
			return 0;
		}
		if (usong->tics_per_unit % step != 0) {
			cons_errs(o->procname,
			    "the step must divide the unit note");
			return 0;
		}
		usong->curquant = usong->tics_per_unit / step;
		return 1;
	}
	cons_errs(o->procname, "the step must nil or integer");
	return 0;
}

unsigned
blt_getq(struct exec *o, struct data **r)
{
	if (usong->curquant > 0) 
		*r = data_newlong(usong->tics_per_unit / usong->curquant);
	else 
		*r = data_newnil();
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
		cons_errs(o->procname, "factor must be between 50 and 200");
		return 0;
	}
	usong->tempo_factor = 0x100 * 100 / tpu;
	if (mux_isopen)
		mux_chgtempo(usong->tempo_factor * usong->tempo  / 0x100);
	return 1;
}

unsigned
blt_getfac(struct exec *o, struct data **r)
{
	*r = data_newlong(usong->tempo_factor);
	return 1;
}


unsigned
blt_ct(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct var *arg;

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
	song_setcurfilt(usong, t->curfilt);
	return 1;
}

unsigned
blt_gett(struct exec *o, struct data **r)
{
	struct songtrk *cur;

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

	arg = exec_varlookup(o, "filtname");
	if (!arg) {
		dbg_puts("blt_setf: filtname: no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		f = NULL;
	} else if (arg->data->type == DATA_REF) {
		f = song_filtlookup(usong, arg->data->val.ref);
		if (!f) {
			cons_errss(o->procname, arg->data->val.ref,
			    "no such filt");
			return 0;
		}
	} else {
		cons_errs(o->procname, "bad filter name");
		return 0;
	}
	song_setcurtrk(usong, NULL);
	song_setcurfilt(usong, f);
	return 1;
}

unsigned
blt_getf(struct exec *o, struct data **r)
{
	struct songfilt *cur;

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

	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	if (!song_try_trk(usong, t)) {
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
	return blt_mutexxx(o, r, 0);
}

unsigned
blt_getmute(struct exec *o, struct data **r)
{
	struct songtrk *t;

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

	/*
	 * print info about channels
	 */
	textout_putstr(tout, "outlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# chan_name,  {devicenum, midichan}\n");
	SONG_FOREACH_OUT(usong, c) {
		textout_indent(tout);
		textout_putstr(tout, c->name.str);
		textout_putstr(tout, "\t");
		textout_putstr(tout, "{");
		textout_putlong(tout, c->dev);
		textout_putstr(tout, " ");
		textout_putlong(tout, c->ch);
		textout_putstr(tout, "}");
		textout_putstr(tout, "\n");

	}
	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");

	textout_putstr(tout, "inlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# chan_name,  {devicenum, midichan}\n");
	SONG_FOREACH_IN(usong, c) {
		textout_indent(tout);
		textout_putstr(tout, c->name.str);
		textout_putstr(tout, "\t");
		textout_putstr(tout, "{");
		textout_putlong(tout, c->dev);
		textout_putstr(tout, " ");
		textout_putlong(tout, c->ch);
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
	textout_putstr(tout, "# filter_name\n");
	SONG_FOREACH_FILT(usong, f) {
		textout_indent(tout);
		textout_putstr(tout, f->name.str);
		textout_putstr(tout, "\n");
		/*
		 * XXX: add info about input and output sets
		 */
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
				dev = i / 16;
				ch = i % 16;
				c = song_chanlookup_bynum(usong, dev, ch, 0);
				if (c) {
					textout_putstr(tout, c->name.str);
				} else {
					textout_putstr(tout, "{");
					textout_putlong(tout, dev);
					textout_putstr(tout, " ");
					textout_putlong(tout, ch);
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
	textout_putstr(tout, "curout ");
	song_getcurchan(usong, &c, 0);
	if (c) {
		textout_putstr(tout, c->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");

	textout_putstr(tout, "curin ");
	song_getcurchan(usong, &c, 1);
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
	if (usong->curquant > 0) {
		textout_putlong(tout, usong->tics_per_unit / usong->curquant);
	} else {
		textout_putstr(tout, "nil");
	}
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
	if (user_flag_batch) {
		cons_err("press ^C to stop idling");
		while (mux_mdep_wait())
			; /* nothing */
		cons_err("idling stopped");
		song_stop(usong);
	}
	return 1;
}

unsigned
blt_play(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	song_play(usong);
	if (user_flag_batch) {
		cons_err("press ^C to stop playback");
		while (mux_mdep_wait() && !usong->complete)
			; /* nothing */
		cons_err("playback stopped");
		song_stop(usong);
		cons_putpos(usong->curpos, 0, 0);
	}
	return 1;
}

unsigned
blt_rec(struct exec *o, struct data **r)
{
	if (!song_try(usong)) {
		return 0;
	}
	song_record(usong);
	if (user_flag_batch) {
		cons_err("press ^C to stop recording");
		while (mux_mdep_wait())
			; /* nothing */
		cons_err("recording stopped");
		song_stop(usong);
		cons_putpos(usong->curpos, 0, 0);
	}
	return 1;
}

unsigned
blt_stop(struct exec *o, struct data **r)
{
	if (!mux_isopen) {
		cons_errs(o->procname, "nothing to stop, ignored");
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

	if (!exec_lookuplong(o, "beats_per_minute", &tempo)) {
		return 0;
	}
	if (tempo < 40 || tempo > 240) {
		cons_errs(o->procname, 
		    "tempo must be between 40 and 240 beats per measure");
		return 0;
	}
	if (!song_try_meta(usong)) {
		return 0;
	}
	track_settempo(&usong->meta, usong->curpos, tempo);
	return 1;
}

unsigned
blt_mins(struct exec *o, struct data **r)
{
	long amount;
	struct data *sig;
	unsigned tic, len, bpm, tpb;
	unsigned long usec24;
	struct seqptr sp;
	struct track t1, t2, tn;
	struct ev ev;

	/* XXX: get a {num denom} syntax */

	if (!exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplist(o, "sig", &sig)) {
		return 0;
	}
	track_timeinfo(&usong->meta, usong->curpos, &tic, &usec24, &bpm, &tpb);
	if (sig == NULL) {
		/* nothing */
	} else if (sig->type == DATA_LONG && sig->next != NULL &&
	    sig->next->type == DATA_LONG && sig->next->next == NULL) {
		bpm = sig->val.num;
		sig = sig->next;
		if (sig->val.num != 1 && sig->val.num != 2 && 
		    sig->val.num != 4 && sig->val.num != 8) {
			cons_errs(o->procname, 
			    "denominator must be 1, 2, 4 or 8");
			return 0;
		}
		tpb = usong->tics_per_unit / sig->val.num;
	} else {
		cons_errs(o->procname, 
		    "signature must be {num denom} or {} list");
		return 0;
	}
	if (!song_try_meta(usong)) {
		return 0;
	}
	len = amount * bpm * tpb;

	track_init(&tn);
	seqptr_init(&sp, &tn);
	seqptr_ticput(&sp, tic);
	ev.cmd = EV_TIMESIG;
	ev.timesig_beats = bpm;
	ev.timesig_tics = tpb;
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
blt_mcut(struct exec *o, struct data **r)
{
	long amount;
	unsigned tic, len;
	struct track t1, t2;

	if (!exec_lookuplong(o, "amount", &amount)) {
		return 0;
	}
	if (!song_try(usong)) {
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
	struct seqptr mp;
	unsigned meas, tpb, otpb, bpm, obpm;
	unsigned long tempo1, otempo1, tempo2, otempo2;
	int stop = 0;

	textout_putstr(tout, "{\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# meas\tsig\ttempo\n");

	otpb = 0;
	obpm = 0;
	otempo1 = otempo2 = 0;
	meas = 0;
	seqptr_init(&mp, &usong->meta);
	while (!stop) {
		/*
		 * scan for a time signature change
		 */
		while (seqptr_evget(&mp)) {
			/* nothing */
		}
		seqptr_getsign(&mp, &bpm, &tpb);
		seqptr_gettempo(&mp, &tempo1);

		if (seqptr_skip(&mp, tpb * bpm) > 0)
			stop = 1;
		seqptr_gettempo(&mp, &tempo2);
		
		if (tpb != otpb || bpm != obpm || 
		    tempo1 != otempo1 || tempo2 != otempo2) {
			otpb = tpb;
			obpm = bpm;
			otempo1 = tempo1;
			otempo2 = tempo2;

			textout_indent(tout);
			textout_putlong(tout, meas);
			textout_putstr(tout, "\t{");
			textout_putlong(tout, bpm);
			textout_putstr(tout, " ");
			textout_putlong(tout, usong->tics_per_unit / tpb);
			textout_putstr(tout, "}\t");
			textout_putlong(tout, 
			    60L * 24000000L / (tempo1 * tpb));
			if (tempo2 != tempo1) {
				textout_putstr(tout, "\t# - ");
				textout_putlong(tout, 
				    60L * 24000000L / (tempo2 * tpb));
			}
			textout_putstr(tout, "\n");
		}
		meas++;
	}
	seqptr_done(&mp);

	textout_shiftleft(tout);
	textout_indent(tout);
	textout_putstr(tout, "}\n");
	return 1;
}


unsigned
blt_mtempo(struct exec *o, struct data **r)
{
	unsigned tic, bpm, tpb;
	unsigned long usec24;

	track_timeinfo(&usong->meta, usong->curpos, &tic, &usec24, &bpm, &tpb);
	*r = data_newlong(60L * 24000000L / (usec24 * tpb));
	return 1;
}

unsigned
blt_msig(struct exec *o, struct data **r)
{
	unsigned tic, bpm, tpb;
	unsigned long usec24;

	track_timeinfo(&usong->meta, usong->curpos, &tic, &usec24, &bpm, &tpb);
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(bpm));
	data_listadd(*r, data_newlong(usong->tics_per_unit / tpb));
	return 1;
}

unsigned
blt_mend(struct exec *o, struct data **r)
{
	*r = data_newlong(song_endpos(usong));
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
		cons_errss(o->procname, name, "no such controller");
		return 0;
	}
	evctl_unconf(num);
	return 1;
}


unsigned
blt_ctlinfo(struct exec *o, struct data **r)
{
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
		cons_errs(o->procname,
		    "mode must be 'on', 'off' or 'rec'");
		return 0;
	}
	metro_setmask(&usong->metro, mask);
	return 1;
}

unsigned
blt_metrocf(struct exec *o, struct data **r)
{
	struct ev evhi, evlo;

	if (!exec_lookupev(o, "eventhi", &evhi, 0) ||
	    !exec_lookupev(o, "eventlo", &evlo, 0)) {
		return 0;
	}
	if (evhi.cmd != EV_NON && evlo.cmd != EV_NON) {
		cons_errs(o->procname, "note-on event expected");
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
		cons_errs(o->procname, "track already exists");
		return 0;
	}
	t = song_trknew(usong, trkname);
	return 1;
}

unsigned
blt_tdel(struct exec *o, struct data **r)
{
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	song_trkdel(usong, t);
	return 1;
}

unsigned
blt_tren(struct exec *o, struct data **r)
{
	char *name;
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_trklookup(usong, name)) {
		cons_errs(o->procname,
		    "name already used by another track");
		return 0;
	}
	str_delete(t->name.str);
	t->name.str = str_new(name);
	return 1;
}

unsigned
blt_texists(struct exec *o, struct data **r)
{
	char *name;
	struct songtrk *t;

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

	if (!exec_lookuplong(o, "measure", &measure) ||
	    !exec_lookuplong(o, "beat", &beat) ||
	    !exec_lookuplong(o, "tic", &tic) ||
	    !exec_lookupev(o, "event", &ev, 0)) {
		return 0;
	}
	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	track_timeinfo(&usong->meta, measure, &pos, NULL, &bpm, &tpb);

	if (beat < 0 || (unsigned)beat >= bpm ||
	    tic  < 0 || (unsigned)tic  >= tpb) {
		cons_errs(o->procname, "beat/tick must fit in the measure");
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	arg = exec_varlookup(o, "filtname");
	if (!arg) {
		dbg_puts("blt_tsetf: filtname: no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		f = NULL;
	} else if (arg->data->type == DATA_REF) {
		f = song_filtlookup(usong, arg->data->val.ref);
		if (!f) {
			cons_errs(o->procname, "no such filt");
			return 0;
		}
	} else {
		cons_errs(o->procname, "bad filt name");
		return 0;
	}
	t->curfilt = f;
	song_setcurfilt(usong, f);
	return 0;
}

unsigned
blt_tgetf(struct exec *o, struct data **r)
{
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!exec_lookuplong(o, "amount", &amount)) {
		return 0;
	}
	if (!song_try_trk(usong, t)) {
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
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
blt_tpaste(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct track copy;
	unsigned stic, stic2, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	stic = CLIP_OFFS;
	stic2 = track_findmeasure(&usong->meta, usong->curpos);
	qstep = usong->curquant / 2;
	if (stic > qstep && stic2 > qstep) {
		stic -= qstep;
		stic2 -= qstep;
	}
	track_init(&copy);
	track_move(&usong->clip, stic, ~0U, &usong->curev, &copy, 1, 0);
	if (!track_isempty(&copy)) {
		copy.first->delta += stic2;
		track_merge(&t->track, &copy);
	}
	track_done(&copy);
	return 1;
}

unsigned
blt_tcopy(struct exec *o, struct data **r)
{
	struct songtrk *t;
	unsigned stic, etic, stic2, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	stic = track_findmeasure(&usong->meta, usong->curpos);
	etic = track_findmeasure(&usong->meta, usong->curpos + usong->curlen);
	stic2 = CLIP_OFFS;
	qstep = usong->curquant / 2;
	if (stic > qstep && stic2 > qstep) {
		stic -= qstep;
		stic2 -= qstep;
	}
	if (etic > qstep) {
		etic -= qstep;
	}
	track_clear(&usong->clip);
	track_move(&t->track, stic, etic - stic,
	    &usong->curev, &usong->clip, 1, 0);
	track_shift(&usong->clip, stic2);
	return 1;
}

unsigned
blt_tmerge(struct exec *o, struct data **r)
{
	struct songtrk *src, *dst;

	if (!exec_lookuptrack(o, "source", &src)) {
		return 0;
	}
	song_getcurtrk(usong, &dst);
	if (dst == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, dst)) {
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!exec_lookuplong(o, "rate", &rate)) {
		return 0;
	}
	if (rate > 100) {
		cons_errs(o->procname, "rate must be between 0 and 100");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!exec_lookuplong(o, "halftones", &halftones)) {
		return 0;
	}
	if (!song_try_trk(usong, t)) {
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
	track_transpose(&t->track, stic, etic - stic, &usong->curev, halftones);
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	*r = data_newlist(NULL);
	track_chanmap(&t->track, map);
	for (i = 0; i < DEFAULT_MAXNCHANS; i++) {
		if (map[i]) {
			c = song_chanlookup_bynum(usong, i / 16, i % 16, 0);
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

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
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
blt_clist(struct exec *o, struct data **r, int input)
{
	struct data *d, *n;
	struct songchan *i;

	d = data_newlist(NULL);
	SONG_FOREACH_CHAN(usong, i, input ? usong->inlist : usong->outlist) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
blt_ilist(struct exec *o, struct data **r)
{
	return blt_clist(o, r, 1);
}

unsigned
blt_olist(struct exec *o, struct data **r)
{
	return blt_clist(o, r, 0);
}

unsigned
blt_cexists(struct exec *o, struct data **r, int input)
{
	struct songchan *c;
	char *name;

	if (!exec_lookupname(o, "channame", &name)) {
		return 0;
	}
	c = song_chanlookup(usong, name, input);
	*r = data_newlong(c != NULL ? 1 : 0);
	return 1;
}

unsigned
blt_iexists(struct exec *o, struct data **r)
{
	return blt_cexists(o, r, 1);
}

unsigned
blt_oexists(struct exec *o, struct data **r)
{
	return blt_cexists(o, r, 0);
}

unsigned
blt_cnew(struct exec *o, struct data **r, int input)
{
	char *name;
	struct songchan *i;
	unsigned dev, ch;

	if (!exec_lookupname(o, "channame", &name) ||
	    !exec_lookupchan_getnum(o, "channum", &dev, &ch, input)) {
		return 0;
	}
	i = song_chanlookup(usong, name, input);
	if (i != NULL) {
		cons_errss(o->procname, name, "already exists");
		return 0;
	}
	i = song_chanlookup_bynum(usong, dev, ch, input);
	if (i != NULL) {
		cons_errs(o->procname, "dev/chan number already in use");
		return 0;
	}
	if (dev > EV_MAXDEV || ch > EV_MAXCH) {
		cons_errs(o->procname, "dev/chan number out of bounds");
		return 0;
	}
	if (!song_try_curchan(usong, input)) {
		return 0;
	}
	i = song_channew(usong, name, dev, ch, input);
	return 1;
}

unsigned
blt_inew(struct exec *o, struct data **r)
{
	return blt_cnew(o, r, 1);
}

unsigned
blt_onew(struct exec *o, struct data **r)
{
	return blt_cnew(o, r, 0);
}

unsigned
blt_cdel(struct exec *o, struct data **r, int input)
{
	struct songchan *c;

	song_getcurchan(usong, &c, input);
	if (c == NULL) {
		cons_errs(o->procname, "no current chan");
		return 0;
	}
	if (!song_try_chan(usong, c, input)) {
		return 0;
	}
	song_chandel(usong, c, input);
	return 1;
}

unsigned
blt_idel(struct exec *o, struct data **r)
{
	return blt_cdel(o, r, 1);
}

unsigned
blt_odel(struct exec *o, struct data **r)
{
	return blt_cdel(o, r, 0);
}

unsigned
blt_cren(struct exec *o, struct data **r, int input)
{
	struct songchan *c;
	char *name;

	song_getcurchan(usong, &c, input);
	if (c == NULL) {
		cons_errs(o->procname, "no current chan");
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_chanlookup(usong, name, input)) {
		cons_errss(o->procname, name, "name already in use");
		return 0;
	}
	str_delete(c->name.str);
	c->name.str = str_new(name);
	return 1;
}

unsigned
blt_iren(struct exec *o, struct data **r)
{
	return blt_cren(o, r, 1);
}

unsigned
blt_oren(struct exec *o, struct data **r)
{
	return blt_cren(o, r, 0);
}

unsigned
blt_cset(struct exec *o, struct data **r, int input)
{
	struct songchan *c, *i;
	unsigned dev, ch;

	song_getcurchan(usong, &c, input);
	if (c == NULL) {
		cons_errs(o->procname, "no current chan");
		return 0;
	}
	if (!exec_lookupchan_getnum(o, "channum", &dev, &ch, input)) {
		return 0;
	}
	i = song_chanlookup_bynum(usong, dev, ch, input);
	if (i != NULL) {
		cons_errs(o->procname, "dev/chan number already used");
		return 0;
	}
	if (!song_try_chan(usong, c, input)) {
		return 0;
	}
	c->dev = dev;
	c->ch = ch;
	track_setchan(&c->conf, dev, ch);
	return 1;
}

unsigned
blt_iset(struct exec *o, struct data **r)
{
	return blt_cset(o, r, 1);
}

unsigned
blt_oset(struct exec *o, struct data **r)
{
	return blt_cset(o, r, 0);
}

unsigned
blt_cgetc(struct exec *o, struct data **r, int input)
{
	struct songchan *c;

	song_getcurchan(usong, &c, input);
	if (c == NULL) {
		cons_errs(o->procname, "no current chan");
		return 0;
	}
	*r = data_newlong(c->ch);
	return 1;
}

unsigned
blt_igetc(struct exec *o, struct data **r)
{
	return blt_cgetc(o, r, 1);
}

unsigned
blt_ogetc(struct exec *o, struct data **r)
{
	return blt_cgetc(o, r, 0);
}

unsigned
blt_cgetd(struct exec *o, struct data **r, int input)
{
	struct songchan *c;

	song_getcurchan(usong, &c, input);
	if (c == NULL) {
		cons_errs(o->procname, "no current chan");
		return 0;
	}
	*r = data_newlong(c->dev);
	return 1;
}

unsigned
blt_igetd(struct exec *o, struct data **r)
{
	return blt_cgetd(o, r, 1);
}

unsigned
blt_ogetd(struct exec *o, struct data **r)
{
	return blt_cgetd(o, r, 0);
}

unsigned
blt_caddev(struct exec *o, struct data **r, int input)
{
	struct songchan *c;
	struct ev ev;

	song_getcurchan(usong, &c, input);
	if (c == NULL) {
		cons_errs(o->procname, "no current chan");
		return 0;
	}
	if (!exec_lookupev(o, "event", &ev, input)) {
		return 0;
	}
	if (ev.ch != c->ch || ev.dev != c->dev) {
		cons_errs(o->procname, "dev/chan mismatch in event spec");
		return 0;
	}
	if (ev_phase(&ev) != (EV_PHASE_FIRST | EV_PHASE_LAST)) {
		cons_errs(o->procname, "event must be stateless");
		return 0;
	}
	song_confev(usong, c, input, &ev);
	return 1;
}

unsigned
blt_iaddev(struct exec *o, struct data **r)
{
	return blt_caddev(o, r, 1);
}

unsigned
blt_oaddev(struct exec *o, struct data **r)
{
	return blt_caddev(o, r, 0);
}

unsigned
blt_crmev(struct exec *o, struct data **r, int input)
{
	struct songchan *c;
	struct evspec es;

	if (!song_try(usong)) {
		return 0;
	}
	song_getcurchan(usong, &c, input);
	if (c == NULL) {
		cons_errs(o->procname, "no current chan");
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es, input)) {
		return 0;
	}
	song_unconfev(usong, c, input, &es);
	return 1;
}

unsigned
blt_irmev(struct exec *o, struct data **r)
{
	return blt_crmev(o, r, 1);
}

unsigned
blt_ormev(struct exec *o, struct data **r)
{
	return blt_crmev(o, r, 0);
}

unsigned
blt_cinfo(struct exec *o, struct data **r, int input)
{
	struct songchan *c;

	song_getcurchan(usong, &c, input);
	if (c == NULL) {
		cons_errs(o->procname, "no current chan");
		return 0;
	}
	track_output(&c->conf, tout);
	textout_putstr(tout, "\n");
	return 1;
}

unsigned
blt_iinfo(struct exec *o, struct data **r)
{
	return blt_cinfo(o, r, 1);
}

unsigned
blt_oinfo(struct exec *o, struct data **r)
{
	return blt_cinfo(o, r, 0);
}

unsigned
blt_flist(struct exec *o, struct data **r)
{
	struct data *d, *n;
	struct songfilt *i;

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

	if (!exec_lookupname(o, "filtname", &name)) {
		return 0;
	}
	i = song_filtlookup(usong, name);
	if (i != NULL) {
		cons_errs(o->procname, "filt already exists");
		return 0;
	}
	i = song_filtnew(usong, name);
	return 1;
}

unsigned
blt_fdel(struct exec *o, struct data **r)
{
	struct songfilt *f;

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
		return 0;
	}
	if (!song_try_filt(usong, f)) {
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

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_filtlookup(usong, name)) {
		cons_errss(o->procname, name, "already in use");
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

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
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

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
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

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
		return 0;
	}
	if (!song_try_filt(usong, f)) {
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

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from, 1) ||
	    !exec_lookupevspec(o, "to", &to, 0)) {
		return 0;
	}
	if (!song_try_filt(usong, f)) {
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

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from, 1) ||
	    !exec_lookupevspec(o, "to", &to, 0)) {
		return 0;
	}
	if (!song_try_filt(usong, f)) {
		return 0;
	}
	filt_mapdel(&f->filt, &from, &to);
	return 1;
}

unsigned
blt_ftransp(struct exec *o, struct data **r)
{
	struct songfilt *f;
	struct evspec es;
	long plus;

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es, 0) ||
	    !exec_lookuplong(o, "plus", &plus)) {
		return 0;
	}
	if (plus < -64 || plus > 63) {
		cons_errs(o->procname, "plus must be in the -64..63 range");
		return 0;
	}
	if ((es.cmd != EVSPEC_ANY && es.cmd != EVSPEC_NOTE) ||
	    (es.cmd == EVSPEC_NOTE &&
		(es.v0_min != 0 || es.v0_max != EV_MAXCOARSE))) {
		cons_errs(o->procname, "set must contain full range notes");
		return 0;
	}
	if (!song_try_filt(usong, f)) {
		return 0;
	}
	filt_transp(&f->filt, &es, plus);
	return 1;
}

unsigned
blt_fvcurve(struct exec *o, struct data **r)
{
	struct songfilt *f;
	struct evspec es;
	long weight;

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es, 0) ||
	    !exec_lookuplong(o, "weight", &weight)) {
		return 0;
	}
	if (weight < -63 || weight > 63) {
		cons_errs(o->procname, "plus must be in the -63..63 range");
		return 0;
	}
	if (es.cmd != EVSPEC_ANY && es.cmd != EVSPEC_NOTE) {
		cons_errs(o->procname, "set must contain notes");
		return 0;
	}
	if (!song_try_filt(usong, f)) {
		return 0;
	}
	filt_vcurve(&f->filt, &es, weight);
	return 1;
}

unsigned
blt_fchgxxx(struct exec *o, struct data **r, int input, int swap)
{
	struct songfilt *f;
	struct evspec from, to;

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		cons_errs(o->procname, "no current filt");
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from, input) ||
	    !exec_lookupevspec(o, "to", &to, input)) {
		return 0;
	}
	if (evspec_isec(&from, &to)) {
		cons_errs(o->procname, 
		    "\"from\" and \"to\" event ranges must be disjoint");
	}
	if (!song_try_filt(usong, f)) {
		return 0;
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
blt_xlist(struct exec *o, struct data **r)
{
	struct data *d, *n;
	struct songsx *i;

	d = data_newlist(NULL);
	SONG_FOREACH_SX(usong, i) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
blt_xexists(struct exec *o, struct data **r)
{
	char *name;
	struct songsx *c;

	if (!exec_lookupname(o, "sysexname", &name)) {
		return 0;
	}
	c = song_sxlookup(usong, name);
	*r = data_newlong(c != NULL ? 1 : 0);
	return 1;
}

unsigned
blt_xnew(struct exec *o, struct data **r)
{
	char *name;
	struct songsx *c;

	if (!exec_lookupname(o, "sysexname", &name)) {
		return 0;
	}
	c = song_sxlookup(usong, name);
	if (c != NULL) {
		cons_errs(o->procname, "sysex already exists");
		return 0;
	}
	if (!song_try_cursx(usong)) {
		return 0;
	}
	c = song_sxnew(usong, name);
	return 1;
}

unsigned
blt_xdel(struct exec *o, struct data **r)
{
	struct songsx *c;

	song_getcursx(usong, &c);
	if (c == NULL) {
		cons_errs(o->procname, "no current sysex");
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	song_sxdel(usong, c);
	return 1;
}

unsigned
blt_xren(struct exec *o, struct data **r)
{
	struct songsx *c;
	char *name;

	song_getcursx(usong, &c);
	if (c == NULL) {
		cons_errs(o->procname, "no current sysex");
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_sxlookup(usong, name)) {
		cons_errss(o->procname, name, "already in use");
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	str_delete(c->name.str);
	c->name.str = str_new(name);
	return 1;
}

unsigned
blt_xinfo(struct exec *o, struct data **r)
{
	struct songsx *c;
	struct sysex *e;
	unsigned i;

	song_getcursx(usong, &c);
	if (c == NULL) {
		cons_errs(o->procname, "no current sysex");
		return 0;
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);

	for (e = c->sx.first; e != NULL; e = e->next) {
		textout_indent(tout);
		textout_putlong(tout, e->unit);
		textout_putstr(tout, " { ");
		if (e->first) {
			for (i = 0; i < e->first->used; i++) {
				if (i > 10) {
					textout_putstr(tout, "... ");
					break;
				}
				textout_putbyte(tout, e->first->data[i]);
				textout_putstr(tout, " ");
			}
		}
		textout_putstr(tout, "}\n");
	}
	textout_shiftleft(tout);
	textout_indent(tout);
	textout_putstr(tout, "}\n");
	return 1;
}


unsigned
blt_xrm(struct exec *o, struct data **r)
{
	struct songsx *c;
	struct sysex *x, **px;
	struct data *d;
	unsigned match;

	song_getcursx(usong, &c);
	if (c == NULL) {
		cons_errs(o->procname, "no current sysex");
		return 0;
	}
	if (!exec_lookuplist(o, "data", &d)) {
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	px = &c->sx.first;
	for (;;) {
		if (!*px) {
			break;
		}
		if (!data_matchsysex(d, *px, &match)) {
			return 0;
		}
		if (match) {
			x = *px;
			*px = x->next;
			if (*px == NULL) {
				c->sx.lastptr = px;
			}
			sysex_del(x);
		} else {
			px = &(*px)->next;
		}
	}
	return 1;
}

unsigned
blt_xsetd(struct exec *o, struct data **r)
{
	struct songsx *c;
	struct sysex *x;
	struct data *d;
	unsigned match;
	long unit;

	song_getcursx(usong, &c);
	if (c == NULL) {
		cons_errs(o->procname, "no current sysex");
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookuplist(o, "data", &d)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS) {
		cons_errs(o->procname, "devnum out of range");
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	for (x = c->sx.first; x != NULL; x = x->next) {
		if (!x) {
			break;
		}
		if (!data_matchsysex(d, x, &match)) {
			return 0;
		}
		if (match) {
			x->unit = unit;
		}
	}
	return 1;
}

unsigned
blt_xadd(struct exec *o, struct data **r)
{
	struct songsx *c;
	struct sysex *x;
	struct data *byte;
	struct var *arg;
	long unit;

	song_getcursx(usong, &c);
	if (c == NULL) {
		cons_errs(o->procname, "no current sysex");
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS) {
		cons_errs(o->procname, "devnum out of range");
		return 0;
	}
	arg = exec_varlookup(o, "data");
	if (!arg) {
		dbg_puts("exec_lookupev: no such var\n");
		dbg_panic();
	}
	if (arg->data->type != DATA_LIST) {
		cons_errs(o->procname, "data must be a list of numbers");
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	x = sysex_new(unit);
	for (byte = arg->data->val.list; byte != 0; byte = byte->next) {
		if (byte->type != DATA_LONG) {
			cons_errs(o->procname, "only bytes allowed as data");
			sysex_del(x);
			return 0;
		}
		if (byte->val.num < 0 || byte->val.num > 0xff) {
			cons_errs(o->procname, "data out of range");
			sysex_del(x);
			return 0;
		}
		sysex_add(x, byte->val.num);
	}
	if (!sysex_check(x)) {
		cons_errs(o->procname, "bad sysex format");
		sysex_del(x);
		return 0;
	}
	if (x->first) {
		sysexlist_put(&c->sx, x);
	} else {
		sysex_del(x);
	}
	return 1;
}

unsigned
blt_dnew(struct exec *o, struct data **r)
{
	long unit;
	char *path, *modename;
	unsigned mode;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookupstring(o, "path", &path) ||
	    !exec_lookupname(o, "mode", &modename)) {
		return 0;
	}
	if (str_eq(modename, "ro")) {
		mode = MIDIDEV_MODE_IN;
	} else if (str_eq(modename, "wo")) {
		mode = MIDIDEV_MODE_OUT;
	} else if (str_eq(modename, "rw")) {
		mode = MIDIDEV_MODE_IN | MIDIDEV_MODE_OUT;
	} else {
		cons_errss(o->procname, modename, 
		    "bad mode (allowed: ro, wo, rw)");
		return 0;
	}
	return mididev_attach(unit, path, mode);
}

unsigned
blt_ddel(struct exec *o, struct data **r)
{
	long unit;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit)) {
		return 0;
	}
	return mididev_detach(unit);
}

unsigned
blt_dclkrx(struct exec *o, struct data **r)
{
	struct var *arg;
	long unit;

	if (!song_try(usong)) {
		return 0;
	}
	arg = exec_varlookup(o, "devnum");
	if (!arg) {
		dbg_puts("blt_dclkrx: no such var\n");
		dbg_panic();
	}
	if (arg->data->type == DATA_NIL) {
		mididev_master = NULL;
		return 0;
	} else if (arg->data->type == DATA_LONG) {
		unit = arg->data->val.num;
		if (unit < 0 || unit >= DEFAULT_MAXNDEVS ||
		    !mididev_byunit[unit]) {
			cons_errs(o->procname, "bad device number");
			return 0;
		}
		mididev_master = mididev_byunit[unit];
		return 1;
	}
	cons_errs(o->procname, "bad argument type for 'unit'");
	return 0;
}

unsigned
blt_dclktx(struct exec *o, struct data **r)
{
	struct data *units, *n;
	unsigned i, tx[DEFAULT_MAXNDEVS];

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplist(o, "devlist", &units)) {
		return 0;
	}
	for (i = 0; i < DEFAULT_MAXNDEVS; i++)
		tx[i] = 0;
	for (n = units; n != NULL; n = n->next) {
		if (n->type != DATA_LONG ||
		    n->val.num < 0 || n->val.num >= DEFAULT_MAXNDEVS ||
		    !mididev_byunit[n->val.num]) {
			cons_errs(o->procname, "bad device number");
			return 0;
		}
		tx[n->val.num] = 1;
	}
	for (i = 0; i < DEFAULT_MAXNDEVS; i++) {
		if (mididev_byunit[i])
			mididev_byunit[i]->sendrt = tx[i];
	}
	return 1;
}

unsigned
blt_dclkrate(struct exec *o, struct data **r)
{
	long unit, tpu;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookuplong(o, "tics_per_unit", &tpu)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_errs(o->procname, "bad device number");
		return 0;
	}
	if (tpu < DEFAULT_TPU || (tpu % DEFAULT_TPU)) {
		cons_errs(o->procname, "device tpu must be multiple of 96");
		return 0;
	}
	mididev_byunit[unit]->ticrate = tpu;
	return 1;
}

unsigned
blt_dinfo(struct exec *o, struct data **r)
{
	long unit;

	if (!exec_lookuplong(o, "devnum", &unit)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_errs(o->procname, "bad device number");
		return 0;
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);

	textout_indent(tout);
	textout_putstr(tout, "devnum ");
	textout_putlong(tout, unit);
	textout_putstr(tout, "\n");

	if (mididev_master == mididev_byunit[unit]) {
		textout_indent(tout);
		textout_putstr(tout, "clkrx\t\t\t# master clock source\n");
	}
	if (mididev_byunit[unit]->sendrt) {
		textout_indent(tout);
		textout_putstr(tout, "clktx\t\t\t# sends clock ticks\n");
	}

	textout_indent(tout);
	textout_putstr(tout, "clkrate ");
	textout_putlong(tout, mididev_byunit[unit]->ticrate);
	textout_putstr(tout, "\n");

	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");
	return 1;
}

unsigned
blt_dixctl(struct exec *o, struct data **r)
{
	long unit;
	struct data *list;
	unsigned ctlset;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookuplist(o, "ctlset", &list)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_errs(o->procname, "bad device number");
		return 0;
	}
	if (!data_list2ctlset(list, &ctlset)) {
		return 0;
	}
	mididev_byunit[unit]->ixctlset = ctlset;
	return 1;
}

unsigned
blt_doxctl(struct exec *o, struct data **r)
{
	long unit;
	struct data *list;
	unsigned ctlset;

	if (!song_try(usong)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookuplist(o, "ctlset", &list)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_errs(o->procname, "bad device number");
		return 0;
	}
	if (!data_list2ctlset(list, &ctlset)) {
		return 0;
	}
	mididev_byunit[unit]->oxctlset = ctlset;
	return 1;
}
