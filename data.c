/* $Id$ */
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

/*
 * data_s is used to store values of the user variable
 * and contants and implements basig arithmetic
 *
 * a data_s can contain a "nil", a long, a string or a list
 */
	 
#include "dbg.h"
#include "str.h"
#include "cons.h"
#include "data.h"

struct data_s *
data_newnil(void) {
	struct data_s *o;
	o = (struct data_s *)mem_alloc(sizeof(struct data_s));
	o->type = DATA_NIL;
	o->next = NULL;
	return o;
}


struct data_s *
data_newlong(long val) {
	struct data_s *o;
	o = data_newnil();
	o->val.num = val;
	o->type = DATA_LONG;
	return o;
}


struct data_s *
data_newstring(char *val) {
	struct data_s *o;
	o = data_newnil();
	o->val.str = str_new(val);
	o->type = DATA_STRING;
	return o;
}


struct data_s *
data_newref(char *val) {
	struct data_s *o;
	o = data_newnil();
	o->val.ref = str_new(val);
	o->type = DATA_REF;
	return o;
}


struct data_s *
data_newlist(struct data_s *list) {
	struct data_s *o;
	o = data_newnil();
	o->val.list = list;
	o->type = DATA_LIST;
	return o;
}

struct data_s *
data_newuser(void *addr) {
	struct data_s *o;
	o = data_newnil();
	o->type = DATA_USER;
	o->val.user = addr;
	return o;
}


unsigned
data_numitem(struct data_s *o) {
	struct data_s *i;
	unsigned n;

	n = 1;
	if (o->type == DATA_LIST) {	
		for (i = o->val.list; i != NULL; i = i->next) {
			n += data_numitem(i);
		}
	}
	return n;
}

void
data_listadd(struct data_s *o, struct data_s *v) {
	struct data_s **i;
	i = &o->val.list;
	while (*i != NULL) {
		i = &(*i)->next;
	}
	v->next = NULL;
	*i = v;
}


void
data_listremove(struct data_s *o, struct data_s *v) {
	struct data_s **i;
	i = &o->val.list;
        while (*i != NULL) {
		if (*i == v) {
	        	*i = v->next;
	                v->next = NULL;
	                return;
		}
		i = &(*i)->next;
	}
	dbg_puts("data_listremove: not found\n");
	dbg_panic();
}



void
data_clear(struct data_s *o) {
	struct data_s *i, *inext;
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
		break;
	default:
		dbg_puts("data_clear: unknown type\n");
		dbg_panic();
		break;
	}
	o->type = DATA_NIL;
}

void
data_delete(struct data_s *o) {
	data_clear(o);
	mem_free(o);
}

void
data_dbg(struct data_s *o) {
	struct data_s *i;
	
	switch(o->type) {
	case DATA_NIL:
		dbg_puts("(nil)");
		break;
	case DATA_USER:
		dbg_puts("(user)");
		break;
	case DATA_LONG:
		if (o->val.num < 0) {
			dbg_puts("-");
			dbg_putu((unsigned) - o->val.num );
		} else {
			dbg_putu((unsigned)   o->val.num);
		}
		break;
	case DATA_STRING:
		dbg_puts("\"");
		dbg_puts(o->val.str);
		dbg_puts("\"");
		break;
	case DATA_REF:
		dbg_puts("@");
		str_dbg(o->val.ref);
		break;
	case DATA_LIST:
		dbg_puts("{");
		for (i = o->val.list; i != NULL; i = i->next) {
			data_dbg(i);
			if (i->next) {
				dbg_puts(" ");
			}
		}
		dbg_puts("}");
		break;
	default:
		dbg_puts("(unknown type)");
		break;
	}
}


/* -------------------------------------------------------------------- */

	/*
	 * copy src into dst
	 * never fails
	 */

void
data_assign(struct data_s *dst, struct data_s *src) {
	struct data_s *n, *i, **j;
	if (dst == src) {
		dbg_puts("data_assign: src and dst are the same\n");
		dbg_panic();
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
	default:
		dbg_puts("data_assign: bad data type\n");
		dbg_panic();
	}
}


	/*
	 * return 1 if op1 et op2 are identical, 
	 * 0 overwise
	 */


unsigned
data_id(struct data_s *op1, struct data_s *op2) {
	struct data_s *i1, *i2;
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
	default:
		dbg_puts("data_id: bad data types\n");
		dbg_panic();
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
data_eval(struct data_s *o) {
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
	default:
		dbg_puts("data_eval: bad data type\n");
		dbg_panic();
	}
	/* not reached */
	return 0;	
}

