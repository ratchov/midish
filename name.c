/* $Id: name.c,v 1.5 2006/02/14 12:21:41 alex Exp $ */
/*
 * Copyright (c) 2003-2006 Alexandre Ratchov
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
 * name_s is a simply linked list of strings
 */


#include "dbg.h"
#include "name.h"

void
name_init(struct name_s *o, char *name) {
	o->str = str_new(name);
	o->next = NULL;
}

void
name_done(struct name_s *o) {
	str_delete(o->str);
}

struct name_s *
name_new(char *name) {
	struct name_s *o;
	o = (struct name_s *)mem_alloc(sizeof(struct name_s));
	name_init(o, name);
	return o;
}

struct name_s *
name_newarg(char *name, struct name_s *next) {
	struct name_s *o;
	o = name_new(name);
	o->next = next;
	return o;
}

void
name_delete(struct name_s *o) {
	name_done(o);
	mem_free(o);
}

void
name_dbg(struct name_s *o) {
	for (; o != NULL; o = o->next) {
		str_dbg(o->str);
		if (o->next) {
			dbg_puts(".");
		}
	}
}

void
name_insert(struct name_s **first, struct name_s *i) {
	i->next = *first;
	*first = i;
}

void
name_add(struct name_s **first, struct name_s *v) {
	struct name_s **i;
	i = first;
	while (*i != NULL) {
		i = &(*i)->next;
	}
	v->next = NULL;
	*i = v;
}

void
name_remove(struct name_s **first, struct name_s *v) {
	struct name_s **i;
	i = first;
	while (*i != NULL) {
		if (*i == v) {
			*i = v->next;
			v->next = NULL;
			return;
		}
		i = &(*i)->next;
	}
	dbg_puts("name_remove: not found\n");
	dbg_panic();
}

void
name_empty(struct name_s **first) {
	struct name_s *i, *inext;
	for (i = *first; i != NULL; i = inext) {
		inext = i->next;
		name_delete(i);
	}
	*first = NULL;
}


void
name_cat(struct name_s **dst, struct name_s **src) {
	while (*dst != NULL) {
		dst = &(*dst)->next;
	}
	while (*src != NULL) {
		*dst = name_new((*src)->str);
		dst = &(*dst)->next;
		src = &(*src)->next;
	}
}


unsigned
name_eq(struct name_s **first1, struct name_s **first2) {
	struct name_s *n1 = *first1, *n2 = *first2;
	
	for (;;) {
		if (n1 == NULL && n2 == NULL) {
			return 1;
		} else if (n1 == NULL || n2 == NULL || !str_eq(n1->str, n2->str)) {
			return 0;
		}
		n1 = n1->next;
		n2 = n2->next;
	}
}


struct name_s *
name_lookup(struct name_s **first, char *str) {
	struct name_s *i;
	for (i = *first; i != NULL; i = i->next) {
		if (i->str == NULL)
			continue;
		if (str_eq(i->str, str))
			return i;
	}
	return 0;
}
