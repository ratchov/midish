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
 * 	  materials  provided with the distribution.
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

#ifndef SEQ_NAME_H
#define SEQ_NAME_H

#include "str.h"

	/*
	 * a name is an entry in a simple list of strings
	 * the string buffer is owned by the name
	 */

struct name_s {
	char *str;
	struct name_s *next;
};

void	       name_init(struct name_s *o, char *name);
void	       name_done(struct name_s *o);
struct name_s *name_new(char *name);
struct name_s *name_newarg(char *name, struct name_s *next);
void	       name_dbg(struct name_s *);
void	       name_delete(struct name_s *o);
void	       name_insert(struct name_s **first, struct name_s *i);
void	       name_add(struct name_s **first, struct name_s *v);
void	       name_remove(struct name_s **first, struct name_s *v);
void	       name_empty(struct name_s **first);
void           name_cat(struct name_s **dst, struct name_s **src); 
unsigned       name_eq(struct name_s **first1, struct name_s **first2);

struct name_s *name_lookup(struct name_s **first, char *str);

#endif /* SEQ_NAME_H */
