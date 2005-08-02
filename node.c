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

#include "dbg.h"
#include "str.h"
#include "data.h"
#include "node.h"
#include "exec.h"
#include "cons.h"

struct node_s *
node_new(struct node_vmt_s *vmt, struct data_s *data) {
	struct node_s *o;
	o = (struct node_s *)mem_alloc(sizeof(struct node_s));
	o->vmt = vmt;
	o->data = data;
	o->list = o->next = 0;
	return o;
}

void
node_delete(struct node_s *o) {
	struct node_s *i, *inext;
	if (o == 0) {
		return;
	}
	for (i = o->list; i != 0; i = inext) {
		inext = i->next;
		node_delete(i);
	}
	if (o->data) {
		data_delete(o->data);
	}
	mem_free(o);
}

void
node_dbg(struct node_s *o, unsigned depth) {
#define NODE_MAXDEPTH 30
	static char str[2 * NODE_MAXDEPTH + 1] = "";
	struct node_s *i;
	
	dbg_puts(str);
	dbg_puts(o->next ? "+-" : "\\-");
	if (o == 0) {
		dbg_puts("<EMPTY>\n");
		return;
	}
	dbg_puts(o->vmt->name);
	if (o->data) {
		dbg_puts("(");
		data_dbg(o->data);
		dbg_puts(")");
	}
	dbg_puts(depth >= NODE_MAXDEPTH && o->list ? "[...]\n" : "\n");
	if (depth < NODE_MAXDEPTH) {
		str[2 * depth] = o->next ? '|' : ' ';
		str[2 * depth + 1] = ' ';
		str[2 * depth + 2] = '\0';
		for (i = o->list; i != 0; i = i->next) {
			node_dbg(i, depth + 1);
		}
		str[2 * depth] = '\0';
	}
#undef NODE_MAXDEPTH
}


void
node_insert(struct node_s **n, struct node_s *e) {
	e->next = *n;
	*n = e;
}

void
node_replace(struct node_s **n, struct node_s *e) {
	if (e->list != 0) {
		dbg_puts("node_replace: e->list != 0\n");
		dbg_panic();
	}
	e->list = *n;
	e->next = (*n)->next;
	(*n)->next = 0;
	*n = e;
}


	/*
	 * run a node.
	 * the following rule must be respected
	 * 1) node_exec must be called always with *r == 0
	 * 2) in statements (slist,...) *r != 0 if and only if RETURN
	 * 3) in expressions (add, ...) *r == 0 if and only if ERROR
	 */


unsigned
node_exec(struct node_s *o, struct exec_s *x, struct data_s **r) {
	unsigned result;
	if (x->depth == EXEC_MAXDEPTH) {
		cons_err("too many nested operations");
		return RESULT_ERR;
	}
	*r = 0;
	x->depth++;
	result = o->vmt->exec(o, x, r);
	x->depth--;
	if (result == RESULT_ERR && *r) {
		data_delete(*r);
		*r = 0;
	}
	return result;
}

unsigned
node_exec_unary(struct node_s *o, struct exec_s *x, struct data_s **r, 
	unsigned (*func)(struct data_s *)) { 
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	if (!func(*r)) {
		return RESULT_ERR;
	}
	return RESULT_OK;
}

