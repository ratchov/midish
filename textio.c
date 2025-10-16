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

/*
 * textin implemets inputs from text files (or stdin)
 * (open/close, line numbering, etc...). Used by lex
 *
 * textout implements outputs into text files (or stdout)
 * (open/close, indentation...)
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "utils.h"
#include "textio.h"
#include "cons.h"

struct textin
{
	FILE *file;
	unsigned line, col;
};

struct textout
{
	FILE *file;
	unsigned indent, isconsole, col;
};

/* -------------------------------------------------------- input --- */

struct textin *
textin_new(char *filename)
{
	struct textin *o;

	o = xmalloc(sizeof(struct textin), "textin");
	if (filename == NULL) {
		o->file = stdin;
	} else {
		o->file = fopen(filename, "r");
		if (o->file == NULL) {
			logx(1, "%s: failed to open input file", filename);
			xfree(o);
			return 0;
		}
	}
	o->line = o->col = 0;
	return o;
}

void
textin_delete(struct textin *o)
{
	fclose(o->file);
	xfree(o);
}

unsigned
textin_getchar(struct textin *o, int *c)
{
	*c = fgetc(o->file);
	if (*c == EOF) {
		*c = CHAR_EOF;
		if (ferror(o->file)) {
			logx(1, "fgetc: %s", strerror(errno));
		}
		return 1;
	}
	if (*c == '\n') {
		o->col = 0;
		o->line++;
	} else if (*c == '\t') {
		o->col += 8;
	} else {
		o->col++;
	}
	*c = *c & 0xff;
	return 1;
}

void
textin_getpos(struct textin *o, unsigned *line, unsigned *col)
{
	*line = o->line;
	*col = o->col;
}

/* ------------------------------------------------------- output --- */

struct textout *
textout_new(char *filename)
{
	struct textout *o;

	o = xmalloc(sizeof(struct textout), "textout");
	if (filename != NULL) {
		o->file = fopen(filename, "w");
		if (o->file == NULL) {
			logx(1, "%s: failed to open output file", filename);
			xfree(o);
			return 0;
		}
		o->isconsole = 0;
	} else {
		o->file = stdout;
		o->isconsole = 1;
	}
	o->indent = 0;
	o->col = 0;
	return o;
}

void
textout_delete(struct textout *o)
{
	if (!o->isconsole) {
		fclose(o->file);
	}
	xfree(o);
}

void
textout_shiftleft(struct textout *o)
{
	o->indent--;
}

void
textout_shiftright(struct textout *o)
{
	o->indent++;
}

void
textout_putstr(struct textout *o, char *str)
{
	static char buf[1] = {'\t'};
	char *p;
	unsigned int i;

	while (1) {
		if (str[0] == 0)
			break;

		if (o->col == 0) {
			if (o->isconsole)
				el_hide();
			for (i = 0; i < o->indent; i++) {
				fwrite(buf, sizeof(buf), 1, o->file);
				o->col += 8;
			}
		}

		for (p = str; *p; p++) {
			if (*p == '\n') {
				p++;
				o->col = 0;
				break;
			}
			o->col++;
		}

		fwrite(str, p - str, 1, o->file);
		str = p;
	}
}

void
textout_putlong(struct textout *o, long val)
{
	char buf[sizeof(val) * 3 + 1];

	snprintf(buf, sizeof(buf), "%ld", val);
	textout_putstr(o, buf);
}

void
textout_putbyte(struct textout *o, unsigned val)
{
	char buf[sizeof(val) * 2 + 1];

	snprintf(buf, sizeof(buf), "0x%02x", val & 0xff);
	textout_putstr(o, buf);
}

/* ------------------------------------------------------------------ */

struct textout *tout;

void
textio_init(void)
{
	tout = textout_new(NULL);
}

void
textio_done(void)
{
	textout_delete(tout);
	tout = 0;
}
