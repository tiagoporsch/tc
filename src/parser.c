#include "parser.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "sym.h"
#include "token.h"
#include "vec.h"

static struct vec* tokens;
static int current_token = 0;

extern char* input_filename;
static void error(struct token* t, const char* format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, "%s:%d:%d: error: ", input_filename, t->line, t->line_index);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(1);
}

static void type_error(struct token* t, int old, int new) {
	error(t, "can't convert %s to %s.\n",
			strdup(type_tostr(old)), strdup(type_tostr(new)));
}

static void token_error(struct token* t, int expected) {
	if (expected != 0) {
		error(t, "expected '%s' (%d), got '%s' (%d).\n", token_type_tostr(expected),
				expected, strdup(token_type_tostr(t->token)), t->token);
	} else {
		error(t, "invalid token '%s'.\n", token_type_tostr(t->token));
	}
}

static struct token* prev() {
	return tokens->data[--current_token];
}

static struct token* next() {
	return tokens->data[current_token++];
}

static struct token* peek() {
	return tokens->data[current_token];
}

static struct token* lookahead(int n) {
	return tokens->data[current_token + n];
}

static bool optional(int token) {
	if (peek()->token == token)
		 return next() != NULL;
	return false;
}

static struct token* expect(int token) {
	struct token* t = next();
	if (t->token != token)
		token_error(t, token);
	return t;
}

/*
 * Expressions
 */
struct expr* expr_scale(struct expr* e, int factor) {
	struct expr* x = malloc(sizeof(struct expr));
	x->expr_type = EXPR_MUL;
	x->binop.left = e;
	x->binop.left->parent = x;
	x->binop.right = malloc(sizeof(struct expr));
	x->binop.right->parent = x;
	x->binop.right->expr_type = EXPR_NUMBER;
	x->binop.right->number = factor;
	return x;
}

// type
//	: s0 | u0 | s1 | u1 | s8 | u8 | s16 | u16 | s32 | u32 | s64 | u64
//	| type '*'
int parse_type() {
	int t = expect(T_TYPE)->type;
	while (optional('*'))
		t = type_toptr(t);
	return t;
}

// primary_expr
//	: NAME
//	| NUMBER
//	| STRING
//	| '(' expr ')'
struct expr* parse_primary_expr(struct symtable* st) {
	struct expr* e = malloc(sizeof(struct expr));
	switch (peek()->token) {
		case T_NAME: {
			e->expr_type = EXPR_NAME;
			e->name = expect(T_NAME)->name;
			struct sym* s = sym_get(st, e->name);
			if (s == NULL)
				error(prev(), "couldn't find variable '%s'.\n", e->name);
			e->type = s->type;
			} break;
		case T_NUMBER:
			e->expr_type = EXPR_NUMBER;
			e->number = expect(T_NUMBER)->number;
			e->type = type_fromint(e->number);
			break;
		case T_STRING: {
			static long string_count = 0;
			struct sym* s = sym_alloc(SYM_STRING);
			s->offset = (int) string_count;
			s->name = expect(T_STRING)->name;
			sym_put(symtable_get_root(st), s);

			e->expr_type = EXPR_STRING;
			e->number = string_count++;
			e->name = s->name;
			e->type = type_toptr(TYPE_8 | TYPE_SIGNED);
			} break;
		case '(':
			expect('(');
			e = parse_expr(st);
			expect(')');
			break;
		default:
			token_error(peek(), 0);
			break;
	}
	return e;
}

