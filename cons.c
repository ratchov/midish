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
#ifdef HAVE_READLINE
#include <unistd.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "dbg.h"
#include "textio.h"
#include "cons.h"

#ifdef HAVE_READLINE
unsigned cons_index, cons_interactive;
char *cons_buf;
#endif

void
cons_init(void) {
#ifdef HAVE_READLINE
	cons_index = 0;
	cons_buf = 0; 
	cons_interactive = isatty(STDIN_FILENO) & isatty(STDOUT_FILENO);
#endif
}

void
cons_done(void) {
#ifdef HAVE_READLINE
	if (cons_buf) {
		free(cons_buf);
	}
#endif
}

int
cons_getc(char *prompt) {
	int c;
#ifdef HAVE_READLINE
	if (cons_interactive) {
		if (!cons_buf) {
			cons_buf = readline(prompt);
			if (!cons_buf) {
				fputs("\n", stdout);
				return CHAR_EOF;
			}
			if (cons_buf != '\0') {
				add_history(cons_buf);
			}
			cons_index = 0;
		}
		c = cons_buf[cons_index++];
		if (c == '\0') {
			free(cons_buf);
			cons_buf = 0;
			return '\n';
		}
	} else {
#endif
		fflush(stdout);
		c = fgetc(stdin);
		if (c < 0) {
			if (!feof(stdin)) {
				perror("stdin");
			}
			return CHAR_EOF;
		}
#ifdef HAVE_READLINE
	}
#endif
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




