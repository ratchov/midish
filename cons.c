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

#include "utils.h"
#include "textio.h"
#include "cons.h"
#include "tty.h"
#include "user.h"

/*
 * print song position
 */
void
cons_putpos(unsigned measure, unsigned beat, unsigned tic)
{
	char buf[32];

	if (user_flag_verb) {
		fprintf(stdout, "+pos %u %u %u\n", measure, beat, tic);
		fflush(stdout);
	}
	if (tic == 0) {
		snprintf(buf, sizeof(buf), "[%04u:%02u]> ", measure, beat);
		//log_puts(buf);
		//log_puts("\n");
		tty_setprompt(buf);
	}
}

/*
 * print song position
 */
void
cons_puttag(char *tag)
{
	if (user_flag_verb) {
		fprintf(stdout, "+%s\n", tag);
		fflush(stdout);
	}
}

/*
 * print "+ready"
 */
void
cons_ready(void)
{
	if (user_flag_verb) {
		fprintf(stdout, "+ready\n");
		fflush(stdout);
	}
}

/*
 * follows routines that report user non-fatal errors please use them
 * instead of log_xxx (the latter are only for debugging)
 */

void
cons_err(char *mesg)
{
	log_puts(mesg);
	log_puts("\n");
}

void
cons_errs(char *s, char *mesg)
{
	log_puts(s);
	log_puts(": ");
	log_puts(mesg);
	log_puts("\n");
}

void
cons_erru(unsigned long u, char *mesg)
{
	log_putu(u);
	log_puts(": ");
	log_puts(mesg);
	log_puts("\n");
}

void
cons_errss(char *s0, char *s1, char *mesg)
{
	log_puts(s0);
	log_puts(": ");
	log_puts(s1);
	log_puts(": ");
	log_puts(mesg);
	log_puts("\n");
}

void
cons_errsu(char *s, unsigned long u, char *mesg)
{
	log_puts(s);
	log_puts(": ");
	log_putu(u);
	log_puts(": ");
	log_puts(mesg);
	log_puts("\n");
}

void
cons_erruu(unsigned long u0, unsigned long u1, char *mesg)
{
	log_putu(u0);
	log_puts(": ");
	log_putu(u1);
	log_puts(": ");
	log_puts(mesg);
	log_puts("\n");
}
