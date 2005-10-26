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
 *
 * a simple LL(1)-like parser. Uses the following grammar:
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
 * expr:	or
 * 
 * call:	ident [ expr expr ... expr ]
 * 
 * stmt:	"let" ident "=" expr endl
 * 		"for" ident "in" expr slist endl
 * 		"if" expr slist [ else slist ] endl
 * 		"for" ident "in" expr slist endl
 * 		"return" expr endl
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

#include "dbg.h"
#include "textio.h"
#include "lex.h"
#include "data.h"
#include "parse.h"
#include "node.h"

#include "cons.h"	/* cons_errxxx */

/* ------------------------------------------------------------- */

void
parse_recover(struct parse_s *o, char *msg) {
	lex_err(&o->lex, msg);
	for (;;) {
		if (o->lex.id == TOK_EOF) {
			parse_ungetsym(o);
			break;
		}
		if (o->lex.id == TOK_ENDLINE) {
			break;
		}
		if (!parse_getsym(o)) {
			break;
		}
	}
}

struct parse_s *
parse_new(char *filename) {
	struct parse_s *o;
	o = (struct parse_s *)mem_alloc(sizeof(struct parse_s));	
	if (!lex_init(&o->lex, filename)) {
		mem_free(o);
		return 0;
	}
	o->lookavail = 0;
	return o;
}

void
parse_delete(struct parse_s *o) {
	lex_done(&o->lex);
	mem_free(o);
}

unsigned
parse_getsym(struct parse_s *o) {
	if (o->lookavail) {
		o->lookavail = 0;
		return 1;
	}
	return lex_scan(&o->lex);
}

void
parse_ungetsym(struct parse_s *o) {
	if (o->lookavail) {
		dbg_puts("parse_ungetsym: looksym already set\n");
		dbg_panic();
	}
	o->lookavail = 1; 
}

unsigned
parse_isfirst(struct parse_s *o, unsigned *first) {
	while (*first != 0) {
		if (*first == o->lex.id) {
			return 1;
		}
		first++;
	}
	return 0;
}

/* ------------------------------------------------------------- */

unsigned first_endl[] = {
	TOK_ENDLINE, TOK_SEMICOLON, 0
};

unsigned first_proc[] = {
	TOK_PROC, 0
};

unsigned first_assign[] = {
	TOK_LET, 0
};

unsigned first_call[] = {
	TOK_IDENT, 0
};

unsigned first_expr[] = {
	TOK_MINUS, TOK_EXCLAM, TOK_TILDE,
	TOK_LPAR, TOK_DOLLAR, TOK_LBRACE, TOK_IDENT, TOK_NIL,
	TOK_NUM, TOK_STRING, TOK_LBRACKET, 0
};

unsigned first_stmt[] = {
	TOK_SEMICOLON, TOK_ENDLINE, 
	TOK_IF, TOK_FOR, TOK_IDENT, TOK_LET, TOK_RETURN, 0
};
	 
unsigned
parse_endl(struct parse_s *o, struct node_s **n) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_SEMICOLON && o->lex.id != TOK_ENDLINE) {
		parse_recover(o, "';' or new line expected");
		return 0;
	}
	return 1;
}

