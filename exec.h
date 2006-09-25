/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
 * All rights reseved
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

#ifndef MIDISH_EXEC_H
#define MIDISH_EXEC_H

#include "name.h"

enum RESULT_ID {
	RESULT_ERR = 0,
	RESULT_OK, RESULT_BREAK, RESULT_CONTINUE, RESULT_RETURN, RESULT_EXIT
};

struct data;
struct var;
struct proc;
struct node;
struct tree;
struct exec;

/*
 * a variable is a (identifier, value) pair
 */
struct var {
	struct name name;	/* name (identifier) */
	struct data *data;	/* value, see data.h */
};

/*
 * a procedure is a name, a list of argument names
 * and a the actual code (tree)
 */
struct proc {
	struct name name;
	struct name *args;
	struct node *code;
};

/*
 * exec is the interpreter's environment
 */
 
struct exec {
	struct var *globals;	/* list of global variables */
	struct var **locals;	/* pointer to list of local variables */
	struct proc *procs;	/* list of user and built-in procs */
	char *procname;		/* current proc name, for err messages */
#define EXEC_MAXDEPTH	40
	unsigned depth;		/* max depth of nested proc calls */
};

struct var *var_new(char *name, struct data *data);
void        var_delete(struct var *i);
void	    var_dbg(struct var *i);
struct var *var_lookup(struct var **first, char *name);
void	    var_insert(struct var **first, struct var *i);
void	    var_empty(struct var **first);

struct exec *exec_new(void);
void	     exec_delete(struct exec *o);
struct proc *exec_proclookup(struct exec *o, char *name);
struct var  *exec_varlookup(struct exec *o, char *name);

void exec_newbuiltin(struct exec *o, char *name, unsigned func(struct exec *, struct data **), struct name *args);
void exec_newvar(struct exec *o, char *name, struct data *val);
void exec_dumpprocs(struct exec *o);
void exec_dumpvars(struct exec *o);
unsigned exec_lookupname(struct exec *exec, char *name, char **val);
unsigned exec_lookupstring(struct exec *exec, char *name, char **val);
unsigned exec_lookuplong(struct exec *exec, char *name, long *val);
unsigned exec_lookuplist(struct exec *exec, char *name, struct data **val);
unsigned exec_lookupbool(struct exec *exec, char *name, long *val);

struct proc *proc_new(char *name);
void 	     proc_delete(struct proc *o);
struct proc *proc_lookup(struct proc **first, char *name);
void	     proc_empty(struct proc **first);
void 	     proc_dbg(struct proc *o);

#endif /* MIDISH_EXEC_H */
