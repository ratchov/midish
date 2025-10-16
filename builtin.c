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
#include <stdio.h>
#include "utils.h"
#include "defs.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"
#include "frame.h"
#include "help.h"
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
#include "undo.h"

unsigned
blt_info(struct exec *o, struct data **r)
{
	exec_dumpprocs(o);
	exec_dumpvars(o);
	return 1;
}

unsigned
blt_shut(struct exec *o, struct data **r)
{
	unsigned i;
	struct ev ev;
	struct mididev *dev;

	/*
	 * XXX: should raise mode to SONG_IDLE and
	 * use mixout
	 */
	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	mux_open();
	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		for (i = 0; i < EV_MAXCH; i++) {
			ev.cmd = EV_XCTL;
			ev.dev = dev->unit;
			ev.ch = i;
			ev.ctl_num = 121;
			ev.ctl_val = 0;
			mux_putev(&ev);
			ev.cmd = EV_XCTL;
			ev.dev = dev->unit;
			ev.ch = i;
			ev.ctl_num = 123;
			ev.ctl_val = 0;
			mux_putev(&ev);
			ev.cmd = EV_BEND;
			ev.dev = dev->unit;
			ev.ch = i;
			ev.bend_val = EV_BEND_DEFAULT;
			mux_putev(&ev);
		}
	}
	mux_close();
	return 1;
}

unsigned
blt_proclist(struct exec *o, struct data **r)
{
	struct proc *i;
	struct data *d, *n;

	d = data_newlist(NULL);
	PROC_FOREACH(i, o->procs) {
		if (i->code->vmt == &node_vmt_slist) {
			n = data_newref(i->name.str);
			data_listadd(d, n);
		}
	}
	*r = d;
	return 1;
}

unsigned
blt_builtinlist(struct exec *o, struct data **r)
{
	struct proc *i;
	struct data *d, *n;

	d = data_newlist(NULL);
	PROC_FOREACH(i, o->procs) {
		if (i->code->vmt == &node_vmt_builtin) {
			n = data_newref(i->name.str);
			data_listadd(d, n);
		}
	}
	*r = d;
	return 1;
}

unsigned
blt_version(struct exec *o, struct data **r)
{
	logx(1, "%s", VERSION);
	return 1;
}

unsigned
blt_panic(struct exec *o, struct data **r)
{
	panic();
	/* not reached */
	return 0;
}

unsigned
blt_debug(struct exec *o, struct data **r)
{
	extern unsigned filt_debug, mididev_debug, mux_debug, mixout_debug,
	    norm_debug, pool_debug, song_debug,
	    timo_debug;
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
	} else {
		logx(1, "%s: unknuwn debug-flag", o->procname);
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
	struct data *d;

	arg = exec_varlookup(o, "...");
	if (!arg) {
		logx(1, "%s: no variable argument list", __func__);
		return 0;
	}

	for (d = arg->data->val.list; d != NULL; d = d->next)
		data_print(d);
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
	logx(1, "%s", msg);
	return 0;
}

unsigned
blt_h(struct exec *o, struct data **r)
{
	char *name;
	struct data *d;
	struct help *h;
	struct var *arg;

	arg = exec_varlookup(o, "...");
	if (!arg) {
		logx(1, "%s: no variable argument list", __func__);
		return 0;
	}

	d = arg->data->val.list;
	if (d == NULL) {
		name = "intro";
	} else if (d->next == NULL) {
		if (d->type != DATA_REF) {
			logx(1, "%s: keyword expected", o->procname);
			return 0;
		}
		name = d->val.str;
	} else {
		logx(1, "%s: only one keyword expected", o->procname);
		return 0;
	}

	h = help_list;
	while (1) {
		if (h->key == NULL) {
			logx(1, "%s: %s: no help available", o->procname, name);
			return 0;
		}
		if (str_eq(h->key, name))
			break;
		h++;
	}

	help_fmt(h->text);
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
		logx(1, "%s: channame: no such param", __func__);
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
		logx(1, "%s: 'sysexname': no such param", __func__);
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
	struct songtrk *t;

	if (!exec_lookuplong(o, "tics_per_unit", &tpu)) {
		return 0;
	}
	if ((tpu % DEFAULT_TPU) != 0 || tpu < DEFAULT_TPU) {
		logx(1, "%s: tpu must be multiple of 96 tics", o->procname);
		return 0;
	}
	if (tpu > TPU_MAX) {
		logx(1, "%s: tpu too large", o->procname);
		return 0;
	}
	if (!song_try_mode(usong, 0)) {
		return 0;
	}

	undo_track_save(usong, &usong->meta, o->procname, NULL);
	track_prescale(&usong->meta, usong->tics_per_unit, tpu);
	undo_track_diff(usong);

	SONG_FOREACH_TRK(usong, t) {
		undo_track_save(usong, &t->track, NULL, NULL);
		track_prescale(&t->track, usong->tics_per_unit, tpu);
		undo_track_diff(usong);
	}

	undo_scale(usong, NULL, NULL, usong->tics_per_unit, tpu);

	undo_setuint(usong, NULL, NULL,
	    &usong->curquant, usong->curquant * tpu / usong->tics_per_unit);
	undo_setuint(usong, NULL, NULL,
	    &usong->tics_per_unit, tpu);

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
		logx(1, "%s: measure cant be negative", o->procname);
		return 0;
	}
	if (!song_try_curpos(usong)) {
		return 0;
	}
	usong->curpos = measure;
	if (usong->mode >= SONG_IDLE)
		song_setmode(usong, SONG_IDLE);
	song_goto(usong, usong->curpos);
	mux_flush();
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
		logx(1, "%s: 'measures' parameter cant be negative", o->procname);
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
blt_loop(struct exec *o, struct data **r)
{
	if (!song_try_mode(usong, SONG_IDLE)) {
		return 0;
	}
	usong->loop = 1;
	return 1;
}

