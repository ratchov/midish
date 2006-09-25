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

#ifndef MIDISH_DATA_H
#define MIDISH_DATA_H

#define DATA_NIL	0
#define DATA_LONG	1
#define DATA_STRING	2
#define DATA_REF	3
#define DATA_LIST	4
#define DATA_USER	5

#define DATA_MAXNITEMS	4096

struct name;

struct data {
	unsigned type;
	union {
		char *str;
		long num;
		struct data *list;
		char *ref;
		void *user;
	} val;
	struct data *next;
};

struct data *data_newnil(void);
struct data *data_newlong(long);
struct data *data_newstring(char *);
struct data *data_newref(char *);
struct data *data_newlist(struct data *);
struct data *data_newuser(void *);
void	       data_delete(struct data *o);
void	       data_setfield(struct data *dst, char *field);
void	       data_dbg(struct data *);

void	       data_listadd(struct data *, struct data *);
void	       data_listremove(struct data *o, struct data *v);
struct data *data_listlookup(struct data *o, struct name *ref);

void	 data_assign(struct data *dst, struct data *src);
unsigned data_eval(struct data *o);
unsigned data_id(struct data *op1, struct data *op2);

unsigned data_not(struct data *op1);
unsigned data_and(struct data *op1, struct data *op2);
unsigned data_or(struct data *op1, struct data *op2);
unsigned data_eq(struct data *op1, struct data *op2);
unsigned data_neq(struct data *op1, struct data *op2);
unsigned data_lt(struct data *op1, struct data *op2);
unsigned data_le(struct data *op1, struct data *op2);
unsigned data_gt(struct data *op1, struct data *op2);
unsigned data_ge(struct data *op1, struct data *op2);
unsigned data_add(struct data *op1, struct data *op2);
unsigned data_sub(struct data *op1, struct data *op2);
unsigned data_neg(struct data *op1);
unsigned data_mul(struct data *op1, struct data *op2);
unsigned data_div(struct data *op1, struct data *op2);
unsigned data_mod(struct data *op1, struct data *op2);
unsigned data_lshift(struct data *op1, struct data *op2);
unsigned data_rshift(struct data *op1, struct data *op2);
unsigned data_bitand(struct data *op1, struct data *op2);
unsigned data_bitor(struct data *op1, struct data *op2);
unsigned data_bitxor(struct data *op1, struct data *op2);
unsigned data_bitnot(struct data *op1);

#endif /* MIDISH_DATA_H */
