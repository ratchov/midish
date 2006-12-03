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
 * implements chanxxx built-in functions
 * available through the interpreter
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
user_func_chanlist(struct exec *o, struct data **r) {
	struct data *d, *n;
	struct songchan *i;

	d = data_newlist(NULL);
	for (i = user_song->chanlist; i != NULL; i = (struct songchan *)i->name.next) {
		n = data_newref(i->name.str);
		data_listadd(d, n);
	}
	*r = d;
	return 1;
}

unsigned
user_func_channew(struct exec *o, struct data **r) {
	char *name;
	struct songchan *i;
	unsigned dev, ch;
	
	if (!exec_lookupname(o, "channame", &name) ||
	    !exec_lookupchan_getnum(o, "channum", &dev, &ch)) {
		return 0;
	}
	i = song_chanlookup(user_song, name);
	if (i != NULL) {
		cons_err("channew: chan already exists");
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
user_func_chandelete(struct exec *o, struct data **r) {
	struct songchan *c;
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
user_func_chanrename(struct exec *o, struct data **r) {
	struct songchan *c;
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
user_func_chanexists(struct exec *o, struct data **r) {
	struct songchan *i;
	unsigned dev, ch;
	
	if (!exec_lookupchan_getnum(o, "channame", &dev, &ch)) {
		return 0;
	}
	i = song_chanlookup_bynum(user_song, dev, ch);
	*r = data_newlong(i != NULL ? 1 : 0);
	return 1;
}


unsigned
user_func_chanset(struct exec *o, struct data **r) {
	struct songchan *c, *i;
	unsigned dev, ch;
	
	if (!exec_lookupchan_getref(o, "channame", &c) ||
	    !exec_lookupchan_getnum(o, "channum", &dev, &ch)) {
		return 0;
	}
	i = song_chanlookup_bynum(user_song, dev, ch);
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
user_func_changetch(struct exec *o, struct data **r) {
	struct songchan *i;
	
	if (!exec_lookupchan_getref(o, "channame", &i)) {
		return 0;
	}
	*r = data_newlong(i->ch);
	return 1;
}

unsigned
user_func_changetdev(struct exec *o, struct data **r) {
	struct songchan *i;
	
	if (!exec_lookupchan_getref(o, "channame", &i)) {
		return 0;
	}
	*r = data_newlong(i->dev);
	return 1;
}

unsigned
user_func_chanconfev(struct exec *o, struct data **r) {
	struct songchan *c;
	struct ev ev;
	
	if (!exec_lookupchan_getref(o, "channame", &c) ||
	    !exec_lookupev(o, "event", &ev)) {
		return 0;
	}
	if (ev.data.voice.ch != c->ch || ev.data.voice.dev != c->dev) {
		cons_err("chanconfev: dev/chan mismatch in event spec");
		return 0;
	}
	track_confev(&c->conf, &ev);
	return 1;
}

unsigned
user_func_chaninfo(struct exec *o, struct data **r) {
	struct songchan *c;
	
	if (!exec_lookupchan_getref(o, "channame", &c)) {
		return 0;
	}
	track_output(&c->conf, tout);
	textout_putstr(tout, "\n");
	return 1;
}

unsigned
user_func_chansetcurinput(struct exec *o, struct data **r) {
	unsigned dev, ch;
	struct songchan *c;
	struct data *l;
	
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
user_func_changetcurinput(struct exec *o, struct data **r) {
	struct songchan *c;
	
	if (!exec_lookupchan_getref(o, "channame", &c)) {
		return 0;
	}
	*r = data_newlist(NULL);
	data_listadd(*r, data_newlong(c->curinput_dev));
	data_listadd(*r, data_newlong(c->curinput_ch));
	return 1;
}