// postfix_expr
//	: primary_expr
//	| postfix_expr '[' expr ']'
//	| postfix_expr '(' assign_expr? (',' assign_expr)* ')'
struct expr* parse_postfix_expr(struct symtable* st) {
	struct expr* e = parse_primary_expr(st);
	if (optional('(')) {
		if (e->expr_type != EXPR_NAME)
			token_error(prev(), T_NAME);
		struct expr* call = e;
		e = malloc(sizeof(struct expr));
		struct sym* s = sym_get(st, call->name);
		e->type = s->type;
		e->expr_type = EXPR_CALL;
		e->call.call = call;
		e->call.call->parent = e;
		e->call.args = vec_alloc();
		for (int i = 0; i < s->param_count; i++) {
			struct expr* arg = parse_assign_expr(st);
			if (!type_fits(arg->type, s->params[i]))
				type_error(prev(), arg->type, s->params[i]);
			vec_push(e->call.args, arg);
			if (i != s->param_count - 1)
				expect(',');
		}
		expect(')');
	} else if (optional('[')) {
		if (e->expr_type != EXPR_NAME)
			token_error(prev(), T_NAME);
		struct expr* left = e;
		e = malloc(sizeof(struct expr));
		e->expr_type = EXPR_DEREF;
		e->type = type_fromptr(left->type);
		e->unop = malloc(sizeof(struct expr));
		e->unop->parent = e;
		e->unop->expr_type = EXPR_ADD;
		e->unop->type = left->type;
		e->unop->binop.left = left;
		e->unop->binop.left->parent = e->unop;
		e->unop->binop.right = expr_scale(parse_expr(st), type_getsize(e->type));
		e->unop->binop.right->parent = e->unop;
		expect(']');
	}
	return e;
}

// unary_expr
//	: postfix_expr
//	| '*' cast_expr
//	| 'sizeof' type
struct expr* parse_unary_expr(struct symtable* st) {
	struct expr* e;
	switch (peek()->token) {
		case '*':
			expect('*');
			e = malloc(sizeof(struct expr));
			e->expr_type = EXPR_DEREF;
			e->unop = parse_cast_expr(st);
			e->unop->parent = e;
			e->type = type_fromptr(e->unop->type);
			break;
		case T_SIZEOF:
			expect(T_SIZEOF);
			e = malloc(sizeof(struct expr));
			e->expr_type = EXPR_NUMBER;
			e->number = type_getsize(parse_type());
			e->type = type_fromint(e->number);
			break;
		default:
			e = parse_postfix_expr(st);
			break;
	}
	return e;
}

// cast_expr
//	: unary_expr
//	| '(' type ')' cast_expr
struct expr* parse_cast_expr(struct symtable* st) {
	struct expr* e;
	if (peek()->token == '(' && lookahead(1)->token == T_TYPE) {
		expect('(');
		e = malloc(sizeof(struct expr));
		e->expr_type = EXPR_CAST;
		e->type = parse_type();
		expect(')');
		e->unop = parse_cast_expr(st);
		e->unop->parent = e;
	} else {
		e = parse_unary_expr(st);
	}
	return e;
}

// mul_expr
//	: cast_expr
//	| mul_expr '*' cast_expr
//	| mul_expr '/' cast_expr
struct expr* parse_mul_expr(struct symtable* st) {
	struct expr* e = parse_cast_expr(st);
	while (peek()->token == '*' ||
			peek()->token == '/') {
		struct token* t = next();
		struct expr* left = e;
		e = malloc(sizeof(struct expr));
		if (t->token == '*') e->expr_type = EXPR_MUL;
		else if (t->token == '/') e->expr_type = EXPR_DIV;
		e->binop.left = left;
		e->binop.left->parent = e;
		e->binop.right = parse_cast_expr(st);
		e->binop.right->parent = e;
		e->type = type_bigger(e->binop.left->type, e->binop.right->type);
	}
	return e;
}

