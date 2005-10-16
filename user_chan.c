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
 * implements chanxxx built-in functions
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
user_func_chanlist(struct exec_s *o, struct data_s **r) {
	struct data_s *d, *n;
	struct songchan_s *i;

	d = data_newlist(NULL);
	for (i = user_song->chanlist; i != NULL; i = (struct songchan_s *)i->name.next) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
user_func_channew(struct exec_s *o, struct data_s **r) {
	char *name;
	struct var_s *arg;
	struct songchan_s *i;
	unsigned dev, ch;
	
	if (!exec_lookupname(o, "channame", &name)) {
		return 0;
	}
	i = song_chanlookup(user_song, name);
	if (i != NULL) {
		cons_err("channew: chan already exists");
		return 0;
	}
	arg = exec_varlookup(o, "channum");
	if (!arg) {
		dbg_puts("exec_lookupchan: no such var\n");
		dbg_panic();
	}
	if (!data_list2chan(arg->data, &dev, &ch)) {
		return 0;
	}
	i = song_chanlookup_bynum(user_song, dev, ch);
	if (i != NULL) {
		cons_errs(i->name.str, "dev/chan number already used");
		return 0;
	}
	
	i = songchan_new(name);
	if (dev > EV_MAXDEV || ch > EV_MAXCH) {
		cons_err("channew: dev/chan number out of bounds");
		return 0;
	}
	i->dev = dev;
	i->ch = ch;
	song_chanadd(user_song, i);
	return 1;
}

unsigned
user_func_chandelete(struct exec_s *o, struct data_s **r) {
	struct songchan_s *c;
	if (!exec_lookupchan_getref(o, "channame", &c)) {
		return 0;
	}
	if (!song_chanrm(user_song, c)) {
		return 0;
	}
	songchan_delete(c);
	return 1;
}

unsigned
user_func_chanrename(struct exec_s *o, struct data_s **r) {
	struct songchan_s *c;
	char *name;
	
	if (!exec_lookupchan_getref(o, "channame", &c) ||
	    !exec_lookupname(o, "newname", &name)) {
		return 0;
	}
	if (song_chanlookup(user_song, name)) {
		cons_err("name already used by another chan");
		return 0;
	}
	str_delete(c->name.str);
	c->name.str = str_new(name);
	return 1;
}

unsigned
user_func_chanexists(struct exec_s *o, struct data_s **r) {
	char *name;
	struct songchan_s *i;
	if (!exec_lookupname(o, "channame", &name)) {
		return 0;
	}
	i = song_chanlookup(user_song, name);
	*r = data_newlong(i != NULL ? 1 : 0);
	return 1;
}

unsigned
user_func_changetch(struct exec_s *o, struct data_s **r) {
	struct songchan_s *i;
	
	if (!exec_lookupchan_getref(o, "channame", &i)) {
		return 0;
	}
	*r = data_newlong(i->ch);
	return 1;
}

unsigned
user_func_changetdev(struct exec_s *o, struct data_s **r) {
	struct songchan_s *i;
	
	if (!exec_lookupchan_getref(o, "channame", &i)) {
		return 0;
	}
	*r = data_newlong(i->dev);
	return 1;
}

unsigned
user_func_chanconfev(struct exec_s *o, struct data_s **r) {
	struct songchan_s *c;
	struct seqptr_s cp;
	struct ev_s ev;
	
	if (!exec_lookupchan_getref(o, "channame", &c) ||
	    !exec_lookupev(o, "event", &ev)) {
		return 0;
	}
	if (ev.data.voice.ch != c->ch || ev.data.voice.dev != c->dev) {
		cons_err("chanconfev: mismatch dev/chan in event spec");
		return 0;
	}
	track_rew(&c->conf, &cp);
	track_evlast(&c->conf, &cp);
	track_evput(&c->conf, &cp, &ev);
	track_opcheck(&c->conf);
	return 1;
}

unsigned
user_func_chaninfo(struct exec_s *o, struct data_s **r) {
	struct songchan_s *c;
	
	if (!exec_lookupchan_getref(o, "channame", &c)) {
		return 0;
	}
	track_output(&c->conf, tout);
	textout_putstr(tout, "\n");
	return 1;
}

unsigned
user_func_chansetcurinput(struct exec_s *o, struct data_s **r) {
	unsigned dev, ch;
	struct songchan_s *c;
	struct data_s *l;
	
	if (!exec_lookupchan_getref(o, "channame", &c) ||
	    !exec_lookuplist(o, "inputchan", &l)) {
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
user_func_changetcurinput(struct exec_s *o, struct data_s **r) {
	struct songchan_s *c;
	
	if (!exec_lookupchan_getref(o, "channame", &c)) {
		return 0;
	}
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(c->curinput_dev));
	data_listadd(*r, data_newlong(c->curinput_ch));
	return 1;
}
