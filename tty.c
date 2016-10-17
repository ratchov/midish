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
/*
 * see http://www.inwap.com/pdp10/ansicode.txt
 */
/*
 * TODO
 *
 * - we've the following pattern repeated many times, factor it
 *
 *	min = el_curs;
 *	max = el_used;
 *	...
 *	if (max < el_used)
 *		max = el_used;
 *	el_refresh(min, max);
 *
 * - make inserting chars in completion mode search starting
 *   from the current item
 *
 * - ^G during completion must revert to selection
 */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

#include "tty.h"
#include "utils.h"

#define EL_MODE_EDIT		0
#define EL_MODE_SEARCH		1
#define EL_MODE_COMPL		2

#define EL_LINEMAX	1024
#define EL_PROMPTMAX	64
#define EL_HISTMAX	200

struct textline {
	struct textline *next, *prev;
	char text[1];
};

struct textbuf {
	struct textline *head, *tail;
	unsigned int count;
};

void el_draw(void *);
void el_resize(void *, int);
void el_onkey(void *, int);

struct termios tty_tattr;			/* backup of attributes */
int tty_tstate;					/* one of TTY_TSTATE_xxx */
char tty_escbuf[4];				/* escape sequence */
int tty_escpar[TTY_ESC_NPAR];			/* escape seq. parameters */
int tty_nescpar;				/* number of params found */
int tty_escval;					/* temp var to parse params */
int tty_twidth;					/* terminal width */
int tty_tcursx;					/* terminal cursor x pos */

char tty_obuf[1024];				/* output buffer */
int tty_oused;					/* bytes used in tty_obuf */
int tty_initialized;

struct tty_ops *tty_ops;
void *tty_arg;

char el_prompt[EL_PROMPTMAX];			/* current prompt */
char el_buf[EL_LINEMAX];			/* line being editted */
int el_used = 0;				/* chars used in the buffer */
int el_curs = 0;				/* char pointed by cursor */
int el_offs = 0;				/* most left char displayed */
int el_pos = 0;					/* on screen text position */
int el_width = 0;				/* on screen text width */
int el_start = 0;				/* tty_read() position */
int el_mode;					/* one of EL_MODE_xxx */
struct textline *el_curline;
int el_selstart, el_selend;
struct el_ops *el_ops;
void *el_arg;
struct textbuf el_hist, el_compl;

struct tty_ops el_tty_ops = {
	el_draw,
	el_resize,
	el_onkey
};

void
textbuf_rm(struct textbuf *p, struct textline *l)
{
	if (l->next)
		l->next->prev = l->prev;
	if (l->prev)
		l->prev->next = l->next;
	if (p->head == l)
		p->head = l->next;
	if (p->tail == l)
		p->tail = l->prev;
	p->count--;
	xfree(l);
}

void
textbuf_ins(struct textbuf *p, struct textline *next, char *line, size_t len)
{
	struct textline *l;

	l = xmalloc(offsetof(struct textline, text) + len + 1, "textline");
	memcpy(l->text, line, len);
	l->text[len] = 0;

	l->next = next;
	if (next) {
		l->prev = next->prev;
		next->prev = l;
	} else {
		l->prev = p->tail;
		p->tail = l;
	}
	if (l->prev)
		l->prev->next = l;
	else
		p->head = l;
	p->count++;
}

int
textbuf_search(struct textbuf *p,
	char *pat, size_t patlen,
	struct textline **rline, unsigned int *rpos)
{
	struct textline *l;
	unsigned int pos, i;

	if (patlen == 0)
		return 0;
	l = *rline;
	while (1) {
		if (l == NULL)
			l = p->tail;
		else
			l = l->prev;
		if (l == NULL)
			break;
		pos = 0;
		while (1) {
			if (rpos == NULL || l->text[pos] == 0)
				break;
			i = 0;
			while (1) {
				if (i == patlen) {
					if (rpos)
						*rpos = pos + i;
					*rline = l;
					return 1;
				}
				if (pat[i] != l->text[pos + i])
					break;
				i++;
			}
			pos++;
		}
	}
	return 0;
}

