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
static void validateAST(struct astNode*, const struct function*, const struct module*);
char* validateExpressionAST(struct astNode*, const struct function*, const struct module*, int);
static void validateBinaryOp(struct list*, char*, char*, const struct function*, const struct module*, int);
static void removeArray(char* str);
static int validateParamType(struct list*, struct map*, const struct function*, const struct module*, int);

void validator_validate(struct program* program) {
    ASSERT(program != NULL);

    struct list* modules = map_getKeyList(program->modulesMap);   
    struct listElem* moduleElem;
    for(moduleElem = list_begin(modules); moduleElem != list_end(modules); moduleElem = list_next(moduleElem)) {
        struct module* module = map_get(program->modulesMap, (char*)moduleElem->data);

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
            validateType(global->type, module);
            // VALIDATE CODE AST
            global->isDeclared = 1;
            char *initType = validateExpressionAST(global->code, NULL, module, 1);
            if(strcmp(global->type, initType)) {
                error("Type mismatch when assigning to variable \"%s\"", global->name);
            }
        }

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
    }
    return;
}

/*
    Returns the position where a type ends, used to find array's base type 
    
    Ex:
    "int"       -> findTypeEnd -> "int|"
    "int array" -> findTypeEnd -> "int| array"
    where | represents the index returned
*/
static int findTypeEnd(const char* type) {
    int i = 0;
    while (type[i] != '\0' && type[i] != ' ') i++;
    return i;
}

/*
    Checks that a type exists either as a primitive type, or as a user defined struct */
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
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            validateAST((struct astNode*)elem->data, function, module);
        }
        break;
    case AST_VARDECLARE:
        var = (struct variable*) node->data;
        var->isDeclared = 1;
        validateType(var->type, module);
        break;
    case AST_VARDEFINE: {
        var = (struct variable*) node->data;
        var->isDeclared = 1;
        validateType(var->type, module);
        char *initType = validateExpressionAST(var->code, function, module, 0);
        if(strcmp(var->type, initType)) {
            error("Type mismatch when assigning to variable \"%s\"", var->name);
        }
        break;
    }
    case AST_IF: {
        struct astNode* condition = node->children->head.next->data;
        struct astNode* block = node->children->head.next->next->data;
        char *conditionType = validateExpressionAST(condition, function, module, 0);
        if(strcmp("boolean", conditionType)) {
            error("If statement must take in boolean type");
        }
        validateAST(block, function, module);
        if(node->children->head.next->next != list_end(node->children)) {
            struct astNode* elseBlock = node->children->head.next->next->next->data;
            validateAST(elseBlock, function, module);
        }
        break;
    }
    case AST_WHILE: {
        struct astNode* condition = node->children->head.next->data;
        struct astNode* block = node->children->head.next->next->data;
        char *conditionType = validateExpressionAST(condition, function, module, 0);
        if(strcmp("boolean", conditionType)) {
            error("While statement must take in boolean type");
        }
        validateAST(block, function, module);
        break;
    }
    case AST_RETURN: {
        struct astNode* retval = node->children->head.next->data;
        if(retval == NULL) {
            if(strcmp(function->self.type, "void")) {
                error("Return values do not match");
            }
        } else {
            char *returnType = validateExpressionAST(retval, function, module, 0);
            if(strcmp(function->self.type, returnType)) {
                error("Return values do not match");
            }
        }
        break;
    }
    default:
        LOG("%p", validateExpressionAST(node, function, module, 0));
    }
}

/*
    Recursively goes through expression, checks to make sure that the type of 
    the inputs is correct, returns output type based on input */
