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
