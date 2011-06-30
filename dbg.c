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
 * dbg_xxx() routines are used to quickly store traces into a trace buffer.
 * This allows traces to be collected during time sensitive operations without
 * disturbing them. The buffer can be flushed on standard error later, when
 * slow syscalls are no longer disruptive, e.g. at the end of the poll() loop.
 *
 * prof_xxx() routines record (small) integers and provide simple statistical
 * properties like: minimum, maximum, average and variance.
 *
 * mem_xxx() routines are simple wrappers around malloc() overwriting memory
 * blocks with random data upon allocation and release.
 */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "dbg.h"

/*
 * size of the buffer where traces are stored
 */
#define DBG_BUFSZ	8192

/*
 * store a character in the trace buffer
 */
#define DBG_PUTC(c) do {			\
	if (dbg_used < DBG_BUFSZ)		\
		dbg_buf[dbg_used++] = (c);	\
} while (0)

char dbg_buf[DBG_BUFSZ];	/* buffer where traces are stored */
unsigned dbg_used = 0;		/* bytes used in the buffer */
unsigned dbg_sync = 1;		/* if true, flush after each '\n' */

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
	char *p = msg;
	int c;

	while ((c = *p++) != '\0') {
		DBG_PUTC(c);
		if (dbg_sync && c == '\n')
			dbg_flush();
	}
}

/*
 * store a hex in the debug buffer
 */
void
dbg_putx(unsigned long num)
{
	char dig[sizeof(num) * 2], *p = dig, c;
	unsigned ndig;

	if (num != 0) {
		for (ndig = 0; num != 0; ndig++) {
			*p++ = num & 0xf;
			num >>= 4;
		}
		for (; ndig != 0; ndig--) {
			c = *(--p);
			c += (c < 10) ? '0' : 'a' - 10;
			DBG_PUTC(c);
		}
	} else 
		DBG_PUTC('0');
}

/*
 * store a decimal in the debug buffer
 */
void
dbg_putu(unsigned long num)
{
	char dig[sizeof(num) * 3], *p = dig;
	unsigned ndig;

	if (num != 0) {
		for (ndig = 0; num != 0; ndig++) {
			*p++ = num % 10;
			num /= 10;
		}
		for (; ndig != 0; ndig--)
			DBG_PUTC(*(--p) + '0');
	} else
		DBG_PUTC('0');
}

/*
 * store a signed integer in the trace buffer
 */
void
dbg_puti(long num)
{
	if (num < 0) {
		DBG_PUTC('-');
		num = -num;
	}
	dbg_putu(num);
}

/*
 * store a percent in the debug buffer
 */
void
dbg_putpct(unsigned long n)
{
	dbg_putu(n / 100);
	DBG_PUTC('.');
	n %= 100;
	DBG_PUTC('0' + n / 10);
	DBG_PUTC('0' + n % 10);
}

/*
 * abort program execution after a fatal error, we should
 * put code here to backup user data
 */
void
dbg_panic(void)
{
	dbg_flush();
	abort();
}

/*
 * header of a memory block
 */
struct mem_hdr {
	char *owner;		/* who allocaed the block ? */
	unsigned words;		/* data chunk size expressed in sizeof(int) */
#define MAGIC_FREE 0xa55f9811	/* a (hopefully) ``rare'' number */
	unsigned magic;		/* random number, but not MAGIC_FREE */
};

unsigned mem_nalloc = 0, mem_nfree = 0, mem_debug = 0;

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
mem_alloc(unsigned bytes, char *owner)
{
	unsigned words, i, *p;
	struct mem_hdr *hdr;

	if (bytes == 0) {
		dbg_puts("mem_alloc: nbytes = 0\n");
		dbg_panic();
	}

	/*
	 * calculates the number of ints corresponding to ``bytes''
	 */
	words = (bytes + sizeof(int) - 1) / sizeof(int);

	/*
	 * allocate the header, the data chunk and the trailer
	 */
	hdr = malloc(sizeof(struct mem_hdr) + (words + 1) * sizeof(int));
	if (hdr == NULL) {
		dbg_puts("mem_alloc: failed to allocate ");
		dbg_putx(words);
		dbg_puts(" words\n");
		dbg_panic();
	}

	/*
	 * find a random magic, but not MAGIC_FREE
	 */
	do {
		hdr->magic = mem_rnd();
	} while (hdr->magic == MAGIC_FREE);

	/*
	 * randomize data chunk
	 */
	p = (unsigned *)(hdr + 1);
	for (i = words; i > 0; i--)
		*p++ = mem_rnd();

	/*
	 * trailer is equal to the magic
	 */
	*p = hdr->magic;

	hdr->owner = owner;
	hdr->words = words;
	mem_nalloc++;
	return hdr + 1;
}

/*
 * free a memory block. Also check that the header and the trailer
 * werent changed and randomise the block, so that the block is not
 * usable once freed
 */
void
mem_free(void *mem)
{
	struct mem_hdr *hdr;
	unsigned i, *p;

	hdr = (struct mem_hdr *)mem - 1;
	p = (unsigned *)mem;

	if (hdr->magic == MAGIC_FREE) {
		dbg_puts("mem_free: block seems already freed\n");
		dbg_panic();
	}
	if (hdr->magic != p[hdr->words]) {
		dbg_puts("mem_free: block corrupted\n");
		dbg_panic();
	}

	/*
	 * randomize block, so it's not usable
	 */
	for (i = hdr->words; i > 0; i--)
		*p++ = mem_rnd();

	hdr->magic = MAGIC_FREE;
	mem_nfree++;
	free(hdr);
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

		delta = prof_sqrt(p->sumsqr / p->n - mean * mean);
		dbg_puts(", delta=");
		dbg_putpct(delta);
	}
	dbg_puts("\n");
}
