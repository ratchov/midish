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
 * implements misc built-in functions
 * available through the interpreter
 */

#include "dbg.h"
#include "default.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"

#include "textio.h"
#include "lex.h"
#include "parse.h"

#include "mux.h"
#include "mididev.h"

#include "trackop.h"
#include "track.h"
#include "song.h"
#include "user.h"
#include "smf.h"
#include "saveload.h"
#include "rmidi.h"	/* for rmidi_debug */

struct song_s *user_song;
unsigned user_flag_batch = 0;
unsigned user_flag_verb = 0;

/* -------------------------------------------------- some tools --- */

	/*
	 * execute a script from a file in the 'exec' environnement
	 * the script has acces to the global variables, but
	 * not to the local variables of the calling proc. Thus
	 * it can be safely used in a procedure
	 */

unsigned
exec_runfile(struct exec_s *exec, char *filename) {
	struct parse_s *parse;
	struct var_s **locals;
	struct node_s *root;
	struct data_s *data;
	unsigned res;
	
	res = 0;
	root = NULL;
	data = NULL;
	parse = parse_new(filename);	
	if (!parse) {
		return 0;
	}
	locals = exec->locals;
	exec->locals = &exec->globals;
	if (parse_prog(parse, &root)) {
		/*node_dbg(root, 0);*/
		res = node_exec(root, exec, &data);
	}
	exec->locals = locals;
	parse_delete(parse);
	node_delete(root);
	return res;
}

	/*
	 * find the pointer to the songtrk contained in 'var'
	 * ('var' must be a reference)
	 */

unsigned
exec_lookuptrack(struct exec_s *o, char *var, struct songtrk_s **res) {
	char *name;	
	struct songtrk_s *t;
	if (!exec_lookupname(o, var, &name)) {
		return 0;
	}
	t = song_trklookup(user_song, name);
	if (t == NULL) {
		cons_errs(name, "no such track");
		return 0;
	}
	*res = t;
	return 1;
}


	/*
	 * find the (dev, ch) couple for the channel referenced by 'var'
	 * 'var' can be 
	 *	- a reference to a songchan
	 *	- a list of two integers (like '{dev chan}')
	 */

unsigned
exec_lookupchan_getnum(struct exec_s *o, char *var, 
    unsigned *dev, unsigned *ch) {
	struct var_s *arg;
	
	arg = exec_varlookup(o, var);
	if (!arg) {
		dbg_puts("exec_lookupchan_getnum: no such var\n");
		dbg_panic();
	}
	if (!data_list2chan(arg->data, dev, ch)) {
		return 0;
	}
	return 1;
}

	/*
	 * find the pointer to an existing songchan
	 * that is referenced by 'var'.
	 * ('var' must be a reference)
	 */

unsigned
exec_lookupchan_getref(struct exec_s *o, char *var, struct songchan_s **res) {
	struct var_s *arg;
	struct songchan_s *i;
	
	arg = exec_varlookup(o, var);
	if (!arg) {
		dbg_puts("exec_lookupchan: no such var\n");
		dbg_panic();
	}
	if (arg->data->type == DATA_REF) {
		i = song_chanlookup(user_song, arg->data->val.ref);
	} else {
		cons_err("bad channel name");
		return 0;
	}
	if (i == NULL) {
		cons_errs(arg->data->val.ref, "no such chan");
		return 0;
	}
	*res = i;
	return 1;
}


	/*
	 * find the pointer to the songfilt contained in 'var'
	 * ('var' must be a reference)
	 */

unsigned
exec_lookupfilt(struct exec_s *o, char *var, struct songfilt_s **res) {
	char *name;	
	struct songfilt_s *f;
	if (!exec_lookupname(o, var, &name)) {
		return 0;
	}
	f = song_filtlookup(user_song, name);
	if (f == NULL) {
		cons_errs(name, "no such filt");
		return 0;
	}
	*res = f;
	return 1;
}

	/*
	 * find the pointer to the songsx contained in 'var'
	 * ('var' must be a reference)
	 */

