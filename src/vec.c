#include "vec.h"

#include <stdlib.h>

struct vec *vec_alloc() {
	struct vec *v = malloc(sizeof(struct vec));
	v->capacity = 16;
	v->size = 0;
	v->data = malloc(sizeof(void*) * v->capacity);
	return v;
}

void vec_free(struct vec *v) {
	free(v->data);
	free(v);
}

void vec_push(struct vec *v, void *e) {
	if (v->size == v->capacity)
		v->data = realloc(v->data, sizeof(void*) * (v->capacity *= 2));
	v->data[v->size++] = e;
}