void
textbuf_init(struct textbuf *p)
{
	p->head = p->tail = NULL;
	p->count = 0;
}

void
textbuf_done(struct textbuf *p)
{
	while (p->head)
		textbuf_rm(p, p->head);
}

void
textbuf_copy(struct textbuf *p, char *buf, size_t size, struct textline *l)
{
	char *s;

	if (l == NULL)
		buf[0] = 0;
	else {
		s = l->text;
		while (1) {
			*buf = *s;
			if (*s == 0)
				break;
			buf++;
			s++;
		}
	}
}

int
textbuf_findline(struct textbuf *p,
	char *pat, size_t patlen,
	struct textline **rline)
{
	struct textline *l;
	unsigned int i;

	l = *rline;
	while (1) {
		if (l == NULL)
			return 0;
		i = 0;
		while (1) {
			if (i == patlen) {
				*rline = l;
				return 1;
			}
			if (pat[i] != l->text[i])
				break;
			i++;
		}
		l = l->next;
	}
}

void
el_enter(void)
{
	char *p = el_buf;

	/* append new line */
	el_buf[el_used++] = '\n';

	/* invoke caller handler */
	while (el_used-- > 0)
		el_ops->onchar(el_arg, *p++);
}

void
el_replace(size_t start, size_t end, char *text, size_t textsize)
{
	int i, len, off;

	if (el_used - (end - start) + textsize > EL_LINEMAX) {
		log_puts("text to paste too long (ignored)\n");
		return;
	}

	/*
	 * move text after selection
	 */
	len = el_used - end;
	off = end - start - textsize;
	if (off > 0) {
		i = end;
		while (i < el_used) {
			el_buf[i - off] = el_buf[i];
			i++;
		}
	} else if (off < 0) {
		i = el_used;
		while (i > end) {
			i--;
			el_buf[i - off] = el_buf[i];
		}
	}
	el_used -= off;

	/*
	 * copy text into hole
	 */
	for (i = 0; i < textsize; i++)
		el_buf[start + i] = text[i];

	end = start + textsize;
}

void
el_refresh(int start, int end)
{
	int x, w, todo;

	if (el_curs < el_offs) {
		/*
		 * need to scroll left
		 */
		el_offs = el_curs;
		start = el_offs;
		end = el_offs + el_width;
	} else if (el_curs > el_offs + el_width - 1) {
		/*
		 * need to scroll right
		 */
		el_offs = el_curs - el_width + 1;
		start = el_offs;
		end = el_offs + el_width;
	} else {
		/*
		 * no need to scroll, clip to our width
		 */
		if (start < el_offs)
			start = el_offs;
		if (end > el_offs + el_width)
			end = el_offs + el_width;
	}
	if (end > start) {
		x = start - el_offs;
		w = end - start;
		if (end > el_used)
			todo = el_used - start;
		else
			todo = end - start;
		tty_tputs(el_pos + x, w, el_buf + start, todo);
	}
	tty_tsetcurs(el_pos + el_curs - el_offs);
}

char *
el_getprompt(void)
{
#if 1
	char *prompt;

	switch (el_mode) {
	case EL_MODE_EDIT:
		prompt = el_prompt;
		break;
	case EL_MODE_SEARCH:
		prompt = "search: ";
		break;
	case EL_MODE_COMPL:
		prompt = "complete: ";
		break;
	default:
		prompt = "unknown: ";
	}
	return prompt;
#else
	return el_prompt;
#endif
}

void
el_compladd(char *s)
{
	struct textline *l;
	int cmp;

	for (l = el_compl.head; l != NULL; l = l->next) {
		cmp = strcmp(l->text, s);
		if (cmp == 0) {
			log_puts(s);
			log_puts(": duplicate completion item\n");
			panic();
		}
		if (cmp > 0)
			break;
	}
	textbuf_ins(&el_compl, l, s, strlen(s));
}

void
compl_end(void)
{
	while (el_compl.head)
		textbuf_rm(&el_compl, el_compl.head);
}

