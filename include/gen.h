#pragma once

#include <stdio.h>

#include "parser.h"

int gen_expr(struct expr*, struct symtable*);
void gen_stmt(struct stmt*, struct symtable*);
void gen(struct lib*);
