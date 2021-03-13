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

void parser_removeComments(struct list*);
void parser_condenseArrayIdentifiers(struct list*);
struct symbolNode* parser_parseTokens(struct list*, struct symbolNode*);

#endif