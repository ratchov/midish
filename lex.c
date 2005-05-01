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
 *	- The name of the author may not be used to endorse or promote
 *	  products derived from this software without specific prior
 *	  written permission.
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
 * simple lexical analyser: just converts a string into a stream of
 * tokens. 
 */

#include "dbg.h"
#include "lex.h"
#include "str.h"
#include "textio.h"
#include "user.h"	/* for user_printstr */

#define IS_SPACE(c)	((c) == ' ' || (c) == '\r' || (c) == '\t')
#define IS_PRINTABLE(c)	((c) >= ' ')
#define IS_DIGIT(c)	((c) >= '0' && (c) <= '9')
#define IS_ALPHA(c)	(((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
#define IS_IDFIRST(c)	(IS_ALPHA(c) || (c) == '_')
#define IS_IDNEXT(c)	(IS_ALPHA(c) || (c) == '_' || IS_DIGIT(c))
#define IS_QUOTE(c)	((c) == '"')

/* ----------------------------------------------------- tokdefs --- */

struct tokdef_s lex_op[] = {
	{ TOK_EQ, 		"=="		},
	{ TOK_NEQ, 		"!="		},
	{ TOK_GE, 		">="		},
	{ TOK_LE, 		"<="		},
	{ TOK_LSHIFT, 		"<<"		},
	{ TOK_RSHIFT, 		">>"		},
	{ TOK_AND, 		"&&"		},
	{ TOK_OR, 		"||"		},
	{ TOK_ASSIGN,		"=" 		},
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
	{ TOK_BITAND, 		"&"		},
	{ TOK_BITOR, 		"|"		},
	{ TOK_BITXOR, 		"^"		},
	{ TOK_TILDE,		"~"		},
	{ TOK_EXCLAM, 		"!"		},
	{ TOK_GT, 		">"		},
	{ TOK_LT, 		"<"		},
	{ TOK_AT,		"@"		},
	{ TOK_DOLLAR,		"$"		},
	{ TOK_DOT,		"."		},
	{ 0,			0		}
};

struct tokdef_s lex_kw[] = {
	{ TOK_IF,		"if" 		},
	{ TOK_ELSE,		"else" 		},
	{ TOK_PROC,		"proc"		},
	{ TOK_LET,		"let"		},
	{ TOK_RETURN,		"return"	},
	{ TOK_FOR,		"for"		},
	{ TOK_IN,		"in"		},
	{ TOK_NIL,		"nil"		},
	{ 0,		  	0		}
};

unsigned
lex_init(struct lex_s *o, char *filename) {
	o->lookchar = -1;
	o->in = textin_new(filename, "> ");
	if (!o->in) {
		return 0;
	}
	return 1;
}

void
lex_done(struct lex_s *o) {
	textin_delete(o->in);
}

unsigned
lex_getchar(struct lex_s *o, int *c) {
	if (o->lookchar < 0) {
		textin_getpos(o->in, &o->line, &o->col);
		if (!textin_getchar(o->in, c)) {
			return 0;
		}
	} else {
		*c = o->lookchar;
		o->lookchar = -1;
	}
	return 1;
}

void
lex_ungetchar(struct lex_s *o, int c) {
	if (o->lookchar >= 0) {
		dbg_puts("lex_ungetchar: lookchar already set\n");
		dbg_panic();
	}
	o->lookchar = c;
}

void
lex_error(struct lex_s *o, char *msg) {
	user_printstr("near line ");
	user_printlong(o->line + 1);
	user_printstr(", col ");
	user_printlong(o->col + 1);
	user_printstr(": ");
	user_printstr(msg);
}


	/*
	 * convert string to number in any base between 2 and 36
	 */

unsigned
lex_str2long(struct lex_s *o, unsigned base) {
	char *p;
	unsigned digit;
	unsigned long hi, lo;
#define BITS		(8 * sizeof(unsigned long) / 2)
#define LOWORD(a)	((a) & ((1L << BITS) - 1))
#define HIWORD(a)	((a) >> BITS)
	o->longval = 0;
	for (p = o->strval; *p != 0; p++) {
		if (*p >= 'a' && *p <= 'z') {
			digit = 10 + *p - 'a';
		} else if (*p >= 'A' && *p <= 'Z') {
			digit = 10 + *p - 'A';
		} else {
			digit = *p - '0';
		}
		if (digit >= base) {
			lex_error(o, "not allowed digit in numeric constant\n");
			return 0;
		}
		lo = digit + base * LOWORD(o->longval);
		hi = base * HIWORD(o->longval);
		if (HIWORD(hi + HIWORD(lo)) != 0) {
			lex_error(o, "overflow in numeric constant\n");
			return 0;
		}
		o->longval = (hi << BITS) + lo;
	}	
#undef BITS
#undef LOWORD
#undef HIWORD
	return 1;
}

