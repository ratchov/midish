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

#ifndef SEQ_POOL_H
#define SEQ_POOL_H

#define POOL_DEBUG

#define STACK_HEAD(type)						\
struct {								\
	struct type *first;						\
}

#define STACK_ENTRY(type)						\
struct {								\
	struct type *next;						\
}
 
#define	STACK_INIT(head) 						\
	do {								\
		(head)->first = 0;					\
	} while(0)

#define	STACK_FOREACH(i, head, field)					\
	for((i) = (head)->first; (i) != 0; (i) = (i)->field.next)

#define	STACK_PUSH(head, i, field)					\
	do {								\
		(i)->field.next = (head)->first;			\
		(head)->first = (i);					\
	} while (0)

#define	STACK_POP(head, i, field)					\
	do {								\
		(i) = (head)->first;					\
		(head)->first = (head)->first->field.next;		\
	} while (0)

struct poolentry_s {
	struct poolentry_s *next;
};

struct pool_s {
	unsigned char *data;
	struct poolentry_s *first;
#ifdef POOL_DEBUG
	unsigned maxused, used, newcnt;
#endif
	unsigned itemnum, itemsize;
	char *name;
};

void  pool_init(struct pool_s *, char *, unsigned, unsigned);
void  pool_done(struct pool_s *);

void *pool_new(struct pool_s *);
void  pool_del(struct pool_s *, void *);	

#endif /* SEQ_POOL_H */
