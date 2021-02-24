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
static void validateAST(struct astNode*, const struct function* function, const struct module* module);
char* validateExpressionType(struct astNode*, const struct function*, const struct module*);
static void validateBinaryOp(struct list* children, char* leftType, char* rightType, const struct function* function, const struct module* module);
static void removeArray(char* str);

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
            validateAST(function->self.code, function, module);
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
    memset(temp, 0, 254);
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

static void validateAST(struct astNode* node, const struct function* function, const struct module* module) {
    if(node == NULL) return;

    struct listElem* elem;
    struct variable* var;

    switch(node->type) {
    case AST_BLOCK:
        break;
    case AST_VARDECLARE:
        var = (struct variable*) node->data;
        validateType(var->type, module);
        break;
    case AST_VARDEFINE: {
        var = (struct variable*) node->data;
        validateType(var->type, module);
        char *initType = validateExpressionType(((struct variable*) node->data)->code, function, module);
        if(strcmp(var->type, initType)) {
            error("Type mismatch when assigning to variable \"%s\"", var->name);
        }
        break;
    }
    default:
        error("Unsupported feature!");
    }

    for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
        validateAST((struct astNode*)elem->data, function, module);
    }
}

/*
    Recursively goes through expression, checks to make sure that the type of 
    the inputs is correct, returns output type based on input */