unsigned
lex_scan(struct lex_s *o) {
	int c, cn;
	unsigned i, base;
	
	for (;;) {
		if (!lex_getchar(o, &c)) {
			return 0;
		}

		/* check for end of buf */
		if (c == CHAR_EOF) {
			o->id = TOK_EOF;
			return 1;
		}

		/* strip spaces */
		if (IS_SPACE(c)) {
			continue;
		}

		/* check if line continues */
		if (c == '\\') {
			do {
				if (!lex_getchar(o, &c)) {
					return 0;
				}
			} while (IS_SPACE(c));
			if (c != '\n') {
				lex_error(o, "newline exected after '\\'\n");
				return 0;
			}
			continue;
		}

		/* check for newline */
		if (c == '\n') {
			o->id = TOK_ENDLINE;
			return 1;
		}

		/* strip comments */
		if (c == '#') {
			do {
				if (!lex_getchar(o, &c)) {
					return 0;
				}
			} while (c != '\n' && c  != '\\' && c != CHAR_EOF);
			lex_ungetchar(o, c);
			continue;
		}

		/* check for string */
		if (IS_QUOTE(c)) {
			i = 0;
			for (;;) {
				if (!lex_getchar(o, &c)) {
					return 0;
				}
				/* 
				 * dont strip \n embedded in strings in order
				 * not to break error recovering code in the parsers
				 */				 
				if (!IS_PRINTABLE(c)) {
					lex_error(o, "non printable char in string constant\n");
					lex_ungetchar(o, c);
					goto recover;
				}
				if (IS_QUOTE(c)) {
					o->strval[i++] = '\0';
					break;
				}
				if (i >= STRING_MAXLEN) {
					lex_error(o, "string constant too long\n");
					goto recover;
				}
				o->strval[i++] = c;
			}
			o->id = TOK_STRING;
			return 1;
		}

		/* check for numeric */
		if (IS_DIGIT(c)) {
			base = 10;
			if (c == '0') {
				if (!lex_getchar(o, &c)) {
					return 0;
				}
				if (c == 'x' || c == 'X') {
					base = 16;
					if (!lex_getchar(o, &c)) {
						return 0;
					}
					if (!IS_DIGIT(c) && !IS_ALPHA(c)) {
						lex_error(o, "bad hex constant\n");
						lex_ungetchar(o, c);
						goto recover;
					}
				} 
			}

			i = 0;
			for (;;) {
				if (!IS_DIGIT(c) && !IS_ALPHA(c)) {
					o->strval[i++] = '\0';
					lex_ungetchar(o, c);
					break;
				}
				o->strval[i++] = c;
				if (i >= TOK_MAXLEN) {
					lex_error(o, "numeric constant too long\n");
					goto recover;
				}
				if (!lex_getchar(o, &c)) {
					return 0;
				}
			}			
			if (!lex_str2long(o, base)) {
				goto recover;
			}
			o->id = TOK_NUM;
			return 1;
		}

		/* check for identifier or keyword */
		if (IS_IDFIRST(c)) {
			i = 0;
			for (;;) {
				if (i >= IDENT_MAXLEN) {
					lex_error(o, "alphanumeric token too long\n");
					goto recover;
				}
				o->strval[i++] = c;
				if (!lex_getchar(o, &c)) {
					return 0;
				}
				if (!IS_IDNEXT(c)) {
					o->strval[i++] = '\0';
					lex_ungetchar(o, c);
					break;
				}
			}
			for (i = 0; lex_kw[i].id != 0; i++) {
				if (str_eq(lex_kw[i].str, o->strval)) {
					o->id = lex_kw[i].id;
					return 1;
				}
			}			
			o->id = TOK_IDENT;
			return 1;
		}

		/* check for operators */
		if (!lex_getchar(o, &cn)) {
			return 0;
		}
		for (i = 0; lex_op[i].id != 0; i++) {
			if (lex_op[i].str[0] == c) {
				if  (lex_op[i].str[1] == 0) {
					lex_ungetchar(o, cn);
					o->id = lex_op[i].id;
					return 1;
				} else if (lex_op[i].str[1] == cn) {
					o->id = lex_op[i].id;
					return 1;
				}
			}
		}
		lex_error(o, "bad token\n");
		lex_ungetchar(o, cn);
		goto recover;
	}
recover:
	for (;;) {
		if (!lex_getchar(o, &c)) {
			return 0;
		}
		if (c == '\n' || c == '\\' || c == CHAR_EOF) {
			lex_ungetchar(o, c);
			break;
		}
	}
	return 0;
}

void
lex_dbg(struct lex_s *o) {
	struct tokdef_s *t;
	
	if (o->id == 0) {
		dbg_puts("NULL");
		return;
	} else if (o->id == TOK_ENDLINE) {
		dbg_puts("\\n");
		return;
	}
	for (t = lex_op; t->id != 0; t++) {
		if (t->id == o->id) {
			dbg_puts(t->str);
			return;
		}
	}
	for (t = lex_kw; t->id != 0; t++) {
		if (t->id == o->id) {
			dbg_puts(t->str);
			return;
		}
	}
	switch(o->id) {		
	case TOK_IDENT:
		dbg_puts("IDENT{");
		dbg_puts(o->strval);
		dbg_puts("}");
		break;
	case TOK_NUM:
		dbg_puts("NUM{");
		dbg_putu(o->longval);
		dbg_puts("}");
		break;
	case TOK_STRING:
		dbg_puts("STRING{\"");
		dbg_puts(o->strval);
		dbg_puts("\"}");
		break;			
	default:
		dbg_puts("UNKNOWN");
		break;
	}
}

