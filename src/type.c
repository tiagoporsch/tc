#include "type.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int type_fromstr(const char* s) {
	if (!strcmp(s, "s0"))	return TYPE_0 | TYPE_SIGNED;
	if (!strcmp(s, "s1"))	return TYPE_1 | TYPE_SIGNED;
	if (!strcmp(s, "s8"))	return TYPE_8 | TYPE_SIGNED;
	if (!strcmp(s, "s16"))	return TYPE_16 | TYPE_SIGNED;
	if (!strcmp(s, "s32"))	return TYPE_32 | TYPE_SIGNED;
	if (!strcmp(s, "s64"))	return TYPE_64 | TYPE_SIGNED;
	if (!strcmp(s, "u0"))	return TYPE_0;
	if (!strcmp(s, "u1"))	return TYPE_1;
	if (!strcmp(s, "u8"))	return TYPE_8;
	if (!strcmp(s, "u16"))	return TYPE_16;
	if (!strcmp(s, "u32"))	return TYPE_32;
	if (!strcmp(s, "u64"))	return TYPE_64;
	return 0;
}

int type_fromint(long n) {
	if (n >> 32) return TYPE_64 | TYPE_SIGNED;
	if (n >> 16) return TYPE_32 | TYPE_SIGNED;
	if (n >>  8) return TYPE_16 | TYPE_SIGNED;
	return TYPE_8 | TYPE_SIGNED;
}

char* type_tostr(int t) {
	static char buffer[128];
	int index = sprintf(buffer, type_issigned(t) ? "s" : "u");
	switch (type_gettype(t)) {
		case TYPE_0: index += sprintf(buffer + index, "0"); break;
		case TYPE_1: index += sprintf(buffer + index, "1"); break;
		case TYPE_8: index += sprintf(buffer + index, "8"); break;
		case TYPE_16: index += sprintf(buffer + index, "16"); break;
		case TYPE_32: index += sprintf(buffer + index, "32"); break;
		case TYPE_64: index += sprintf(buffer + index, "64"); break;
		default: index += sprintf(buffer + index, "(%d)", type_gettype(t)); break;
	}
	while (type_getpointer(t) > 0) {
		index = sprintf(buffer + index, "*");
		t = type_fromptr(t);
	}
	return buffer;
}

int type_getsize(int t) {
	if (type_getpointer(t) > 0)
		return 8;
	switch (type_gettype(t)) {
		case TYPE_0: return 0;
		case TYPE_1: return 1;
		case TYPE_8: return 1;
		case TYPE_16: return 2;
		case TYPE_32: return 4;
		case TYPE_64: return 8;
		default:
			printf("Error at type_getsize: no size for type '%s'.\n", type_tostr(t));
			exit(1);
	}
}

int type_getalign(int t) {
	int size = type_getsize(t);
	return size < 4 ? 4 : size;
}

int type_bigger(int t1, int t2) {
	return type_getsize(t1) > type_getsize(t2) ? t1 : t2;
}

bool type_fits(int type, int into) {
	if (type == into)
		return true;
	if (type_getpointer(type) || type_getpointer(into)) {
		if (type_getpointer(type) != type_getpointer(into))
			return false;
		if (type_gettype(type) != type_gettype(into))
			return false;
	}
	if (type_issigned(type) < type_issigned(into)) {
		if (type_getsize(type) >= type_getsize(into))
			return false;
	}
	if (type_getsize(type) > type_getsize(into))
		return false;
	return true;
}
