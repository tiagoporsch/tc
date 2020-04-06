#include "cg.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "sym.h"

extern FILE* output_file;

/*
 * Utilities
 */
// halt and catch fire
static int error(const char* format, ...) {
	va_list args;
	va_start(args, format);
	printf("cg error: ");
	vprintf(format, args);
	va_end(args);
	exit(1);
	return -1;
}

static void out(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(output_file, format, args);
	va_end(args);
}

static char* cg_get_load_instr(int type) {
	if (type_getsize(type) == 8) return "mov";
	else return type_issigned(type) ? "movsx" : "movzx";
}

static char* cg_get_size(int type) {
	switch (type_getsize(type)) {
		case 1: return "byte";
		case 2: return "word";
		case 4: return "dword";
		case 8: return "qword";
		default:
			error("cg_get_size: invalid type %d.\n", type);
			return NULL;
	}
}

/*
 * Registers
 */
#define REG_COUNT 8
static int reg_used[REG_COUNT] = { 0 };
static char* reg64[] = { "r10",  "r11",  "r12",  "r13",  "r14",  "r15",  "rax", "rbx" };
static char* reg32[] = { "r10d", "r11d", "r12d", "r13d", "r14d", "r15d", "eax", "ebx" };
static char* reg16[] = { "r10w", "r11w", "r12w", "r13w", "r14w", "r15w", "ax",  "bx" };
static char* reg8[] =  { "r10b", "r11b", "r12b", "r13b", "r14b", "r15b", "al",  "bl" };

#define ARG_REG_COUNT 6
static char* arg_reg64[] = { "rdi", "rsi", "rdx", "rcx", "r8",  "r9" };
static char* arg_reg32[] = { "edi", "esi", "edx", "ecx", "r8d", "r9d" };

// returns the name of the register 'r' when storing a value of type 'type'
// from it.
static char* cg_get_reg_name(int r, int type) {
	switch (type_getsize(type)) {
		case 1: return reg8[r];
		case 2: return reg16[r];
		case 4: return reg32[r];
		case 8: return reg64[r];
		default:
			error("cg_get_reg_name: invalid type %d.\n", type);
			return NULL;
	}
}

// returns the name of the register 'r' when loading a value of type 'type'
// into it.
static char* cg_get_reg_load_name(int r, int type) {
	return type == TYPE_32 ? reg32[r] : reg64[r];
}

int cg_reg_alloc() {
	int r = 0;
	int ru = 10000000;
	for (int i = 0; i < REG_COUNT; i++) {
		if (reg_used[i] < ru) {
			ru = reg_used[i];
			r = i;
		}
	}
	if (reg_used[r]++ > 0) {
		out("\tpush\t%s\n", reg64[r]);
	}
	return r;
}

void cg_reg_free(int r) {
	if (reg_used[r] == 0) {
		printf("Attempting to free already free'd register %s.\n", reg64[r]);
	} else if (--reg_used[r] > 0) {
		out("\tpop\t%s\n", reg64[r]);
	}
}

void cg_reg_free_all() {
	for (int i = 0; i < REG_COUNT; i++) {
		while (--reg_used[i] > 0) {
			out("\tpop\t%s\n", reg64[i]);
		}
		reg_used[i] = 0;
	}
}

void cg_reg_push_used() {
	for (int i = 0; i < REG_COUNT; i++) {
		if (reg_used[i] > 0) {
			out("\tpush\t%s\n", reg64[i]);
		}
	}
}

void cg_reg_pop_used() {
	for (int i = REG_COUNT - 1; i >= 0; i--) {
		if (reg_used[i] > 0) {
			out("\tpop\t%s\n", reg64[i]);
		}
	}
}


/*
 * Declarations
 */
int cg_new_label() {
	static int label_count = 0;
	return label_count++;
}

void cg_decl_label(int label) {
	out("L%d:\n", label);
}

void cg_decl_global(struct sym* s) {
	out("%s ", s->name);
	switch (type_getsize(s->type)) {
		case 1: out("db 0\n"); break;
		case 2: out("dw 0\n"); break;
		case 4: out("dd 0\n"); break;
		case 8: out("dq 0\n"); break;
		default:
			error("cg_decl_global\n");
			break;
	}
}

void cg_decl_string(struct sym* s) {
	out("LC%d: db ", s->offset);
	for (char* str = s->name; *str; str++)
		out("%d, ", *str);
	out("0\n");
}


/*
 * Branching
 */
void cg_jmp(int label) {
	out("\tjmp L%d\n", label);
}