void
compl_next(void)
{
	unsigned int max;

	if (textbuf_findline(&el_compl,
		el_buf + el_selstart, el_curs - el_selstart,
		&el_curline)) {
		max = el_used;
		el_replace(el_selstart, el_selend,
		    el_curline->text, strlen(el_curline->text));
		el_selend = el_selstart + strlen(el_curline->text);
		if (max < el_used)
			max = el_used;
		el_refresh(el_curs, max);
	} else
		el_curline = NULL;
}

void
compl_start(void)
{
	unsigned int min, max, off, i;
	struct textline *n;

	el_ops->oncompl(el_arg, el_buf, el_curs, el_used, &el_selstart, &el_selend);
	el_curline = el_compl.head;

	if (textbuf_findline(&el_compl,
		el_buf + el_selstart, el_curs - el_selstart,
		&el_curline)) {
		min = el_curs;
		max = el_used;
		el_replace(el_selstart, el_selend,
		    el_curline->text, strlen(el_curline->text));
		el_selend = el_selstart + strlen(el_curline->text);

		/*
		 * complete ensuring uniqueness
		 */
		off = el_selend - el_selstart;
		n = el_curline;
		while (1) {
		next_line:
			n = n->next;
			if (n == NULL)
				break;
			i = 0;
			while (1) {
				if (i == off)
					goto next_line;
				if (n->text[i] != el_buf[el_selstart + i])
					break;
				i++;
			}
			if (el_selstart + i < el_curs)
				break;
			off = i;
		}
		el_curs = el_selstart + off;

		if (max < el_used)
			max = el_used;
		el_refresh(min, max);
	} else
		el_curline = NULL;
}

void
el_setmode(int mode)
{
	if (el_mode != mode) {
		if (el_mode == EL_MODE_COMPL)
			compl_end();
		el_mode = mode;
		el_resize(NULL, tty_twidth);
	}
}

void
el_draw(void *arg)
{
	tty_tclear();
	if (el_pos > 0)
		tty_tputs(0, el_pos, el_getprompt(), el_pos);
	el_refresh(el_offs, el_offs + el_width);
	tty_tflush();
}

void
el_search(void)
{
	unsigned int cpos;

	if (textbuf_search(&el_hist,
		el_buf + el_selstart,
		el_curs - el_selstart,
		&el_curline, &cpos)) {
		textbuf_copy(&el_hist, el_buf, EL_LINEMAX, el_curline);
		el_used = strlen(el_buf);
		el_curs = cpos;
		el_selstart = cpos - (el_selend - el_selstart); 
		el_selend = cpos;
	} else {
		el_replace(el_selend, el_used, NULL, 0);
		el_replace(0, el_selstart, NULL, 0);
		el_curs = el_used;
		el_selstart = 0;
		el_selend = el_used;
		el_offs = 0;
		el_curline = NULL;
	}
}

