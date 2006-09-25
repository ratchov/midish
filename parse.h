/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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

#ifndef MIDISH_PARSE_H
#define MIDISH_PARSE_H

#include "lex.h"

struct node;

struct parse {
	struct lex lex;
	unsigned lookavail;
};

struct parse *parse_new(char *);
void		parse_delete(struct parse *);

void		parse_error(struct parse *, char *);

unsigned	parse_getsym(struct parse *);
void		parse_ungetsym(struct parse *);

unsigned	parse_end(struct parse *, struct node **);
unsigned	parse_call(struct parse *, struct node **);
unsigned	parse_expr(struct parse *, struct node **);
unsigned	parse_addsub(struct parse *, struct node **);
unsigned	parse_muldiv(struct parse *, struct node **);
unsigned	parse_neg(struct parse *, struct node **);
unsigned	parse_const(struct parse *, struct node **);
unsigned	parse_assign(struct parse *, struct node **);
unsigned	parse_stmt(struct parse *, struct node **);
unsigned	parse_slist(struct parse *, struct node **);
unsigned	parse_proc(struct parse *, struct node **);
unsigned	parse_line(struct parse *o, struct node **n);
unsigned	parse_prog(struct parse *o, struct node **n);

#endif /* MIDISH_PARSE_H */
