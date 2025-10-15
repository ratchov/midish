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
#include <stdio.h>
#include "utils.h"
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
	o = (struct var *)xmalloc(sizeof(struct var), "var");
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
	xfree(o);
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
	o = (struct proc *)xmalloc(sizeof(struct proc), "proc");
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
	xfree(o);
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
 * create a new empty execution environment
 */
struct exec *
exec_new(void)
{
	struct exec *o;
	o = (struct exec *)xmalloc(sizeof(struct exec), "exec");
	o->procs = NULL;
	o->globals = NULL;
	o->locals = &o->globals;
	o->procname = "top-level";
	o->depth = 0;
	o->result = RESULT_OK;
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
		logx(1, "%s: depth != 0", __func__);
		panic();
	}
	var_empty(&o->globals);
	proc_empty(&o->procs);
	xfree(o);
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
	char buf[128], *end = buf + sizeof(buf), *p;
	struct proc *proc;
	struct name *n;

	PROC_FOREACH(proc, o->procs) {
		p = buf;
		p += snprintf(buf, sizeof(buf), "%s(", proc->name.str);
		for (n = proc->args; n != NULL; n = n->next) {
			p += snprintf(p, p < end ? end - p : 0, "%s", n->str);
			if (n->next)
				p += snprintf(p, p < end ? end - p : 0, ", ");
		}
		p += snprintf(p, p < end ? end - p : 0, ")");
		logx(1, "%s", buf);
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

	VAR_FOREACH(v, o->globals)
		logx(1, "%s = {data:%p}", v->name.str, v->data);
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
		cons_errss(o->procname, name, "must be set to a reference");
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
		cons_errss(o->procname, name, "must be set to a string");
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
		cons_errss(o->procname, name, "must be set to a number");
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
		cons_errss(o->procname, name, "must be set to a list");
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
		cons_errss(o->procname, name, "must be set to a bool");
		return 0;
	}
	res = data_eval(var->data);
	*val = res;
	return 1;
}