void
el_onkey(void *arg, int key)
{
	char text[4];
	int endpos;
	size_t max;

	if (key == 0) {
		el_ops->onchar(el_arg, -1);
		return;
	}
	if (key == (TTY_KEY_CTRL | 'C')) {
		tty_int();
		return;
	}
	if (key < 0x100) {
		text[0] = key;
		if (el_mode == EL_MODE_COMPL) {
			el_selstart = el_curs;
			el_replace(el_selstart, el_selend, NULL, 0);
			el_selend = el_selstart;
			el_setmode(EL_MODE_EDIT);
		}
		if (el_mode == EL_MODE_EDIT) {
			if (el_used == EL_LINEMAX - 1)
				return;
			el_replace(el_curs, el_curs, text, 1);
			el_curs++;
			el_refresh(el_curs - 1, el_used);
		} else if (el_mode == EL_MODE_SEARCH) {
			if (el_used == EL_LINEMAX - 1)
				return;
			el_replace(el_curs, el_curs, text, 1);
			el_curs++;
			el_selend++;
			max = el_used;
			el_curline = NULL;
			el_search();
			if (max < el_used)
				max = el_used;
			el_refresh(0, max);
		}
	} else if (key == TTY_KEY_DEL || key == (TTY_KEY_CTRL | 'D')) {
		el_setmode(EL_MODE_EDIT);
		if (el_curs == el_used)
			return;
		max = el_used;
		el_replace(el_curs, el_curs + 1, NULL, 0);
		el_refresh(el_curs, max);
	} else if (key == TTY_KEY_BS || key == (TTY_KEY_CTRL | 'H')) {
		if (el_mode == EL_MODE_COMPL) {
			el_selstart = el_curs;
			el_replace(el_selstart, el_selend, NULL, 0);
			el_selend = el_selstart;
			el_setmode(EL_MODE_EDIT);
		}
		if (el_mode == EL_MODE_EDIT) {
			if (el_curs == 0)
				return;
			max = el_used;
			el_curs--;
			el_replace(el_curs, el_curs + 1, NULL, 0);
			el_refresh(el_curs, max);
		} else if (el_mode == EL_MODE_SEARCH) {
			max = el_used;
			if (el_curs == 0) {
				el_setmode(EL_MODE_EDIT);
				el_used = el_curs = 0;
			} else {
				el_selend--;
				el_curs--;
				el_replace(el_curs, el_curs + 1, NULL, 0);
				el_curline = NULL;
				el_search();
			}
			if (max < el_used)
				max = el_used;
			el_refresh(0, max);
		}
	} else if (key == TTY_KEY_LEFT || key == (TTY_KEY_CTRL | 'B')) {
		el_setmode(EL_MODE_EDIT);
		if (el_curs == 0)
			return;
		el_curs--;
		el_refresh(el_curs, el_curs);
	} else if (key == TTY_KEY_RIGHT || key == (TTY_KEY_CTRL | 'F')) {
		el_setmode(EL_MODE_EDIT);
		if (el_curs == el_used)
			return;
		el_curs++;
		el_refresh(el_curs, el_curs);
	} else if (key == TTY_KEY_HOME || key == (TTY_KEY_CTRL | 'A')) {
		el_setmode(EL_MODE_EDIT);
		if (el_curs == 0)
			return;
		el_curs = 0;
		el_refresh(el_curs, el_curs);
	} else if (key == TTY_KEY_END || key == (TTY_KEY_CTRL | 'E')) {
		el_setmode(EL_MODE_EDIT);
		if (el_curs == el_used)
			return;
		el_curs = el_used;
		el_refresh(el_curs, el_curs);
	} else if (key == (TTY_KEY_CTRL | 'K')) {
		el_setmode(EL_MODE_EDIT);
		endpos = el_used;
		el_used = el_curs;
		el_refresh(el_curs, endpos);
	} else if (key == TTY_KEY_UP || key == (TTY_KEY_CTRL | 'P')) {
		el_setmode(EL_MODE_EDIT);
		if (el_curline != el_hist.head) {
			max = el_used;
			if (el_curline == NULL)
				el_curline = el_hist.tail;
			else
				el_curline = el_curline->prev;
			textbuf_copy(&el_hist, el_buf, EL_LINEMAX, el_curline);
			el_used = strlen(el_buf);
			el_offs = 0;
			el_curs = el_used;
			if (max < el_used)
				max = el_used;
			el_refresh(0, max);
		}
	} else if (key == TTY_KEY_DOWN || key == (TTY_KEY_CTRL | 'N')) {
		el_setmode(EL_MODE_EDIT);
		if (el_curline != NULL) {
			max = el_used;
			el_curline = el_curline->next;
			textbuf_copy(&el_hist, el_buf, EL_LINEMAX, el_curline);
			el_used = strlen(el_buf);
			el_offs = 0;
			el_curs = el_used;
			if (max < el_used)
				max = el_used;
			el_refresh(0, max);
		}
	} else if (key == TTY_KEY_ENTER) {
		if (el_mode == EL_MODE_COMPL)
			el_setmode(EL_MODE_EDIT);
		if (el_mode == EL_MODE_EDIT) {
			tty_tendl();
			if (el_used > 0) {
				if (el_hist.count == EL_HISTMAX)
					textbuf_rm(&el_hist, el_hist.head);
				textbuf_ins(&el_hist, NULL, el_buf, el_used);
				el_curline = NULL;
			}
			el_enter();
			el_curs = el_offs = el_used = 0;
			el_draw(arg);
		} else if (el_mode == EL_MODE_SEARCH) {
			el_setmode(EL_MODE_EDIT);
		}
	} else if (key == (TTY_KEY_CTRL | 'L')) {
		el_resize(NULL, tty_twidth);
	} else if (key == (TTY_KEY_CTRL | 'G')) {
		if (el_mode != EL_MODE_EDIT)
			el_setmode(EL_MODE_EDIT);
	} else if (key == (TTY_KEY_CTRL | 'R')) {
		max = el_used;
		if (el_mode != EL_MODE_SEARCH) {
			el_setmode(EL_MODE_SEARCH);
			el_curs = el_used = el_selstart = el_selend = 0;
			el_curline = NULL;
		}
		el_search();
		if (max < el_used)
			max = el_used;
		el_refresh(0, max);
	} else if (key == TTY_KEY_TAB || key == (TTY_KEY_CTRL | 'I')) {
		if (el_mode == EL_MODE_EDIT) {
			compl_start();
			if (el_curline && el_curs < el_selend)
				el_setmode(EL_MODE_COMPL);
			else
				compl_end();
		} else if (el_mode == EL_MODE_COMPL) {
			el_curline = el_curline->next;
			compl_next();
			if (el_curline == NULL && el_compl.head) {
				el_curline = el_compl.head;
				compl_next();
			}
		}
	}
}

