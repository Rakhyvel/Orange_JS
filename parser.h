/*  parser.h

    Author: Joseph Shimel
    Date: 2/3/21
*/

#ifndef PARSER_H
#define PARSER_H

#include "./util/list.h"

/*
    AST nodes have types that tell them apart */
enum astType {
    // StatementNode types
	AST_BLOCK, AST_VARDECLARE, AST_VARDEFINE, AST_IF, AST_WHILE,
	// Values
    AST_NUMLITERAL, AST_CALL, AST_VAR, AST_STRINGLITERAL, AST_CHARLITERAL,
    // Math operators
	AST_PLUS, AST_MINUS, AST_MULTIPLY, AST_DIVIDE, AST_ASSIGN,
    // Branch operators
    AST_IS, AST_ISNT, AST_GREATER, AST_LESSER,
    // Boolean operators
    AST_AND, AST_OR, 
    // Indexing
    AST_DOT, AST_INDEX,
    // Unused
    AST_NOP
};

/*
    Abstract Syntax Trees describe the program and the language's grammar
    in a far more effective way than text or tokens. */
struct astNode {
    enum astType type;
    struct list* children; // of other AST's usually
    char varType[255];
    char varName[255];
};

/*
    Stores information about the program being compiled. */
struct program {
    struct map* modulesMap;
};

/*
    Modules are a way of collecting functions and module together in one place.
    They offer a namespace to help keep code readable and organized. They can 
    be used to maintain state and keep a project manageable. */
struct module {
    char name[255];
    struct map* functionsMap;

    struct program* program;
};

/*
    Functions take in input, perform actions on that input, and produce output. */
struct function {
    char name[255];
    char returnType[255];
    struct list* argTypes;
    struct list* argNames;
    struct astNode* code;

    struct module* module;
    struct program* program;
};

struct program* parser_initProgram();
struct module* parser_initModule();
struct function* parser_initFunction();

void parser_addModules(struct program*, struct list*);
void parser_addFunctions(struct module*, struct list*);

struct astNode* parser_createAST(struct list*);
void parser_printAST(struct astNode*, int);

#endif