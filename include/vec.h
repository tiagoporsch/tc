#pragma once

#include <stdbool.h>

struct vec {
	void** data;
	int capacity;
	int size;
};

struct iterator {
	struct vec *vec;
	int index;
};

struct vec *vec_alloc();
void vec_free(struct vec*);
void vec_push(struct vec*, void*);
