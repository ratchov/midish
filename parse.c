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
 * A simple LL(1)-like parser for the followingi grammar:
 *
 * endl:	"\n"
 * 		";"
 *
 * cst:		num
 * 		string
 * 		ident
 * 		nil
 * 		"$" ident
 * 		"[" call "]"
 * 		"(" expr ")"
 * 		"{" [ expr expr ...  expr ] "}"
 *
 * unary:	"-" unary
 * 		"~" unary
 * 		"!" unary
 * 		cst
 *
 * muldiv:	unary "*" unary
 * 		unary "/" unary
 * 		unary "%" unary
 *
 * addsub:	muldiv "+" muldiv
 * 		muldiv "-" muldiv
 *
 * shift:	addsub "<<" addsub
 * 		addsub ">>" addsub
 *
 * compare:	shift "<" shift
 * 		shift ">" shift
 * 		shift "<=" shift
 * 		shift ">=" shift
 *
 * equal:	compare "==" compare
 * 		compare "!=" compare
 *
 * bitand:	equal "&" equal
 *
 * bitxor:	bitand "^" bitand
 *
 * bitor:	bitxor "|" bitxor
 *
 * and:		bitor "&&" bitor
 *
 * or:		and "||" and
 *
 * range:	or ".." or
 *
 * expr:	or
 *
 * call:	ident [ expr expr ... expr ]
 *
 * stmt:	"let" ident "=" expr endl
 * 		"for" ident "in" expr slist
 * 		"if" expr slist [ else slist ]
 * 		"for" ident "in" expr slist
 * 		"return" expr
 * 		call endl
 * 		endl
 *
 * slist:	"{" [ stmt stmt ... stmt ] "}"
 *
 * proc:	"proc" ident [ ident ident ... ident ] slist
 *
 * line:	proc
 * 		stmt
 *
 * prog:	[ line line ... line ] EOF
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "data.h"
#include "parse.h"
#include "node.h"
#include "utils.h"
#include "exec.h"
#include "cons.h"

/*
 * tokinizer states
 */
enum LEX_STATE {
	LEX_ANY, LEX_ERROR, LEX_COMMENT, LEX_BREAK, LEX_BASE, LEX_NUM,
	LEX_IDENT, LEX_STRING, LEX_OP
};

/*
 * parser states
 */
enum {
	PARSE_RANGE, PARSE_RANGE_1,
	PARSE_OR, PARSE_OR_1,
	PARSE_AND, PARSE_AND_1,
	PARSE_BITOR, PARSE_BITOR_1,
	PARSE_BITXOR, PARSE_BITXOR_1,
	PARSE_BITAND, PARSE_BITAND_1,
	PARSE_EQ, PARSE_EQ_1,
	PARSE_CMP, PARSE_CMP_1,
	PARSE_SHIFT, PARSE_SHIFT_1,
	PARSE_SUB, PARSE_SUB_1,
	PARSE_DIV, PARSE_DIV_1,
	PARSE_UN,
	PARSE_CST,
	PARSE_LIST_1, PARSE_LIST_2,
	PARSE_PAR_1, PARSE_PAR_2,
	PARSE_VAR_1,
	PARSE_FUNC_1, PARSE_FUNC_2,
	PARSE_CALL, PARSE_CALL_1, PARSE_CALL_2,
	PARSE_STMT,
	PARSE_IF_1, PARSE_IF_2, PARSE_IF_3, PARSE_IF_4, 
	PARSE_FOR_1, PARSE_FOR_2, PARSE_FOR_3, PARSE_FOR_4, 
	PARSE_RET_1,		
	PARSE_LET_1, PARSE_LET_2,
	PARSE_ENDL,
	PARSE_SLIST, PARSE_SLIST_1, PARSE_SLIST_2,
	PARSE_PROC, PARSE_PROC_1, PARSE_PROC_2, PARSE_PROC_3,
	PARSE_PROG, PARSE_PROG_1,
	PARSE_ERROR
};

unsigned parse_debug = 0;

