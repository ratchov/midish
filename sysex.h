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

#ifndef MIDISH_SYSEX_H
#define MIDISH_SYSEX_H

struct chunk_s {
	struct chunk_s *next;
	unsigned used;
#define CHUNK_SIZE	0x100
	unsigned char data[CHUNK_SIZE];
};

struct sysex_s {
	struct sysex_s *next;
	unsigned unit;
	struct chunk_s *first, *last;
};

struct sysexlist_s {
	struct sysex_s *first, **lastptr;
};

void chunk_pool_init(unsigned size);
void chunk_pool_done(void);
struct chunk_s *chunk_new(void);
void chunk_del(struct chunk_s *o);

void sysex_pool_init(unsigned size);
void sysex_pool_done(void);
struct sysex_s *sysex_new(unsigned unit);
void sysex_del(struct sysex_s *o);
void sysex_add(struct sysex_s *o, unsigned data);
void sysex_dbg(struct sysex_s *o);

void sysexlist_init(struct sysexlist_s *o);
void sysexlist_done(struct sysexlist_s *o);
void sysexlist_put(struct sysexlist_s *o, struct sysex_s *e);
struct sysex_s *sysexlist_get(struct sysexlist_s *o);
void sysexlist_dbg(struct sysexlist_s *o);

#endif /* MIDISH_SYSEX_H */
