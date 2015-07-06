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

#ifndef MIDISH_TEXTIO_H
#define MIDISH_TEXTIO_H

#define CHAR_EOF (-1)

struct textin;
struct textout;

struct textin *textin_new(char *);
void textin_delete(struct textin *);
unsigned textin_getchar(struct textin *, int *);
void textin_getpos(struct textin *, unsigned *, unsigned *);

struct textout *textout_new(char *);
void textout_delete(struct textout *);
void textout_indent(struct textout *);
void textout_shiftleft(struct textout *);
void textout_shiftright(struct textout *);
void textout_putstr(struct textout *, char *);
void textout_putlong(struct textout *, unsigned long);
void textout_putbyte(struct textout *, unsigned);

/* ------------------------------------------------- stdin/stdout --- */

extern struct textout *tout;
extern struct textin *tin;

void textio_init(void);
void textio_done(void);

#endif /* MIDISH_TEXTIO_H */
