/*  validator.c

    The validator's job is to take in a program data structure, look through 
    it's components, and make sure (validate) that the program is correct.

    Author: Joseph Shimel
    Date: 2/19/21
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "./main.h"
#include "./validator.h"
#include "./parser.h"

#include "../util/map.h"
#include "../util/list.h"
#include "../util/debug.h"

static int findTypeEnd(const char*);
static void validateType(const char*, const struct module*);
static void validateAST(struct astNode*);

void validator_validate(struct program* program) {
    ASSERT(program != NULL);

    struct list* modules = map_getKeyList(program->modulesMap);   
    struct listElem* moduleElem;
    for(moduleElem = list_begin(modules); moduleElem != list_end(modules); moduleElem = list_next(moduleElem)) {
        struct module* module = map_get(program->modulesMap, (char*)moduleElem->data);

        // VALIDATE FUNCTIONS
        struct list* functions = map_getKeyList(module->functionsMap);
        struct listElem* functionElem;
        for(functionElem = list_begin(functions); functionElem != list_end(functions); functionElem = list_next(functionElem)) {
            struct function* function = map_get(module->functionsMap, (char*)functionElem->data);
            // VALIDATE TYPE EXISTS
            validateType(function->self.type, module);
            // VALIDATE CODE AST
            validateAST(function->self.code);
        }

        // VALIDATE GLOBALS
        struct list* globals = map_getKeyList(module->globalsMap);
        struct listElem* globalElem;
        for(globalElem = list_begin(globals); globalElem != list_end(globals); globalElem = list_next(globalElem)) {
            struct variable* global = map_get(module->globalsMap, (char*)globalElem->data);
            // VALIDATE STATE
            if(!module->isStatic && !global->isConstant) {
                error("Global variable \"%s\" declared in non-state module \"%s\"", global->name, module->name);
            }
            // VALIDATE TYPE
            // VALIDATE CODE AST
        }
    }
    return;
}

static int findTypeEnd(const char* type) {
    int i = 0;
    while (type[i] != '\0' && type[i] != ' ') i++;
    return i;
}

static void validateType(const char* type, const struct module* module) {
    // Check primitives
    int end = findTypeEnd(type);
    char temp[255];
    strncpy(temp, type, end);
    if(strcmp(temp, "int") != 0 && strcmp(temp, "char") != 0 && 
        strcmp(temp, "boolean") != 0 && strcmp(temp, "void") != 0 && 
        strcmp(temp, "real") != 0) {
        struct dataStruct* dataStruct = map_get(module->program->dataStructsMap, temp);
        if(dataStruct == NULL || (dataStruct->self.isPrivate && dataStruct->module != module)) {
            error("Unrecognized type \"%s\"", type);
        }
    }
}

static void validateAST(struct astNode* node) {
    if(node == NULL) return;

    struct listElem* elem;
    switch(node->type) {
    case AST_BLOCK:
        break;
    case AST_VARDECLARE:
        break;
    default:
        error("Unsupported feature!");
    }

    for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
        validateAST((struct astNode*)elem->data);
    }
}