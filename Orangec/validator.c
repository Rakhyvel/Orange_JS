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
static int findTypeEnd(const char*);
static struct symbolNode* findSymbol(const char*, const struct symbolNode*, const char*, int);
static int validateType(const char*, const struct symbolNode*, const char*, int);
static void validateBinaryOp(struct list*, char*, char*);
static struct symbolNode* findExplicitSymbol(char*, char*, const struct symbolNode*, const char*, int);
static int validateArrayType(struct list*, char*, const char*, int);
static int validateParamType(struct list*, struct map*, const char*, int);
static char* validateStructField(char*, char*, const char*, int);
static char* validateExpressionAST(struct astNode* node);
static void removeArray(char*);
static int typesMatch(const char*, const char*, const char*, int);
static void validateAST(struct astNode*);

/*
    Takes in a program, looks through structures, modules, globals, and 
    functions, and validates that they make sense as an Orange program */
void validator_validate(struct symbolNode* symbolNode) {
    struct list* children = symbolNode->children->keyList;
    struct listElem* elem = list_begin(children);
    switch(symbolNode->symbolType) {
    case SYMBOL_PROGRAM: {
        for(;elem != list_end(children); elem = list_next(elem)) {
            struct symbolNode* child = (struct symbolNode*)elem->data;
            if(child->symbolType != SYMBOL_MODULE) {
                error(child->filename, child->line, "Must be defined inside a module");
            }
            validator_validate((struct symbolNode*)elem->data);
        }
    } break;
    case SYMBOL_MODULE: {
        for(;elem != list_end(children); elem = list_next(elem)) {
            struct symbolNode* child = (struct symbolNode*)elem->data;
            if(child->symbolType == SYMBOL_BLOCK) {
                error(child->filename, child->line, "Module members must be structs, variables, or functions");
            }
            validator_validate(child);
        }
    } break;
    case SYMBOL_VARIABLE:
    case SYMBOL_FUNCTION: {
        // Valid type
        validateType(symbolNode->type, symbolNode->parent, symbolNode->filename, symbolNode->line);
        // Valid AST
        validateAST(symbolNode->code);
        // All children must be valid
        for(;elem != list_end(children); elem = list_next(elem)) {
            validator_validate((struct symbolNode*)elem->data);
        }
    } break;
    case SYMBOL_STRUCT:
    case SYMBOL_BLOCK: {
        // All children must be valid
        for(;elem != list_end(children); elem = list_next(elem)) {
            validator_validate((struct symbolNode*)elem->data);
        }
    } break;
    default:
        error("", 1, "Bad");
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
    Checks to see if a given type, for a given scope, is valid */
static int validateType(const char* type, const struct symbolNode* scope, const char* filename, int line) {
    // Check primitives
    int end = findTypeEnd(type);
    char temp[255];
    memset(temp, 0, 254);
    strncpy(temp, type, end);
    // If the type isn't private, check to see if there is a struct defined with the name
    if(!isPrimitive(temp)) {
        struct symbolNode* dataStruct = findSymbol(type, scope, filename, line);
        if(dataStruct == NULL || dataStruct->symbolType != SYMBOL_STRUCT) {
            error(filename, line, "Unknown type %s", type);
        }
    }
    return 1;
}

/*
    Determines if two types are the same. For structs, the expected type may 
    be a parent struct of the actual struct 
    
    USE THIS FUNCTION for comapring two unknown types. Use strcmp ONLY if the 
    expected type is fixed and known. */
static int typesMatch(const char* expected, const char* actual, const char* filename, int line) {
    if(isPrimitive(expected) || isPrimitive(actual)) {
        return !strcmp(expected, actual);
    } else if (!isPrimitive(expected) && !strcmp(actual, "None")) {
        return 1;
    } else if (!strcmp(expected, "Any") && !isPrimitive(actual)) {
        return 1;
    } else if(strstr(expected, " array")){
        char expectedBase[255];
        char actualBase[255];
        if(strcmp(expected, actual)){
            return 0;
        }
        strcpy(expectedBase, expected);
        strcpy(actualBase, actual);
        removeArray(expectedBase);
        removeArray(actualBase);
        return typesMatch(expectedBase, actualBase, filename, line);
    } else {
        // here type must be struct
        if(map_get(program->children, actual) == NULL) {
            error(filename, line, "Unknown struct \"%s\" ", actual);
        } else {
            return !strcmp(expected, actual);
        }
    }
    return 1;
}

/*
    Takes in an AST and checks to make sure that it is correct */
static void validateAST(struct astNode* node) {
    if(node == NULL) return;

    struct listElem* elem;
    struct symbolNode* var;
    LOG("Validating AST \"%s\" ", parser_astToString(node->type));

    switch(node->type) {
    case AST_BLOCK:
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            validateAST((struct astNode*)elem->data);
        }
        break;
    case AST_VARDECLARE:
        var = (struct symbolNode*) node->data;
        var->isDeclared = 1;
        validateType(var->type, node->scope, var->filename, var->line);
        break;
    case AST_VARDEFINE: {
        var = (struct symbolNode*) node->data;
        var->isDeclared = 1;
        validateType(var->type, node->scope, var->filename, var->line);
        char *initType = validateExpressionAST(var->code);
        if(!typesMatch(var->type, initType, var->filename, var->line)) {
            error(var->filename, var->line, "Value type mismatch when assigning to variable \"%s\". Expected \"%s\" type, actual type was \"%s\" ", var->name, var->type, initType);
        }
        break;
    }
    case AST_IF: {
        struct astNode* condition = node->children->head.next->data;
        struct astNode* block = node->children->head.next->next->data;
        char *conditionType = validateExpressionAST(condition);
        if(strcmp("boolean", conditionType)) {
            error(condition->filename, condition->line, "If expected boolean type, actual type was \"%s\" ", conditionType);
        }
        validateAST(block);
        break;
    }
    case AST_IFELSE: {
        struct astNode* condition = node->children->head.next->data;
        struct astNode* block = node->children->head.next->next->data;
        char *conditionType = validateExpressionAST(condition);
        if(strcmp("boolean", conditionType)) {
            error(condition->filename, condition->line, "If expected boolean type, actual type was \"%s\" ", conditionType);
        }
        validateAST(block);
        if(node->children->head.next->next != list_end(node->children)) {
            struct astNode* elseBlock = node->children->head.next->next->next->data;
            validateAST(elseBlock);
        }
        break;
    }
    case AST_WHILE: {
        struct astNode* condition = node->children->head.next->data;
        struct astNode* block = node->children->head.next->next->data;
        char *conditionType = validateExpressionAST(condition);
        if(strcmp("boolean", conditionType)) {
            error(condition->filename, condition->line,"While expected boolean type, actual type was \"%s\" ", conditionType);
        }
        validateAST(block);
        break;
    }
    case AST_RETURN: {
        struct astNode* retval = node->children->head.next->data;
        if(retval == NULL) {
            if(strcmp(node->scope->type, "void")) {
                error(retval->filename, retval->line,"Cannot return value from void type function");
            }
        } else {
            char *returnType = validateExpressionAST(retval);
            if(!typesMatch(node->scope->type, returnType, node->filename, node->line)) {
                error(retval->filename, retval->line,"Return values do not match, expected \"%s\" , actual was \"%s\" ", node->scope->type, returnType);
            }
        }
        break;
    }
    default:
        LOG("%p", validateExpressionAST(node));
    }
}

/*
    Recursively goes through expression, checks to make sure that the type of 
    the inputs is correct, returns output type based on input */
char* validateExpressionAST(struct astNode* node) {
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
    case AST_NULL:
        strcpy(retval, "None");
        return retval;
    case AST_VAR: {
        struct symbolNode* var = findSymbol(node->data, node->scope, node->filename, node->line);
        strcpy(retval, var->type);
        return retval;
    }
    // RECURSIVE CASES
    case AST_ADD:
    case AST_SUBTRACT:
    case AST_MULTIPLY:
    case AST_DIVIDE: {
        validateBinaryOp(node->children, left, right);
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
        struct symbolNode* var = NULL; // Needs to have a var to assign to
        if(leftAST->type != AST_VAR && leftAST->type != AST_DOT && leftAST->type != AST_INDEX && leftAST->type != AST_MODULEACCESS) {
            error(node->filename, node->line, "Left side of assignment must be a location");
        }
        // Constant assignment validation- find variable associated with assignment
        if(leftAST->type == AST_VAR) {
            var = findSymbol(leftAST->data, leftAST->scope, node->filename, node->line);
        } else if(leftAST->type == AST_MODULEACCESS) {
            struct astNode* moduleIdent = leftAST->children->head.next->next->data;
            struct astNode* nameIdent = leftAST->children->head.next->data;
            if(nameIdent->type != AST_VAR) {
                error(node->filename, node->line, "Left side of assignment must be a location");
            }
            var = findExplicitSymbol(moduleIdent->data, nameIdent->data, leftAST->scope, leftAST->filename, leftAST->line);
        }
        // Indexing is not checked, you can change the contents of an array if it is constant
        if(var != NULL && var->isConstant) {
            error(node->filename, node->line, "Cannot assign to constant \"%s\" ", var->name);
        }
        // Left/right type matching
        validateBinaryOp(node->children, left, right);
        if(!typesMatch(left, right, node->filename, node->line)) {
            error(node->filename, node->line, "Value type mismatch. Expected \"%s\" type, actual type was \"%s\" ", left, right);
        }
        strcpy(retval, left);
        return retval;
    }
    case AST_OR:
    case AST_AND:
        validateBinaryOp(node->children, left, right);
         if(!strcmp(left, "boolean") && !strcmp(right, "boolean")) {
            strcpy(retval, "boolean");
            return retval;
        } else {
            error(node->filename, node->line, "Value type mismatch. Expected boolean type, actual types were \"%s\" and \"%s\" ", left, right);
        }
    case AST_GREATER:
    case AST_LESSER:
    case AST_GREATEREQUAL:
    case AST_LESSEREQUAL: {
        validateBinaryOp(node->children, left, right);
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
        struct symbolNode* symbol = findSymbol(node->data, node->scope, node->filename, node->line);
        // ARRAY LITERAL
        if(strstr(node->data, " array")){
            char base[255];
            strcpy(base, node->data);
            removeArray(base);
            validateArrayType(node->children, base, node->filename, node->line);
            strcpy(retval, node->data);
            return retval;
        }
        // STRUCT INIT
        else if(symbol->symbolType == SYMBOL_STRUCT) {
            int err = validateParamType(node->children, symbol->children, node->filename, node->line);
            
            if(!err || list_isEmpty(node->children)) {
                strcpy(retval, node->data);
                return retval;
            } else if(err > 0){
                error(node->filename, node->line, "Too many arguments for struct \"%s\" initialization", symbol->name);
            } else if(err < 0){
                error(node->filename, node->line, "Too few arguments for struct \"%s\" initialization", symbol->name);
            }
        } 
        // FUNCTION CALL
        else if(symbol->symbolType == SYMBOL_FUNCTION) {
            if(!node->scope->isStatic) {
                error(node->filename, node->line, "Cannot call a static function from global scope");
            }
            int err = validateParamType(node->children, symbol->children, node->filename, node->line);
            
            if(!err) {
                strcpy(retval, symbol->type);
                return retval;
            } else if(err > 0){
                error(node->filename, node->line, "Too many arguments for function call");
            } else if(err < 0){
                error(node->filename, node->line, "Too few arguments for function call");
            }
        }
    }
    case AST_DOT: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct astNode* rightAST = node->children->head.next->data;

        char* type = validateExpressionAST(leftAST);
        if(strstr(type, " array") && !strcmp(rightAST->data, "length")) {
            strcpy(retval, "int");
        } else {
            strcpy(retval, validateStructField(type, rightAST->data, node->filename, node->line));
        }
        return retval;
    }
    case AST_INDEX: {
        struct astNode* leftAST = node->children->head.next->next->data;
        struct astNode* rightAST = node->children->head.next->data;

        char* rightType = validateExpressionAST(rightAST);
        if(strcmp(rightType, "int")) {
            error(node->filename, node->line, "Value type mismatch when indexing array. Expected int type, actual type was \"%s\" ", rightType);
        }
        // SIZE ARRAY INIT
        if(leftAST->type == AST_VAR && validateType(leftAST->data, node->scope, node->filename, node->line)){
            strcpy(retval, leftAST->data);
            strcat(retval, " array");
            return retval;
        } 
        // ARRAY INDEXING
        else {
            char* leftType = validateExpressionAST(leftAST);
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

        struct symbolNode* symbol = findExplicitSymbol(leftAST->data, rightAST->data, node->scope, node->filename, node->line);
        if(symbol != NULL) {
            if (rightAST->type == AST_CALL) { // Validate that the call is well formed
                validateExpressionAST(rightAST);
            }
            return symbol->type;
        } else if(rightAST->type == AST_CALL) {
            error(node->filename, node->line, "Module \"%s\" does not contain function \"%s\"", leftAST->data, rightAST->data);
        } else if(rightAST->type == AST_VAR) {
            error(node->filename, node->line, "Module \"%s\" does not contain variable \"%s\"", leftAST->data, rightAST->data);
        }
    }
    case AST_CAST: {
        struct astNode* rightAST = node->children->head.next->data;
        char* oldType = validateExpressionAST(rightAST);
        char* newType = node->data;
        if(!strcmp(newType, "None")) {
            error(node->filename, node->line, "Cannot cast %s to None", oldType);
        }
        if(strcmp(oldType, newType)) {
            if(strcmp(oldType, "Any") && strcmp(newType, "Any")) {
                error(node->filename, node->line, "Cannot cast %s to %s", oldType, newType);
            }
        }
        strcpy(retval, node->data);
        return retval;
    }
    case AST_NEW: {
        struct astNode* rightAST = node->children->head.next->data;
        if(rightAST->type != AST_CALL && rightAST->type != AST_INDEX) {
            error(node->filename, node->line, "New operand must be a struct call");
        }
        char* oldType = validateExpressionAST(rightAST);
        strcpy(retval, oldType);
        return retval;
    }
    case AST_FREE: {
        struct astNode* rightAST = node->children->head.next->data;
        if(rightAST->type != AST_VAR && rightAST->type != AST_INDEX) {
            error(node->filename, node->line, "Free operand must be a variable");
        }
        strcpy(retval, "None");
        return retval;
    }
    default:
        PANIC("AST \"%s\" validation is not implemented yet", parser_astToString(node->type));
    }
    return NULL;
}

