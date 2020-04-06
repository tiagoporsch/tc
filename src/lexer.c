#include "lexer.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

extern char* input_filename;
extern FILE* input_file;
extern FILE* output_file;

static int line = 1;
static int line_index = 0;

static void error(const char* format, ...) {
	va_list args;
	va_start(args, format);
	printf("%s:%d:%d: error: ", input_filename, line, line_index);
	vprintf(format, args);
	va_end(args);
	exit(1);
}

/*
 * Reading
 */
static int next() {
	int c = fgetc(input_file);
	line_index++;
	if (c == '\n') {
		line++;
		line_index = 0;
	}
	return c;
}

static int peek() {
	int c = fgetc(input_file);
	ungetc(c, input_file);
	return c;
}

static bool optional(int c) {
	if (peek() == c)
		return next() > 0;
	return false;
}

static void expect(int c) {
	if (next() != c)
		error("invalid character. expected '%c'.\n", c);
}

static int skipws() {
	int c = next();
	while (c == ' ' || c == '\t' || c == '\n' || c == '\r')
		c = next();
	return c;
}

static int skipto(char* to) {
	int c = next();
	while (strchr(to, c) == NULL)
		c = next();
	return c;
}

/*
 * Checking
 */
static int isvarchar(int c) {
	return isalnum(c) || c == '_';
}

static int to_number(int c) {
	if (c >= 'a' && c <= 'f') return 10 + c - 'a';
	if (c >= 'A' && c <= 'F') return 10 + c - 'A';
	if (c >= '0' && c <= '9') return c - '0';
	return -1;
}

/*
 * Lexing
 */
static long lex_number(int c, int base) {
	long n = to_number(c);
	while (to_number(peek()) != -1)
		n = base * n + to_number(next());
	return n;
}

static char* lex_word(int c) {
	static char buffer[512];
	int index = 0;
	buffer[index++] = c;
	while (isvarchar(peek()))
		buffer[index++] = next();
	buffer[index] = '\0';
	return buffer;
}

static char* lex_string() {
	static char buffer[512];
	int index = 0;
	for (int c = next(); c != '"'; c = next()) {
		if (c == '\\') {
			switch (next()) {
				case 'n': c = '\n'; break;
			}
		}
		buffer[index++] = c;
	}
	buffer[index] = '\0';
	return buffer;
}

struct token* lex_token() {
	// Skip whitespace
	int c = skipws();
	// Skip comments
	while (c == '/') {
		if (optional('/')) {
			skipto("\n");
		} else if (optional('*')) {
			int comments = 1;
			while (comments > 0) {
				if (optional('/') && optional('*'))
					comments++;
				else if (optional('*') && optional('/'))
					comments--;
				else next();
			}
		} else {
			break;
		}
		c = skipws();
	}

	struct token* t = malloc(sizeof(struct token));
	t->line = line;
	t->line_index = line_index;

	switch (c) {
		case EOF:
			t->token = T_EOF;
			break;
		case '\'':
			t->token = T_NUMBER;
			t->number = next();
			if (t->number == '\\') {
				switch (next()) {
					case 'b': t->number = '\b'; break;
					case 't': t->number = '\t'; break;
					case 'n': t->number = '\n'; break;
					case 'f': t->number = '\f'; break;
					case 'r': t->number = '\r'; break;
					default:
						error("invalid escape sequence\n");
						break;
				}
			}
			expect('\'');
			break;
		case '"':
			t->token = T_STRING;
			t->name = strdup(lex_string());
			break;
		case '0':
			t->token = T_NUMBER;
			if      (optional('b')) t->number = lex_number(next(), 2);
			else if (optional('o')) t->number = lex_number(next(), 8);
			else if (optional('x')) t->number = lex_number(next(), 16);
			else                    t->number = lex_number(c,      10);
			break;
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9':
			t->token = T_NUMBER;
			t->number = lex_number(c, 10);
			break;
		case '>':
			if (optional('=')) t->token = T_GE;
			else if (optional('>')) {
				if (optional('=')) t->token = T_SHR_ASSIGN;
				else t->token = T_SHR;
			} else t->token = c;
			break;
		case '<':
			if (optional('=')) t->token = T_LE;
			else if (optional('<')) {
				if (optional('=')) t->token = T_SHL_ASSIGN;
				else t->token = T_SHL;
			} else t->token = c;
			break;
		case '+':
			if (optional('=')) t->token = T_ADD_ASSIGN;
			else if (optional('+')) t->token = T_INC;
			else t->token = c;
			break;
		case '-':
			if (optional('=')) t->token = T_SUB_ASSIGN;
			else if (optional('-')) t->token = T_DEC;
			else t->token = c;
			break;
		case '*':
			if (optional('=')) t->token = T_MUL_ASSIGN;
			else t->token = c;
			break;
		case '/':
			if (optional('=')) t->token = T_DIV_ASSIGN;
			else t->token = c;
			break;
		case '&':
			if (optional('=')) t->token = T_AND_ASSIGN;
			else t->token = c;
			break;
		case '|':
			if (optional('=')) t->token = T_OR_ASSIGN;
			else t->token = c;
			break;
		case '=':
			if (optional('=')) t->token = T_EQ;
			else t->token = c;
			break;
		case '!':
			if (optional('=')) t->token = T_NE;
			else t->token = c;
			break;
		case '{':
		case '}':
		case ',':
		case ':':
		case '[':
		case ']':
		case '(':
		case ')':
		case ';':
			t->token = c;
			break;
		default: {
			if (!isalpha(c) && c != '_')
				error("invalid character '%c'.\n", c);
			char* name = lex_word(c);
			if (!strcmp(name, "asm")) {
				c = skipws();
				if (c != '{')
					error("empty asm directive");
				optional('\n');
				while ((c = next()) != '}')
					fputc(c, output_file);
				free(t);
				return lex_token();
			} else if (token_type_fromstr(name) != T_EOF) {
				t->token = token_type_fromstr(name);
			} else if (type_fromstr(name) != 0) {
				t->token = T_TYPE;
				t->type = type_fromstr(name);
			} else {
				t->token = T_NAME;
				t->name = strdup(name);
			}
			break;
		}
	}
	return t;
}

struct vec* lex() {
	struct vec* v = vec_alloc();
	struct token* t;
	do vec_push(v, t = lex_token());
	while (t->token != T_EOF);
	return v;
}
