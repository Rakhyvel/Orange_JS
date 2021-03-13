/*
    Author: Joseph Shimel
    Date: 3/12/21
*/

#ifndef AST_H
#define AST_H

#include "./symbol.h"
#include "./token.h"

#include "../util/list.h"

/*
    AST nodes have types that tell them apart */
enum astType {
	// Literals
    AST_VAR, AST_INTLITERAL, AST_REALLITERAL, AST_CHARLITERAL, 
    AST_STRINGLITERAL, AST_TRUE, AST_FALSE, AST_NULL, AST_CALL, AST_VERBATIM,
    // Math operators
	AST_ADD, AST_SUBTRACT, AST_MULTIPLY, AST_DIVIDE, AST_ASSIGN,
    // Branch operators
    AST_IS, AST_ISNT, AST_GREATER, AST_LESSER, AST_GREATEREQUAL, 
    AST_LESSEREQUAL,
    // Boolean operators
    AST_AND, AST_OR, 
    // Type operators
    AST_CAST, AST_NEW, AST_FREE,
    // StatementNode types
	AST_BLOCK, AST_SYMBOLDEFINE, AST_IF, AST_IFELSE, AST_WHILE, AST_RETURN,
    // Indexing
    AST_DOT, AST_INDEX, AST_MODULEACCESS,
    // Unused
    AST_NOP
};

/*
    Abstract Syntax Trees describe the actual code of a language in a more
    efficient way. */
struct astNode {
    enum astType type;
    struct list* children; // list of OTHER AST's ONLY!
    void* data;
    struct symbolNode* scope;

	const char* filename;
	int line;
};

struct astNode* ast_create(enum astType, const char*, int, struct symbolNode*);
char* ast_toString(enum astType);
char* itoa(int);
enum astType ast_tokenToAST(enum tokenType);

#endif