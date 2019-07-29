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
#ifndef MIDISH_PARSE_H
#define MIDISH_PARSE_H

enum TOK_ID {
	TOK_EOF = 1, TOK_ERR,
	/* data */
	TOK_IDENT, TOK_NUM, TOK_STRING,
	/* operators */
	TOK_ASSIGN,
	TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PCT,
	TOK_LSHIFT, TOK_RSHIFT, TOK_BITAND, TOK_BITOR, TOK_BITXOR, TOK_TILDE,
	TOK_EQ, TOK_NEQ, TOK_GE, TOK_GT, TOK_LE, TOK_LT,
	TOK_EXCLAM, TOK_AND, TOK_OR,
	TOK_LPAR, TOK_RPAR, TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
	TOK_COMMA, TOK_DOT, TOK_SEMICOLON, TOK_COLON, TOK_RANGE, TOK_ELLIPSIS,
	TOK_AT, TOK_DOLLAR, TOK_ENDLINE,
	/* keywords */
	TOK_IF, TOK_ELSE, TOK_WHILE, TOK_DO, TOK_FOR, TOK_IN,
	TOK_PROC, TOK_LET, TOK_RETURN, TOK_EXIT, TOK_NIL
};

#define IDENT_MAXSZ	32
#define STRING_MAXSZ	1024

#define IS_SPACE(c)	((c) == ' ' || (c) == '\r' || (c) == '\t')
#define IS_PRINTABLE(c)	((c) >= ' ' && (c) != 0x7f)
#define IS_DIGIT(c)	((c) >= '0' && (c) <= '9')
#define IS_ALPHA(c)	(((c) >= 'A' && (c) <= 'Z') || \
			 ((c) >= 'a' && (c) <= 'z'))
#define IS_IDFIRST(c)	(IS_ALPHA(c) || (c) == '_')
#define IS_IDNEXT(c)	(IS_IDFIRST(c) || IS_DIGIT(c))
#define IS_QUOTE(c)	((c) == '"')


struct node;
struct exec;

struct pst {
	unsigned pstate;	/* backup of pstate */
	struct node **pnode;	/* backup of node */
};

struct parse {
	/*
	 * lexical analyser
	 */
	unsigned lstate;
	unsigned base;
	unsigned opindex;
	unsigned used;
	void (*tokcb)(void *, unsigned, unsigned long);
	void *tokarg;
	char buf[STRING_MAXSZ];
	unsigned line, col;
	char *filename;

	/*
	 * parser
	 */
#define PARSE_STACKLEN	64
	struct pst stack[PARSE_STACKLEN];
	struct pst *sp;
	struct node *root;		/* root of the tree */
	struct exec *exec;
	void (*cb)(struct exec *, struct node *);
};

void lex_init(struct parse *, char *,
    void (*)(void *, unsigned, unsigned long), void *);
void lex_done(struct parse *);
void lex_handle(struct parse *, int);
void lex_toklog(unsigned, unsigned long);

void parse_init(struct parse *,
	struct exec *, void (*)(struct exec *, struct node *));
void parse_done(struct parse *);
void parse_cb(void *, unsigned, unsigned long);

#endif /* !defined(MIDISH_PARSE_H) */
