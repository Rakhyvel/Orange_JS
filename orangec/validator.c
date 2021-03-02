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

// Private functions
struct dataStruct* extendStructs(char*, const char*, int);
void validateStructFields(struct astNode*);
static int findTypeEnd(const char*);
static void validateType(const char*, const struct module*, const char*, int);
static void validateAST(struct astNode*, const struct function*, const struct module*);
char* validateExpressionAST(struct astNode*, const struct function*, const struct module*, int, int);
static void validateBinaryOp(struct list*, char*, char*, const struct function*, const struct module*, int, int);
static void removeArray(char*);
static struct variable* findGlobal(char* moduleName, char* name, const char* filename, int line);
static struct variable* findFunction(char* moduleName, char* name, const char* filename, int line);
static struct variable* findVariable(char*, struct block*, const char*, int);
static int validateParamType(struct list*, struct map*, const struct function*, const struct module*, int, int, const char*, int);
static char* validateStructField(char*, char*, const char*, int);

/*
    Takes in a program, looks through structures, modules, globals, and 
    functions, and validates that they make sense as an Orange program */
void validator_validate() {
    ASSERT(program != NULL);

    int hasStart = 0;

    struct list* dataStructs = map_getKeyList(program->dataStructsMap);   
    struct listElem* dataStructElem;
    for(dataStructElem = list_begin(dataStructs); dataStructElem != list_end(dataStructs); dataStructElem = list_next(dataStructElem)) {
        struct dataStruct* dataStruct = map_get(program->dataStructsMap, (char*)dataStructElem->data);
        extendStructs(dataStruct->self.name, dataStruct->self.filename, dataStruct->self.line);
        validateStructFields(dataStruct->definition->self.code);
    }

    struct list* modules = map_getKeyList(program->modulesMap);   
    struct listElem* moduleElem;
    for(moduleElem = list_begin(modules); moduleElem != list_end(modules); moduleElem = list_next(moduleElem)) {
        struct module* module = map_get(program->modulesMap, (char*)moduleElem->data);

        // VALIDATE GLOBALS
        struct list* globals = map_getKeyList(module->block->varMap);
        struct listElem* globalElem;
        for(globalElem = list_begin(globals); globalElem != list_end(globals); globalElem = list_next(globalElem)) {
            struct variable* global = map_get(module->block->varMap, (char*)globalElem->data);
            // VALIDATE STATE
            if(!module->isStatic && !global->isConstant) {
                error(global->filename, global->line, "Global variable \"%s\" declared in non-state module \"%s\"", global->name, module->name);
            }
            // VALIDATE TYPE
            validateType(global->type, module, global->filename, global->line);
            // VALIDATE CODE AST
            global->isDeclared = 1;
            if(global->code == NULL) {
                error(global->filename, global->line, "Global \"%s\" was not initialized");
            }
            char *initType = validateExpressionAST(global->code, NULL, module, 1, 1);
            if(strcmp(global->type, initType)) {
                error(global->filename, global->line, "Value type mismatch when assigning to variable \"%s\". Expected \"%s\" type, actual type was \"%s\" ", global->name, global->type, initType);
            }
        }

        // VALIDATE FUNCTIONS
        struct list* functions = map_getKeyList(module->functionsMap);
        struct listElem* functionElem;
        for(functionElem = list_begin(functions); functionElem != list_end(functions); functionElem = list_next(functionElem)) {
            struct function* function = map_get(module->functionsMap, (char*)functionElem->data);
            // VALIDATE TYPE EXISTS
            validateType(function->self.type, module, function->self.filename, function->self.line);
            // VALIDATE CODE AST
            validateAST(function->self.code, function, module);
            if(!strcmp(function->self.name, "start")) {
                if(hasStart) {
                    error(function->self.filename, function->self.line, "Too many starting points");
                } else {
                    hasStart = 1;
                }
            }
        }
    }
    if(!hasStart) {
        error(NULL, 0, "No starting point (function named \"start\") for program found\n");
    }
    return;
}

/*
    Takes in a name of a struct, copies in it's parent struct's fields if any, 
    and copies in the parent struct's ancestor structs */