void cg_jmp_if_false(int label, int r) {
	out("\ttest %s, %s\n", reg64[r], reg64[r]);
	out("\tjz L%d\n", label);
}

void cg_push_arg(int i, int r) {
	if (i < ARG_REG_COUNT) {
		out("\tmov %s, %s\n", arg_reg64[i], reg64[r]);
	} else {
		out("\tpush %s\n", reg64[r]);
	}
}

int cg_call(const char* name) {
	out("\tcall %s\n", name);
	cg_reg_pop_used();
	int r = cg_reg_alloc();
	out("\tmov %s, rax\n", reg64[r]);
	return r;
}

void cg_ret(int r) {
	out("\tmov rax, %s\n", r < 0 ? "0" : reg64[r]);
	out("\tleave\n");
	out("\tret\n");
}

/*
 * Load (something into a register)
 */
// register <- number
int cg_load_number(int number) {
	int r = cg_reg_alloc();
	out("\tmov %s, %d\n", reg64[r], number);
	return r;
}

// register <- &string
int cg_load_string(int label) {
	int r = cg_reg_alloc();
	out("\tmov %s, LC%d\n", reg64[r], label);
	return r;
}

// register <- variable
int cg_load_name(const char* name, struct symtable* st) {
	struct sym* s = sym_get(st, name);
	int r = cg_reg_alloc();
	if (s->sym_type == SYM_LOCAL) {
		out("\t%s %s, %s [rbp%d]\n",
			cg_get_load_instr(s->type),
			cg_get_reg_load_name(r, s->type),
			cg_get_size(s->type),
			s->offset);
	} else if (s->sym_type == SYM_GLOBAL) {
		out("\t%s %s, %s [%s]\n",
			cg_get_load_instr(s->type),
			cg_get_reg_load_name(r, s->type),
			cg_get_size(s->type),
			s->name);
	} else {
		return error("cg_load_name: invalid symbol type %d.\n", s->sym_type);
	}
	return r;
}

// register <- *register
int cg_load_addr(int r, int type) {
	out("\t%s %s, %s [%s]\n",
		cg_get_load_instr(type),
		cg_get_reg_load_name(r, type),
		cg_get_size(type),
		reg64[r]);
	return r;
}

/*
 * Store (something from a register)
 */
// variable <- register
void cg_store_name(int r, const char* name, struct symtable* st) {
	struct sym* s = sym_get(st, name);
	if (s->sym_type == SYM_LOCAL) {
		out("\tmov [rbp%d], %s\n",
			s->offset,
			cg_get_reg_name(r, s->type));
	} else if (s->sym_type == SYM_GLOBAL) {
		out("\tmov [%s], %s\n",
			s->name,
			cg_get_reg_name(r, s->type));
	} else {
		error("cg_store_name: invalid symbol type %d.\n", s->sym_type);
	}
}

// *register <- register
void cg_store_addr(int rsrc, int rdest, int type) {
	out("\tmov [%s], %s\n",
		reg64[rdest],
		cg_get_reg_name(rsrc, type));
}

/*
 * Unary prefix
 */

// r <- (type) r
int cg_cast(int r, int type) {
	if (type_getsize(type) != 8) {
		out("\tand %s, %d\n",
			reg64[r],
			~(1 << (8 * type_getsize(type))));
	}
	return r;
}

// r <- &variable
int cg_addrof(const char* name, struct symtable* st) {
	int r = cg_reg_alloc();
	struct sym* s = sym_get(st, name);
	if (s->sym_type == SYM_LOCAL) {
		out("\tlea %s, [rbp%d]\n", reg64[r], s->offset);
	} else if (s->sym_type == SYM_GLOBAL) {
		out("\tlea %s, [%s]\n", reg64[r], s->name);
	} else {
		error("cg_addrof: invalid symbol type %d\n", s->sym_type);
	}
	return r;
}

/*
 * Binary arithmetic
 */
