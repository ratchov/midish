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

#ifndef MIDISH_TEXTIO_H
#define MIDISH_TEXTIO_H

#define CHAR_EOF (~0)

struct textin;
struct textout;

struct textin *textin_new(char *filename);
void textin_delete(struct textin *o);
unsigned textin_getchar(struct textin *o, int *c);
void textin_getpos(struct textin *o, unsigned *line, unsigned *col);
void textin_setprompt(struct textin *o, char *prompt);

struct textout *textout_new(char *filename);
void textout_delete(struct textout *o);
void textout_indent(struct textout *o);
void textout_shiftleft(struct textout *o);
void textout_shiftright(struct textout *o);
void textout_putstr(struct textout *o, char *str);
void textout_putlong(struct textout *o, unsigned long val);
void textout_putbyte(struct textout *o, unsigned val);

/* ------------------------------------------------- stdin/stdout --- */

extern struct textout *tout;
extern struct textin *tin;

void textio_init(void);
void textio_done(void);

#endif /* MIDISH_TEXTIO_H */