struct dataStruct* extendStructs(char* name, const char* filename, int line) {
    struct dataStruct* dataStruct = map_get(program->dataStructsMap, name);
    if(dataStruct == NULL) {
        return NULL;
    }
    if(dataStruct->self.type[0] != '\0') {
        struct dataStruct* parent = extendStructs(dataStruct->self.type, filename, line);
        if(parent == NULL) {
            error(filename, line, "Struct \"%s\" inherits from unknown struct \"%s\"", name, dataStruct->self.type);
        }
        map_copy(dataStruct->parentSet, parent->parentSet);
        struct map* newFieldMap = map_create();
        struct map* oldMap = dataStruct->definition->block->varMap;
        map_copy(newFieldMap, parent->definition->block->varMap);
        map_copy(newFieldMap, dataStruct->definition->block->varMap);
        dataStruct->definition->block->varMap = newFieldMap;
        map_destroy(oldMap);
    }
    set_add(dataStruct->parentSet, name);
    return dataStruct;
}

void validateStructFields(struct astNode* block) {
    struct listElem* elem;
    for(elem = list_begin(block->children); elem != list_end(block->children); elem = list_next(elem)) {
        struct astNode* node = ((struct astNode*) elem->data);
        if(node->type != AST_VARDEFINE) {
            error(node->filename, node->line, "Structs may only define field definitions");
        }
    }
}

/*
    Returns the position where a type ends, used to find array's base type 
    
    Ex:
    "int"             -> findTypeEnd -> "int|"
    "int array"       -> findTypeEnd -> "int| array"
    "int array array" -> findTypeEnd -> "int| array array"
    where | represents the index returned
*/
static int findTypeEnd(const char* type) {
    int i = 0;
    while (type[i] != '\0' && type[i] != ' ') i++;
    return i;
}

/*
    Checks to see if a type is a primitive, built in type*/
static int isPrimitive(const char* type) {
    if(!strcmp(type,  "int") || !strcmp(type, "char") ||
        !strcmp(type, "boolean") || !strcmp(type, "void") || 
        !strcmp(type, "real") || !strcmp(type, "byte") || 
        !strcmp(type, "struct")) {
        return 1;
    } else {
        return 0;
    }
}

/*
    Checks that a type exists either as a primitive type, or as a user defined struct */
static void validateType(const char* type, const struct module* module, const char* filename, int line) {
    // Check primitives
    int end = findTypeEnd(type);
    char temp[255];
    memset(temp, 0, 254);
    strncpy(temp, type, end);
    if(!isPrimitive(temp)) {
        struct dataStruct* dataStruct = map_get(program->dataStructsMap, temp);
        if(dataStruct == NULL || (dataStruct->self.isPrivate && dataStruct->module != module)) {
            error(filename, line, "Unknown type \"%s\" ", type);
        }
    }
}

/*
    Determines if two types are the same. For structs, the expected type may 
    be a parent struct of the actual struct */
static int typesMatch(const char* expected, const char* actual, const char* filename, int line) {
    if(isPrimitive(expected)) {
        return !strcmp(expected, actual);
    } else if(strstr(expected, " array") != NULL){
        char expectedBase[255];
        char actualBase[255];
        if(strcmp(expected, actual)){
            return 0;
        }
        strncpy(expectedBase, expected, findTypeEnd(expected));
        strncpy(actualBase, actual, findTypeEnd(actual));
        return !strcmp(expectedBase, actualBase);
    } else {
        struct dataStruct* dataStruct = map_get(program->dataStructsMap, actual);
        if(dataStruct == NULL) {
            error(filename, line, "Unknown struct \"%s\" ", actual);
        }
        return set_contains(dataStruct->parentSet, expected);
    }
}

/*
    Takes in an AST and checks to make sure that it is correct */
