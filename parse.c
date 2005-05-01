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
 * a simple LL(1)-like parser.
 *
 */

#include "dbg.h"
#include "textio.h"
#include "lex.h"
#include "data.h"
#include "tree.h"
#include "parse.h"

unsigned parse_debug = 0;

/* ------------------------------------------------------------- */

void
parse_error(struct parse_s *o, char *str) {
	lex_error(&o->lex, str);
}

struct parse_s *
parse_new(char *filename) {
	struct parse_s *o;
	o = mem_alloc(sizeof(struct parse_s));	
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
	if (o->lookavail) {
		dbg_puts("parse_done: lookahead token still available\n");
	}
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

unsigned first_end[] = {
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

	/*
	 * END:		';' | '\n'
	 */
	 
unsigned
parse_end(struct parse_s *o) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_SEMICOLON && o->lex.id != TOK_ENDLINE) {
		parse_error(o, "';' or new line expected\n");
		return 0;
	}
	return 1;
}

	/*
	 * ARRAY:	'{' EXPR EXPR ... EXPR '}'
	 */

unsigned
parse_array(struct parse_s *o, struct node_s **nextptr) {
	struct tree_s tree;
	struct node_s *node;

	if (!parse_getsym(o)) {
		goto bad1;
	} 
	if (o->lex.id != TOK_LBRACE) {
		parse_error(o, "'{' expected at the begging of list\n");
		goto bad1;
	}
	tree_init(&tree);
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (parse_isfirst(o, first_expr)) {
			parse_ungetsym(o);
			if (!parse_expr(o, &node)) {
				goto bad2;
			}
			tree_add(&tree, node);
		} else if (o->lex.id == TOK_RBRACE) {
			*nextptr = node_newlist(&tree);
			return 1;
		} else {
			parse_error(o, "'}' expected at end of list\n");
			goto bad2;
		}
	}
bad2:	tree_done(&tree);
bad1:	return 0;
}	 

	/*
	 * CONST: NUM | STRING | IDENT | '(' EXPR ')' | '[' CALL ']'
	 */

unsigned
parse_const(struct parse_s *o, struct node_s **nextptr) {
	struct data_s *data;
		
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id == TOK_LPAR) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_expr)) {
			parse_error(o, "expression expected afeter '('\n");
			return 0;
		}
		parse_ungetsym(o);
		if (!parse_expr(o, nextptr)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			node_delete(*nextptr);
			return 0;
		}
		if (o->lex.id != TOK_RPAR) {
			parse_error(o, "missing closing ')'\n");
			node_delete(*nextptr);
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_NUM) {
		data = data_newlong((long)o->lex.longval);
		*nextptr = node_newconst(data);
		return 1;
	} else if (o->lex.id == TOK_STRING) {
		data = data_newstring(o->lex.strval);
		*nextptr = node_newconst(data);
		return 1;
	} else if (o->lex.id == TOK_IDENT) {
		data = data_newref(o->lex.strval);
		*nextptr = node_newconst(data);
		return 1;
	} else if (o->lex.id == TOK_NIL) {
		data = data_newnil();
		*nextptr = node_newconst(data);
		return 1;
	} else if (o->lex.id == TOK_DOLLAR) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id != TOK_IDENT) {
			parse_error(o, "identifier expected after '$'\n");
			return 0;
		}
		*nextptr = node_newvar(str_new(o->lex.strval));
		return 1;	
	} else if (o->lex.id == TOK_LBRACKET) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_call)) {
			parse_error(o, "function call expected after '['\n");
			return 0;
		}
		parse_ungetsym(o);
		if (!parse_call(o, nextptr)) {
			return 0;
		}
		if (!parse_getsym(o)) {
			node_delete(*nextptr);
			return 0;
		}
		if (o->lex.id != TOK_RBRACKET) {
			node_delete(*nextptr);
			parse_error(o, "']' expected\n");
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_LBRACE) {
		parse_ungetsym(o);
		if (!parse_array(o, nextptr)) {
			return 0;
		}
		return 1;
	}
	parse_error(o, "expression expected\n");
	parse_ungetsym(o);
	return 0;
}

	/*
	 * NEG:		PARNUM | '-' NEG
	 */