struct tokname {
	unsigned id;		/* token id */
	char *str;		/* corresponding string */
} lex_kw[] = {
	{ TOK_IF,		"if" 		},
	{ TOK_ELSE,		"else" 		},
	{ TOK_PROC,		"proc"		},
	{ TOK_LET,		"let"		},
	{ TOK_RETURN,		"return"	},
	{ TOK_FOR,		"for"		},
	{ TOK_IN,		"in"		},
	{ TOK_EXIT,		"exit"		},
	{ TOK_NIL,		"nil"		}
}, lex_op[] = {
	{ TOK_ELLIPSIS,		"..."		},
	{ TOK_RANGE,		".."		},
	{ TOK_DOT,		"."		},
	{ TOK_EQ, 		"=="		},
	{ TOK_ASSIGN,		"=" 		},
	{ TOK_NEQ, 		"!="		},
	{ TOK_EXCLAM, 		"!"		},
	{ TOK_GE, 		">="		},
	{ TOK_RSHIFT, 		">>"		},
	{ TOK_GT, 		">"		},
	{ TOK_LE, 		"<="		},
	{ TOK_LSHIFT, 		"<<"		},
	{ TOK_LT, 		"<"		},
	{ TOK_AND, 		"&&"		},
	{ TOK_BITAND, 		"&"		},
	{ TOK_OR, 		"||"		},
	{ TOK_BITOR, 		"|"		},
	{ TOK_PLUS,		"+" 		},
	{ TOK_MINUS,		"-" 		},
	{ TOK_STAR,		"*" 		},
	{ TOK_SLASH,		"/" 		},
	{ TOK_PCT,		"%" 		},
	{ TOK_LPAR,		"(" 		},
	{ TOK_RPAR,  		")" 		},
	{ TOK_LBRACE,  		"{" 		},
	{ TOK_RBRACE,  		"}" 		},
	{ TOK_LBRACKET, 	"[" 		},
	{ TOK_RBRACKET,		"]" 		},
	{ TOK_COMMA, 		"," 		},
	{ TOK_SEMICOLON,	";" 		},
	{ TOK_COLON, 		":" 		},
	{ TOK_BITXOR, 		"^"		},
	{ TOK_TILDE,		"~"		},
	{ TOK_AT,		"@"		},
	{ TOK_DOLLAR,		"$"		},
	{ TOK_ENDLINE,		"\n"		}
};

#define LEX_NKW (sizeof(lex_kw) / sizeof(lex_kw[0]))
#define LEX_NOP (sizeof(lex_op) / sizeof(lex_op[0]))

/*
 * names of parser pstates (debug only)
 */
char *parse_pstates[] = {
	"PARSE_RANGE", "PARSE_RANGE_1",
	"PARSE_OR", "PARSE_OR_1",
	"PARSE_AND", "PARSE_AND_1",
	"PARSE_BITOR", "PARSE_BITOR_1",
	"PARSE_BITXOR", "PARSE_BITXOR_1",
	"PARSE_BITAND", "PARSE_BITAND_1",
	"PARSE_EQ", "PARSE_EQ_1",
	"PARSE_CMP", "PARSE_CMP_1",
	"PARSE_SHIFT", "PARSE_SHIFT_1",
	"PARSE_SUB", "PARSE_SUB_1",
	"PARSE_DIV", "PARSE_DIV_1",
	"PARSE_UN",
	"PARSE_CST",
	"PARSE_LIST_1", "PARSE_LIST_2",
	"PARSE_PAR_1", "PARSE_PAR_2",
	"PARSE_VAR_1",
	"PARSE_FUNC_1", "PARSE_FUNC_2",
	"PARSE_CALL", "PARSE_CALL_1", "PARSE_CALL_2",
	"PARSE_STMT",
	"PARSE_IF_1", "PARSE_IF_2", "PARSE_IF_3", "PARSE_IF_4",
	"PARSE_FOR_1", "PARSE_FOR_2", "PARSE_FOR_3", "PARSE_FOR_4",
	"PARSE_RET_1",
	"PARSE_LET_1", "PARSE_LET_2",
	"PARSE_ENDL",
	"PARSE_SLIST", "PARSE_SLIST_1", "PARSE_SLIST_2",
	"PARSE_PROC", "PARSE_PROC_1", "PARSE_PROC_2", "PARSE_PROC_3",
	"PARSE_PROG", "PARSE_PROG_1",
	"PARSE_ERROR"
};

/*
 * set of tokens that may start an expression
 */
unsigned first_expr[] =
{
	TOK_MINUS, TOK_EXCLAM, TOK_TILDE,
	TOK_LPAR, TOK_DOLLAR, TOK_LBRACE, TOK_LBRACKET, 
	TOK_IDENT, TOK_NUM, TOK_STRING, TOK_NIL, TOK_ELLIPSIS, 0
};

/*
 * set of tokens that may start a pstatement
 */
unsigned first_stmt[] =
{
	TOK_SEMICOLON, TOK_ENDLINE,
	TOK_IF, TOK_FOR, TOK_IDENT, TOK_LET, TOK_RETURN, TOK_EXIT, 0
};