static void validateAST(struct astNode* node, const struct function* function, const struct module* module) {
    if(node == NULL) return;

    struct listElem* elem;
    struct variable* var;
    LOG("Validating AST \"%s\" ", parser_astToString(node->type));

    switch(node->type) {
    case AST_BLOCK:
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            validateAST((struct astNode*)elem->data, function, module);
        }
        break;
    case AST_VARDECLARE:
        var = (struct variable*) node->data;
        var->isDeclared = 1;
        validateType(var->type, module, var->filename, var->line);
        break;
    case AST_VARDEFINE: {
        var = (struct variable*) node->data;
        var->isDeclared = 1;
        validateType(var->type, module, var->filename, var->line);
        char *initType = validateExpressionAST(var->code, function, module, 0, 0);
        if(!typesMatch(var->type, initType, var->filename, var->line)) {
            error(var->filename, var->line, "Value type mismatch when assigning to variable \"%s\". Expected \"%s\" type, actual type was \"%s\" ", var->name, var->type, initType);
        }
        break;
    }
    case AST_IF: {
        struct astNode* condition = node->children->head.next->data;
        struct astNode* block = node->children->head.next->next->data;
        char *conditionType = validateExpressionAST(condition, function, module, 0, 0);
        if(strcmp("boolean", conditionType)) {
            error(condition->filename, condition->line, "If expected boolean type, actual type was \"%s\" ", conditionType);
        }
        validateAST(block, function, module);
        break;
    }
    case AST_IFELSE: {
        struct astNode* condition = node->children->head.next->data;
        struct astNode* block = node->children->head.next->next->data;
        char *conditionType = validateExpressionAST(condition, function, module, 0, 0);
        if(strcmp("boolean", conditionType)) {
            error(condition->filename, condition->line, "If expected boolean type, actual type was \"%s\" ", conditionType);
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
        char *conditionType = validateExpressionAST(condition, function, module, 0, 0);
        if(strcmp("boolean", conditionType)) {
            error(condition->filename, condition->line,"While expected boolean type, actual type was \"%s\" ", conditionType);
        }
        validateAST(block, function, module);
        break;
    }
    case AST_RETURN: {
        struct astNode* retval = node->children->head.next->data;
        if(retval == NULL) {
            if(strcmp(function->self.type, "void")) {
                error(retval->filename, retval->line,"Cannot return value from void type function");
            }
        } else {
            char *returnType = validateExpressionAST(retval, function, module, 0, 0);
            if(strcmp(function->self.type, returnType)) {
                error(retval->filename, retval->line,"Return values do not match, expected \"%s\" , actual was \"%s\" ", function->self.type, returnType);
            }
        }
        break;
    }
    default:
        LOG("%p", validateExpressionAST(node, function, module, 0, 0));
    }
}

/*
    Recursively goes through expression, checks to make sure that the type of 
    the inputs is correct, returns output type based on input */
