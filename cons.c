/*
 * Copyright (c) 2003-2005 Alexandre Ratchov
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

#include <stdio.h>
#include <stdlib.h>

#include "dbg.h"
#include "textio.h"
#include "cons.h"

unsigned cons_breakcnt;
unsigned cons_need_prompt=1;

unsigned
cons_break(void) {
	if (cons_breakcnt > 0) {
		cons_breakcnt = 0;
		cons_err("\n--interrupt--\n");
		return 1;
	} else {
		return 0;
	}
}

void
cons_init(void) {
	cons_breakcnt = 0;
	cons_need_prompt = 1;
	cons_mdep_init();
}

void
cons_done(void) {
	cons_mdep_done();
}

int
cons_getc(char *prompt) {
	int c;
	if (cons_need_prompt) {
		fputs("+ready\n", stdout);
		cons_need_prompt = 0;
	}
	fflush(stdout);
	fflush(stderr);
	c = fgetc(stdin);
	if (c == '\n') {
		cons_need_prompt = 1;
	}
	cons_breakcnt = 0;	/* ignore keyboard breaks */
	if (c < 0) {
		if (ferror(stdin)) {
			perror("cons_getc: stdin");
		}
		return CHAR_EOF;
	}
	return c;
}

void
cons_err(char *mesg) {
	fprintf(stderr, "%s\n", mesg);
}

void
cons_errs(char *s, char *mesg) {
	fprintf(stderr, "%s: %s\n", s, mesg);
}

void
cons_erru(unsigned long u, char *mesg) {
	fprintf(stderr, "%lu: %s\n", u, mesg);
}

void
cons_errss(char *s0, char *s1, char *mesg) {
	fprintf(stderr, "%s: %s: %s\n", s0, s1, mesg);
}

void
cons_errsu(char *s, unsigned long u, char *mesg) {
	fprintf(stderr, "%s: %lu: %s\n", s, u, mesg);
}

void
cons_erruu(unsigned long u0, unsigned long u1, char *mesg) {
	fprintf(stderr, "%lu.%lu: %s\n", u0, u1, mesg);
}