// add_expr
//	: mul_expr
//	| add_expr '+' mul_expr
//	| add_expr '-' mul_expr
struct expr* parse_add_expr(struct symtable* st) {
	struct expr* e = parse_mul_expr(st);
	while (peek()->token == '+' ||
			peek()->token == '-') {
		struct token* t = next();
		struct expr* left = e;
		e = malloc(sizeof(struct expr));
		if (t->token == '+') e->expr_type = EXPR_ADD;
		else if (t->token == '-') e->expr_type = EXPR_SUB;
		e->binop.left = left;
		e->binop.left->parent = e;
		e->binop.right = parse_mul_expr(st);
		e->binop.right->parent = e;
		if (type_getpointer(e->binop.left->type)) {
			if (type_getpointer(e->binop.right->type))
				error(prev(), "can't add two pointers.\n");
			e->type = e->binop.left->type;
			e->binop.right = expr_scale(e->binop.right,
					type_getsize(type_fromptr(e->type)));
			e->binop.right->parent = e;
		} else if (type_getpointer(e->binop.right->type)) {
			if (type_getpointer(e->binop.left->type))
				error(prev(), "can't add two pointers.\n");
			e->type = e->binop.right->type;
			e->binop.left = expr_scale(e->binop.left,
					type_getsize(type_fromptr(e->type)));
			e->binop.left->parent = e;
		} else {
			e->type = type_bigger(e->binop.left->type, e->binop.right->type);
		}
	}
	return e;
}

// shift_expr
//	: add_expr
//	| shift_expr '<<' add_expr
//	| shift_expr '>>' add_expr
struct expr* parse_shift_expr(struct symtable* st) {
	struct expr* e = parse_add_expr(st);
	while (peek()->token == T_SHL ||
			peek()->token == T_SHR) {
		struct token* t = next();
		struct expr* left = e;
		e = malloc(sizeof(struct expr));
		if (t->token == T_SHL) e->expr_type = EXPR_SHL;
		else if (t->token == T_SHR) e->expr_type = EXPR_SHR;
		e->binop.left = left;
		e->binop.left->parent = e;
		e->binop.right = parse_add_expr(st);
		e->binop.right->parent = e;
		e->type = e->binop.left->type;
	}
	return e;
}

// rel_expr
//	: shift_expr
//	| rel_expr '<' shift_expr
//	| rel_expr '>' shift_expr
//	| rel_expr '<=' shift_expr
//	| rel_expr '>=' shift_expr
struct expr* parse_rel_expr(struct symtable* st) {
	struct expr* e = parse_shift_expr(st);
	while (peek()->token == '<' ||
		   peek()->token == '>' ||
		   peek()->token == T_LE ||
		   peek()->token == T_GE) {
		struct token* t = next();
		struct expr* left = e;
		e = malloc(sizeof(struct expr));
		if (t->token == '<') e->expr_type = EXPR_LT;
		else if (t->token == '>') e->expr_type = EXPR_GT;
		else if (t->token == T_LE) e->expr_type = EXPR_LTE;
		else if (t->token == T_GE) e->expr_type = EXPR_GTE;
		e->binop.left = left;
		e->binop.left->parent = e;
		e->binop.right = parse_shift_expr(st);
		e->binop.right->parent = e;
		e->type = TYPE_8 | TYPE_SIGNED;
	}
	return e;
}

// eq_expr
//	: rel_expr
//	| eq_expr '==' rel_expr
//	| eq_expr '!=' rel_expr
struct expr* parse_eq_expr(struct symtable* st) {
	struct expr* e = parse_rel_expr(st);
	while (peek()->token == T_EQ ||
			peek()->token == T_NE) {
		struct token* t = next();
		struct expr* left = e;
		e = malloc(sizeof(struct expr));
		if (t->token == T_EQ) e->expr_type = EXPR_EQ;
		else if (t->token == T_NE) e->expr_type = EXPR_NEQ;
		e->binop.left = left;
		e->binop.left->parent = e;
		e->binop.right = parse_rel_expr(st);
		e->binop.right->parent = e;
		e->type = TYPE_8 | TYPE_SIGNED;
	}
	return e;
}

// and_expr
//	: eq_expr
//	| and_expr '&' eq_expr
struct expr* parse_and_expr(struct symtable* st) {
	struct expr* e = parse_eq_expr(st);
	while (optional('&')) {
		struct expr* left = e;
		e = malloc(sizeof(struct expr));
		e->expr_type = EXPR_AND;
		e->binop.left = left;
		e->binop.left->parent = e;
		e->binop.right = parse_eq_expr(st);
		e->binop.right->parent = e;
		e->type = e->binop.left->type;
	}
	return e;
}

