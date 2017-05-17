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

#define TTY_KEY_SHIFT		(1 << 31)
#define TTY_KEY_CTRL		(1 << 30)
#define TTY_KEY_ALT		(1 << 29)

enum TTY_KEY {
	/*
	 * we put special "control" keys in the "private" unicode plane,
	 * allowing to encode any key as a simple int
	 *
	 * certain function keys (tab, enter, delete... ) have unicode
	 * character, but since we don't use them as "characters" we
	 * put them in the private plane as well, shifted by 0x100000
	 */
	TTY_KEY_BS = 0x100008,
	TTY_KEY_TAB = 0x100009,
	TTY_KEY_ENTER = 0x10000d,
	TTY_KEY_ESC = 0x10001b,
	TTY_KEY_DEL = 0x10007f,
	TTY_KEY_UP = 0x100100, TTY_KEY_DOWN, TTY_KEY_LEFT, TTY_KEY_RIGHT,
	TTY_KEY_HOME, TTY_KEY_END, TTY_KEY_PGUP, TTY_KEY_PGDOWN,
	TTY_KEY_INSERT
};

#define TTY_TSTATE_ANY		0	/* expect any char */
#define TTY_TSTATE_ESC		1	/* got ESC */
#define TTY_TSTATE_CSI		2	/* got CSI, parsing it */
#define TTY_TSTATE_CSI_PAR	3	/* parsing CSI param (number) */
#define TTY_TSTATE_ERROR	4	/* found error, skipping */
#define TTY_TSTATE_INT		5	/* got ESC */
#define TTY_ESC_NPAR		8	/* max params in CSI */

struct pollfd;

struct tty_ops {
	void (*draw)(void *);
	void (*resize)(void *, int);
	void (*onkey)(void *, int);
};

int tty_init(void);
void tty_done(void);
int tty_pollfd(struct pollfd *);
int tty_revents(struct pollfd *);
void tty_winch(void);
void tty_int(void);
void tty_reset(void);
void tty_write(void *, size_t);

void tty_tflush(void);
void tty_toutput(char *, int);
void tty_tsetcurs(int);
void tty_tputs(int, int, char *, int);
void tty_tclear(void);
void tty_tendl(void);

extern struct tty_ops *tty_ops;
extern void *tty_arg;

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
void el_setprompt(char *);
void el_compladd(char *);

extern struct tty_ops el_tty_ops;

#endif
