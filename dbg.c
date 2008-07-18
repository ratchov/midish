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
#include "dbg.h"

#define MAGIC_ALLOC	13942
#define MAGIC_FREE	59811

unsigned mem_nalloc = 0, mem_nfree = 0, mem_debug = 0;

/*
 * following routines are used to output debug info, use them instead
 * of fprintf(stderr, ...) because the will be turned to no-op in
 * stable version
 */

void
dbg_puts(char *msg)
{
	fputs(msg, stderr);
}

void
dbg_putx(unsigned long n)
{
	fprintf(stderr, "%lx", n);
}

void
dbg_putu(unsigned long n)
{
	fprintf(stderr, "%lu", n);
}

/*
 * abort the execution of the program after a fatal error, we should
 * put code here to backup user data
 */
void
dbg_panic(void)
{
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

	n = 4 +	(n + sizeof(unsigned) - 1) / sizeof(unsigned);

	buf = (unsigned *)malloc(n * sizeof(unsigned));

	if (buf == NULL) {
		dbg_puts("mem_alloc: failed to allocate ");
		dbg_putx(n);
		dbg_puts(" words\n");
		dbg_panic();
	}

	for (i = 0; i < n; i++) {
		buf[i] = mem_rnd();
	}

	buf[0] = MAGIC_ALLOC; 	/* state of the block */
	buf[1] = n;		/* size of the bloc */
	buf[2] = mem_rnd();	/* a random number */
	buf[n - 1] = buf[2];

	mem_nalloc++;

	return buf + 3;
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

	buf = (unsigned *)mem - 3;
	n = buf[1];

	if (buf[0] == MAGIC_FREE) {
		dbg_puts("mem_free: block seems already freed\n");
		dbg_panic();
	}

	if (buf[0] != MAGIC_ALLOC) {
		dbg_puts("mem_free: block header corrupt\n");
		dbg_panic();
	}

	if (buf[2] != buf[n - 1]) {
		dbg_puts("mem_free: block corrupt\n");
		dbg_panic();
	}

	for (i = 3; i < n; i++) {
		buf[i] = mem_rnd();
	}
	buf[0] = MAGIC_FREE;

	free(buf);
	mem_nfree++;
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
	unsigned res = 0;
	unsigned one = 1 << (8 * sizeof(unsigned) / 2);
 
	while (one > op)
		one >>= 2;

	while (one != 0) {
		if (op >= res + one) {
			op -= res + one;
 			res += 2 * one;
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
}

void
prof_val(struct prof *p, unsigned val)
{
	if (p->max < val)
		p->max = val;
	if (p->min > val)
		p->min = val;
	p->sum += val;
	p->sumsqr += (val * val);
	p->n++;
}

void
prof_dbg(struct prof *p)
{
	unsigned mean, delta;

	dbg_puts(p->name);
	dbg_puts(": n=");
	dbg_putu(p->n);
	if (p->n != 0) {
		dbg_puts(", min=");
		dbg_putu(p->min / 100);
		dbg_puts(".");
		dbg_putu(p->min % 100);
		
		dbg_puts(", max=");
		dbg_putu(p->max / 100);
		dbg_puts(".");
		dbg_putu(p->max % 100);
		
		mean = p->sum / p->n;
		dbg_puts(", mean=");
		dbg_putu(mean / 100);
		dbg_puts(".");
		dbg_putu(mean % 100);

		delta = prof_sqrt((p->sumsqr - p->sum * p->sum / p->n) / p->n);
		dbg_puts(", delta=");
		dbg_putu(delta / 100);
		dbg_puts(".");
		dbg_putu(delta % 100);
	}
	dbg_puts("\n");
}
