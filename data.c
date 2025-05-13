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

/*
 * the data structure is used to store values of user variables, (used
 * by the interpreter, see node.c). This module implements basic
 * methods to manipulate data structures and the arithmetic primitives
 * used by the interpreter.
 *
 * currently allowed data types are
 *	- 'nil' (ie no value)
 *	- long signed integer
 *	- string
 *	- reference, ie an identifier (function name, track name...)
 *	- an user type 'void *addr' pointer
 *	- a list of values
 */

#include "utils.h"
#include "str.h"
#include "cons.h"
#include "data.h"

/*
 * allocate a new data structure and initialize it as 'nil'
 */
struct data *
data_newnil(void)
{
	struct data *o;
	o = (struct data *)xmalloc(sizeof(struct data), "data");
	o->type = DATA_NIL;
	o->next = NULL;
	return o;
}

/*
 * allocate a new data structure and initialize with the given integer
 */
struct data *
data_newlong(long val)
{
	struct data *o;
	o = data_newnil();
	o->val.num = val;
	o->type = DATA_LONG;
	return o;
}

/*
 * allocate a new data structure and initialize with (a copy of) the
 * given string
 */
struct data *
data_newstring(char *val)
{
	struct data *o;
	o = data_newnil();
	o->val.str = str_new(val);
	o->type = DATA_STRING;
	return o;
}

/*
 * allocate a new data structure and initialize with (a copy of) the
 * given reference
 */
struct data *
data_newref(char *val)
{
	struct data *o;
	o = data_newnil();
	o->val.ref = str_new(val);
	o->type = DATA_REF;
	return o;
}


/*
 * allocate a new data structure and initialize with the given list
 * (not copied).  The list argument is the first item of a linked list
 * of data structures
 */
struct data *
data_newlist(struct data *list)
{
	struct data *o;
	o = data_newnil();
	o->val.list = list;
	o->type = DATA_LIST;
	return o;
}

/*
 * allocate a new data structure and initialize with the given user
 * type
 */
struct data *
data_newuser(void *addr)
{
	struct data *o;
	o = data_newnil();
	o->type = DATA_USER;
	o->val.user = addr;
	return o;
}

/*
 * allocate a new data structure and initialize with the given range
 */
struct data *
data_newrange(long min, long max)
{
	struct data *o;
	o = data_newnil();
	o->type = DATA_RANGE;
	o->val.range.min = min;
	o->val.range.max = max;
	return o;
}

/*
 * return the number of data structures contained in the given data
 * structure
 */
unsigned
data_numitem(struct data *o)
{
	struct data *i;
	unsigned n;

	n = 1;
	if (o->type == DATA_LIST) {
		for (i = o->val.list; i != NULL; i = i->next) {
			n += data_numitem(i);
		}
	}
	return n;
}

/*
 * add to the end of given 'o' data structure (must be of type
 * DATA_LIST) the given data structure (can be of any type)
 */
void
data_listadd(struct data *o, struct data *v)
{
	struct data **i;
	i = &o->val.list;
	while (*i != NULL) {
		i = &(*i)->next;
	}
	v->next = NULL;
	*i = v;
}

/*
 * remove the given data struct from the given list
 */
void
data_listremove(struct data *o, struct data *v)
{
	struct data **i;
	i = &o->val.list;
        while (*i != NULL) {
		if (*i == v) {
	        	*i = v->next;
	                v->next = NULL;
	                return;
		}
		i = &(*i)->next;
	}
	log_puts("data_listremove: not found\n");
	panic();
}

/*
 * clear a data structure and set it to by of type 'DATA_NIL'
 */
void
data_clear(struct data *o)
{
	struct data *i, *inext;
	switch(o->type) {
	case DATA_STRING:
		str_delete(o->val.str);
		break;
	case DATA_REF:
		str_delete(o->val.ref);
		break;
	case DATA_LIST:
		for (i = o->val.list; i != NULL; i = inext) {
			inext = i->next;
			data_delete(i);
		}
		break;
	case DATA_LONG:
	case DATA_NIL:
	case DATA_USER:
	case DATA_RANGE:
		break;
	default:
		log_puts("data_clear: unknown type\n");
		panic();
		break;
	}
	o->type = DATA_NIL;
}

void
data_delete(struct data *o)
{
	data_clear(o);
	xfree(o);
}

void
data_log(struct data *o)
{
	struct data *i;

	switch(o->type) {
	case DATA_NIL:
		log_puts("(nil)");
		break;
	case DATA_USER:
		log_puts("(user)");
		break;
	case DATA_LONG:
		log_puti(o->val.num);
		break;
	case DATA_STRING:
		log_puts("\"");
		log_puts(o->val.str);
		log_puts("\"");
		break;
	case DATA_REF:
		log_puts("@");
		str_log(o->val.ref);
		break;
	case DATA_LIST:
		log_puts("{");
		for (i = o->val.list; i != NULL; i = i->next) {
			data_log(i);
			if (i->next) {
				log_puts(" ");
			}
		}
		log_puts("}");
		break;
	case DATA_RANGE:
		log_puti(o->val.range.min);
		log_puts(":");
		log_puti(o->val.range.max);
		break;
	default:
		log_puts("(unknown type)");
		break;
	}
}

