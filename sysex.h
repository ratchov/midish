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

#ifndef MIDISH_SYSEX_H
#define MIDISH_SYSEX_H

struct chunk {
	struct chunk *next;
	unsigned used;			/* bytes used in 'data' */
#define CHUNK_SIZE	0x100
	unsigned char data[CHUNK_SIZE];
};

struct sysex {
	struct sysex *next;
	unsigned unit;			/* device number */
	struct chunk *first, *last;
};

struct sysexlist {
	struct sysex *first, **lastptr;
};

void 	      chunk_pool_init(unsigned);
void 	      chunk_pool_done(void);
struct chunk *chunk_new(void);
void	      chunk_del(struct chunk *);

void	      sysex_pool_init(unsigned);
void	      sysex_pool_done(void);
struct sysex *sysex_new(unsigned);
void	      sysex_del(struct sysex *);
void	      sysex_add(struct sysex *, unsigned);
void	      sysex_dbg(struct sysex *);
unsigned      sysex_check(struct sysex *);

void 	      sysexlist_init(struct sysexlist *);
void	      sysexlist_done(struct sysexlist *);
void	      sysexlist_clear(struct sysexlist *);
void	      sysexlist_put(struct sysexlist *, struct sysex *);
struct sysex *sysexlist_get(struct sysexlist *);
void	      sysexlist_dbg(struct sysexlist *);

#endif /* MIDISH_SYSEX_H */
