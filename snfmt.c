/*
 * Copyright (c) 2023 Alexandre Ratchov <alex@caoua.org>
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
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "snfmt.h"

/*
 * Max number of args any function may take
 */
#define SNFMT_NARG		8

/*
 * Max size of "{foobar:%x,%y,%z}" string
 */
#define SNFMT_NAMEMAX		64

/*
 * Max size of the format string of a single snprintf() argument
 */
#define SNFMT_FMTMAX		32

/*
 * We want the real function to be defined
 */
#ifdef snfmt
#undef snfmt
#endif

struct snfmt_ctx {
	const char *fmt;
	va_list ap;
};

/*
 * Skip %'s width or precision: * or unsigned decimal
 */
static void
snfmt_scanparam(struct snfmt_ctx *ctx)
{
	if (*ctx->fmt == '*') {
		va_arg(ctx->ap, int);
		ctx->fmt++;
	} else {
		while (*ctx->fmt >= '0' && *ctx->fmt <= '9')
			ctx->fmt++;
	}
}

/*
 * Parse a %-based conversion specifier and convert its va_list argument
 * to union snfmt_arg. Return the number of chars produced in 'name'.
 */
static int
snfmt_scanpct(struct snfmt_ctx *ctx, char *name, union snfmt_arg *arg)
{
	size_t size;
	int c;

	/* skip leading '%' */
	ctx->fmt++;

	/*
	 * skip flags, width and precision
	 */
	while (1) {
		c = *ctx->fmt;
		if (c != ' ' && c != '#' && c != '+' && c != '-' && c != '0')
			break;
		ctx->fmt++;
	}

	snfmt_scanparam(ctx);
	if (*ctx->fmt == '.') {
		ctx->fmt++;
		snfmt_scanparam(ctx);
	}

	/*
	 * parse optional size specifier (l, ll, z, ...)
	 */
	switch ((c = *ctx->fmt++)) {
	case 'L':
		c = *ctx->fmt++;
		size = sizeof(long double);
		break;
	case 'l':
		c = *ctx->fmt++;
		if (c == 'l') {
			c = *ctx->fmt++;
			size = sizeof(long long);
		} else
			size = sizeof(long);
		break;
	case 'j':
		c = *ctx->fmt++;
		size = sizeof(intmax_t);
		break;
	case 't':
		c = *ctx->fmt++;
		size = sizeof(ptrdiff_t);
		break;
	case 'z':
		c = *ctx->fmt++;
		size = sizeof(size_t);
		break;
	case 'h':
		c = *ctx->fmt++;
		if (c == 'h')
			c = *ctx->fmt++;
		/* FALLTHROUGH */
	default:
		size = sizeof(int);
	}

	/*
	 * convert the argument to union snfmt_arg
	 */
	switch (c) {
	case 'c':
		arg->i = va_arg(ctx->ap, int);
		break;
	case 'd':
	case 'i':
	case 'u':
	case 'x':
	case 'X':
	case 'o':
		if (size == sizeof(int))
			arg->i = va_arg(ctx->ap, int);
		else if (size == sizeof(long))
			arg->i = va_arg(ctx->ap, long);
		else
			arg->i = va_arg(ctx->ap, long long);
		break;
	case 'a':
	case 'A':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
		if (size == sizeof(double))
			arg->f = va_arg(ctx->ap, double);
		else
			arg->f = va_arg(ctx->ap, long double);
		break;
	case 's':
	case 'p':
	case 'n':
		arg->p = va_arg(ctx->ap, void *);
		break;
	default:
		return 0;
	}

	name[0] = '%';
	name[1] = c;
	name[2] = 0;
	return 1;
}

/*
 * Parse conversion function name and arguments in curly braces
 */
static int
snfmt_scanfunc(struct snfmt_ctx *ctx, char *name, union snfmt_arg *arg)
{
	size_t n = 0, index = 0;
	int c;

	/* skip leading '{' */
	ctx->fmt++;

	/*
	 * copy up to the ':' char
	 */
	do {
		if ((c = *ctx->fmt++) == 0 || n == SNFMT_NAMEMAX - 1)
			return 0;
		if (c == '}') {
			name[n] = 0;
			return 1;
		}
		name[n++] = c;
	} while (c != ':');

	/*
	 * copy %<conv_char>[,%<conv_char>]...
	 */
	while (1) {
		if (n >= SNFMT_NAMEMAX - 3 || index == SNFMT_NARG || *ctx->fmt != '%')
			return 0;
		if (!snfmt_scanpct(ctx, name + n, arg + index))
			return 0;
		n += 2;
		index++;
		if ((c = *ctx->fmt++) == '}')
			return 1;
		if (c != ',' || n >= SNFMT_NAMEMAX)
			return 0;
		name[n++] = c;
	}
}

/*
 * Format the given string using snprintf(3)-style semantics, allowing
 * {}-based extensions (ex. "{foobar:%p}").
 */
int
snfmt(snfmt_func *func, char *buf, size_t bufsz, const char *fmt, ...)
{
	va_list ap;
	size_t len;

	va_start(ap, fmt);
	len = snfmt_va(func, buf, bufsz, fmt, ap);
	va_end(ap);
	return len;
}

int
snfmt_va(snfmt_func *func, char *buf, size_t bufsz, const char *fmt, va_list ap)
{
	struct snfmt_ctx ctx;
	union snfmt_arg arg[SNFMT_NARG];
	char name[SNFMT_NAMEMAX], ofmt[SNFMT_FMTMAX];
	char *wptr = buf, *end = buf + bufsz;
	size_t avail, ofmtsize;
	int c, ret;

	/* we return an int */
	if (bufsz > INT_MAX)
		bufsz = INT_MAX;

	while ((c = *fmt) != 0) {

		switch (c) {
		case '{':
			break;
		case '%':
			if (fmt[1] != '%')
				break;
			fmt++;
			/* FALLTHROUGH */
		default:
		copy_and_continue:
			if (wptr < end)
				*wptr = c;
			wptr++;
			fmt++;
			continue;
		}

		ctx.fmt = fmt;
		va_copy(ctx.ap, ap);
		avail = wptr < end ? end - wptr : 0;

		switch (c) {
		case '{':
			if (snfmt_scanfunc(&ctx, name, arg)) {
				if ((ret = func(wptr, avail, name, arg)) != -1)
					break;
			}
			va_end(ctx.ap);
			goto copy_and_continue;
		case '%':
			if (!snfmt_scanpct(&ctx, name, arg)) {
			fmt_err:
				wptr += snprintf(wptr, avail, "(err)");
				va_end(ctx.ap);
				return wptr - buf;
			}
			if ((ret = func(wptr, avail, name, arg)) != -1)
				break;

			ofmtsize = ctx.fmt - fmt;
			if (ofmtsize >= sizeof(ofmt))
				goto fmt_err;
			memcpy(ofmt, fmt, ofmtsize);
			ofmt[ofmtsize] = 0;

			ret = vsnprintf(wptr, avail, ofmt, ap);
			if (ret == -1)
				goto fmt_err;
		}

		wptr += ret;
		fmt = ctx.fmt;
		va_copy(ap, ctx.ap);
		va_end(ctx.ap);
	}

	/*
	 * add terminating '\0'
	 */
	if (bufsz > 0)
		*(wptr < end ? wptr : end - 1) = 0;

	return wptr - buf;
}