/*
 * copy src into dst, never fails
 */
void
data_assign(struct data *dst, struct data *src)
{
	struct data *n, *i, **j;
	if (dst == src) {
		log_puts("data_assign: src and dst are the same\n");
		panic();
	}
	data_clear(dst);
	switch(src->type) {
	case DATA_NIL:
		break;
	case DATA_LONG:
		dst->type = DATA_LONG;
		dst->val.num = src->val.num;
		break;
	case DATA_STRING:
		dst->type = DATA_STRING;
		dst->val.str = str_new(src->val.str);
		break;
	case DATA_REF:
		dst->type = DATA_REF;
		dst->val.ref = str_new(src->val.ref);
		break;
	case DATA_LIST:
		dst->type = DATA_LIST;
		dst->val.list = NULL;
		for (i = src->val.list; i != NULL; i = i->next) {
			n = data_newnil();
			data_assign(n, i);
			for (j = &dst->val.list; *j != NULL; j = &(*j)->next)
				; /* noting */
			n->next = NULL;
			*j = n;
		}
		break;
	case DATA_RANGE:
		dst->type = DATA_RANGE;
		dst->val.range.min = src->val.range.min;
		dst->val.range.max = src->val.range.max;
		break;
	default:
		log_puts("data_assign: bad data type\n");
		panic();
	}
}


/*
 * return 1 if op1 et op2 are identical, 0 overwise
 */
unsigned
data_id(struct data *op1, struct data *op2)
{
	struct data *i1, *i2;
	if (op1->type != op2->type) {
		return 0;
	}
	switch(op1->type) {
	case DATA_NIL:
		return 1;
	case DATA_LONG:
		return op1->val.num == op2->val.num ? 1 : 0;
	case DATA_STRING:
		return str_eq(op1->val.str, op2->val.str);
	case DATA_REF:
		return str_eq(op1->val.ref, op2->val.ref);
	case DATA_LIST:
		i1 = op1->val.list;
		i2 = op2->val.list;
		for (;;) {
			if (i1 == NULL && i2 == NULL) {
				return 1;
			} else if (i1 == NULL || i2 == NULL || !data_id(i1, i2)) {
				return 0;
			}
			i1 = i1->next;
			i2 = i2->next;
		}
		/* not reached */
		break;
	case DATA_RANGE:
		return op1->val.range.min == op2->val.range.min &&
		    op1->val.range.max == op2->val.range.max;
		break;
	default:
		log_puts("data_id: bad data types\n");
		panic();
	}
	/* not reached */
	return 0;
}

/*
 * return 1 if the argument is
 *	- a ref
 * 	- a not empty string or list
 *	- a non-zero integer
 * and 0 overwise
 */
unsigned
data_eval(struct data *o)
{
	switch(o->type) {
	case DATA_NIL:
		return 0;
	case DATA_LONG:
		return o->val.num != 0 ? 1 : 0;
	case DATA_STRING:
		return *o->val.str != '\0' ? 1 : 0;
	case DATA_REF:
		return 1;
	case DATA_LIST:
		return o->val.list != NULL ? 1 : 0;
	case DATA_RANGE:
		return 1;
	default:
		log_puts("data_eval: bad data type\n");
		panic();
	}
	/* not reached */
	return 0;
}


/*
 * Each of the following routines appies an unary operator to the
 * first argument. Return 1 on success, 0 on failure
 */

unsigned
data_neg(struct data *op1)
{
	if (op1->type == DATA_LONG) {
		op1->val.num = - op1->val.num;
		return 1;
	}
	cons_err("bad types in unary minus");
	return 0;
}

unsigned
data_bitnot(struct data *op1)
{
	if (op1->type == DATA_LONG) {
		op1->val.num = ~ op1->val.num;
		return 1;
	}
	cons_err("bad type in bitwise not ('~')");
	return 0;
}

unsigned
data_not(struct data *op1)
{
	if (data_eval(op1)) {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 0;
	} else {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 1;
	}
	return 1;
}


/*
 * Each of the following routines calculates the result of a binary
 * oprator applied to the couple of arguments and stores the result in
 * the first argument. Return 1 on success, 0 on failure
 */
