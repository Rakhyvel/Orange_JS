/*
    Author: Joseph Shimel
    Date: 3/10/21
*/

#ifndef SYMBOL_H
#define SYMBOL_H

enum symbolType {
    SYMBOL_UNSURE,
    SYMBOL_PROGRAM,
    SYMBOL_MODULE,
    SYMBOL_STRUCT,
    SYMBOL_VARIABLE, 
    SYMBOL_EXTERN_VARIABLE, 
    SYMBOL_FUNCTIONPTR, 
    SYMBOL_FUNCTION,
    SYMBOL_BLOCK
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
    char path[255];
    char type[255];
    char name[255];
    int id;
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

struct symbolNode* symbol_findExplicitSymbol(char*, char*, const struct symbolNode*, const char*, int);
struct symbolNode* symbol_findSymbol(const char*, const struct symbolNode*);

#endif