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

#define VAR_FOREACH(i,list)			\
	for (i = (struct var *)(list);		\
	     i != NULL;				\
	     i = (struct var *)(i->name.next))


/*
 * a procedure is a name, a list of argument names
 * and a the actual code (tree)
 */
struct proc {
	struct name name;
	struct name *args;
	struct node *code;
};

#define PROC_FOREACH(i,list)			\
	for (i = (struct proc *)(list);		\
	     i != NULL;				\
	     i = (struct proc *)(i->name.next))

/*
 * exec is the interpreter's environment
 */

struct exec {
	struct name *globals;	/* list of global variables */
	struct name **locals;	/* pointer to list of local variables */
	struct name *procs;	/* list of user and built-in procs */
	char *procname;		/* current proc name, for err messages */
#define EXEC_MAXDEPTH	40
	unsigned depth;		/* max depth of nested proc calls */
};

struct var *var_new(struct name **, char *, struct data *);
void        var_delete(struct name **, struct var *);
void	    var_dbg(struct var *);
void	    var_empty(struct name **);

struct exec *exec_new(void);
void	     exec_delete(struct exec *);
struct proc *exec_proclookup(struct exec *, char *);
struct var  *exec_varlookup(struct exec *, char *);

void exec_newbuiltin(struct exec *, char *, unsigned (*)(struct exec *, struct data **), struct name *);
void exec_newvar(struct exec *, char *, struct data *);
void exec_dumpprocs(struct exec *);
void exec_dumpvars(struct exec *);
unsigned exec_lookupname(struct exec *, char *, char **);
unsigned exec_lookupstring(struct exec *, char *, char **);
unsigned exec_lookuplong(struct exec *, char *, long *);
unsigned exec_lookuplist(struct exec *, char *, struct data **);
unsigned exec_lookupbool(struct exec *, char *, long *);

struct proc *proc_new(char *);
void 	     proc_delete(struct proc *);
void	     proc_empty(struct name **);
void 	     proc_dbg(struct proc *);

#define EXEC_OBSOLETE(o)							\
	do {									\
		static int signaled = 0;					\
										\
		if (signaled)							\
			break;							\
		cons_errs(o->procname,						\
		    "obsolete function, will be removed in next release");	\
		signaled = 1;							\
	} while (0)

#endif /* MIDISH_EXEC_H */
