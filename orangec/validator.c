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

#include "./validator.h"
#include "./parser.h"

#include "../util/map.h"
#include "../util/list.h"
#include "../util/debug.h"

static bool validateAST(struct astNode*, char* errMsg);

void validator_validate(struct program* program) {
    ASSERT(program != NULL);
    char errMsg[255];

    struct list* modules = map_getKeyList(program->modulesMap);   
    struct listElem* moduleElem;
    for(moduleElem = list_begin(modules); moduleElem != list_end(modules); moduleElem = list_next(moduleElem)) {
        struct list* functions = map_getKeyList(((struct module*)moduleElem->data)->functionsMap);
        struct listElem* functionElem;
        for(functionElem = list_begin(functions); functionElem != list_end(functions); functionElem = list_next(functionElem)) {
            if (!validateAST(((struct function*) functionElem->data)->code, errMsg)) {
                fprintf(stderr, "Error: %s\n", errMsg);
                exit(1);
            }
        }
    }
}

static bool validateAST(struct astNode* node, char* errMsg) {
    struct listElem* elem;
    switch(node->type) {
    case AST_BLOCK:
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            if(!validateAST((struct astNode*)elem->data, errMsg)) {
                return false;
            }
        }
        return true;
    default:
        strncpy(errMsg, "Unsupported feature!!", 254);
        return false;
    }
}