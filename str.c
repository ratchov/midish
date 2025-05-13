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
 * simple string manupulation functions. A NULL pointer is considered
 * as a "non-sense" string, and cannot be used with str_new() and
 * str_delete(). Non-sense strings cannot be compared and have no
 * length
 */

#include <string.h>
#include "utils.h"
#include "str.h"

/*
 * allocate a new string and copy the string from the given argument
 * into the allocated buffer the argument cannot be NULL.
 */
char *
str_new(char *val)
{
	unsigned cnt;
	char *s, *buf;

	if (val == NULL) {
		log_puts("str_new: NULL pointer argument\n");
		panic();
	}
	cnt = str_len(val) + 1;
	buf = xmalloc(cnt, "str");
	for (s = buf; cnt > 0; cnt--) {
		*s++ = *val++;
	}
	return buf;
}

/*
 * free a string allocated with str_new() can't be used on NULL
 * pointer
 */
void
str_delete(char *s)
{
	if (s == NULL) {
		log_puts("str_delete: NULL pointer argument\n");
		panic();
	}
	xfree(s);
}

/*
 * print the string to stderr; can be safely used even if the string
 * is NULL
 */
void
str_log(char *s)
{
	if (s == NULL) {
		log_puts("<nullstring>");
	} else {
		log_puts(s);
	}
}

/*
 * return 1 if the two strings are identical and 0 otherwise. Two
 * zero-length strings are considered identical.
 */
unsigned
str_eq(char *s1, char *s2)
{

	if (s1 == NULL || s2 == NULL) {
		log_puts("std_id: NULL pointer argument\n");
		panic();
	}
	for (;;) {
		if (*s1 == '\0' && *s2 == '\0') {
			return 1;
		} else if (*s1 == '\0' || *s2 == '\0' || *s1 != *s2) {
			return 0;
		}
		s1++;
		s2++;
	}
}

/*
 * return the length of the given string
 */
unsigned
str_len(char *s)
{
	unsigned n;
	for (n = 0; *s; s++) {
		n++;
	}
	return n;
}

/*
 * concatenate two strings.
 */
char *
str_cat(char *s1, char *s2)
{
	size_t n1, n2;
	char *buf;

	n1 = str_len(s1);
	n2 = str_len(s2);

	buf = xmalloc(n1 + n2 + 1, "str");

	memcpy(buf, s1, n1);
	memcpy(buf + n1, s2, n2);

	buf[n1 + n2] = 0;
	return buf;
}