unsigned
exec_lookupsx(struct exec_s *o, char *var, struct songsx_s **res) {
	char *name;	
	struct songsx_s *t;
	if (!exec_lookupname(o, var, &name)) {
		return 0;
	}
	t = song_sxlookup(user_song, name);
	if (t == NULL) {
		cons_errs(name, "no such sysex");
		return 0;
	}
	*res = t;
	return 1;
}

	/*
	 * fill the event with the one referenced by 'var'
	 * 'var' can be:
	 * 	- { noff chan xxx yyy }
	 * 	- { non chan xxx yyy }
	 * 	- { ctl chan xxx yyy }
	 * 	- { kat chan xxx yyy }
	 * 	- { cat chan xxx }
	 * 	- { pc chan xxx }
	 * 	- { bend chan xxx }
	 * where 'chan' is in the same as in lookupchan_getnum
	 * and 'xxx' and 'yyy' are integers
	 */

unsigned
exec_lookupev(struct exec_s *o, char *name, struct ev_s *ev) {
	struct var_s *arg;
	struct data_s *d;
	unsigned dev, ch;

	arg = exec_varlookup(o, name);
	if (!arg) {
		dbg_puts("exec_lookupev: no such var\n");
		dbg_panic();
	}
	d = arg->data;

	if (d->type != DATA_LIST) {
		cons_err("event spec must be a list");
		return 0;
	}
	d = d->val.list;
	if (!d || d->type != DATA_REF || 
	    !ev_str2cmd(ev, d->val.ref) ||
	    !EV_ISVOICE(ev)) {
		cons_err("bad status in event spec");
		return 0;
	}
	d = d->next;
	if (!d) {
		cons_err("no channel in event spec");
		return 0;
	}
	if (!data_list2chan(d, &dev, &ch)) {
		return 0;
	}
	ev->data.voice.dev = dev;
	ev->data.voice.ch = ch;
	d = d->next;
	if (!d || d->type != DATA_LONG) {
		cons_err("bad byte0 in event spec");
		return 0;
	}
	if (ev->cmd == EV_BEND) {
		EV_SETBEND(ev, d->val.num);
	} else {
		ev->data.voice.b0 = d->val.num;
	}		
	d = d->next;
	if (ev->cmd != EV_PC && ev->cmd != EV_CAT && ev->cmd != EV_BEND) {
		if (!d || d->type != DATA_LONG) {
			cons_err("bad byte1 in event spec");
			return 0;
		}
		ev->data.voice.b1 = d->val.num;
	} else {
		if (d) {
			cons_err("extra data in event spec");
			return 0;
		}
	}
	return 1;
}

	/* 
	 * fill the evspec with the one referenced by var.
	 * var is of this form
	 * 	- { [type [chanrange [xxxrange [yyyrange]]]] }	
	 * (brackets mean optionnal)
	 */

