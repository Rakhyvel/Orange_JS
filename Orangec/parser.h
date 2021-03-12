/*  parser.h

    Author: Joseph Shimel
    Date: 2/3/21
*/

#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#include "./main.h"
#include "./symbol.h"

#include "../util/list.h"

// Pre-processing
void parser_removeComments(struct list*);
void parser_condenseArrayIdentifiers(struct list*);

// Symbol tree functions
struct symbolNode* parser_parseTokens(struct list*, struct symbolNode*);

// AST functions
struct astNode* parser_createAST(struct list*, struct symbolNode*);

#endif