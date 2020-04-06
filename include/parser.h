#pragma once

#include "type.h"
#include "vec.h"

enum expr_type {
	EXPR_NUMBER, EXPR_STRING, EXPR_NAME,
	EXPR_CALL, EXPR_CAST,

	// Unary prefix
	EXPR_DEREF,

	// Binary
	EXPR_ADD, EXPR_SUB, EXPR_MUL, EXPR_DIV,
	EXPR_AND, EXPR_OR, EXPR_SHL, EXPR_SHR,
	// Binary equals
	EXPR_EQ, EXPR_NEQ, EXPR_LT, EXPR_GT, EXPR_LTE, EXPR_GTE,
	// Binary assign
	EXPR_ASSIGN,
};
struct expr {
	enum expr_type expr_type;
	struct expr* parent;
	int type;

	long number;
	char* name;
	struct expr* unop;
	struct {
		struct expr* call;
		struct vec* args;
	} call;
	struct {
		struct expr* left;
		struct expr* right;
	} binop;
};

enum stmt_type {
	STMT_COMPOUND,
	STMT_IF,
	STMT_WHILE,
	STMT_RETURN,
	STMT_EXPR,
	STMT_NOOP,
};
struct stmt {
	enum stmt_type stmt_type;
	union {
		struct {
			struct symtable* st;
			struct vec* stmts;
		} compound;
		struct {
			struct expr* cond;
			struct stmt* _true;
			struct stmt* _false;
		} _if;
		struct {
			struct expr* cond;
			struct stmt* stmt;
		} _while;
		struct expr* expr;
	};
};

struct func {
	int type;
	char* name;
	struct symtable* st;
	struct stmt* stmt;
};

struct lib {
	struct symtable* st;
	struct vec* funcs;
};

int    parse_type();
struct expr* parse_primary_expr(struct symtable*);
struct expr* parse_postfix_expr(struct symtable*);
struct expr* parse_unary_expr(struct symtable*);
struct expr* parse_cast_expr(struct symtable*);
struct expr* parse_mul_expr(struct symtable*);
struct expr* parse_add_expr(struct symtable*);
struct expr* parse_shift_expr(struct symtable*);
struct expr* parse_rel_expr(struct symtable*);
struct expr* parse_eq_expr(struct symtable*);
struct expr* parse_assign_expr(struct symtable*);
struct expr* parse_expr(struct symtable*);
struct stmt* parse_compound_stmt(struct symtable*);
struct stmt* parse_sel_stmt(struct symtable*);
struct stmt* parse_iter_stmt(struct symtable*);
struct stmt* parse_jump_stmt(struct symtable*);
struct stmt* parse_decl_stmt(struct symtable*);
struct stmt* parse_expr_stmt(struct symtable*);
struct stmt* parse_stmt(struct symtable*);
void         parse_ext_func(struct symtable*);
struct func* parse_func(struct symtable*);

struct lib*  parse(struct vec*);