unsigned
exec_lookupevspec(struct exec_s *o, char *name, struct evspec_s *e) {
	struct var_s *arg;
	struct data_s *d;
	struct songchan_s *i;
	unsigned lo, hi;

	arg = exec_varlookup(o, name);
	if (!arg) {
		dbg_puts("exec_lookupev: no such var\n");
		dbg_panic();
	}
	d = arg->data;
	if (d->type != DATA_LIST) {
		cons_err("list expected in event range spec");
		return 0;
	}

	/* default match any event */
	evspec_reset(e);

	d = d->val.list;
	if (!d) {
		/* empty list match anything */
		return 1;
	}
	if (d->type != DATA_REF ||
	    !evspec_str2cmd(e, d->val.ref)) {
		cons_err("bad status in event spec");
		return 0;
	}
	
	d = d->next;
	if (!d) {
		/* 
		 * match events of the given type (or 'any')
		 * on any channel, with any value(s)
		 */
		return 1;
	}
	if (d->type == DATA_REF) {
		i = song_chanlookup(user_song, d->val.ref);
		if (i == NULL) {
			cons_err("no such chan name");
			return 0;
		}
		e->dev_min = e->dev_max = i->dev;
		e->ch_min = e->ch_max = i->ch;
	} else if (d->type == DATA_LIST) {
		if (!d->val.list) {		/* empty list = any chan/dev */
			/* nothing */
		} else if (d->val.list && 
		    d->val.list->next && 
		    !d->val.list->next->next) {
			if (!data_list2range(d->val.list, 0, EV_MAXDEV, &lo, &hi)) {
				return 0;
			}
			e->dev_min = lo;
			e->dev_max = hi;
			if (!data_list2range(d->val.list->next, 0, EV_MAXCH, &lo, &hi)) {
				return 0;
			}
			e->ch_min = lo;
			e->ch_max = hi;	
		} else {
			cons_err("bad channel range spec");
			return 0;
		}
	}

	d = d->next;
	if (!d) {
		/* match events of the given type (or 'any' 
		 * with the given channel range but with any value(s) */ 
		return 1;
	}
	if (e->cmd == EVSPEC_ANY) {
		goto toomany;
	}
	if (e->cmd == EVSPEC_BEND) {
		if (!data_list2range(d, 0, EV_MAXBEND, &lo, &hi)) {
			return 0;
		}
		e->b0_min = lo;
		e->b0_max = hi;
	} else {
		if (!data_list2range(d, 0, EV_MAXB0, &lo, &hi)) {
			return 0;
		}
		e->b0_min = lo;
		e->b0_max = hi;
	}
	d = d->next;
	if (!d) {
		return 1;
	}
	if (e->cmd != EVSPEC_PC && 
	    e->cmd != EVSPEC_CAT && 
	    e->cmd != EVSPEC_BEND) {
		if (!data_list2range(d, 0, EV_MAXB1, &lo, &hi)) {
			return 0;
		}
		e->b1_min = lo;
		e->b1_max = hi;
		d = d->next;
		if (!d) {
			return 1;
		}
	}
toomany:
	cons_err("too many ranges in event spec");
	return 0;				
}

	/*
	 * print a data_s to the user console
	 */

void
data_print(struct data_s *d) {
	struct data_s *i;
	
	switch(d->type) {
	case DATA_NIL:
		textout_putstr(tout, "(nil)");
		break;
	case DATA_LONG:
		textout_putlong(tout, d->val.num);
		break;
	case DATA_STRING:
		textout_putstr(tout, d->val.str);
		break;
	case DATA_REF:
		textout_putstr(tout, d->val.ref);
		break;
	case DATA_LIST:
		for (i = d->val.list; i != NULL; i = i->next) {
			data_print(i);
			if (i->next) {
				textout_putstr(tout, " ");
			}
		}
		break;
	default:
		dbg_puts("data_print: unknown type\n");
		break;
	}
}

	/*
	 * convert lists to channels, 'data' can be
	 * 	- a reference to an existing songchan
	 *	- a pair of integers '{ dev midichan }'
	 */
	 
unsigned
data_list2chan(struct data_s *o, unsigned *res_dev, unsigned *res_ch) {
	struct songchan_s *i;
	long dev, ch;

	if (o->type == DATA_LIST) {
		if (!o->val.list || 
		    !o->val.list->next || 
		    o->val.list->next->next ||
		    o->val.list->type != DATA_LONG || 
		    o->val.list->next->type != DATA_LONG) {
			cons_err("bad {dev midichan} in spec");
			return 0;
		}
		dev = o->val.list->val.num;
		ch = o->val.list->next->val.num;
		if (ch < 0 || ch > EV_MAXCH || 
		    dev < 0 || dev > EV_MAXDEV) {
			cons_err("bad dev/midichan ranges");
			return 0;
		}
		*res_dev = dev;
		*res_ch = ch;
		return 1;
	} else if (o->type == DATA_REF) {
		i = song_chanlookup(user_song, o->val.ref);
		if (i == NULL) {
			cons_errs(o->val.ref, "no such chan name");
			return 0;
		}
		*res_dev = i->dev;
		*res_ch = i->ch;
		return 1;
	} else {
		cons_err("bad channel specification");
		return 0;
	}
}


	/*
	 * convert a data_s to a pair of integers
	 * data can be:
	 * 	- a liste of 2 integers
	 *	- a single integer (then min = max)
	 */
	 
	 
