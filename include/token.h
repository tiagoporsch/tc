#pragma once

#include "type.h"

enum {
	T_EXTERN = 127, T_VAR, T_FN, T_RETURN,
	T_IF, T_ELSE, T_WHILE,
	T_SIZEOF,

	T_SHR_ASSIGN, T_SHL_ASSIGN, T_ADD_ASSIGN, T_SUB_ASSIGN,
	T_MUL_ASSIGN, T_DIV_ASSIGN, T_AND_ASSIGN, T_OR_ASSIGN,
	T_SHR, T_SHL, T_INC, T_DEC, T_AND,
	T_OR, T_LE, T_GE, T_EQ, T_NE,

	T_NUMBER, T_STRING, T_TYPE, T_NAME, T_EOF,
};

struct token {
	int line, line_index;
	int token;
	union {
		long number;
		int type;
		char* name;
	};
};

int token_type_fromstr(const char*);
const char* token_type_tostr(int);
const char* token_tostr(struct token*);
