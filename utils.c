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
 * log_xxx() routines are used to quickly store traces into a trace buffer.
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
#include "utils.h"

/*
 * size of the buffer where traces are stored
 */
#define LOG_BUFSZ	8192

/*
 * store a character in the trace buffer
 */
#define LOG_PUTC(c) do {			\
	if (log_used < LOG_BUFSZ)		\
		log_buf[log_used++] = (c);	\
} while (0)

char log_buf[LOG_BUFSZ];	/* buffer where traces are stored */
unsigned log_used = 0;		/* bytes used in the buffer */
unsigned log_sync = 1;		/* if true, flush after each '\n' */

/*
 * write debug info buffer on stderr
 */
void
log_flush(void)
{
	if (log_used ==  0)
		return;
	write(STDERR_FILENO, log_buf, log_used);
	log_used = 0;
}

/*
 * store a string in the debug buffer
 */
void
log_puts(char *msg)
{
	char *p = msg;
	int c;

	while ((c = *p++) != '\0') {
		LOG_PUTC(c);
		if (log_sync && c == '\n')
			log_flush();
	}
}

/*
 * store a hex in the debug buffer
 */
void
log_putx(unsigned long num)
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
			LOG_PUTC(c);
		}
	} else
		LOG_PUTC('0');
}

/*
 * store a decimal in the debug buffer
 */
void
log_putu(unsigned long num)
{
	char dig[sizeof(num) * 3], *p = dig;
	unsigned ndig;

	if (num != 0) {
		for (ndig = 0; num != 0; ndig++) {
			*p++ = num % 10;
			num /= 10;
		}
		for (; ndig != 0; ndig--)
			LOG_PUTC(*(--p) + '0');
	} else
		LOG_PUTC('0');
}

/*
 * store a signed integer in the trace buffer
 */
void
log_puti(long num)
{
	if (num < 0) {
		LOG_PUTC('-');
		num = -num;
	}
	log_putu(num);
}

/*
 * store a percent in the debug buffer
 */
void
log_pct(unsigned long n)
{
	log_putu(n / 100);
	LOG_PUTC('.');
	n %= 100;
	LOG_PUTC('0' + n / 10);
	LOG_PUTC('0' + n % 10);
}

/*
 * abort program execution after a fatal error, we should
 * put code here to backup user data
 */
void
panic(void)
{
	log_flush();
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
memrnd(void)
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
xmalloc(unsigned bytes, char *owner)
{
	unsigned words, i, *p;
	struct mem_hdr *hdr;

	if (bytes == 0) {
		log_puts("xmalloc: nbytes = 0\n");
		panic();
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
		log_puts("xmalloc: failed to allocate ");
		log_putx(words);
		log_puts(" words\n");
		panic();
	}

	/*
	 * find a random magic, but not MAGIC_FREE
	 */
	do {
		hdr->magic = memrnd();
	} while (hdr->magic == MAGIC_FREE);

	/*
	 * randomize data chunk
	 */
	p = (unsigned *)(hdr + 1);
	for (i = words; i > 0; i--)
		*p++ = memrnd();

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
xfree(void *mem)
{
	struct mem_hdr *hdr;
	unsigned i, *p;

	hdr = (struct mem_hdr *)mem - 1;
	p = (unsigned *)mem;

	if (hdr->magic == MAGIC_FREE) {
		log_puts("xfree: block seems already freed\n");
		panic();
	}
	if (hdr->magic != p[hdr->words]) {
		log_puts("xfree: block corrupted\n");
		panic();
	}

	/*
	 * randomize block, so it's not usable
	 */
	for (i = hdr->words; i > 0; i--)
		*p++ = memrnd();

	hdr->magic = MAGIC_FREE;
	mem_nfree++;
	free(hdr);
}

void
mem_stats(void)
{
	if (mem_debug) {
		log_puts("mem_stats: used=");
		log_putu(mem_nalloc - mem_nfree);
		log_puts(", alloc=");
		log_putu(mem_nalloc);
		log_puts("\n");
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
		log_puts("prof_val: ");
		log_putu(val);
		log_puts(": too large\n");
		p->err++;
		return;
	}
	sumsqr = p->sumsqr + val * val;
	if (sumsqr < p->sumsqr) {
		log_puts("prof_val: overflow\n");
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
prof_log(struct prof *p)
{
	unsigned mean, delta;

	log_puts(p->name);
	log_puts(": err=");
	log_putu(p->err);
	log_puts(", n=");
	log_putu(p->n);
	if (p->n != 0) {
		log_puts(", min=");
		log_pct(p->min);

		log_puts(", max=");
		log_pct(p->max);

		mean = p->sum / p->n;
		log_puts(", mean=");
		log_pct(mean);

		delta = prof_sqrt(p->sumsqr / p->n - mean * mean);
		log_puts(", delta=");
		log_pct(delta);
	}
	log_puts("\n");
}
