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

#ifndef MIDISH_DATA_H
#define MIDISH_DATA_H


#define DATA_MAXNITEMS	4096

struct name;

/*
 * the following represents a "value" for the interpreter. all types
 * use the same strucure
 */
struct data {
#define DATA_NIL	0
#define DATA_LONG	1
#define DATA_STRING	2
#define DATA_REF	3
#define DATA_LIST	4
#define DATA_USER	5
#define DATA_RANGE	6
	unsigned type;			/* type of the value */
	union {
		char *str;		/* if string */
		long num;		/* if a number */
		struct data *list;	/* if a list of values */
		char *ref;		/* if a reference (name) */
		void *user;		/* user defined */
		struct {		/* a range */
			unsigned min;
			unsigned max;
		} range;
	} val;
	struct data *next;
};

struct data *data_newnil(void);
struct data *data_newlong(long);
struct data *data_newstring(char *);
struct data *data_newref(char *);
struct data *data_newlist(struct data *);
struct data *data_newuser(void *);
struct data *data_newrange(unsigned, unsigned);
void	     data_delete(struct data *);
void	     data_setfield(struct data *, char *);
void	     data_dbg(struct data *);
void	     data_listadd(struct data *, struct data *);
void	     data_listremove(struct data *, struct data *);
struct data *data_listlookup(struct data *, struct name *);

void	 data_assign(struct data *, struct data *);
unsigned data_eval(struct data *);
unsigned data_id(struct data *, struct data *);

unsigned data_not(struct data *);
unsigned data_and(struct data *, struct data *);
unsigned data_or(struct data *, struct data *);
unsigned data_eq(struct data *, struct data *);
unsigned data_neq(struct data *, struct data *);
unsigned data_lt(struct data *, struct data *);
unsigned data_le(struct data *, struct data *);
unsigned data_gt(struct data *, struct data *);
unsigned data_ge(struct data *, struct data *);
unsigned data_add(struct data *, struct data *);
unsigned data_sub(struct data *, struct data *);
unsigned data_neg(struct data *);
unsigned data_mul(struct data *, struct data *);
unsigned data_div(struct data *, struct data *);
unsigned data_mod(struct data *, struct data *);
unsigned data_lshift(struct data *, struct data *);
unsigned data_rshift(struct data *, struct data *);
unsigned data_bitand(struct data *, struct data *);
unsigned data_bitor(struct data *, struct data *);
unsigned data_bitxor(struct data *, struct data *);
unsigned data_bitnot(struct data *);

#endif /* MIDISH_DATA_H */
