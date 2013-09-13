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

#ifndef MIDISH_LEX_H
#define MIDISH_LEX_H

#define IDENT_MAXLEN	30
#define STRING_MAXLEN	1024
#define TOK_MAXLEN	STRING_MAXLEN

enum SYM_ID {
	TOK_EOF = 0, TOK_ASSIGN,
	TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PCT,
	TOK_LSHIFT, TOK_RSHIFT, TOK_BITAND, TOK_BITOR, TOK_BITXOR, TOK_TILDE,
	TOK_EQ, TOK_NEQ, TOK_GE, TOK_GT, TOK_LE, TOK_LT,
	TOK_EXCLAM, TOK_AND, TOK_OR,
	TOK_LPAR, TOK_RPAR, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
	TOK_COMMA, TOK_DOT, TOK_SEMICOLON, TOK_COLON, TOK_RANGE,
	TOK_AT, TOK_DOLLAR, TOK_ENDLINE,
	/* keywords */
	TOK_IF, TOK_ELSE, TOK_WHILE, TOK_DO, TOK_FOR, TOK_IN,
	TOK_PROC, TOK_LET, TOK_RETURN, TOK_EXIT, TOK_NIL,
	/* data */
	TOK_IDENT, TOK_NUM, TOK_STRING
};

struct tokdef {
	unsigned id;			/* token id */
	char *str;			/* corresponding string */
};

struct lex {
	unsigned id;
	char strval[TOK_MAXLEN + 1];
	unsigned long longval;
	struct textin *in;		/* input file */
	int lookchar;			/* used by ungetchar */
	unsigned line, col;		/* for error reporting */
};

unsigned lex_init(struct lex *, char *);
void     lex_done(struct lex *);
unsigned lex_scan(struct lex *);
void	 lex_log(struct lex *);

unsigned lex_getchar(struct lex *, int *);
void	 lex_ungetchar(struct lex *, int);
void	 lex_err(struct lex *, char *);
void	 lex_recover(struct lex *, char *);
unsigned lex_str2long(struct lex *, unsigned);

#endif /* MIDISH_LEX_H */