unsigned
parse_cst(struct parse_s *o, struct node_s **n) {
	struct data_s *data;
		
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id == TOK_LPAR) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_expr)) {
			parse_recover(o, "expression expected afeter '('");
			return 0;
		}
		parse_ungetsym(o);
		if (!parse_expr(o, n)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_RPAR) {
			parse_recover(o, "missing closing ')'");
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_NUM) {
		data = data_newlong((long)o->lex.longval);
		*n = node_new(&node_vmt_cst, data);
		return 1;
	} else if (o->lex.id == TOK_STRING) {
		data = data_newstring(o->lex.strval);
		*n = node_new(&node_vmt_cst, data);
		return 1;
	} else if (o->lex.id == TOK_IDENT) {
		data = data_newref(o->lex.strval);
		*n = node_new(&node_vmt_cst, data);
		return 1;
	} else if (o->lex.id == TOK_NIL) {
		data = data_newnil();
		*n = node_new(&node_vmt_cst, data);
		return 1;
	} else if (o->lex.id == TOK_DOLLAR) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			parse_recover(o, "identifier expected after '$'");
			return 0;
		}
		*n = node_new(&node_vmt_var, data_newstring(o->lex.strval));
		return 1;
	} else if (o->lex.id == TOK_LBRACKET) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_call)) {
			parse_recover(o, "proc call expected after '['");
			return 0;
		}
		parse_ungetsym(o);
		if (!parse_call(o, n)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_RBRACKET) {
			parse_recover(o, "']' expected");
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_LBRACE) {
		*n = node_new(&node_vmt_list, NULL);
		n = &(*n)->list;
		for (;;) {
			if (!parse_getsym(o)) {
				return 0;
			}
			if (parse_isfirst(o, first_expr)) {
				parse_ungetsym(o);
				if (!parse_expr(o, n)) {
					return 0;
				}
				n = &(*n)->next;
			} else if (o->lex.id == TOK_RBRACE) {
				return 1;
			} else {
				parse_recover(o, "expression or '}' expected in list");
				return 0;
			}
		}
	}
	parse_recover(o, "bad term in expression");
	return 0;
}


