/*
 * Copyright (c) 2003-2007 Alexandre Ratchov <alex@caoua.org>
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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "dbg.h"

#define MAGIC_FREE	0xa55f9811
#define DBG_BUFSZ	4096

char dbg_buf[DBG_BUFSZ];
unsigned dbg_used = 0, dbg_sync = 1;
unsigned mem_nalloc = 0, mem_nfree = 0, mem_debug = 0;

/*
 * following routines are used to output debug info; messages are
 * stored into a buffer rather than being printed on stderr to avoid
 * disturbing time sensitive operations by TTY output.
 */

/*
 * write debug info buffer on stderr
 */
void
dbg_flush(void)
{
	if (dbg_used ==  0)
		return;
	write(STDERR_FILENO, dbg_buf, dbg_used);
	dbg_used = 0;
}

/*
 * store a string in the debug buffer
 */
void
dbg_puts(char *msg)
{
	char *sp, *dp;
	int c;

	sp = msg;
	dp = dbg_buf + dbg_used;
	while ((c = *sp++) != '\0') {
		if (dbg_used < DBG_BUFSZ) {
			*dp++ = c;
			dbg_used++;
		}
		if (dbg_sync && c == '\n')
			dbg_flush();
	}
}

/*
 * store a hex in the debug buffer
 */
void
dbg_putx(unsigned long n)
{
	dbg_used += snprintf(dbg_buf + dbg_used,
	    DBG_BUFSZ - dbg_used, "%lx", n);
	if (dbg_used > DBG_BUFSZ)
		dbg_used = DBG_BUFSZ;
}

/*
 * store a decimal in the debug buffer
 */
void
dbg_putu(unsigned long n)
{
	dbg_used += snprintf(dbg_buf + dbg_used,
	    DBG_BUFSZ - dbg_used, "%lu", n);
	if (dbg_used > DBG_BUFSZ)
		dbg_used = DBG_BUFSZ;
}

/*
 * store a percent in the debug buffer
 */
void
dbg_putpct(unsigned long n)
{
	dbg_used += snprintf(dbg_buf + dbg_used,
	    DBG_BUFSZ - dbg_used, "%lu.%02lu", n / 100, n % 100);
	if (dbg_used > DBG_BUFSZ)
		dbg_used = DBG_BUFSZ;
}

/*
 * abort the execution of the program after a fatal error, we should
 * put code here to backup user data
 */
void
dbg_panic(void)
{
	dbg_flush();
	abort();
}

/*
 * return a random number, will be used to randomize memory bocks
 */
unsigned
mem_rnd(void)
{
	static unsigned seed = 1989123;
	seed = (seed * 1664525) + 1013904223;
	return seed;
}

/*
 * allocate 'n' bytes of memory (with n > 0). This functions never
 * fails (and never returns NULL), if there isn't enough memory then
 * we abord the program.  The memory block is randomized to break code
 * that doesn't initialize the block.  We also add a footer and a
 * trailer to detect writes outside the block boundaries.
 */
void *
mem_alloc(unsigned n)
{
	unsigned i, *buf;

	if (n == 0) {
		dbg_puts("mem_alloc: nbytes = 0\n");
		dbg_panic();
	}
	n = 3 +	(n + sizeof(unsigned) - 1) / sizeof(unsigned);
	buf = (unsigned *)malloc(n * sizeof(unsigned));
	if (buf == NULL) {
		dbg_puts("mem_alloc: failed to allocate ");
		dbg_putx(n);
		dbg_puts(" words\n");
		dbg_panic();
	}
	for (i = 0; i < n; i++)
		buf[i] = mem_rnd();
	while (buf[0] == MAGIC_FREE)
		buf[0] = mem_rnd();	/* a random number */
	buf[1] = n;			/* size of the bloc */
	buf[n - 1] = buf[0];

	mem_nalloc++;
	return buf + 2;
}

/*
 * free a memory block. Also check that the header and the trailer
 * werent changed and randomise the block, so that the block is not
 * usable once freed
 */
void
mem_free(void *mem)
{
	unsigned *buf, n;
	unsigned i;

	buf = (unsigned *)mem - 2;
	n = buf[1];

	if (buf[0] == MAGIC_FREE) {
		dbg_puts("mem_free: block seems already freed\n");
		dbg_panic();
	}
	if (buf[0] != buf[n - 1]) {
		dbg_puts("mem_free: block corrupted\n");
		dbg_panic();
	}
	for (i = 2; i < n; i++)
		buf[i] = mem_rnd();

	buf[0] = MAGIC_FREE;
	mem_nfree++;
	free(buf);
}

void
mem_stats(void)
{
	if (mem_debug) {
		dbg_puts("mem_stats: used=");
		dbg_putu(mem_nalloc - mem_nfree);
		dbg_puts(", alloc=");
		dbg_putu(mem_nalloc);
		dbg_puts("\n");
	}
}

unsigned
prof_sqrt(unsigned op)
{
	unsigned t, res = 0;
	unsigned one = 1 << (8 * sizeof(unsigned) / 2);
 
	while (one > op)
		one >>= 2;

	while (one != 0) {
		t = res + one;
		if (op >= t) {
			op -= t;
 			res += one << 1;
		}
		res >>= 1;
		one >>= 2;
	}
        return res;
}

void
prof_reset(struct prof *p, char *name)
{
	p->n = 0;
	p->min = ~0U;
	p->max = 0;
	p->sum = 0;
	p->sumsqr = 0;
	p->name = name;
	p->err = 0;
}

void
prof_val(struct prof *p, unsigned val)
{
#define MAXVAL ((1 << sizeof(unsigned) * 4) - 1) 
	unsigned sumsqr;

	if (p->err) {
		p->err++;
		return;
	}
	if (val > MAXVAL) {
		dbg_puts("prof_val: ");
		dbg_putu(val);
		dbg_puts(": too large\n");
		p->err++;
		return;
	}
	sumsqr = p->sumsqr + val * val;
	if (sumsqr < p->sumsqr) {
		dbg_puts("prof_val: overflow\n");
		p->err++;
		return;
	}
	if (p->max < val)
		p->max = val;
	if (p->min > val)
		p->min = val;
	p->sumsqr = sumsqr;
	p->sum += val;
	p->n++;
#undef MAXVAL
}

void
prof_dbg(struct prof *p)
{
	unsigned mean, delta;

	dbg_puts(p->name);
	dbg_puts(": err=");
	dbg_putu(p->err);
	dbg_puts(", n=");
	dbg_putu(p->n);
	if (p->n != 0) {
		dbg_puts(", min=");
		dbg_putpct(p->min);
		
		dbg_puts(", max=");
		dbg_putpct(p->max);
		
		mean = p->sum / p->n;
		dbg_puts(", mean=");
		dbg_putpct(mean);

		delta = prof_sqrt((p->sumsqr - p->sum * p->sum / p->n) / p->n);
		dbg_puts(", delta=");
		dbg_putpct(delta);
	}
	dbg_puts("\n");
}
