/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
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

#ifndef MIDISH_TEXTIO_H
#define MIDISH_TEXTIO_H

#define CHAR_EOF (~0)

struct textin;
struct textout;

struct textin *textin_new(char *);
void textin_delete(struct textin *);
unsigned textin_getchar(struct textin *, int *);
void textin_getpos(struct textin *, unsigned *, unsigned *);

struct textout *textout_new(char *);
void textout_delete(struct textout *);
void textout_indent(struct textout *);
void textout_shiftleft(struct textout *);
void textout_shiftright(struct textout *);
void textout_putstr(struct textout *, char *);
void textout_putlong(struct textout *, unsigned long);
void textout_putbyte(struct textout *, unsigned);

/* ------------------------------------------------- stdin/stdout --- */

extern struct textout *tout;
extern struct textin *tin;

void textio_init(void);
void textio_done(void);

#endif /* MIDISH_TEXTIO_H */