void
el_resize(void *arg, int w)
{
	int end;

	el_pos = strlen(el_getprompt());
	if (el_pos >= w)
		el_pos = 0;
	el_width = w - el_pos;
	if (w == 0)
		return;
	end = el_offs + el_width - 1;
	if (el_curs > end)
		el_curs = end;
	el_draw(arg);
}

void
el_setprompt(char *str)
{
	size_t len, dif;

	len = strlen(str);
	if (len > EL_PROMPTMAX - 1) {
		dif = len - (EL_PROMPTMAX - 1);
		len -= dif;
		str += dif;
	}
	memcpy(el_prompt, str, len);
	el_prompt[len] = 0;
	el_resize(NULL, tty_twidth);
}

void
el_init(struct el_ops *ops, void *arg)
{
	el_ops = ops;
	el_arg = arg;
	el_width = el_curs = el_used = el_offs = el_pos = 0;
	el_prompt[0] = '\0';
#ifdef DEBUG
	memset(el_buf, '.', sizeof(el_buf));
#endif
	textbuf_init(&el_hist);
	textbuf_init(&el_compl);
	el_curline = NULL;
	el_mode = EL_MODE_EDIT;

	tty_ops = &el_tty_ops;
	tty_arg = NULL;
}

void
el_done(void)
{
	textbuf_done(&el_compl);
	textbuf_done(&el_hist);
}

int
tty_init(void)
{
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
		return 0;
	if (tcgetattr(STDIN_FILENO, &tty_tattr) < 0) {
		log_perror("can't get tty attributes: tcgetattr");
		return 0;
	}
	tty_ops = NULL;
	tty_arg = NULL;
	tty_initialized = 1;
	return 1;
}

void
tty_done(void)
{
	if (tty_initialized) {
		tty_tclear();
		tty_tflush();
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tty_tattr) < 0)
			log_perror("can't flush tty: tcsetattr");
		tty_initialized = 0;
	}
}

/*
 * convert escape sequence to TTY_KEY_xxx
 */
