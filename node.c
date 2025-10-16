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
 * this module implements the tree containing interpreter code. Each
 * node of the tree represents one instruction.
 */
#include <stdio.h>
#include "utils.h"
#include "str.h"
#include "data.h"
#include "node.h"
#include "exec.h"
#include "cons.h"
#include "user.h"
#include "textio.h"

struct node *
node_new(struct node_vmt *vmt, struct data *data)
{
	struct node *o;

	o = xmalloc(sizeof(struct node), "node");
	o->vmt = vmt;
	o->data = data;
	o->list = o->next = NULL;
	return o;
}

void
node_delete(struct node *o)
{
	struct node *i, *inext;
	if (o == NULL) {
		return;
	}
	for (i = o->list; i != NULL; i = inext) {
		inext = i->next;
		node_delete(i);
	}
	if (o->data) {
		data_delete(o->data);
	}
	xfree(o);
}

size_t
node_fmt(char *buf, size_t bufsz, struct node *o)
{
	char *p = buf, *end = buf + bufsz;

	if (o == NULL)
		return snprintf(buf, bufsz, "<EMPTY>");

	p += snprintf(buf, sizeof(buf), "%s", o->vmt->name);
	if (o->data) {
		p += snprintf(p, p < end ? end - p : 0, "(");
		p += data_fmt(p, p < end ? end - p : 0, o->data);
		p += snprintf(p, p < end ? end - p : 0, ")");
	}
	return p - buf;
}

void
node_log(struct node *o, unsigned depth)
{
	char nodestr[128];
#define NODE_MAXDEPTH 30
	static char str[2 * NODE_MAXDEPTH + 1] = "";
	struct node *i;

	node_fmt(nodestr, sizeof(nodestr), o);

	logx(1, "%s%s<%s>", str, (o != NULL && o->next != NULL) ? "+-" : "\\-", nodestr);

	if (depth < NODE_MAXDEPTH) {
		str[2 * depth] = o->next ? '|' : ' ';
		str[2 * depth + 1] = ' ';
		str[2 * depth + 2] = '\0';
		for (i = o->list; i != NULL; i = i->next) {
			node_log(i, depth + 1);
		}
		str[2 * depth] = '\0';
	}
#undef NODE_MAXDEPTH
}

void
node_insert(struct node **n, struct node *e)
{
	e->next = *n;
	*n = e;
}

void
node_replace(struct node **n, struct node *e)
{
	if (e->list != NULL) {
		logx(1, "%s: e->list != NULL", __func__);
		panic();
	}
	e->list = *n;
	e->next = (*n)->next;
	(*n)->next = NULL;
	*n = e;
}


/*
 * run a node.
 * the following rule must be respected
 * 1) node_exec must be called always with *r == NULL
 * 2) in statements (slist,...) *r != NULL if and only if RETURN
 * 3) in expressions (add, ...) *r == NULL if and only if ERROR
 */
unsigned
node_exec(struct node *o, struct exec *x, struct data **r)
{
	unsigned result;
	if (x->depth == EXEC_MAXDEPTH) {
		logx(1, "too many nested operations");
		return RESULT_ERR;
	}
	*r = NULL;
	x->depth++;
	result = o->vmt->exec(o, x, r);
	x->depth--;
	if (result == RESULT_ERR && *r) {
		data_delete(*r);
		*r = NULL;
	}
	return result;
}

/*
 * execute an unary operator ( '-', '!', '~')
 */
unsigned
node_exec_unary(struct node *o, struct exec *x, struct data **r,
	unsigned (*func)(struct data *)) {
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	if (!func(*r)) {
		return RESULT_ERR;
	}
	return RESULT_OK;
}

/*
 * execute a binary operator
 */
unsigned
node_exec_binary(struct node *o, struct exec *x, struct data **r,
	unsigned (*func)(struct data *, struct data *)) {
	struct data *lhs;
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	lhs = *r;
	*r = NULL;
	if (node_exec(o->list->next, x, r) == RESULT_ERR) {
		data_delete(lhs);
		return RESULT_ERR;
	}
	if (!func(lhs, *r)) {
		data_delete(lhs);
		return RESULT_ERR;
	}
	data_delete(*r);
	*r = lhs;
	return RESULT_OK;
}

