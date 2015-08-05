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
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

#include "tty.h"
#include "utils.h"

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

#define TTY_MODE_EDIT		0
#define TTY_MODE_SEARCH		1

#define TTY_HIST_BUFSZ		0x1000
#define TTY_HIST_BUFMASK	(TTY_HIST_BUFSZ - 1)

void tty_draw(void);
void tty_onkey(int);
void tty_onresize(int);
void tty_tflush(void);
void tty_toutput(char *, int);
void tty_tsetcurs(int);
void tty_tputs(int, int, char *, int);
void tty_tclear(void);
void tty_tendl(void);

char tty_prompt[TTY_PROMPTMAX];			/* current prompt */
char tty_lbuf[TTY_LINEMAX];			/* line being editted */
int tty_lused = 0;				/* chars used in the buffer */
int tty_lcurs = 0;				/* char pointed by cursor */
int tty_loffs = 0;				/* most left char displayed */
int tty_lpos = 5;				/* on screen text position */
int tty_lwidth = 0;				/* on screen text width */
int tty_lstart = 0;				/* tty_read() position */

int tty_in, tty_out;				/* controlling terminal fd */
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

void (*tty_cb)(void *, char *, size_t), *tty_arg;

char *tty_hist_data;
size_t tty_hist_start, tty_hist_end, tty_hist_ptr;

char tty_sbuf[TTY_LINEMAX];			/* search pattern */
int tty_sused = 0;				/* chars used in search pat */

int tty_mode;

void
tty_hist_add(char *line, size_t size)
{
	size_t used;
	int c;

	/* remove entries until there's enough space */
	while (1) {
		used = tty_hist_end - tty_hist_start;
		if (used + size <= TTY_HIST_BUFSZ - 1)
			break;
		while (1) {
			c = tty_hist_data[tty_hist_start & TTY_HIST_BUFMASK];
			tty_hist_start++;
			if (c == 0)
				break;
		}
	}

	/* store the new entry */
	while (size > 0) {
		tty_hist_data[tty_hist_end & TTY_HIST_BUFMASK] = *line++;
		tty_hist_end++;
		size--;
	}

	/* add separator */
	tty_hist_data[tty_hist_end & TTY_HIST_BUFMASK] = 0;
	tty_hist_end++;
}

int
tty_hist_prev(size_t *ptr)
{
	size_t p = *ptr;

	if (((tty_hist_start - p) & TTY_HIST_BUFMASK) == 0)
		return 0;
	while (1) {
		p--;
		if (((tty_hist_start - p) & TTY_HIST_BUFMASK) == 0 ||
		    tty_hist_data[(p - 1) & TTY_HIST_BUFMASK] == 0)
			break;
	}
	*ptr = p;
	return 1;
}

int
tty_hist_next(size_t *ptr)
{
	size_t p = *ptr;

	if (((tty_hist_end - p) & TTY_HIST_BUFMASK) == 0)
		return 0;
	while (1) {
		p++;
		if (((tty_hist_end - p) & TTY_HIST_BUFMASK) == 0)
			return 0;
		if (tty_hist_data[(p - 1) & TTY_HIST_BUFMASK] == 0)
			break;
	}
	*ptr = p;
	return 1;
}

size_t
tty_hist_copy(size_t p, char *line)
{
	size_t count;
	int c;

	count = 0;
	while (1) {		
		c = tty_hist_data[p & TTY_HIST_BUFMASK];
		if (c == 0)
			break;
		*line++ = c;		
		count++;
		p++;
	}
	return count;
}

int
tty_hist_match(size_t p, char *pat, size_t pat_size)
{
	while (pat_size > 0) {
		if (*pat++ != tty_hist_data[p++ & TTY_HIST_BUFMASK])
			return 0;
		pat_size--;
	}
	return 1;
}

int
tty_hist_search(size_t *ptr, char *pat, size_t pat_size)
{
	size_t p = *ptr;

	for (;;) {
		if (!tty_hist_prev(&p))
			return 0;
		if (tty_hist_match(p, pat, pat_size))
			break;
	}
	*ptr = p;
	return 1;
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
	tty_toutput(buf, len);
	tty_draw();
}

void
tty_eof(void)
{
	tty_cb(tty_arg, NULL, 0);
}

void
tty_enter(void)
{
	tty_lbuf[tty_lused] = 0;
	tty_cb(tty_arg, tty_lbuf, tty_lused);
}

