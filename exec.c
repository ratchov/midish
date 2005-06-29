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
#include "exec.h"
#include "data.h"
#include "node.h"

#include "user.h"	/* for user_printstr */

unsigned exec_debug = 0;

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
	o->code = 0;
	return o;
}

void
proc_delete(struct proc_s *o) {
	node_delete(o->code);
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
	node_dbg(o->code, 0);
}
		

struct exec_s *
exec_new(void) {
	struct exec_s *o;
	o = (struct exec_s *)mem_alloc(sizeof(struct exec_s));
	o->procs = 0;
	o->globals = 0;
	o->locals = &o->globals;
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

void
exec_newbuiltin(struct exec_s *o, char *name,
    unsigned func(struct exec_s *, struct data_s **), struct name_s *args) {
	struct proc_s *newp;
	
	newp = proc_new(name);
	newp->args = args;
	newp->code = node_new(&node_vmt_builtin, data_newuser((void *)func));
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
	struct var_s *v;
	for (v = o->globals; v != 0; v = (struct var_s *)v->name.next) {
		dbg_puts(v->name.str);
		dbg_puts(" = ");
		data_dbg(v->data);
		dbg_puts("\n");
	}
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
exec_lookuplist(struct exec_s *exec, char *name, struct data_s **val) {
	struct var_s *var;
	var = exec_varlookup(exec, name);
	if (var == 0 || var->data->type != DATA_LIST) {
		exec_error(exec, name);
		exec_error(exec, ": no such list\n");
		return 0;
	}
	*val = var->data->val.list;
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

