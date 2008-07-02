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

void	     name_init(struct name *o, char *name);
void	     name_done(struct name *o);
struct name *name_new(char *name);
struct name *name_newarg(char *name, struct name *next);
void	     name_dbg(struct name *);
void	     name_delete(struct name *o);
void	     name_insert(struct name **first, struct name *i);
void	     name_add(struct name **first, struct name *v);
void	     name_remove(struct name **first, struct name *v);
void	     name_empty(struct name **first);
void         name_cat(struct name **dst, struct name **src);
unsigned     name_eq(struct name **first1, struct name **first2);
struct name *name_lookup(struct name **first, char *str);

#endif /* MIDISH_NAME_H */
