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
 * implements the semantic tree 
 * and all execution primitives 
 */

#include "dbg.h"
#include "tree.h"
#include "data.h"

#include "user.h"	/* for user_printstr */

unsigned tree_debug = 0;

void
exec_error(struct exec_s *o, char *str) {
	user_printstr(str);
}

/* ----------------------------------------------- variable lists --- */

struct var_s *
var_new(char *name, struct data_s *data) {
	struct var_s *o;
	o = (struct var_s *)mem_alloc(sizeof(struct var_s));
	name_init(&o->name, name);
	o->data = data;
	return o;
}

void
var_delete(struct var_s *o) {
	if (o->data != 0) {
		data_delete(o->data);
	}
	name_done(&o->name);
	mem_free(o);
}

void
var_dbg(struct var_s *o) {
	str_dbg(o->name.str);
	dbg_puts(" = ");
	if (o->data != 0)
		data_dbg(o->data);
	else
		dbg_puts("<null>");
}

struct var_s *
var_lookup(struct var_s **first, char *name) {
	return (struct var_s *)name_lookup((struct name_s **)first, name);
}

void
var_insert(struct var_s **first, struct var_s *i) {
	name_insert((struct name_s **)first, (struct name_s *)i);
}

void
var_empty(struct var_s **first) {
	struct var_s *i, *inext;
	for (i = *first; i != 0; i = inext) {
		inext = (struct var_s *)i->name.next;
		var_delete(i);
	}
	*first = 0;
}

struct proc_s *
proc_new(char *name) {
	struct proc_s *o;
	o = (struct proc_s *)mem_alloc(sizeof(struct proc_s));
	name_init(&o->name, name);
	o->args = 0;
	o->type = 0;
	tree_init(&o->code.tree);
	return o;
}

void
proc_delete(struct proc_s *o) {
	if (!(o->type & PROC_BUILTIN)) {
		tree_done(&o->code.tree);
	}
	name_empty(&o->args);
	name_done(&o->name);
	mem_free(o);
}

struct proc_s *
proc_lookup(struct proc_s **first, char *name) {
	return (struct proc_s *)name_lookup((struct name_s **)first, name);
}

void
proc_empty(struct proc_s **first) {
	struct proc_s *i, *inext;
	for (i = *first; i != 0; i = inext) {
		inext = (struct proc_s *)i->name.next;
		proc_delete(i);
	}
	*first = 0;
}

void
proc_dbg(struct proc_s *o) {
	struct name_s *i;
	str_dbg(o->name.str);
	dbg_puts("(");
	if (o->args) {
		i = o->args;
		for (;;) {
			dbg_puts(i->str);
			i = i->next;
			if (!i) {
				break;
			}
			dbg_puts(", ");
		}
	}
	dbg_puts(")\n");
	if (o->type & PROC_BUILTIN) {
		dbg_puts("<builtin>\n");
	} else {
		tree_dbg(&o->code.tree, 1);
	}
}
		

struct exec_s *
exec_new(void) {
	struct exec_s *o;
	o = (struct exec_s *)mem_alloc(sizeof(struct exec_s));
	o->procs = 0;
	o->globals = 0;
	o->locals = &o->globals;
	o->acc = 0;
	o->depth = 0;
	return o;
}


void
exec_delete(struct exec_s *o) {
	if (o->depth != 0) {
		dbg_puts("exec_done: depth != 0\n");
		dbg_panic();
	}
	var_empty(&o->globals);
	proc_empty(&o->procs);
	if (o->acc) {
		dbg_puts("exec_done: unused data");
		data_dbg(o->acc);
		dbg_puts("\n");
		data_delete(o->acc);
		o->acc = 0;
	}
	mem_free(o);
}


struct proc_s *
exec_proclookup(struct exec_s *o, char *name) {
	return (struct proc_s *)name_lookup((struct name_s **)&o->procs, name);
}


struct var_s *
exec_varlookup(struct exec_s *o, char *name) {
	struct var_s *var;
	var = (struct var_s *)name_lookup((struct name_s **)o->locals, name);
	if (var != 0) {
		return var;
	}
	if (o->locals != &o->globals) {
		var = (struct var_s *)name_lookup((struct name_s **)&o->globals, name);
		if (var != 0) {
			return var;
		}
	}
	return 0;
}

struct data_s *
exec_getacc(struct exec_s *o) {
	struct data_s *val;
	val = o->acc;
	if (val == 0) {
		dbg_puts("exec_getacc: no data\n");
		dbg_panic();
	}
	o->acc = 0;
	return val;
}

