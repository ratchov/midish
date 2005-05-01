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

#ifndef SEQ_TREE_H
#define SEQ_TREE_H

#include "name.h"

extern unsigned tree_debug;

enum NODE_ID {
	NODE_CONST = 1, NODE_VAR,
	NODE_CALL, NODE_IF, NODE_IFELSE, NODE_FOR,
	NODE_RETURN, NODE_ASSIGN, NODE_LIST,
	NODE_EQ, NODE_LE, NODE_LT,
	NODE_AND, NODE_OR, NODE_NOT,
	NODE_ADD, NODE_SUB, NODE_MUL, NODE_DIV, NODE_MOD, NODE_NEG,
	NODE_LSHIFT, NODE_RSHIFT, NODE_BITAND, NODE_BITOR, 
	NODE_BITXOR, NODE_BITNOT
};

enum RESULT_ID {
	RESULT_ERROR = 0,
	RESULT_SUCCESS, RESULT_BREAK, RESULT_CONTINUE, RESULT_RETURN
};

struct data_s;
struct var_s;
struct proc_s;
struct node_s;
struct tree_s;
struct exec_s;

struct tree_s {
	struct node_s *first;
};

struct node_s {
	unsigned id;
	struct node_s *next;
	union {
		struct {
			struct data_s *data;
		} cst;
		struct {
			char *name;
		} var;
		struct {
			char *name;
			struct node_s *expr;
		} assign;
		struct {
			char *name;
			struct tree_s args;
		} call;
		struct {
			struct node_s *expr;
			struct tree_s stmt1, stmt2;
		} cond;
		struct {
			char *name;
			struct node_s *expr;
			struct tree_s stmt;
		} loop;
		struct {
			struct node_s *expr1, *expr2;
		} op;
	} param;
};

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
#define PROC_BUILTIN	1
	unsigned type;
	struct name_s *args;
	union {
		struct tree_s tree;
		unsigned (*func)(struct exec_s *);
	} code;
};

	/*
	 * exec is the state of the interpreter
	 */
	 
struct exec_s {
	struct var_s **locals, *globals;
	struct proc_s *procs;
	struct data_s *acc;
#define EXEC_MAXDEPTH	40
	unsigned depth;
};

struct var_s *var_new(char *name, struct data_s *data);
void          var_delete(struct var_s *i);
void	      var_dbg(struct var_s *i);
struct var_s *var_lookup(struct var_s **first, char *name);
void	      var_insert(struct var_s **first, struct var_s *i);
void	      var_empty(struct var_s **first);

struct exec_s *exec_new(void);
void	       exec_delete(struct exec_s *o);
struct proc_s *exec_proclookup(struct exec_s *o, char *name);
struct var_s  *exec_varlookup(struct exec_s *o, char *name);
struct data_s *exec_getacc(struct exec_s *o);
void	       exec_putacc(struct exec_s *o, struct data_s *val);

void exec_newbuiltin(struct exec_s *o, char *name, unsigned func(struct exec_s *exec), struct name_s *args);
void exec_dumpprocs(struct exec_s *o);
unsigned exec_lookupname(struct exec_s *exec, char *name, char **val);
unsigned exec_lookupstring(struct exec_s *exec, char *name, char **val);
unsigned exec_lookuplong(struct exec_s *exec, char *name, long *val);
unsigned exec_lookupbool(struct exec_s *exec, char *name, long *val);
void     exec_newvar(struct exec_s *o, char *name, struct data_s *val);

struct proc_s *proc_new(char *name);
void 	       proc_delete(struct proc_s *o);
struct proc_s *proc_lookup(struct proc_s **first, char *name);
void	       proc_empty(struct proc_s **first);
void 	       proc_dbg(struct proc_s *o);

struct node_s *node_newconst(struct data_s *data);
struct node_s *node_newvar(char *name);
struct node_s *node_newassign(char *name, struct node_s *val);
struct node_s *node_newcall(char *name, struct tree_s *args);
struct node_s *node_newcond(struct node_s *c);
struct node_s *node_newop(unsigned, struct node_s *, struct node_s *);
struct node_s *node_newlist(struct tree_s *args);
struct node_s *node_newloop(char *, struct node_s *);
struct node_s *node_newindir(unsigned id, struct node_s *expr, char *field);

void	       node_dbg(struct node_s *o, unsigned depth);
void	       node_delete(struct node_s *o);
unsigned       node_exec(struct node_s *o, struct exec_s *exec);

void	 tree_init(struct tree_s *o);
void	 tree_done(struct tree_s *o);
void	 tree_add(struct tree_s *o, struct node_s *v);
void	 tree_move(struct tree_s *o, struct tree_s *src);
void	 tree_empty(struct tree_s *o);
void	 tree_dbg(struct tree_s *o, unsigned depth);
unsigned tree_exec(struct tree_s *o, struct exec_s *exec);

#endif /* SEQ_TREE_H */
