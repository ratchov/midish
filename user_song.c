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
 * implements songxxx built-in functions available through the
 * interpreter
 *
 * each function is described in the manual.html file
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


unsigned
user_func_songsetcurchan(struct exec *o, struct data **r) {
	struct songchan *t;
	struct var *arg;
	
	if (!song_try(user_song)) {
		return 0;
	}
	arg = exec_varlookup(o, "channame");
	if (!arg) {
		dbg_puts("user_func_songsetcurchan: 'channame': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcurchan(user_song, NULL);
		return 1;
	} 
	if (!exec_lookupchan_getref(o, "channame", &t)) {
		return 0;
	}
	song_setcurchan(user_song, t);
	return 1;
}

unsigned
user_func_songgetcurchan(struct exec *o, struct data **r) {
	struct songchan *cur;
	
	if (!song_try(user_song)) {
		return 0;
	}
	song_getcurchan(user_song, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
user_func_songsetcursysex(struct exec *o, struct data **r) {
	struct songsx *t;
	struct var *arg;
	
	if (!song_try(user_song)) {
		return 0;
	}
	arg = exec_varlookup(o, "sysexname");
	if (!arg) {
		dbg_puts("user_func_songsetcursysex: 'sysexname': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcursx(user_song, NULL);
		return 1;
	} 
	if (!exec_lookupsx(o, "sysexname", &t)) {
		return 0;
	}
	song_setcursx(user_song, t);
	return 1;
}

unsigned
user_func_songgetcursysex(struct exec *o, struct data **r) {
	struct songsx *cur;
	
	if (!song_try(user_song)) {
		return 0;
	}
	song_getcursx(user_song, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
user_func_songsetunit(struct exec *o, struct data **r) {
	long tpu;
	struct songtrk *t;

	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "tics_per_unit", &tpu)) {
		return 0;
	}
	if ((tpu % DEFAULT_TPU) != 0 || tpu < DEFAULT_TPU) {
		cons_err("songsetunit: unit must be multiple of 96 tics");
		return 0;
	}
	SONG_FOREACH_TRK(user_song, t) {
		track_scale(&t->track, user_song->tics_per_unit, tpu);
	}
	track_scale(&user_song->meta, user_song->tics_per_unit, tpu);
	user_song->curquant = user_song->curquant * tpu / user_song->tics_per_unit;
	user_song->tics_per_unit = tpu;
	return 1;
}

unsigned
user_func_songgetunit(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	*r = data_newlong(user_song->tics_per_unit);
	return 1;
}

unsigned
user_func_songsetcurpos(struct exec *o, struct data **r) {
	long measure;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "measure", &measure)) {
		return 0;
	}
	if (measure < 0) {
		cons_err("measure cant be negative");
		return 0;
	}
	user_song->curpos = measure;
	cons_putpos(measure, 0, 0);
	return 1;
}

unsigned
user_func_songgetcurpos(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	*r = data_newlong(user_song->curpos);
	return 1;
}

unsigned
user_func_songsetcurlen(struct exec *o, struct data **r) {
	long len;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "length", &len)) {
		return 0;
	}
	if (len < 0) {
		cons_err("'measures' parameter cant be negative");
		return 0;
	}
	user_song->curlen = len;
	return 1;
}

unsigned
user_func_songgetcurlen(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	*r = data_newlong(user_song->curlen);
	return 1;
}

unsigned
user_func_songsetcurquant(struct exec *o, struct data **r) {
	long quantum;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "quantum", &quantum)) {
		return 0;
	}
	if (quantum < 0 || (unsigned)quantum > user_song->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}
	user_song->curquant = quantum;
	return 1;
}

unsigned
user_func_songgetcurquant(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	*r = data_newlong(user_song->curquant);
	return 1;
}