void
tty_ins(int offs, int c)
{
	int i;
	
	for (i = tty_lused; i != offs; i--)
		tty_lbuf[i] = tty_lbuf[i - 1];
	tty_lbuf[tty_lcurs] = c;
	tty_lused++;
}

void
tty_del(int offs)
{
	int i;
	
	tty_lused--;
	for (i = offs; i != tty_lused; i++)
		tty_lbuf[i] = tty_lbuf[i + 1];
}

void
tty_refresh(int start, int end)
{
	int x, w, todo;

	if (tty_lcurs < tty_loffs) {
		/*
		 * need to scroll left
		 */
		tty_loffs = tty_lcurs;
		start = tty_loffs;
		end = tty_loffs + tty_lwidth;
	} else if (tty_lcurs > tty_loffs + tty_lwidth - 1) {
		/*
		 * need to scroll right
		 */
		tty_loffs = tty_lcurs - tty_lwidth + 1;
		start = tty_loffs;
		end = tty_loffs + tty_lwidth;
	} else {
		/*
		 * no need to scroll, clip to our width
		 */
		if (start < tty_loffs)
			start = tty_loffs;
		if (end > tty_loffs + tty_lwidth)
			end = tty_loffs + tty_lwidth;
	}
	if (end > start) {
		x = start - tty_loffs;
		w = end - start;
		if (end > tty_lused)
			todo = tty_lused - start;
		else
			todo = end - start;
		tty_tputs(tty_lpos + x, w, tty_lbuf + start, todo);
	}
	tty_tsetcurs(tty_lpos + tty_lcurs - tty_loffs);
}

void
tty_draw(void)
{
	tty_tclear();
	if (tty_lpos > 0)
		tty_tputs(0, tty_lpos, tty_prompt, tty_lpos);
	tty_refresh(tty_loffs, tty_loffs + tty_lwidth);
	tty_tflush();
}