// or_expr
//	: and_expr
//	| or_expr '|' and_expr
struct expr* parse_or_expr(struct symtable* st) {
	struct expr* e = parse_and_expr(st);
	while (optional('|')) {
		struct expr* left = e;
		e = malloc(sizeof(struct expr));
		e->expr_type = EXPR_OR;
		e->binop.left = left;
		e->binop.left->parent = e;
		e->binop.right = parse_and_expr(st);
		e->binop.right->parent = e;
		e->type = e->binop.left->type;
	}
	return e;
}

// assign_expr
//	: or_expr
//	| unary_expr '=' assign_expr
struct expr* parse_assign_expr(struct symtable* st) {
	struct expr* e = parse_or_expr(st);
	if (e->expr_type == EXPR_NAME || e->expr_type == EXPR_DEREF) {
		if (optional('=')) {
			struct expr* left = e;
			e = malloc(sizeof(struct expr));
			e->expr_type = EXPR_ASSIGN;
			e->binop.left = left;
			e->binop.left->parent = e;
			e->binop.right = parse_assign_expr(st);
			e->binop.right->parent = e;
			if (!type_fits(e->binop.right->type, e->binop.left->type))
				type_error(prev(), e->binop.left->type, e->binop.right->type);
		}
	}
	return e;
}

// expr
//	: assign_expr
struct expr* parse_expr(struct symtable* st) {
	return parse_assign_expr(st);
}

// compound_stmt
//	: '{' stmt* '}'
struct stmt* parse_compound_stmt(struct symtable* st) {
	struct stmt* s = malloc(sizeof(struct stmt));
	expect('{');
	s->stmt_type = STMT_COMPOUND;
	s->compound.st = symtable_alloc(st);
	s->compound.stmts = vec_alloc();
	while (peek()->token != '}') {
		vec_push(s->compound.stmts, parse_stmt(s->compound.st));
	}
	expect('}');
	return s;
}

// sel_stmt
//	: 'if' expr stmt ('else' stmt)?
struct stmt* parse_sel_stmt(struct symtable* st) {
	struct stmt* s = malloc(sizeof(struct stmt));
	switch (next()->token) {
		case T_IF:
			s->stmt_type = STMT_IF;
			s->_if.cond = parse_expr(st);
			s->_if._true = parse_stmt(st);
			s->_if._false = optional(T_ELSE) ? parse_stmt(st) : NULL;
			break;
		default:
			break;
	}
	return s;
}

// iter_stmt
//	: 'while' expr stmt
struct stmt* parse_iter_stmt(struct symtable* st) {
	struct stmt* s = malloc(sizeof(struct stmt));
	switch (next()->token) {
		case T_WHILE:
			s->stmt_type = STMT_WHILE;
			s->_while.cond = parse_expr(st);
			s->_while.stmt = parse_stmt(st);
			break;
		default:
			break;
	}
	return s;
}

// jump_stmt
//	: 'return' expr? ';'
struct stmt* parse_jump_stmt(struct symtable* st) {
	struct stmt* s = malloc(sizeof(struct stmt));
	switch (next()->token) {
		case T_RETURN:
			s->stmt_type = STMT_RETURN;
			if (!optional(';')) {
				s->expr = parse_expr(st);
				expect(';');
			} else {
				s->expr = NULL;
			}
			break;
		default:
			break;
	}
	return s;
}

// decl_stmt
//	: 'var' NAME ':' type ('=' assign_expr)? ';'
struct stmt* parse_decl_stmt(struct symtable* st) {
	struct sym* sym = sym_alloc(symtable_get_root(st) == st ? SYM_GLOBAL : SYM_LOCAL);
	expect(T_VAR);
	sym->name = expect(T_NAME)->name;
	expect(':');
	sym->type = parse_type();
	sym->offset = sym->sym_type == SYM_LOCAL ?
		sym_get_last_offset(st) - type_getalign(sym->type) : 0;