void
lex_init(struct parse *l, char *filename,
    void (*tokcb)(void *, unsigned, unsigned long), void *tokarg)
{
	l->lstate = LEX_ANY;
	l->tokcb = tokcb;
	l->tokarg = tokarg;
	l->filename = filename;
	l->line = 1;
}	

void
lex_done(struct parse *l)
{
	if (l->lstate != LEX_ANY)
		logx(1, "%s: unterminated token", __func__);
}

size_t
lex_tokfmt(char *buf, size_t bufsz, unsigned id, unsigned long val)
{
	unsigned i;

	if (id == TOK_ENDLINE)
		return snprintf(buf, bufsz, "\\n");


	for (i = 0; i < LEX_NKW; i++) {
		if (lex_kw[i].id == id)
			return snprintf(buf, bufsz, "%s", lex_kw[i].str);
	}
	for (i = 0; i < LEX_NOP; i++) {
		if (lex_op[i].id == id)
			return snprintf(buf, bufsz, "%s", lex_op[i].str);
	}
	switch (id) {
	case TOK_EOF:
		return snprintf(buf, bufsz, "EOF");
	case TOK_ERR:
		return snprintf(buf, bufsz, "ERR");
	case TOK_IDENT:
		return snprintf(buf, bufsz, "@%s", (char *)val);
	case TOK_NUM:
		return snprintf(buf, bufsz, "@%lu", val);
	case TOK_STRING:
		return snprintf(buf, bufsz, "\"%s\"", (char *)val);
	default:
		return snprintf(buf, bufsz, "UNK");
	}
}

void
lex_toklog(unsigned id, unsigned long val)
{
	char buf[64];

	lex_tokfmt(buf, sizeof(buf), id, val);
	logx(1, "%s", buf);
}

void
lex_err(struct parse *l, char *msg)
{
	logx(1, "%s: %u: %s", l->filename, l->line, msg);
	l->lstate = LEX_ERROR;
	l->tokcb(l->tokarg, TOK_ERR, 0);
}

void
lex_found(struct parse *l, unsigned id, unsigned long val)
{
	l->lstate = LEX_ANY;
	l->tokcb(l->tokarg, id, val);
}

/*
 * convert string to number in the appropriate base
 */
int
lex_atol(struct parse *l)
{
	char *p;
	unsigned digit;
	unsigned long hi, lo;
	unsigned long val;
#define BITS		(8 * sizeof(unsigned long) / 2)
#define LOWORD(a)	((a) & ((1L << BITS) - 1))
#define HIWORD(a)	((a) >> BITS)
	val = 0;
	for (p = l->buf; *p != '\0'; p++) {
		if (*p >= 'a' && *p <= 'z') {
			digit = 10 + *p - 'a';
		} else if (*p >= 'A' && *p <= 'Z') {
			digit = 10 + *p - 'A';
		} else {
			digit = *p - '0';
		}
		if (digit >= l->base) {
			lex_err(l, "not allowed digit in numeric constant");
			return 0;
		}
		lo = digit + l->base * LOWORD(val);
		hi = l->base * HIWORD(val);
		if (HIWORD(hi + HIWORD(lo)) != 0) {
			lex_err(l, "overflow in numeric constant");
			return 0;
		}
		val = (hi << BITS) + lo;
	}
#undef BITS
#undef LOWORD
#undef HIWORD
	lex_found(l, TOK_NUM, val);
	return 1;
}

