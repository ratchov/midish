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

#ifndef MIDISH_NAME_H
#define MIDISH_NAME_H

#include "str.h"

/*
 * a name is an entry in a simple list of strings the string buffer is
 * owned by the name, so it need not to be allocated if name_xxx
 * routines are used
 */
struct name {
	char *str;
	struct name *next;
};

void	     name_init(struct name *, char *);
void	     name_done(struct name *);
struct name *name_new(char *);
struct name *name_newarg(char *, struct name *);
void	     name_log(struct name *);
void	     name_delete(struct name *);
void	     name_insert(struct name **, struct name *);
void	     name_add(struct name **, struct name *);
void	     name_remove(struct name **, struct name *);
void	     name_empty(struct name **);
void         name_cat(struct name **, struct name **);
unsigned     name_eq(struct name **, struct name **);
struct name *name_lookup(struct name **, char *);

#endif /* MIDISH_NAME_H */
