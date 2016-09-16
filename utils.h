/*
 * Copyright (c) 2003-2012 Alexandre Ratchov <alex@caoua.org>
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

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

struct prof {
	char *name;
	unsigned n;
	unsigned min;
	unsigned max;
	unsigned sum;
	unsigned sumsqr;
	unsigned err;
};

#ifdef __cplusplus
extern "C" {
#endif

void log_puts(char *);
void log_putx(unsigned long);
void log_putu(unsigned long);
void log_puti(long);
void log_perror(char *);
void log_putp(void *);
void panic(void);
void log_flush(void);

void *xmalloc(size_t, char *);
char *xstrdup(char *, char *);
void xfree(void *);

void prof_reset(struct prof *, char *);
void prof_val(struct prof *, unsigned);
void prof_log(struct prof *);

#ifdef __cplusplus
}
#endif

extern unsigned log_sync;

#endif /* UTILS_H */
