#pragma once

#include "parser.h"
#include "sym.h"

// Registers
int cg_reg_alloc();
void cg_reg_free(int);
void cg_reg_free_all();
void cg_reg_push_used();
void cg_reg_pop_used();

// Declarations
int cg_new_label();
void cg_decl_label(int);
void cg_decl_global(struct sym*);
void cg_decl_string(struct sym*);

// Branching
void cg_jmp(int);
void cg_jmp_if_false(int, int);
void cg_push_arg(int, int);
int cg_call(const char*);
void cg_ret(int);

// Loading/storing
int cg_load_number(int);
int cg_load_string(int);
int cg_load_name(const char*, struct symtable*);
int cg_load_addr(int, int);
void cg_store_name(int, const char*, struct symtable*);
void cg_store_addr(int, int, int);

// Unary prefix
int cg_cast(int, int);
int cg_addrof(const char*, struct symtable*);

// Binary arithmetic
int cg_add(int, int);
int cg_sub(int, int);
int cg_mul(int, int);
int cg_div(int, int);
int cg_and(int, int);
int cg_or(int, int);
int cg_shl(int, int);
int cg_shr(int, int);

// Binary equality
int cg_eq(int, int);
int cg_neq(int, int);
int cg_lt(int, int);
int cg_gt(int, int);
int cg_lte(int, int);
int cg_gte(int, int);
int cg_cmp(int, int, enum expr_type);

// Pre and postambles
void cg_func_pre(struct func*);
void cg_func_post(struct func*);
void cg_lib_post(struct lib*);