void
tty_onkey(int key)
{
	int endpos;
	size_t max, p;

	if (key == (TTY_KEY_CTRL | 'C')) {
		tty_mode = TTY_MODE_EDIT;
		tty_tclear();
		tty_eof();
		tty_draw();
		return;
	}
	if (key < 0x100) {
		if (tty_mode == TTY_MODE_SEARCH) {
			if (tty_sused == TTY_LINEMAX - 1)
				return;
			tty_sbuf[tty_sused++] = key;
			max = tty_lused;
			if (tty_hist_match(tty_hist_ptr,
				tty_sbuf, tty_sused) ||
			    tty_hist_search(&tty_hist_ptr,
				tty_sbuf, tty_sused)) {
				tty_lused = tty_hist_copy(tty_hist_ptr,
				    tty_lbuf);
				if (max < tty_lused)
					max = tty_lused;
				tty_lcurs = tty_sused;
			} else {
				tty_hist_ptr = tty_hist_end;
				tty_mode = TTY_MODE_EDIT;
				memcpy(tty_lbuf, tty_sbuf, tty_sused);
				tty_lused = tty_lcurs = tty_sused;;
				if (max < tty_lused)
					max = tty_lused;
			}
			tty_refresh(0, max);
		} else {
			if (tty_lused == TTY_LINEMAX - 1)
				return;
			tty_ins(tty_lcurs, key);
			tty_lcurs++;
			tty_refresh(tty_lcurs - 1, tty_lused);
		}
	} else if (key == TTY_KEY_DEL || key == (TTY_KEY_CTRL | 'D')) {
		tty_mode = TTY_MODE_EDIT;
		if (tty_lcurs == tty_lused)
			return;
		tty_del(tty_lcurs);
		tty_refresh(tty_lcurs, tty_lused + 1);
	} else if (key == TTY_KEY_BS || key == (TTY_KEY_CTRL | 'H')) {
		if (tty_mode == TTY_MODE_SEARCH) {
			max = tty_lused;
			if (tty_sused > 0) {
				tty_sused--;
				tty_lcurs--;
			}
			if (tty_sused == 0) {
				tty_mode = TTY_MODE_EDIT;
				tty_lused = tty_lcurs = 0;
			} else {
				p = tty_hist_end;
				if (tty_hist_search(&p, tty_sbuf, tty_sused) &&
				    tty_hist_ptr != p) {
					tty_hist_ptr = p;
					tty_lused = tty_hist_copy(p, tty_lbuf);
					if (max < tty_lused)
						max = tty_lused;
					tty_lcurs = tty_sused;
				}
			}
			tty_refresh(0, max);
		} else {
			if (tty_lcurs == 0)
				return;
			tty_lcurs--;
				tty_del(tty_lcurs);
			tty_refresh(tty_lcurs, tty_lused + 1);
		}
	} else if (key == TTY_KEY_LEFT || key == (TTY_KEY_CTRL | 'B')) {
		tty_mode = TTY_MODE_EDIT;
		if (tty_lcurs == 0)
			return;
		tty_lcurs--;
		tty_refresh(tty_lcurs, tty_lcurs);
	} else if (key == TTY_KEY_RIGHT || key == (TTY_KEY_CTRL | 'F')) {
		tty_mode = TTY_MODE_EDIT;
		if (tty_lcurs == tty_lused)
			return;
		tty_lcurs++;
		tty_refresh(tty_lcurs, tty_lcurs);
	} else if (key == TTY_KEY_HOME || key == (TTY_KEY_CTRL | 'A')) {
		tty_mode = TTY_MODE_EDIT;
		if (tty_lcurs == 0)
			return;
		tty_lcurs = 0;
		tty_refresh(tty_lcurs, tty_lcurs);
	} else if (key == TTY_KEY_END || key == (TTY_KEY_CTRL | 'E')) {
		tty_mode = TTY_MODE_EDIT;
		if (tty_lcurs == tty_lused)
			return;
		tty_lcurs = tty_lused;
		tty_refresh(tty_lcurs, tty_lcurs);
	} else if (key == (TTY_KEY_CTRL | 'K')) {
		tty_mode = TTY_MODE_EDIT;
		endpos = tty_lused;
		tty_lused = tty_lcurs;
		tty_refresh(tty_lcurs, endpos);
	} else if (key == TTY_KEY_UP || key == (TTY_KEY_CTRL | 'P')) {
		tty_mode = TTY_MODE_EDIT;
		if (tty_hist_prev(&tty_hist_ptr)) {
			max = tty_lused;
			tty_lused = tty_hist_copy(tty_hist_ptr, tty_lbuf);
			if (max < tty_lused)
				max = tty_lused;
			tty_loffs = 0;
			tty_lcurs = tty_lused;
			tty_refresh(0, max);
		}
	} else if (key == TTY_KEY_DOWN || key == (TTY_KEY_CTRL | 'N')) {
		tty_mode = TTY_MODE_EDIT;
		if (tty_hist_next(&tty_hist_ptr)) {
			max = tty_lused;
			tty_lused = tty_hist_copy(tty_hist_ptr, tty_lbuf);
			if (max < tty_lused)
				max = tty_lused;
			tty_loffs = 0;
			tty_lcurs = tty_lused;
			tty_refresh(0, max);
		} else {
			tty_hist_ptr = tty_hist_end;
			max = tty_lused;
			tty_lbuf[0] = 0;
			tty_loffs = tty_lcurs = tty_lused = 0;
			tty_refresh(0, max);
		}
	} else if (key == TTY_KEY_ENTER) {
		tty_tendl();
		if (tty_lused > 0) {
			tty_hist_add(tty_lbuf, tty_lused);
			tty_hist_ptr = tty_hist_end;
		}
		tty_enter();
		tty_lcurs = tty_loffs = tty_lused = 0;
		tty_draw();
		tty_mode = TTY_MODE_EDIT;
	} else if (key == (TTY_KEY_CTRL | 'L')) {
		tty_refresh(tty_loffs, tty_loffs + tty_lwidth);
	} else if (key == (TTY_KEY_CTRL | 'R')) {
		max = tty_lused;
		if (tty_mode != TTY_MODE_SEARCH) {
			tty_mode = TTY_MODE_SEARCH;
			tty_sused = tty_lused;
			memcpy(tty_sbuf, tty_lbuf, tty_lused);
			tty_lcurs = tty_sused;
		}
		if (tty_hist_search(&tty_hist_ptr, tty_sbuf, tty_sused)) {
			tty_lused = tty_hist_copy(tty_hist_ptr,
			    tty_lbuf);
			if (max < tty_lused)
				max = tty_lused;
		} else {
			tty_hist_ptr = tty_hist_end;
			tty_mode = TTY_MODE_EDIT;
			memcpy(tty_lbuf, tty_sbuf, tty_sused);
			tty_lused = tty_lcurs = tty_sused;
		}
		tty_refresh(0, max);
	}
}