unsigned
data_list2range(struct data_s *d, unsigned min, unsigned max, 
    unsigned *lo, unsigned *hi) {
    	if (d->type == DATA_LONG) {
		*lo = *hi = d->val.num;
	} else if (d->type == DATA_LIST) {
		d = d->val.list;
		if (!d) {
			*lo = min;
			*hi = max;
			return 1;
		} 
		if (!d->next || d->next->next || 
		    d->type != DATA_LONG || d->next->type != DATA_LONG) {
			cons_err("exactly 0 ore 2 numbers expected in range spec");
			return 0;
		}
		*lo = d->val.num;
		*hi = d->next->val.num;
	} else {
		cons_err("list or number expected in range spec");
		return 0;
	}
	if (*lo < min || *lo > max || *hi < min || *hi > max || *lo > *hi) {
		cons_err("range values out of bounds");
		return 0;
	}
	return 1;
}

	/*
	 * check if the pattern in data (list of integers)
	 * match the beggining of the given sysex
	 */

unsigned
data_matchsysex(struct data_s *d, struct sysex_s *sx, unsigned *res) {
	unsigned i;
	struct chunk_s *ck;
	
	i = 0;
	ck = sx->first;
	while (d) {
		if (d->type != DATA_LONG) {
			cons_err("not-a-number in sysex pattern");
			return 0;
		}
		for (;;) {
			if (!ck) {
				*res = 0;
				return 1;
			} 
			if (i < ck->used) {
				break;
			}
			ck = ck->next;
			i = 0;
		}		
		if (d->val.num != ck->data[i++]) {
			*res = 0;
			return 1;
		}
		d = d->next;	
	}
	*res = 1;
	return 1;
}


/* ---------------------------------------- interpreter functions --- */

	/* XXX: for testing */

unsigned
user_func_ev(struct exec_s *o, struct data_s **r) {
	struct evspec_s ev;
	if (!exec_lookupevspec(o, "ev", &ev)) {
		return 0;
	}
	evspec_dbg(&ev);
	dbg_puts("\n");
	return 1;
}


unsigned
user_func_panic(struct exec_s *o, struct data_s **r) {
	dbg_panic();
	/* not reached */
	return 0;
}


unsigned
user_func_debug(struct exec_s *o, struct data_s **r) {
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
	} else {
		cons_err("debug: unknuwn debug-flag");
		return 0;
	}
	return 1;
}

unsigned
user_func_exec(struct exec_s *o, struct data_s **r) {
	char *filename;		
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}	
	return exec_runfile(o, filename);
}


unsigned
user_func_print(struct exec_s *o, struct data_s **r) {
	struct var_s *arg;
	arg = exec_varlookup(o, "value");
	if (!arg) {
		dbg_puts("user_func_print: 'value': no such param\n");
		return 0;
	}
	data_print(arg->data);
	textout_putstr(tout, "\n");
	*r = data_newnil();
	return 1;
}

unsigned
user_func_info(struct exec_s *o, struct data_s **r) {
	exec_dumpprocs(o);
	exec_dumpvars(o);
	return 1;
}

unsigned
user_func_metroswitch(struct exec_s *o, struct data_s **r) {
	long onoff;
	if (!exec_lookuplong(o, "onoff", &onoff)) {
		return 0;
	}	
	user_song->metro_enabled = onoff;
	return 1;
}

unsigned
user_func_metroconf(struct exec_s *o, struct data_s **r) {
	struct ev_s evhi, evlo;
	if (!exec_lookupev(o, "eventhi", &evhi) ||
	    !exec_lookupev(o, "eventlo", &evlo)) {
		return 0;
	}
	if (evhi.cmd != EV_NON && evlo.cmd != EV_NON) {
		cons_err("note-on event expected");
		return 0;
	}
	user_song->metro_hi = evhi;
	user_song->metro_lo = evlo;
	return 1;
}