void
lex_handle(struct parse *l, int c)
{
	unsigned i;

	if (c == '\n')
		l->line++;
	for (;;) {
		switch (l->lstate) {
		case LEX_ANY:
			if (c < 0) {
				lex_found(l, TOK_EOF, 0);
				return;
			}
			if (IS_SPACE(c))
				return;
			if (c == '#') {
				l->lstate = LEX_COMMENT;
				return;
			}
			if (c == '\\') {
				l->lstate = LEX_BREAK;
				return;
			}
			if (IS_DIGIT(c)) {
				l->used = 0;
				if (c == '0') {
					l->lstate = LEX_BASE;
					return;
				}
				l->lstate = LEX_NUM;
				l->base = 10;
				break;
			}
			if (IS_IDFIRST(c)) {
				l->lstate = LEX_IDENT;
				l->buf[0] = c;
				l->used = 1;
				return;
			}
			if (IS_QUOTE(c)) {
				l->lstate = LEX_STRING;
				l->used = 0;
				return;
			}

			for (i = 0; ; i++) {
				if (i == LEX_NOP) {
					lex_err(l, "character doesn't start any token");
					break;
				}
				if (lex_op[i].str[0] == c) {
					l->lstate = LEX_OP;
					l->opindex = i;
					l->used = 0;
					break;
				}
			}
			break;
		case LEX_ERROR:
			if (c < 0 || c == '\n') {
				l->lstate = LEX_ANY;
				break;
			}
			return;
		case LEX_COMMENT:
			if (c < 0 || c == '\n') {
				l->lstate = LEX_ANY;
				break;
			}
			return;
		case LEX_BREAK:
			if (IS_SPACE(c))
				return;
			if (c != '\n') {
				lex_err(l, "newline expected after '\\'");
				break;
			}
			l->lstate = LEX_ANY;
			return;
		case LEX_BASE:
			l->lstate = LEX_NUM;
			if (c == 'x' || c == 'X') {
				l->base = 16;
				return;
			}
			l->base = 8;
			break;
		case LEX_NUM:
			if (IS_DIGIT(c) || IS_ALPHA(c)) {
				if (l->used == STRING_MAXSZ - 1) {
					lex_err(l, "constant too long");
					break;
				}
				l->buf[l->used++] = c;
				return;
			}
			l->buf[l->used] = 0;
			lex_atol(l);
			break;
		case LEX_IDENT:
			if (IS_IDNEXT(c)) {
				if (l->used == IDENT_MAXSZ - 1) {
					lex_err(l, "identifier too long");
					break;
				}
				l->buf[l->used++] = c;
				return;
			}
			l->buf[l->used] = '\0';
			for (i = 0;; i++) {
				if (i == LEX_NKW) {
					lex_found(l, TOK_IDENT, (unsigned long)l->buf);
					break;
				}
				if (strcmp(lex_kw[i].str, l->buf) == 0) {
					lex_found(l, lex_kw[i].id, 0);
					break;
				}
			}
			break;
		case LEX_STRING:
			if (IS_QUOTE(c)) {
				l->buf[l->used] = '\0';
				lex_found(l, TOK_STRING, (unsigned long)l->buf);
				return;
			}
			if (!IS_PRINTABLE(c)) {
				lex_err(l, "non printable char in string");
				break;
			}
			if (l->used == STRING_MAXSZ - 1) {
				lex_err(l, "string too long");
				break;
			}
			l->buf[l->used++] = c;
			return;
		case LEX_OP:
			if (lex_op[l->opindex].str[l->used] == c) {
				l->used++;
				if (lex_op[l->opindex].str[l->used] == '\0')
					lex_found(l, lex_op[l->opindex].id, 0);
				return;
			} else if (lex_op[l->opindex].str[l->used] == '\0') {
				lex_found(l, lex_op[l->opindex].id, 0);
				break;
			}

			l->opindex++;
			if (l->opindex == LEX_NOP) {
			bad_op:
				lex_err(l, "bad operator");
				break;
			}
			for (i = 0; i < l->used; i++) {
				if (lex_op[i].str[i] != lex_op[i].str[i])
					goto bad_op;
			}
			break;
		default:
			logx(1, "%s: bad state", __func__);
			panic();
		}
	}
}

/*
 * return true if the given token is in the given set
 */
unsigned
tok_is(unsigned id, unsigned *set)
{
	while (*set != 0) {
		if (id == *set)
			return 1;
		set++;
	}
	return 0;
}

/*
 * initialize the parser
 */
void
parse_init(struct parse *p, struct exec *e,
    void (*cb)(struct exec *, struct node *))
{
	p->root = NULL;
	p->sp = p->stack;
	p->sp->pstate = PARSE_PROG;
	p->sp->pnode = (void *)0xdeadbeef;
	p->exec = e;
	p->cb = cb;
}

void
parse_done(struct parse *p)
{
	if (p->sp->pstate != PARSE_PROG)
		logx(1, "%s: unterminated rule", __func__);
	if (p->sp - p->stack > 0)
		logx(1, "%s: stack not empty", __func__);
}

/*
 * push the current pstate in the stack and
 * switch to the given pstate
 */
void
parse_begin(struct parse *p, unsigned newpstate, struct node **newpnode)
{
	p->sp++;
	p->sp->pstate = newpstate;
	p->sp->pnode = newpnode;
}

/*
 * pop and restore a pstate
 */
void
parse_end(struct parse *p)
{
	p->sp--;
}

/*
 * display a syntax error and start recovering
 */
