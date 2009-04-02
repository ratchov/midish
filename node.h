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

#ifndef MIDISH_NODE_H
#define MIDISH_NODE_H

struct node;
struct node_vmt;
struct exec;

struct node {
	struct node_vmt *vmt;
	struct data *data;
	struct node *next, *list;
};

struct node_vmt {
	char *name;
	unsigned (*exec)(struct node *, struct exec *, struct data **);
};

struct node *node_new(struct node_vmt *, struct data *);
void	     node_delete(struct node *);
void	     node_dbg(struct node *, unsigned);
void	     node_insert(struct node **, struct node *);
void	     node_replace(struct node **, struct node *);
unsigned     node_exec(struct node *, struct exec *, struct data **);


extern struct node_vmt
	node_vmt_proc, node_vmt_slist,
	node_vmt_call, node_vmt_elist, node_vmt_builtin,
	node_vmt_cst, node_vmt_var, node_vmt_list, node_vmt_range,
	node_vmt_eq, node_vmt_neq, node_vmt_le,
	node_vmt_lt, node_vmt_ge, node_vmt_gt,
	node_vmt_ignore, node_vmt_if, node_vmt_for,
	node_vmt_return, node_vmt_exit, node_vmt_assign, node_vmt_nop,
	node_vmt_and, node_vmt_or, node_vmt_not,
	node_vmt_neg, node_vmt_add, node_vmt_sub,
	node_vmt_mul, node_vmt_div, node_vmt_mod,
	node_vmt_lshift, node_vmt_rshift,
	node_vmt_bitand, node_vmt_bitor, node_vmt_bitxor, node_vmt_bitnot;

#endif /* MIDISH_NODE_H */
