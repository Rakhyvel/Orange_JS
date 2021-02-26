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
	AST_BLOCK, AST_VARDECLARE, AST_VARDEFINE, AST_IF, AST_WHILE, AST_RETURN,
	// Values
    AST_INTLITERAL, AST_REALLITERAL, AST_CALL, AST_VAR, AST_STRINGLITERAL, 
    AST_CHARLITERAL, AST_ARRAYLITERAL, AST_TRUE, AST_FALSE,
    // Math operators
	AST_ADD, AST_SUBTRACT, AST_MULTIPLY, AST_DIVIDE, AST_ASSIGN,
    // Branch operators
    AST_IS, AST_ISNT, AST_GREATER, AST_LESSER,
    // Boolean operators
    AST_AND, AST_OR, 
    // Indexing
    AST_DOT, AST_INDEX, AST_MODULEACCESS,
    // Unused
    AST_NOP
};

/*
    Abstract Syntax Trees describe the program and the language's grammar
    in a far more effective way than text or tokens. */
struct astNode {
    enum astType type;
    struct list* children; // list of OTHER AST's ONLY!
    void* data;

	char* filename;
	int line;
};

/*
    Stores information about the program being compiled. */
struct program {
    struct map* modulesMap; // name -> struct module
    struct map* dataStructsMap; // name -> struct dataStruct
    struct map* fileMap;
};

/*
    Modules are a way of collecting functions and module together in one place.
    They offer a namespace to help keep code readable and organized. They can 
    be used to maintain state and keep a project manageable. */
struct module {
    char name[255];
    struct map* functionsMap; // name -> struct function
    struct map* globalsMap; // name -> struct variable

    int isStatic;

    struct program* program;
};

struct variable {
    char varType[255];
    char type[255];
    char name[255];
    struct astNode* code;

    int isConstant;
    int isPrivate;
    int isDeclared;
};

/*
    Functions take in input, perform actions on that input, and produce output. */
struct function {
    struct variable self;
    struct map* argMap; // name -> struct variable
    struct map* varMap; // name -> struct variable

    struct module* module;
    struct program* program;
};

struct dataStruct {
    struct variable self;
    struct map* fieldMap; // name -> struct variable
    struct map* parentSet; // names of parents, and self

    struct module* module;
    struct program* program;
};

struct program* parser_initProgram();
struct module* parser_initModule();
struct function* parser_initFunction();
struct dataStruct* parser_initDataStruct();

void parser_removeComments(struct list*);
void parser_addModules(struct program*, struct list*);
void parser_addElements(struct module*, struct list*);

struct astNode* parser_createAST(struct list*, struct function*);
void parser_printAST(struct astNode*, int);

char* parser_astToString(enum astType);

bool parser_astTakesTypes(char* types,...);
char* parser_astReturnType(enum astType, char* inputs,...);

#endif