char* validateExpressionAST(struct astNode* node, const struct function* function, const struct module* module, int isGlobal, int external) {
    char left[255], right[255];
    char* retval = (char*)malloc(sizeof(char) * 255);

    LOG("Validating expression \"%s\" ", parser_astToString(node->type));
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
        struct variable* var = findVariable(node->data, function->block, node->filename, node->line);
        strcpy(retval, var->type);
        return retval;
    }
    // RECURSIVE CASES
    case AST_ADD:
    case AST_SUBTRACT:
    case AST_MULTIPLY:
    case AST_DIVIDE: {
        validateBinaryOp(node->children, left, right, function, module, isGlobal, external);
        if((!strcmp(left, "real") && !strcmp(right, "real")) || 
            (!strcmp(left, "real") && !strcmp(right, "int")) || 
            (!strcmp(left, "int") && !strcmp(right, "real"))) {
            strcpy(retval, "real");
            return retval;
        } else if(!strcmp(left, "int") && !strcmp(right, "int")) {
            strcpy(retval, "int");
            return retval;
        } else {
            error(node->filename, node->line, "Value type mismatch. Expected int or real type, actual types were \"%s\" and \"%s\" ", left, right);
        }
    }
    case AST_ASSIGN: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct variable* var = NULL; // Needs to have a var to assign to
        if(leftAST->type != AST_VAR && leftAST->type != AST_DOT && leftAST->type != AST_INDEX && leftAST->type != AST_MODULEACCESS) {
            error(node->filename, node->line, "Left side of assignment must be a location");
        }
        // Constant assignment validation- find variable associated with assignment
        if(leftAST->type == AST_VAR) {
            var = findVariable(leftAST->data, function->block, node->filename, node->line);
        } else if(leftAST->type == AST_MODULEACCESS) {
            struct astNode* moduleIdent = leftAST->children->head.next->next->data;
            struct astNode* nameIdent = leftAST->children->head.next->data;
            if(nameIdent->type != AST_VAR) {
                error(node->filename, node->line, "Left side of assignment must be a location");
            }
            var = findGlobal(moduleIdent->data, nameIdent->data, node->filename, node->line);
        }
        // Indexing is not checked, you can change the contents of an array if it is constant
        if(var != NULL && var->isConstant) {
            error(node->filename, node->line, "Cannot assign to constant \"%s\" ", var->name);
        }
        // Left/right type matching
        validateBinaryOp(node->children, left, right, function, module, isGlobal, external);
        if(!typesMatch(left, right, node->filename, node->line)) {
            error(node->filename, node->line, "Value type mismatch. Expected \"%s\" type, actual type was \"%s\" ", left, right);
        }
        strcpy(retval, left);
        return retval;
    }
    case AST_OR:
    case AST_AND:
        validateBinaryOp(node->children, left, right, function, module, isGlobal, external);
         if(!strcmp(left, "boolean") && !strcmp(right, "boolean")) {
            strcpy(retval, "boolean");
            return retval;
        } else {
            error(node->filename, node->line, "Value type mismatch. Expected boolean type, actual types were \"%s\" and \"%s\" ", left, right);
        }
    case AST_GREATER:
    case AST_LESSER:{
        validateBinaryOp(node->children, left, right, function, module, isGlobal, external);
        if((!strcmp(left, "real") && !strcmp(right, "real")) || 
            (!strcmp(left, "real") && !strcmp(right, "int")) || 
            (!strcmp(left, "int") && !strcmp(right, "real")) ||
            (!strcmp(left, "int") && !strcmp(right, "int"))) {
            strcpy(retval, "boolean");
            return retval;
        } else {
            error(node->filename, node->line, "Value type mismatch. Expected int or real type, actual types were \"%s\" and \"%s\" ", left, right);
        }
    }
    case AST_IS:
    case AST_ISNT:
        strcpy(retval, "boolean");
        return retval;
    case AST_CALL: {
        struct dataStruct* dataStruct = map_get(program->dataStructsMap, node->data);
        struct function* callee = map_get(module->functionsMap, (char*)node->data);
        // ARRAY LITERAL
        if(strstr(node->data, " array")){
            strcpy(retval, node->data);
            return retval;
        }
        // STRUCT INIT
        else if(dataStruct != NULL) {
            int err = validateParamType(node->children, dataStruct->definition->argBlock->varMap, function, module, isGlobal, external, node->filename, node->line);
            if(!err) {
                strcpy(retval, node->data);
                return retval;
            } else if(err > 0){
                error(node->filename, node->line, "Too many arguments for struct \"%s\" initialization", dataStruct->self.name);
            } else if(err < 0){
                error(node->filename, node->line, "Too few arguments for struct \"%s\" initialization", dataStruct->self.name);
            }
        } 
        // FUNCTION CALL
        else if(callee != NULL) {
            if(isGlobal && module->isStatic) {
                error(node->filename, node->line, "Cannot call a static function from global scope");
            }
            int err = validateParamType(node->children, callee->argBlock->varMap, function, module, isGlobal, external, node->filename, node->line);
            
            if(!err) {
                strcpy(retval, callee->self.type);
                return retval;
            } else if(err > 0){
                error(node->filename, node->line, "Too many arguments for function call");
            } else if(err < 0){
                error(node->filename, node->line, "Too few arguments for function call");
            }
        } else {
            error(node->filename, node->line, "Unknown function or struct \"%s\" ", node->data);
        }
    }
    case AST_DOT: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct astNode* rightAST = node->children->head.next->data;

        char* type = validateExpressionAST(leftAST, function, module, isGlobal, external);
        strcpy(retval, validateStructField(type, rightAST->data, node->filename, node->line));
        return retval;
    }
    case AST_INDEX: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct astNode* rightAST = node->children->head.next->data;
        // SIZE ARRAY INIT
        if(leftAST->type == AST_VAR && strstr(leftAST->data, " array")){
            strcpy(retval, leftAST->data);
            return retval;
        } 
        // ARRAY INDEXING
        else {
            char* rightType = validateExpressionAST(rightAST, function, module, isGlobal, external);
            if(strcmp(rightType, "int")) {
                error(node->filename, node->line, "Value type mismatch when indexing array. Expected int type, actual type was \"%s\" ", rightType);
            }

            char* leftType = validateExpressionAST(leftAST, function, module, isGlobal, external);
            if(!strstr(leftType, " array")) {
                error(node->filename, node->line, "Value type mismatch when indexing array. Expected array type, actual type was \"%s\" ", leftType);
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
        if(leftAST->type != AST_VAR) {
            error(node->filename, node->line, "Left side of module access operator must be module name");
        } 
        if(rightAST->type != AST_VAR && rightAST->type != AST_CALL) {
            error(node->filename, node->line, "Right side of module access operator must be varibale or function name");
        }

        // Check module exists
        struct module* newModule = map_get(program->modulesMap, leftAST->data);
        if(newModule == NULL) {
            error(node->filename, node->line, "Unknown module \"%s\"", leftAST->data);
        }
        // Check that module is fine
        if((!module->isStatic || isGlobal) && newModule->isStatic) {
            error(node->filename, node->line, "Cannot access static members from a non static module");
        }
        // Check either function/variable exist
        struct variable* var = findGlobal(newModule->name, rightAST->data, node->filename, node->line);
        struct variable* func = findFunction(newModule->name, rightAST->data, node->filename, node->line);
        if(var != NULL && rightAST->type == AST_VAR) {
            return var->type;
        } else if (func != NULL && rightAST->type == AST_CALL) {
            return func->type;
        } else if(rightAST->type == AST_CALL) {
            error(node->filename, node->line, "Module \"%s\" does not contain function \"%s\"", leftAST->data, rightAST->data);
        } else if(rightAST->type == AST_VAR) {
            error(node->filename, node->line, "Module \"%s\" does not contain variable \"%s\"", leftAST->data, rightAST->data);
        }
    }
    default:
        PANIC("AST \"%s\" is not an expression", "lol");
    }
    return NULL;
}