	struct stmt* s = malloc(sizeof(struct stmt));
	if (sym->sym_type == SYM_LOCAL && optional('=')) {
		s->stmt_type = STMT_EXPR;
		s->expr = malloc(sizeof(struct expr));
		s->expr->expr_type = EXPR_ASSIGN;
		s->expr->binop.left = malloc(sizeof(struct expr));
		s->expr->binop.left->expr_type = EXPR_NAME;
		s->expr->binop.left->type = sym->type;
		s->expr->binop.left->name = sym->name;
		s->expr->binop.right = parse_assign_expr(st);
	} else {
		s->stmt_type = STMT_NOOP;
	}
	expect(';');

	sym_put(st, sym);
	return s;
}

// expr_stmt
//	| ';'
//	| expr ';'
struct stmt* parse_expr_stmt(struct symtable* st) {
	struct stmt* s = malloc(sizeof(struct stmt));
	if (optional(';')) {
		s->stmt_type = STMT_NOOP;
	} else {
		s->stmt_type = STMT_EXPR;
		s->expr = parse_expr(st);
		expect(';');
	}
	return s;
}

// stmt
//	: compound_stmt
//	| sel_stmt
//	| iter_stmt
//	| jump_stmt
//	| decl_stmt
//	| expr_stmt
struct stmt* parse_stmt(struct symtable* st) {
	switch (peek()->token) {
		case '{': return parse_compound_stmt(st);
		case T_IF: return parse_sel_stmt(st);
		case T_WHILE: return parse_iter_stmt(st);
		case T_RETURN: return parse_jump_stmt(st);
		case T_VAR: return parse_decl_stmt(st);
		default: return parse_expr_stmt(st);
	}
}

// ext_func
//	: 'extern' 'fn' NAME '(' type? (',' type)* ')' (':' type) ';'
void parse_ext_func(struct symtable* st) {
	struct sym* fs = sym_alloc(SYM_FUNC);
	expect(T_EXTERN);
	expect(T_FN);
	fs->name = expect(T_NAME)->name;
	expect('(');
	while (peek()->token != ')') {
		sym_add_param(fs, parse_type());
		optional(',');
	}
	expect(')');
	fs->type = optional(':') ? parse_type() : TYPE_0;
	expect(';');
	sym_put(st, fs);
}

// func
//	: 'fn' NAME '(' (NAME ':' type)? (',' NAME ':' type)* ')' (':' type)? stmt
struct func* parse_func(struct symtable* st) {
	struct func* f = malloc(sizeof(struct func));
	struct sym* fs = sym_alloc(SYM_FUNC);

	expect(T_FN);
	f->name = expect(T_NAME)->name;
	fs->name = f->name;

	f->st = symtable_alloc(st);
	expect('(');
	while (peek()->token != ')') {
		struct sym* s = sym_alloc(SYM_LOCAL);
		s->name = expect(T_NAME)->name;
		expect(':');
		s->type = parse_type();
		s->offset = sym_get_last_offset(f->st) - type_getalign(s->type);
		sym_put(f->st, s);
		sym_add_param(fs, s->type);
		optional(',');
	}
	expect(')');

	f->type = optional(':') ? parse_type() : TYPE_0;
	fs->type = f->type;
	sym_put(st, fs);

	f->stmt = parse_stmt(f->st);
	return f;
}

// lib
//	: (ext_func | decl_stmt | func)*
struct lib* parse(struct vec* _tokens) {
	tokens = _tokens;

	struct lib* l = malloc(sizeof(struct lib));
	l->st = symtable_alloc(NULL);
	l->funcs = vec_alloc();
	while (peek()->token != T_EOF) {
		switch (peek()->token) {
			case T_EXTERN:
				parse_ext_func(l->st);
				break;
			case T_VAR:
				parse_decl_stmt(l->st);
				break;
			case T_FN:
				vec_push(l->funcs, parse_func(l->st));
				break;
			default:
				token_error(peek(), 0);
				break;
		}
	}
	return l;
};