unsigned
user_func_songsetcurtrack(struct exec *o, struct data **r) {
	struct songtrk *t;
	struct var *arg;
	
	if (!song_try(user_song)) {
		return 0;
	}
	arg = exec_varlookup(o, "trackname");
	if (!arg) {
		dbg_puts("user_func_songsetcurtrack: 'trackname': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcurtrk(user_song, NULL);
		return 1;
	} 
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	song_setcurtrk(user_song, t);
	return 1;
}

unsigned
user_func_songgetcurtrack(struct exec *o, struct data **r) {
	struct songtrk *cur;
	
	if (!song_try(user_song)) {
		return 0;
	}
	song_getcurtrk(user_song, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
user_func_songsetcurfilt(struct exec *o, struct data **r) {
	struct songfilt *f;
	struct var *arg;
	
	if (!song_try(user_song)) {
		return 0;
	}
	arg = exec_varlookup(o, "filtname");
	if (!arg) {
		dbg_puts("user_func_songsetcurfilt: 'filtname': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		song_setcurfilt(user_song, NULL);
		return 1;
	} else if (arg->data->type == DATA_REF) {
		f = song_filtlookup(user_song, arg->data->val.ref);
		if (!f) {
			cons_err("no such filt");
			return 0;
		}
		song_setcurfilt(user_song, f);
		return 1;
	}
	return 0;
}

unsigned
user_func_songgetcurfilt(struct exec *o, struct data **r) {
	struct songfilt *cur;
	
	if (!song_try(user_song)) {
		return 0;
	}
	song_getcurfilt(user_song, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}	
	return 1;
}

unsigned
user_func_songinfo(struct exec *o, struct data **r) {
	char map[DEFAULT_MAXNCHANS];
	struct songtrk *t;
	struct songchan *c;
	struct songfilt *f;
	struct songsx *s;
	struct sysex *x;
	unsigned i, count;
	unsigned dev, ch;
	
	if (!song_try(user_song)) {
		return 0;
	}
	/* 
	 * print info about channels 
	 */
	textout_putstr(tout, "chanlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# chan_name,  {devicenum, midichan}, default_input\n");
	SONG_FOREACH_CHAN(user_song, c) {
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
	SONG_FOREACH_FILT(user_song, f) {
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
	SONG_FOREACH_TRK(user_song, t) {
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
				c = song_chanlookup_bynum(user_song, i / 16, i % 16);
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
	SONG_FOREACH_SX(user_song, s) {
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
	song_getcurchan(user_song, &c);
	if (c) {
		textout_putstr(tout, c->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");

	textout_putstr(tout, "curfilt ");
	song_getcurfilt(user_song, &f);
	if (f) {
		textout_putstr(tout, f->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");

	textout_putstr(tout, "curtrack ");
	song_getcurtrk(user_song, &t);
	if (t) {
		textout_putstr(tout, t->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");	

	textout_putstr(tout, "cursysex ");
	song_getcursx(user_song, &s);
	if (s) {
		textout_putstr(tout, s->name.str);
	} else {
		textout_putstr(tout, "nil");
	}
	textout_putstr(tout, "\n");	

	textout_putstr(tout, "curquant ");
	textout_putlong(tout, user_song->curquant);
	textout_putstr(tout, "\n");	
	textout_putstr(tout, "curpos ");
	textout_putlong(tout, user_song->curpos);
	textout_putstr(tout, "\n");	
	textout_putstr(tout, "curlen ");
	textout_putlong(tout, user_song->curlen);
	textout_putstr(tout, "\n");	

	textout_indent(tout);
	textout_putstr(tout, "curinput {");
	song_getcurinput(user_song, &dev, &ch);
	textout_putlong(tout, dev);
	textout_putstr(tout, " ");
	textout_putlong(tout, ch);
	textout_putstr(tout, "}\n");
	return 1;
}

unsigned
user_func_songsave(struct exec *o, struct data **r) {
	char *filename;	

	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}	
	song_save(user_song, filename);
	return 1;
}

unsigned
user_func_songload(struct exec *o, struct data **r) {
	char *filename;		
	unsigned res;

	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	song_done(user_song);
	song_init(user_song);
	res = song_load(user_song, filename);
	cons_putpos(user_song->curpos, 0, 0);
	return res;
}

unsigned
user_func_songreset(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	song_done(user_song);
	song_init(user_song);
	cons_putpos(user_song->curpos, 0, 0);
	return 1;
}

unsigned
user_func_songexportsmf(struct exec *o, struct data **r) {
	char *filename;

	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}	
	return song_exportsmf(user_song, filename);
}

unsigned
user_func_songimportsmf(struct exec *o, struct data **r) {
	char *filename;
	struct song *sng;

	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	sng = song_importsmf(filename);
	if (sng == NULL) {
		return 0;
	}
	song_delete(user_song);
	user_song = sng;
	cons_putpos(user_song->curpos, 0, 0);
	return 1;
}

unsigned
user_func_songidle(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	song_idle(user_song);
	return 1;
}
		
unsigned
user_func_songplay(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	song_play(user_song);
	return 1;
}

unsigned
user_func_songrecord(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	song_record(user_song);
	return 1;
}

unsigned
user_func_songstop(struct exec *o, struct data **r) {
	if (!mux_isopen) {
		cons_err("nothing to stop, ignored");
		return 1;
	}
	song_stop(user_song);
	return 1;
}

unsigned
user_func_songsettempo(struct exec *o, struct data **r) {
	long tempo, measure;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "measure", &measure) ||
	    !exec_lookuplong(o, "beats_per_minute", &tempo)) {
		return 0;
	}	
	if (tempo < 40 || tempo > 240) {
		cons_err("tempo must be between 40 and 240 beats per measure");
		return 0;
	}
	track_settempo(&user_song->meta, measure, tempo);
	return 1;
}

unsigned
user_func_songtimeins(struct exec *o, struct data **r) {
	long num, den, amount, from;
	unsigned tic, len, tpm;
	unsigned long usec24;
	struct seqptr sp;
	struct track t1, t2, tn;
	struct ev ev;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "numerator", &num) || 
	    !exec_lookuplong(o, "denominator", &den)) {
		return 0;
	}
	if (den != 1 && den != 2 && den != 4 && den != 8) {
		cons_err("only 1, 2, 4 and 8 are supported as denominator");
		return 0;
	}

	tpm = user_song->tics_per_unit * num / den;
	track_timeinfo(&user_song->meta, from, &tic, &usec24, NULL, NULL);
	len = amount * tpm;

	track_init(&tn);
	seqptr_init(&sp, &tn);
	seqptr_ticput(&sp, tic);
	ev.cmd = EV_TIMESIG;
	ev.timesig_beats = num;
	ev.timesig_tics = user_song->tics_per_unit / den;
	seqptr_evput(&sp, &ev);
	ev.cmd = EV_TEMPO;
	ev.tempo_usec24 = usec24;
	seqptr_evput(&sp, &ev);
	seqptr_ticput(&sp, len);
	seqptr_done(&sp);

	track_init(&t1);
	track_init(&t2);
	track_move(&user_song->meta, 0,   tic, NULL, &t1, 1, 1);
	track_move(&user_song->meta, tic, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, tic + len);
	track_clear(&user_song->meta);
	track_merge(&user_song->meta, &t1);
	track_merge(&user_song->meta, &tn);
	track_merge(&user_song->meta, &t2);
	track_done(&t1);
	track_done(&t2);	     
	track_done(&tn);	
	return 1;
}

unsigned
user_func_songtimerm(struct exec *o, struct data **r) {
	long amount, from;
	unsigned tic, len;
	struct track t1, t2;

	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount)) {
		return 0;
	}
	tic = track_findmeasure(&user_song->meta, from);
	len = track_findmeasure(&user_song->meta, from + amount) - tic;

	track_init(&t1);
	track_init(&t2);
	track_move(&user_song->meta, 0,         tic, NULL, &t1, 1, 1);
	track_move(&user_song->meta, tic + len, ~0U, NULL, &t2, 1, 1);
	track_shift(&t2, tic);
	track_clear(&user_song->meta);
	track_merge(&user_song->meta, &t1);
	if (!track_isempty(&t2)) {
		track_merge(&user_song->meta, &t2);
	}
	track_done(&t1);
	track_done(&t2);
	return 1;
}

unsigned
user_func_songtimeinfo(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	track_output(&user_song->meta, tout);
	textout_putstr(tout, "\n");
	return 1;
}

unsigned
user_func_songsetcurinput(struct exec *o, struct data **r) {
	unsigned dev, ch;
	struct data *l;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplist(o, "inputchan", &l)) {
		return 0;
	}
	if (!data_num2chan(l, &dev, &ch)) {
		return 0;
	}
	song_setcurinput(user_song, dev, ch);
	return 1;
}

unsigned
user_func_songgettempo(struct exec *o, struct data **r) {
	long from;
	unsigned tic, bpm, tpb;
	unsigned long usec24;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "from", &from)) {
		return 0;
	}
	track_timeinfo(&user_song->meta, from, &tic, &usec24, &bpm, &tpb);
	*r = data_newlong(60L * 24000000L / (usec24 * tpb));
	return 1;
}

unsigned
user_func_songgetsign(struct exec *o, struct data **r) {
	long from;
	unsigned tic, bpm, tpb;
	unsigned long usec24;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "from", &from)) {
		return 0;
	}
	track_timeinfo(&user_song->meta, from, &tic, &usec24, &bpm, &tpb);
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(bpm));
	data_listadd(*r, data_newlong(user_song->tics_per_unit / tpb));
	return 1;
}

