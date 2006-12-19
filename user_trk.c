/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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
 * implements trackxxx built-in functions
 * available through the interpreter.
 *
 * for more details about what does each function, refer
 * to manual.html
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
user_func_tracklist(struct exec *o, struct data **r) {
	struct data *d, *n;
	struct songtrk *i;

	d = data_newlist(NULL);
	for (i = user_song->trklist; i != NULL; i = (struct songtrk *)i->name.next) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
user_func_tracknew(struct exec *o, struct data **r) {
	char *trkname;
	struct songtrk *t;
	
	if (!exec_lookupname(o, "trackname", &trkname)) {
		return 0;
	}
	t = song_trklookup(user_song, trkname);
	if (t != NULL) {
		cons_err("tracknew: track already exists");
		return 0;
	}
	t = songtrk_new(trkname);
	song_trkadd(user_song, t);
	return 1;
}

unsigned
user_func_trackdelete(struct exec *o, struct data **r) {
	struct songtrk *t;
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	if (!song_trkrm(user_song, t)) {
		return 0;
	}
	songtrk_delete(t);
	return 1;
}

unsigned
user_func_trackrename(struct exec *o, struct data **r) {
	struct songtrk *t;
	char *name;
	
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_trklookup(user_song, name)) {
		cons_err("name already used by another track");
		return 0;
	}
	str_delete(t->name.str);
	t->name.str = str_new(name);
	return 1;
}

unsigned
user_func_trackexists(struct exec *o, struct data **r) {
	char *name;
	struct songtrk *t;
	
	if (!exec_lookupname(o, "trackname", &name)) {
		return 0;
	}
	t = song_trklookup(user_song, name);
	*r = data_newlong(t != NULL ? 1 : 0);
	return 1;
}

unsigned
user_func_trackaddev(struct exec *o, struct data **r) {
	long measure, beat, tic;
	struct ev ev;
	struct seqptr tp;
	struct songtrk *t;
	unsigned pos, bpm, tpb;

	if (!exec_lookuptrack(o, "trackname", &t) || 
	    !exec_lookuplong(o, "measure", &measure) || 
	    !exec_lookuplong(o, "beat", &beat) || 
	    !exec_lookuplong(o, "tic", &tic) || 
	    !exec_lookupev(o, "event", &ev)) {
		return 0;
	}

	pos = song_measuretotic(user_song, measure);
	track_timeinfo(&user_song->meta, pos, NULL, &bpm, &tpb);
	
	if (beat < 0 || (unsigned)beat >= bpm || 
	    tic  < 0 || (unsigned)tic  >= tpb) {
		cons_err("beat and tic must fit in the selected measure");
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
user_func_tracksetcurfilt(struct exec *o, struct data **r) {
	struct songtrk *t;
	struct songfilt *f;
	struct var *arg;
	
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}	
	arg = exec_varlookup(o, "filtname");
	if (!arg) {
		dbg_puts("user_func_tracksetcurfilt: 'filtname': no such param\n");
		return 0;
	}
	if (arg->data->type == DATA_NIL) {
		t->curfilt = NULL;
		return 1;
	} else if (arg->data->type == DATA_REF) {
		f = song_filtlookup(user_song, arg->data->val.ref);
		if (!f) {
			cons_err("no such filt");
			return 0;
		}
		t->curfilt = f;
		return 1;
	}
	return 0;
}

unsigned
user_func_trackgetcurfilt(struct exec *o, struct data **r) {
	struct songtrk *t;
	
	if (!exec_lookuptrack(o, "trackname", &t)) {
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
user_func_trackcheck(struct exec *o, struct data **r) {
	char *trkname;
	struct songtrk *t;
	
	if (!exec_lookupname(o, "trackname", &trkname)) {
		return 0;
	}
	t = song_trklookup(user_song, trkname);
	if (t == NULL) {
		cons_err("trackcheck: no such track");
		return 0;
	}
	track_check(&t->track);
	return 1;
}

unsigned
user_func_trackcut(struct exec *o, struct data **r) {
	struct songtrk *t;
	long from, amount, quant;
	unsigned tic, len;
	struct track t1, t2;
	struct evspec es;
	
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "quantum", &quant)) {
		return 0;
	}
	tic = song_measuretotic(user_song, from);
	len = song_measuretotic(user_song, from + amount) - tic;

	if (quant < 0 || (unsigned)quant > user_song->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}

	if (tic > (unsigned)quant/2) {
		tic -= quant/2;
	}
	
	evspec_reset(&es);
	track_init(&t1);
	track_init(&t2);
	track_move(&t->track, 0,         tic, &es, &t1, 1, 1);
	track_move(&t->track, tic + len, ~0U, &es, &t2, 1, 1);
	t2.first->delta += tic;
	track_clear(&t->track);
	track_merge(&t->track, &t1);
	track_merge(&t->track, &t2);
	track_done(&t1);
	track_done(&t2);
	return 1;
}

unsigned
user_func_trackblank(struct exec *o, struct data **r) {
	struct songtrk *t;
	struct evspec es;
	long from, amount, quant;
	unsigned tic, len;
	
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "quantum", &quant) ||
	    !exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}
	tic = song_measuretotic(user_song, from);
	len = song_measuretotic(user_song, from + amount) - tic;
	
	if (quant < 0 || (unsigned)quant > user_song->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}

	if (tic > (unsigned)quant/2) {
		tic -= quant/2;
	}
	track_move(&t->track, tic, len, &es, NULL, 0, 1);
	return 1;
}

