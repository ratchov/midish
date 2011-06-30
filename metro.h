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

#ifndef MIDISH_METRO_H
#define MIDISH_METRO_H

#include "ev.h"
#include "timo.h"

struct metro {
	unsigned mode;		/* same as song->mode */
	unsigned mask;		/* enabled if (mask | mode) != 0 */
	struct ev hi, lo;	/* high and low click note-on events */
	struct ev *ev;		/* event currently sounding (or NULL) */
	struct timo to;		/* timeout for the noteoff */
};

void metro_init(struct metro *);
void metro_done(struct metro *);
void metro_tic(struct metro *, unsigned, unsigned);
void metro_shut(struct metro *);
void metro_setmode(struct metro *, unsigned);
void metro_setmask(struct metro *, unsigned);
unsigned metro_str2mask(struct metro *, char *, unsigned *);

#endif /* MIDISH_METRO_H */