char* validateExpressionType(struct astNode* node, const struct function* function, const struct module* module) {
    char left[255], right[255];
    char* retval = (char*)malloc(sizeof(char) * 255);

    switch(node->type){
    // BASE CASES
    case AST_INTLITERAL:
        strcpy(retval, "int");
        return retval;
    case AST_REALLITERAL:
        strcpy(retval, "real");
        return retval;
    case AST_CHARLITERAL:
        strcpy(retval, "char");
        return retval;
    case AST_STRINGLITERAL:
        strcpy(retval, "char array");
        return retval;
    case AST_FALSE:
    case AST_TRUE:
        strcpy(retval, "boolean");
        return retval;
    case AST_VAR: {
        free(retval);
        char* varName = (char*)node->data;
        if(function != NULL) {
            if(map_get(function->varMap, varName) != NULL) {
                return ((struct variable*) map_get(function->varMap, varName))->type;
            } else if(map_get(function->argMap, varName) != NULL) {
                return ((struct variable*) map_get(function->argMap, varName))->type;
            }
        }
        if(map_get(module->globalsMap, varName) != NULL) {
            return ((struct variable*) map_get(module->globalsMap, varName))->type;
        }
        error("The variable \"%s\" is not visible", varName);
    }
    // RECURSIVE CASES
    case AST_ADD:
    case AST_SUBTRACT:
    case AST_MULTIPLY:
    case AST_DIVIDE: {
        validateBinaryOp(node->children, left, right, function, module);
        if((!strcmp(left, "real") && !strcmp(right, "real")) || 
            (!strcmp(left, "real") && !strcmp(right, "int")) || 
            (!strcmp(left, "int") && !strcmp(right, "real"))) {
            strcpy(retval, "real");
            return retval;
        } else if(!strcmp(left, "int") && !strcmp(right, "int")) {
            strcpy(retval, "int");
            return retval;
        } else {
            error("Type mismatch with %s or %s, expected int or real", left, right);
        }
    }
    case AST_ASSIGN: {
        struct astNode* leftAST = node->children->head.next->next->data;
        if(leftAST->type != AST_VAR) {
            error("Left side of assignment must be a location");
        }
        validateBinaryOp(node->children, left, right, function, module);
        if(strcmp(left, right)) {
            error("Type mismatch");
        }
        strcpy(retval, left);
        return retval;
    }
    case AST_OR:
    case AST_AND:
        validateBinaryOp(node->children, left, right, function, module);
         if(!strcmp(left, "boolean") && !strcmp(right, "boolean")) {
            strcpy(retval, "boolean");
            return retval;
        } else {
            error("Type mismatch with %s and %s, expected boolean", left, right);
        }
    case AST_GREATER:
    case AST_LESSER:{
        validateBinaryOp(node->children, left, right, function, module);
        if((!strcmp(left, "real") && !strcmp(right, "real")) || 
            (!strcmp(left, "real") && !strcmp(right, "int")) || 
            (!strcmp(left, "int") && !strcmp(right, "real")) ||
            (!strcmp(left, "int") && !strcmp(right, "int"))) {
            strcpy(retval, "boolean");
            return retval;
        } else {
            error("Type mismatch with %s or %s, expected int or real", left, right);
        }
    }
    case AST_IS:
    case AST_ISNT:
        strcpy(retval, "boolean");
        return retval;
    case AST_CALL: {
        if(strstr(node->data, " array")){
            strcpy(retval, node->data);
            removeArray(retval);
            return retval;
        } else if(map_get(module->program->dataStructsMap, node->data) != NULL) {
            struct dataStruct* dataStruct = map_get(module->program->dataStructsMap, node->data);
            struct list* params = map_getKeyList(dataStruct->fieldMap);
            struct list* args = node->children;
            struct listElem* paramElem;
            struct listElem* argElem;
            for(paramElem = list_begin(params), argElem = list_begin(args); 
                paramElem != list_end(params) && argElem != list_end(args); 
                paramElem = list_next(paramElem), argElem = list_next(argElem)) {
                char* paramType = ((struct variable*)map_get(dataStruct->fieldMap, ((char*)paramElem->data)))->type;
                char* argType = validateExpressionType((struct astNode*)argElem->data, function, module);
                if(strcmp(paramType, argType)) {
                    error("Wrong argument type for struct %s, was %s, expected %s", node->data, argType, paramType);
                }
            }
            if(argElem == list_end(args) && paramElem == list_end(params)) {
                strcpy(retval, node->data);
                return retval;
            } else if(argElem != list_end(args)){
                error("Too many arguments");
            } else {
                error("Too few arguments");
            }
        } else {
            struct function* callee = map_get(module->functionsMap, (char*)node->data);
            if(callee != NULL) {
                struct list* params = map_getKeyList(callee->argMap);
                struct list* args = node->children;
                struct listElem* paramElem;
                struct listElem* argElem;
                for(paramElem = list_begin(params), argElem = list_begin(args); 
                    paramElem != list_end(params) && argElem != list_end(args); 
                    paramElem = list_next(paramElem), argElem = list_next(argElem)) {
                    char* paramType = ((struct variable*)map_get(callee->argMap, ((char*)paramElem->data)))->type;
                    char* argType = validateExpressionType((struct astNode*)argElem->data, function, module);
                    if(strcmp(paramType, argType)) {
                        error("Wrong argument type for function %s, was %s, expected %s", node->data, argType, paramType);
                    }
                }
                if(argElem == list_end(args) && paramElem == list_end(params)) {
                    strcpy(retval, callee->self.type);
                    return retval;
                } else if(argElem != list_end(args)){
                    error("Too many arguments");
                } else {
                    error("Too few arguments");
                }
            } else {
                error("Unknown function %s", node->data);
            }
        }
    }
    case AST_MODULEACCESS: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct astNode* rightAST = node->children->head.next->data;
        if(leftAST->type != AST_VAR || (rightAST->type != AST_VAR && rightAST->type != AST_CALL)) {
            error("Malformed module access modifier");
        }
        struct module* newModule = map_get(module->program->modulesMap, leftAST->data);
        if(newModule == NULL) {
            error("Unknown module \"%s\"", leftAST->data);
        }
        if(map_get(newModule->functionsMap, rightAST->data) != NULL
            && ((struct function*)map_get(newModule->functionsMap, rightAST->data))->self.isPrivate) {
            error("Unknown function %s:%s", leftAST->data, rightAST->data);
        }
        if(map_get(newModule->globalsMap, rightAST->data) != NULL
            && ((struct variable*)map_get(newModule->globalsMap, rightAST->data))->isPrivate) {
            error("Unknown variable %s:%s", leftAST->data, rightAST->data);
        }
        return validateExpressionType(rightAST, NULL, newModule);
    }
    default:
        PANIC("AST %s is not an expression", parser_astToString(node->type));
    }
    return NULL;
}

static void validateBinaryOp(struct list* children, char* leftType, char* rightType, const struct function* function, const struct module* module) {
    strcpy(rightType, validateExpressionType(children->head.next->data, function, module));
    strcpy(leftType, validateExpressionType(children->head.next->next->data, function, module));
}

static void removeArray(char* str) {
    int length = strlen(str);
    for(int i = length - 1; i >= 0; i--){
        if(str[i] == 'a' && str[i-11] == ' '){
            str[i - 1] = '\0';
        }
    }
}