void
tty_onesc(void)
{
	int c, key;

	switch (tty_escbuf[1]) {
	case '[':
		switch (tty_escbuf[2]) {
		case 'A':
			key = TTY_KEY_UP;
			break;
		case 'B':
			key = TTY_KEY_DOWN;
			break;
		case 'C':
			key = TTY_KEY_RIGHT;
			break;
		case 'D':
			key = TTY_KEY_LEFT;
			break;
		case 'F':
			key = TTY_KEY_END;
			break;
		case 'H':
			key = TTY_KEY_HOME;
			break;
		case '~':
			if (tty_nescpar == 0)
				return;
			switch (tty_escpar[0]) {
			case 1:
				key = TTY_KEY_HOME;
				break;
			case 2:
				key = TTY_KEY_INSERT;
				break;
			case 3:
				key = TTY_KEY_DEL;
				break;
			case 4:
				key = TTY_KEY_END;
				break;
			case 5:
				key = TTY_KEY_PGUP;
				break;
			case 6:
				key = TTY_KEY_PGDOWN;
				break;
			default:
				return;
			}
			break;
		default:
			return;
		}
		break;
	case '0':
		switch (tty_escbuf[2]) {
		case 'A':
			key = TTY_KEY_UP;
			break;
		case 'B':
			key = TTY_KEY_DOWN;
			break;
		case 'C':
			key = TTY_KEY_RIGHT;
			break;
		case 'D':
			key = TTY_KEY_LEFT;
			break;
		case 'M':
			key = TTY_KEY_ENTER;
			break;
		case 'j':
			key = '*';
			break;
		case 'k':
			key = '+';
			break;
		case 'l':
			key = ',';
			break;
		case 'm':
			key = '-';
			break;
		case 'n':
			key = '.';
			break;
		case 'o':
			key = '/';
			break;
		default:
			c = tty_escbuf[2];
			if (c >= 'p' && c <= 'p' + 10)
				key = '0' + c - 'p';
			else
				return;
		}
	default:
		return;
	}
	tty_ops->onkey(tty_arg, key);
}

void
tty_oninput(unsigned int c)
{
	int key;

	if (c >= 0x80)
		return;
	for (;;) {
		switch (tty_tstate) {
		case TTY_TSTATE_ANY:
			switch (c) {
			case 0x09:
				key = TTY_KEY_TAB;
				break;
			case 0x0a:
				key = TTY_KEY_ENTER;
				break;
			case 0x08:
			case 0x7f:
				key = TTY_KEY_BS;
				break;
			case 0x1b:
				tty_tstate = TTY_TSTATE_ESC;
				return;
			default:
				key = c <= 0x1f ?
				    ('@' + c) | TTY_KEY_CTRL : c;
			}
			tty_ops->onkey(tty_arg, key);
			return;
		case TTY_TSTATE_ESC:
			if (c >= 0x20 && c <= 0x2f) {
				/* 2-char esc sequence, we ignore it */
				tty_tstate = TTY_TSTATE_INT;
			} else if (c >= 0x30 && c <= 0x3f) {
				/* 2-char esc sequence, we ignore it */
				tty_tstate = TTY_TSTATE_ANY;
				return;
			} else {
				switch (c) {
				case 'O':
				case 'N':
					/*
					 * SS2 and SS3 are supposed to
					 * have no modifiers, and to
					 * be terminated by the next
					 * char. As in 2015 certain
					 * terminals use modifiers, we
					 * treat these as CSI
					 */
				case '[':
					tty_tstate = TTY_TSTATE_CSI;
					tty_escbuf[0] = 0x1b;
					tty_escbuf[1] = c;
					tty_nescpar = 0;
					break;
				default:
					tty_tstate = TTY_TSTATE_ERROR;
				}
			}
			return;
		case TTY_TSTATE_INT:
			if (c >= 0x30 && c <= 0x7e)
				tty_tstate = TTY_TSTATE_ANY;
			return;
		case TTY_TSTATE_CSI:
			if (c >= '0' && c <= '9') {
				tty_tstate = TTY_TSTATE_CSI_PAR;
				tty_escval = 0;
				continue;
			} else if (c >= 0x40 && c <= 0x7e) {
				tty_escbuf[2] = c;
				tty_tstate = TTY_TSTATE_ANY;
				tty_onesc();
			} else {
				tty_tstate = TTY_TSTATE_ERROR;
				continue;
			}
			return;
		case TTY_TSTATE_CSI_PAR:
			if (c >= '0' && c <= '9') {
				tty_escval = tty_escval * 10 + c - '0';
				if (tty_escval > 255) {
					tty_tstate = TTY_TSTATE_ERROR;
					continue;
				}
				return;
			} else {
				if (tty_nescpar == TTY_ESC_NPAR) {
					tty_tstate = TTY_TSTATE_ERROR;
					continue;
				}
				tty_escpar[tty_nescpar++] = tty_escval;
				tty_tstate = TTY_TSTATE_CSI;
				if (c == ';')
					return;
				continue;
			}
			return;
		case TTY_TSTATE_ERROR:
			if (c >= 0x40 && c <= 0x7e)
				tty_tstate = TTY_TSTATE_ANY;
			return;
		}
	}
}