unsigned
blt_noloop(struct exec *o, struct data **r)
{
	if (!song_try_mode(usong, SONG_IDLE)) {
		return 0;
	}
	usong->loop = 0;
	return 1;
}

unsigned
blt_setq(struct exec *o, struct data **r)
{
	long step;
	struct var *arg;

	arg = exec_varlookup(o, "step");
	if (!arg) {
		logx(1, "%s: step: no such param", __func__);
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
			logx(1, "%s: the step must be in the 1..96 range", o->procname);
			return 0;
		}
		if (usong->tics_per_unit % step != 0) {
			logx(1, "%s: the step must divide the unit note", o->procname);
			return 0;
		}
		usong->curquant = usong->tics_per_unit / step;
		return 1;
	}
	logx(1, "%s: the step must nil or integer", o->procname);
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
		logx(1, "%s: factor must be between 50 and 200", o->procname);
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
		logx(1, "%s: 'trackname': no such param", __func__);
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
		logx(1, "%s: filtname: no such param", __func__);
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		f = NULL;
	} else if (arg->data->type == DATA_REF) {
		f = song_filtlookup(usong, arg->data->val.ref);
		if (!f) {
			logx(1, "%s: %s: no such filt", o->procname, arg->data->val.ref);
			return 0;
		}
	} else {
		logx(1, "%s: bad filter name", o->procname);
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
	textout_putstr(tout, "# chan_name,  {devicenum, midichan}\n");
	SONG_FOREACH_CHAN(usong, c) {
		if (c->isinput)
			continue;
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
	textout_putstr(tout, "# chan_name,  {devicenum, midichan}\n");
	SONG_FOREACH_CHAN(usong, c) {
		if (!c->isinput)
			continue;
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
	textout_putstr(tout, "# filter_name\n");
	SONG_FOREACH_FILT(usong, f) {
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
	textout_putstr(tout, "# track_name,  default_filter,  used_channels,  flags\n");
	SONG_FOREACH_TRK(usong, t) {
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
	textout_putstr(tout, "# sysex_name,  number_messages\n");
	SONG_FOREACH_SX(usong, s) {
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
	textout_putstr(tout, "tap ");
	textout_putstr(tout, song_tap_modestr[usong->tap_mode]);
	textout_putstr(tout, "\n");
	textout_putstr(tout, "tapev ");
	evspec_output(&usong->tap_evspec, tout);
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
	struct song *newsong;
	char *filename;
	unsigned res;

	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	song_stop(usong);
	newsong = song_new();
	res = song_load(newsong, filename);
	if (res) {
		song_delete(usong);
		usong = newsong;
		cons_putpos(usong->curpos, 0, 0);
	} else
		song_delete(newsong);
	return res;
}

unsigned
blt_reset(struct exec *o, struct data **r)
{
	song_stop(usong);
	song_done(usong);
	evpat_reset();
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
	song_stop(usong);
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
	song_idle(usong);
	if (user_flag_batch) {
		logx(1, "press ^C to stop idling");
		while (mux_mdep_wait(0))
			; /* nothing */
		logx(1, "idling stopped");
		song_stop(usong);
	}
	return 1;
}

unsigned
blt_play(struct exec *o, struct data **r)
{
	if (usong->tap_mode && (mididev_clksrc || mididev_mtcsrc)) {
		logx(1, "%s: can't start in tap mode with external clock", o->procname);
		return 0;
	}
	song_play(usong);
	if (user_flag_batch) {
		logx(1, "press ^C to stop playback");
		while (!usong->complete && mux_mdep_wait(0))
			; /* nothing */
		logx(1, "playback stopped");
		song_stop(usong);
	}
	return 1;
}

unsigned
blt_rec(struct exec *o, struct data **r)
{
	if (usong->tap_mode && (mididev_clksrc || mididev_mtcsrc)) {
		logx(1, "%s: can't start in tap mode with external clock", o->procname);
		return 0;
	}
	song_record(usong);
	if (user_flag_batch) {
		logx(1, "press ^C to stop recording");
		while (mux_mdep_wait(0))
			; /* nothing */
		logx(1, "recording stopped");
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
		logx(1, "%s: tempo must be between 40 and 240 beats per measure", o->procname);
		return 0;
	}
	if (!song_try_meta(usong)) {
		return 0;
	}
	undo_track_save(usong, &usong->meta, o->procname, NULL);
	track_settempo(&usong->meta, usong->curpos, tempo);
	undo_track_diff(usong);
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
			logx(1, "%s: denominator must be 1, 2, 4 or 8", o->procname);
			return 0;
		}
		tpb = usong->tics_per_unit / sig->val.num;
	} else {
		logx(1, "%s: signature must be {num denom} or {} list", o->procname);
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

	undo_track_save(usong, &usong->meta, o->procname, NULL);
	track_ins(&usong->meta, tic, len);
	track_merge(&usong->meta, &tn);
	undo_track_diff(usong);
	track_done(&tn);

	qstep = usong->curquant / 2;
	if (tic > qstep) {
		tic -= qstep;
	}
	SONG_FOREACH_TRK(usong, t) {
		undo_track_save(usong, &t->track, NULL, NULL);
		track_ins(&t->track, tic, len);
		undo_track_diff(usong);
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
	undo_track_save(usong, &usong->meta, o->procname, NULL);
	track_ins(&usong->meta, wtic, etic - stic);
	track_merge(&usong->meta, &paste);
	undo_track_diff(usong);
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
		undo_track_save(usong, &t->track, NULL, NULL);
		track_ins(&t->track, wtic, etic - stic);
		track_merge(&t->track, &paste);
		undo_track_diff(usong);
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
	undo_track_save(usong, &usong->meta, o->procname, NULL);
	track_move(&usong->meta, 0,   stic, NULL, &t1, 1, 1);
	track_move(&usong->meta, etic, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, stic);
	track_clear(&usong->meta);
	track_merge(&usong->meta, &t1);
	track_merge(&usong->meta, &paste);
	if (!track_isempty(&t2)) {
		track_merge(&usong->meta, &t2);
	}
	undo_track_diff(usong);
	track_done(&t1);
	track_done(&t2);

	qstep = usong->curquant / 2;
	if (stic > qstep) {
		stic -= qstep;
		etic -= qstep;
	}
	SONG_FOREACH_TRK(usong, t) {
		undo_track_save(usong, &t->track, NULL, NULL);
		track_cut(&t->track, stic, etic - stic);
		undo_track_diff(usong);
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
	if (!isfine && val != EV_UNDEF)
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
		logx(1, "%s: %s: no such controller", o->procname, name);
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
blt_evpat(struct exec *o, struct data **r)
{
	struct data *byte;
	struct var *arg;
	char *name, *ref;
	unsigned char *pattern;
	unsigned spec, size, cmd;

	if (!song_try_mode(usong, 0))
		return 0;
	if (!exec_lookupname(o, "name", &ref))
		return 0;
	if (evpat_lookup(ref, &cmd)) {
		if (!song_try_ev(usong, cmd))
			return 0;
		evpat_unconf(cmd);
	}
	for (cmd = 0; cmd < EV_NUMCMD; cmd++) {
		if (evinfo[cmd].ev && str_eq(evinfo[cmd].ev, ref)) {
			logx(1, "%s: name already in use", o->procname);
			return 0;
		}
	}
	for (cmd = EV_PAT0;; cmd++) {
		if (cmd == EV_PAT0 + EV_NPAT) {
			logx(1, "%s: too many sysex patterns", o->procname);
			return 0;
		}
		if (evinfo[cmd].ev == NULL)
			break;
	}
	name = str_new(ref);
	pattern = xmalloc(EV_PATSIZE, "evpat");
	arg = exec_varlookup(o, "pattern");
	if (!arg) {
		logx(1, "%s: no such var", __func__);
		panic();
	}
	if (arg->data->type != DATA_LIST) {
		logx(1, "%s: data must be a list", o->procname);
		goto err1;
	}
	size = 0;
	for (byte = arg->data->val.list; byte != NULL; byte = byte->next) {
		if (size == EV_PATSIZE) {
			logx(1, "%s: pattern too long", o->procname);
			goto err1;
		}
		if (byte->type == DATA_LONG) {
			if (byte->val.num < 0 ||
			    byte->val.num > 0xff) {
				logx(1, "%s: out of range byte in sysex pattern", o->procname);
				goto err1;
			}
			pattern[size++] = byte->val.num;
		} else if (byte->type == DATA_REF) {
			if (str_eq(byte->val.ref, "v0") ||
			    str_eq(byte->val.ref, "v0_hi")) {
				spec = EV_PATV0_HI;
			} else if (str_eq(byte->val.ref, "v0_lo")) {
				spec = EV_PATV0_LO;
			} else if (str_eq(byte->val.ref, "v1") ||
			    str_eq(byte->val.ref, "v1_hi")) {
				spec = EV_PATV1_HI;
			} else if (str_eq(byte->val.ref, "v1_lo")) {
				spec = EV_PATV1_LO;
			} else {
				logx(1, "%s: bad atom in sysex pattern", o->procname);
				goto err1;
			}
			pattern[size++] = spec;
		} else {
			logx(1, "%s: bad pattern", o->procname);
			goto err1;
		}
	}
	if (!evpat_set(cmd, name, pattern, size))
		goto err1;
	return 1;
err1:
	xfree(pattern);
	str_delete(name);
	return 0;
}

unsigned
blt_evinfo(struct exec *o, struct data **r)
{
	evpat_output(tout);
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
		logx(1, "%s: mode must be 'on', 'off' or 'rec'", o->procname);
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
		logx(1, "%s: note-on event expected", o->procname);
		return 0;
	}
	metro_shut(&usong->metro);
	usong->metro.hi = evhi;
	usong->metro.lo = evlo;
	return 1;
}

unsigned
blt_tap(struct exec *o, struct data **r)
{
	char *mstr;
	int mode;

	if (!exec_lookupname(o, "mode", &mstr)) {
		return 0;
	}
	if (str_eq(mstr, "off")) {
		mode = 0;
	} else if (str_eq(mstr, "start")) {
		mode = SONG_TAP_START;
	} else if (str_eq(mstr, "tempo")) {
		mode = SONG_TAP_TEMPO;
	} else {
		logx(1, "%s: mode must be 'off', 'start' or 'tempo'", o->procname);
		return 0;
	}
	usong->tap_mode = mode;
	return 1;
}

unsigned
blt_tapev(struct exec *o, struct data **r)
{
	struct evspec es;

	if (!exec_lookupevspec(o, "evspec", &es, 0)) {
		return 0;
	}
	usong->tap_evspec = es;
	return 1;
}

unsigned
blt_undo(struct exec *o, struct data **r)
{
	if (!song_try_mode(usong, 0))
		return 0;
	undo_pop(usong);
	return 1;
}

unsigned
blt_undolist(struct exec *o, struct data **r)
{
	struct undo *u;

	for (u = usong->undo; u != NULL; u = u->next) {
		if (u->func == NULL)
			continue;
		textout_putstr(tout, u->func);
		if (u->name != NULL) {
			textout_putstr(tout, " ");
			textout_putstr(tout, u->name);
			if (u->type == UNDO_EMPTY && (u->next == NULL ||
				u->next->name != NULL))
				textout_putstr(tout, " (no-op)");
		}
		textout_putstr(tout, "\n");
	}
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
	char *tren;
	struct songtrk *t;

	if (!song_try_mode(usong, 0)) {
		return 0;
	}
	if (!exec_lookupname(o, "trackname", &tren)) {
		return 0;
	}
	t = song_trklookup(usong, tren);
	if (t != NULL) {
		logx(1, "%s: track already exists", o->procname);
		return 0;
	}
	t = undo_tnew_do(usong, o->procname, tren);
	return 1;
}

unsigned
blt_tdel(struct exec *o, struct data **r)
{
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	undo_tdel_do(usong, t, o->procname);
	return 1;
}

unsigned
blt_tren(struct exec *o, struct data **r)
{
	char *name;
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_trklookup(usong, name)) {
		logx(1, "%s: name already used by another track", o->procname);
		return 0;
	}
	undo_setstr(usong, o->procname, &t->name.str, name);
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
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	track_timeinfo(&usong->meta, measure, &pos, NULL, &bpm, &tpb);

	if (beat < 0 || (unsigned)beat >= bpm ||
	    tic  < 0 || (unsigned)tic  >= tpb) {
		logx(1, "%s: beat/tick must fit in the measure", o->procname);
		return 0;
	}
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	pos += beat * tpb + tic;
	tp = seqptr_new(&t->track);
	seqptr_seek(tp, pos);
	seqptr_evput(tp, &ev);
	seqptr_del(tp);
	undo_track_diff(usong);
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
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	arg = exec_varlookup(o, "filtname");
	if (!arg) {
		logx(1, "%s: filtname: no such param", __func__);
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		f = NULL;
	} else if (arg->data->type == DATA_REF) {
		f = song_filtlookup(usong, arg->data->val.ref);
		if (!f) {
			logx(1, "%s: no such filt", o->procname);
			return 0;
		}
	} else {
		logx(1, "%s: bad filt name", o->procname);
		return 0;
	}
	t->curfilt = f;
	song_setcurfilt(usong, f);
	return 1;
}

unsigned
blt_tgetf(struct exec *o, struct data **r)
{
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
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
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	track_check(&t->track);
	undo_track_diff(usong);
	return 1;
}

unsigned
blt_trewrite(struct exec *o, struct data **r)
{
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!song_try_trk(usong, t)) {
		return 0;
	}
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	track_rewrite(&t->track);
	undo_track_diff(usong);
	return 1;
}

unsigned
blt_tcut(struct exec *o, struct data **r)
{
	unsigned tic, len, qstep;
	struct songtrk *t;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
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
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	track_cut(&t->track, tic, len);
	usong->curlen = 0;
	undo_track_diff(usong);
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
		logx(1, "%s: no current track", o->procname);
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
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	track_ins(&t->track, tic, len);
	usong->curlen += amount;
	undo_track_diff(usong);
	return 1;
}

unsigned
blt_tclr(struct exec *o, struct data **r)
{
	struct songtrk *t;
	unsigned tic, len, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
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
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	track_move(&t->track, tic, len, &usong->curev, NULL, 0, 1);
	undo_track_diff(usong);
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
		logx(1, "%s: no current track", o->procname);
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
		undo_track_save(usong, &t->track, o->procname, t->name.str);
		track_merge(&t->track, &copy);
		undo_track_diff(usong);
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
		logx(1, "%s: no current track", o->procname);
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
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!song_try_trk(usong, dst)) {
		return 0;
	}
	undo_track_save(usong, &dst->track, o->procname, dst->name.str);
	track_merge(&src->track, &dst->track);
	undo_track_diff(usong);
	return 1;
}

unsigned
blt_tquant_common(struct exec *o, struct data **r, int all)
{
	struct songtrk *t;
	unsigned tic, len, qstep, offset;
	long rate;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!exec_lookuplong(o, "rate", &rate)) {
		return 0;
	}
	if (rate > 100) {
		logx(1, "%s: rate must be between 0 and 100", o->procname);
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
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	if (all) {
		track_quantize(&t->track, &usong->curev,
		    tic, len, offset, 2 * qstep, rate);
	} else {
		track_quantize_frame(&t->track, &usong->curev,
		    tic, len, offset, 2 * qstep, rate);
	}
	undo_track_diff(usong);
	return 1;
}

unsigned
blt_tquant(struct exec *o, struct data **r)
{
	logx(1, "warning, tquant is deprecated, use tquanta instead");
	return blt_tquant_common(o, r, 1);
}

unsigned
blt_tquanta(struct exec *o, struct data **r)
{
	return blt_tquant_common(o, r, 1);
}

unsigned
blt_tquantf(struct exec *o, struct data **r)
{
	return blt_tquant_common(o, r, 0);
}

unsigned
blt_ttransp(struct exec *o, struct data **r)
{
	struct songtrk *t;
	long halftones;
	unsigned tic, len, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!exec_lookuplong(o, "halftones", &halftones)) {
		return 0;
	}
	if (halftones < -64 || halftones >= 63) {
		logx(1, "%s: argument not in the -64..63 range", o->procname);
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
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	track_transpose(&t->track, tic, len, &usong->curev, halftones);
	undo_track_diff(usong);
	return 1;
}

unsigned
blt_tvcurve(struct exec *o, struct data **r)
{
	struct songtrk *t;
	unsigned tic, len, qstep;
	long weight;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	if (!exec_lookuplong(o, "weight", &weight))
		return 0;
	if (weight < -63 || weight > 63) {
		logx(1, "%s: weight must be in the -63..63 range", o->procname);
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
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	track_vcurve(&t->track, tic, len, &usong->curev, weight);
	undo_track_diff(usong);
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
		logx(1, "%s: no current track", o->procname);
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
	undo_track_save(usong, &t->track, o->procname, t->name.str);
	track_evmap(&t->track, tic, len, &usong->curev, &from, &to);
	undo_track_diff(usong);
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
		logx(1, "%s: no current track", o->procname);
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
		logx(1, "%s: no current track", o->procname);
		return 0;
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);

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
	textout_putstr(tout, "}\n");
	return 1;
}


void
blt_tdump_output(struct state *s,
	unsigned meas, unsigned beat, unsigned tick, struct textout *f)
{
	char posbuf[32];
	struct seqev *se;
	unsigned phase;
	unsigned delta;
	int more = 0;

	se = s->pos;
	phase = ev_phase(&se->ev);

	delta = 0;
	for (;;) {
		if (ev_match(&se->ev, &s->ev)) {
			if (more) {
				snprintf(posbuf, sizeof(posbuf), "%10u", delta);
			} else {
				snprintf(posbuf, sizeof(posbuf), "%04u:%02u:%02u",
				    meas, beat, tick);
				more = 1;
			}
			textout_putstr(tout, posbuf);
			textout_putstr(f, " ");
			ev_output(&se->ev, f);
			textout_putstr(f, "\n");

			phase = ev_phase(&se->ev);
			if (phase & EV_PHASE_LAST)
				break;
		}
		se = se->next;
		delta += se->delta;
	}
}

unsigned
blt_tdump(struct exec *o, struct data **r)
{
	struct state *s;
	struct songtrk *t;
	struct seqptr *mp, *tp;
	unsigned meas, beat, tick, tpb, bpm;
	unsigned mdelta, tdelta;
	unsigned abspos, start, end, qstep;

	song_getcurtrk(usong, &t);
	if (t == NULL) {
		logx(1, "%s: no current track", o->procname);
		return 0;
	}

	start = track_findmeasure(&usong->meta, usong->curpos);
	end = track_findmeasure(&usong->meta, usong->curpos + usong->curlen);
	qstep = usong->curquant / 2;
	if (start > qstep)
		start -= qstep;

	abspos = meas = beat = tick = 0;
	mp = seqptr_new(&usong->meta);
	tp = seqptr_new(&t->track);

	textout_putstr(tout, "{\n");
	textout_shiftright(tout);

	while (1) {
		/*
		 * scan for a time signature change
		 */
		while (seqptr_evget(mp))
			; /* nothing */

		while (1) {
			s = seqptr_evget(tp);
			if (s == NULL)
				break;
			if (!(s->phase & EV_PHASE_FIRST))
				continue;
			if (state_inspec(s, &usong->curev) &&
			    abspos >= start &&
			    abspos < end)
				blt_tdump_output(s, meas, beat, tick, tout);
		}

		/*
		 * move to the next non empty tick: the next tic is the
		 * smaller position of the next event of each track
		 */
		tdelta = tp->pos->delta - tp->delta;
		if (tdelta == 0)
			break;
		mdelta = mp->pos->delta - mp->delta;
		if (mdelta > 0) {
			if (mdelta < tdelta)
				tdelta = mdelta;
			seqptr_ticskip(mp, tdelta);
		}
		seqptr_ticskip(tp, tdelta);
		abspos += tdelta;

		seqptr_getsign(mp, &bpm, &tpb);
		tick += tdelta;
		beat += tick / tpb;
		tick = tick % tpb;
		meas += beat / bpm;
		beat = beat % bpm;
	}

	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");

	seqptr_del(tp);
	seqptr_del(mp);

	return 1;
}

unsigned
blt_clist(struct exec *o, struct data **r, int input)
{
	struct data *d, *n;
	struct songchan *i;

	d = data_newlist(NULL);
	SONG_FOREACH_CHAN(usong, i) {
		if (!!i->isinput != !!input)
			continue;
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
	struct songchan *c;
	unsigned dev, ch;

	if (!exec_lookupname(o, "channame", &name) ||
	    !exec_lookupchan_getnum(o, "channum", &dev, &ch, input)) {
		return 0;
	}
	c = song_chanlookup(usong, name, input);
	if (c != NULL) {
		logx(1, "%s: %s: already exists", o->procname, name);
		return 0;
	}
	c = song_chanlookup_bynum(usong, dev, ch, input);
	if (c != NULL) {
		logx(1, "%s: dev/chan number already in use", o->procname);
		return 0;
	}
	if (dev > EV_MAXDEV || ch > EV_MAXCH) {
		logx(1, "%s: dev/chan number out of bounds", o->procname);
		return 0;
	}
	if (!song_try_curchan(usong, input)) {
		return 0;
	}
	undo_cnew_do(usong, dev, ch, input, o->procname, name);
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
		logx(1, "%s: no current chan", o->procname);
		return 0;
	}
	if (!song_try_chan(usong, c, input)) {
		return 0;
	}
	undo_cdel_do(usong, c, o->procname);
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
		logx(1, "%s: no current chan", o->procname);
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_chanlookup(usong, name, input)) {
		logx(1, "%s: %s: channel name already in use", o->procname, name);
		return 0;
	}
	if (c->filt && song_filtlookup(usong, name)) {
		logx(1, "%s: %s: filt name already in use", o->procname, name);
		return 0;
	}
	undo_setstr(usong, o->procname, &c->name.str, name);
	if (c->filt)
		undo_setstr(usong, NULL, &c->filt->name.str, name);
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
		logx(1, "%s: no current chan", o->procname);
		return 0;
	}
	if (!exec_lookupchan_getnum(o, "channum", &dev, &ch, input)) {
		return 0;
	}
	i = song_chanlookup_bynum(usong, dev, ch, input);
	if (i != NULL) {
		logx(1, "%s: dev/chan number already used", o->procname);
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
	evspec_reset(&to);
	to.dev_min = to.dev_max = dev;
	to.ch_min = to.ch_max = ch;

	undo_setuint(usong, o->procname, c->name.str, &c->dev, dev);
	undo_setuint(usong, NULL, c->name.str, &c->ch, ch);

	SONG_FOREACH_FILT(usong, f) {
		undo_filt_save(usong, &f->filt, NULL, f->name.str);
		if (input)
			filt_chgin(&f->filt, &from, &to, 0);
		else
			filt_chgout(&f->filt, &from, &to, 0);
	}
	undo_track_save(usong, &c->conf, NULL, c->name.str);
	track_setchan(&c->conf, dev, ch);
	undo_track_diff(usong);
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
		logx(1, "%s: no current chan", o->procname);
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
		logx(1, "%s: no current chan", o->procname);
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
		logx(1, "%s: no current chan", o->procname);
		return 0;
	}
	if (!exec_lookupev(o, "event", &ev, input)) {
		return 0;
	}
	if (!EV_ISVOICE(&ev)) {
		logx(1, "%s: only voice events can be added to channels", o->procname);
		return 0;
	}
	if (ev.ch != c->ch || ev.dev != c->dev) {
		logx(1, "%s: event device or channel mismatch channel ones", o->procname);
		return 0;
	}
	if (ev_phase(&ev) != (EV_PHASE_FIRST | EV_PHASE_LAST)) {
		logx(1, "%s: event must be stateless", o->procname);
		return 0;
	}
	undo_track_save(usong, &c->conf, o->procname, c->name.str);
	song_confev(usong, c, &ev);
	undo_track_diff(usong);
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
		logx(1, "%s: no current chan", o->procname);
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es, input)) {
		return 0;
	}
	undo_track_save(usong, &c->conf, o->procname, c->name.str);
	song_unconfev(usong, c, &es);
	undo_track_diff(usong);
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
		logx(1, "%s: no current chan", o->procname);
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
		logx(1, "%s: filt already exists", o->procname);
		return 0;
	}
	undo_fnew_do(usong, o->procname, name);
	return 1;
}