unsigned
node_exec_binary(struct node_s *o, struct exec_s *x, struct data_s **r,
	unsigned (*func)(struct data_s *, struct data_s *)) { 
	struct data_s *lhs;
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	lhs = *r;
	*r = 0;
	if (node_exec(o->list->next, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	if (!func(lhs, *r)) {
		return RESULT_ERR;
	}
	data_delete(*r);
	*r = lhs;
	return RESULT_OK;
}


/* ------------------------------------------------------------------ */


unsigned
node_exec_proc(struct node_s *o, struct exec_s *x, struct data_s **r) {
	struct proc_s *p;
	struct node_s *a;
	struct name_s *args;

	args = 0;
	for (a = o->list->list; a != 0; a = a->next) {
		if (name_lookup(&args, a->data->val.ref)) {
			cons_err("duplicate arguments in proc definition");
			return RESULT_ERR;
		}
		name_add(&args, name_new(a->data->val.ref));
	}
	p = exec_proclookup(x, o->data->val.ref);
	if (p != 0) {
		name_empty(&p->args);
		node_delete(p->code);
	} else {
		p = proc_new(o->data->val.ref);
		name_insert((struct name_s **)&x->procs, (struct name_s *)p);
	}
	p->args = args;
	p->code = o->list->next;
	o->list->next = 0;
	return RESULT_OK;
}

unsigned
node_exec_alist(struct node_s *o, struct exec_s *x, struct data_s **r) {
	dbg_puts("node_exec_alist should not be executed\n");
	return RESULT_ERR;
}

unsigned
node_exec_slist(struct node_s *o, struct exec_s *x, struct data_s **r) {
	struct node_s *i;
	unsigned result;
	
	for (i = o->list; i != 0; i = i->next) {
		result = node_exec(i, x, r);
		if (result != RESULT_OK) {
			return result;
		}
	}
	return RESULT_OK;
}

unsigned
node_exec_builtin(struct node_s *o, struct exec_s *x, struct data_s **r) {	
	if (!((unsigned (*)(struct exec_s *, struct data_s **))
	    o->data->val.user)(x, r)) {
		return RESULT_ERR;
	}
	if (!*r) {
		*r = data_newnil();
	}
	return RESULT_OK;		
}

unsigned
node_exec_cst(struct node_s *o, struct exec_s *x, struct data_s **r) {
	*r = data_newnil();
	data_assign(*r, o->data);
	return RESULT_OK;
}

unsigned
node_exec_var(struct node_s *o, struct exec_s *x, struct data_s **r) {
	struct var_s *v;
	
	v = exec_varlookup(x, o->data->val.ref);
	if (v == 0) {
		cons_errss(x->procname, o->data->val.ref, "no such variable");
		return RESULT_ERR;
	}
	*r = data_newnil();
	data_assign(*r, v->data);
	return RESULT_OK;
}

unsigned
node_exec_ignore(struct node_s *o, struct exec_s *x, struct data_s **r) {
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	data_delete(*r);
	*r = 0;
	return RESULT_OK;
}


unsigned
node_exec_call(struct node_s *o, struct exec_s *x, struct data_s **r) {
	struct proc_s *p;
	struct var_s  **oldlocals, *newlocals;
	struct name_s *argn;
	struct node_s *argv;
	char *procname_save;
	unsigned result;
	
	newlocals = 0;
	result = RESULT_ERR;
	
	p = exec_proclookup(x, o->data->val.ref);
	if (p == 0) {
		cons_errs(o->data->val.ref, "no such proc");
		goto finish;
	}
	argv = o->list;
	for (argn = p->args; argn != 0; argn = argn->next) {
		if (argv == 0) {
			cons_errs(o->data->val.ref, "to few arguments");
			goto finish;
		}
		if (node_exec(argv, x, r) == RESULT_ERR) {
			goto finish;
		}
		var_insert(&newlocals, var_new(argn->str, *r));
		argv = argv->next;
		*r = 0;
	}
	if (argv != 0) {
		cons_errs(o->data->val.ref, "to many arguments");
		goto finish;
	}	
	oldlocals = x->locals;
	x->locals = &newlocals;
	procname_save = x->procname;
	x->procname = p->name.str;
	if (node_exec(p->code, x, r) != RESULT_ERR) {
		if (*r == 0) {			/* always return something */
			*r = data_newnil();
		}
		result = RESULT_OK;
	}
	x->locals = oldlocals;
	x->procname = procname_save;
finish:
	var_empty(&newlocals);
	return result;
}

unsigned
node_exec_if(struct node_s *o, struct exec_s *x, struct data_s **r) {
	unsigned cond, result;
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	cond = data_eval(*r);
	data_delete(*r);
	*r = 0;
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
node_exec_for(struct node_s *o, struct exec_s *x, struct data_s **r) {
	unsigned result;
	struct data_s *list, *i;
	struct var_s *v;
	if (node_exec(o->list, x, &list) == RESULT_ERR) {
		return RESULT_ERR;
	}
	if (list->type != DATA_LIST) {
		cons_errs(x->procname, "argument to 'for' must be a list");
		return RESULT_ERR;
	}
	v = exec_varlookup(x, o->data->val.ref);
	if (v == 0) {
		v = var_new(o->data->val.ref, data_newnil());
		var_insert(x->locals, v);
	}
	result = RESULT_OK;
	for (i = list->val.list; i != 0; i = i->next) {
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
	data_delete(list);
	return result;
}

unsigned 
node_exec_return(struct node_s *o, struct exec_s *x, struct data_s **r) {
	if (node_exec(o->list, x, r) == RESULT_ERR) {
		return RESULT_ERR;
	}
	return RESULT_RETURN;
}

unsigned
node_exec_assign(struct node_s *o, struct exec_s *x, struct data_s **r) {
	struct var_s *v;
	struct data_s *expr;

	if (node_exec(o->list, x, &expr) == RESULT_ERR) {
		return RESULT_ERR;
	}
	v = exec_varlookup(x, o->data->val.ref);
	if (v == 0) {
		v = var_new(o->data->val.ref, expr);
		var_insert(x->locals, v);
	} else {
		data_delete(v->data);
		v->data = expr;
	}
	return RESULT_OK;
}

unsigned
node_exec_nop(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return RESULT_OK;
}

unsigned
node_exec_list(struct node_s *o, struct exec_s *x, struct data_s **r) {
	struct node_s *arg;
	struct data_s *d;
	
	*r = data_newlist(0);

	for (arg = o->list; arg != 0; arg = arg->next) {
		if (node_exec(arg, x, &d) == RESULT_ERR) {
			data_delete(*r);
			*r = 0;
			return RESULT_ERR;
		}
		data_listadd(*r, d);
	}
	return RESULT_OK;
}

unsigned
node_exec_eq(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_eq);
}

unsigned
node_exec_neq(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_neq);
}

unsigned
node_exec_le(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_le);
}

unsigned
node_exec_lt(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_lt);
}


