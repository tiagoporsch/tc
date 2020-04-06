#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

// Bit 31 -> signed
#define TYPE_SIGNED (1 << 31)
// Bits 30..27 -> size
#define	TYPE_0  (1 << 27)
#define TYPE_1  (2 << 27)
#define TYPE_8  (3 << 27)
#define TYPE_16 (4 << 27)
#define TYPE_32 (5 << 27)
#define TYPE_64 (6 << 27)

// Bits 26..0 -> pointer depth

static inline bool type_issigned(int t) { return t & TYPE_SIGNED; }
static inline int type_gettype(int t) { return t & ((long) 15 << 27); }
static inline int type_getpointer(int t) { return t & ~((long) 31 << 27); }

static inline int type_toptr(int t) {
	return t + 1;
}
static inline int type_fromptr(int t) {
	if (type_getpointer(t) > 0)
		return t - 1;
	printf("Error: can't dereference non-pointer.\n");
	exit(1);
}

int type_fromstr(const char*);
int type_fromint(long);
char* type_tostr(int);

int type_getsize(int);
int type_getalign(int);

int type_bigger(int, int);
bool type_fits(int, int);
