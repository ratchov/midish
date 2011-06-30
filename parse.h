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

#ifndef MIDISH_PARSE_H
#define MIDISH_PARSE_H

#include "lex.h"

struct node;

struct parse {
	struct lex lex;
	unsigned lookavail;
};

struct parse *parse_new(char *);
void	      parse_delete(struct parse *);

void	      parse_error(struct parse *, char *);

unsigned      parse_getsym(struct parse *);
void	      parse_ungetsym(struct parse *);

unsigned      parse_end(struct parse *, struct node **);
unsigned      parse_call(struct parse *, struct node **);
unsigned      parse_expr(struct parse *, struct node **);
unsigned      parse_addsub(struct parse *, struct node **);
unsigned      parse_muldiv(struct parse *, struct node **);
unsigned      parse_neg(struct parse *, struct node **);
unsigned      parse_const(struct parse *, struct node **);
unsigned      parse_assign(struct parse *, struct node **);
unsigned      parse_stmt(struct parse *, struct node **);
unsigned      parse_slist(struct parse *, struct node **);
unsigned      parse_proc(struct parse *, struct node **);
unsigned      parse_line(struct parse *, struct node **);
unsigned      parse_prog(struct parse *, struct node **);

#endif /* MIDISH_PARSE_H */
