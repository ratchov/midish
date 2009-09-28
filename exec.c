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
 * this module implements the execution environment of scripts:
 * variables lists, procedure lists and various primitives
 * to handle them.
 *
 * a variable is a simple (name, value) pair. The name is stored
 * in a name structure (see name.h) and the value is stored in
 * a data structure (see data.h).
 *
 * a procedure is a (name, args, code) triplet. The name is the
 * (unique) name that identifies the procedure, 'args' is the
 * list of argument names and 'code' is the tree containing the
 * instructions of the procedure
 */

#include "dbg.h"
#include "exec.h"
#include "data.h"
#include "node.h"

#include "cons.h"	/* for cons_errxxx */

/* ----------------------------------------------- variable lists --- */

/*
 * create a new variable with the given name and value
 * and insert it on the given variable list
 */
struct var *
var_new(struct name **list, char *name, struct data *data)
{
	struct var *o;
	o = (struct var *)mem_alloc(sizeof(struct var), "var");
	o->data = data;
	name_init(&o->name, name);
	name_insert(list, (struct name *)o);
	return o;
}

/*
 * delete the given variable from the given list
 */
void
var_delete(struct name **list, struct var *o)
{
	name_remove(list, (struct name *)o);
	if (o->data != NULL) {
		data_delete(o->data);
	}
	name_done(&o->name);
	mem_free(o);
}

/*
 * print "name = value" on stderr
 */
void
var_dbg(struct var *o)
{
	str_dbg(o->name.str);
	dbg_puts(" = ");
	if (o->data != NULL)
		data_dbg(o->data);
	else
		dbg_puts("<null>");
}

/*
 * delete all variables and clear the given list
 */
void
var_empty(struct name **list)
{
	while (*list != NULL) {
		var_delete(list, (struct var *)*list);
	}
}

/*
 * allocate a new procedure with the given name
 */
struct proc *
proc_new(char *name)
{
	struct proc *o;
	o = (struct proc *)mem_alloc(sizeof(struct proc), "proc");
	name_init(&o->name, name);
	o->args = NULL;
	o->code = NULL;
	return o;
}

/*
 * delete the given procedure: free name, arguments and code
 */
void
proc_delete(struct proc *o)
{
	node_delete(o->code);
	name_empty(&o->args);
	name_done(&o->name);
	mem_free(o);
}

/*
 * free all procedures and clear the given list
 */
void
proc_empty(struct name **first)
{
	struct name *i;
	while (*first) {
		i = *first;
		name_remove(first, i);
		proc_delete((struct proc *)i);
	}
	*first = NULL;
}

/*
 * dump a procedure on stderr in the following format:
 * 	name (arg1, arg2, ...)
 *	code_tree
 */
void
proc_dbg(struct proc *o)
{
	struct name *i;
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
	node_dbg(o->code, 0);
}

/*
 * create a new empty execution environment
 */
struct exec *
exec_new(void)
{
	struct exec *o;
	o = (struct exec *)mem_alloc(sizeof(struct exec), "exec");
	o->procs = NULL;
	o->globals = NULL;
	o->locals = &o->globals;
	o->procname = "top-level";
	o->depth = 0;
	return o;
}

/*
 * delete the given execution environment. Must not be
 * called inside a procedure
 */
void
exec_delete(struct exec *o)
{
	if (o->depth != 0) {
		dbg_puts("exec_done: depth != 0\n");
		dbg_panic();
	}
	var_empty(&o->globals);
	proc_empty(&o->procs);
	mem_free(o);
}

/*
 * find the variable with the given name in the
 * execution environment. If there a matching variable
 * in the local list we return it, else we search in the
 * global list.
 */
struct var *
exec_varlookup(struct exec *o, char *name)
{
	struct name *var;

	var = name_lookup(o->locals, name);
	if (var != NULL) {
		return (struct var *)var;
	}
	if (o->locals != &o->globals) {
		var = name_lookup(&o->globals, name);
		if (var != NULL) {
			return (struct var *)var;
		}
	}
	return NULL;
}

