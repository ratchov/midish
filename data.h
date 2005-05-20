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

#ifndef MIDISH_DATA_H
#define MIDISH_DATA_H

#define DATA_NIL	0
#define DATA_LONG	1
#define DATA_STRING	2
#define DATA_REF	3
#define DATA_LIST	4

struct name_s;

struct data_s {
	unsigned type;
	union {
		char *str;
		long num;
		struct data_s *list;
		char *ref;
	} val;
	struct data_s *next;
};

struct data_s *data_newnil(void);
struct data_s *data_newlong(long);
struct data_s *data_newstring(char *);
struct data_s *data_newref(char *);
struct data_s *data_newlist(struct data_s *);
void	       data_delete(struct data_s *o);
void	       data_setfield(struct data_s *dst, char *field);
void	       data_dbg(struct data_s *);

void	       data_listadd(struct data_s *, struct data_s *);
void	       data_listremove(struct data_s *o, struct data_s *v);
struct data_s *data_listlookup(struct data_s *o, struct name_s *ref);

void	 data_assign(struct data_s *dst, struct data_s *src);
unsigned data_eval(struct data_s *o);
unsigned data_id(struct data_s *op1, struct data_s *op2);

unsigned data_not(struct data_s *op1);
unsigned data_and(struct data_s *op1, struct data_s *op2);
unsigned data_or(struct data_s *op1, struct data_s *op2);
unsigned data_eq(struct data_s *op1, struct data_s *op2);
unsigned data_neq(struct data_s *op1, struct data_s *op2);
unsigned data_lt(struct data_s *op1, struct data_s *op2);
unsigned data_le(struct data_s *op1, struct data_s *op2);
unsigned data_gt(struct data_s *op1, struct data_s *op2);
unsigned data_ge(struct data_s *op1, struct data_s *op2);
unsigned data_add(struct data_s *op1, struct data_s *op2);
unsigned data_sub(struct data_s *op1, struct data_s *op2);
unsigned data_neg(struct data_s *op1);
unsigned data_mul(struct data_s *op1, struct data_s *op2);
unsigned data_div(struct data_s *op1, struct data_s *op2);
unsigned data_mod(struct data_s *op1, struct data_s *op2);
unsigned data_lshift(struct data_s *op1, struct data_s *op2);
unsigned data_rshift(struct data_s *op1, struct data_s *op2);
unsigned data_bitand(struct data_s *op1, struct data_s *op2);
unsigned data_bitor(struct data_s *op1, struct data_s *op2);
unsigned data_bitxor(struct data_s *op1, struct data_s *op2);
unsigned data_bitnot(struct data_s *op1);

#endif /* MIDISH_DATA_H */
