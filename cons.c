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

#include <stdio.h>
#include <stdlib.h>

#include "dbg.h"
#include "textio.h"
#include "cons.h"
#include "user.h"

unsigned cons_ready;

void
cons_init(void)
{
	cons_ready = 1;
	cons_mdep_init();
}

void
cons_done(void)
{
	cons_mdep_done();
}

/*
 * Same as fgetc(stdin), but if midish is started with the verb flag,
 * print "+ready\n" to stdout and flush it. This is useful to
 * front-ends that open midish in a pair of pipes
 */
int
cons_getc(void)
{
	int c;
	if (cons_ready) {
		if (user_flag_verb) {
			fprintf(stdout, "+ready\n");
			fflush(stdout);
		}
		cons_ready = 0;
	}
	c = cons_mdep_getc();
	if (c == '\n') {
		cons_ready = 1;
	}
	return c;
}

/*
 * print song position
 */
void
cons_putpos(unsigned measure, unsigned beat, unsigned tic)
{
	if (user_flag_verb) {
		fprintf(stdout, "+pos %u %u %u\n", measure, beat, tic);
		fflush(stdout);
	}
}

/*
 * follows routines that report user non-fatal errors please use them
 * instead of dbg_xxx (the latter are only for debugging)
 */

void
cons_err(char *mesg)
{
	fprintf(stderr, "%s\n", mesg);
}

void
cons_errs(char *s, char *mesg)
{
	fprintf(stderr, "%s: %s\n", s, mesg);
}

void
cons_erru(unsigned long u, char *mesg)
{
	fprintf(stderr, "%lu: %s\n", u, mesg);
}

void
cons_errss(char *s0, char *s1, char *mesg)
{
	fprintf(stderr, "%s: %s: %s\n", s0, s1, mesg);
}

void
cons_errsu(char *s, unsigned long u, char *mesg)
{
	fprintf(stderr, "%s: %lu: %s\n", s, u, mesg);
}

void
cons_erruu(unsigned long u0, unsigned long u1, char *mesg)
{
	fprintf(stderr, "%lu.%lu: %s\n", u0, u1, mesg);
}
