#pragma once

#include "type.h"

enum sym_type {
	SYM_FUNC, SYM_GLOBAL, SYM_LOCAL, SYM_STRING,
};
struct sym {
	enum sym_type sym_type;
	int type;
	char* name;
	int offset;

	int param_count;
	int param_capacity;
	int* params;
};

struct symtable {
	struct symtable* parent;
	struct vec* syms;
};

// Sym
struct sym* sym_alloc(enum sym_type);
void sym_add_param(struct sym*, int);

void sym_put(struct symtable*, struct sym*);
struct sym* sym_get(struct symtable*, const char*);
int sym_get_last_offset(struct symtable*);

// Symtable
struct symtable* symtable_alloc(struct symtable*);
void symtable_free(struct symtable*);

struct symtable* symtable_get_root(struct symtable*);