/*
 * execute a procedure definition: just check arguments
 * and move the tree into a proc structure
 */
unsigned
node_exec_proc(struct node *o, struct exec *x, struct data **r)
{
	struct proc *p;
	struct data *a;
	struct name *args;

	args = NULL;
	for (a = o->data->val.list->next; a != NULL; a = a->next) {
		if (name_lookup(&args, a->val.ref)) {
			logx(1, "duplicate arguments in proc definition");
			name_empty(&args);
			return RESULT_ERR;
		}
		name_add(&args, name_new(a->val.ref));
	}
	p = exec_proclookup(x, o->data->val.list->val.ref);
	if (p != NULL) {
		name_empty(&p->args);
		node_delete(p->code);
	} else {
		p = proc_new(o->data->val.list->val.ref);
		name_insert((struct name **)&x->procs, (struct name *)p);
	}
	p->args = args;
	p->code = o->list;
	o->list = NULL;
	return RESULT_OK;
}

/*
 * execute a list of statements
 */
unsigned
node_exec_slist(struct node *o, struct exec *x, struct data **r)
{
	struct node *i;
	unsigned result;

	for (i = o->list; i != NULL; i = i->next) {
		result = node_exec(i, x, r);
		if (result != RESULT_OK) {
			/* stop on ERR, BREAK, CONTINUE, RETURN, EXIT */
			return result;
		}
	}
	return RESULT_OK;
}

/*
 * execute a builtin function
 * if the function didn't set 'r', then set it to 'nil'
 */
unsigned
node_exec_builtin(struct node *o, struct exec *x, struct data **r)
{

	if (!((unsigned (*)(struct exec *, struct data **))
	    o->data->val.user)(x, r)) {
		return RESULT_ERR;
	}
	if (!*r) {
		*r = data_newnil();
	}
	return RESULT_OK;
}

/*
 * return a constant
 */
unsigned
node_exec_cst(struct node *o, struct exec *x, struct data **r)
{
	*r = data_newnil();
	data_assign(*r, o->data);
	return RESULT_OK;
}

/*
 * return the value of the variable (in the node)
 */
unsigned
node_exec_var(struct node *o, struct exec *x, struct data **r)
{
	struct var *v;

	v = exec_varlookup(x, o->data->val.ref);
	if (v == NULL) {
		logx(1, "%s: %s: no such variable", x->procname, o->data->val.ref);
		return RESULT_ERR;
	}
	*r = data_newnil();
	data_assign(*r, v->data);
	return RESULT_OK;
}

/*
 * ignore the result of the expression
 * (used to ignore return values of calls)
 */
unsigned
node_exec_ignore(struct node *o, struct exec *x, struct data **r)
{
	unsigned result;

	result = node_exec(o->list, x, r);
	if (result == RESULT_ERR || result == RESULT_EXIT) {
		return result;
	}
	if (*r != NULL && (*r)->type != DATA_NIL) {
		data_print(*r);
		textout_putstr(tout, "\n");
	}
	data_delete(*r);
	*r = NULL;
	return RESULT_OK;
}

/*
 * call a procedure
 */
unsigned
node_exec_call(struct node *o, struct exec *x, struct data **r)
{
	struct proc *p;
	struct name **oldlocals, *newlocals;
	struct name *argn;
	struct node *argv;
	struct var *valist;
	char *procname_save;
	unsigned result;

	newlocals = NULL;
	result = RESULT_ERR;

	p = exec_proclookup(x, o->data->val.ref);
	if (p == NULL) {
		logx(1, "%s: no such proc", o->data->val.ref);
		goto finish;
	}
	valist = NULL;
	argv = o->list;
	for (argn = p->args; argn != NULL; argn = argn->next) {
		if (str_eq(argn->str, "...")) {
			valist = var_new(&newlocals, "...", data_newlist(NULL));
			break;
		}
		if (argv == NULL) {
			logx(1, "%s: to few arguments", o->data->val.ref);
			goto finish;
		}
		if (node_exec(argv, x, r) == RESULT_ERR) {
			goto finish;
		}
		var_new(&newlocals, argn->str, *r);
		argv = argv->next;
		*r = NULL;
	}
	if (valist == NULL && argv != NULL) {
		logx(1, "%s: to many arguments", o->data->val.ref);
		goto finish;
	}
	while (argv != NULL) {
		if (node_exec(argv, x, r) == RESULT_ERR)
			goto finish;
		data_listadd(valist->data, *r);
		argv = argv->next;
		*r = NULL;
	}
	oldlocals = x->locals;
	x->locals = &newlocals;
	procname_save = x->procname;
	x->procname = p->name.str;
	result = node_exec(p->code, x, r);
	if (result != RESULT_ERR) {
		if (*r == NULL) {	/* we always return something */
			*r = data_newnil();
		}
		if (result != RESULT_EXIT) {
			result = RESULT_OK;
		}
	}
	x->locals = oldlocals;
	x->procname = procname_save;
finish:
	var_empty(&newlocals);
	return result;
}

