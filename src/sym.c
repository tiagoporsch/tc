#include "sym.h"

#include <stdlib.h>
#include <string.h>

#include "vec.h"

struct sym* sym_alloc(enum sym_type sym_type) {
	struct sym* s = malloc(sizeof(struct sym));
	s->sym_type = sym_type;
	s->param_count = 0;
	if (s->sym_type == SYM_FUNC) {
		s->param_capacity = 6;
		s->params = malloc(s->param_capacity * sizeof(int));
	} else {
		s->param_capacity = 0;
	}
	s->offset = 0;
	return s;
}

void sym_add_param(struct sym* s, int type) {
	if (s->param_count == s->param_capacity)
		s->params = realloc(s->params, sizeof(int) * (s->param_capacity *= 2));
	s->params[s->param_count++] = type;
}

void sym_put(struct symtable* st, struct sym* s) {
	vec_push(st->syms, s);
}

struct sym* sym_get(struct symtable* st, const char* name) {
	while (st != NULL) {
		for (int i = 0; i < st->syms->size; i++) {
			struct sym* s = st->syms->data[i];
			if (!strcmp(name, s->name))
				return s;
		}
		st = st->parent;
	}
	return NULL;
}

int sym_get_last_offset(struct symtable* st) {
	int offset = 0;
	while (st != NULL) {
		for (int i = 0; i < st->syms->size; i++) {
			struct sym* s = st->syms->data[i];
			if (s->offset < offset)
				offset = s->offset;
		}
		st = st->parent;
	}
	return offset;
}

struct symtable* symtable_alloc(struct symtable* parent) {
	struct symtable* st = malloc(sizeof(struct symtable));
	st->parent = parent;
	st->syms = vec_alloc();
	return st;
}

void symtable_free(struct symtable* st) {
	for (int i = 0; i < st->syms->size; i++) {
		struct sym* s = st->syms->data[i];
		if (s->sym_type == SYM_FUNC)
			free(s->params);
		free(s);
	}
	free(st);
}

struct symtable* symtable_get_root(struct symtable* st) {
	while (st->parent != NULL)
		st = st->parent;
	return st;
}
