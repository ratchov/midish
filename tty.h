/*
 * Copyright (c) 2015 Alexandre Ratchov <alex@caoua.org>
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
#ifndef TTY_H
#define TTY_H

#include <stddef.h>

struct pollfd;

int tty_init(void);
void tty_done(void);
int tty_pollfd(struct pollfd *);
int tty_revents(struct pollfd *);
void tty_winch(void);
void tty_int(void);
void tty_reset(void);

struct el_ops {
	/*
	 * Called for every character when a new line is entered,
	 * including for \n and EOF.
	 */
	void (*onchar)(void *, int);

	/*
	 * Called when a completion list must be built (when the TAB
	 * key is hit the first time).  The function gets a pointer to
	 * the text, its size and the current cursor position.  It
	 * must parse the text, build the appropriate completion list
	 * (by calling el_compladd() for each item) and return in the
	 * extents of the sub-string being completed (for instance the
	 * last component of the path).
	 */
	void (*oncompl)(void *, char *, int, int, int *, int *);
};

void el_init(struct el_ops *, void *);
void el_done(void);
void el_hide(void);
void el_show(void);
void el_setprompt(char *);
void el_compladd(char *);

#endif
