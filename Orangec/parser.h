/*  parser.h

    Author: Joseph Shimel
    Date: 2/3/21
*/

#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "./main.h"
#include "../util/list.h"

/*
    AST nodes have types that tell them apart */
enum astType {
    // StatementNode types
	AST_BLOCK, AST_VARDECLARE, AST_VARDEFINE, AST_IF, AST_IFELSE, AST_WHILE, AST_RETURN,
	// Values
    AST_INTLITERAL, AST_REALLITERAL, AST_CALL, AST_VAR, AST_STRINGLITERAL, 
    AST_CHARLITERAL, AST_ARRAYLITERAL, AST_TRUE, AST_FALSE, AST_NULL,
    // Math operators
	AST_ADD, AST_SUBTRACT, AST_MULTIPLY, AST_DIVIDE, AST_ASSIGN,
    // Branch operators
    AST_IS, AST_ISNT, AST_GREATER, AST_LESSER, AST_GREATEREQUAL, AST_LESSEREQUAL,
    // Boolean operators
    AST_AND, AST_OR, 
    // Type operators
    AST_CAST, AST_NEW, AST_FREE,
    // Indexing
    AST_DOT, AST_INDEX, AST_MODULEACCESS,
    // Unused
    AST_NOP
};

enum symbolType {
    SYMBOL_UNSURE,
    SYMBOL_PROGRAM,
    SYMBOL_MODULE,
    SYMBOL_STRUCT,
    SYMBOL_VARIABLE, 
    SYMBOL_FUNCTION,
    SYMBOL_BLOCK
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

/*
    The Symbol Tree describes symbols and their relationship to other symbols.
    
    Symbols include:
    - Modules
    - Structs
    - Variables
    - Functions
    - The program as a whole */
struct symbolNode {
    // Data
    enum symbolType symbolType;
    char type[255];
    char name[255];
    struct astNode* code;

    // Parse tree
    struct symbolNode* parent;
    struct map* children; // name -> other symbolNodes

    // Flags
    int isPrivate;  // only accessed by direct descendants (ie not root access operator ":")
    int isStatic;   // can access other static symbols
    int isConstant; // value cannot change
    int isDeclared; // has value been set or not

    // Metadata
    const char* filename;
    int line;
};

// Init functions
struct symbolNode* parser_createSymbolNode(enum symbolType, struct symbolNode*, const char*, int);

// Pre-processing
void parser_removeComments(struct list*);
void parser_condenseArrayIdentifiers(struct list*);

// Symbol tree functions
struct symbolNode* parser_parseTokens(struct list*, struct symbolNode*);

// AST functions
struct astNode* parser_createAST(struct list*, struct symbolNode*);
void parser_printAST(struct astNode*, int);
char* parser_astToString(enum astType);

#endif