unsigned
user_func_shut(struct exec_s *o, struct data_s **r) {
	unsigned i;
	struct ev_s ev;
	struct mididev_s *dev;

	mux_init(NULL, NULL);

	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		for (i = 0; i < EV_MAXCH; i++) {
			ev.cmd = EV_CTL;
			ev.data.voice.dev = dev->unit;		
			ev.data.voice.ch = i;
			ev.data.voice.b0 = 121;
			ev.data.voice.b1 = 0;
			mux_putev(&ev);
			ev.cmd = EV_CTL;		
			ev.data.voice.dev = dev->unit;		
			ev.data.voice.ch = i;
			ev.data.voice.b0 = 123;
			ev.data.voice.b1 = 0;
			mux_putev(&ev);
			ev.cmd = EV_BEND;
			ev.data.voice.dev = dev->unit;		
			ev.data.voice.ch = i;
			ev.data.voice.b0 = EV_BEND_DEFAULTLO;
			ev.data.voice.b1 = EV_BEND_DEFAULTHI;
		}
	}
	
	mux_flush();
	mux_done();
	return 1;
}

unsigned
user_func_sendraw(struct exec_s *o, struct data_s **r) {
	struct var_s *arg;
	struct data_s *i;
	unsigned char byte;
	long device;
	
	arg = exec_varlookup(o, "list");
	if (!arg) {
		dbg_puts("user_func_sendraw: 'list': no such param\n");
		dbg_panic();
	}
	if (arg->data->type != DATA_LIST) {
		cons_err("argument must be a list");
		return 0;
	}
	if (!exec_lookuplong(o, "device", &device)) {
		return 0;
	}
	if (device < 0 || device >= DEFAULT_MAXNDEVS) {
		cons_err("sendraw: device out of range");
		return 0;
	}
	for (i = arg->data->val.list; i != NULL; i = i->next) {
		if (i->type != DATA_LONG || i->val.num < 0 || i->val.num > 255) {
			cons_err("list elements must be integers in 0..255");
			return 0;
		}
	}
	
	mux_init(NULL, NULL);
	for (i = arg->data->val.list; i != NULL; i = i->next) {
		byte = i->val.num;
		mux_sendraw(device, &byte, 1);
	}
	mux_flush();
	mux_done();
	return 1;
}


