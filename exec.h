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

#ifndef MIDISH_EXEC_H
#define MIDISH_EXEC_H

#include "name.h"

extern unsigned exec_debug;

enum RESULT_ID {
	RESULT_ERR = 0,
	RESULT_OK, RESULT_BREAK, RESULT_CONTINUE, RESULT_RETURN
};

struct data_s;
struct var_s;
struct proc_s;
struct node_s;
struct tree_s;
struct exec_s;

	/*
	 * a variable is a name with appended data
	 */

struct var_s {
	struct name_s name;
	struct data_s *data;
};

	/*
	 * a procedure is a name, a list of names as arguments
	 * and a tree (the code)
	 */

struct proc_s {
	struct name_s name;
	struct name_s *args;
	struct node_s *code;
};

	/*
	 * exec is the state of the interpreter
	 */
	 
struct exec_s {
	struct var_s *globals;	/* list of global variables */
	struct var_s **locals;	/* pinter to list of local variables */
	struct proc_s *procs;	/* list of user an built-in procs */
	char *procname;		/* current proc name, for err messages */
#define EXEC_MAXDEPTH	40
	unsigned depth;		/* call depth */
};

struct var_s *var_new(char *name, struct data_s *data);
void          var_delete(struct var_s *i);
void	      var_dbg(struct var_s *i);
struct var_s *var_lookup(struct var_s **first, char *name);
void	      var_insert(struct var_s **first, struct var_s *i);
void	      var_empty(struct var_s **first);

struct exec_s *exec_new(void);
void	       exec_delete(struct exec_s *o);
void	       exec_err(struct exec_s *o, char *mesg);
void	       exec_errs(struct exec_s *o, char *s, char *mesg);
struct proc_s *exec_proclookup(struct exec_s *o, char *name);
struct var_s  *exec_varlookup(struct exec_s *o, char *name);
void	       exec_error(struct exec_s *o, char *msg);

void exec_newbuiltin(struct exec_s *o, char *name, unsigned func(struct exec_s *, struct data_s **), struct name_s *args);
void exec_dumpprocs(struct exec_s *o);
void exec_dumpvars(struct exec_s *o);
unsigned exec_lookupname(struct exec_s *exec, char *name, char **val);
unsigned exec_lookupstring(struct exec_s *exec, char *name, char **val);
unsigned exec_lookuplong(struct exec_s *exec, char *name, long *val);
unsigned exec_lookupbool(struct exec_s *exec, char *name, long *val);
unsigned exec_lookuplist(struct exec_s *exec, char *name, struct data_s **val);
void     exec_newvar(struct exec_s *o, char *name, struct data_s *val);

struct proc_s *proc_new(char *name);
void 	       proc_delete(struct proc_s *o);
struct proc_s *proc_lookup(struct proc_s **first, char *name);
void	       proc_empty(struct proc_s **first);
void 	       proc_dbg(struct proc_s *o);

#endif /* MIDISH_EXEC_H */