unsigned
user_func_songgetcurinput(struct exec *o, struct data **r) {
	unsigned dev, ch;

	if (!song_try(user_song)) {
		return 0;
	}
	song_getcurinput(user_song, &dev, &ch);  
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(dev));
	data_listadd(*r, data_newlong(ch));
	return 1;
}

unsigned
user_func_songsetfactor(struct exec *o, struct data **r) {
	long tpu;
	if (!exec_lookuplong(o, "tempo_factor", &tpu)) {
		return 0;
	}
	if (tpu < 50 || tpu > 200) {
		cons_err("songsetfactor: factor must be between 50 and 200");
		return 0;
	}
	user_song->tempo_factor = 0x100 * 100 / tpu;
	return 1;
}

unsigned
user_func_songgetfactor(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	*r = data_newlong(user_song->tempo_factor);
	return 1;
}

unsigned
user_func_ctlconfx(struct exec *o, struct data **r) {
	char *name;
	unsigned num, old, val;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookupname(o, "name", &name) ||
	    !exec_lookupctl(o, "ctl", &num) ||
	    !exec_lookupval(o, "defval", 1, &val)) {
		return 0;
	}
	if (num >= 32) {
		cons_err("only controllers 0..31 can be 14bit");
		return 0;
	}
	if (evctl_lookup(name, &old)) {
		evctl_unconf(old);
	}
	evctl_unconf(num);
	evctl_conf(num, name, val);
	return 1;
}

unsigned
user_func_ctlconf(struct exec *o, struct data **r) {
	char *name;
	unsigned num, old, val;
	
	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookupname(o, "name", &name) ||
	    !exec_lookupctl(o, "ctl", &num) ||
	    !exec_lookupval(o, "defval", 0, &val)) {
		return 0;
	}
	if (evctl_lookup(name, &old)) {
		evctl_unconf(old);
	}
	evctl_unconf(num);
	evctl_conf(num, name, val << 7);
	return 1;
}

unsigned
user_func_ctlunconf(struct exec *o, struct data **r) {
	char *name;
	unsigned num;
	
	if (!song_try(user_song)) {
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
user_func_ctlinfo(struct exec *o, struct data **r) {
	if (!song_try(user_song)) {
		return 0;
	}
	evctltab_output(evctl_tab, tout);
	return 1;
}

