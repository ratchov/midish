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
 * implements songxxx built-in functions
 * available through the interpreter
 */

#include "dbg.h"
#include "default.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"

#include "trackop.h"
#include "song.h"
#include "user.h"
#include "smf.h"
#include "saveload.h"
#include "textio.h"



unsigned
user_func_songsetcurchan(struct exec_s *o, struct data_s **r) {
	struct songchan_s *t;
	struct var_s *arg;
	
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
user_func_songgetcurchan(struct exec_s *o, struct data_s **r) {
	struct songchan_s *cur;
	
	song_getcurchan(user_song, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}

unsigned
user_func_songsetcursysex(struct exec_s *o, struct data_s **r) {
	struct songsx_s *t;
	struct var_s *arg;
	
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
user_func_songgetcursysex(struct exec_s *o, struct data_s **r) {
	struct songsx_s *cur;
	
	song_getcursx(user_song, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}


unsigned
user_func_songsetunit(struct exec_s *o, struct data_s **r) {	/* tics per unit note */
	long tpu;
	if (!exec_lookuplong(o, "tics_per_unit", &tpu)) {
		return 0;
	}
	if ((tpu % DEFAULT_TPU) != 0 || tpu < DEFAULT_TPU) {
		cons_err("songsetunit: unit must be multiple of 96 tics");
		return 0;
	}
	/* XXX: should check that all tracks are empty (tempo track included) */
	if (user_song->trklist) {
		cons_err("WARNING: unit must be changed before any tracks are created");
	}
	user_song->tics_per_unit = tpu;
	return 1;
}

unsigned
user_func_songgetunit(struct exec_s *o, struct data_s **r) {	/* tics per unit note */
	*r = data_newlong(user_song->tics_per_unit);
	return 1;
}


unsigned
user_func_songsetcurpos(struct exec_s *o, struct data_s **r) {
	long measure;
	
	if (!exec_lookuplong(o, "measure", &measure)) {
		return 0;
	}
	if (measure < 0) {
		cons_err("measure cant be negative");
		return 0;
	}
	user_song->curpos = measure;
	return 1;
}

unsigned
user_func_songgetcurpos(struct exec_s *o, struct data_s **r) {
	*r = data_newlong(user_song->curpos);
	return 1;
}

unsigned
user_func_songsetcurlen(struct exec_s *o, struct data_s **r) {
	long len;
	
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
user_func_songgetcurlen(struct exec_s *o, struct data_s **r) {
	*r = data_newlong(user_song->curlen);
	return 1;
}


unsigned
user_func_songsetcurquant(struct exec_s *o, struct data_s **r) {
	long quantum;
	
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
user_func_songgetcurquant(struct exec_s *o, struct data_s **r) {
	*r = data_newlong(user_song->curquant);
	return 1;
}



unsigned
user_func_songsetcurtrack(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	struct var_s *arg;
	
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
user_func_songgetcurtrack(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *cur;
	
	song_getcurtrk(user_song, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}
	return 1;
}


unsigned
user_func_songsetcurfilt(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *f;
	struct var_s *arg;
	
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
user_func_songgetcurfilt(struct exec_s *o, struct data_s **r) {
	struct songfilt_s *cur;
	
	song_getcurfilt(user_song, &cur);
	if (cur) {
		*r = data_newref(cur->name.str);
	} else {
		*r = data_newnil();
	}	
	return 1;
}

unsigned
user_func_songinfo(struct exec_s *o, struct data_s **r) {
	char map[DEFAULT_MAXNCHANS];
	struct songtrk_s *t;
	struct songchan_s *c;
	struct songfilt_s *f;
	struct songsx_s *s;
	struct sysex_s *x;
	unsigned i, count;
	unsigned dev, ch;
	
	/* print info about channels */	

	textout_putstr(tout, "chanlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# chan_name,  {devicenum, midichan}, default_input\n");
	for (c = user_song->chanlist; c != NULL; c = (struct songchan_s *)c->name.next) {
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

	/* print info about filters */	

	textout_putstr(tout, "filtlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# filter_name,  default_channel\n");
	for (f = user_song->filtlist; f != NULL; f = (struct songfilt_s *)f->name.next) {
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

	/* print info about tracks */

	textout_putstr(tout, "tracklist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# track_name,  default_filter,  used_channels,  flags\n");
	for (t = user_song->trklist; t != NULL; t = (struct songtrk_s *)t->name.next) {
		textout_indent(tout);
		textout_putstr(tout, t->name.str);
		textout_putstr(tout, "\t");
		if (t->curfilt != NULL) {
			textout_putstr(tout, t->curfilt->name.str);
		} else {
			textout_putstr(tout, "nil");
		}
		textout_putstr(tout, "\t{");
		track_opchaninfo(&t->track, map);
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

	/* print info about sysex banks */	

	textout_putstr(tout, "sysexlist {\n");
	textout_shiftright(tout);
	textout_indent(tout);
	textout_putstr(tout, "# sysex_name,  number_messages\n");
	for (s = user_song->sxlist; s != NULL; s = (struct songsx_s *)s->name.next) {
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
	
	/* print current values */

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
user_func_songsave(struct exec_s *o, struct data_s **r) {
	char *filename;	
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}	
	song_save(user_song, filename);
	return 1;
}

unsigned
user_func_songload(struct exec_s *o, struct data_s **r) {
	char *filename;		
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	song_done(user_song);
	song_init(user_song);
	return song_load(user_song, filename);
}


unsigned
user_func_songreset(struct exec_s *o, struct data_s **r) {
	song_done(user_song);
	song_init(user_song);
	return 1;
}



unsigned
user_func_songexportsmf(struct exec_s *o, struct data_s **r) {
	char *filename;
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}	
	return song_exportsmf(user_song, filename);
}


unsigned
user_func_songimportsmf(struct exec_s *o, struct data_s **r) {
	char *filename;
	struct song_s *sng;
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}
	sng = song_importsmf(filename);
	if (sng == NULL) {
		return 0;
	}
	song_delete(user_song);
	user_song = sng;
	return 1;
}


unsigned
user_func_songidle(struct exec_s *o, struct data_s **r) {
	song_idle(user_song);
	return 1;
}
		
unsigned
user_func_songplay(struct exec_s *o, struct data_s **r) {
	song_play(user_song);
	return 1;
}

unsigned
user_func_songrecord(struct exec_s *o, struct data_s **r) {
	song_record(user_song);
	return 1;
}

unsigned
user_func_songsettempo(struct exec_s *o, struct data_s **r) {	/* beat per minute*/
	long tempo, measure;
	struct ev_s ev;
	struct seqptr_s mp;
	unsigned bpm, tpb;
	unsigned long dummy;
	unsigned pos;
	
	if (!exec_lookuplong(o, "measure", &measure) ||
	    !exec_lookuplong(o, "beats_per_minute", &tempo)) {
		return 0;
	}	
	if (tempo < 40 || tempo > 240) {
		cons_err("tempo must be between 40 and 240 beats per measure");
		return 0;
	}
	pos = track_opfindtic(&user_song->meta, measure);
	track_optimeinfo(&user_song->meta, pos, &dummy, &bpm, &tpb);
	
	track_rew(&user_song->meta, &mp);
	track_seekblank(&user_song->meta, &mp, pos);
	
	ev.cmd = EV_TEMPO;
	ev.data.tempo.usec24 = 60L * 24000000L / (tempo * tpb);
	
	track_evlast(&user_song->meta, &mp);
	track_evput(&user_song->meta, &mp, &ev);
	track_opcheck(&user_song->meta);
	return 1;
}

unsigned
user_func_songtimeins(struct exec_s *o, struct data_s **r) {
	long num, den, amount, from;
	struct ev_s ev;
	struct seqptr_s mp;
	unsigned pos, tics, save_bpm, save_tpb;
	unsigned long dummy_usec24;
	
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
	if (amount == 0) {
		return 1;
	}

	ev.cmd = EV_TIMESIG;
	ev.data.sign.beats = num;
	ev.data.sign.tics = user_song->tics_per_unit / den;

	pos = track_opfindtic(&user_song->meta, from);
	tics = ev.data.sign.beats * ev.data.sign.tics * amount;

	track_optimeinfo(&user_song->meta, pos, &dummy_usec24, &save_bpm, &save_tpb);
	
	track_rew(&user_song->meta, &mp);
	track_seekblank(&user_song->meta, &mp, pos);
	track_opinsert(&user_song->meta, &mp, tics);
	
	if (ev.data.sign.beats != save_bpm || ev.data.sign.tics != save_tpb) {
		track_rew(&user_song->meta, &mp);
		track_seek(&user_song->meta, &mp, pos);
		track_evput(&user_song->meta, &mp, &ev);
	
		ev.cmd = EV_TIMESIG;
		ev.data.sign.beats = save_bpm;
		ev.data.sign.tics = save_tpb;
		track_seek(&user_song->meta, &mp, tics);
		track_evput(&user_song->meta, &mp, &ev);
	}
	return 1;
}

unsigned
user_func_songtimerm(struct exec_s *o, struct data_s **r) {
	long amount, from;
	struct ev_s ev;
	struct seqptr_s mp;
	unsigned pos, tics, save_bpm, save_tpb, bpm, tpb;
	unsigned long save_usec24, usec24;
	
	if (!exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount)) {
		return 0;
	}
	if (amount == 0) {
		return 1;
	}

	pos = track_opfindtic(&user_song->meta, from);
	tics = track_opfindtic(&user_song->meta, from + amount) - pos;
	track_optimeinfo(&user_song->meta, pos, &save_usec24, &save_bpm, &save_tpb);
		
	track_rew(&user_song->meta, &mp);
	if (track_seek(&user_song->meta, &mp, pos)) {
		return 1;
	}
	track_opcut(&user_song->meta, &mp, tics);
	track_optimeinfo(&user_song->meta, pos, &usec24, &bpm, &tpb);
	
	if (bpm != save_bpm || tpb != save_tpb) {
		ev.cmd = EV_TIMESIG;
		ev.data.sign.beats = save_bpm;
		ev.data.sign.tics = save_tpb;
		track_rew(&user_song->meta, &mp);
		track_seek(&user_song->meta, &mp, pos);
		track_evput(&user_song->meta, &mp, &ev);
	}
	/* 
	 * XXX: other timesig events should be removed, 
	 * but we don't have a routine for this,
	 * so we use track_opcheck 
	 */
	track_opcheck(&user_song->meta);
	return 1;
}


unsigned
user_func_songtimeinfo(struct exec_s *o, struct data_s **r) {
	track_output(&user_song->meta, tout);
	textout_putstr(tout, "\n");
	return 1;
}


unsigned
user_func_songsetcurinput(struct exec_s *o, struct data_s **r) {
	unsigned dev, ch;
	struct data_s *l;
	
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
user_func_songgetcurinput(struct exec_s *o, struct data_s **r) {
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(user_song->curinput_dev));
	data_listadd(*r, data_newlong(user_song->curinput_ch));
	return 1;
}
