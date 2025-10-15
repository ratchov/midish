/*
 * Copyright (c) 2003-2012 Alexandre Ratchov <alex@caoua.org>
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
 */
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "utils.h"
#include "ev.h"
#include "data.h"
#include "snfmt.h"
#include "state.h"
#include "tty.h"

/*
 * log buffer size
 */
#define LOG_BUFSZ	8192

/*
 * store a character in the log
 */
#define LOG_PUTC(c) do {			\
	if (log_used < LOG_BUFSZ)		\
		log_buf[log_used++] = (c);	\
} while (0)

char log_buf[LOG_BUFSZ];	/* buffer where traces are stored */
unsigned int log_used = 0;	/* bytes used in the buffer */
unsigned int log_sync = 1;	/* if true, flush after each '\n' */

int log_level = 1;

size_t
hexdump_fmt(char *buf, size_t size, unsigned char *blob, size_t blob_size)
{
	char *p = buf, *end = buf + size;
	const char *sep = "";
	size_t i;

	for (i = 0; i < blob_size; i++) {
		p += snprintf(p, p < end ? end - p : 0, "%s%02x", sep, blob[i]);
		sep = " ";
	}

	return p - buf;
}

int
log_fmt(char *buf, size_t size, const char *fmt, union snfmt_arg *args)
{
	if (strcmp(fmt, "ev:%p") == 0)
		return ev_fmt(buf, size, args[0].p);
	if (strcmp(fmt, "evspec:%p") == 0)
		return evspec_fmt(buf, size, args[0].p);
	if (strcmp(fmt, "data:%p") == 0)
		return data_fmt(buf, size, args[0].p);
	if (strcmp(fmt, "hexdump:%p,%u") == 0)
		return hexdump_fmt(buf, size, args[0].p, args[1].u);
	if (strcmp(fmt, "state:%p") == 0)
		return state_fmt(buf, size, args[0].p);
	return -1;
}

/*
 * write the log buffer on stderr
 */
void
log_flush(void)
{
	if (log_used == 0)
		return;
	el_hide();
	write(STDERR_FILENO, log_buf, log_used);
	log_used = 0;
}

/*
 * log a single line to stderr
 */
void
log_do(const char *fmt, ...)
{
	va_list ap;
	int n, save_errno = errno;

	va_start(ap, fmt);
	n = snfmt_va(log_fmt, log_buf + log_used, sizeof(log_buf) - log_used, fmt, ap);
	va_end(ap);

	if (n != -1) {
		log_used += n;

		if (log_used >= sizeof(log_buf))
			log_used = sizeof(log_buf) - 1;
		log_buf[log_used++] = '\n';

		if (log_sync)
			log_flush();
	}
	errno = save_errno;
}

/*
 * abort program execution after a fatal error
 */
void
panic(void)
{
	log_flush();
	(void)kill(getpid(), SIGABRT);
	_exit(1);
}

/*
 * allocate 'size' bytes of memory (with size > 0). This functions never
 * fails (and never returns NULL), if there isn't enough memory then
 * abort the program.
 */
void *
xmalloc(size_t size, char *tag)
{
	void *p;

	p = malloc(size);
	if (p == NULL) {
		logx(1, "failed to allocate %zu bytes", size);
		panic();
	}
	return p;
}

/*
 * free memory allocated with xmalloc()
 */
void
xfree(void *p)
{
#ifdef DEBUG
	if (p == NULL) {
		logx(1, "xfree with NULL arg");
		panic();
	}
#endif
	free(p);
}

/*
 * xmalloc-style strdup(3)
 */
char *
xstrdup(char *s, char *tag)
{
	size_t size;
	void *p;

	size = strlen(s) + 1;
	p = xmalloc(size, tag);
	memcpy(p, s, size);
	return p;
}