void
exec_putacc(struct exec_s *o, struct data_s *val) {
	if (o->acc) {
		dbg_puts("exec_putacc: already used\n");
		dbg_panic();
	}
	o->acc = val;
}

void
exec_newbuiltin(struct exec_s *o, char *name,
    unsigned func(struct exec_s *exec), struct name_s *args) {
	struct proc_s *newp;
	
	newp = proc_new(name);
	newp->type = PROC_BUILTIN;
	newp->code.func = func;
	newp->args = args;
	name_add((struct name_s **)&o->procs, (struct name_s *)newp);
}

void
exec_newvar(struct exec_s *o, char *name, struct data_s *val) {
	name_insert((struct name_s **)&o->globals,
		    (struct name_s *)var_new(name, val));
}


void
exec_dumpprocs(struct exec_s *o) {
	struct proc_s *p;
	struct name_s *n;
	for (p = o->procs; p != 0; p = (struct proc_s *)p->name.next) {
		dbg_puts(p->name.str);
		dbg_puts("(");
		for (n = p->args; n != 0; n = n->next) {
			dbg_puts(n->str);
			if (n->next) {
				dbg_puts(", ");
			}		
		}
		dbg_puts(")\n");
	}
}


void
exec_dumpvars(struct exec_s *o) {
}


unsigned 
exec_lookupname(struct exec_s *exec, char *name, char **val) {
	struct var_s *var;
	var = exec_varlookup(exec, name);
	if (var == 0 || var->data->type != DATA_REF) {
		exec_error(exec, name);
		exec_error(exec, ": no such object name\n");
		return 0;
	}
	*val = var->data->val.ref;
	return 1;
}


unsigned
exec_lookupstring(struct exec_s *exec, char *name, char **val) {
	struct var_s *var;
	var = exec_varlookup(exec, name);
	if (var == 0 || var->data->type != DATA_STRING) {
		exec_error(exec, name);
		exec_error(exec, ": no such string\n");
		return 0;
	}
	*val = var->data->val.str;
	return 1;
}


unsigned
exec_lookuplong(struct exec_s *exec, char *name, long *val) {
	struct var_s *var;
	var = exec_varlookup(exec, name);
	if (var == 0 || var->data->type != DATA_LONG) {
		exec_error(exec, name);
		exec_error(exec, ": no such long\n");
		return 0;
	}
	*val = var->data->val.num;
	return 1;
}

unsigned
exec_lookupbool(struct exec_s *exec, char *name, long *val) {
	struct var_s *var;
	unsigned res;
	
	var = exec_varlookup(exec, name);
	if (var == 0) {
		exec_error(exec, name);
		exec_error(exec, ": no such bool\n");
		return 0;
	}
	res = data_eval(var->data);
	*val = res;
	return 1;
}


/* --------------------------------------------------------- node --- */

unsigned node_exec_unop(struct node_s *o, struct exec_s *exec, unsigned (*func)(struct data_s *));
unsigned node_exec_binop(struct node_s *o, struct exec_s *exec, unsigned (*func)(struct data_s *, struct data_s *));
unsigned node_exec_call(struct node_s *o, struct exec_s *exec);


struct node_s *
node_newconst(struct data_s *data) {
	struct node_s *o;
	o = mem_alloc(sizeof(struct node_s));
	o->id = NODE_CONST;
	o->param.cst.data = data;
	o->next = 0;
	return o;
}


struct node_s *
node_newcall(char *name, struct tree_s *args) {
	struct node_s *o;
	o = mem_alloc(sizeof(struct node_s));
	o->id = NODE_CALL;
	o->param.call.name = name;
	tree_init(&o->param.call.args);
	tree_move(&o->param.call.args, args);
	o->next = 0;
	return o;
}


struct node_s *
node_newloop(char *name, struct node_s *expr) {
	struct node_s *o;
	o = mem_alloc(sizeof(struct node_s));
	o->id = NODE_FOR;
	o->param.loop.name = name;
	o->param.loop.expr = expr;
	tree_init(&o->param.loop.stmt);
	o->next = 0;
	return o;
}

struct node_s *
node_newcond(struct node_s *expr) {
	struct node_s *o;
	o = mem_alloc(sizeof(struct node_s));
	o->id = NODE_IF;
	o->param.cond.expr = expr;
	tree_init(&o->param.cond.stmt1);
	tree_init(&o->param.cond.stmt2);
	o->next = 0;
	return o;
}