// r1 <- r1 + r2
int cg_add(int r1, int r2) {
	out("\tadd %s, %s\n", reg64[r1], reg64[r2]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 - r2
int cg_sub(int r1, int r2) {
	out("\tsub %s, %s\n", reg64[r1], reg64[r2]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 * r2
int cg_mul(int r1, int r2) {
	out("\timul %s, %s\n", reg64[r1], reg64[r2]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 / r2
int cg_div(int r1, int r2) {
	if (reg_used[6]++ > 0)
		out("\tpush rax\n");
	out("\tmov rax, %s\n", reg64[r1]);
	out("\tcqo\n");
	out("\tidiv %s\n", reg64[r2]);
	out("\tmov %s, rax\n", reg64[r1]);
	if (--reg_used[6] > 0)
		out("\tpop rax\n");
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 & r2
int cg_and(int r1, int r2) {
	out("\tand %s, %s\n", reg64[r1], reg64[r2]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 | r2
int cg_or(int r1, int r2) {
	out("\tor %s, %s\n", reg64[r1], reg64[r2]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 << r2
int cg_shl(int r1, int r2) {
	out("\tmov cl, %s\n", reg8[r2]);
	out("\tshl %s, cl\n", reg64[r1]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 >> r2
int cg_shr(int r1, int r2) {
	out("\tmov cl, %s\n", reg8[r2]);
	out("\tshr %s, cl\n", reg64[r1]);
	cg_reg_free(r2);
	return r1;
}

/*
 * Binary equality
 */
// r1 <- r1 == r2
int cg_eq(int r1, int r2) {
	out("\tcmp %s, %s\n", reg64[r1], reg64[r2]);
	out("\tsete %s\n", reg8[r1]);
	out("\tand %s, 1\n", reg64[r1]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 != r2
int cg_neq(int r1, int r2) {
	out("\tcmp %s, %s\n", reg64[r1], reg64[r2]);
	out("\tsetne %s\n", reg8[r1]);
	out("\tand %s, 1\n", reg64[r1]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 < r2
int cg_lt(int r1, int r2) {
	out("\tcmp %s, %s\n", reg64[r1], reg64[r2]);
	out("\tsetl %s\n", reg8[r1]);
	out("\tand %s, 1\n", reg64[r1]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 > r2
int cg_gt(int r1, int r2) {
	out("\tcmp\t%s, %s\n", reg64[r1], reg64[r2]);
	out("\tsetg\t%s\n", reg8[r1]);
	out("\tand %s, 1\n", reg64[r1]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 <= r2
int cg_lte(int r1, int r2) {
	out("\tcmp\t%s, %s\n", reg64[r1], reg64[r2]);
	out("\tsetle\t%s\n", reg8[r1]);
	out("\tand %s, 1\n", reg64[r1]);
	cg_reg_free(r2);
	return r1;
}

// r1 <- r1 >= r2
int cg_gte(int r1, int r2) {
	out("\tcmp\t%s, %s\n", reg64[r1], reg64[r2]);
	out("\tsetge\t%s\n", reg8[r1]);
	out("\tand %s, 1\n", reg64[r1]);
	cg_reg_free(r2);
	return r1;
}

/*
 * Pre and postambles
 */
static int get_last_offset(struct stmt* s) {
	if (s->stmt_type != STMT_COMPOUND)
		return 0;
	int offset = -sym_get_last_offset(s->compound.st);
	for (int i = 0; i < s->compound.stmts->size; i++) {
		int new_offset = get_last_offset(s->compound.stmts->data[i]);
		if (new_offset > offset)
			offset = new_offset;
	}
	return offset;
}

void cg_func_pre(struct func* f) {
	// Standard function header
	out("global %s\n", f->name);
	out("%s:\n", f->name);
	out("\tpush rbp\n");
	out("\tmov rbp, rsp\n");

	// Make space in the stack for local variables and arguments
	int last_offset = get_last_offset(f->stmt);
	if (last_offset > 0)
		out("\tsub rsp, %d\n", last_offset);

	// Move arguments from registers to the stack
	for (int i = 0; i < f->st->syms->size && i < 6; i++) {
		struct sym* s = f->st->syms->data[i];
		switch (type_getsize(s->type)) {
			case 1:
				out("\tmov eax, %s\n", arg_reg32[i]);
				out("\tmov [rbp%d], al\n", s->offset);
				break;
			case 2:
				out("\tmov eax, %s\n", arg_reg32[i]);
				out("\tmov [rbp%d], ax\n", s->offset);
				break;
			case 4:
				out("\tmov [rbp%d], %s\n", s->offset, arg_reg32[i]);
				break;
			case 8:
				out("\tmov [rbp%d], %s\n", s->offset, arg_reg64[i]);
				break;
			default:
				printf("Code generation error: bad parameter type %s\n", type_tostr(s->type));
				exit(1);
		}
	}
}

void cg_func_post(struct func* f) {
	if (f->type == TYPE_0)
		cg_ret(-1);
}

void cg_lib_post(struct lib* l) {
	for (int i = 0; i < l->st->syms->size; i++) {
		struct sym* s = l->st->syms->data[i];
		if (s->sym_type == SYM_GLOBAL)
			cg_decl_global(s);
		else if (s->sym_type == SYM_STRING)
			cg_decl_string(s);
	}
}