unsigned
blt_fdel(struct exec *o, struct data **r)
{
	struct songfilt *f;

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		logx(1, "%s: no current filt", o->procname);
		return 0;
	}
	undo_fdel_do(usong, f, o->procname);
	return 1;
}

unsigned
blt_fren(struct exec *o, struct data **r)
{
	struct songfilt *f;
	struct songchan *c;
	char *name;

	song_getcurfilt(usong, &f);
	if (f == NULL) {
		logx(1, "%s: no current filt", o->procname);
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_filtlookup(usong, name)) {
		logx(1, "%s: %s: filt name already in use", o->procname, name);
		return 0;
	}
	SONG_FOREACH_CHAN(usong, c) {
		if (c->filt == f) {
			logx(1, "%s: %s: rename channel to rename this filter", o->procname, name);
			return 0;
		}
	}
	undo_setstr(usong, o->procname, &f->name.str, name);
	return 1;
}

unsigned
blt_fexists(struct exec *o, struct data **r)
{
	char *name;
	struct songfilt *f;

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
		logx(1, "%s: no current filt", o->procname);
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
		logx(1, "%s: no current filt", o->procname);
		return 0;
	}
	if (mux_isopen)
		norm_shut();
	undo_filt_save(usong, &f->filt, o->procname, f->name.str);
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
		logx(1, "%s: no current filt", o->procname);
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from, 1) ||
	    !exec_lookupevspec(o, "to", &to, 0)) {
		return 0;
	}
	if (mux_isopen)
		norm_shut();
	undo_filt_save(usong, &f->filt, o->procname, f->name.str);
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
		logx(1, "%s: no current filt", o->procname);
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from, 1) ||
	    !exec_lookupevspec(o, "to", &to, 0)) {
		return 0;
	}
	if (mux_isopen)
		norm_shut();
	undo_filt_save(usong, &f->filt, o->procname, f->name.str);
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
		logx(1, "%s: no current filt", o->procname);
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es, 0) ||
	    !exec_lookuplong(o, "plus", &plus)) {
		return 0;
	}
	if (plus < -63 || plus > 63) {
		logx(1, "%s: plus must be in the -63..63 range", o->procname);
		return 0;
	}
	if ((es.cmd != EVSPEC_ANY && es.cmd != EVSPEC_NOTE) ||
	    (es.cmd == EVSPEC_NOTE &&
		(es.v0_min != 0 || es.v0_max != EV_MAXCOARSE))) {
		logx(1, "%s: set must contain full range notes", o->procname);
		return 0;
	}
	if (mux_isopen)
		norm_shut();
	undo_filt_save(usong, &f->filt, o->procname, f->name.str);
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
		logx(1, "%s: no current filt", o->procname);
		return 0;
	}
	if (!exec_lookupevspec(o, "evspec", &es, 0) ||
	    !exec_lookuplong(o, "weight", &weight)) {
		return 0;
	}
	if (weight < -63 || weight > 63) {
		logx(1, "%s: plus must be in the -63..63 range", o->procname);
		return 0;
	}
	if (es.cmd != EVSPEC_ANY && es.cmd != EVSPEC_NOTE) {
		logx(1, "%s: set must contain notes", o->procname);
		return 0;
	}
	if (mux_isopen)
		norm_shut();
	undo_filt_save(usong, &f->filt, o->procname, f->name.str);
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
		logx(1, "%s: no current filt", o->procname);
		return 0;
	}
	if (!exec_lookupevspec(o, "from", &from, input) ||
	    !exec_lookupevspec(o, "to", &to, input)) {
		return 0;
	}
	if (evspec_isec(&from, &to)) {
		logx(1, "%s: \"from\" and \"to\" event ranges must be disjoint", o->procname);
	}
	if (mux_isopen)
		norm_shut();
	undo_filt_save(usong, &f->filt, o->procname, f->name.str);
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
		logx(1, "%s: sysex already exists", o->procname);
		return 0;
	}
	if (!song_try_cursx(usong)) {
		return 0;
	}
	c = undo_xnew_do(usong, o->procname, name);
	return 1;
}