unsigned
node_exec_if(struct node *o, struct exec *x, struct data **r)
{
	unsigned cond, result;
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	cond = data_eval(*r);
	data_delete(*r);
	*r = NULL;
	if (cond) {
		result = node_exec(o->list->next, x, r);
		if (result != RESULT_OK) {
			return result;
		}
	} else if (o->list->next->next) {
		result = node_exec(o->list->next->next, x, r);
		if (result != RESULT_OK) {
			return result;
		}
	}
	return RESULT_OK;
}

unsigned
node_exec_for(struct node *o, struct exec *x, struct data **r)
{
	unsigned result;
	struct data *list, *i;
	struct var *v;

	if (node_exec(o->list, x, &list) == RESULT_ERR) {
		return RESULT_ERR;
	}
	if (list->type != DATA_LIST && list->type != DATA_RANGE) {
		logx(1, "%s: argument to 'for' must be a list or range", x->procname);
		return RESULT_ERR;
	}
	v = exec_varlookup(x, o->data->val.ref);
	if (v == NULL) {
		v = var_new(x->locals, o->data->val.ref, data_newnil());
	}
	result = RESULT_OK;
	if (list->type == DATA_LIST) {
		for (i = list->val.list; i != NULL; i = i->next) {
			data_assign(v->data, i);
			result = node_exec(o->list->next, x, r);
			if (result == RESULT_CONTINUE) {
				continue;
			} else if (result == RESULT_BREAK) {
				break;
			} else if (result != RESULT_OK) {
				break;
			}
		}
	} else {
		if (list->val.range.min <= list->val.range.max) {
			i = data_newlong(list->val.range.min);
			while (1) {
				data_assign(v->data, i);
				result = node_exec(o->list->next, x, r);
				if (result == RESULT_CONTINUE) {
					continue;
				} else if (result == RESULT_BREAK) {
					break;
				} else if (result != RESULT_OK) {
					break;
				}
				if (i->val.num == list->val.range.max)
					break;
				i->val.num++;
			}
			data_delete(i);
		}
	}
	data_delete(list);
	return result;
}

unsigned
node_exec_return(struct node *o, struct exec *x, struct data **r)
{
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	return RESULT_RETURN;
}

unsigned
node_exec_exit(struct node *o, struct exec *x, struct data **r)
{
	return RESULT_EXIT;
}

unsigned
node_exec_assign(struct node *o, struct exec *x, struct data **r)
{
	struct var *v;
	struct data *expr;

	if (node_exec(o->list, x, &expr) == RESULT_ERR) {
		return RESULT_ERR;
	}
	v = exec_varlookup(x, o->data->val.ref);
	if (v == NULL) {
		v = var_new(x->locals, o->data->val.ref, expr);
	} else {
		data_delete(v->data);
		v->data = expr;
	}
	return RESULT_OK;
}

/*
 * do nothing
 */
unsigned
node_exec_nop(struct node *o, struct exec *x, struct data **r)
{
	return RESULT_OK;
}

/*
 * built a list from the expression list
 */
unsigned
node_exec_list(struct node *o, struct exec *x, struct data **r)
{
	struct node *arg;
	struct data *d;

	*r = data_newlist(NULL);

	for (arg = o->list; arg != NULL; arg = arg->next) {
		if (node_exec(arg, x, &d) == RESULT_ERR) {
			data_delete(*r);
			*r = NULL;
			return RESULT_ERR;
		}
		data_listadd(*r, d);
	}
	return RESULT_OK;
}

/*
 * built a range from two integers
 */
