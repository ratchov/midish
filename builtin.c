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
#include "defs.h"
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
#include "norm.h"
#include "builtin.h"
#include "version.h"

unsigned
blt_version(struct exec *o, struct data **r)
{
	cons_err(VERSION);
	return 1;
}

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
	extern unsigned filt_debug, mididev_debug, mux_debug, mixout_debug, 
	    norm_debug, pool_debug, song_debug,
	    timo_debug, mem_debug;
	char *flag;
	long value;

	if (!exec_lookupname(o, "flag", &flag) ||
	    !exec_lookuplong(o, "value", &value)) {
		return 0;
	}
	if (str_eq(flag, "filt")) {
		filt_debug = value;
	} else if (str_eq(flag, "mididev")) {
		mididev_debug = value;
	} else if (str_eq(flag, "mixout")) {
		mixout_debug = value;
	} else if (str_eq(flag, "mux")) {
		mux_debug = value;
	} else if (str_eq(flag, "norm")) {
		norm_debug = value;
	} else if (str_eq(flag, "pool")) {
		pool_debug = value;
	} else if (str_eq(flag, "song")) {
		song_debug = value;
	} else if (str_eq(flag, "timo")) {
		timo_debug = value;
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
	if (!song_try_mode(usong, 0)) {
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
	if (usong->mode >= SONG_IDLE) 
		song_setmode(usong, SONG_IDLE);
	song_goto(usong, usong->curpos);
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
	*r = data_newlong(usong->tempo_factor * 100 / 0x100);
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
blt_mute(struct exec *o, struct data **r)
{
	struct songtrk *t;

	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	song_trkmute(usong, t);
	return 1;
}

unsigned
blt_unmute(struct exec *o, struct data **r)
{
	struct songtrk *t;

	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	song_trkunmute(usong, t);
	return 1;
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

	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	song_stop(usong);
	song_save(usong, filename);
	return 1;
}

unsigned
blt_load(struct exec *o, struct data **r)
{
	char *filename;
	unsigned res;

	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	song_stop(usong);
	song_done(usong);
	evsx_reset();
	song_init(usong);
	res = song_load(usong, filename);
	cons_putpos(usong->curpos, 0, 0);
	return res;
}

unsigned
blt_reset(struct exec *o, struct data **r)
{
	song_stop(usong);
	song_done(usong);
	evsx_reset();
	song_init(usong);
	cons_putpos(usong->curpos, 0, 0);
	return 1;
}

unsigned
blt_export(struct exec *o, struct data **r)
{
	char *filename;

	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	song_stop(usong);
	return song_exportsmf(usong, filename);
}

unsigned
blt_import(struct exec *o, struct data **r)
{
	char *filename;
	struct song *sng;

	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	sng = song_importsmf(filename);
	if (sng == NULL) {
		return 0;
	}
	song_stop(usong);
	song_delete(usong);
	usong = sng;
	cons_putpos(usong->curpos, 0, 0);
	return 1;
}

unsigned
blt_idle(struct exec *o, struct data **r)
{
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
	song_play(usong);
	if (user_flag_batch) {
		cons_err("press ^C to stop playback");
		while (mux_mdep_wait() && !usong->complete)
			; /* nothing */
		cons_err("playback stopped");
		song_stop(usong);
	}
	return 1;
}

unsigned
blt_rec(struct exec *o, struct data **r)
{
	song_record(usong);
	if (user_flag_batch) {
		cons_err("press ^C to stop recording");
		while (mux_mdep_wait())
			; /* nothing */
		cons_err("recording stopped");
		song_stop(usong);
	}
	return 1;
}

unsigned
blt_stop(struct exec *o, struct data **r)
{
	song_stop(usong);
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
	unsigned tic, len, qstep;
	unsigned bpm, tpb, obpm, otpb;
	unsigned long usec24;
	struct songtrk *t;
	struct seqptr *sp;
	struct track tn;
	struct ev ev;

	if (!exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplist(o, "sig", &sig)) {
		return 0;
	}
	track_timeinfo(&usong->meta, usong->curpos, &tic, &usec24, &obpm, &otpb);
	if (sig == NULL) {
		bpm = obpm;
		tpb = otpb;
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
	sp = seqptr_new(&tn);
	seqptr_ticput(sp, tic);
	ev.cmd = EV_TIMESIG;
	ev.timesig_beats = bpm;
	ev.timesig_tics = tpb;
	seqptr_evput(sp, &ev);
	if (tic == 0) {
		/* dont remove initial tempo */
		ev.cmd = EV_TEMPO;
		ev.tempo_usec24 = usec24;
		seqptr_evput(sp, &ev);
	}
	seqptr_ticput(sp, len);
	ev.cmd = EV_TIMESIG;
	ev.timesig_beats = obpm;
	ev.timesig_tics = otpb;
	seqptr_evput(sp, &ev);
	seqptr_del(sp);

	track_ins(&usong->meta, tic, len);
	track_merge(&usong->meta, &tn);
	track_done(&tn);

	qstep = usong->curquant / 2;
	if (tic > qstep) {
		tic -= qstep;
	}
	SONG_FOREACH_TRK(usong, t) {
		track_ins(&t->track, tic, len);
	}
	usong->curlen += amount;
	return 1;
}

unsigned
blt_mdup(struct exec *o, struct data **r)
{
	long where;
	unsigned stic, etic, wtic, qstep;
	unsigned sbpm, stpb, wbpm, wtpb;
	unsigned long usec24;
	struct songtrk *t;
	struct seqptr *sp;
	struct track paste, copy;
	struct ev ev;

	if (!exec_lookuplong(o, "where", &where)) {
		return 0;
	}
	if (where >= 0)
		where = usong->curpos + usong->curlen + where;
	else {
		where = usong->curpos - where;
		if (where < 0)
			where = 0;
	}
	if (!song_try_meta(usong)) {
		return 0;
	}

	track_timeinfo(&usong->meta, usong->curpos, 
	    &stic, NULL, &sbpm, &stpb);
	track_timeinfo(&usong->meta, usong->curpos + usong->curlen,
	    &etic, NULL, NULL, NULL);
	track_timeinfo(&usong->meta, where,
	    &wtic, &usec24, &wbpm, &wtpb);

	/*
	 * backup and restore time signature,
	 * possibly the tempo
	 */
	track_init(&paste);
	sp = seqptr_new(&paste);
	seqptr_ticput(sp, wtic);
	ev.cmd = EV_TIMESIG;
	ev.timesig_beats = sbpm;
	ev.timesig_tics = stpb;
	seqptr_evput(sp, &ev);
	if (wtic == 0) {
		/* dont remove initial tempo */
		ev.cmd = EV_TEMPO;
		ev.tempo_usec24 = usec24;
		seqptr_evput(sp, &ev);
	}
	seqptr_ticput(sp, etic - stic);
	ev.cmd = EV_TIMESIG;
	ev.timesig_beats = wbpm;
	ev.timesig_tics = wtpb;
	seqptr_evput(sp, &ev);
	seqptr_del(sp);

	/*
	 * copy and shift the portion to duplicate
	 */
	track_init(&copy);
	track_move(&usong->meta, stic, etic - stic, NULL, &copy, 1, 0);
	track_shift(&copy, wtic);
	track_merge(&paste, &copy);
	track_done(&copy);

	/*
	 * insert space and merge the duplicated portion
	 */
	track_ins(&usong->meta, wtic, etic - stic);
	track_merge(&usong->meta, &paste);
	track_done(&paste);

	qstep = usong->curquant / 2;
	if (stic > qstep && wtic > qstep) {
		stic -= qstep;
		wtic -= qstep;
		etic -= qstep;
	}	
	SONG_FOREACH_TRK(usong, t) {
		track_init(&paste);
		track_move(&t->track, stic, etic - stic, NULL, &paste, 1, 0);
		track_shift(&paste, wtic);
		track_ins(&t->track, wtic, etic - stic);
		track_merge(&t->track, &paste);
		track_done(&paste);
	}
	return 1;
}

unsigned
blt_mcut(struct exec *o, struct data **r)
{
	unsigned bpm, tpb, stic, etic, qstep;
	unsigned long usec24;
	struct songtrk *t;
	struct seqptr *sp;
	struct track paste;
	struct ev ev;
	struct track t1, t2;

	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	track_timeinfo(&usong->meta, usong->curpos, 
	    &stic, NULL, NULL, NULL);
	track_timeinfo(&usong->meta, usong->curpos + usong->curlen,
	    &etic, &usec24, &bpm, &tpb);
	
	track_init(&paste);
	sp = seqptr_new(&paste);
	seqptr_ticput(sp, stic);
	ev.cmd = EV_TIMESIG;
	ev.timesig_beats = bpm;
	ev.timesig_tics = tpb;
	seqptr_evput(sp, &ev);
	if (stic == 0) {
		/* dont remove initial tempo */
		ev.cmd = EV_TEMPO;
		ev.tempo_usec24 = usec24;
		seqptr_evput(sp, &ev);
	}
	seqptr_del(sp);

	track_init(&t1);
	track_init(&t2);
	track_move(&usong->meta, 0,   stic, NULL, &t1, 1, 1);
	track_move(&usong->meta, etic, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, stic);
	track_clear(&usong->meta);
	track_merge(&usong->meta, &t1);
	track_merge(&usong->meta, &paste);
	if (!track_isempty(&t2)) {
		track_merge(&usong->meta, &t2);
	}
	track_done(&t1);
	track_done(&t2);

	qstep = usong->curquant / 2;
	if (stic > qstep) {
		stic -= qstep;
		etic -= qstep;
	}	
	SONG_FOREACH_TRK(usong, t) {
		track_cut(&t->track, stic, etic - stic);
	}
	usong->curlen = 0;
	return 1;
}

unsigned
blt_minfo(struct exec *o, struct data **r)
{
	struct seqptr *mp;
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
	mp = seqptr_new(&usong->meta);
	while (!stop) {
		/*
		 * scan for a time signature change
		 */
		while (seqptr_evget(mp)) {
			/* nothing */
		}
		seqptr_getsign(mp, &bpm, &tpb);
		seqptr_gettempo(mp, &tempo1);

		if (seqptr_skip(mp, tpb * bpm) > 0)
			stop = 1;
		seqptr_gettempo(mp, &tempo2);
		
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
	seqptr_del(mp);

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

	if (!song_try_mode(usong, 0)) {
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

	if (!song_try_mode(usong, 0)) {
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
blt_evsx(struct exec *o, struct data **r)
{
	struct data *byte;
	struct var *arg;
	char *pattern, *name, *ref;
	unsigned spec, size, cmd;

	if (!song_try_mode(usong, 0))
		return 0;
	if (!exec_lookupname(o, "name", &ref))
		return 0;
	if (evsx_lookup(ref, &cmd)) {
		if (!song_try_ev(usong, cmd))
			return 0;
		evsx_unconf(cmd);
	}
	for (cmd = 0; cmd < EV_NUMCMD; cmd++) {
		if (evinfo[cmd].ev && str_eq(evinfo[cmd].ev, ref)) {
			cons_errs(o->procname, "name already in use");
			return 0;
		}
	}
	for (cmd = EV_SX0;; cmd++) {
		if (cmd == EV_SX0 + EVSX_NMAX) {
			cons_errs(o->procname, "too many sysex patterns");
			return 0;
		}
		if (evinfo[cmd].ev == NULL)
			break;
	}
	name = str_new(ref);
	pattern = mem_alloc(EVSX_MAXSIZE, "evsx");
	arg = exec_varlookup(o, "pattern");
	if (!arg) {
		dbg_puts("exec_lookupev: no such var\n");
		dbg_panic();
	}
	if (arg->data->type != DATA_LIST) {
		cons_errs(o->procname, "data must be a list");
		goto err1;
	}
	size = 0;
	for (byte = arg->data->val.list; byte != NULL; byte = byte->next) {
		if (size == EVSX_MAXSIZE) {
			cons_errs(o->procname, "pattern too long");
			goto err1;
		}
		if (byte->type == DATA_LONG) {
			if (byte->val.num < 0 ||
			    byte->val.num > 0xff) {
				cons_errs(o->procname,
				    "out of range byte in sysex pattern");
				goto err1;
			}
			pattern[size++] = byte->val.num;
		} else if (byte->type == DATA_REF) {
			if (str_eq(byte->val.ref, "v0") ||
			    str_eq(byte->val.ref, "v0_hi")) {
				spec = EVSX_V0_HI;
			} else if (str_eq(byte->val.ref, "v0_lo")) {
				spec = EVSX_V0_LO;
			} else if (str_eq(byte->val.ref, "v1") ||
			    str_eq(byte->val.ref, "v1_hi")) {
				spec = EVSX_V1_HI;
			} else if (str_eq(byte->val.ref, "v1_lo")) {
				spec = EVSX_V1_LO;
			} else {
				cons_errs(o->procname,
				    "bad atom in sysex pattern");
				goto err1;
			}
			pattern[size++] = spec;
		} else {	
			cons_errs(o->procname, "bad pattern");
			goto err1;
		}
	}
	if (!evsx_set(cmd, name, pattern, size))
		goto err1;
	return 1;
err1:
	mem_free(pattern);
	str_delete(name);
	return 0;
}

unsigned
blt_evsxinfo(struct exec *o, struct data **r)
{
	evsx_output(tout);
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

	if (!song_try_mode(usong, 0)) {
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
	struct seqptr *tp;
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
	tp = seqptr_new(&t->track);
	seqptr_seek(tp, pos);
	seqptr_evput(tp, &ev);
	seqptr_del(tp);
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
	unsigned tic, len, qstep;
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	tic = track_findmeasure(&usong->meta, usong->curpos);
	len = track_findmeasure(&usong->meta, usong->curpos + usong->curlen) - tic;
	qstep = usong->curquant / 2;
	if (tic > qstep) {
		tic -= qstep;
	}
	track_cut(&t->track, tic, len);
	usong->curlen = 0;
	return 1;
}

unsigned
blt_tins(struct exec *o, struct data **r)
{
	unsigned tic, len, qstep;
	struct songtrk *t;
	long amount;

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
	tic = track_findmeasure(&usong->meta, usong->curpos);
	len = track_findmeasure(&usong->meta, usong->curpos + amount) - tic;
	qstep = usong->curquant / 2;
	if (tic > qstep) {
		tic -= qstep;
	}
	track_ins(&t->track, tic, len);
	usong->curlen += amount;
	return 1;
}

unsigned
blt_tclr(struct exec *o, struct data **r)
{
	struct songtrk *t;
	unsigned tic, len, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	tic = track_findmeasure(&usong->meta, usong->curpos);
	len = track_findmeasure(&usong->meta, usong->curpos + usong->curlen) - tic;
	qstep = usong->curquant / 2;
	if (tic > qstep) {
		tic -= qstep;
	} else if (tic + len > qstep) {
		len -= qstep;
	}	
	track_move(&t->track, tic, len, &usong->curev, NULL, 0, 1);
	return 1;
}

unsigned
blt_tpaste(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct track copy;
	unsigned tic, tic2, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	tic = CLIP_OFFS;
	tic2 = track_findmeasure(&usong->meta, usong->curpos);
	qstep = usong->curquant / 2;
	if (tic > qstep && tic2 > qstep) {
		tic -= qstep;
		tic2 -= qstep;
	}
	track_init(&copy);
	track_move(&usong->clip, tic, ~0U, &usong->curev, &copy, 1, 0);
	if (!track_isempty(&copy)) {
		copy.first->delta += tic2;
		track_merge(&t->track, &copy);
	}
	track_done(&copy);
	return 1;
}

unsigned
blt_tcopy(struct exec *o, struct data **r)
{
	struct songtrk *t;
	unsigned tic, len, tic2, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	tic = track_findmeasure(&usong->meta, usong->curpos);
	len = track_findmeasure(&usong->meta, usong->curpos + usong->curlen) - tic;
	tic2 = CLIP_OFFS;
	qstep = usong->curquant / 2;
	if (tic > qstep && tic2 > qstep) {
		tic -= qstep;
		tic2 -= qstep;
	} else if (tic + len > qstep) {
		len -= qstep;
	}	
	track_clear(&usong->clip);
	track_move(&t->track, tic, len, &usong->curev, &usong->clip, 1, 0);
	track_shift(&usong->clip, tic2);
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
	unsigned tic, len, qstep, offset;
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
	tic = track_findmeasure(&usong->meta, usong->curpos);
	len = track_findmeasure(&usong->meta, usong->curpos + usong->curlen) - tic;
	qstep = usong->curquant / 2;
	if (tic > qstep) {
		tic -= qstep;
		offset = qstep;
	} else {
		offset = 0;
		if (tic + len > qstep)
			len -= qstep;
	}
	track_quantize(&t->track, tic, len, offset, 2 * qstep, rate);
	return 1;
}

unsigned
blt_ttransp(struct exec *o, struct data **r)
{
	struct songtrk *t;
	long halftones;
	unsigned tic, len, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!exec_lookuplong(o, "halftones", &halftones)) {
		return 0;
	}
	if (halftones < -64 || halftones >= 63) {
		cons_errs(o->procname, "argument not in the -64..63 range");
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	tic = track_findmeasure(&usong->meta, usong->curpos);
	len = track_findmeasure(&usong->meta, usong->curpos + usong->curlen) - tic;
	qstep = usong->curquant / 2;
	if (tic > qstep) {
		tic -= qstep;
	} else if (tic + len > qstep) {
		len -= qstep;
	}
	track_transpose(&t->track, tic, len, &usong->curev, halftones);
	return 1;
}

unsigned
blt_tevmap(struct exec *o, struct data **r)
{
	struct songtrk *t;
	struct evspec from, to;
	unsigned tic, len, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from, 0) ||
	    !exec_lookupevspec(o, "to", &to, 0)) {
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	tic = track_findmeasure(&usong->meta, usong->curpos);
	len = track_findmeasure(&usong->meta, usong->curpos + usong->curlen) - tic;
	qstep = usong->curquant / 2;
	if (tic > qstep) {
		tic -= qstep;
	} else if (tic + len > qstep) {
		len -= qstep;
	}
	track_evmap(&t->track, tic, len, &usong->curev, &from, &to);
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
	struct seqptr *mp, *tp;
	struct state *st;
	unsigned len, count, count_next, tpb, bpm, m;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		cons_errs(o->procname, "no current track");
		return 0;
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);
	textout_indent(tout);

	m = 0;
	count_next = 0;
	tp = seqptr_new(&t->track);
	mp = seqptr_new(&usong->meta);
	for (;;) {
		/*
		 * scan for a time signature change
		 */
		while (seqptr_evget(mp)) {
			/* nothing */
		}
		seqptr_getsign(mp, &bpm, &tpb);

		/*
		 * count starting events
		 */
		len = bpm * tpb;
		count = count_next;
		count_next = 0;
		for (;;) {
			len -= seqptr_ticskip(tp, len);
			if (len == 0)
				break;
			st = seqptr_evget(tp);
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
		if (m >= usong->curpos &&
		    m < usong->curpos + usong->curlen) {
			textout_putlong(tout, count);
			textout_putstr(tout, " ");
			if (len > 0) {
				if (len < usong->curquant / 2) {
					textout_putlong(tout, count_next);
					textout_putstr(tout, " ");
				}
			}
		}		
		if (len > 0)
			break;		
		(void)seqptr_skip(mp, bpm * tpb);
		m++;
	}
	seqptr_del(mp);
	seqptr_del(tp);

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
	struct songchan *i, *c;
	struct evspec src, dst;
	unsigned dev, ch;

	if (!exec_lookupname(o, "channame", &name) ||
	    !exec_lookupchan_getnum(o, "channum", &dev, &ch, input)) {
		return 0;
	}
	c = song_chanlookup(usong, name, input);
	if (c != NULL) {
		cons_errss(o->procname, name, "already exists");
		return 0;
	}
	c = song_chanlookup_bynum(usong, dev, ch, input);
	if (c != NULL) {
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
	c = song_channew(usong, name, dev, ch, input);
	if (c->link) {
		evspec_reset(&src);
		evspec_reset(&dst);
		dst.dev_min = dst.dev_max = c->dev;
		dst.ch_min = dst.ch_max = c->ch;
		SONG_FOREACH_IN(usong, i) {
			src.dev_min = src.dev_max = i->dev;
			src.ch_min = src.ch_max = i->ch;
			filt_mapnew(&c->link->filt, &src, &dst);
		}
	}
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
		cons_errss(o->procname, name, "channel name already in use");
		return 0;
	}
	if (c->link && song_filtlookup(usong, name)) {
		cons_errss(o->procname, name, "filt name already in use");
		return 0;
	}
	str_delete(c->name.str);
	c->name.str = str_new(name);
	if (c->link) {
		str_delete(c->link->name.str);
		c->link->name.str = str_new(name);
	}		
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
	struct evspec from, to;
	struct songchan *c, *i;
	struct songfilt *f;
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
	SONG_FOREACH_FILT(usong, f) {
		if (!song_try_filt(usong, f))
			return 0;
	}
	evspec_reset(&from);
	from.dev_min = from.dev_max = c->dev;
	from.ch_min = from.ch_max = c->ch;	
	c->dev = dev;
	c->ch = ch;
	evspec_reset(&to);
	to.dev_min = to.dev_max = c->dev;
	to.ch_min = to.ch_max = c->ch;
	SONG_FOREACH_FILT(usong, f) {
		if (input)
			filt_chgin(&f->filt, &from, &to, 0);
		else
			filt_chgout(&f->filt, &from, &to, 0);
	}
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
	if (!EV_ISVOICE(&ev)) {
		cons_errs(o->procname,
		    "only voice events can be added to channels");
		return 0;
	}
	if (ev.ch != c->ch || ev.dev != c->dev) {
		cons_errs(o->procname,
		    "event device or channel mismatch channel ones");
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

	if (!song_try_mode(usong, 0)) {
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
		cons_errss(o->procname, name, "filt name already in use");
		return 0;
	}
	if (f->link && song_chanlookup(usong, name, 0)) {
		cons_errss(o->procname, name, "chan name already in use");
		return 0;
	}
	str_delete(f->name.str);
	f->name.str = str_new(name);
	if (f->link) {
		str_delete(f->link->name.str);
		f->link->name.str = str_new(name);
	}		
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
	if (mux_isopen)
		norm_shut();
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
	if (mux_isopen)
		norm_shut();
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
	if (mux_isopen)
		norm_shut();
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
	if (plus < -63 || plus > 63) {
		cons_errs(o->procname, "plus must be in the -63..63 range");
		return 0;
	}
	if ((es.cmd != EVSPEC_ANY && es.cmd != EVSPEC_NOTE) ||
	    (es.cmd == EVSPEC_NOTE &&
		(es.v0_min != 0 || es.v0_max != EV_MAXCOARSE))) {
		cons_errs(o->procname, "set must contain full range notes");
		return 0;
	}
	if (mux_isopen)
		norm_shut();
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
	if (mux_isopen)
		norm_shut();
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
	if (mux_isopen)
		norm_shut();
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
blt_ximport(struct exec *o, struct data **r)
{
	struct songsx *c;
	struct var *arg;
	char *path;
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
	arg = exec_varlookup(o, "path");
	if (!arg) {
		dbg_puts("blt_ximport: path: no such param\n");
		return 0;
	}
	if (arg->data->type != DATA_STRING) {
		cons_errs(o->procname, "path must be string");
		return 0;
	}
	path = arg->data->val.str;
	if (!syx_import(path, &c->sx, unit))
		return 0;
	return 1;
}

unsigned
blt_xexport(struct exec *o, struct data **r)
{
	struct songsx *c;
	struct var *arg;
	char *path;

	song_getcursx(usong, &c);
	if (c == NULL) {
		cons_errs(o->procname, "no current sysex");
		return 0;
	}
	arg = exec_varlookup(o, "path");
	if (!arg) {
		dbg_puts("blt_xexport: path: no such param\n");
		return 0;
	}
	if (arg->data->type != DATA_STRING) {
		cons_errs(o->procname, "path must be string");
		return 0;
	}
	path = arg->data->val.str;
	if (!syx_export(path, &c->sx))
		return 0;
	return 1;
}

unsigned
blt_dlist(struct exec *o, struct data **r)
{
	struct data *d, *n;
	struct mididev *i;

	d = data_newlist(NULL);
	for (i = mididev_list; i != NULL; i = i->next) {
		n = data_newlong(i->unit);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
blt_dnew(struct exec *o, struct data **r)
{
	long unit;
	char *path, *modename;
	struct var *arg;
	unsigned mode;

	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookupname(o, "mode", &modename)) {
		return 0;
	}
	arg = exec_varlookup(o, "path");
	if (!arg) {
		dbg_puts("blt_dnew: path: no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		path = NULL;
	} else if (arg->data->type == DATA_STRING) {
		path = arg->data->val.str;
	} else {
		cons_errs(o->procname, "path must be string or nil");
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

	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit)) {
		return 0;
	}
	if (mididev_mtcsrc == mididev_byunit[unit])
		mididev_mtcsrc = NULL;
	if (mididev_clksrc == mididev_byunit[unit])
		mididev_clksrc = NULL;
	return mididev_detach(unit);
}

unsigned
blt_dmtcrx(struct exec *o, struct data **r)
{
	struct var *arg;
	long unit;

	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	arg = exec_varlookup(o, "devnum");
	if (!arg) {
		dbg_puts("blt_dmtcrx: no such var\n");
		dbg_panic();
	}
	if (arg->data->type == DATA_NIL) {
		mididev_mtcsrc = NULL;
		return 0;
	} else if (arg->data->type == DATA_LONG) {
		unit = arg->data->val.num;
		if (unit < 0 || unit >= DEFAULT_MAXNDEVS ||
		    !mididev_byunit[unit]) {
			cons_errs(o->procname, "bad device number");
			return 0;
		}
		mididev_mtcsrc = mididev_byunit[unit];
		mididev_clksrc = NULL;
		return 1;
	}
	cons_errs(o->procname, "bad argument type for 'unit'");
	return 0;
}

unsigned
blt_dmmctx(struct exec *o, struct data **r)
{
	struct data *units, *n;
	unsigned i, tx[DEFAULT_MAXNDEVS];

	if (!song_try_mode(usong, 0)) {
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
			mididev_byunit[i]->sendmmc = tx[i];
	}
	return 1;
}

unsigned
blt_dclkrx(struct exec *o, struct data **r)
{
	struct var *arg;
	long unit;

	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	arg = exec_varlookup(o, "devnum");
	if (!arg) {
		dbg_puts("blt_dclkrx: no such var\n");
		dbg_panic();
	}
	if (arg->data->type == DATA_NIL) {
		mididev_clksrc = NULL;
		return 0;
	} else if (arg->data->type == DATA_LONG) {
		unit = arg->data->val.num;
		if (unit < 0 || unit >= DEFAULT_MAXNDEVS ||
		    !mididev_byunit[unit]) {
			cons_errs(o->procname, "bad device number");
			return 0;
		}
		mididev_clksrc = mididev_byunit[unit];
		mididev_mtcsrc = NULL;
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

	if (!song_try_mode(usong, 0)) {
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
			mididev_byunit[i]->sendclk = tx[i];
	}
	return 1;
}

unsigned
blt_dclkrate(struct exec *o, struct data **r)
{
	long unit, tpu;

	if (!song_try_mode(usong, 0)) {
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
	struct mididev *dev;
	long unit;
	int i, more;

	if (!exec_lookuplong(o, "devnum", &unit)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_errs(o->procname, "bad device number");
		return 0;
	}
	dev = mididev_byunit[unit];
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);

	textout_indent(tout);
	textout_putstr(tout, "devnum ");
	textout_putlong(tout, unit);
	textout_putstr(tout, "\n");

	if (mididev_mtcsrc == dev) {
		textout_indent(tout);
		textout_putstr(tout, "mtcrx\t\t\t# master MTC source\n");
	}
	if (dev->sendmmc) {
		textout_indent(tout);
		textout_putstr(tout, "mmctx\t\t\t# sends MMC messages\n");
	}
	if (mididev_clksrc == dev) {
		textout_indent(tout);
		textout_putstr(tout, "clkrx\t\t\t# master clock source\n");
	}
	if (dev->sendclk) {
		textout_indent(tout);
		textout_putstr(tout, "clktx\t\t\t# sends clock ticks\n");
	}
	textout_indent(tout);
	textout_putstr(tout, "ixctl {");
	for (i = 0, more = 0; i < 32; i++) {
		if (dev->ixctlset & (1 << i)) {
			if (more)
				textout_putstr(tout, " ");
			textout_putlong(tout, i);
			more = 1;
		}
	}
	textout_putstr(tout, "}\n");

	textout_indent(tout);
	textout_putstr(tout, "oxctl {");
	for (i = 0, more = 0; i < 32; i++) {
		if (dev->oxctlset & (1 << i)) {
			if (more)
				textout_putstr(tout, " ");
			textout_putlong(tout, i);
			more = 1;
		}
	}
	textout_putstr(tout, "}\n");

	textout_indent(tout);
	textout_putstr(tout, "iev {");
	for (i = 0, more = 0; i < EV_NUMCMD; i++) {
		if (dev->ievset & (1 << i)) {
			if (more)
				textout_putstr(tout, " ");
			textout_putstr(tout, evinfo[i].ev);
			more = 1;
		}
	}
	textout_putstr(tout, "}\n");

	textout_indent(tout);
	textout_putstr(tout, "oev {");
	for (i = 0, more = 0; i < EV_NUMCMD; i++) {
		if (dev->oevset & (1 << i)) {
			if (more)
				textout_putstr(tout, " ");
			textout_putstr(tout, evinfo[i].ev);
			more = 1;
		}
	}
	textout_putstr(tout, "}\n");

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

	if (!song_try_mode(usong, 0)) {
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
	if (!data_getctlset(list, &ctlset)) {
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

	if (!song_try_mode(usong, 0)) {
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
	if (!data_getctlset(list, &ctlset)) {
		return 0;
	}
	mididev_byunit[unit]->oxctlset = ctlset;
	return 1;
}

unsigned
blt_diev(struct exec *o, struct data **r)
{
	long unit;
	struct data *list;
	unsigned flags;

	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookuplist(o, "flags", &list)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_errs(o->procname, "bad device number");
		return 0;
	}
	if (!data_getxev(list, &flags)) {
		return 0;
	}
	mididev_byunit[unit]->ievset = flags;
	return 1;
}

unsigned
blt_doev(struct exec *o, struct data **r)
{
	long unit;
	struct data *list;
	unsigned flags;

	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookuplist(o, "flags", &list)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS || !mididev_byunit[unit]) {
		cons_errs(o->procname, "bad device number");
		return 0;
	}
	if (!data_getxev(list, &flags)) {
		return 0;
	}
	mididev_byunit[unit]->oevset = flags;
	return 1;
}
