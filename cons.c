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
	if (cons_isatty && tic == 0) {
		snprintf(buf, sizeof(buf), "[%04u:%02u]> ", measure, beat);
		el_setprompt(buf);
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
