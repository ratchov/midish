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

/*
 * implements misc built-in functions available through the
 * interpreter
 *
 * each function is described in the manual.html file
 */

#include "utils.h"
#include "defs.h"
#include "node.h"
#include "exec.h"
#include "data.h"
#include "cons.h"

#include "textio.h"
#include "parse.h"

#include "mux.h"
#include "mididev.h"

#include "track.h"
#include "song.h"
#include "user.h"
#include "builtin.h"
#include "smf.h"
#include "saveload.h"

struct song *usong;
unsigned user_flag_batch = 0;
unsigned user_flag_verb = 0;

void
exec_cb(struct exec *e, struct node *root)
{
	struct data *data;

	if (root == NULL) {
		log_puts("syntax error\n");
		return;
	}
	if (e->result == RESULT_EXIT) {
		log_puts("exitting, skiped\n");
		return;
	}
	e->result = node_exec(root, e, &data);
	if (data != NULL) {
		if (data->type != DATA_NIL) {
			data_print(data);
			textout_putstr(tout, "\n");
		}
		data_delete(data);
	}
}

/*
 * execute the script in the given file inside the 'exec' environment.
 * the script has acces to all global variables, but not to the local
 * variables of the calling proc. Thus it can be safely used from a
 * procedure
 */
unsigned
exec_runfile(struct exec *exec, char *filename)
{
	struct parse parse;
	struct textin *in;
	struct name **locals;
	int c;

	in = textin_new(filename);
	if (in == NULL)
		return 0;
	locals = exec->locals;
	exec->locals = &exec->globals;
	parse_init(&parse, exec, exec_cb);
	lex_init(&parse, filename, parse_cb, &parse);
	for (;;) {
		if (!textin_getchar(in, &c))
			break;
		lex_handle(&parse, c);
		if (c < 0)
			break;
	}
	exec->locals = locals;
	textin_delete(in);
	lex_done(&parse);
	parse_done(&parse);
	return exec->result == RESULT_OK || exec->result == RESULT_EXIT;
}

/*
 * find the pointer to the songtrk contained in 'var' ('var' must be a
 * reference)
 */
unsigned
exec_lookuptrack(struct exec *o, char *var, struct songtrk **res)
{
	char *name;
	struct songtrk *t;
	if (!exec_lookupname(o, var, &name)) {
		return 0;
	}
	t = song_trklookup(usong, name);
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
    unsigned *dev, unsigned *ch, int input) {
	struct var *arg;

	arg = exec_varlookup(o, var);
	if (!arg) {
		log_puts("exec_lookupchan_getnum: no such var\n");
		panic();
	}
	if (!data_getchan(arg->data, dev, ch, input)) {
		return 0;
	}
	return 1;
}

/*
 * find the pointer to an existing songchan that is referenced by
 * 'var'.  ('var' must be a reference)
 */
unsigned
exec_lookupchan_getref(struct exec *o, char *var,
    struct songchan **res, int input)
{
	struct var *arg;
	struct songchan *i;