unsigned
parse_unary(struct parse_s *o, struct node_s **n) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id == TOK_MINUS) {
		*n = node_new(&node_vmt_neg, NULL);
		if (!parse_unary(o, &(*n)->list)) {
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_EXCLAM) {
		*n = node_new(&node_vmt_not, NULL);
		if (!parse_unary(o, &(*n)->list)) {
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_TILDE) {
		*n = node_new(&node_vmt_bitnot, NULL);
		if (!parse_unary(o, &(*n)->list)) {
			return 0;
		}
		return 1;
	} else {
		parse_ungetsym(o);
	}
	return parse_cst(o, n);
}


unsigned
parse_muldiv(struct parse_s *o, struct node_s **n) {
	if (!parse_unary(o, n)) {
		return 0;
	}	
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_STAR) {
			node_replace(n, node_new(&node_vmt_mul, NULL));
			if (!parse_unary(o, &(*n)->list->next)) {
				return 0;
			}
		} else if (o->lex.id == TOK_SLASH) {
			node_replace(n, node_new(&node_vmt_div, NULL));
			if (!parse_unary(o, &(*n)->list->next)) {
				return 0;
			}
		} else if (o->lex.id == TOK_PCT) {
			node_replace(n, node_new(&node_vmt_mod, NULL));
			if (!parse_unary(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_addsub(struct parse_s *o, struct node_s **n) {
	if (!parse_muldiv(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_PLUS) {
			node_replace(n, node_new(&node_vmt_add, NULL));
			if (!parse_muldiv(o, &(*n)->list->next)) {
				return 0;
			}
		} else if (o->lex.id == TOK_MINUS) {
			node_replace(n, node_new(&node_vmt_sub, NULL));
			if (!parse_muldiv(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_shift(struct parse_s *o, struct node_s **n) {
	if (!parse_addsub(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_LSHIFT) {
			node_replace(n, node_new(&node_vmt_lshift, NULL));
			if (!parse_addsub(o, &(*n)->list->next)) {
				return 0;
			}
		} else if (o->lex.id == TOK_RSHIFT) {
			node_replace(n, node_new(&node_vmt_rshift, NULL));
			if (!parse_addsub(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_compare(struct parse_s *o, struct node_s **n) {
	if (!parse_shift(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_LT) {
			node_replace(n, node_new(&node_vmt_lt, NULL));
			if (!parse_shift(o, &(*n)->list->next)) {
				return 0;
			}
		} else if (o->lex.id == TOK_LE) {
			node_replace(n, node_new(&node_vmt_le, NULL));
			if (!parse_shift(o, &(*n)->list->next)) {
				return 0;
			}
		} else if (o->lex.id == TOK_GT) {
			node_replace(n, node_new(&node_vmt_gt, NULL));
			if (!parse_shift(o, &(*n)->list->next)) {
				return 0;
			}
		} else if (o->lex.id == TOK_GE) {
			node_replace(n, node_new(&node_vmt_ge, NULL));
			if (!parse_shift(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_equal(struct parse_s *o, struct node_s **n) {
	if (!parse_compare(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_EQ) {
			node_replace(n, node_new(&node_vmt_eq, NULL));
			if (!parse_compare(o, &(*n)->list->next)) {
				return 0;
			}
		} else if (o->lex.id == TOK_NEQ) {
			node_replace(n, node_new(&node_vmt_neq, NULL));
			if (!parse_compare(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_bitand(struct parse_s *o, struct node_s **n) {
	if (!parse_equal(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_BITAND) {
			node_replace(n, node_new(&node_vmt_bitand, NULL));
			if (!parse_equal(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_bitxor(struct parse_s *o, struct node_s **n) {
	if (!parse_bitand(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_BITXOR) {
			node_replace(n, node_new(&node_vmt_bitxor, NULL));
			if (!parse_bitand(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_bitor(struct parse_s *o, struct node_s **n) {
	if (!parse_bitxor(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_BITOR) {
			node_replace(n, node_new(&node_vmt_bitor, NULL));
			if (!parse_bitxor(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}


unsigned
parse_and(struct parse_s *o, struct node_s **n) {
	if (!parse_bitor(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_AND) {
			node_replace(n, node_new(&node_vmt_and, NULL));
			if (!parse_bitor(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}

unsigned
parse_or(struct parse_s *o, struct node_s **n) {
	if (!parse_and(o, n)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_OR) {
			node_replace(n, node_new(&node_vmt_or, NULL));
			if (!parse_and(o, &(*n)->list->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	return 1;
}

unsigned
parse_expr(struct parse_s *o, struct node_s **n) {
	return parse_or(o, n);
}

unsigned
parse_call(struct parse_s *o, struct node_s **n) {
	if (!parse_getsym(o)) {
		return 0;
	} 
	if (o->lex.id != TOK_IDENT) {
		return 0;
	}
	*n = node_new(&node_vmt_call, data_newref(o->lex.strval));
	n = &(*n)->list;
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_expr)) {
			parse_ungetsym(o);
			return 1;
		}
		parse_ungetsym(o);
		if (!parse_expr(o, n)) {
			return 0;
		}
		n = &(*n)->next;
	}
	return 0; /* aviod compiler warning */
}

unsigned
parse_stmt(struct parse_s *o, struct node_s **n) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (parse_isfirst(o, first_call)) {
		parse_ungetsym(o);
		*n = node_new(&node_vmt_ignore, NULL);
		if (!parse_call(o, &(*n)->list)) {
			return 0;
		}
		if (!parse_endl(o, NULL)) {
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_IF) {
		*n = node_new(&node_vmt_if, NULL);
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_expr)) {
			parse_recover(o, "expression expected after 'if'");
			return 0;
		}
		parse_ungetsym(o);
		if (!parse_expr(o, &(*n)->list)) {
			return 0;
		}
		if (!parse_slist(o, &(*n)->list->next)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_ELSE) {
			if (!parse_slist(o, &(*n)->list->next->next)) {
				return 0;
			}
		} else {
			parse_ungetsym(o);
		}
		if (!parse_endl(o, NULL)) {
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_FOR) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			parse_recover(o, "identifier expected in 'for' loop");
			return 0;
		}
		*n = node_new(&node_vmt_for, data_newstring(o->lex.strval));
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IN) {
			parse_recover(o, "'in' expected");
			return 0;
		}
		if (!parse_expr(o, &(*n)->list)) {
			return 0;
		}
		if (!parse_slist(o, &(*n)->list->next)) {
			return 0;
		}
		if (!parse_endl(o, NULL)) {
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_RETURN) {
		*n = node_new(&node_vmt_return, NULL);
		if (!parse_expr(o, &(*n)->list)) {
			return 0;
		}
		if (!parse_endl(o, NULL)) {
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_LET) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			parse_recover(o, "identifier expected in the lhs of '='");
			return 0;
		}
		*n = node_new(&node_vmt_assign, data_newstring(o->lex.strval));
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_ASSIGN) {
			parse_recover(o, "'=' expected");
			return 0;
		}	
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_expr)) {
			parse_recover(o, "expression expected after '='");
			return 0;
		}
		parse_ungetsym(o);
		if (!parse_expr(o, &(*n)->list)) {
			return 0;
		}
		if (!parse_endl(o, NULL)) {
			return 0;
		}
		return 1;
	} else  if (parse_isfirst(o, first_endl)) {
		parse_ungetsym(o);
		*n = node_new(&node_vmt_nop, NULL);
		if (!parse_endl(o, NULL)) {
			return 0;
		}
		return 1;
	}
	parse_recover(o, "bad statement");
	return 0;
}

unsigned
parse_slist(struct parse_s *o, struct node_s **n) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		parse_recover(o, "'{' expected");
		return 0;
	}
	*n = node_new(&node_vmt_slist, NULL);
	n = &(*n)->list;
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_RBRACE) {
			break;
		}
		parse_ungetsym(o);
		if (!parse_stmt(o, n)) {
			return 0;
		}
		n = &(*n)->next;
	}
	return 1;
}

unsigned
parse_alist(struct parse_s *o, struct node_s **n) {
	*n = node_new(&node_vmt_alist, NULL);
	n = &(*n)->list;
	for(;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			parse_ungetsym(o);
			break;
		}
		*n = node_new(&node_vmt_cst, data_newref(o->lex.strval));
		n = &(*n)->next;
	}	
	return 1;
}

unsigned
parse_proc(struct parse_s *o, struct node_s **n) {	
	struct node_s **a;
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_PROC) {
		parse_recover(o, "'proc' keyword expected");
		return 0;
	}
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		parse_recover(o, "proc name expected");
		return 0;
	}
	*n = node_new(&node_vmt_proc, data_newref(o->lex.strval));
	(*n)->list = node_new(&node_vmt_alist, NULL);
	a = &(*n)->list->list;
	for(;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_IDENT) {
			*a = node_new(&node_vmt_cst, data_newref(o->lex.strval));
			a = &(*a)->next;
		} else if (o->lex.id == TOK_LBRACE) {
			parse_ungetsym(o);
			break;
		} else {
			parse_ungetsym(o);
			parse_recover(o, "argument name or block expected");
			return 0;
		}
	}	
	if (!parse_slist(o, &(*n)->list->next)) {
		return 0;
	}
	if (!parse_endl(o, NULL)) {
		return 0;
	}
	return 1;
}	

unsigned
parse_line(struct parse_s *o, struct node_s **n) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (parse_isfirst(o, first_proc)) {
		parse_ungetsym(o);
		if (!parse_proc(o, n)) {
			return 0;
		}
	} else if (parse_isfirst(o, first_stmt)) {
		parse_ungetsym(o);
		if (!parse_stmt(o, n)) {
			return 0;
		}
	} else {
		parse_recover(o, "statement or proc definition expected");
		return 0;
	}
	return 1;
}
	
unsigned
parse_prog(struct parse_s *o, struct node_s **n) {
	*n = node_new(&node_vmt_slist, NULL);
	n = &(*n)->list;
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_EOF) {
			return 1;
		}
		parse_ungetsym(o);
		if (!parse_line(o, n)) {
			return 0;
		}
		n = &(*n)->next;
	}
	return 1;
}

