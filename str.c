/*
 * Copyright (c) 2003-2005 Alexandre Ratchov
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

/*
 * simple string manupulation functions. A NULL pointer
 * is considered as a "non-sense" string, and can be used
 * with str_new() and str_delete(). Non-sense strings
 * cannot be compared and have not length
 */
 
#include "dbg.h"
#include "str.h"

	/*
  	 * allocate a new string and copy the string from
  	 * the given argument into the allocated buffer 
  	 * the argument cannot be NULL.
  	 */

char *
str_new(char *val) {
	unsigned i, len;
	char *s;

	if (val == 0) {
		dbg_puts("str_new: NULL pointer argument\n");
		dbg_panic();
	}
	len = str_len(val) + 1;
	s = (char *)mem_alloc(len);
	for (i = 0; i < len; i++) {
		s[i] = val[i];
	}
	return s;
}

	/*
	 * free a string allocated with str_new()
	 * can't used a NULL pointer
	 */

void
str_delete(char *s) {
	if (s == 0) {
		dbg_puts("str_delete: NULL pointer argument\n");
		dbg_panic();
	}
	mem_free(s);
}

	/*
	 * print the string to stderr; can
	 * be safely used even if the string is NULL
	 */

void
str_dbg(char *s) {
	if (s == 0) {
		dbg_puts("<nullstring>");
	} else {
		dbg_puts(s);
	}
}

	/*
	 * return 1 if the two strings are identical
	 * and 0 otherwise. Two zero-length strings are
	 * identical. 
	 */

unsigned
str_eq(char *s1, char *s2) {	
	if (s1 == 0 || s2 == 0) {
		dbg_puts("std_id: NULL pointer argument\n");
		dbg_panic();
	}
	for (;;) {
		if (*s1 == '\0'  &&  *s2 == '\0') {
			return 1; 
		} else if (*s1 == '\0' || *s2 == '\0' || *s1 != *s2) {
			return 0;
		}
		s1++;
		s2++;
	}	
	return 1;
}

unsigned
str_len(char *s) {
	unsigned n;
	for (n = 0; *s; s++) {
		n++;
	}
	return n;
}

