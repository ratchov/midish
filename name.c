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
 * name is a singly-linked list of strings
 */

#include "utils.h"
#include "name.h"

void
name_init(struct name *o, char *name)
{
	o->str = str_new(name);
}

void
name_done(struct name *o)
{
	str_delete(o->str);
}

struct name *
name_new(char *name)
{
	struct name *o;

	o = xmalloc(sizeof(struct name), "name");
	name_init(o, name);
	return o;
}

struct name *
name_newarg(char *name, struct name *next)
{
	struct name *o;
	o = name_new(name);
	o->next = next;
	return o;
}

void
name_delete(struct name *o)
{
	name_done(o);
	xfree(o);
}

void
name_insert(struct name **first, struct name *i)
{
	i->next = *first;
	*first = i;
}

void
name_add(struct name **first, struct name *v)
{
	struct name **i;

	i = first;
	while (*i != NULL) {
		i = &(*i)->next;
	}
	v->next = NULL;
	*i = v;
}

void
name_remove(struct name **first, struct name *v)
{
	struct name **i;

	i = first;
	while (*i != NULL) {
		if (*i == v) {
			*i = v->next;
			v->next = NULL;
			return;
		}
		i = &(*i)->next;
	}
	logx(1, "%s: not found", __func__);
	panic();
}

void
name_empty(struct name **first)
{
	struct name *i, *inext;

	for (i = *first; i != NULL; i = inext) {
		inext = i->next;
		name_delete(i);
	}
	*first = NULL;
}

void
name_cat(struct name **dst, struct name **src)
{
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
name_eq(struct name **first1, struct name **first2)
{
	struct name *n1 = *first1, *n2 = *first2;

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

struct name *
name_lookup(struct name **first, char *str)
{
	struct name *i;

	for (i = *first; i != NULL; i = i->next) {
		if (i->str == NULL)
			continue;
		if (str_eq(i->str, str))
			return i;
	}
	return 0;
}
