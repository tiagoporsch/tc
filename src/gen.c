#include "gen.h"

#include <stdarg.h>
#include <stdlib.h>

#include "cg.h"
#include "lexer.h"
#include "parser.h"
#include "sym.h"

static int error(const char* format, ...) {
	va_list args;
	va_start(args, format);
	printf("Code generation error: ");
	vprintf(format, args);
	va_end(args);
	exit(1);
	return -1;
}

static int gen_lvalue(struct expr* e, struct symtable* st) {
	while (e->expr_type == EXPR_DEREF)
		e = e->unop;
	int r = gen_expr(e, st);
	while (e->parent->expr_type == EXPR_DEREF) {
		if (e->parent->parent->expr_type == EXPR_DEREF)
			r = cg_load_addr(r, e->type);
		e = e->parent;
	}
	return r;
}

int gen_expr(struct expr* e, struct symtable* st) {
	enum expr_type et = e->expr_type;
	switch (et) {
		// Register loading
		case EXPR_NUMBER: return cg_load_number(e->number);
		case EXPR_STRING: return cg_load_string(e->number);
		case EXPR_NAME: return cg_load_name(e->name, st);
		case EXPR_CALL:
			cg_reg_push_used();
			for (int i = e->call.args->size - 1; i >= 0; i--) {
				int r = gen_expr(e->call.args->data[i], st);
				cg_push_arg(i, r);
				cg_reg_free(r);
			}
			return cg_call(e->call.call->name);

		// Unary prefix
		case EXPR_CAST: return cg_cast(gen_expr(e->unop, st), e->type);
//		case EXPR_ADDROF: return cg_addrof(e->name, st);
		case EXPR_DEREF: return cg_load_addr(gen_expr(e->unop, st), e->type);

		// Binary
		case EXPR_ADD: return cg_add(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_SUB: return cg_sub(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_MUL: return cg_mul(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_DIV: return cg_div(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_AND: return cg_and(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_OR: return cg_or(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_SHL: return cg_shl(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_SHR: return cg_shr(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));

		// Binary equality
		case EXPR_EQ: return cg_eq(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_NEQ: return cg_neq(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_LT: return cg_lt(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_GT: return cg_gt(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_LTE: return cg_lte(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));
		case EXPR_GTE: return cg_gte(gen_expr(e->binop.left, st), gen_expr(e->binop.right, st));

		// Binary assignment
		case EXPR_ASSIGN:
			if (e->binop.left->expr_type == EXPR_NAME) {
				cg_store_name(
					gen_expr(e->binop.right, st),
					e->binop.left->name,
					st);
				return -1;
			} else if (e->binop.left->expr_type == EXPR_DEREF) {
				cg_store_addr(
					gen_expr(e->binop.right, st),
					gen_lvalue(e->binop.left->unop, st),
					type_gettype(e->binop.left->type));
				return -1;
			}
			return error("can't assign to expr type %d\n", et);
		default:
			return error("unknown expression type '%d'.\n", et);
	}
}

void gen_stmt(struct stmt* s, struct symtable* st) {
	switch (s->stmt_type) {
		case STMT_COMPOUND:
			for (int i = 0; i < s->compound.stmts->size; i++) {
				gen_stmt(s->compound.stmts->data[i], s->compound.st);
			}
			break;
		case STMT_IF:
			if (s->_if._false) {
				int lelse = cg_new_label();
				int lend = cg_new_label();
				cg_jmp_if_false(lelse, gen_expr(s->_if.cond, st));
				gen_stmt(s->_if._true, st);
				cg_jmp(lend);
				cg_decl_label(lelse);
				gen_stmt(s->_if._false, st);
				cg_decl_label(lend);
			} else {
				int lend = cg_new_label();
				cg_jmp_if_false(lend, gen_expr(s->_if.cond, st));
				gen_stmt(s->_if._true, st);
				cg_decl_label(lend);
			}
			break;
		case STMT_WHILE:
			{
				int lstart = cg_new_label();
				int lend = cg_new_label();
				cg_decl_label(lstart);
				cg_jmp_if_false(lend, gen_expr(s->_while.cond, st));
				gen_stmt(s->_while.stmt, st);
				cg_jmp(lstart);
				cg_decl_label(lend);
			}
			break;
		case STMT_RETURN:
			cg_ret(s->expr ? gen_expr(s->expr, st) : -1);
			break;
		case STMT_EXPR:
			gen_expr(s->expr, st);
			break;
		case STMT_NOOP:
			break;
		default:
			error("unknown statement type '%d'.\n", s->stmt_type);
			break;
	}
	cg_reg_free_all();
}

void gen_func(struct func* f) {
	cg_func_pre(f);
	gen_stmt(f->stmt, f->st);
	cg_func_post(f);
}

void gen(struct lib* l) {
	for (int i = l->funcs->size - 1; i >= 0; i--)
		gen_func(l->funcs->data[i]);
	cg_lib_post(l);
}
