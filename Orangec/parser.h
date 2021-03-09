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
    int isConstant; // value cannot change
    int isPrivate;  // only accessed by direct descendants (ie not root access operator ":")
    int isDeclared; // has value been set or not
    int isStatic;   // can access other static symbols

    // Metadata
    const char* filename;
    int line;
};

/*
    Stores information about the program being compiled. */
struct program {
    struct map* modulesMap; // name -> struct module
    struct map* dataStructsMap; // name -> struct dataStruct
    struct map* fileMap;

    char output[255];
    char target[255];
};

struct block {
    struct block* parent;
    struct map* varMap; // name -> struct variable
    
    struct module* module;
};

/*
    Modules are a way of collecting functions and module together in one place.
    They offer a namespace to help keep code readable and organized. They can 
    be used to maintain state and keep a project manageable. */
struct module {
    char name[255];
    struct map* functionsMap; // name -> struct function
    struct block* block;

    int isStatic;

    const char* filename;
	int line;
    struct program* program;
};

/*
    Holds common data for function, structs, and variables such as type, name,
    an AST, and some flags */
struct variable {
    char varType[255];
    char type[255];
    char name[255];
    struct list* paramTypes;
    struct astNode* code;

    const char* filename;
	int line;

    int isConstant;
    int isPrivate;
    int isDeclared;
};

/*
    Functions take in input, perform actions on that input, and produce output. */
struct function {
    struct variable self;
    struct block* argBlock;
    struct block* block;

    struct module* module;
    struct program* program;
};

/*
    Structs are a collection of variables that can be packaged and moved as one */
struct dataStruct {
    struct variable self;
    struct block* argBlock;

    struct module* module;
    struct program* program;
};

// Init functions
struct program* parser_initProgram();
struct module* parser_initModule(struct program*, const char*, int);
struct function* parser_initFunction(const char*, int);
struct dataStruct* parser_initDataStruct(const char*, int);
struct block* parser_initBlock(struct block*);

// Pre-processing
void parser_removeComments(struct list*);

// Parsing elements
void parser_addModules(struct program*, struct list*);
void parser_addElements(struct module*, struct list*);

// AST functions
struct astNode* parser_createAST(struct list*, struct block*);
void parser_printAST(struct astNode*, int);
char* parser_astToString(enum astType);

#endif