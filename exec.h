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

#endif /* MIDISH_EXEC_H */
