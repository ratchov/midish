/* $Id: textio.c,v 1.10 2006/02/14 12:21:41 alex Exp $ */
/*
 * Copyright (c) 2003-2006 Alexandre Ratchov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * 	- Redistributions of source code must retain the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer.
 *
 * 	- Redistributions in binary form must reproduce the above
 * 	  copyright notice, this list of conditions and the
 * 	  following disclaimer in the documentation and/or other
 * 	  materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * textin_s implemets inputs from text files (or stdin)
 * (open/close, line numbering, etc...). Used by lex_s
 *
 * textout_s implements outputs into text files (or stdout)
 * (open/close, indentation...)
 *
 */

#include <stdio.h>
#include <errno.h>

#include "dbg.h"
#include "textio.h"
#include "cons.h"

struct textin_s {
	FILE *file;
	unsigned isconsole;
	unsigned line, col;
};

struct textout_s {
	FILE *file;
	unsigned indent, isconsole;
};

/* -------------------------------------------------------- input --- */

struct textin_s *
textin_new(char *filename) {
	struct textin_s *o;

	o = (struct textin_s *)mem_alloc(sizeof(struct textin_s));
	if (filename == NULL) {
		o->isconsole = 1;
		o->file = stdin;
	} else {
		o->isconsole = 0;
		o->file = fopen(filename, "r");
		if (o->file == NULL) {
			cons_errs(filename, "failed to open input file");
			mem_free(o);
			return 0;
		}
	}
	o->line = o->col = 0;
	return o;
}

void
textin_delete(struct textin_s *o) {
	if (!o->isconsole) {
		fclose(o->file);
	}
	mem_free(o);
}

unsigned
textin_getchar(struct textin_s *o, int *c) {
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
textin_getpos(struct textin_s *o, unsigned *line, unsigned *col) {
	*line = o->line;
	*col = o->col;
}

/* ------------------------------------------------------- output --- */

struct textout_s *
textout_new(char *filename) {
	struct textout_s *o;
	
	o = (struct textout_s *)mem_alloc(sizeof(struct textout_s));
	if (filename != NULL) {
		o->file = fopen(filename, "w");
		if (o->file == NULL) {
			cons_errs(filename, "filed to open output file");
			mem_free(o);
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
textout_delete(struct textout_s *o) {
	if (!o->isconsole) {
		fclose(o->file);
	}
	mem_free(o);
}

void
textout_indent(struct textout_s *o) {
	unsigned i;
	for (i = 0; i < o->indent; i++) {
		fputc('\t', o->file);
	}
}

void
textout_shiftleft(struct textout_s *o) {
	o->indent--;
}

void
textout_shiftright(struct textout_s *o) {
	o->indent++;
}

void
textout_putstr(struct textout_s *o, char *str) {
	fputs(str, o->file);
}

void
textout_putlong(struct textout_s *o, unsigned long val) {
	fprintf(o->file, "%lu", val);
}

void
textout_putbyte(struct textout_s *o, unsigned val) {
	fprintf(o->file, "0x%02x", val & 0xff);
}

/* ------------------------------------------------------------------ */


struct textout_s *tout;
struct textin_s *tin;

void
textio_init(void) {
	tout = textout_new(NULL);
	tin = textin_new(NULL);
}

void
textio_done(void) {
	textout_delete(tout);
	tout = 0;
	textin_delete(tin);
	tin = 0;
}