struct node_s *
node_newop(unsigned id, struct node_s *expr1, struct node_s *expr2) {
	struct node_s *o;
	o = mem_alloc(sizeof(struct node_s));
	o->id = id;
	o->param.op.expr1 = expr1;
	o->param.op.expr2 = expr2;
	o->next = 0;
	return o;
}


struct node_s *
node_newlist(struct tree_s *args) {
	struct node_s *o;
	o = mem_alloc(sizeof(struct node_s));
	o->id = NODE_LIST;
	tree_init(&o->param.call.args);
	tree_move(&o->param.call.args, args);
	o->next = 0;
	return o;
}

struct node_s *
node_newvar(char *name) {
	struct node_s *o;
	o = mem_alloc(sizeof(struct node_s));
	o->id = NODE_VAR;
	o->param.var.name = name;
	o->next = 0;
	return o;
}


struct node_s *
node_newassign(char *name, struct node_s *expr) {
	struct node_s *o;
	o = mem_alloc(sizeof(struct node_s));
	o->id = NODE_ASSIGN;
	o->param.assign.name = name;
	o->param.assign.expr = expr;
	o->next = 0;
	return o;
}


void
node_dbg(struct node_s *o, unsigned depth) {
	unsigned i;
	
	for (i = 0; i < depth; i++) {
		dbg_puts("  ");
	}
	switch(o->id) {
	case NODE_CONST:
		dbg_puts("CONST(");
		data_dbg(o->param.cst.data);
		dbg_puts(")\n");		
		break;
	case NODE_VAR:
		dbg_puts("$");
		str_dbg(o->param.var.name);
		dbg_puts("\n");
		break;
	case NODE_ASSIGN:
		dbg_puts("ASSIGN(");
		str_dbg(o->param.assign.name);
		dbg_puts(")\n");
		node_dbg(o->param.assign.expr, depth + 1);		
		break;
	case NODE_CALL:
		dbg_puts("CALL(");
		dbg_puts(o->param.call.name);
		dbg_puts(")\n");
		tree_dbg(&o->param.call.args, depth + 1);
		break;
	case NODE_LIST:
		dbg_puts("LIST\n");
		tree_dbg(&o->param.call.args, depth + 1);
		break;
	case NODE_FOR:
		dbg_puts("FOR(");
		dbg_puts(o->param.loop.name);
		dbg_puts(")\n");
		node_dbg(o->param.loop.expr, depth + 1);
		dbg_puts("\n");
		tree_dbg(&o->param.loop.stmt, depth + 1);
		break;
	case NODE_IF:
		dbg_puts("IF\n");
		node_dbg(o->param.cond.expr, depth + 1);
		dbg_puts("\n");
		tree_dbg(&o->param.cond.stmt1, depth + 1);
		break;
	case NODE_IFELSE:
		dbg_puts("IFELSE\n");
		node_dbg(o->param.cond.expr, depth + 1);
		dbg_puts("\n");
		tree_dbg(&o->param.cond.stmt1, depth + 1);
		dbg_puts("\n");
		tree_dbg(&o->param.cond.stmt2, depth + 1);
		break;
	case NODE_ADD:
		dbg_puts("ADD\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_SUB:
		dbg_puts("SUB\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_MUL:
		dbg_puts("MUL\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_DIV:
		dbg_puts("DIV\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_MOD:
		dbg_puts("MOD\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_NEG:
		dbg_puts("NEG\n");
		node_dbg(o->param.op.expr1, depth + 1);
		break;
	case NODE_LSHIFT:
		dbg_puts("LSHIFT\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_RSHIFT:
		dbg_puts("RSHIFT\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_BITAND:
		dbg_puts("BITAND\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_BITOR:
		dbg_puts("BITOR\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_BITXOR:
		dbg_puts("BITXOR\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_BITNOT:
		dbg_puts("BITNOT\n");
		node_dbg(o->param.op.expr1, depth + 1);
		break;
	case NODE_AND:
		dbg_puts("AND\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_OR:
		dbg_puts("OR\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_NOT:
		dbg_puts("NOT\n");
		node_dbg(o->param.op.expr1, depth + 1);
		break;
	case NODE_EQ:
		dbg_puts("EQ\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_LT:
		dbg_puts("LT\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_LE:
		dbg_puts("LE\n");
		node_dbg(o->param.op.expr1, depth + 1);
		node_dbg(o->param.op.expr2, depth + 1);
		break;
	case NODE_RETURN:
		dbg_puts("RETURN\n");
		node_dbg(o->param.op.expr1, depth + 1);
		break;
	default:
		dbg_puts("UNKNOWN\n");
	}
}

void
node_delete(struct node_s *o) {
	switch(o->id) {
	case NODE_CONST:
		data_delete(o->param.cst.data);
		break;
	case NODE_VAR:
		str_delete(o->param.var.name);
		break;
	case NODE_ASSIGN:
		str_delete(o->param.assign.name);
		node_delete(o->param.assign.expr);
		break;
	case NODE_CALL:
		str_delete(o->param.call.name);
		tree_done(&o->param.call.args);
		break;
	case NODE_LIST:
		tree_done(&o->param.call.args);
		break;
	case NODE_FOR:
		str_delete(o->param.loop.name);
		tree_done(&o->param.loop.stmt);
		node_delete(o->param.loop.expr);
		break;
	case NODE_IF:
	case NODE_IFELSE:
		node_delete(o->param.cond.expr);
		tree_done(&o->param.cond.stmt1);
		tree_done(&o->param.cond.stmt2);
		break;
	case NODE_ADD:
	case NODE_SUB:
	case NODE_MUL:
	case NODE_DIV:
	case NODE_MOD:
	case NODE_LSHIFT:
	case NODE_RSHIFT:
	case NODE_BITAND:
	case NODE_BITOR:
	case NODE_BITXOR:
	case NODE_AND:
	case NODE_OR:
	case NODE_EQ:
	case NODE_LT:
	case NODE_LE:
		node_delete(o->param.op.expr1);
		node_delete(o->param.op.expr2);
		break;
	case NODE_NEG:
	case NODE_BITNOT:
	case NODE_NOT:
	case NODE_RETURN:
		node_delete(o->param.op.expr1);
		break;
	default:
		dbg_puts("node_delete: not implemented\n");
		node_dbg(o, 0);
		dbg_panic();
	}
	mem_free(o);
}


unsigned
node_exec_unop(struct node_s *o, struct exec_s *exec,
    unsigned (*func)(struct data_s *)) {
	struct data_s *expr1;

	if (!node_exec(o->param.op.expr1, exec)) {
		return 0;
	}
	expr1 = exec_getacc(exec);
	if (func(expr1)) {
		exec_putacc(exec, expr1);
		return RESULT_SUCCESS;
	}
	data_delete(expr1);
	return 0;
}

unsigned
node_exec_binop(struct node_s *o, struct exec_s *exec,
    unsigned (*func)(struct data_s *, struct data_s *)) {
	struct data_s *expr1, *expr2;

	if (!node_exec(o->param.op.expr1, exec)) {
		return 0;
	}
	expr1 = exec_getacc(exec);
	if (!node_exec(o->param.op.expr2, exec)) {
		data_delete(expr1);
		return 0;
	}
	expr2 = exec_getacc(exec);
	if (func(expr1, expr2)) {
		data_delete(expr2);
		exec_putacc(exec, expr1);
		return RESULT_SUCCESS;
	}
	data_delete(expr1);
	data_delete(expr2);
	return 0;
}

unsigned
node_exec_assign(struct node_s *o, struct exec_s *exec) {
	struct data_s *expr;
	struct var_s *v;

	if (!node_exec(o->param.assign.expr, exec)) {
		return 0;
	}
	expr = exec_getacc(exec);
	v = exec_varlookup(exec, o->param.assign.name);
	if (v == 0) {
		v = var_new(o->param.assign.name, expr);
		var_insert(exec->locals, v);
	} else {
		data_delete(v->data);
		v->data = expr;
	}
	return 1;
}

unsigned
node_exec_call(struct node_s *o, struct exec_s *exec) {
	struct proc_s *proc;
	struct var_s  **oldlocals, *newlocals;
	struct name_s *argn;
	struct node_s *argv;
	
	oldlocals = exec->locals;
	newlocals = 0;
	
	proc = exec_proclookup(exec, o->param.call.name);
	if (proc == 0) {
		exec_error(exec, o->param.call.name);
		exec_error(exec, ": no such proc\n");
		return 0;
	}
	argv = o->param.call.args.first;
	for (argn = proc->args; argn != 0; argn = argn->next) {
		if (argv == 0) {
			exec_error(exec, "to few arguments\n");
			var_empty(&newlocals);
			return 0;
		}
		if (!node_exec(argv, exec)) {
			var_empty(&newlocals);
			return 0;
		}
		var_insert(&newlocals, var_new(argn->str, exec_getacc(exec)));
		argv = argv->next;
	}
	if (argv != 0) {
		exec_error(exec, "to many arguments\n");
		var_empty(&newlocals);
		return 0;
	}
	exec->locals = &newlocals;
	if (proc->type & PROC_BUILTIN) {
		if (!proc->code.func(exec)) {
			var_empty(exec->locals);
			exec->locals = oldlocals;
			return 0;
		}
	} else {
		if (!tree_exec(&proc->code.tree, exec)) {
			var_empty(exec->locals);
			exec->locals = oldlocals;
			return 0;
		}
	}
	var_empty(exec->locals);
	exec->locals = oldlocals;
	if (exec->acc == 0) {
		exec_putacc(exec, data_newnil());
	}
	return RESULT_SUCCESS;
}


unsigned
node_exec_list(struct node_s *o, struct exec_s *exec) {
	struct node_s *arg;
	struct data_s *d;
	
	d = data_newlist(0);

	for (arg = o->param.call.args.first; arg != 0; arg = arg->next) {
		if (!node_exec(arg, exec)) {
			data_delete(d);
			return 0;
		}
		data_listadd(d, exec_getacc(exec));
	}
	exec_putacc(exec, d);
	return RESULT_SUCCESS;
}

unsigned
node_exec_loop(struct node_s *o, struct exec_s *exec) {
	unsigned res;
	struct data_s *list, *i;
	struct var_s *v;
	if (!node_exec(o->param.loop.expr, exec)) {
		return 0;
	}
	list = exec_getacc(exec);
	if (list->type != DATA_LIST) {
		exec_error(exec, "for: argument in not a list\n");
		data_delete(list);
		return 0;
	}
	for (i = list->val.list; i != 0; i = i->next) {
		v = exec_varlookup(exec, o->param.loop.name);
		if (v == 0) {
			v = var_new(o->param.loop.name, data_newnil());
			var_insert(exec->locals, v);
		}
		data_assign(v->data, i);
		res = tree_exec(&o->param.loop.stmt, exec);
		if (!res || res == RESULT_RETURN) {
			data_delete(list);
			return res;
		}
		data_delete(exec_getacc(exec));
	}
	data_delete(list);
	return RESULT_SUCCESS;
}

unsigned
node_exec_var(struct node_s *o, struct exec_s *exec) {
	struct data_s *d;
	struct var_s *v;
	
	v = exec_varlookup(exec, o->param.var.name);
	if (v == 0) {
		exec_error(exec, o->param.var.name);
		exec_error(exec, ": no such variable\n");
		return 0;
	}
	d = data_newnil();
	data_assign(d, v->data);
	exec_putacc(exec, d);
	return 1;
}

unsigned
node_exec(struct node_s *o, struct exec_s *exec) {
	struct data_s *d;
	unsigned res;
	
	if (exec->depth > EXEC_MAXDEPTH) {
		exec_error(exec, "max execution depth reached\n");
		return 0;
	}
	exec->depth++;
	
	switch(o->id) {
	case NODE_CONST:
		d = data_newnil();
		data_assign(d, o->param.cst.data);
		exec_putacc(exec, d);
		goto success;
	case NODE_VAR:
		if (node_exec_var(o, exec)) {
			goto success;
		}
		break;
	case NODE_LIST:
		if (node_exec_list(o, exec)) {
			goto success;
		}
		break;
	case NODE_ASSIGN:
		if (node_exec_assign(o, exec)) {
			goto success;
		}
		break;
	case NODE_FOR:
		res = node_exec_loop(o, exec);
		if (res != RESULT_SUCCESS) {
			exec->depth--;
			return res;
		}			
		goto success;
	case NODE_IF:
		if (!node_exec(o->param.cond.expr, exec)) {
			goto error;
		}
		d = exec_getacc(exec);
		res = data_eval(d);
		data_delete(d);
		if (res) {
			res = tree_exec(&o->param.cond.stmt1, exec);
			if (res != RESULT_SUCCESS) {
				exec->depth--;
				return res;
			}
		}
		goto success;
	case NODE_IFELSE:
		if (!node_exec(o->param.cond.expr, exec)) {
			goto error;
		}
		d = exec_getacc(exec);
		res = data_eval(d);
		data_delete(d);
		if (res) {
			res = tree_exec(&o->param.cond.stmt1, exec);
			if (res != RESULT_SUCCESS) {
				exec->depth--;
				return res;
			}
		} else {
			res = tree_exec(&o->param.cond.stmt2, exec);
			if (res != RESULT_SUCCESS) {
				exec->depth--;
				return res;
			}
		}
		goto success;
	case NODE_CALL:
		if (node_exec_call(o, exec)) {
			goto success;
		}
		break;
	case NODE_RETURN:
		if (!node_exec(o->param.op.expr1, exec)) {
			goto error;
		}
		exec->depth--;
		return RESULT_RETURN;
	case NODE_ADD:
		if (node_exec_binop(o, exec, data_add)) {
			goto success;
		}
		break;
	case NODE_SUB:
		if (node_exec_binop(o, exec, data_sub)) {
			goto success;
		}
		break;
	case NODE_MUL:
		if (node_exec_binop(o, exec, data_mul)) {
			goto success;
		}
		break;
	case NODE_DIV:
		if (node_exec_binop(o, exec, data_div)) {
			goto success;
		}
		break;
	case NODE_MOD:
		if (node_exec_binop(o, exec, data_mod)) {
			goto success;
		}
		break;
	case NODE_LSHIFT:
		if (node_exec_binop(o, exec, data_lshift)) {
			goto success;
		}
		break;
	case NODE_RSHIFT:
		if (node_exec_binop(o, exec, data_rshift)) {
			goto success;
		}
		break;
	case NODE_BITAND:
		if (node_exec_binop(o, exec, data_bitand)) {
			goto success;
		}
		break;
	case NODE_BITOR:
		if (node_exec_binop(o, exec, data_bitor)) {
			goto success;
		}
		break;
	case NODE_BITXOR:
		if (node_exec_binop(o, exec, data_bitxor)) {
			goto success;
		}
		break;
	case NODE_EQ:
		if (node_exec_binop(o, exec, data_eq)) {
			goto success;
		}
		break;
	case NODE_LT:
		if (node_exec_binop(o, exec, data_lt)) {
			goto success;
		}
		break;
	case NODE_LE:
		if (node_exec_binop(o, exec, data_le)) {
			goto success;
		}
		break;
	case NODE_AND:
		if (node_exec_binop(o, exec, data_and)) {
			goto success;
		}
		break;
	case NODE_OR:
		if (node_exec_binop(o, exec, data_or)) {
			goto success;
		}
		break;
	case NODE_NEG:
		if (node_exec_unop(o, exec, data_neg)) {
			goto success;
		}
		break;
	case NODE_BITNOT:
		if (node_exec_unop(o, exec, data_bitnot)) {
			goto success;
		}
		break;
	case NODE_NOT:
		if (node_exec_unop(o, exec, data_not)) {
			goto success;
		}
		break;
	default:
		dbg_puts("node_exec: unhandled node (not implemented)\n");
	}
error:
	exec->depth--;
	return 0;
success:
	exec->depth--;
	return RESULT_SUCCESS;
}

/* --------------------------------------------------------- tree --- */

void
tree_init(struct tree_s *o) {
	o->first = 0;
}

void
tree_done(struct tree_s *o) {
	tree_empty(o);
}

void
tree_empty(struct tree_s *o) {
	struct node_s *i, *inext;
	for (i = o->first; i != 0; i = inext) {
		inext = i->next;
		node_delete(i);
	}
	o->first = 0;
}

void
tree_add(struct tree_s *o, struct node_s *v) {
	struct node_s **i;
	i = &o->first;
	while (*i != 0) {
		i = &(*i)->next;
	}
	v->next = 0;
	*i = v;
}

void
tree_move(struct tree_s *o, struct tree_s *src) {
	struct node_s **i;
	i = &o->first;
	while (*i != 0) {
		i = &(*i)->next;
	}
	*i = src->first;
	src->first = 0;
}

void
tree_dbg(struct tree_s *o, unsigned depth) {
	struct node_s *i = o->first;
	while (i) {
		node_dbg(i, depth);
		i = i->next;
	}
}

unsigned
tree_exec(struct tree_s *o, struct exec_s *exec) {
	struct data_s *res;
	struct node_s *i = o->first;
	unsigned result;

	while (i) {
		result = node_exec(i, exec);
		if (!result) {
			return 0;
		} else if (result == RESULT_RETURN) {
			return RESULT_RETURN;
		}
		if (exec->acc != 0) {
			res = exec_getacc(exec);
			/*
			dbg_puts("tree_exec: node leaved acc=");
			data_dbg(res);
			dbg_puts(", will be deleted\n");
			*/
			data_delete(res);
		}
		i = i->next;
	}
	exec_putacc(exec, data_newnil());
	return RESULT_SUCCESS;
}