/*
    Copies the types of both the left expression and right expression to given strings. */
static void validateBinaryOp(struct list* children, char* leftType, char* rightType) {
    strcpy(rightType, validateExpressionAST(children->head.next->data));
    strcpy(leftType, validateExpressionAST(children->head.next->next->data));
}

static struct symbolNode* findExplicitSymbol(char* moduleName, char* memberName, const struct symbolNode* scope, const char* filename, int line) {
    struct symbolNode* module = map_get(program->children, moduleName);
    if(module == NULL) {
        error(filename, line, "Unknown module %s", moduleName);
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

/*
    Returns the symbol with the given name relative to a given starting scope.
    
    Will return NULL if no symbol with the name is found in any direct ancestor
    scopes. */
static struct symbolNode* findSymbol(const char* symbolName, const struct symbolNode* scope, const char* filename, int line) {
    if(scope == NULL) {
        error(filename, line, "Unknown variable \"%s\"", symbolName);
    } else {
        struct symbolNode* symbol = map_get(scope->children, symbolName);
        if(symbol != NULL) {
            return symbol;
        } else {
            return findSymbol(symbolName, scope->parent, filename, line);
        }
    }
    return NULL;
}

/*
    Takes in a list of AST's representing expressions for arguments, checks each of their types against a map of given
    parameters, represented as symbolNodes.
    
    Returns: 1 when too many args, 
            -1 when too few args,
             0 when okay args. */
static int validateParamType(struct list* args, struct map* paramMap, const char* filename, int line) {
    struct list* params = map_getKeyList(paramMap);
    struct listElem* paramElem;
    struct listElem* argElem;
    for(paramElem = list_begin(params), argElem = list_begin(args); 
        paramElem != list_end(params) && argElem != list_end(args); 
        paramElem = list_next(paramElem), argElem = list_next(argElem)) {
        if(!strcmp(paramElem->data, "_block")) { // functions store blocks along with parameters, this excludes checking that
            continue;
        }
        char* paramType = ((struct symbolNode*)map_get(paramMap, ((char*)paramElem->data)))->type;
        char* argType = validateExpressionAST((struct astNode*)argElem->data);
        if(!typesMatch(paramType, argType, filename, line)) {
            error(filename, line, "Value type mismatch when passing argument. Expected \"%s\" type, actual type was \"%s\" ", paramType, argType);
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
    Validates that all types in an array literal are the expected type */
static int validateArrayType(struct list* args, char* expected, const char* filename, int line) {
    struct listElem* argElem;
    for(argElem = list_begin(args); argElem != list_end(args); argElem = list_next(argElem)) {
        char* argType = validateExpressionAST((struct astNode*)argElem->data);
        if(typesMatch(expected, argType, filename, line)) {
            error(filename, line, "Value type mismatch when creating array. Expected \"%s\" type, actual type was \"%s\" ", argType, expected);
        }
    }
    return 1;
}

/*
    Checks to see if a struct contains a field, or if a super struct contains the field. */
static char* validateStructField(char* structName, char* fieldName, const char* filename, int line) {
    struct symbolNode* dataStruct = map_get(program->children, structName);
    if(dataStruct == NULL) {
        error(filename, line, "Unknown struct \"%s\" ", structName);
    }
    if(map_get(dataStruct->children, fieldName) == NULL) {
        error(filename, line, "Unknown field \"%s\" for struct \"%s\" ", fieldName, structName);
    }

    return ((struct symbolNode*)map_get(dataStruct->children, fieldName))->type;
}