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
#include <errno.h>

#include "utils.h"
#include "textio.h"
#include "cons.h"

struct textin
{
	FILE *file;
	unsigned isconsole;
	unsigned line, col;
};

struct textout
{
	FILE *file;
	unsigned indent, isconsole;
};

/* -------------------------------------------------------- input --- */

struct textin *
textin_new(char *filename)
{
	struct textin *o;

	o = xmalloc(sizeof(struct textin), "textin");
	if (filename == NULL) {
		o->isconsole = 1;
		o->file = stdin;
	} else {
		o->isconsole = 0;
		o->file = fopen(filename, "r");
		if (o->file == NULL) {
			cons_errs(filename, "failed to open input file");
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
	if (!o->isconsole) {
		fclose(o->file);
	}
	xfree(o);
}

unsigned
textin_getchar(struct textin *o, int *c)
{
	if (o->isconsole) {
		*c = cons_getc();	/* because it handles ^C */
	} else {
		*c = fgetc(o->file);
	}
	if (*c < 0) {
		*c = CHAR_EOF;
		if (ferror(o->file)) {
			perror("fgetc");
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
			cons_errs(filename, "failed to open output file");
			xfree(o);
			return 0;
		}
		o->isconsole = 0;
	} else {
		o->file = stdout;
		o->isconsole = 1;
	}
	o->indent = 0;
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
textout_indent(struct textout *o)
{
	unsigned i;
	for (i = 0; i < o->indent; i++) {
		fputc('\t', o->file);
	}
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
	fputs(str, o->file);
}

void
textout_putlong(struct textout *o, unsigned long val)
{
	fprintf(o->file, "%lu", val);
}

void
textout_putbyte(struct textout *o, unsigned val)
{
	fprintf(o->file, "0x%02x", val & 0xff);
}

/* ------------------------------------------------------------------ */


struct textout *tout;
struct textin *tin;

void
textio_init(void)
{
	tout = textout_new(NULL);
	tin = textin_new(NULL);
}

void
textio_done(void)
{
	textout_delete(tout);
	tout = 0;
	textin_delete(tin);
	tin = 0;
}
