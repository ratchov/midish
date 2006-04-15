/* $Id: sysex.h,v 1.5 2006/02/17 13:18:06 alex Exp $ */
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

#ifndef MIDISH_SYSEX_H
#define MIDISH_SYSEX_H

struct chunk {
	struct chunk *next;
	unsigned used;
#define CHUNK_SIZE	0x100
	unsigned char data[CHUNK_SIZE];
};

struct sysex {
	struct sysex *next;
	unsigned unit;
	struct chunk *first, *last;
};

struct sysexlist {
	struct sysex *first, **lastptr;
};

void 		chunk_pool_init(unsigned size);
void 		chunk_pool_done(void);
struct chunk *chunk_new(void);
void		chunk_del(struct chunk *o);

void		sysex_pool_init(unsigned size);
void		sysex_pool_done(void);
struct sysex *sysex_new(unsigned unit);
void		sysex_del(struct sysex *o);
void		sysex_add(struct sysex *o, unsigned data);
void		sysex_dbg(struct sysex *o);
unsigned	sysex_check(struct sysex *o);

void 		sysexlist_init(struct sysexlist *o);
void		sysexlist_done(struct sysexlist *o);
void		sysexlist_put(struct sysexlist *o, struct sysex *e);
struct sysex *sysexlist_get(struct sysexlist *o);
void		sysexlist_dbg(struct sysexlist *o);

#endif /* MIDISH_SYSEX_H */
