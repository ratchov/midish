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

#ifndef MIDISH_TIMO_H
#define MIDISH_TIMO_H

struct timo {
	struct timo *next;
	unsigned val;			/* time to wait before the callback */
	unsigned set;			/* true if the timeout is set */
	void (*cb)(void *arg);		/* routine to call on expiration */
	void *arg;			/* argument to give to 'cb' */
};

void timo_set(struct timo *, void (*)(void *), void *);
void timo_add(struct timo *, unsigned);
void timo_del(struct timo *);
void timo_update(unsigned);
void timo_init(void);
void timo_done(void);

extern unsigned timo_abstime;

#endif /* MIDISH_TIMO_H */
