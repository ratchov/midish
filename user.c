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
 * implements misc built-in functions available through the
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

#include "textio.h"
#include "lex.h"
#include "parse.h"

#include "mux.h"
#include "mididev.h"

#include "track.h"
#include "song.h"
#include "user.h"
#include "smf.h"
#include "saveload.h"
#include "rmidi.h"	/* for rmidi_debug */

struct song *user_song;
unsigned user_flag_batch = 0;
unsigned user_flag_verb = 0;

/* -------------------------------------------------- some tools --- */

/*
 * execute the script in the given file inside the 'exec' environment.
 * the script has acces to all global variables, but not to the local
 * variables of the calling proc. Thus it can be safely used from a
 * procedure
 */
unsigned
exec_runfile(struct exec *exec, char *filename) {
	struct parse *parse;
	struct name **locals;
	struct node *root;
	struct data *data;
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
		res = node_exec(root, exec, &data);
	}
	exec->locals = locals;
	parse_delete(parse);
	node_delete(root);
	return res;
}

/*
 * find the pointer to the songtrk contained in 'var' ('var' must be a
 * reference)
 */
unsigned
exec_lookuptrack(struct exec *o, char *var, struct songtrk **res) {
	char *name;	
	struct songtrk *t;
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
exec_lookupchan_getnum(struct exec *o, char *var, 
    unsigned *dev, unsigned *ch) {
	struct var *arg;
	
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
 * find the pointer to an existing songchan that is referenced by
 * 'var'.  ('var' must be a reference)
 */
unsigned
exec_lookupchan_getref(struct exec *o, char *var, struct songchan **res) {
	struct var *arg;
	struct songchan *i;
	
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
 * find the pointer to the songfilt contained in 'var' ('var' must be
 * a reference)
 */
unsigned
exec_lookupfilt(struct exec *o, char *var, struct songfilt **res) {
	char *name;	
	struct songfilt *f;
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
 * find the pointer to the songsx contained in 'var' ('var' must be a
 * reference)
 */
unsigned
exec_lookupsx(struct exec *o, char *var, struct songsx **res) {
	char *name;	
	struct songsx *t;
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
 *	- { xctl chan xxx yyy }
 *	- { xpc chan xxx yyy }
 *	- { rpn chan xxx yyy }
 *	- { nrpn chan xxx yyy }
 * where 'chan' is in the same as in lookupchan_getnum
 * and 'xxx' and 'yyy' are integers
 */
unsigned
exec_lookupev(struct exec *o, char *name, struct ev *ev) {
	struct var *arg;
	struct data *d;
	unsigned dev, ch, max, num;

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
	ev->dev = dev;
	ev->ch = ch;
	d = d->next;
	if (ev->cmd == EV_XCTL || ev->cmd == EV_CTL) {
		if (!d || !data_getctl(d, &num)) {
			return 0;
		}
		ev->ctl_num = num;
	} else {
		if (ev->cmd == EV_BEND || ev->cmd == EV_NRPN || ev->cmd == EV_RPN) {
			max = EV_MAXFINE; 
		} else {
			max = EV_MAXCOARSE;
		}
		if (!d || d->type != DATA_LONG || d->val.num < 0 || d->val.num > max) {
			cons_err("bad byte0 in event spec");
			return 0;
		}
		ev->v0 = d->val.num;
	}
	d = d->next;
	if (ev->cmd == EV_PC || ev->cmd == EV_CAT || ev->cmd == EV_BEND) {
		if (d) {
			cons_err("extra data in event spec");
			return 0;
		}
	} else {
		if (ev->cmd == EV_XPC || ev->cmd == EV_XCTL ||
		    ev->cmd == EV_NRPN || ev->cmd == EV_RPN) {
			max = EV_MAXFINE;
		} else {
			max = EV_MAXCOARSE;
		}
		if (!d || d->type != DATA_LONG || 
		    d->val.num < 0 || d->val.num > max) {
			cons_err("bad byte1 in event spec");
			return 0;
		}
		ev->v1 = d->val.num;
	}

	/*
	 * convert all CTL -> XCTL and PC -> XPC
	 */
	if (ev->cmd == EV_CTL) {
		ev->cmd = EV_XCTL;
		ev->ctl_val <<= 7;
	}
	if (ev->cmd == EV_PC) {
		ev->cmd = EV_XPC;
		ev->pc_bank = EV_UNDEF;
	}
	return 1;
}

/* 
 * fill the evspec with the one referenced by var.
 * var is of this form
 * 	- { [type [chanrange [xxxrange [yyyrange]]]] }	
 * (brackets mean argument is optionnal)
 */
unsigned
exec_lookupevspec(struct exec *o, char *name, struct evspec *e) {
	struct var *arg;
	struct data *d;
	struct songchan *i;
	unsigned lo, hi, min, max;

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
	evspec_reset(e);

	/*
	 * find the event type
	 */
	d = d->val.list;
	if (!d) {
		goto done;
	}
	if (d->type != DATA_REF ||
	    !evspec_str2cmd(e, d->val.ref)) {
		cons_err("bad status in event spec");
		return 0;
	}
	e->v0_min = evinfo[e->cmd].v0_min;
	e->v0_max = evinfo[e->cmd].v0_max;
	e->v1_min = evinfo[e->cmd].v1_min;
	e->v1_max = evinfo[e->cmd].v1_max;

	/*
	 * find the {device channel} pair
	 */
	d = d->next;
	if (!d) {
		goto done;
	}
	if ((evinfo[e->cmd].flags & (EV_HAS_DEV | EV_HAS_CH)) == 0) {
		goto toomany;
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
	} else {
		cons_err("list or ref expected as channel range spec");
		return 0;
	}

	/*
	 * find the first parameter range
	 */
	d = d->next;
	if (!d) {
		goto done;
	}
	if (evinfo[e->cmd].nranges < 1) {
		goto toomany;
	}
	if ((e->cmd == EVSPEC_CTL || e->cmd == EV_XCTL) && 
	    d->type == DATA_REF) {
		if (!data_getctl(d, &hi)) {
			return 0;
		}
		e->v0_min = e->v0_max = hi;
	} else {
		if (!data_list2range(d, e->v0_min, e->v0_max, &min, &max)) {
			return 0;
		}
		e->v0_min = min;
		e->v0_max = max;
	}

	/*
	 * find the second parameter range; we desallow CTL/XCTL values
	 * in order not to interefere with states
	 */
	d = d->next;
	if (!d) {
		goto done;
	}
	if (evinfo[e->cmd].nranges < 2) {
		goto toomany;
	}
	if (!data_list2range(d, e->v1_min, e->v1_max, &min, &max)) {
		return 0;
	}
	e->v1_min = min;
	e->v1_max = max;
	d = d->next;
	if (d) {
		goto toomany;
	}
 done:
	/*
	 * convert PC->XPC and CTL->XCTL
	 */
	if (e->cmd == EVSPEC_PC) {
		e->cmd = EVSPEC_XPC;
		e->v1_min = evinfo[e->cmd].v1_min;
		e->v1_max = evinfo[e->cmd].v1_max;
	} else if (e->cmd == EVSPEC_CTL) {
		e->cmd = EVSPEC_XCTL;
		e->v1_min =  (e->v1_min << 7) & 0x3fff;
		e->v1_max = ((e->v1_max << 7) & 0x3fff) | 0x7f;
	}
	
	dbg_puts("lookupevspec: ");
	evspec_dbg(e);
	dbg_puts("\n");
	
	return 1;
 toomany:
	cons_err("too many ranges/values in event spec");
	return 0;
}

/*
 * find the (hi lo) couple for the 14bit controller
 * number:
 *	- an integer for 7bit controller
 *	- a list of two integers (like '{hi lo}')
 */
unsigned
exec_lookupctl(struct exec *o, char *var, unsigned *num) {
	struct var *arg;
	
	arg = exec_varlookup(o, var);
	if (!arg) {
		dbg_puts("exec_lookupctl: no such var\n");
		dbg_panic();
	}
	return data_getctl(arg->data, num);
}

/*
 * print a struct data to the console
 */
void
data_print(struct data *d) {
	struct data *i;
	
	switch(d->type) {
	case DATA_NIL:
		textout_putstr(tout, "nil");
		break;
	case DATA_LONG:
		if (d->val.num < 0) {
			textout_putstr(tout, "-");
			textout_putlong(tout, -d->val.num);
		} else {
			textout_putlong(tout, d->val.num);
		}
		break;
	case DATA_STRING:
		textout_putstr(tout, "\"");
		textout_putstr(tout, d->val.str);
		textout_putstr(tout, "\"");
		break;
	case DATA_REF:
		textout_putstr(tout, d->val.ref);
		break;
	case DATA_LIST:
		textout_putstr(tout, "{");
		for (i = d->val.list; i != NULL; i = i->next) {
			data_print(i);
			if (i->next) {
				textout_putstr(tout, " ");
			}
		}
		textout_putstr(tout, "}");
		break;
	case DATA_RANGE:
		textout_putlong(tout, d->val.range.min);
		textout_putstr(tout, ":");
		textout_putlong(tout, d->val.range.max);
		break;
	default:
		dbg_puts("data_print: unknown type\n");
		break;
	}
}

/*
 * convert 2 integer lists to channels
 */
unsigned
data_num2chan(struct data *o, unsigned *res_dev, unsigned *res_ch) {
	long dev, ch;

	if (o == NULL || 
	    o->next == NULL || 
	    o->next->next != NULL ||
	    o->type != DATA_LONG || 
	    o->next->type != DATA_LONG) {
		cons_err("bad {dev midichan} in spec");
		return 0;
	}
	dev = o->val.num;
	ch = o->next->val.num;
	if (ch < 0 || ch > EV_MAXCH || 
	    dev < 0 || dev > EV_MAXDEV) {
		cons_err("bad dev/midichan ranges");
		return 0;
	}
	*res_dev = dev;
	*res_ch = ch;
	return 1;
}

/*
 * lookup the value of the given controller
 */
unsigned
exec_lookupval(struct exec *o, char *n, unsigned isfine, unsigned *r) {
	struct var *arg;
	unsigned max;

	arg = exec_varlookup(o, n);
	if (!arg) {
		dbg_puts("exec_lookupval: no such var\n");
		dbg_panic();
	}
	if (arg->data->type == DATA_NIL) {
		*r = EV_UNDEF;
		return 1;
	} else if (arg->data->type == DATA_LONG) {
		   max = isfine ? EV_MAXFINE : EV_MAXCOARSE;
		   if (arg->data->val.num < 0 || arg->data->val.num > max) {
			   cons_err("controller value out of range");
			   return 0;
		   }
		   *r = arg->data->val.num;
		   return 1;
	} else {
		cons_err("bad type of controller value\n");
		return 0;
	}
}

/*
 * convert lists to channels, 'data' can be
 * 	- a reference to an existing songchan
 *	- a pair of integers '{ dev midichan }'
 */
unsigned
data_list2chan(struct data *o, unsigned *res_dev, unsigned *res_ch) {
	struct songchan *i;

	if (o->type == DATA_LIST) {
		return data_num2chan(o->val.list, res_dev, res_ch);
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
 * convert a struct data to a pair of integers
 * data can be:
 * 	- a liste of 2 integers
 *	- a single integer (then min = max)
 */
unsigned
data_list2range(struct data *d, unsigned min, unsigned max, 
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
			cons_err("exactly 0 or 2 numbers expected in range spec");
			return 0;
		}
		*lo = d->val.num;
		*hi = d->next->val.num;
	} else if (d->type == DATA_RANGE) {
		*lo = d->val.range.min;
		*hi = d->val.range.max;
	} else {
		cons_err("range or number expected in range spec");
		return 0;
	}
	if (*lo < min || *lo > max || *hi < min || *hi > max || *lo > *hi) {
		cons_err("range values out of bounds");
		return 0;
	}
	return 1;
}

/*
 * convert a list to bitmap of continuous controllers
 */
unsigned
data_list2ctlset(struct data *d, unsigned *res) {
	unsigned ctlset;
	
	ctlset = 0;
	while (d) {
		if (d->type != DATA_LONG) {
			cons_err("not-a-number in controller set");
			return 0;
		}
		if (d->val.num < 0 || d->val.num >= 32) {
			cons_err("controller number out of range 0..31");
			return 0;
		}
		if (evctl_isreserved(d->val.num)) {
			cons_erru(d->val.num, "controller number reserved");
			return 0;
		}
		ctlset |= (1 << d->val.num);
		d = d->next;	
	}
	*res = ctlset;
	return 1;
}


/*
 * check if the pattern in data (list of integers)
 * match the beggining of the given sysex
 */
unsigned
data_matchsysex(struct data *d, struct sysex *sx, unsigned *res) {
	unsigned i;
	struct chunk *ck;
	
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

/*
 * convert a 'struct data' to a controller number
 */
unsigned
data_getctl(struct data *d, unsigned *num) {
    	if (d->type == DATA_LONG) {
		if (d->val.num < 0 || d->val.num > EV_MAXCOARSE) {
			cons_err("7bit ctl number out of bounds");
			return 0;
		}
		if (evctl_isreserved(d->val.num)) {
			cons_err("controller is reserved for bank, rpn, nrpn");
			return 0;
		}
		*num = d->val.num;
	} else if (d->type == DATA_REF) {
		if (!evctl_lookup(d->val.ref, num)) {
			cons_errs(d->val.ref, "no such controller\n");
			return 0;
		}
	} else {
		cons_err("number or identifier expected in ctl spec");
		return 0;
	}
	return 1;
}

/* ---------------------------------------- interpreter functions --- */

unsigned
user_func_panic(struct exec *o, struct data **r) {
	dbg_panic();
	/* not reached */
	return 0;
}

unsigned
user_func_debug(struct exec *o, struct data **r) {
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
	} else if (str_eq(flag, "mem")) {
		mem_debug = value;
	} else {
		cons_err("debug: unknuwn debug-flag");
		return 0;
	}
	return 1;
}

unsigned
user_func_exec(struct exec *o, struct data **r) {
	char *filename;		
	if (!exec_lookupstring(o, "filename", &filename)) {
		return 0;
	}	
	return exec_runfile(o, filename);
}

unsigned
user_func_print(struct exec *o, struct data **r) {
	struct var *arg;
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
user_func_info(struct exec *o, struct data **r) {
	exec_dumpprocs(o);
	exec_dumpvars(o);
	return 1;
}

unsigned
user_func_metroswitch(struct exec *o, struct data **r) {
	long onoff;

	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookuplong(o, "onoff", &onoff)) {
		return 0;
	}	
	user_song->metro.enabled = onoff;
	return 1;
}

unsigned
user_func_metroconf(struct exec *o, struct data **r) {
	struct ev evhi, evlo;

	if (!song_try(user_song)) {
		return 0;
	}
	if (!exec_lookupev(o, "eventhi", &evhi) ||
	    !exec_lookupev(o, "eventlo", &evlo)) {
		return 0;
	}
	if (evhi.cmd != EV_NON && evlo.cmd != EV_NON) {
		cons_err("note-on event expected");
		return 0;
	}
	user_song->metro.hi = evhi;
	user_song->metro.lo = evlo;
	return 1;
}

unsigned
user_func_shut(struct exec *o, struct data **r) {
	unsigned i;
	struct ev ev;
	struct mididev *dev;

	if (!song_try(user_song)) {
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
user_func_sendraw(struct exec *o, struct data **r) {
	struct var *arg;
	struct data *i;
	unsigned char byte;
	long device;
	
	if (!song_try(user_song)) {
		return 0;
	}
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
	
	mux_open();
	for (i = arg->data->val.list; i != NULL; i = i->next) {
		byte = i->val.num;
		mux_sendraw(device, &byte, 1);
	}
	mux_close();
	return 1;
}

unsigned
user_func_proclist(struct exec *o, struct data **r) {
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
user_func_builtinlist(struct exec *o, struct data **r) {
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
user_mainloop(void) {
	struct parse *parse;
	struct exec *exec;
	struct node *root;
	struct data *data;
	unsigned result, exitcode;
	
	/* 
	 * create the project (ie the song) and
	 * the execution environment of the interpreter
	 */
	user_song = song_new();
	exec = exec_new();

	/*
	 * register built-in functions 
	 */
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
	exec_newbuiltin(exec, "trackmerge", user_func_trackmerge,
			name_newarg("source", 
			name_newarg("dest", NULL)));
	exec_newbuiltin(exec, "trackquant", user_func_trackquant, 
			name_newarg("trackname", 
			name_newarg("from", 
			name_newarg("amount", 
			name_newarg("rate", 
			name_newarg("quantum", NULL))))));
	exec_newbuiltin(exec, "tracktransp", user_func_tracktransp, 
			name_newarg("trackname", 
			name_newarg("from", 
			name_newarg("amount", 
			name_newarg("halftones",
			name_newarg("quantum", 
			name_newarg("evspec", NULL)))))));
	exec_newbuiltin(exec, "tracksetmute", user_func_tracksetmute,
			name_newarg("trackname",
			name_newarg("muteflag", NULL)));
	exec_newbuiltin(exec, "trackgetmute", user_func_trackgetmute,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackchanlist", user_func_trackchanlist,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "trackinfo", user_func_trackinfo, 
			name_newarg("trackname", 
			name_newarg("quantum", 
			name_newarg("evspec", NULL))));

	exec_newbuiltin(exec, "chanlist", user_func_chanlist, NULL);
	exec_newbuiltin(exec, "chanset", user_func_chanset, 
			name_newarg("channame",
			name_newarg("channum", NULL)));
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
	exec_newbuiltin(exec, "chanunconfev", user_func_chanunconfev,
			name_newarg("channame",
			name_newarg("evspec", NULL)));
	exec_newbuiltin(exec, "chansetcurinput", user_func_chansetcurinput, 
			name_newarg("channame", 
			name_newarg("inputchan", NULL)));
	exec_newbuiltin(exec, "changetcurinput", user_func_changetcurinput,
			name_newarg("channame", NULL));

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
	exec_newbuiltin(exec, "filtchgich", user_func_filtchgich,
			name_newarg("filtname",
			name_newarg("oldchan", 
			name_newarg("newchan", NULL))));
	exec_newbuiltin(exec, "filtchgidev", user_func_filtchgidev,
			name_newarg("filtname",
			name_newarg("olddev", 
			name_newarg("newdev", NULL))));
	exec_newbuiltin(exec, "filtswapich", user_func_filtswapich,
			name_newarg("filtname",
			name_newarg("oldchan", 
			name_newarg("newchan", NULL))));
	exec_newbuiltin(exec, "filtswapidev", user_func_filtswapidev,
			name_newarg("filtname",
			name_newarg("olddev", 
			name_newarg("newdev", NULL))));
	exec_newbuiltin(exec, "filtchgoch", user_func_filtchgoch,
			name_newarg("filtname",
			name_newarg("oldchan", 
			name_newarg("newchan", NULL))));
	exec_newbuiltin(exec, "filtchgodev", user_func_filtchgodev,
			name_newarg("filtname",
			name_newarg("olddev", 
			name_newarg("newdev", NULL))));
	exec_newbuiltin(exec, "filtswapoch", user_func_filtswapoch,
			name_newarg("filtname",
			name_newarg("oldchan", 
			name_newarg("newchan", NULL))));
	exec_newbuiltin(exec, "filtswapodev", user_func_filtswapodev,
			name_newarg("filtname",
			name_newarg("olddev", 
			name_newarg("newdev", NULL))));
	exec_newbuiltin(exec, "filtevmap", user_func_filtevmap,
			name_newarg("filtname",
			name_newarg("from", 
			name_newarg("to", NULL))));
	exec_newbuiltin(exec, "filtevunmap", user_func_filtevunmap,
			name_newarg("filtname",
			name_newarg("from", 
			name_newarg("to", NULL))));
	exec_newbuiltin(exec, "filtsetcurchan", user_func_filtsetcurchan, 
			name_newarg("filtname", 
			name_newarg("channame", NULL)));
	exec_newbuiltin(exec, "filtgetcurchan", user_func_filtgetcurchan,
			name_newarg("filtname", NULL));

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

	exec_newbuiltin(exec, "songgetunit", user_func_songgetunit, NULL);
	exec_newbuiltin(exec, "songsetunit", user_func_songsetunit, 
			name_newarg("tics_per_unit", NULL));
	exec_newbuiltin(exec, "songgetfactor", user_func_songgetfactor, NULL);
	exec_newbuiltin(exec, "songsetfactor", user_func_songsetfactor, 
			name_newarg("tempo_factor", NULL));
	exec_newbuiltin(exec, "songgetcurpos", user_func_songgetcurpos, NULL);
	exec_newbuiltin(exec, "songsetcurpos", user_func_songsetcurpos, 
			name_newarg("measure", NULL));
	exec_newbuiltin(exec, "songgetcurlen", user_func_songgetcurlen, NULL);
	exec_newbuiltin(exec, "songsetcurlen", user_func_songsetcurlen, 
			name_newarg("length", NULL));
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
	exec_newbuiltin(exec, "songgetcurinput", user_func_songgetcurinput, NULL);
	exec_newbuiltin(exec, "songsetcurinput", user_func_songsetcurinput, 
			name_newarg("inputchan", NULL));
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
	exec_newbuiltin(exec, "songstop", user_func_songstop, NULL);
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
	exec_newbuiltin(exec, "songgettempo", user_func_songgettempo, 
			name_newarg("from", NULL));
	exec_newbuiltin(exec, "songgetsign", user_func_songgetsign, 
			name_newarg("from", NULL));
	exec_newbuiltin(exec, "ctlconf", user_func_ctlconf, 
			name_newarg("name", 
			name_newarg("ctl", 
			name_newarg("defval", NULL))));
	exec_newbuiltin(exec, "ctlconfx", user_func_ctlconf, 
			name_newarg("name", 
			name_newarg("ctl", 
			name_newarg("defval", NULL))));
	exec_newbuiltin(exec, "ctlunconf", user_func_ctlunconf, 
			name_newarg("name", NULL));
	exec_newbuiltin(exec, "ctlinfo", user_func_ctlinfo, NULL);

	exec_newbuiltin(exec, "metroswitch", user_func_metroswitch, 
			name_newarg("onoff", NULL));
	exec_newbuiltin(exec, "metroconf", user_func_metroconf, 
			name_newarg("eventhi",
			name_newarg("eventlo", NULL)));
	exec_newbuiltin(exec, "shut", user_func_shut, NULL);
	exec_newbuiltin(exec, "sendraw", user_func_sendraw, 
			name_newarg("device", 
			name_newarg("list", NULL)));
	exec_newbuiltin(exec, "proclist", user_func_proclist, NULL);
	exec_newbuiltin(exec, "builtinlist", user_func_builtinlist, NULL);

	exec_newbuiltin(exec, "devattach", user_func_devattach,
			name_newarg("unit", 
			name_newarg("path", 
			name_newarg("mode", NULL))));
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
	exec_newbuiltin(exec, "devixctl", user_func_devixctl,
			name_newarg("unit", 
			name_newarg("ctlset", NULL)));
	exec_newbuiltin(exec, "devoxctl", user_func_devoxctl,
			name_newarg("unit", 
			name_newarg("ctlset", NULL)));

	/*
	 * run the user startup script: $HOME/.midishrc or /etc/midishrc
	 */
	if (!user_flag_batch) {
		exec_runrcfile(exec);
	}

	/*
	 * create the parser and start parsing standard input
	 */
	parse = parse_new(NULL);
	if (parse == NULL) {
		return 0;
	}

	cons_putpos(user_song->curpos, 0, 0);

	root = NULL;
	data = NULL;
	for (;;) {
		/*
		 * print mem_alloc() and mem_free() stats, useful to
		 * track memory leaks
		 */
		mem_stats();
		
		/*
		 * parse a block
		 */
		if (!parse_getsym(parse)) {
			goto err;
		}
		if (parse->lex.id == TOK_EOF) {
			/* end-of-file (user quit) */
			exitcode = 1;
			break;
		}
		parse_ungetsym(parse);
		if (!parse_line(parse, &root)) {
			node_delete(root);
			root = NULL;
			goto err;
		}
		
		/* 
		 * at this stage no parse error, execute the tree
		 */
		result = node_exec(root, exec, &data);
		node_delete(root);
		root = NULL;
		if (result == RESULT_OK) {
			continue;
		}
		if (result == RESULT_EXIT) {
			exitcode = 1;	/* 1 means success */
			break;
		}
		
	err:	
		/* 
		 * in batch mode stop on the first error
		 */
		if (user_flag_batch) {
			exitcode = 0;	/* 0 means failure */
			break;
		}
	}
	
	parse_delete(parse);
	exec_delete(exec);
	song_delete(user_song);
	user_song = NULL;	
	return exitcode;
}