unsigned
user_func_trackcopy(struct exec *o, struct data **r) {
	struct songtrk *t, *t2;
	struct track copy;
	struct evspec es;
	long from, where, amount, quant;
	unsigned tic, tic2, len;
	
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuptrack(o, "trackname2", &t2) ||
	    !exec_lookuplong(o, "where", &where) ||
	    !exec_lookuplong(o, "quantum", &quant) ||
	    !exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}
	tic  = song_measuretotic(user_song, from);
	len  = song_measuretotic(user_song, from + amount) - tic;
	tic2 = song_measuretotic(user_song, where);

	if (quant < 0 || (unsigned)quant > user_song->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}
	if (tic > (unsigned)quant/2 && tic2 > (unsigned)quant/2) {
		tic -= quant/2;
		tic2 -= quant/2;
	}

	track_init(&copy);
	track_move(&t->track, tic, len, &es, &copy, 1, 0);
	copy.first->delta += tic2;
	track_merge(&t2->track, &copy);
	track_done(&copy);
	return 1;
}

unsigned
user_func_trackinsert(struct exec *o, struct data **r) {
	char *trkname;
	struct songtrk *t;
	long from, amount, quant;
	unsigned tic, len;
	struct track t1, t2;
	struct evspec es;
	
	if (!exec_lookupname(o, "trackname", &trkname) ||
	    !exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "quantum", &quant)) {
		return 0;
	}
	t = song_trklookup(user_song, trkname);
	if (t == NULL) {
		cons_err("trackinsert: no such track");
		return 0;
	}

	tic = song_measuretotic(user_song, from);
	len = song_measuretotic(user_song, from + amount) - tic;

	if (quant < 0 || (unsigned)quant > user_song->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}

	if (tic > (unsigned)quant/2) {
		tic -= quant/2;
	}
	evspec_reset(&es);
	track_init(&t1);
	track_init(&t2);
	track_move(&t->track, 0,   tic, &es, &t1, 1, 1);
	track_move(&t->track, tic, ~0U, &es, &t2, 1, 1);
	t2.first->delta += tic + len;
	track_clear(&t->track);
	track_merge(&t->track, &t1);
	track_merge(&t->track, &t2);
	track_done(&t1);
	track_done(&t2);	     
	return 1;
}

unsigned
user_func_trackmerge(struct exec *o, struct data **r) {
	struct songtrk *src, *dst;

	if (!exec_lookuptrack(o, "source", &src) ||
	    !exec_lookuptrack(o, "dest", &dst)) {
		return 0;
	}
	track_merge(&src->track, &dst->track);
	return 1;
}

unsigned
user_func_trackquant(struct exec *o, struct data **r) {
	char *trkname;
	struct songtrk *t;
	long from, amount;
	unsigned start, len, offset;
	long quant, rate;
	
	if (!exec_lookupname(o, "trackname", &trkname) ||
	    !exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "quantum", &quant) || 
	    !exec_lookuplong(o, "rate", &rate)) {
		return 0;
	}	
	t = song_trklookup(user_song, trkname);
	if (t == NULL) {
		cons_err("trackquant: no such track");
		return 0;
	}
	if (rate > 100) {
		cons_err("trackquant: rate must be between 0 and 100");
		return 0;
	}

	start = song_measuretotic(user_song, from);
	len = song_measuretotic(user_song, from + amount) - start;
	
	if (quant < 0 || (unsigned)quant > user_song->tics_per_unit) {
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
user_func_tracktransp(struct exec *o, struct data **r) {
	struct songtrk *t;
	long from, amount, quant, halftones;
	unsigned tic, len;
	struct evspec es;
	
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookuplong(o, "from", &from) ||
	    !exec_lookuplong(o, "amount", &amount) ||
	    !exec_lookuplong(o, "halftones", &halftones) ||
	    !exec_lookuplong(o, "quantum", &quant) ||
	    !exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}
	tic = song_measuretotic(user_song, from);
	len = song_measuretotic(user_song, from + amount) - tic;
	
	if (quant < 0 || (unsigned)quant > user_song->tics_per_unit) {
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
user_func_tracksetmute(struct exec *o, struct data **r) {
	struct songtrk *t;
	long flag;
	
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookupbool(o, "muteflag", &flag)) {
		return 0;
	}
	t->mute = flag;
	return 1;
}

unsigned
user_func_trackgetmute(struct exec *o, struct data **r) {
	struct songtrk *t;
	
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	*r = data_newlong(t->mute);
	return 1;
}

unsigned
user_func_trackchanlist(struct exec *o, struct data **r) {
	struct songtrk *t;
	struct songchan *c;
	struct data *num;
	char map[DEFAULT_MAXNCHANS];
	unsigned i;
	
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	*r = data_newlist(NULL);
	track_chanmap(&t->track, map);
	for (i = 0; i < DEFAULT_MAXNCHANS; i++) {
		if (map[i]) {
			c = song_chanlookup_bynum(user_song, i / 16, i % 16);
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
user_func_trackinfo(struct exec *o, struct data **r) {
	struct songtrk *t;
	struct seqptr mp, tp;
	struct evspec es;
	struct state *st;
	long quant;
	unsigned len, count, count_next, tpb, bpm;

	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookuplong(o, "quantum", &quant) ||
	    !exec_lookupevspec(o, "evspec", &es)) {
		return 0;
	}

	if (quant < 0 || (unsigned)quant > user_song->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);
	textout_indent(tout);
	
	count_next = 0;
	seqptr_init(&tp, &t->track);
	seqptr_init(&mp, &user_song->meta);
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
			if (st->phase & EV_PHASE_FIRST &&
			    evspec_matchev(&es, &st->ev)) {
				if (len >= quant / 2) 
					count++;
				else
					count_next++;
	                }
		}
		textout_putlong(tout, count);
		textout_putstr(tout, " ");
		if (seqptr_eot(&tp)) {
			if (len < quant / 2) {
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
