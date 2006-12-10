/*
 * Copyright (c) 2003-2006 Alexandre Ratchov <alex@caoua.org>
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

#ifndef MIDISH_POOL_H
#define MIDISH_POOL_H

#define POOL_DEBUG

/*
 * entry of the pool. Any real pool entry is
 * cast to this structure by the pool code. The
 * actual size of a pool entry is in 'itemsize'
 * field of the pool structure
 */
struct poolent {
	struct poolent *next;
};

/*
 * the pool is a linked list of 'itemnum' blocks
 * of size 'itemsize'. The pool name is for
 * debugging prurposes only
 */
struct pool {
	unsigned char *data;	/* memory block of the pool */
	struct poolent *first;	/* head of linked list */
#ifdef POOL_DEBUG
	unsigned maxused;	/* max pool usage */
	unsigned used;		/* current pool usage */
	unsigned newcnt;	/* current items allocated */
#endif
	unsigned itemnum;	/* total number of entries */
	unsigned itemsize;	/* size of a sigle entry */
	char *name;		/* name of the pool */
};

void  pool_init(struct pool *, char *, unsigned, unsigned);
void  pool_done(struct pool *);

void *pool_new(struct pool *);
void  pool_del(struct pool *, void *);	

#endif /* MIDISH_POOL_H */