/*
 * find the procedure with the given name
 */
struct proc *
exec_proclookup(struct exec *o, char *name)
{
	return (struct proc *)name_lookup(&o->procs, name);
}

/*
 * add a new built-in procedure in the exec environment.
 */
void
exec_newbuiltin(struct exec *o, char *name,
    unsigned func(struct exec *, struct data **), struct name *args) {
	struct proc *newp;

	newp = proc_new(name);
	newp->args = args;
	newp->code = node_new(&node_vmt_builtin, data_newuser((void *)func));
	name_add(&o->procs, (struct name *)newp);
}

/*
 * add a new global variable in the exec environment
 */
void
exec_newvar(struct exec *o, char *name, struct data *val)
{
	var_new(&o->globals, name, val);
}

/*
 * dump all procs in the following format:
 *	proc1(arg1, arg2, ...)
 *	proc2(arg1, arg2, ...)
 *	...
 */
void
exec_dumpprocs(struct exec *o)
{
	struct proc *p;
	struct name *n;

	PROC_FOREACH(p, o->procs) {
		dbg_puts(p->name.str);
		dbg_puts("(");
		for (n = p->args; n != NULL; n = n->next) {
			dbg_puts(n->str);
			if (n->next) {
				dbg_puts(", ");
			}
		}
		dbg_puts(")\n");
	}
}

/*
 * dump all global variables in the following format:
 *	var1 = value
 *	var2 = value
 *	...
 */
void
exec_dumpvars(struct exec *o)
{
	struct var *v;
	VAR_FOREACH(v, o->globals) {
		dbg_puts(v->name.str);
		dbg_puts(" = ");
		data_dbg(v->data);
		dbg_puts("\n");
	}
}

/*
 * find a variable with the given name with value of type DATA_REF
 */
unsigned
exec_lookupname(struct exec *o, char *name, char **val)
{
	struct var *var;
	var = exec_varlookup(o, name);
	if (var == NULL || var->data->type != DATA_REF) {
		cons_errss(o->procname, name, "no such reference");
		return 0;
	}
	*val = var->data->val.ref;
	return 1;
}

/*
 * find a variable with the given name with value of type DATA_STRING
 */
unsigned
exec_lookupstring(struct exec *o, char *name, char **val)
{
	struct var *var;
	var = exec_varlookup(o, name);
	if (var == NULL || var->data->type != DATA_STRING) {
		cons_errss(o->procname, name, "no such string");
		return 0;
	}
	*val = var->data->val.str;
	return 1;
}


/*
 * find a variable with the given name with value of type DATA_LONG
 */
unsigned
exec_lookuplong(struct exec *o, char *name, long *val)
{
	struct var *var;
	var = exec_varlookup(o, name);
	if (var == NULL || var->data->type != DATA_LONG) {
		cons_errss(o->procname, name, "no such long");
		return 0;
	}
	*val = var->data->val.num;
	return 1;
}

/*
 * find a variable with the given name with value of type DATA_LIST
 */
unsigned
exec_lookuplist(struct exec *o, char *name, struct data **val)
{
	struct var *var;
	var = exec_varlookup(o, name);
	if (var == NULL || var->data->type != DATA_LIST) {
		cons_errss(o->procname, name, "no such list");
		return 0;
	}
	*val = var->data->val.list;
	return 1;
}

/*
 * find a variable with the given name convert its value to
 * a boolean and return it
 */
unsigned
exec_lookupbool(struct exec *o, char *name, long *val)
{
	struct var *var;
	unsigned res;

	var = exec_varlookup(o, name);
	if (var == NULL) {
		cons_errss(o->procname, name, "no such bool");
		return 0;
	}
	res = data_eval(var->data);
	*val = res;
	return 1;
}

