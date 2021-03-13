/*  symbol.c

    A symbol is a way to label a piece of code or data. They are ordered 
    hierarchically, so that children can see ancestor symbols, but not the
    other way around. Sort of a bottom up approach.

    However, that gets clunky fast, and so a simple solution is to provide a
    safe way to access symbols in a top down approach via the ":" operator.

    Marking a symbol with the "private" modifier means that it cannot be 
    accessed with the ":" operator, but can still be seen by descendant symbols.

    Author: Joseph Shimel
    Date: 3/10/21
*/

#include <stdlib.h>

#include "./main.h"
#include "./symbol.h"

#include "../util/list.h"
#include "../util/debug.h"

static int num_ids = 0;

/*
    Allocates and initializes the program struct */
struct symbolNode* symbol_create(enum symbolType symbolType, struct symbolNode* parent, const char* filename, int line) {
    struct symbolNode* retval = (struct symbolNode*)calloc(1, sizeof(struct symbolNode));

    retval->symbolType = symbolType;
    retval->parent = parent;
    retval->children = map_create();
    retval->filename = filename;
    retval->line = line;
    retval->id = num_ids++;

    return retval;
}

/*
    Returns the symbol with the given name relative to a given starting scope.
    
    Will return NULL if no symbol with the name is found in any direct ancestor
    scopes. */
struct symbolNode* symbol_find(const char* symbolName, const struct symbolNode* scope) {
    struct symbolNode* symbol = map_get(scope->children, symbolName);
    if(symbol != NULL) {
        return symbol;
    } else if(scope->parent != NULL) {
        return symbol_find(symbolName, scope->parent);
    }
    return NULL;
}

/*
    Searches a given module for a given member.
    
    Does not return null, instead errors itself for finer grain errors */
struct symbolNode* symbol_findExplicit(char* moduleName, char* memberName, const struct symbolNode* scope, const char* filename, int line) {
    struct symbolNode* module = map_get(program->children, moduleName);
    if(module == NULL) {
        error(filename, line, "Unknown module \"%s\"", moduleName);
    }
    if(module->isStatic && !scope->isStatic) {
        error(filename, line, "Cannot access static module from non static module");
    }
    struct symbolNode* member = map_get(module->children, memberName);
    if(member == NULL || member->isPrivate) {
        error(filename, line, "Unknown member %s in %s", memberName, moduleName);
    }
    return member;
}