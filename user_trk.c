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
 * implements trackxxx built-in functions
 * available through the interpreter
 */

#include "dbg.h"
#include "default.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"

#include "trackop.h"
#include "track.h"
#include "song.h"
#include "user.h"

#include "saveload.h"
#include "textio.h"



unsigned
user_func_tracklist(struct exec_s *o, struct data_s **r) {
	struct data_s *d, *n;
	struct songtrk_s *i;

	d = data_newlist(NULL);
	for (i = user_song->trklist; i != NULL; i = (struct songtrk_s *)i->name.next) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
user_func_tracknew(struct exec_s *o, struct data_s **r) {
	char *trkname;
	struct songtrk_s *t;
	
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
user_func_trackdelete(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
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
user_func_trackrename(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
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
user_func_trackexists(struct exec_s *o, struct data_s **r) {
	char *name;
	struct songtrk_s *t;
	
	if (!exec_lookupname(o, "trackname", &name)) {
		return 0;
	}
	t = song_trklookup(user_song, name);
	*r = data_newlong(t != NULL ? 1 : 0);
	return 1;
}


unsigned
user_func_trackinfo(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	char *name;
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	textout_putstr(tout, "{\n");
	textout_shiftright(tout);
	
	textout_indent(tout);
	textout_putstr(tout, "curfilt ");
	/* warning 'cond ? val1 : val2' is a 'const char *' in gcc */
	if (t->curfilt) {
		name =  t->curfilt->name.str;
	} else {
		name = "nil";
	}
	textout_putstr(tout, name);
	textout_putstr(tout, "\n");
	if (t->mute) {
		textout_indent(tout);
		textout_putstr(tout, "mute\n");
	}
	textout_shiftleft(tout);
	textout_putstr(tout, "}\n");
	return 1;
}



unsigned
user_func_trackaddev(struct exec_s *o, struct data_s **r) {
	long measure, beat, tic;
	struct ev_s ev;
	struct seqptr_s tp;
	struct songtrk_s *t;
	unsigned pos, bpm, tpb;
	unsigned long dummy;

	if (!exec_lookuptrack(o, "trackname", &t) || 
	    !exec_lookuplong(o, "measure", &measure) || 
	    !exec_lookuplong(o, "beat", &beat) || 
	    !exec_lookuplong(o, "tic", &tic) || 
	    !exec_lookupev(o, "event", &ev)) {
		return 0;
	}

	pos = track_opfindtic(&user_song->meta, measure);
	track_optimeinfo(&user_song->meta, pos, &dummy, &bpm, &tpb);
	
	if (beat < 0 || (unsigned)beat >= bpm || 
	    tic  < 0 || (unsigned)tic  >= tpb) {
		cons_err("beat and tic must fit in the selected measure");
		return 0;
	}

	pos += beat * tpb + tic;	

	track_rew(&t->track, &tp);
	track_seekblank(&t->track, &tp, pos);
	track_evlast(&t->track, &tp);
	track_evput(&t->track, &tp, &ev);
	return 1;
}


unsigned
user_func_tracksetcurfilt(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	struct songfilt_s *f;
	struct var_s *arg;
	
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
user_func_trackgetcurfilt(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	
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
user_func_trackcheck(struct exec_s *o, struct data_s **r) {
	char *trkname;
	struct songtrk_s *t;
	
	if (!exec_lookupname(o, "trackname", &trkname)) {
		return 0;
	}
	t = song_trklookup(user_song, trkname);
	if (t == NULL) {
		cons_err("trackcheck: no such track");
		return 0;
	}
	track_opcheck(&t->track);
	return 1;
}


unsigned
user_func_trackgetlen(struct exec_s *o, struct data_s **r) {
	char *trkname;
	struct songtrk_s *t;
	unsigned len;
	
	if (!exec_lookupname(o, "trackname", &trkname)) {
		return 0;
	}
	t = song_trklookup(user_song, trkname);
	if (t == NULL) {
		cons_err("trackgetlen: no such track");
		return 0;
	}
	len = track_numtic(&t->track);
	*r = data_newlong((long)len);
	return 1;
}


unsigned
user_func_trackcut(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	long from, amount, quant;
	unsigned tic, len;
	struct seqptr_s op;
	
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

	track_rew(&t->track, &op);
	track_seek(&t->track, &op, tic);
	track_opcut(&t->track, &op, len);
	return 1;
}


unsigned
user_func_trackblank(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	struct track_s null;
	struct seqptr_s tp;
	struct evspec_s es;
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

	track_init(&null);
	track_rew(&t->track, &tp);
	track_seek(&t->track, &tp, tic);
	track_opextract(&t->track, &tp, len, &null, &es);
	track_done(&null);
	return 1;
}


unsigned
user_func_trackcopy(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t, *t2;
	struct track_s null, null2;
	struct seqptr_s tp, tp2;
	struct evspec_s es;
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

	track_init(&null);
	track_init(&null2);
	track_rew(&t->track, &tp);
	track_seek(&t->track, &tp, tic);
	track_rew(&t2->track, &tp2);
	track_seekblank(&t2->track, &tp2, tic2);
	track_opextract(&t->track, &tp, len, &null, &es);
	track_framecp(&null, &null2);
	track_frameins(&t->track, &tp, &null);
	track_frameins(&t2->track, &tp2, &null2);
	track_done(&null);
	return 1;
}


unsigned
user_func_trackinsert(struct exec_s *o, struct data_s **r) {
	char *trkname;
	struct songtrk_s *t;
	struct seqptr_s tp;
	long from, amount, quant;
	unsigned tic, len;
	
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

	track_rew(&t->track, &tp);
	track_seekblank(&t->track, &tp, tic);
	track_opinsert(&t->track, &tp, len);
	return 1;
}


unsigned
user_func_trackquant(struct exec_s *o, struct data_s **r) {
	char *trkname;
	struct songtrk_s *t;
	struct seqptr_s tp;
	long from, amount;
	unsigned tic, len, first;
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

	tic = song_measuretotic(user_song, from);
	len = song_measuretotic(user_song, from + amount) - tic;
	
	if (quant < 0 || (unsigned)quant > user_song->tics_per_unit) {
		cons_err("quantum must be between 0 and tics_per_unit");
		return 0;
	}

	if (tic > (unsigned)quant / 2) {
		tic -= quant / 2;
		first = quant / 2;
	} else {
		first = 0;
		len -= quant / 2;
	}
	
	track_rew(&t->track, &tp);
	track_seek(&t->track, &tp, tic);
	track_opquantise(&t->track, &tp, first, len, (unsigned)quant, (unsigned)rate);
	return 1;
}

unsigned
user_func_tracksetmute(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	long flag;
	
	if (!exec_lookuptrack(o, "trackname", &t) ||
	    !exec_lookupbool(o, "muteflag", &flag)) {
		return 0;
	}
	t->mute = flag;
	return 1;
}

unsigned
user_func_trackgetmute(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	*r = data_newlong(t->mute);
	return 1;
}

unsigned
user_func_trackchanlist(struct exec_s *o, struct data_s **r) {
	struct songtrk_s *t;
	struct songchan_s *c;
	struct data_s *num;
	char map[DEFAULT_MAXNCHANS];
	unsigned i;
	
	if (!exec_lookuptrack(o, "trackname", &t)) {
		return 0;
	}
	*r = data_newlist(NULL);
	track_opchaninfo(&t->track, map);
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