unsigned
node_exec_ge(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_ge);
}

unsigned
node_exec_gt(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_gt);
}

unsigned
node_exec_and(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_and);
}

unsigned
node_exec_or(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_binary(o, x, r, data_or);
}

unsigned
node_exec_not(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_unary(o, x, r, data_not);
}

unsigned
node_exec_add(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_binary(o, x, r, data_add);
}

unsigned
node_exec_sub(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_binary(o, x, r, data_sub);
}

unsigned
node_exec_mul(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_binary(o, x, r, data_mul);
}

unsigned
node_exec_div(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_div);
}

unsigned
node_exec_mod(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_binary(o, x, r, data_mod);
}

unsigned
node_exec_neg(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_unary(o, x, r, data_neg);
}

unsigned
node_exec_lshift(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_binary(o, x, r, data_lshift);
}

unsigned
node_exec_rshift(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_binary(o, x, r, data_rshift);
}

unsigned
node_exec_bitand(struct node_s *o, struct exec_s *x, struct data_s **r) { 
	return node_exec_binary(o, x, r, data_bitand);
}

unsigned
node_exec_bitor(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_bitor);
}

unsigned
node_exec_bitxor(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_binary(o, x, r, data_bitxor);
}

unsigned
node_exec_bitnot(struct node_s *o, struct exec_s *x, struct data_s **r) {
	return node_exec_unary(o, x, r, data_bitnot);
}

struct node_vmt_s 
node_vmt_proc = { "proc", node_exec_proc },
node_vmt_alist = { "alist", node_exec_alist },
node_vmt_slist = { "slist", node_exec_slist },
node_vmt_cst = { "cst", node_exec_cst },
node_vmt_var = { "var", node_exec_var },
node_vmt_call = { "call", node_exec_call },
node_vmt_ignore = { "ignore", node_exec_ignore },
node_vmt_builtin = { "builtin", node_exec_builtin },
node_vmt_if = { "if", node_exec_if },
node_vmt_for = { "for", node_exec_for },
node_vmt_return = { "return", node_exec_return },
node_vmt_assign = { "assign", node_exec_assign },
node_vmt_nop = { "nop", node_exec_nop },
node_vmt_list = { "list", node_exec_list},
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