/* ----------------------------------------------------- relations --- */
			
unsigned
data_not(struct data_s *op1) {
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


unsigned
data_and(struct data_s *op1, struct data_s *op2) {
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
data_or(struct data_s *op1, struct data_s *op2) {
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


unsigned
data_eq(struct data_s *op1, struct data_s *op2) {
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
data_neq(struct data_s *op1, struct data_s *op2) {
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
data_lt(struct data_s *op1, struct data_s *op2) {
	if (op1->type != DATA_LONG || op2->type != DATA_LONG) {
		cons_err("bad types in '<'");
		return 0;
	}
	op1->val.num = op1->val.num < op2->val.num ? 1 : 0;
	return 1;
}


unsigned
data_le(struct data_s *op1, struct data_s *op2) {
	if (op1->type != DATA_LONG || op2->type != DATA_LONG) {
		cons_err("bad types in '<='");
		return 0;
	}
	op1->val.num = op1->val.num <= op2->val.num ? 1 : 0;
	return 1;
}


unsigned
data_gt(struct data_s *op1, struct data_s *op2) {
	if (op1->type != DATA_LONG || op2->type != DATA_LONG) {
		cons_err("bad types in '>'");
		return 0;
	}
	op1->val.num = op1->val.num > op2->val.num ? 1 : 0;
	return 1;
}


unsigned
data_ge(struct data_s *op1, struct data_s *op2) {
	if (op1->type != DATA_LONG || op2->type != DATA_LONG) {
		cons_err("bad types in '>='");
		return 0;
	}
	op1->val.num = op1->val.num >= op2->val.num ? 1 : 0;
	return 1;
}


/* ---------------------------------------------------- arithmetic --- */

unsigned 
data_add(struct data_s *op1, struct data_s *op2) {
	struct data_s **i;
	
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num += op2->val.num;
		return 1;
	} else if (op1->type == DATA_LIST && op2->type == DATA_LIST) {
		for (i = &op1->val.list; *i != NULL; i = &(*i)->next)
			; /* nothing */
		*i = op2->val.list;
		op2->val.list = NULL;
		return 1;
	}
	cons_err("bad types in addition");
	return 0;
}


unsigned
data_sub(struct data_s *op1, struct data_s *op2) {
	struct data_s **i, *j;

	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num -= op2->val.num;
		return 1;
	} else if (op1->type == DATA_LIST && op2->type == DATA_LIST) {
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
data_neg(struct data_s *op1) {
	if (op1->type == DATA_LONG) {
		op1->val.num = - op1->val.num;
		return 1;
	}
	cons_err("bad types in unary minus");
	return 0;
}


unsigned
data_mul(struct data_s *op1, struct data_s *op2) {
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num *= op2->val.num;
		return 1;
	}
	cons_err("bad types in multiplication ('*')");
	return 0;
}


unsigned
data_div(struct data_s *op1, struct data_s *op2) {
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
data_mod(struct data_s *op1, struct data_s *op2) {
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
data_lshift(struct data_s *op1, struct data_s *op2) {
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num <<= op2->val.num;
		return 1;
	}
	cons_err("bad types in left shift ('<<')");
	return 0;
}


unsigned
data_rshift(struct data_s *op1, struct data_s *op2) {
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num >>= op2->val.num;
		return 1;
	}
	cons_err("bad types in right shift ('>>')");
	return 0;
}


unsigned
data_bitor(struct data_s *op1, struct data_s *op2) {
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num |= op2->val.num;
		return 1;
	}
	cons_err("bad types in bitwise or ('|')");
	return 0;
}


unsigned
data_bitand(struct data_s *op1, struct data_s *op2) {
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num &= op2->val.num;
		return 1;
	}
	cons_err("bad types in bitwise and ('&')");
	return 0;
}


unsigned
data_bitxor(struct data_s *op1, struct data_s *op2) {
	if (op1->type == DATA_LONG && op2->type == DATA_LONG) {
		op1->val.num ^= op2->val.num;
		return 1;
	}
	cons_err("bad types in bitwise xor ('^')");
	return 0;
}


unsigned
data_bitnot(struct data_s *op1) {
	if (op1->type == DATA_LONG) {
		op1->val.num = ~ op1->val.num;
		return 1;
	}
	cons_err("bad type in bitwise not ('~')");
	return 0;
}