void
user_mainloop(void) {
	struct parse_s *parse;
	struct exec_s *exec;
	struct node_s *root;
	struct data_s *data;
	
	user_song = song_new();
	exec = exec_new();

	exec_newbuiltin(exec, "ev", user_func_ev, 
			name_newarg("ev", NULL));
	exec_newbuiltin(exec, "print", user_func_print, 
			name_newarg("value", NULL));
	exec_newbuiltin(exec, "exec", user_func_exec, 
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "debug", user_func_debug,
			name_newarg("flag", 
			name_newarg("value", NULL)));
	exec_newbuiltin(exec, "panic", user_func_panic, NULL);
	exec_newbuiltin(exec, "info", user_func_info, NULL);
		
	exec_newbuiltin(exec, "tracklist", user_func_tracklist, NULL);
	exec_newbuiltin(exec, "tracknew", user_func_tracknew, 
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackdelete", user_func_trackdelete, 
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackrename", user_func_trackrename,
			name_newarg("trackname",
			name_newarg("newname", NULL)));
	exec_newbuiltin(exec, "trackexists", user_func_trackexists, 
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackinfo", user_func_trackinfo, 
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackaddev", user_func_trackaddev,
			name_newarg("trackname", 
			name_newarg("measure", 
			name_newarg("beat", 
			name_newarg("tic", 
			name_newarg("event", NULL))))));
	exec_newbuiltin(exec, "tracksetcurfilt", user_func_tracksetcurfilt, 
			name_newarg("trackname", 
			name_newarg("filtname", NULL)));
	exec_newbuiltin(exec, "trackgetcurfilt", user_func_trackgetcurfilt,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackcheck", user_func_trackcheck,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackgetlen", user_func_trackgetlen, 
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackcut", user_func_trackcut,
			name_newarg("trackname", 
			name_newarg("from", 
			name_newarg("amount", 
			name_newarg("quantum", NULL)))));
	exec_newbuiltin(exec, "trackblank", user_func_trackblank,
			name_newarg("trackname", 
			name_newarg("from", 
			name_newarg("amount", 
			name_newarg("quantum", 
			name_newarg("evspec", NULL))))));
	exec_newbuiltin(exec, "trackcopy", user_func_trackcopy,
			name_newarg("trackname", 
			name_newarg("from", 
			name_newarg("amount", 
			name_newarg("trackname2", 
			name_newarg("where", 
			name_newarg("quantum", 
			name_newarg("evspec",  NULL))))))));
	exec_newbuiltin(exec, "trackinsert", user_func_trackinsert,
			name_newarg("trackname", 
			name_newarg("from", 
			name_newarg("amount", 
			name_newarg("quantum", NULL)))));
	exec_newbuiltin(exec, "trackquant", user_func_trackquant, 
			name_newarg("trackname", 
			name_newarg("from", 
			name_newarg("amount", 
			name_newarg("rate", 
			name_newarg("quantum", NULL))))));
	exec_newbuiltin(exec, "tracksetmute", user_func_tracksetmute,
			name_newarg("trackname",
			name_newarg("muteflag", NULL)));
	exec_newbuiltin(exec, "trackgetmute", user_func_trackgetmute,
			name_newarg("trackname", NULL));

	exec_newbuiltin(exec, "chanlist", user_func_chanlist, NULL);
	exec_newbuiltin(exec, "channew", user_func_channew, 
			name_newarg("channame",
			name_newarg("channum", NULL)));
	exec_newbuiltin(exec, "chandelete", user_func_chandelete, 
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "chanrename", user_func_chanrename,
			name_newarg("channame",
			name_newarg("newname", NULL)));
	exec_newbuiltin(exec, "chanexists", user_func_chanexists, 
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "chaninfo", user_func_chaninfo, 
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "changetch", user_func_changetch,
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "changetdev", user_func_changetdev,
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "chanconfev", user_func_chanconfev,
			name_newarg("channame",
			name_newarg("event", NULL)));

	exec_newbuiltin(exec, "sysexlist", user_func_sysexlist, NULL);
	exec_newbuiltin(exec, "sysexnew", user_func_sysexnew, 
			name_newarg("sysexname", NULL));
	exec_newbuiltin(exec, "sysexdelete", user_func_sysexdelete, 
			name_newarg("sysexname", NULL));
	exec_newbuiltin(exec, "sysexrename", user_func_sysexrename,
			name_newarg("sysexname",
			name_newarg("newname", NULL)));
	exec_newbuiltin(exec, "sysexexists", user_func_sysexexists, 
			name_newarg("sysexname", NULL));
	exec_newbuiltin(exec, "sysexinfo", user_func_sysexinfo, 
			name_newarg("sysexname", NULL));
	exec_newbuiltin(exec, "sysexclear", user_func_sysexclear, 
			name_newarg("sysexname", 
			name_newarg("data", NULL)));
	exec_newbuiltin(exec, "sysexsetunit", user_func_sysexsetunit, 
			name_newarg("sysexname", 
			name_newarg("unit", 
			name_newarg("data", NULL))));
	exec_newbuiltin(exec, "sysexadd", user_func_sysexadd, 
			name_newarg("sysexname", 
			name_newarg("unit",
			name_newarg("data", NULL))));


	exec_newbuiltin(exec, "filtlist", user_func_filtlist, NULL);
	exec_newbuiltin(exec, "filtnew", user_func_filtnew, 
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "filtdelete", user_func_filtdelete, 
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "filtrename", user_func_filtrename,
			name_newarg("filtname",
			name_newarg("newname", NULL)));
	exec_newbuiltin(exec, "filtinfo", user_func_filtinfo, 
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "filtexists", user_func_filtexists, 
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "filtreset", user_func_filtreset, 
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "filtdevdrop", user_func_filtdevdrop,
			name_newarg("filtname",
			name_newarg("indev", NULL)));
	exec_newbuiltin(exec, "filtnodevdrop", user_func_filtnodevdrop,
			name_newarg("filtname",
			name_newarg("indev", NULL)));
	exec_newbuiltin(exec, "filtdevmap", user_func_filtdevmap,
			name_newarg("filtname",
			name_newarg("indev", 
			name_newarg("outdev", NULL))));
	exec_newbuiltin(exec, "filtnodevmap", user_func_filtnodevmap,
			name_newarg("filtname",
			name_newarg("outdev", NULL)));
	exec_newbuiltin(exec, "filtchandrop", user_func_filtchandrop,
			name_newarg("filtname",
			name_newarg("inchan", NULL)));
	exec_newbuiltin(exec, "filtnochandrop", user_func_filtnochandrop,
			name_newarg("filtname",
			name_newarg("inchan", NULL)));
	exec_newbuiltin(exec, "filtchanmap", user_func_filtchanmap,
			name_newarg("filtname",
			name_newarg("inchan", 
			name_newarg("outchan", NULL))));
	exec_newbuiltin(exec, "filtnochanmap", user_func_filtnochanmap,
			name_newarg("filtname",
			name_newarg("outchan", NULL)));
	exec_newbuiltin(exec, "filtkeydrop", user_func_filtkeydrop,
			name_newarg("filtname",
			name_newarg("inchan", 
			name_newarg("keystart", 
			name_newarg("keyend", NULL)))));
	exec_newbuiltin(exec, "filtnokeydrop", user_func_filtnokeydrop,
			name_newarg("filtname",
			name_newarg("inchan", 
			name_newarg("keystart", 
			name_newarg("keyend", NULL)))));
	exec_newbuiltin(exec, "filtkeymap", user_func_filtkeymap,
			name_newarg("filtname",
			name_newarg("inchan", 
			name_newarg("outchan", 
			name_newarg("keystart", 
			name_newarg("keyend", 
			name_newarg("keyplus", NULL)))))));
	exec_newbuiltin(exec, "filtnokeymap", user_func_filtnokeymap,
			name_newarg("filtname",
			name_newarg("outchan", 
			name_newarg("keystart", 
			name_newarg("keyend", NULL)))));
	exec_newbuiltin(exec, "filtctldrop", user_func_filtctldrop,
			name_newarg("filtname",
			name_newarg("inchan", 
			name_newarg("inctl", NULL))));
	exec_newbuiltin(exec, "filtnoctldrop", user_func_filtnoctldrop,
			name_newarg("filtname",
			name_newarg("inchan", 
			name_newarg("inctl", NULL))));
	exec_newbuiltin(exec, "filtctlmap", user_func_filtctlmap,
			name_newarg("filtname",
			name_newarg("inchan", 
			name_newarg("outchan", 
			name_newarg("inctl", 
			name_newarg("outctl", NULL))))));
	exec_newbuiltin(exec, "filtnoctlmap", user_func_filtnoctlmap,
			name_newarg("filtname",
			name_newarg("outchan", 
			name_newarg("outctl", NULL))));
	exec_newbuiltin(exec, "filtswapichan", user_func_filtswapichan,
			name_newarg("filtname",
			name_newarg("oldchan", 
			name_newarg("newchan", NULL))));
	exec_newbuiltin(exec, "filtswapidev", user_func_filtswapidev,
			name_newarg("filtname",
			name_newarg("olddev", 
			name_newarg("newdev", NULL))));
	exec_newbuiltin(exec, "filtsetcurchan", user_func_filtsetcurchan, 
			name_newarg("filtname", 
			name_newarg("channame", NULL)));
	exec_newbuiltin(exec, "filtgetcurchan", user_func_filtgetcurchan,
			name_newarg("filtname", NULL));

	exec_newbuiltin(exec, "songgetunit", user_func_songgetunit, NULL);
	exec_newbuiltin(exec, "songsetunit", user_func_songsetunit, 
			name_newarg("tics_per_unit", NULL));
	exec_newbuiltin(exec, "songgetcurpos", user_func_songgetcurpos, NULL);
	exec_newbuiltin(exec, "songsetcurpos", user_func_songsetcurpos, 
			name_newarg("measure", NULL));
	exec_newbuiltin(exec, "songgetcurquant", user_func_songgetcurquant, NULL);
	exec_newbuiltin(exec, "songsetcurquant", user_func_songsetcurquant, 
			name_newarg("quantum", NULL));
	exec_newbuiltin(exec, "songgetcurtrack", user_func_songgetcurtrack, NULL);
	exec_newbuiltin(exec, "songsetcurtrack", user_func_songsetcurtrack, 
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "songgetcurfilt", user_func_songgetcurfilt, NULL);
	exec_newbuiltin(exec, "songsetcurfilt", user_func_songsetcurfilt, 
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "songgetcursysex", user_func_songgetcursysex, NULL);
	exec_newbuiltin(exec, "songsetcursysex", user_func_songsetcursysex, 
			name_newarg("sysexname", NULL));
	exec_newbuiltin(exec, "songgetcurchan", user_func_songgetcurchan, NULL);
	exec_newbuiltin(exec, "songsetcurchan", user_func_songsetcurchan, 
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "songinfo", user_func_songinfo, NULL);
	exec_newbuiltin(exec, "songsave", user_func_songsave, 
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "songload", user_func_songload, 
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "songreset", user_func_songreset, NULL);
	exec_newbuiltin(exec, "songexportsmf", user_func_songexportsmf, 
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "songimportsmf", user_func_songimportsmf, 
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "songidle", user_func_songidle, NULL);
	exec_newbuiltin(exec, "songplay", user_func_songplay, NULL);
	exec_newbuiltin(exec, "songrecord", user_func_songrecord, NULL);
	exec_newbuiltin(exec, "songsetunit", user_func_songsetunit, NULL);
	exec_newbuiltin(exec, "songsettempo", user_func_songsettempo, 
			name_newarg("measure", 
			name_newarg("beats_per_minute", NULL)));
	exec_newbuiltin(exec, "songtimeins", user_func_songtimeins, 
			name_newarg("from", 
			name_newarg("amount", 
			name_newarg("numerator", 
			name_newarg("denominator", NULL)))));
	exec_newbuiltin(exec, "songtimerm", user_func_songtimerm, 
			name_newarg("from", 
			name_newarg("amount", NULL)));
	exec_newbuiltin(exec, "songtimeinfo", user_func_songtimeinfo, NULL);

	exec_newbuiltin(exec, "metroswitch", user_func_metroswitch, 
			name_newarg("onoff", NULL));
	exec_newbuiltin(exec, "metroconf", user_func_metroconf, 
			name_newarg("eventhi",
			name_newarg("eventlo", NULL)));
	exec_newbuiltin(exec, "shut", user_func_shut, NULL);
	exec_newbuiltin(exec, "sendraw", user_func_sendraw, 
			name_newarg("device", 
			name_newarg("list", NULL)));

	exec_newbuiltin(exec, "devattach", user_func_devattach,
			name_newarg("unit", 
			name_newarg("path", NULL)));
	exec_newbuiltin(exec, "devdetach", user_func_devdetach,
			name_newarg("unit", NULL));
	exec_newbuiltin(exec, "devlist", user_func_devlist, NULL);
	exec_newbuiltin(exec, "devsetmaster", user_func_devsetmaster,
			name_newarg("unit", NULL));
	exec_newbuiltin(exec, "devgetmaster", user_func_devgetmaster, NULL);
	exec_newbuiltin(exec, "devsendrt", user_func_devsendrt,
			name_newarg("unit", 
			name_newarg("sendrt", NULL)));
	exec_newbuiltin(exec, "devticrate", user_func_devticrate,
			name_newarg("unit", 
			name_newarg("tics_per_unit", NULL)));
	exec_newbuiltin(exec, "devinfo", user_func_devinfo,
			name_newarg("unit", NULL));

	if (!user_flag_batch) {
		exec_runrcfile(exec);
	}

	parse = parse_new(NULL);
	if (parse == NULL) {
		return;
	}

	root = NULL;
	data = NULL;
	for (;;) {
		if (parse_getsym(parse)) {
			/* at this stage no lexical error */
			if (parse->lex.id == TOK_EOF) {
				/* end-of-file (user quit) */
				break;
			}
			parse_ungetsym(parse);
			if (parse_line(parse, &root)) {
				/* at this stage no parse error */
				if (node_exec(root, exec, &data) == RESULT_OK) {
					/* at this stage no exec error */
					node_delete(root);
					root = NULL;
					continue;
				}
			}
			node_delete(root);
			root = NULL;
		}
		/* error */
		if (user_flag_batch) {
			break;
		}
	}

	parse_delete(parse);
	exec_delete(exec);
	song_delete(user_song);
	user_song = NULL;	
}