unsigned
blt_xdel(struct exec *o, struct data **r)
{
	struct songsx *c;

	song_getcursx(usong, &c);
	if (c == NULL) {
		logx(1, "%s: no current sysex", o->procname);
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	undo_xdel_do(usong, o->procname, c);
	return 1;
}

unsigned
blt_xren(struct exec *o, struct data **r)
{
	struct songsx *c;
	char *name;

	song_getcursx(usong, &c);
	if (c == NULL) {
		logx(1, "%s: no current sysex", o->procname);
		return 0;
	}
	if (!exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_sxlookup(usong, name)) {
		logx(1, "%s: %s: already in use", o->procname, name);
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	undo_setstr(usong, o->procname, &c->name.str, name);
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
		logx(1, "%s: no current sysex", o->procname);
		return 0;
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);

	for (e = c->sx.first; e != NULL; e = e->next) {
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
	textout_putstr(tout, "}\n");
	return 1;
}


unsigned
blt_xrm(struct exec *o, struct data **r)
{
	struct songsx *c;
	struct sysex *x, *xnext;
	struct data *d;
	unsigned int pos, match;

	song_getcursx(usong, &c);
	if (c == NULL) {
		logx(1, "%s: no current sysex", o->procname);
		return 0;
	}
	if (!exec_lookuplist(o, "data", &d)) {
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	pos = 0;
	undo_start(usong, o->procname, c->name.str);
	for (x = c->sx.first; x != NULL; x = xnext) {
		xnext = x->next;
		if (!data_matchsysex(d, x, &match))
			return 0;
		if (match)
			undo_xrm_do(usong, NULL, c, pos);
		else
			pos++;
	}
	return 1;
}

unsigned
blt_xsetd(struct exec *o, struct data **r)
{
	struct songsx *c;
	struct sysex *x;
	struct data *d;
	unsigned int match;
	long unit;

	song_getcursx(usong, &c);
	if (c == NULL) {
		logx(1, "%s: no current sysex", o->procname);
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit) ||
	    !exec_lookuplist(o, "data", &d)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS) {
		logx(1, "%s: devnum out of range", o->procname);
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}

	undo_start(usong, o->procname, c->name.str);
	for (x = c->sx.first; x != NULL; x = x->next) {
		if (!data_matchsysex(d, x, &match)) {
			return 0;
		}
		if (match) {
			undo_setuint(usong,
			    o->procname, c->name.str, &x->unit, unit);
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
		logx(1, "%s: no current sysex", o->procname);
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS) {
		logx(1, "%s: devnum out of range", o->procname);
		return 0;
	}
	arg = exec_varlookup(o, "data");
	if (!arg) {
		logx(1, "%s: no such var", __func__);
		panic();
	}
	if (arg->data->type != DATA_LIST) {
		logx(1, "%s: data must be a list of numbers", o->procname);
		return 0;
	}
	if (!song_try_sx(usong, c)) {
		return 0;
	}
	x = sysex_new(unit);
	for (byte = arg->data->val.list; byte != 0; byte = byte->next) {
		if (byte->type != DATA_LONG) {
			logx(1, "%s: only bytes allowed as data", o->procname);
			sysex_del(x);
			return 0;
		}
		if (byte->val.num < 0 || byte->val.num > 0xff) {
			logx(1, "%s: data out of range", o->procname);
			sysex_del(x);
			return 0;
		}
		sysex_add(x, byte->val.num);
	}
	if (!sysex_check(x)) {
		logx(1, "%s: bad sysex format", o->procname);
		sysex_del(x);
		return 0;
	}
	if (x->first) {
		undo_xadd_do(usong, o->procname, c, x);
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
		logx(1, "%s: no current sysex", o->procname);
		return 0;
	}
	if (!exec_lookuplong(o, "devnum", &unit)) {
		return 0;
	}
	if (unit < 0 || unit >= DEFAULT_MAXNDEVS) {
		logx(1, "%s: devnum out of range", o->procname);
		return 0;
	}
	arg = exec_varlookup(o, "path");
	if (!arg) {
		logx(1, "%s: path: no such param", __func__);
		return 0;
	}
	if (arg->data->type != DATA_STRING) {
		logx(1, "%s: path must be string", o->procname);
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
		logx(1, "%s: no current sysex", o->procname);
		return 0;
	}
	arg = exec_varlookup(o, "path");
	if (!arg) {
		logx(1, "%s: path: no such param", __func__);
		return 0;
	}
	if (arg->data->type != DATA_STRING) {
		logx(1, "%s: path must be string", o->procname);
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
		logx(1, "%s: path: no such param", __func__);
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		path = NULL;
	} else if (arg->data->type == DATA_STRING) {
		path = arg->data->val.str;
	} else {
		logx(1, "%s: path must be string or nil", o->procname);
		return 0;
	}
	if (str_eq(modename, "ro")) {
		mode = MIDIDEV_MODE_IN;
	} else if (str_eq(modename, "wo")) {
		mode = MIDIDEV_MODE_OUT;
	} else if (str_eq(modename, "rw")) {
		mode = MIDIDEV_MODE_IN | MIDIDEV_MODE_OUT;
	} else {
		logx(1, "%s: %s: bad mode (allowed: ro, wo, rw)", o->procname, modename);
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
		logx(1, "%s: no such var", __func__);
		panic();
	}
	if (arg->data->type == DATA_NIL) {
		mididev_mtcsrc = NULL;
		return 0;
	} else if (arg->data->type == DATA_LONG) {
		unit = arg->data->val.num;
		if (unit < 0 || unit >= DEFAULT_MAXNDEVS ||
		    !mididev_byunit[unit]) {
			logx(1, "%s: bad device number", o->procname);
			return 0;
		}
		mididev_mtcsrc = mididev_byunit[unit];
		mididev_clksrc = NULL;
		return 1;
	}
	logx(1, "%s: bad argument type for 'unit'", o->procname);
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
			logx(1, "%s: bad device number", o->procname);
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
		logx(1, "%s: devnum: no such var", __func__);
		panic();
	}
	if (arg->data->type == DATA_NIL) {
		mididev_clksrc = NULL;
		return 0;
	} else if (arg->data->type == DATA_LONG) {
		unit = arg->data->val.num;
		if (unit < 0 || unit >= DEFAULT_MAXNDEVS ||
		    !mididev_byunit[unit]) {
			logx(1, "%s: bad device number", o->procname);
			return 0;
		}
		mididev_clksrc = mididev_byunit[unit];
		mididev_mtcsrc = NULL;
		return 1;
	}
	logx(1, "%s: bad argument type for 'unit'", o->procname);
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
			logx(1, "%s: bad device number", o->procname);
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
		logx(1, "%s: bad device number", o->procname);
		return 0;
	}
	if (tpu < DEFAULT_TPU || (tpu % DEFAULT_TPU)) {
		logx(1, "%s: device tpu must be multiple of 96", o->procname);
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
		logx(1, "%s: bad device number", o->procname);
		return 0;
	}
	dev = mididev_byunit[unit];
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);

	textout_putstr(tout, "devnum ");
	textout_putlong(tout, unit);
	textout_putstr(tout, "\n");

	if (mididev_mtcsrc == dev) {
		textout_putstr(tout, "mtcrx\t\t\t# master MTC source\n");
	}
	if (dev->sendmmc) {
		textout_putstr(tout, "mmctx\t\t\t# sends MMC messages\n");
	}
	if (mididev_clksrc == dev) {
		textout_putstr(tout, "clkrx\t\t\t# master clock source\n");
	}
	if (dev->sendclk) {
		textout_putstr(tout, "clktx\t\t\t# sends clock ticks\n");
	}
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
		logx(1, "%s: bad device number", o->procname);
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
		logx(1, "%s: bad device number", o->procname);
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
		logx(1, "%s: bad device number", o->procname);
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
		logx(1, "%s: bad device number", o->procname);
		return 0;
	}
	if (!data_getxev(list, &flags)) {
		return 0;
	}
	mididev_byunit[unit]->oevset = flags;
	return 1;
}