void
parse_err(struct parse *p, char *msg)
{
	if (msg)
		logx(1, "%s: %u: %s", p->filename, p->line, msg);
	node_delete(p->root);
	p->root = NULL;
	p->sp = p->stack;
	p->sp->pstate = PARSE_ERROR;
	p->sp->pnode = (void *)0xdeadbeef;
	p->cb(p->exec, NULL);
}

void
parse_found(struct parse *p)
{
	if (parse_debug)
		node_log(p->root, 0);

	p->cb(p->exec, p->root);
	node_delete(p->root);
	p->root = NULL;
}

void
parse_log(struct parse *p)
{
	char istr[PARSE_STACKLEN * 2 + 1], *ip = istr;
	struct pst *sp;

	for (sp = p->stack; sp != p->sp; sp++) {
		*ip++ = '.';
		*ip++ = ' ';
	}
	*ip++ = 0;

	logx(1, "%s%s", istr, parse_pstates[sp->pstate]);
}

/*
 * process the given token
 */
void
parse_cb(void *arg, unsigned id, unsigned long val)
{
	struct parse *p = (struct parse *)arg;
	struct node_vmt *vmt;
	struct data *args;
	struct pst *sp;

	if (parse_debug) {
		lex_toklog(id, val);
	}

	for (;;) {
		if (parse_debug)
			parse_log(p);

		if (id == TOK_ERR) {
			parse_err(p, NULL);
			id = 0;
		}
		sp = p->sp;
		switch (sp->pstate) {
		case PARSE_RANGE:
			sp->pstate = PARSE_RANGE_1;
			parse_begin(p, PARSE_OR, sp->pnode);
			break;
		case PARSE_RANGE_1:
			if (id == 0)
				return;
			if (id == TOK_RANGE) {
				vmt = &node_vmt_range;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_OR, &(*sp->pnode)->list->next);
			break;
		case PARSE_OR:
			sp->pstate = PARSE_OR_1;
			parse_begin(p, PARSE_AND, sp->pnode);
			break;
		case PARSE_OR_1:
			if (id == 0)
				return;
			if (id == TOK_OR) {
				vmt = &node_vmt_or;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_AND, &(*sp->pnode)->list->next);
			break;
		case PARSE_AND:
			sp->pstate = PARSE_AND_1;
			parse_begin(p, PARSE_BITOR, sp->pnode);
			break;
		case PARSE_AND_1:
			if (id == 0)
				return;
			if (id == TOK_AND) {
				vmt = &node_vmt_and;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_BITOR, &(*sp->pnode)->list->next);
			break;
		case PARSE_BITOR:
			sp->pstate = PARSE_BITOR_1;
			parse_begin(p, PARSE_BITXOR, sp->pnode);
			break;
		case PARSE_BITOR_1:
			if (id == 0)
				return;
			if (id == TOK_BITOR) {
				vmt = &node_vmt_bitor;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_BITXOR, &(*sp->pnode)->list->next);
			break;
		case PARSE_BITXOR:
			sp->pstate = PARSE_BITXOR_1;
			parse_begin(p, PARSE_BITAND, sp->pnode);
			break;
		case PARSE_BITXOR_1:
			if (id == 0)
				return;
			if (id == TOK_BITXOR) {
				vmt = &node_vmt_bitxor;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_BITAND, &(*sp->pnode)->list->next);
			break;
		case PARSE_BITAND:
			sp->pstate = PARSE_BITAND_1;
			parse_begin(p, PARSE_EQ, sp->pnode);
			break;
		case PARSE_BITAND_1:
			if (id == 0)
				return;
			if (id == TOK_BITAND) {
				vmt = &node_vmt_bitand;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_EQ, &(*sp->pnode)->list->next);
			break;
		case PARSE_EQ:
			sp->pstate = PARSE_EQ_1;
			parse_begin(p, PARSE_CMP, sp->pnode);
			break;
		case PARSE_EQ_1:
			if (id == 0)
				return;
			if (id == TOK_EQ) {
				vmt = &node_vmt_eq;
			} else if (id == TOK_NEQ) {
				vmt = &node_vmt_neq;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_CMP, &(*sp->pnode)->list->next);
			break;
		case PARSE_CMP:
			sp->pstate = PARSE_CMP_1;
			parse_begin(p, PARSE_SHIFT, sp->pnode);
			break;
		case PARSE_CMP_1:
			if (id == 0)
				return;
			if (id == TOK_LT) {
				vmt = &node_vmt_lt;
			} else if (id == TOK_LE) {
				vmt = &node_vmt_le;
			} else if (id == TOK_GT) {
				vmt = &node_vmt_gt;
			} else if (id == TOK_GE) {
				vmt = &node_vmt_ge;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_SHIFT, &(*sp->pnode)->list->next);
			break;
		case PARSE_SHIFT:
			sp->pstate = PARSE_SHIFT_1;
			parse_begin(p, PARSE_SUB, sp->pnode);
			break;
		case PARSE_SHIFT_1:
			if (id == 0)
				return;
			if (id == TOK_LSHIFT) {
				vmt = &node_vmt_lshift;
			} else if (id == TOK_RSHIFT) {
				vmt = &node_vmt_rshift;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_SUB, &(*sp->pnode)->list->next);
			break;
		case PARSE_SUB:
			sp->pstate = PARSE_SUB_1;
			parse_begin(p, PARSE_DIV, sp->pnode);
			break;
		case PARSE_SUB_1:
			if (id == 0)
				return;
			if (id == TOK_MINUS) {
				vmt = &node_vmt_sub;
			} else if (id == TOK_PLUS) {
				vmt = &node_vmt_add;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_DIV, &(*sp->pnode)->list->next);
			break;
		case PARSE_DIV:
			sp->pstate = PARSE_DIV_1;
			parse_begin(p, PARSE_UN, sp->pnode);
			break;
		case PARSE_DIV_1:
			if (id == 0)
				return;
			if (id == TOK_SLASH) {
				vmt = &node_vmt_div;
			} else if (id == TOK_STAR) {
				vmt = &node_vmt_mul;
			} else if (id == TOK_PCT) {
				vmt = &node_vmt_mod;
			} else {
				parse_end(p);
				break;
			}
			id = 0;
			node_replace(sp->pnode, node_new(vmt, NULL));
			parse_begin(p, PARSE_UN, &(*sp->pnode)->list->next);
			break;
		case PARSE_UN:
			if (id == 0)
				return;
			if (id == TOK_MINUS) {
				vmt = &node_vmt_neg;
			} else if (id == TOK_EXCLAM) {
				vmt = &node_vmt_not;
			} else if (id == TOK_TILDE) {
				vmt =&node_vmt_bitnot;
			} else {
				sp->pstate = PARSE_CST;
				break;
			}
			id = 0;
			*sp->pnode = node_new(vmt, NULL);
			sp->pnode = &(*sp->pnode)->list;
			break;
		case PARSE_CST:
			if (id == 0)
				return;
			if (id == TOK_NUM) {
				*sp->pnode = node_new(&node_vmt_cst,
					data_newlong(val));
				parse_end(p);
			} else if (id == TOK_STRING) {
				*sp->pnode = node_new(&node_vmt_cst,
					data_newstring((char *)val));
				parse_end(p);
			} else if (id == TOK_IDENT) {
				*sp->pnode = node_new(&node_vmt_cst,
					data_newref((char *)val));
				parse_end(p);
			} else if (id == TOK_ELLIPSIS) {
				*sp->pnode = node_new(&node_vmt_var,
					data_newref((char *)"..."));
				parse_end(p);
			} else if (id == TOK_NIL) {
				*sp->pnode = node_new(&node_vmt_cst,
					data_newnil());
				parse_end(p);
			} else if (id == TOK_LPAR) {
				sp->pstate = PARSE_PAR_1;
			} else if (id == TOK_DOLLAR) {
				sp->pstate = PARSE_VAR_1;
			} else if (id == TOK_LBRACKET) {
				sp->pstate = PARSE_FUNC_1;
			} else if (id == TOK_LBRACE) {
				*sp->pnode = node_new(&node_vmt_list, NULL);
				sp->pnode = &(*sp->pnode)->list;
				sp->pstate = PARSE_LIST_1;
			} else {
				parse_err(p, "value or ``('' expected");
				break;
			}
			id = 0;
			break;
		case PARSE_LIST_1:
			if (id == 0)
				return;
			if (id == TOK_RBRACE) {
				parse_end(p);
			} else if (tok_is(id, first_expr)) {
				sp->pstate = PARSE_LIST_2;
				parse_begin(p, PARSE_RANGE, sp->pnode);
				break;
			} else {
				parse_err(p, "expr or ``}'' expected");
				break;
			}
			id = 0;
			break;
		case PARSE_LIST_2:
			sp->pnode = &(*sp->pnode)->next;
			sp->pstate = PARSE_LIST_1;
			break;
		case PARSE_PAR_1:
			sp->pstate = PARSE_PAR_2;
			parse_begin(p, PARSE_RANGE, sp->pnode);
			break;
		case PARSE_PAR_2:
			if (id == 0)
				return;
			if (id != TOK_RPAR) {
				parse_err(p, "')' expected");
				break;
			}
			id = 0;
			parse_end(p);
			break;
		case PARSE_VAR_1:
			if (id == 0)
				return;
			if (id != TOK_IDENT) {
				parse_err(p, "identifier expected");
				break;
			}
			id = 0;
			*sp->pnode = node_new(&node_vmt_var,
				data_newref((char *)val));
			parse_end(p);
			break;
		case PARSE_FUNC_1:
			sp->pstate = PARSE_FUNC_2;
			parse_begin(p, PARSE_CALL, sp->pnode);
			break;
		case PARSE_FUNC_2:
			if (id == 0)
				return;
			if (id != TOK_RBRACKET) {
				parse_err(p, "']' expected");
				break;
			}
			id = 0;
			parse_end(p);
			break;
		case PARSE_CALL:
			if (id == 0)
				return;
			if (id != TOK_IDENT) {
				parse_err(p, "proc identifier expected");
				break;
			}
			id = 0;
			*sp->pnode = node_new(&node_vmt_call,
				data_newref((char *)val));
			sp->pnode = &(*sp->pnode)->list;
			sp->pstate = PARSE_CALL_1;
			break;
		case PARSE_CALL_1:
			if (id == 0)
				return;
			if (!tok_is(id, first_expr)) {
				parse_end(p);
				break;
			}
			sp->pstate = PARSE_CALL_2;
			parse_begin(p, PARSE_RANGE, sp->pnode);
			break;
		case PARSE_CALL_2:
			sp->pnode = &(*sp->pnode)->next;
			sp->pstate = PARSE_CALL_1;
			break;
		case PARSE_STMT:
			if (id == 0)
				return;
			if (id == TOK_IDENT) {
				sp->pstate = PARSE_ENDL;
				parse_begin(p, PARSE_CALL, sp->pnode);
				break;
			} else if (id == TOK_EXIT) {
				*sp->pnode = node_new(&node_vmt_exit, NULL);
				sp->pstate = PARSE_ENDL;
			} else if (id == TOK_IF) {
				sp->pstate = PARSE_IF_1;
			} else if (id == TOK_FOR) {
				sp->pstate = PARSE_FOR_1;
			} else if (id == TOK_RETURN) {
				sp->pstate = PARSE_RET_1;
			} else if (id == TOK_LET) {
				sp->pstate = PARSE_LET_1;
			} else if (id == TOK_ENDLINE || id == TOK_SEMICOLON) {
				*sp->pnode = node_new(&node_vmt_nop, NULL);
				parse_end(p);
			} else {
				parse_err(p, "pstatement expected");
				break;
			}
			id = 0;
			break;
		case PARSE_IF_1:
			if (id == 0)
				return;
			if (!tok_is(id, first_expr)) {
				parse_err(p, "expr expected after ``if''");
				break;
			}
			*sp->pnode = node_new(&node_vmt_if, NULL);
			sp->pnode = &(*sp->pnode)->list;
			sp->pstate = PARSE_IF_2;
			parse_begin(p, PARSE_RANGE, sp->pnode);
			break;
		case PARSE_IF_2:
			sp->pnode = &(*sp->pnode)->next;
			sp->pstate = PARSE_IF_3;
			parse_begin(p, PARSE_SLIST, sp->pnode);
			break;
		case PARSE_IF_3:
			if (id == 0)
				return;
			if (id != TOK_ELSE) {
				parse_end(p);
				break;
			}
			id = 0;
			sp->pnode = &(*sp->pnode)->next;
			sp->pstate = PARSE_IF_4;
			parse_begin(p, PARSE_SLIST, sp->pnode);
			break;
		case PARSE_IF_4:
			parse_end(p);
			break;
		case PARSE_FOR_1:
			if (id == 0)
				return;
			if (id != TOK_IDENT) {
				parse_err(p, "ident expected after ``for''");
				break;
			}
			id = 0;
			*sp->pnode = node_new(&node_vmt_for,
				data_newref((char *)val));
			sp->pstate = PARSE_FOR_2;
			break;
		case PARSE_FOR_2:
			if (id == 0)
				return;
			if (id != TOK_IN) {
				parse_err(p, "``in'' expected");
				break;
			}
			id = 0;
			sp->pnode = &(*sp->pnode)->list;
			sp->pstate = PARSE_FOR_3;
			parse_begin(p, PARSE_RANGE, sp->pnode);
			break;
		case PARSE_FOR_3:
			sp->pnode = &(*sp->pnode)->next;
			sp->pstate = PARSE_FOR_4;
			parse_begin(p, PARSE_SLIST, sp->pnode);
			break;
		case PARSE_FOR_4:
			parse_end(p);
			break;
		case PARSE_RET_1:
			*sp->pnode = node_new(&node_vmt_return, NULL);
			sp->pnode = &(*sp->pnode)->list;
			sp->pstate = PARSE_ENDL;
			parse_begin(p, PARSE_RANGE, sp->pnode);
			break;
		case PARSE_LET_1:
			if (id == 0)
				return;
			if (id != TOK_IDENT) {
				parse_err(p, "ref expected after ``let''");
				break;
			}
			id = 0;
			*sp->pnode = node_new(&node_vmt_assign, 
				data_newref((char *)val));
			sp->pstate = PARSE_LET_2;
			break;
		case PARSE_LET_2:
			if (id == 0)
				return;
			if (id != TOK_ASSIGN) {
				parse_err(p, "``='' expected");
				break;
			}
			id = 0;
			sp->pnode = &(*sp->pnode)->list;
			sp->pstate = PARSE_ENDL;
			parse_begin(p, PARSE_RANGE, sp->pnode);
			break;
		case PARSE_ENDL:
			if (id == 0)
				return;
			if (id == TOK_SEMICOLON || id == TOK_ENDLINE) {
				id = 0;
				parse_end(p);
				break;
			}
			parse_err(p, "``;'' or new line expected");
			break;
		case PARSE_SLIST:
			if (id == 0)
				return;
			if (id != TOK_LBRACE) {
				parse_err(p, "``{'' expected");
				break;
			}
			id = 0;
			*sp->pnode = node_new(&node_vmt_slist, NULL);
			sp->pnode = &(*sp->pnode)->list;
			sp->pstate = PARSE_SLIST_1;
			break;
		case PARSE_SLIST_1:
			if (id == 0)
				return;
			if (id == TOK_RBRACE) {
				id = 0;
				parse_end(p);
				break;
			}
			sp->pstate = PARSE_SLIST_2;
			parse_begin(p, PARSE_STMT, sp->pnode);
			break;
		case PARSE_SLIST_2:
			sp->pnode = &(*sp->pnode)->next;
			sp->pstate = PARSE_SLIST_1;
			break;
		case PARSE_PROC_1:
			if (id == 0)
				return;
			if (id != TOK_IDENT) {
				parse_err(p, "ref expected after ``proc''");
				break;
			}
			id = 0;
			args = data_newlist(NULL);
			data_listadd(args, 
				data_newref((char *)val));
			*sp->pnode = node_new(&node_vmt_proc, args);
			sp->pstate = PARSE_PROC_2;
			break;
		case PARSE_PROC_2:
			if (id == 0)
				return;
			if (id == TOK_IDENT) {
				id = 0;
				args = (*sp->pnode)->data;
				data_listadd(args, data_newref((char *)val));
				break;
			}
			if (id == TOK_ELLIPSIS) {
				id = 0;
				args = (*sp->pnode)->data;
				data_listadd(args, data_newref("..."));
				sp->pstate = PARSE_PROC_3;
				break;
			}
			if (id == TOK_LBRACE) {
				sp->pstate = PARSE_PROC_3;
				break;
			}
			parse_err(p, "arg name or block expected");
			break;
		case PARSE_PROC_3:
			if (id == 0)
				return;
			if (id != TOK_LBRACE) {
				parse_err(p, "'{' expected");
				break;
			}
			sp->pnode = &(*sp->pnode)->list;
			sp->pstate = PARSE_SLIST;
			break;
		case PARSE_PROG:
			if (id == 0)
				return;
			if (id == TOK_PROC) {
				id = 0;
				sp->pstate = PARSE_PROG_1;
				parse_begin(p, PARSE_PROC_1, &p->root);
				break;
			}
			if (tok_is(id, first_stmt)) {
				sp->pstate = PARSE_PROG_1;
				parse_begin(p, PARSE_STMT, &p->root);
				break;
			}
			if (id == TOK_EOF)
				return;
			parse_err(p, "stmt or proc def expected");
			break;
		case PARSE_PROG_1:
			parse_found(p);
			sp->pstate = PARSE_PROG;
			break;
		case PARSE_ERROR:
			if (id == 0)
				return;
			if (id == TOK_EOF) {
				sp->pstate = PARSE_PROG;
				break;
			}
			if (id == TOK_ENDLINE || id == TOK_RBRACE)
				sp->pstate = PARSE_PROG;
			id = 0;
			break;
		default:
			logx(1, "%s: bad state", __func__);
			panic();
		}
	}
}
