#include "token.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* token_type_str[] = {
	"extern", "var", "fn", "return",
	"if", "else", "while",
	"sizeof",

	">>=", "<<=", "+=", "-=",
	"*=", "/=", "&=", "|=",
	">>", "<<", "++", "--", "&&",
	"||", "<=", ">=", "==", "!=",

	"'NUMBER", "'TYPE", "'NAME", "'EOF",
};

int token_type_fromstr(const char* s) {
	for (int i = 0; i <= T_SIZEOF - T_EXTERN; i++)
		if (!strcmp(s, token_type_str[i]))
			return T_EXTERN + i;
	return T_EOF;
}

const char* token_type_tostr(int t) {
	if (t < T_EXTERN) {
		static char buffer[2] = { 0 };
		buffer[0] = t;
		return buffer;
	} else if (t < T_NUMBER) {
		return token_type_str[t - T_EXTERN];
	} else if (t <= T_EOF) {
		return token_type_str[t - T_EXTERN] + 1;
	}
	return "?";
}

const char* token_tostr(struct token* t) {
	static char buffer[21];
	if (t->token == T_NUMBER) {
		sprintf(buffer, "%ld", t->number);
		return buffer;
	} else if (t->token == T_TYPE) {
		return type_tostr(t->type);
	} else if (t->token == T_NAME) {
		return t->name;
	} else if (t->token == T_EOF) {
		return "EOF";
	} else {
		return token_type_tostr(t->token);
	}
}