unsigned
parse_unary(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *rhs;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id == TOK_MINUS) {
		if (!parse_unary(o, &rhs)) {
			return 0;
		}
		*nextptr = node_newop(NODE_NEG, rhs, 0);
		return 1;
	} else if (o->lex.id == TOK_EXCLAM) {
		if (!parse_unary(o, &rhs)) {
			return 0;
		}
		*nextptr = node_newop(NODE_NOT, rhs, 0);
		return 1;
	} else if (o->lex.id == TOK_TILDE) {
		if (!parse_unary(o, &rhs)) {
			return 0;
		}
		*nextptr = node_newop(NODE_BITNOT, rhs, 0);
		return 1;
	} else {
		parse_ungetsym(o);
	}
	return parse_const(o, nextptr);
}

	/*
	 * MULDIV:	NEG [ {'*'|'/'} NEG ... {'*'|'/'} NEG ]
	 */

unsigned
parse_muldiv(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_unary(o, &lhs)) {
		return 0;
	}	
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_STAR) {
			if (!parse_unary(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_MUL, lhs, rhs);
		} else if (o->lex.id == TOK_SLASH) {
			if (!parse_unary(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_DIV, lhs, rhs);
		} else if (o->lex.id == TOK_PCT) {
			if (!parse_unary(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_MOD, lhs, rhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}

	/*
	 * ADDSUB:	MULDIV [ {'*'|'/'} MULDIV ... {'*'|'/'} MULDIV ]
	 */


unsigned
parse_addsub(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_muldiv(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_PLUS) {
			if (!parse_muldiv(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_ADD, lhs, rhs);
		} else if (o->lex.id == TOK_MINUS) {
			if (!parse_muldiv(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_SUB, lhs, rhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}

	/*
	 * SHIFT:	ADDSUB [ {'<<'|'>>'} ADDSUB ... {'<<'|'>>'} ADDSUB ]
	 */


unsigned
parse_shift(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_addsub(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_LSHIFT) {
			if (!parse_addsub(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_LSHIFT, lhs, rhs);
		} else if (o->lex.id == TOK_RSHIFT) {
			if (!parse_addsub(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_RSHIFT, lhs, rhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}


	/*
	 * COMPARE:	SHIFT [ {'<'|'>'|'<='|'=>'} SHIFT ...  ADDSUB ]
	 */


unsigned
parse_compare(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_shift(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_LT) {
			if (!parse_shift(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_LT, lhs, rhs);
		} else if (o->lex.id == TOK_LE) {
			if (!parse_shift(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_LE, lhs, rhs);
		} else if (o->lex.id == TOK_GT) {
			if (!parse_shift(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_LE, rhs, lhs);
		} else if (o->lex.id == TOK_GE) {
			if (!parse_shift(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_LT, rhs, lhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}



	/*
	 * EQUAL:	COMPARE [ {'=='|'!=' COMPARE...   ]
	 */


unsigned
parse_equal(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_compare(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_EQ) {
			if (!parse_compare(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_EQ, lhs, rhs);
		} else if (o->lex.id == TOK_NEQ) {
			if (!parse_compare(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_EQ, lhs, rhs);
			lhs = node_newop(NODE_NOT, lhs, 0);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}

	/*
	 * BITAND:	EQUAL [ '&' EQUAL...   ]
	 */

unsigned
parse_bitand(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_equal(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_BITAND) {
			if (!parse_equal(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_BITAND, lhs, rhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}

	/*
	 * BITXOR:	BITAND [ '^' BITAND...   ]
	 */


unsigned
parse_bitxor(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_bitand(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_BITXOR) {
			if (!parse_bitand(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_BITXOR, lhs, rhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}



	/*
	 * BITOR:	BITXOR [ '|' BITXOR...   ]
	 */


unsigned
parse_bitor(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_bitxor(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_BITOR) {
			if (!parse_bitxor(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_BITOR, lhs, rhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}



	/*
	 * AND:		BITOR [ '&&' BITOR...   ]
	 */


unsigned
parse_and(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_bitor(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_AND) {
			if (!parse_bitor(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_AND, lhs, rhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}



	/*
	 * OR:		AND [ '||' AND...   ]
	 */


unsigned
parse_or(struct parse_s *o, struct node_s **nextptr) {
	struct node_s *lhs, *rhs;
	
	if (!parse_and(o, &lhs)) {
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_OR) {
			if (!parse_and(o, &rhs)) {
				node_delete(lhs);
				return 0;
			}
			lhs = node_newop(NODE_OR, lhs, rhs);
		} else {
			parse_ungetsym(o);
			break;
		}
	}
	*nextptr = lhs;
	return 1;
}

	/*
	 * EXPR:	OR
	 */

unsigned
parse_expr(struct parse_s *o, struct node_s **nextptr) {
	return parse_or(o, nextptr);
}


unsigned
parse_call(struct parse_s *o, struct node_s **nextptr) {
	struct tree_s tree;
	struct node_s *node;
	char *name;

	if (!parse_getsym(o)) {
		return 0;
	} 
	if (o->lex.id != TOK_IDENT) {
		return 0;
	}
	name = str_new(o->lex.strval);
	tree_init(&tree);
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_expr)) {
			parse_ungetsym(o);
			*nextptr = node_newcall(name, &tree);
			return 1;
		}
		parse_ungetsym(o);
		if (!parse_expr(o, &node)) {
			str_delete(name);
			tree_done(&tree);
			return 0;
		}
		tree_add(&tree, node);
	}
	return 0; /* aviod compiler warning */
}

	/*
	 * ASSIGN:	EXPR '=' EXPR
	 */

unsigned
parse_assign(struct parse_s *o, struct tree_s *parent) {
	char *lhs;
	struct node_s *rhs;

	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LET) {
		parse_error(o, "'let' expected in assignement\n");
		return 0;
	}
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		parse_error(o, "identifier expected in the lhs of '='\n");
		return 0;
	}
	lhs = str_new(o->lex.strval);
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_ASSIGN) {
		parse_error(o, "'=' expected\n");
		str_delete(lhs);
		return 0;
	}	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (!parse_isfirst(o, first_expr)) {
		dbg_puts("expression expected after '='\n");
		str_delete(lhs);
		return 0;
	}
	parse_ungetsym(o);
	if (!parse_expr(o, &rhs)) {
		str_delete(lhs);
		return 0;
	}
	if (!parse_end(o)) {
		return 0;
	}
	
	tree_add(parent, node_newassign(lhs, rhs));
	return 1;
}

	/*
	 * FOR:	IDENT 'in' BLOCK
	 */

unsigned
parse_for(struct parse_s *o, struct tree_s *parent) {
	char *name;
	struct node_s *node;
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		parse_error(o, "identifier expected in 'for' loop\n");
		return 0;
	}
	name = str_new(o->lex.strval);
	if (!parse_getsym(o)) {
		str_delete(name);
		return 0;
	}
	if (o->lex.id != TOK_IN) {
		parse_error(o, "'in' expected\n");
		str_delete(name);
		return 0;
	}
	if (!parse_expr(o, &node)) {
		str_delete(name);
		return 0;
	}
	node = node_newloop(name, node);
	if (!parse_block(o, &node->param.loop.stmt)) {
		node_delete(node);
		return 0;
	}
	tree_add(parent, node);
	return 1;
}

	/*
	 * STMT:	';' |
	 *		'return' EXPR ';' |
	 *		'if' EXPR BLOCK [ 'else' BLOCK ] |
	 *		'for' EXPR 'in' BLOCK |
	 *		'let' ASSIGN ';' |
	 *		IDENT [ EXPR ... EXPR ] ';' |
	 *		BLOCK
	 */

unsigned
parse_stmt(struct parse_s *o, struct tree_s *parent) {
	struct node_s *node;

	if (!parse_getsym(o)) {
		return 0;
	}
	if (parse_isfirst(o, first_call)) {
		parse_ungetsym(o);
		if (!parse_call(o, &node)) {
			return 0;
		}
		if (!parse_end(o)) {
			node_delete(node);
			return 0;
		}
		tree_add(parent, node);
		return 1;
	} else if (o->lex.id == TOK_IF) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (!parse_isfirst(o, first_expr)) {
			parse_error(o, "expression expected after 'if'\n");
			return 0;
		}
		parse_ungetsym(o);
		if (!parse_expr(o, &node)) {
			return 0;
		}
		node = node_newcond(node);
		if (!parse_block(o, &node->param.cond.stmt1)) {
			node_delete(node);
			return 0;
		}
		if (!parse_getsym(o)) {
			node_delete(node);
			return 0;
		}
		if (o->lex.id == TOK_ELSE) {
			if (!parse_block(o, &node->param.cond.stmt2)) {
				node_delete(node);
				return 0;
			}
			node->id = NODE_IFELSE;
			tree_add(parent, node);
			return 1;
		} else {
			parse_ungetsym(o);
			node->id = NODE_IF;
			tree_add(parent, node);
			return 1;
		}
	} else if (o->lex.id == TOK_FOR) {
		if (!parse_for(o, parent)) {
			return 0;
		}
		return 1;
	} else if (o->lex.id == TOK_RETURN) {
		if (!parse_expr(o, &node)) {
			return 0;
		}
		node = node_newop(NODE_RETURN, node, 0);
		if (!parse_end(o)) {
			node_delete(node);
			return 0;
		}
		tree_add(parent, node);
		return 1;
	} else if (o->lex.id == TOK_LET) {
		parse_ungetsym(o);
		return parse_assign(o, parent);
	} else  if (parse_isfirst(o, first_end)) {
		parse_ungetsym(o);
		if (!parse_end(o)) {
			return 0;
		}
		return 1;
	}

	parse_error(o, "bad statement\n");
	return 0;
}

	/*
	 * BLOCK:	'{' STMT ... '}'
	 */
	
unsigned
parse_block(struct parse_s *o, struct tree_s *parent) {
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_LBRACE) {
		parse_error(o, "'{' expected\n");
		return 0;
	}
	for (;;) {
		if (!parse_getsym(o)) {
			return 0;
		}
		if (o->lex.id == TOK_RBRACE) {
			break;
		}
		parse_ungetsym(o);
		if (!parse_stmt(o, parent)) {
			return 0;
		}
	}
	return 1;
}

	/*
	 * FUNCTION:	'proc' IDENT [ IDENT ... IDENT ] BLOCK
	 */

unsigned
parse_function(struct parse_s *o, struct proc_s **nextptr) {
	struct proc_s *proc;
	
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_PROC) {
		parse_error(o, "'proc' keyword expected\n");
		return 0;
	}
	if (!parse_getsym(o)) {
		return 0;
	}
	if (o->lex.id != TOK_IDENT) {
		parse_error(o, "function name expected\n");
		return 0;
	}
	proc = proc_new(o->lex.strval);
	for(;;) {
		if (!parse_getsym(o)) {
			proc_delete(proc);
			return 0;
		}
		if (o->lex.id == TOK_IDENT) {
			name_add(&proc->args, name_new(o->lex.strval));
		} else if (o->lex.id == TOK_LBRACE) {
			parse_ungetsym(o);
			break;
		} else {
			parse_error(o, "argument name expected\n");
			proc_delete(proc);
			return 0;
		}
	}
	if (!parse_block(o, &proc->code.tree)) {
		proc_delete(proc);
		return 0;
	}
	*nextptr = proc;
	return 1;
}
	
	/*
	 * PROG:	[ { FUNCTION | STMT | 'eof' } ... ]
	 */

unsigned
parse_prog(struct parse_s *o, struct exec_s *exec) {
	struct tree_s start;
	struct proc_s *newp, *oldp;
	
	tree_init(&start);

	for (;;) {
		textin_setprompt(o->lex.in, "> ");
		if (!parse_getsym(o)) {
			tree_done(&start);
			return 0;
		}
		textin_setprompt(o->lex.in, "! ");
		if (o->lex.id == TOK_EOF) {
			/* end of file, finish */
			tree_done(&start);
			return 1;
		}
		if (parse_isfirst(o, first_proc)) {
			parse_ungetsym(o);
			if (!parse_function(o, &newp)) {
				goto recover;
			}
			oldp = exec_proclookup(exec, newp->name.str);
			if (oldp) {
				name_remove((struct name_s **)&exec->procs, (struct name_s *)oldp);
				proc_delete(oldp);
			}
			if (parse_debug) {
				proc_dbg(newp);
			}
			name_insert((struct name_s **)&exec->procs, (struct name_s *)newp);
			continue;
		} else if (parse_isfirst(o, first_stmt)) {
			parse_ungetsym(o);
			if (!parse_stmt(o, &start)) {
				goto recover;
			}
			if (tree_debug) {
				tree_dbg(&start, 0);
			}
			if (!tree_exec(&start, exec)) {
				tree_done(&start);
				return 0;
			}
			/* restart */
			data_delete(exec_getacc(exec));
			tree_empty(&start);
			continue;
		} else {
			parse_error(o, "statement or function definition expected\n");
			goto recover;
		}
	}
		
recover:
	while (o->lex.id != TOK_ENDLINE && o->lex.id != TOK_EOF) {
		if (!parse_getsym(o)) {
			tree_done(&start);
			return 0;
		}
	}
	tree_done(&start);
	return 0;
}