unsigned
data_add(struct data *op1, struct data *op2)
{
	struct data **i;
	char *s1, *s2;

	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num += op2->val.num;
		return 1;
	} else if (op1->type == DATA_LIST && op2->type == DATA_LIST) {
		/*
		 * concatenate 2 lists
		 */
		for (i = &op1->val.list; *i != NULL; i = &(*i)->next)
			; /* nothing */
		*i = op2->val.list;
		op2->val.list = NULL;
		return 1;
	} else if (op1->type == DATA_STRING && op2->type == DATA_STRING) {
		/*
		 * concatenate 2 strings
		 */
		s1 = op1->val.str;
		s2 = op2->val.str;
		op1->val.str = str_cat(s1, s2);
		str_delete(s1);
		return 1;
	}
	cons_err("bad types in addition");
	return 0;
}

unsigned
data_sub(struct data *op1, struct data *op2)
{
	struct data **i, *j;

	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num -= op2->val.num;
		return 1;
	} else if (op1->type == DATA_LIST && op2->type == DATA_LIST) {
		/*
		 * remove from the first list all elements
		 * that are present in the second list
		 */
		i = &op1->val.list;
		while (*i != NULL) {
			for (j = op2->val.list; j != NULL; j = j->next) {
				if (data_id(*i, j)) {
					goto found;
				}
			}
			i = &(*i)->next;
			continue;
		found:
			j = *i;
			*i = j->next;
			data_delete(j);
			continue;
		}
		return 1;
	}
	cons_err("bad types in substraction");
	return 0;
}

unsigned
data_mul(struct data *op1, struct data *op2)
{
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num *= op2->val.num;
		return 1;
	}
	cons_err("bad types in multiplication ('*')");
	return 0;
}

unsigned
data_div(struct data *op1, struct data *op2)
{
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		if (op2->val.num == 0) {
			cons_err("division by zero");
			return 0;
		}
		op1->val.num /= op2->val.num;
		return 1;
	}
	cons_err("bad types in division ('/') ");
	return 0;
}

unsigned
data_mod(struct data *op1, struct data *op2)
{
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		if (op2->val.num == 0) {
			cons_err("division by zero");
			return 0;
		}
		op1->val.num %= op2->val.num;
		return 1;
	}
	cons_err("bad types in division ('%')");
	return 0;
}

unsigned
data_lshift(struct data *op1, struct data *op2)
{
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num <<= op2->val.num;
		return 1;
	}
	cons_err("bad types in left shift ('<<')");
	return 0;
}

unsigned
data_rshift(struct data *op1, struct data *op2)
{
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num >>= op2->val.num;
		return 1;
	}
	cons_err("bad types in right shift ('>>')");
	return 0;
}

unsigned
data_bitor(struct data *op1, struct data *op2)
{
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num |= op2->val.num;
		return 1;
	}
	cons_err("bad types in bitwise or ('|')");
	return 0;
}

unsigned
data_bitand(struct data *op1, struct data *op2)
{
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num &= op2->val.num;
		return 1;
	}
	cons_err("bad types in bitwise and ('&')");
	return 0;
}


unsigned
data_bitxor(struct data *op1, struct data *op2)
{
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num ^= op2->val.num;
		return 1;
	}
	cons_err("bad types in bitwise xor ('^')");
	return 0;
}

unsigned
data_eq(struct data *op1, struct data *op2)
{
	if (data_id(op1, op2)) {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 1;
	} else {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 0;
	}
	return 1;
}

unsigned
data_neq(struct data *op1, struct data *op2)
{
	if (data_id(op1, op2)) {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 0;
	} else {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 1;
	}
	return 1;
}

unsigned
data_lt(struct data *op1, struct data *op2)
{
	if (op1->type != DATA_LONG || op2->type != DATA_LONG) {
		cons_err("bad types in '<'");
		return 0;
	}
	op1->val.num = op1->val.num < op2->val.num ? 1 : 0;
	return 1;
}

unsigned
data_le(struct data *op1, struct data *op2)
{
	if (op1->type != DATA_LONG || op2->type != DATA_LONG) {
		cons_err("bad types in '<='");
		return 0;
	}
	op1->val.num = op1->val.num <= op2->val.num ? 1 : 0;
	return 1;
}

unsigned
data_gt(struct data *op1, struct data *op2)
{
	if (op1->type != DATA_LONG || op2->type != DATA_LONG) {
		cons_err("bad types in '>'");
		return 0;
	}
	op1->val.num = op1->val.num > op2->val.num ? 1 : 0;
	return 1;
}

unsigned
data_ge(struct data *op1, struct data *op2)
{
	if (op1->type != DATA_LONG || op2->type != DATA_LONG) {
		cons_err("bad types in '>='");
		return 0;
	}
	op1->val.num = op1->val.num >= op2->val.num ? 1 : 0;
	return 1;
}

unsigned
data_and(struct data *op1, struct data *op2)
{
	if (data_eval(op1) && data_eval(op2)) {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 1;
	} else {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 0;
	}
	return 1;
}

unsigned
data_or(struct data *op1, struct data *op2)
{
	if (data_eval(op1) || data_eval(op2)) {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 1;
	} else {
		data_clear(op1);
		op1->type = DATA_LONG;
		op1->val.num = 0;
	}
	return 1;
}

