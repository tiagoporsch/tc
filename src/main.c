#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "lexer.h"
#include "parser.h"
#include "token.h"
#include "vec.h"

char* input_filename;
FILE* input_file;

char* output_filename;
FILE* output_file;

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Usage: %s file\n", argv[0]);
		return 1;
	}

	input_filename = argv[1];
	input_file = fopen(input_filename, "r");
	if (input_file == NULL) {
		printf("Error opening file '%s' for reading.\n", input_filename);
		return 1;
	}

	output_filename = strdup(argv[1]);
	output_filename[strlen(output_filename) - 1] = 's';
	output_file = fopen(output_filename, "w");
	if (output_file == NULL) {
		printf("Error opening file '%s' for writing.\n", output_filename);
		fclose(input_file);
		return 1;
	}

	gen(parse(lex()));

	fclose(input_file);
	fclose(output_file);
	return 0;
}