char* validateExpressionAST(struct astNode* node, const struct function* function, const struct module* module, int isGlobal) {
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
        char* varName = (char*)node->data;
        // Function variable/argument
        if(function != NULL) {
            struct variable* var = map_get(function->varMap, varName);
            struct variable* arg = map_get(function->argMap, varName);
            if(var != NULL && var->isDeclared) {
                strcpy(retval, ((struct variable*) map_get(function->varMap, varName))->type);
                return retval;
            } else if(arg != NULL) {
                strcpy(retval, ((struct variable*) map_get(function->argMap, varName))->type);
                return retval;
            }
        }
        // Module global variable
        if(map_get(module->globalsMap, varName) != NULL) {
            strcpy(retval, ((struct variable*) map_get(module->globalsMap, varName))->type);
            return retval;
        }
        error("The variable \"%s\" is not visible", varName);
    }
    // RECURSIVE CASES
    case AST_ADD:
    case AST_SUBTRACT:
    case AST_MULTIPLY:
    case AST_DIVIDE: {
        validateBinaryOp(node->children, left, right, function, module, isGlobal);
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
        if(leftAST->type != AST_VAR && leftAST->type != AST_DOT && leftAST->type != AST_INDEX && leftAST->type != AST_MODULEACCESS) {
            error("Left side of assignment must be a location");
        }
        validateBinaryOp(node->children, left, right, function, module, isGlobal);
        if(strcmp(left, right)) {
            error("Type mismatch");
        }
        strcpy(retval, left);
        return retval;
    }
    case AST_OR:
    case AST_AND:
        validateBinaryOp(node->children, left, right, function, module, isGlobal);
         if(!strcmp(left, "boolean") && !strcmp(right, "boolean")) {
            strcpy(retval, "boolean");
            return retval;
        } else {
            error("Type mismatch with %s and %s, expected boolean", left, right);
        }
    case AST_GREATER:
    case AST_LESSER:{
        validateBinaryOp(node->children, left, right, function, module, isGlobal);
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
        struct dataStruct* dataStruct = map_get(module->program->dataStructsMap, node->data);
        struct function* callee = map_get(module->functionsMap, (char*)node->data);
        // ARRAY LITERAL
        if(strstr(node->data, " array")){
            strcpy(retval, node->data);
            return retval;
        } 
        // STRUCT INIT
        else if(dataStruct != NULL) {
            int err = validateParamType(node->children, dataStruct->fieldMap, function, module, isGlobal);
            if(!err) {
                strcpy(retval, node->data);
                return retval;
            } else if(err > 0){
                error("Too many arguments");
            } else if(err < 0){
                error("Too few arguments");
            }
        } 
        // FUNCTION CALL
        else if(callee != NULL) {
            int err = validateParamType(node->children, callee->argMap, function, module, isGlobal);
            if(!err) {
                strcpy(retval, callee->self.type);
                return retval;
            } else if(err > 0){
                error("Too many arguments");
            } else if(err < 0){
                error("Too few arguments");
            }
        } else {
            error("Unknown function or struct %s", node->data);
        }
    }
    case AST_DOT: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct astNode* rightAST = node->children->head.next->data;

        struct dataStruct* dataStruct = map_get(module->program->dataStructsMap, validateExpressionAST(leftAST, function, module, isGlobal));
        if(dataStruct == NULL) {
            error("%s is not a struct!", leftAST->data);
        }
        if(map_get(dataStruct->fieldMap, rightAST->data) == NULL) {
            error("Unknown field %s!", rightAST->data);
        }
        strcpy(retval, ((struct variable*)map_get(dataStruct->fieldMap, rightAST->data))->type);
        return retval;
    }
    case AST_INDEX: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct astNode* rightAST = node->children->head.next->data;
        // SIZE ARRAY INIT
        if(strstr(leftAST->data, " array")){
            strcpy(retval, leftAST->data);
            return retval;
        } 
        // ARRAY INDEXING
        else {
            if(rightAST->type != AST_INTLITERAL) {
                error("Right side of index must be an integer");
            }

            char* leftType = validateExpressionAST(leftAST, function, module, isGlobal);
            if(!strstr(leftType, " array")) {
                error("Left side must be an array type");
            }

            strcpy(retval,leftType);
            removeArray(retval);
            return retval;
        }
    }
    case AST_MODULEACCESS: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct astNode* rightAST = node->children->head.next->data;
        // Check ast type's are correct (left=var, right={var, call})
        if(leftAST->type != AST_VAR || (rightAST->type != AST_VAR && rightAST->type != AST_CALL)) {
            error("Malformed module access modifier");
        }
        // Check module exists
        struct module* newModule = map_get(module->program->modulesMap, leftAST->data);
        if(newModule == NULL) {
            error("Unknown module \"%s\"", leftAST->data);
        }
        // Check that module is fine
        if((!module->isStatic || isGlobal) && newModule->isStatic) {
            error("Cannot access static members from a non static module");
        }
        // Check either function/variable exist
        int funcExists = (map_get(newModule->functionsMap, rightAST->data) != NULL && 
                        !((struct function*)map_get(newModule->functionsMap, rightAST->data))->self.isPrivate);
        int varExists = (map_get(newModule->globalsMap, rightAST->data) != NULL &&
                        !((struct variable*)map_get(newModule->globalsMap, rightAST->data))->isPrivate);
        if(funcExists || varExists) {
            return validateExpressionAST(rightAST, NULL, newModule, isGlobal);
        } else if(rightAST->type == AST_CALL) {
            error("Unknown function %s:%s", leftAST->data, rightAST->data);
        } else if(rightAST->type == AST_VAR) {
            error("Unknown variable %s:%s", leftAST->data, rightAST->data);
        }
    }
    default:
        PANIC("AST %s is not an expression", parser_astToString(node->type));
    }
    return NULL;
}

/*
    Copies the types of both the left expression and right expression to given strings. */
static void validateBinaryOp(struct list* children, char* leftType, char* rightType, const struct function* function, const struct module* module, int isGlobal) {
    strcpy(rightType, validateExpressionAST(children->head.next->data, function, module, isGlobal));
    strcpy(leftType, validateExpressionAST(children->head.next->next->data, function, module, isGlobal));
}

/*
    Takes in an array type, returns the lower level type (the type without the "array" modifier) */
static void removeArray(char* str) {
    ASSERT(strstr(str, " array"));
    int length = strlen(str);
    for(int i = length - 1; i >= 0; i--){
        if(str[i] == 'a' && str[i-1] == ' '){
            str[i - 1] = '\0';
        }
    }
}

/*
    Takes in a list of AST's representing expressions for arguments, checks each of their types against a list of given
    parameters. 
    
    Returns: 1 when too many args, 
            -1 when too few args,
             0 when okay args. */
static int validateParamType(struct list* args, struct map* paramMap, const struct function* function, const struct module* module, int isGlobal) {
    struct list* params = map_getKeyList(paramMap);
    struct listElem* paramElem;
    struct listElem* argElem;
    for(paramElem = list_begin(params), argElem = list_begin(args); 
        paramElem != list_end(params) && argElem != list_end(args); 
        paramElem = list_next(paramElem), argElem = list_next(argElem)) {
        char* paramType = ((struct variable*)map_get(paramMap, ((char*)paramElem->data)))->type;
        char* argType = validateExpressionAST((struct astNode*)argElem->data, function, module, isGlobal);
        if(strcmp(paramType, argType)) {
            error("Wrong argument type %s, expected %s", argType, paramType);
        }
    }
    if(argElem != list_end(args)) {
        return 1;
    } else if(paramElem != list_end(params)){
        return -1;
    } else {
        return 0;
    }
}