	arg = exec_varlookup(o, var);
	if (!arg) {
		log_puts("exec_lookupchan: no such var\n");
		panic();
	}
	if (arg->data->type == DATA_REF) {
		i = song_chanlookup(usong, arg->data->val.ref, input);
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
exec_lookupfilt(struct exec *o, char *var, struct songfilt **res)
{
	char *name;
	struct songfilt *f;

	if (!exec_lookupname(o, var, &name)) {
		return 0;
	}
	f = song_filtlookup(usong, name);
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
exec_lookupsx(struct exec *o, char *var, struct songsx **res)
{
	char *name;
	struct songsx *t;

	if (!exec_lookupname(o, var, &name)) {
		return 0;
	}
	t = song_sxlookup(usong, name);
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
exec_lookupev(struct exec *o, char *name, struct ev *ev, int input)
{
	struct var *arg;
	struct data *d;
	unsigned dev, ch, num;

	arg = exec_varlookup(o, name);
	if (!arg) {
		log_puts("exec_lookupev: no such var\n");
		panic();
	}
	d = arg->data;

	if (d->type != DATA_LIST) {
		cons_err("event spec must be a list");
		return 0;
	}
	d = d->val.list;
	if (!d || d->type != DATA_REF ||
	    !ev_str2cmd(ev, d->val.ref) ||
	    (!EV_ISVOICE(ev) && !EV_ISSX(ev))) {
		cons_err("bad status in event spec");
		return 0;
	}
	d = d->next;
	if (!d) {
		cons_err("channel and/or device missing in event spec");
		return 0;
	}
	if (evinfo[ev->cmd].flags & EV_HAS_CH) {
		if (!data_getchan(d, &dev, &ch, input)) {
			return 0;
		}
		ev->dev = dev;
		ev->ch = ch;
	} else {
		if (d->type != DATA_LONG) {
			cons_err("device number expected in event spec");
			return 0;
		}
		if (d->val.num < 0 || d->val.num >= DEFAULT_MAXNDEVS) {
			cons_err("device number out of range in event spec");
			return 0;
		}
		ev->dev = d->val.num;
		ev->ch = 0;
	}
	d = d->next;
	if (ev->cmd == EV_XCTL || ev->cmd == EV_CTL) {
		if (!d || !data_getctl(d, &num)) {
			return 0;
		}
		ev->ctl_num = num;
	} else {
		if (ev->cmd == EV_XPC && d != NULL && d->type == DATA_NIL) {
			ev->v0 = EV_UNDEF;
		} else if (d == NULL ||
		    d->type != DATA_LONG ||
		    d->val.num < 0 ||
		    d->val.num > evinfo[ev->cmd].v0_max) {
			cons_err("bad v0 in event spec");
			return 0;
		} else
			ev->v0 = d->val.num;
	}
	d = d->next;
	if (evinfo[ev->cmd].nparams < 2) {
		if (d != NULL) {
			cons_err("extra data in event spec");
			return 0;
		}
	} else {
		if (d == NULL ||
		    d->type != DATA_LONG ||
		    d->val.num < 0 ||
		    d->val.num > evinfo[ev->cmd].v1_max) {
			cons_err("bad v1 in event spec");
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
		ev->pc_prog = ev->v0;
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
exec_lookupevspec(struct exec *o, char *name, struct evspec *e, int input)
{
	struct var *arg;
	struct data *d;
	struct songchan *i;
	unsigned lo, hi, min, max;

	arg = exec_varlookup(o, name);
	if (!arg) {
		log_puts("exec_lookupev: no such var\n");
		panic();
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
	if (!(evinfo[e->cmd].flags & EV_HAS_DEV))
		e->dev_max = e->dev_min;
	if (!(evinfo[e->cmd].flags & EV_HAS_CH))
		e->ch_max = e->ch_min;
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
		i = song_chanlookup(usong, d->val.ref, input);
		if (i == NULL) {
			cons_err("no such chan name");
			return 0;
		}
		e->dev_min = e->dev_max = i->dev;
		e->ch_min = e->ch_max = i->ch;
	} else if (d->type == DATA_LIST) {
		if (!d->val.list) {
			/* empty list = any chan/dev */
		} else if (d->val.list &&
		    d->val.list->next &&
		    !d->val.list->next->next) {
			if (!data_getrange(d->val.list, 0,
				EV_MAXDEV, &lo, &hi)) {
				return 0;
			}
			e->dev_min = lo;
			e->dev_max = hi;
			if (!data_getrange(d->val.list->next, 0,
				EV_MAXCH, &lo, &hi)) {
				return 0;
			}
			e->ch_min = lo;
			e->ch_max = hi;
		} else {
			cons_err("bad channel range spec");
			return 0;
		}
	} else if (d->type == DATA_LONG) {
		if (d->val.num < 0 || d->val.num > EV_MAXDEV) {
			cons_err("bad device number");
			return 0;
		}
		e->dev_min = e->dev_max = d->val.num;
		e->ch_min = 0;
		e->ch_max = (evinfo[e->cmd].flags & EV_HAS_CH) ?
			EV_MAXCH : e->ch_min;
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
	} else if (e->cmd == EV_XPC && d->type == DATA_NIL) {
		e->v0_min = e->v0_max = EV_UNDEF;
	} else {
		if (!data_getrange(d, e->v0_min, e->v0_max, &min, &max)) {
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
	if (!data_getrange(d, e->v1_min, e->v1_max, &min, &max)) {
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
		e->v1_min = e->v0_min;
		e->v1_max = e->v0_max;
		e->v0_min = e->v0_max = EV_UNDEF;
	} else if (e->cmd == EVSPEC_CTL) {
		e->cmd = EVSPEC_XCTL;
		e->v1_min =  (e->v1_min << 7) & 0x3fff;
		e->v1_max = ((e->v1_max << 7) & 0x3fff) | 0x7f;
	}

#if 0
	log_puts("lookupevspec: ");
	evspec_log(e);
	log_puts("\n");
#endif

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
exec_lookupctl(struct exec *o, char *var, unsigned *num)
{
	struct var *arg;

	arg = exec_varlookup(o, var);
	if (!arg) {
		log_puts("exec_lookupctl: no such var\n");
		panic();
	}
	return data_getctl(arg->data, num);
}

/*
 * print a struct data to the console
 */
void
data_print(struct data *d)
{
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
		log_puts("data_print: unknown type\n");
		break;
	}
}

/*
 * lookup the value of the given controller
 */
unsigned
exec_lookupval(struct exec *o, char *n, unsigned isfine, unsigned *r)
{
	struct var *arg;
	unsigned max;

	arg = exec_varlookup(o, n);
	if (!arg) {
		log_puts("exec_lookupval: no such var\n");
		panic();
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
		cons_err("bad type of controller value");
		return 0;
	}
}

/*
 * convert lists to channels, 'data' can be
 * 	- a reference to an existing songchan
 *	- a pair of integers '{ dev midichan }'
 */
unsigned
data_getchan(struct data *o, unsigned *res_dev, unsigned *res_ch, int input)
{
	struct songchan *i;
	long dev, ch;

	if (o->type == DATA_LIST) {
		o = o->val.list;
		if (o == NULL ||
		    o->next == NULL ||
		    o->next->next != NULL ||
		    o->type != DATA_LONG ||
		    o->next->type != DATA_LONG) {
			cons_err("bad {dev midichan} pair");
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
	} else if (o->type == DATA_REF) {
		i = song_chanlookup(usong, o->val.ref, input);
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
data_getrange(struct data *d, unsigned min, unsigned max,
    unsigned *rlo, unsigned *rhi)
{
	long lo, hi;

    	if (d->type == DATA_LONG) {
		lo = hi = d->val.num;
	} else if (d->type == DATA_LIST) {
		d = d->val.list;
		if (!d) {
			*rlo = min;
			*rhi = max;
			return 1;
		}
		if (!d->next || d->next->next ||
		    d->type != DATA_LONG || d->next->type != DATA_LONG) {
			cons_err("exactly 0 or 2 numbers expected in range spec");
			return 0;
		}
		lo = d->val.num;
		hi = d->next->val.num;
	} else if (d->type == DATA_RANGE) {
		lo = d->val.range.min;
		hi = d->val.range.max;
	} else {
		cons_err("range or number expected in range spec");
		return 0;
	}
	if (lo < min || lo > max || hi < min || hi > max || lo > hi) {
		cons_err("range values out of bounds");
		return 0;
	}
	*rlo = lo;
	*rhi = hi;
	return 1;
}

/*
 * convert a list to bitmap of continuous controllers
 */
unsigned
data_getctlset(struct data *d, unsigned *res)
{
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
		ctlset |= (1 << d->val.num);
		d = d->next;
	}
	*res = ctlset;
	return 1;
}

/*
 * convert a list to bitmap of CONV_xxx constants
 */
unsigned
data_getxev(struct data *d, unsigned *res)
{
	static unsigned cmds[] = {EV_XPC, EV_NRPN, EV_RPN};
	unsigned i, conv, cmd;

	conv = 0;
	while (d) {
		if (d->type != DATA_REF) {
		err:
			cons_err("xpc, rpn, or nrpn expected as flag");
			return 0;
		}
		i = 0;
		for (;;) {
			if (i == sizeof(cmds) / sizeof(int))
				goto err;
			cmd = cmds[i];
			if (str_eq(d->val.ref, evinfo[cmd].ev))
				break;
			i++;
		}
		conv |= (1 << cmd);
		d = d->next;
	}
	*res = conv;
	return 1;
}

/*
 * check if the pattern in data (list of integers)
 * match the beggining of the given sysex
 */
unsigned
data_matchsysex(struct data *d, struct sysex *sx, unsigned *res)
{
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
data_getctl(struct data *d, unsigned *num)
{
    	if (d->type == DATA_LONG) {
		if (d->val.num < 0 || d->val.num > EV_MAXCOARSE) {
			cons_err("7bit ctl number out of bounds");
			return 0;
		}
		*num = d->val.num;
	} else if (d->type == DATA_REF) {
		if (!evctl_lookup(d->val.ref, num)) {
			cons_errs(d->val.ref, "no such controller");
			return 0;
		}
	} else {
		cons_err("number or identifier expected in ctl spec");
		return 0;
	}
	return 1;
}

static struct parse parse;
static struct exec *exec;
static unsigned exitcode;
int done = 0;

void
user_onchar(void *arg, int c)
{
	if (done)
		return;
	lex_handle(&parse, c);
	if (c < 0) {
		/* end-of-file (user quit) */
		exitcode = 1;
		done = 1;
	}
	if (exec->result == RESULT_EXIT) {
		exitcode = 1;
		done = 1;
	}
	if (exec->result == RESULT_ERR && user_flag_batch) {
		exitcode = 0;
		done = 1;
	}
	if (c == '\n')
		cons_ready();
}

void
user_oncompl(void *arg, char *text, int curs, int used, int *rstart, int *rend)
{
	struct name *n;
	int start, end;
	int c, quote;

	quote = 0;
	start = curs;
	while (1) {
		if (start-- == 0)
			break;
		if (text[start] == '"')
			quote ^= 1;
	}

	if (quote) {

		/*
		 * quoted test is file-name, find directory components
		 * and scan directory
		 */

		end = curs;
		while (1) {
			if (end == used || text[end] == '"')
				break;
			end++;
		}

		start = curs;
		while (1) {
			if (text[start - 1] == '"')
				break;
			start--;
		}

		user_oncompl_path(text, &start, &end);
	} else {

		/*
		 * unquoted text is a proc name; complete
		 * if it's either at the beginning or after '['
		 */

		start = curs;
		while (1) {
			if (start == 0)
				break;
			c = text[start - 1];
			if (!IS_IDNEXT(c)) {
				if (c == '[')
					break;
				return;
			}
			start--;
		}
		end = curs;
		while (1) {
			if (end == used || !IS_IDNEXT(text[end]))
				break;
			end++;
		}

		for (n = exec->procs; n != NULL; n = n->next)
			el_compladd(n->str);
	}

	*rstart = start;
	*rend = end;
}

struct el_ops user_el_ops = {
	user_onchar,
	user_oncompl
};

unsigned
user_mainloop(void)
{
	cons_init(&user_el_ops, NULL);
	textio_init();
	evctl_init();
	seqev_pool_init(DEFAULT_MAXNSEQEVS);
	state_pool_init(DEFAULT_MAXNSTATES);
	chunk_pool_init(DEFAULT_MAXNCHUNKS);
	sysex_pool_init(DEFAULT_MAXNSYSEXS);
	seqptr_pool_init(DEFAULT_MAXNSEQPTRS);

	/*
	 * create the project (ie the song) and
	 * the execution environment of the interpreter
	 */
	mididev_listinit();
	usong = song_new();
	exec = exec_new();

	/*
	 * register built-in functions
	 */
	exec_newbuiltin(exec, "print", blt_print,
			name_newarg("...", NULL));
	exec_newbuiltin(exec, "err", blt_err,
			name_newarg("message", NULL));
	exec_newbuiltin(exec, "h", blt_h,
			name_newarg("...", NULL));
	exec_newbuiltin(exec, "exec", blt_exec,
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "debug", blt_debug,
			name_newarg("flag",
			name_newarg("value", NULL)));
	exec_newbuiltin(exec, "version", blt_version, NULL);
	exec_newbuiltin(exec, "panic", blt_panic, NULL);
	exec_newbuiltin(exec, "info", blt_info, NULL);

	exec_newbuiltin(exec, "getunit", blt_getunit, NULL);
	exec_newbuiltin(exec, "setunit", blt_setunit,
			name_newarg("tics_per_unit", NULL));
	exec_newbuiltin(exec, "getfac", blt_getfac, NULL);
	exec_newbuiltin(exec, "fac", blt_fac,
			name_newarg("tempo_factor", NULL));
	exec_newbuiltin(exec, "getpos", blt_getpos, NULL);
	exec_newbuiltin(exec, "g", blt_goto,
			name_newarg("measure", NULL));
	exec_newbuiltin(exec, "getlen", blt_getlen, NULL);
	exec_newbuiltin(exec, "sel", blt_sel,
			name_newarg("length", NULL));
	exec_newbuiltin(exec, "loop", blt_loop, NULL);
	exec_newbuiltin(exec, "noloop", blt_noloop, NULL);
	exec_newbuiltin(exec, "getq", blt_getq, NULL);
	exec_newbuiltin(exec, "setq", blt_setq,
			name_newarg("step", NULL));
	exec_newbuiltin(exec, "ev", blt_ev,
			name_newarg("evspec", NULL));
	exec_newbuiltin(exec, "gett", blt_gett, NULL);
	exec_newbuiltin(exec, "ct", blt_ct,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "getf", blt_getf, NULL);
	exec_newbuiltin(exec, "cf", blt_cf,
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "getx", blt_getx, NULL);
	exec_newbuiltin(exec, "cx", blt_cx,
			name_newarg("sysexname", NULL));
	exec_newbuiltin(exec, "geti", blt_geti, NULL);
	exec_newbuiltin(exec, "ci", blt_ci,
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "geto", blt_geto, NULL);
	exec_newbuiltin(exec, "co", blt_co,
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "mute", blt_mute,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "unmute", blt_unmute,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "getmute", blt_getmute,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "ls", blt_ls, NULL);
	exec_newbuiltin(exec, "save", blt_save,
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "load", blt_load,
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "reset", blt_reset, NULL);
	exec_newbuiltin(exec, "export", blt_export,
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "import", blt_import,
			name_newarg("filename", NULL));
	exec_newbuiltin(exec, "i", blt_idle, NULL);
	exec_newbuiltin(exec, "p", blt_play, NULL);
	exec_newbuiltin(exec, "r", blt_rec, NULL);
	exec_newbuiltin(exec, "s", blt_stop, NULL);
	exec_newbuiltin(exec, "t", blt_tempo,
			name_newarg("beats_per_minute", NULL));
	exec_newbuiltin(exec, "mins", blt_mins,
			name_newarg("amount",
			name_newarg("sig", NULL)));
	exec_newbuiltin(exec, "mcut", blt_mcut, NULL);
	exec_newbuiltin(exec, "mdup", blt_mdup,
			name_newarg("where", NULL));
	exec_newbuiltin(exec, "minfo", blt_minfo, NULL);
	exec_newbuiltin(exec, "mtempo", blt_mtempo, NULL);
	exec_newbuiltin(exec, "msig", blt_msig, NULL);
	exec_newbuiltin(exec, "mend", blt_mend, NULL);
	exec_newbuiltin(exec, "ctlconf", blt_ctlconf,
			name_newarg("name",
			name_newarg("ctl",
			name_newarg("defval", NULL))));
	exec_newbuiltin(exec, "ctlconfx", blt_ctlconfx,
			name_newarg("name",
			name_newarg("ctl",
			name_newarg("defval", NULL))));
	exec_newbuiltin(exec, "ctlunconf", blt_ctlunconf,
			name_newarg("name", NULL));
	exec_newbuiltin(exec, "ctlinfo", blt_ctlinfo, NULL);
	exec_newbuiltin(exec, "evpat", blt_evpat,
			name_newarg("name",
			name_newarg("pattern", NULL)));
	exec_newbuiltin(exec, "evinfo", blt_evinfo, NULL);
	exec_newbuiltin(exec, "m", blt_metro,
			name_newarg("onoff", NULL));
	exec_newbuiltin(exec, "metrocf", blt_metrocf,
			name_newarg("eventhi",
			name_newarg("eventlo", NULL)));
	exec_newbuiltin(exec, "tap", blt_tap,
			name_newarg("mode", NULL));
	exec_newbuiltin(exec, "tapev", blt_tapev,
			name_newarg("evspec", NULL));
	exec_newbuiltin(exec, "u", blt_undo, NULL);
	exec_newbuiltin(exec, "ul", blt_undolist, NULL);
	exec_newbuiltin(exec, "tlist", blt_tlist, NULL);
	exec_newbuiltin(exec, "tnew", blt_tnew,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "tdel", blt_tdel, NULL);
	exec_newbuiltin(exec, "tren", blt_tren,
			name_newarg("newname", NULL));
	exec_newbuiltin(exec, "texists", blt_texists,
			name_newarg("trackname", NULL));
	exec_newbuiltin(exec, "taddev", blt_taddev,
			name_newarg("measure",
			name_newarg("beat",
			name_newarg("tic",
			name_newarg("event", NULL)))));
	exec_newbuiltin(exec, "tsetf", blt_tsetf,
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "tgetf", blt_tgetf, NULL);
	exec_newbuiltin(exec, "tcheck", blt_tcheck, NULL);
	exec_newbuiltin(exec, "trewrite", blt_trewrite, NULL);
	exec_newbuiltin(exec, "tcut", blt_tcut, NULL);
	exec_newbuiltin(exec, "tclr", blt_tclr, NULL);
	exec_newbuiltin(exec, "tpaste", blt_tpaste, NULL);
	exec_newbuiltin(exec, "tcopy", blt_tcopy, NULL);
	exec_newbuiltin(exec, "tins", blt_tins,
			name_newarg("amount", NULL));
	exec_newbuiltin(exec, "tmerge", blt_tmerge,
			name_newarg("source", NULL));
	exec_newbuiltin(exec, "tquanta", blt_tquanta,
			name_newarg("rate", NULL));
	exec_newbuiltin(exec, "tquantf", blt_tquantf,
			name_newarg("rate", NULL));
	exec_newbuiltin(exec, "ttransp", blt_ttransp,
			name_newarg("halftones", NULL));
	exec_newbuiltin(exec, "tvcurve", blt_tvcurve,
			name_newarg("weight", NULL));
	exec_newbuiltin(exec, "tevmap", blt_tevmap,
			name_newarg("from",
			name_newarg("to", NULL)));
	exec_newbuiltin(exec, "tclist", blt_tclist, NULL);
	exec_newbuiltin(exec, "tinfo", blt_tinfo, NULL);
	exec_newbuiltin(exec, "tdump", blt_tdump, NULL);

	exec_newbuiltin(exec, "ilist", blt_ilist, NULL);
	exec_newbuiltin(exec, "iexists", blt_iexists,
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "iset", blt_iset,
			name_newarg("channum", NULL));
	exec_newbuiltin(exec, "inew", blt_inew,
			name_newarg("channame",
			name_newarg("channum", NULL)));
	exec_newbuiltin(exec, "idel", blt_idel, NULL);
	exec_newbuiltin(exec, "iren", blt_iren,
			name_newarg("newname", NULL));
	exec_newbuiltin(exec, "iinfo", blt_iinfo, NULL);
	exec_newbuiltin(exec, "igetc", blt_igetc, NULL);
	exec_newbuiltin(exec, "igetd", blt_igetd, NULL);
	exec_newbuiltin(exec, "iaddev", blt_iaddev,
			name_newarg("event", NULL));
	exec_newbuiltin(exec, "irmev", blt_irmev,
			name_newarg("evspec", NULL));

	exec_newbuiltin(exec, "olist", blt_olist, NULL);
	exec_newbuiltin(exec, "oexists", blt_oexists,
			name_newarg("channame", NULL));
	exec_newbuiltin(exec, "oset", blt_oset,
			name_newarg("channum", NULL));
	exec_newbuiltin(exec, "onew", blt_onew,
			name_newarg("channame",
			name_newarg("channum", NULL)));
	exec_newbuiltin(exec, "odel", blt_odel, NULL);
	exec_newbuiltin(exec, "oren", blt_oren,
			name_newarg("newname", NULL));
	exec_newbuiltin(exec, "oinfo", blt_oinfo, NULL);
	exec_newbuiltin(exec, "ogetc", blt_ogetc, NULL);
	exec_newbuiltin(exec, "ogetd", blt_ogetd, NULL);
	exec_newbuiltin(exec, "oaddev", blt_oaddev,
			name_newarg("event", NULL));
	exec_newbuiltin(exec, "ormev", blt_ormev,
			name_newarg("evspec", NULL));

	exec_newbuiltin(exec, "flist", blt_flist, NULL);
	exec_newbuiltin(exec, "fexists", blt_fexists,
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "fnew", blt_fnew,
			name_newarg("filtname", NULL));
	exec_newbuiltin(exec, "fdel", blt_fdel, NULL);
	exec_newbuiltin(exec, "fren", blt_fren,
			name_newarg("newname", NULL));
	exec_newbuiltin(exec, "finfo", blt_finfo, NULL);
	exec_newbuiltin(exec, "freset", blt_freset, NULL);
	exec_newbuiltin(exec, "fmap", blt_fmap,
			name_newarg("from",
			name_newarg("to", NULL)));
	exec_newbuiltin(exec, "funmap", blt_funmap,
			name_newarg("from",
			name_newarg("to", NULL)));
	exec_newbuiltin(exec, "ftransp", blt_ftransp,
			name_newarg("evspec",
			name_newarg("plus", NULL)));
	exec_newbuiltin(exec, "fvcurve", blt_fvcurve,
			name_newarg("evspec",
			name_newarg("weight", NULL)));
	exec_newbuiltin(exec, "fchgin", blt_fchgin,
			name_newarg("from",
			name_newarg("to", NULL)));
	exec_newbuiltin(exec, "fchgout", blt_fchgout,
			name_newarg("from",
			name_newarg("to", NULL)));
	exec_newbuiltin(exec, "fswapin", blt_fswapin,
			name_newarg("from",
			name_newarg("to", NULL)));
	exec_newbuiltin(exec, "fswapout", blt_fswapout,
			name_newarg("from",
			name_newarg("to", NULL)));

	exec_newbuiltin(exec, "xlist", blt_xlist, NULL);
	exec_newbuiltin(exec, "xexists", blt_xexists,
			name_newarg("sysexname", NULL));
	exec_newbuiltin(exec, "xnew", blt_xnew,
			name_newarg("sysexname", NULL));
	exec_newbuiltin(exec, "xdel", blt_xdel, NULL);
	exec_newbuiltin(exec, "xren", blt_xren,
			name_newarg("newname", NULL));
	exec_newbuiltin(exec, "xinfo", blt_xinfo, NULL);
	exec_newbuiltin(exec, "xrm", blt_xrm,
			name_newarg("data", NULL));
	exec_newbuiltin(exec, "xsetd", blt_xsetd,
			name_newarg("devnum",
			name_newarg("data", NULL)));
	exec_newbuiltin(exec, "xadd", blt_xadd,
			name_newarg("devnum",
			name_newarg("data", NULL)));
	exec_newbuiltin(exec, "ximport", blt_ximport,
		        name_newarg("devnum",
			name_newarg("path", NULL)));
	exec_newbuiltin(exec, "xexport", blt_xexport,
			name_newarg("path", NULL));

	exec_newbuiltin(exec, "shut", blt_shut, NULL);
	exec_newbuiltin(exec, "proclist", blt_proclist, NULL);
	exec_newbuiltin(exec, "builtinlist", blt_builtinlist, NULL);

	exec_newbuiltin(exec, "dnew", blt_dnew,
			name_newarg("devnum",
			name_newarg("path",
			name_newarg("mode", NULL))));
	exec_newbuiltin(exec, "ddel", blt_ddel,
			name_newarg("devnum", NULL));
	exec_newbuiltin(exec, "dlist", blt_dlist, NULL);
	exec_newbuiltin(exec, "dmtcrx", blt_dmtcrx,
			name_newarg("devnum", NULL));
	exec_newbuiltin(exec, "dmmctx", blt_dmmctx,
			name_newarg("devlist", NULL));
	exec_newbuiltin(exec, "dclktx", blt_dclktx,
			name_newarg("devlist", NULL));
	exec_newbuiltin(exec, "dclkrx", blt_dclkrx,
			name_newarg("devnum", NULL));
	exec_newbuiltin(exec, "dclkrate", blt_dclkrate,
			name_newarg("devnum",
			name_newarg("tics_per_unit", NULL)));
	exec_newbuiltin(exec, "dinfo", blt_dinfo,
			name_newarg("devnum", NULL));
	exec_newbuiltin(exec, "dixctl", blt_dixctl,
			name_newarg("devnum",
			name_newarg("ctlset", NULL)));
	exec_newbuiltin(exec, "doxctl", blt_doxctl,
			name_newarg("devnum",
			name_newarg("ctlset", NULL)));
	exec_newbuiltin(exec, "diev", blt_diev,
			name_newarg("devnum",
			name_newarg("flags", NULL)));
	exec_newbuiltin(exec, "doev", blt_doev,
			name_newarg("devnum",
			name_newarg("flags", NULL)));

	/*
	 * run the user startup script: $HOME/.midishrc or /etc/midishrc
	 */
	if (!user_flag_batch) {
		exec_runrcfile(exec);
		if (mididev_list == NULL)
			cons_err("Warning, no MIDI devices configured.");
	}

	/*
	 * create the parser and start parsing standard input
	 */
	parse_init(&parse, exec, exec_cb);
	lex_init(&parse, "stdin", parse_cb, &parse);

	cons_ready();
	cons_putpos(usong->curpos, 0, 0);

	done = 0;
	while (!done && mux_mdep_wait(1))
		; /* nothing */

	song_delete(usong);
	usong = NULL;
	lex_done(&parse);
	parse_done(&parse);
	exec_delete(exec);
	mididev_listdone();
	seqptr_pool_done();
	sysex_pool_done();
	chunk_pool_done();
	state_pool_done();
	seqev_pool_done();
	evctl_done();
	textio_done();
	cons_done();
	return exitcode;
}