/*
    Copies the types of both the left expression and right expression to given strings. */
static void validateBinaryOp(struct list* children, char* leftType, char* rightType, const struct function* function, const struct module* module, int isGlobal, int external) {
    strcpy(rightType, validateExpressionAST(children->head.next->data, function, module, isGlobal, external));
    strcpy(leftType, validateExpressionAST(children->head.next->next->data, function, module, isGlobal, external));
}

/*
    Takes in an array type, returns the lower level type (the type without the "array" modifier) */
static void removeArray(char* str) {
    ASSERT(strstr(str, " array"));
    int length = strlen(str);
    for(int i = length - 1; i >= 0; i--){
        if(str[i] == 'a' && str[i-1] == ' '){
            str[i - 1] = '\0';
            return;
        }
    }
}

static struct variable* findGlobal(char* modName, char* name, const char* filename, int line) {
    struct module* module = map_get(program->modulesMap, modName);
    if(module == NULL) {
        return NULL;
    } else {
        struct variable* var = map_get(module->block->varMap, name);
        if(var == NULL || var->isPrivate) {
            return NULL;
        } else {
            return var;
        }
    }
    return NULL;
}

static struct variable* findFunction(char* modName, char* name, const char* filename, int line) {
    struct module* module = map_get(program->modulesMap, modName);
    if(module == NULL) {
        return NULL;
    } else {
        struct variable* var = (struct variable*)map_get(module->functionsMap, name);
        if(var == NULL || var->isPrivate) {
            return NULL;
        } else {
            return var;
        }
    }
    return NULL;
}

/*
    Takes in a name, function, and module, and returns a variable, if it can be found */
static struct variable* findVariable(char* name, struct block* block, const char* filename, int line) {
    if(block == NULL) {
        error(filename, line, "Unknown variable \"%s\"", name);
    } else {
        struct variable* var = map_get(block->varMap, name);
        if(var == NULL) {
            return findVariable(name, block->parent, filename, line);
        } else {
            return var;
        }
    }
    return NULL;
}

/*
    Takes in a list of AST's representing expressions for arguments, checks each of their types against a list of given
    parameters. 
    
    Returns: 1 when too many args, 
            -1 when too few args,
             0 when okay args. */
static int validateParamType(struct list* args, struct map* paramMap, const struct function* function, const struct module* module, int isGlobal, int external,const char* filename, int line) {
    struct list* params = map_getKeyList(paramMap);
    struct listElem* paramElem;
    struct listElem* argElem;
    for(paramElem = list_begin(params), argElem = list_begin(args); 
        paramElem != list_end(params) && argElem != list_end(args); 
        paramElem = list_next(paramElem), argElem = list_next(argElem)) {
        char* paramType = ((struct variable*)map_get(paramMap, ((char*)paramElem->data)))->type;
        char* argType = validateExpressionAST((struct astNode*)argElem->data, function, module, isGlobal, external);
        if(strcmp(paramType, argType)) {
            error(filename, line, "Value type mismatch when passing argument to function. Expected \"%s\" type, actual type was \"%s\" ", argType, paramType);
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

/*
    Checks to see if a struct contains a field, or if a super struct contains the field. */
static char* validateStructField(char* structName, char* fieldName, const char* filename, int line) {
    struct dataStruct* dataStruct = map_get(program->dataStructsMap, structName);
    if(dataStruct == NULL) {
        error(filename, line, "Unknown struct \"%s\" ", structName);
    }
    if(map_get(dataStruct->definition->block->varMap, fieldName) == NULL) {
        error(filename, line, "Unknown field \"%s\" for struct \"%s\" ", fieldName, structName);
    }

    return ((struct variable*)map_get(dataStruct->definition->block->varMap, fieldName))->type;
   
    return NULL;
}