void
tty_onresize(int w)
{
	int end;
	
	tty_lpos = strlen(tty_prompt);
	if (tty_lpos >= w)
		tty_lpos = 0;
	tty_lwidth = w - tty_lpos;
	if (w == 0)
		return;
	end = tty_loffs + tty_lwidth - 1;
	if (tty_lcurs > end)
		tty_lcurs = end;
	tty_draw();
}

void
tty_setline(char *str)
{
	size_t len;
	
	len = strlen(str);
	if (len > TTY_LINEMAX - 1)
		len = TTY_LINEMAX - 1;
	memcpy(tty_lbuf, str, len);
	tty_lused = len;
	tty_lcurs = len;
	tty_loffs = len + 1 - tty_lwidth;
	if (tty_loffs < 0)
		tty_loffs = 0;
	tty_draw();
}

void
tty_setprompt(char *str)
{
	size_t len, dif;
	
	len = strlen(str);
	if (len > TTY_PROMPTMAX - 1) {
		dif = len - (TTY_PROMPTMAX - 1);
		len -= dif;
		str += dif;
	}
	memcpy(tty_prompt, str, len);
	tty_prompt[len] = 0;
	tty_onresize(tty_twidth);
}

int
tty_init(void (*cb)(void *, char *, size_t), void *arg)
{	
	tty_hist_data = xmalloc(TTY_HIST_BUFSZ, "tty_hist_data");
	if (tty_hist_data == NULL)
		return 0;
#ifdef DEBUG
	memset(tty_hist_data, '!', TTY_HIST_BUFSZ);
#endif	
	tty_hist_start = tty_hist_end = tty_hist_ptr;
	
	tty_cb = cb;
	tty_arg = arg;
	tty_oused = 0;
	tty_lwidth = tty_lcurs = tty_lused = tty_loffs = tty_lpos = 0;
	tty_prompt[0] = '\0';
	tty_in = STDIN_FILENO;
	tty_out = STDOUT_FILENO;
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
		return 0;
	if (tcgetattr(tty_in, &tty_tattr) < 0) {
		log_perror("tcgetattr");
		return 0;
	}
#ifdef DEBUG
	memset(tty_lbuf, '.', sizeof(tty_lbuf));
#endif	
	tty_initialized = 1;
	tty_reset();
	return 1;
}

void
tty_done(void)
{
	if (!tty_initialized)
		return;
	tty_tclear();
	tty_tflush();
	if (tcsetattr(tty_in, TCSAFLUSH, &tty_tattr) < 0)
		log_perror("tcsetattr");
	tty_initialized = 0;
	xfree(tty_hist_data);
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
	tty_onkey(key);
}

void
tty_oninput(int c)
{
	int key;
	
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
			tty_onkey(key);
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
	if (ioctl(tty_out, TIOCGWINSZ, &ws) != -1) {
		tty_twidth = ws.ws_col;
		if (tty_twidth <= 0)
			tty_twidth = 80;
	} else {
		log_perror("TIOCGWINSZ");
		tty_twidth = 80;
	}
	tty_onresize(tty_twidth);
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
	if (tcsetattr(tty_in, TCSAFLUSH, &tio) < 0)
		log_perror("tcsetattr");
	tty_winch();
}

void
tty_tflush(void)
{
	ssize_t n;
	
	if (tty_oused > 0) {
		n = write(tty_out, tty_obuf, tty_oused);
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

int
tty_pollfd(struct pollfd *pfds)
{
	pfds->fd = tty_in;
	pfds->events = POLLIN;
	pfds->revents = 0;
	return 1;
}

int
tty_revents(struct pollfd *pfds)
{
	char buf[128];
	ssize_t i, n;
	int c;
	
	if (pfds[0].revents & POLLIN) {
		n = read(tty_in, buf, sizeof(buf));
		if (n < 0) {
			log_perror("stdin");
			tty_eof();
			return POLLHUP;
		}
		if (n == 0) {
			tty_eof();
			return POLLHUP;
		}
		if (!tty_initialized) {
			for (i = 0; i < n; i++) {
				c = buf[i];
				if (tty_lused == TTY_LINEMAX) {
					log_puts("input line too long\n");
					tty_eof();
				} else if (c == '\n') {
					tty_enter();
					tty_lused = 0;
				} else
					tty_lbuf[tty_lused++] = c;
			}
		} else {
			for (i = 0; i < n; i++)
				tty_oninput(buf[i]);
			tty_tflush();
		}
	}
	return 0;
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