void
tty_winch(void)
{
	struct winsize ws;

	if (!tty_initialized)
		return;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) {
		tty_twidth = ws.ws_col;
		if (tty_twidth <= 0)
			tty_twidth = 80;
	} else {
		log_perror("TIOCGWINSZ");
		tty_twidth = 80;
	}
	tty_ops->resize(tty_arg, tty_twidth);
}

void
tty_int(void)
{
	tty_tclear();
	tty_ops->draw(tty_arg);
}

void
tty_reset(void)
{
	struct termios tio;

	if (!tty_initialized)
		return;
	tio = tty_tattr;
	tio.c_lflag &= ~(ICANON | IXON | IXOFF /* | ISIG*/);
	tio.c_lflag &= ~ECHO;
	tio.c_cc[VMIN] = 1;
	tio.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tio) < 0)
		log_perror("tcsetattr");
	tty_winch();
}

void
tty_tflush(void)
{
	ssize_t n;

	if (tty_oused > 0) {
		n = write(STDOUT_FILENO, tty_obuf, tty_oused);
		if (n < 0) {
			log_perror("stdout");
			return;
		}
		tty_oused = 0;
	}
}

void
tty_toutput(char *buf, int len)
{
	int n;

	while (len > 0) {
		n = sizeof(tty_obuf) - tty_oused;
		if (n == 0) {
			tty_tflush();
			continue;
		}
		if (n > len)
			n = len;
		memcpy(tty_obuf + tty_oused, buf, n);
		tty_oused += n;
		buf += n;
		len -= n;
	}
}


void
tty_tesc1(int cmd, int par0)
{
	char buf[32];
	int len;

	len = snprintf(buf, sizeof(buf), "\x1b[%u%c", par0, cmd);
	tty_toutput(buf, len);
}

void
tty_tsetcurs(int x)
{
	if (x < tty_tcursx) {
		tty_toutput("\r", 1);
		tty_tcursx = 0;
	}
	if (x > tty_tcursx)
		tty_tesc1('C', x - tty_tcursx);
	tty_tcursx = x;
}

void
tty_tputs(int x, int w, char *buf, int len)
{
	tty_tsetcurs(x);
	if (len > 0) {
		if (len > w)
			len = w;
		tty_toutput(buf, len);
		tty_tcursx += len;
	}
	if (len < w)
		tty_tesc1('X', w - len);
}

void
tty_tendl(void)
{
	tty_toutput("\n\r", 2);
	tty_tcursx = 0;
}

void
tty_tclear(void)
{
	tty_toutput("\r\x1b[K", 4);
	tty_tcursx = 0;
}

int
tty_pollfd(struct pollfd *pfds)
{
	pfds->fd = STDIN_FILENO;
	pfds->events = POLLIN;
	pfds->revents = 0;
	return 1;
}

int
tty_revents(struct pollfd *pfds)
{
	char buf[1024];
	ssize_t i, n;

	if (pfds[0].revents & POLLIN) {
		n = read(STDIN_FILENO, buf, sizeof(buf));
		if (n < 0) {
			log_perror("stdin");
			tty_ops->onkey(tty_arg, 0);
			return POLLHUP;
		}
		if (n == 0) {
			/* XXX use tty_ops->onkey() */
			tty_ops->onkey(tty_arg, 0);
			return POLLHUP;
		}
		for (i = 0; i < n; i++)
			tty_oninput(buf[i]);
		tty_tflush();
	}
	return 0;
}

void
tty_write(void *buf, size_t len)
{
	if (!tty_initialized) {
		write(STDERR_FILENO, buf, len);
		return;
	}
	tty_tclear();
	tty_tflush();
	write(STDOUT_FILENO, buf, len);
	tty_ops->draw(tty_arg);
}