unsigned
node_exec_range(struct node *o, struct exec *x, struct data **r)
{
	struct data *min, *max;

	if (!node_exec(o->list, x, &min))
		return RESULT_ERR;
	if (!node_exec(o->list->next, x, &max))
		return RESULT_ERR;
	if (min->type != DATA_LONG || max->type != DATA_LONG) {
		logx(1, "cannot create a range with non integers");
		return RESULT_ERR;
	}
	if (min->val.num > max->val.num) {
		logx(1, "max > min, cant create a valid range");
		return RESULT_ERR;
	}
	*r = data_newrange(min->val.num, max->val.num);
	return RESULT_OK;
}

unsigned
node_exec_eq(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_eq);
}

unsigned
node_exec_neq(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_neq);
}

unsigned
node_exec_le(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_le);
}

unsigned
node_exec_lt(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_lt);
}


unsigned
node_exec_ge(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_ge);
}

unsigned
node_exec_gt(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_gt);
}

unsigned
node_exec_and(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_and);
}

unsigned
node_exec_or(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_binary(o, x, r, data_or);
}

unsigned
node_exec_not(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_unary(o, x, r, data_not);
}

unsigned
node_exec_add(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_binary(o, x, r, data_add);
}

unsigned
node_exec_sub(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_binary(o, x, r, data_sub);
}

unsigned
node_exec_mul(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_binary(o, x, r, data_mul);
}

unsigned
node_exec_div(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_div);
}

unsigned
node_exec_mod(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_binary(o, x, r, data_mod);
}

unsigned
node_exec_neg(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_unary(o, x, r, data_neg);
}

unsigned
node_exec_lshift(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_binary(o, x, r, data_lshift);
}

unsigned
node_exec_rshift(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_binary(o, x, r, data_rshift);
}

unsigned
node_exec_bitand(struct node *o, struct exec *x, struct data **r)
{

	return node_exec_binary(o, x, r, data_bitand);
}

unsigned
node_exec_bitor(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_bitor);
}

unsigned
node_exec_bitxor(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_binary(o, x, r, data_bitxor);
}

unsigned
node_exec_bitnot(struct node *o, struct exec *x, struct data **r)
{
	return node_exec_unary(o, x, r, data_bitnot);
}

struct node_vmt
node_vmt_proc = { "proc", node_exec_proc },
node_vmt_slist = { "slist", node_exec_slist },
node_vmt_cst = { "cst", node_exec_cst },
node_vmt_var = { "var", node_exec_var },
node_vmt_call = { "call", node_exec_call },
node_vmt_ignore = { "ignore", node_exec_ignore },
node_vmt_builtin = { "builtin", node_exec_builtin },
node_vmt_if = { "if", node_exec_if },
node_vmt_for = { "for", node_exec_for },
node_vmt_return = { "return", node_exec_return },
node_vmt_exit = { "exit", node_exec_exit },
node_vmt_assign = { "assign", node_exec_assign },
node_vmt_nop = { "nop", node_exec_nop },
node_vmt_list = { "list", node_exec_list},
node_vmt_range = { "range", node_exec_range},
node_vmt_eq = { "eq", node_exec_eq },
node_vmt_neq = { "neq", node_exec_neq },
node_vmt_le = { "le", node_exec_le },
node_vmt_lt = { "lt", node_exec_lt },
node_vmt_ge = { "ge", node_exec_ge },
node_vmt_gt = { "gt", node_exec_gt },
node_vmt_and = { "and", node_exec_and },
node_vmt_or = { "or", node_exec_or },
node_vmt_not = { "not", node_exec_not },
node_vmt_add = { "add", node_exec_add },
node_vmt_sub = { "sub", node_exec_sub },
node_vmt_mul = { "mul", node_exec_mul },
node_vmt_div = { "div", node_exec_div },
node_vmt_mod = { "mod", node_exec_mod },
node_vmt_neg = { "neg", node_exec_neg },
node_vmt_lshift = { "lshift", node_exec_lshift },
node_vmt_rshift = { "rshift", node_exec_rshift },
node_vmt_bitand = { "bitand", node_exec_bitand },
node_vmt_bitor = { "bitor", node_exec_bitor },
node_vmt_bitxor = { "bitxor", node_exec_bitxor },
node_vmt_bitnot = { "bitnot", node_exec_bitnot };
