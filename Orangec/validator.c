/*  validator.c

    The validator's job is to take in a program data structure, look through 
    it's components, and make sure (validate) that the program is correct.

    Author: Joseph Shimel
    Date: 2/19/21
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./ast.h"
#include "./main.h"
#include "./validator.h"

#include "../util/map.h"
#include "../util/list.h"
#include "../util/debug.h"

static void updateType(struct symbolNode*);
static void updateStruct(struct symbolNode*);
static void validateAST(struct astNode*);
static char* validateExpressionAST(struct astNode*);
static void validateBinaryOp(struct list*, char*, char*);
static int findTypeEnd(const char*);
static bool isPrimitive(const char*);
static void removeArray(char*);
static bool typesMatch(const char*, const char*, const struct symbolNode*, const char*, int);
static bool validateType(const char*, const struct symbolNode*);
static int validateFunctionTypesMatch(struct map*, struct map*, struct symbolNode*, const char*, int);
static bool validateArrayType(struct list*, char*, struct symbolNode*, const char*, int);
static int validateParamType(struct list*, struct map*, struct symbolNode*, const char*, int);
static char* validateStructField(char*, char*, const char*, int);

/*
    Goes through the symbol tree, updates the types of symbols from the 
    written plain type to a "true" type, that is relates specifically to a type.
    
    Consider the following code:
        Mod1 {
            struct Example(int i)
        }
        Mod2 {
            struct Example{int i}
            void ex() {
                Mod1:Example example;
            }
        }

    To the parser, the variable example will have the type Example, as 
    specified by the programmer. However, there are multiple types with that
    name, and the validator cannot be sure which is which.
    
    To resolve the issue, orangec assigns every symbol a UID, and types
    are resolved using their type name, concatenated with the $ symbol, and 
    their UID represented in base-36. */
void validator_updateStructType(struct symbolNode* symbolNode) {
    ASSERT(symbolNode != NULL);
    struct list* children = symbolNode->children->keyList;
    struct listElem* elem = list_begin(children);
    // Correct struct type, if exists
    switch(symbolNode->symbolType) {
    case SYMBOL_VARIABLE:
    case SYMBOL_FUNCTIONPTR:
    case SYMBOL_FUNCTION:
    case SYMBOL_BLOCK:
        if(strstr(symbolNode->type, "$")) {
            updateType(symbolNode);
        } else {
            updateStruct(symbolNode);
        } // rollover to default ->
    default: {
        for(;elem != list_end(children); elem = list_next(elem)) {
            struct symbolNode* child = (struct symbolNode*)map_get(symbolNode->children, (char*)elem->data);
            ASSERT(child != NULL);
            validator_updateStructType(child);
        }
    } break;
    }
}

/*
    Takes in a symbol, and evaluates it according to the symbol type. 
    
    - All symbols must have valid children.
    - Programs cannot have children that are not modules.
    - Modules cannot have children that are blocks.
    - Variable delcarations must have a valid type, and a valid AST, if they have 
      one. 
      
    If all of these conditions are met, the symbol is valid. */
void validator_validate(struct symbolNode* symbolNode) {
    ASSERT(symbolNode != NULL);
    struct list* children = symbolNode->children->keyList;
    struct listElem* elem = list_begin(children);
    LOG("Validating %s", symbolNode->name);
    // Correct struct type, if exists
    switch(symbolNode->symbolType) {
    case SYMBOL_PROGRAM: {
        for(;elem != list_end(children); elem = list_next(elem)) {
            struct symbolNode* child = (struct symbolNode*)map_get(symbolNode->children, (char*)elem->data);
            ASSERT(child != NULL);
            if(child->symbolType == SYMBOL_MODULE) {
                validator_validate(child);
            } else {
                error(child->filename, child->line, "Must be defined inside a module\n");
            }
        }
    } break;
    case SYMBOL_MODULE: {
        for(;elem != list_end(children); elem = list_next(elem)) {
            struct symbolNode* child = (struct symbolNode*)map_get(symbolNode->children, (char*)elem->data);
            ASSERT(child != NULL);
            if(child->symbolType != SYMBOL_BLOCK) {
                validator_validate(child);
            } else {
                error(child->filename, child->line, "Module members must be structs, variables, or functions\n");
            }
        }
    } break;
    case SYMBOL_FUNCTIONPTR:
    case SYMBOL_VARIABLE: {
        // Valid type
        if(!validateType(symbolNode->type, symbolNode->parent)) {
            error(symbolNode->filename, symbolNode->line, "Unknown type %s", symbolNode->type);
        }
        // Valid AST
        if(symbolNode->code != NULL) {
            char* actual = validateExpressionAST(symbolNode->code);
            if(!typesMatch(symbolNode->type, actual, symbolNode->parent, symbolNode->filename, symbolNode->line)) {
                error(symbolNode->filename, symbolNode->line, "Value type mismatch. Expected \"%s\" type, actual type was \"%s\" ", symbolNode->type, actual);
            }
            symbolNode->isDefined = 1;
        }
        // All children must be valid
        for(;elem != list_end(children); elem = list_next(elem)) {
            validator_validate((struct symbolNode*)map_get(symbolNode->children, (char*)elem->data));
        }
    } break;
    case SYMBOL_FUNCTION: {
        // Valid type
        if(!validateType(symbolNode->type, symbolNode->parent)) {
            error(symbolNode->filename, symbolNode->line, "Unknown type %s", symbolNode->type);
        }
        // All children must be valid- done before AST validation so as to allow SYMBOL_BLOCK's to update their struct type
        for(;elem != list_end(children); elem = list_next(elem)) {
            validator_validate((struct symbolNode*)map_get(symbolNode->children, (char*)elem->data));
        }
        // Valid AST
        validateAST(symbolNode->code);
    } break;
    case SYMBOL_ENUM:
    case SYMBOL_STRUCT:
    case SYMBOL_BLOCK: {
        // All children must be valid
        for(;elem != list_end(children); elem = list_next(elem)) {
            validator_validate((struct symbolNode*)map_get(symbolNode->children, (char*)elem->data));
        }
    } break;
    default:
        error("", 1, "Bad symbol\n");
    }
}

/*
    This function is called before validation when a function or variable has 
    a $ character as part of its type. The $ symbol indicate that the type is
    defined in another module, either an enum or a struct.
    
    Takes in a symbol, whose type is in the form module$type, and converts it 
    so that the type is in the form type#uid. This prevents type name clashing. */
static void updateType(struct symbolNode* symbolNode) {
    char mod[255];
    memset(mod, 0, 254);
    char member[255];
    memset(member, 0, 254);
    int end = findTypeEnd(symbolNode->type);
    strncpy(mod, symbolNode->type, end);
    strcpy(member, symbolNode->type + end + 1);
    struct symbolNode* symbol = symbol_findExplicit(mod, member, symbolNode->parent, symbolNode->filename, symbolNode->line);
    if(symbol->symbolType == SYMBOL_STRUCT || symbol->symbolType == SYMBOL_ENUM) {
        strcpy(symbolNode->type, symbol->type);
    } // findExplicit throws its own errors
}

/*
    This function is called before validation on a symbol node that does not 
    have a $ character in its type. This indicates that the type is local to 
    the module the symbol is in.
    
    However, to prevent type clashes when combining modules, types are given a
    unique ID that they are referenced by, meaning that the plaintext type is
    not valid. 
    
    This function converts the plaintext type to the full type, in the form
    type#UID */
static void updateStruct(struct symbolNode* symbolNode) {
    struct symbolNode* symbol = symbol_find(symbolNode->type, symbolNode->parent);
    if(symbol != NULL && (symbol->symbolType == SYMBOL_STRUCT || symbol->symbolType == SYMBOL_ENUM)) {
        LOG("%s", symbol->type);
        strcpy(symbolNode->type, symbol->type);
    } // type may not be valid here, but that will be checked later
}

/*
    This function takes in an AST node, and checks it's data, like it's type, 
    children's type, children's validity. */
static void validateAST(struct astNode* node) {
    struct listElem* elem;
    LOG("Validating AST \"%s\" ", ast_toString(node->type));

    switch(node->type) {
    case AST_BLOCK:
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            validateAST((struct astNode*)elem->data);
        }
        break;
    case AST_SYMBOLDEFINE: {
        struct symbolNode* var = (struct symbolNode*) node->data;
        LOG("%p", node->scope);
        var->isDeclared = 1;
        validator_validate(var);
    } break;
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
        struct astNode* elseBlock = node->children->head.next->next->next->data;
        validateAST(elseBlock);
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
            if(!typesMatch(node->scope->type, returnType, node->scope, node->filename, node->line)) {
                error(retval->filename, retval->line,"Return values do not match, expected \"%s\" , actual was \"%s\" ", node->scope->type, returnType);
            }
        }
        break;
    }
    default:
        validateExpressionAST(node);
    }
}

/*
    Recursively goes through expression, checks to make sure that the type of 
    the inputs is correct, returns output type based on input */
char* validateExpressionAST(struct astNode* node) {
    char left[255], right[255];
    char* retval = (char*)malloc(sizeof(char) * 255);

    LOG("Validating expression \"%s\" ", ast_toString(node->type));
    switch(node->type){
    // BASE CASES
    case AST_VAR: {
        struct symbolNode* var = symbol_find(node->data, node->scope);
        if(var == NULL) {
            error(node->filename, node->line, "Unknown symbol %s", node->data);
        } else if(!var->isDeclared) {
            error(node->filename, node->line, "Symbol %s is undeclared", node->data);
        } else {
            strcpy(retval, var->type);
            return retval;
        }
    }
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
    // RECURSIVE CASES
    case AST_CALL: {
        // ARRAY LITERAL
        if(strstr(node->data, " array")){
            LOG("Array literal call");
            if(node->parent == NULL || node->parent->type != AST_NEW) {
                error(node->filename, node->line, "Arrays must be allocated with \"new\" operator");
            }
            char base[255];
            strcpy(base, node->data);
            removeArray(base);
            validateArrayType(node->children, base, node->scope, node->filename, node->line);
            strcpy(retval, node->data);
            return retval;
        }
        
        struct symbolNode* symbol = symbol_find(node->data, node->scope);
        if(symbol == NULL) {
            error(node->filename, node->line, "Unknown symbol %s", node->data);
        // STRUCT INIT
        } else if(symbol->symbolType == SYMBOL_STRUCT) {
            LOG("Struct init call");
            int err = validateParamType(node->children, symbol->children, node->scope, node->filename, node->line);
            if(node->parent == NULL || node->parent->type != AST_NEW) {
                error(node->filename, node->line, "Structs must be allocated with \"new\" operator");
            }
            
            if(!err || list_isEmpty(node->children)) {
                strcpy(retval, symbol->type);
                return retval;
            } else if(err > 0){
                error(node->filename, node->line, "Too many arguments for struct \"%s\" initialization", symbol->name);
            } else if(err < 0){
                error(node->filename, node->line, "Too few arguments for struct \"%s\" initialization", symbol->name);
            }
        } 
        // FUNCTION CALL
        else if(symbol->symbolType == SYMBOL_FUNCTION || symbol->symbolType == SYMBOL_FUNCTIONPTR) {
            LOG("Function call");
            if(!node->scope->isStatic && symbol->isStatic) {
                error(node->filename, node->line, "Cannot call a static function from global scope");
            }
            int err = validateParamType(node->children, symbol->children, node->scope, node->filename, node->line);
            
            if(err == -1) {
                strcpy(retval, symbol->type);
                return retval;
            } else if(err > -1){
                error(node->filename, node->line, "Too many arguments for function call");
            } else if(err < -1){
                error(node->filename, node->line, "Too few arguments for function call");
            }
        }
    }
    case AST_VERBATIM: {
        struct listElem* elem;
        for(elem = list_begin(node->children); elem != list_end(node->children); elem = list_next(elem)) {
            validateAST((struct astNode*)elem->data);
        }
        strcpy(retval, "Any");
        return retval;
    }
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
            var = symbol_find(leftAST->data, leftAST->scope);
            if(var == NULL) {
                error(node->filename, node->line, "Unknown symbol %s", node->data);
            }
        } else if(leftAST->type == AST_MODULEACCESS) {
            struct astNode* moduleIdent = leftAST->children->head.next->next->data;
            struct astNode* nameIdent = leftAST->children->head.next->data;
            if(nameIdent->type != AST_VAR) {
                error(node->filename, node->line, "Left side of assignment must be a location");
            }
            var = symbol_findExplicit(moduleIdent->data, nameIdent->data, leftAST->scope, leftAST->filename, leftAST->line);
        } // else {leftAST is index}

        // Indexing is not checked, you can change the contents of an array if it is constant
        if(var != NULL && var->isConstant) {
            error(node->filename, node->line, "Cannot assign to constant \"%s\" ", var->name);
        }
        // Left/right type matching
        validateBinaryOp(node->children, left, right);
        if(var != NULL) {
            var->isDefined = 1;
        }
        LOG("%s == %s", left, right);
        if(!typesMatch(left, right, node->scope, node->filename, node->line)) {
            error(node->filename, node->line, "Value type mismatch. Expected \"%s\" type, actual type was \"%s\" ", left, right);
        }
        strcpy(retval, left);
        return retval;
    }
    case AST_IS:
    case AST_ISNT:
        validateBinaryOp(node->children, left, right);
        strcpy(retval, "boolean");
        return retval;
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
    case AST_AND:
    case AST_OR:
        validateBinaryOp(node->children, left, right);
         if(!strcmp(left, "boolean") && !strcmp(right, "boolean")) {
            strcpy(retval, "boolean");
            return retval;
        } else {
            error(node->filename, node->line, "Value type mismatch. Expected boolean type, actual types were \"%s\" and \"%s\" ", left, right);
        }
    case AST_CAST: {
        struct astNode* rightAST = node->children->head.next->data;
        char* oldType = validateExpressionAST(rightAST);
        char* newType = node->data;
        if(!strcmp(newType, "None")) {
            error(node->filename, node->line, "Cannot cast %s to None", oldType);
        }
        if(strcmp(oldType, newType)) {
            struct symbolNode* symbol = map_get(typeMap, oldType);
            if((symbol == NULL && !strcmp(newType, "int")) && // Converting enum to int
                (strcmp(oldType, "real") && strcmp(newType, "int")) && // Converting real to int
                (strcmp(oldType, "Any") && strcmp(newType, "Any"))) { // Converting anything to/from Any is legal
                error(node->filename, node->line, "Cannot cast %s to %s", oldType, newType);
            }
        }
        strcpy(retval, node->data);
        return retval;
    }
    case AST_NEW: {
        struct astNode* rightAST = node->children->head.next->data;
        if(rightAST->type != AST_CALL && rightAST->type != AST_INDEX && rightAST->type != AST_MODULEACCESS) {
            error(node->filename, node->line, "New operand must be a struct call");
        }
        char* oldType = validateExpressionAST(rightAST);
        strcpy(retval, oldType);
        return retval;
    }
    case AST_FREE: {
        strcpy(retval, "None");
        return retval;
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
        if(leftAST->type == AST_VAR && validateType(leftAST->data, node->scope)){
            if(node->parent == NULL || node->parent->type != AST_NEW) {
                error(node->filename, node->line, "Arrays must be allocated with \"new\" operator");
            }
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

        struct symbolNode* symbol = symbol_findExplicit(leftAST->data, rightAST->data, node->scope, node->filename, node->line); // findExplicit throws errors itself, no need to check for NULL
        rightAST->scope = map_get(program->children, leftAST->data);
        if (rightAST->type == AST_CALL) { // Validate that the call is well formed
            validateExpressionAST(rightAST);
        }
        return symbol->type;
    }
    default:
        PANIC("AST \"%s\" validation is not implemented yet", ast_toString(node->type));
    }
    return NULL;
}

/*
    Copies the types of both the left expression and right expression to given strings. */
static void validateBinaryOp(struct list* children, char* leftType, char* rightType) {
    strcpy(rightType, validateExpressionAST(children->head.next->data));
    strcpy(leftType, validateExpressionAST(children->head.next->next->data));
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
    while (type[i] != '\0' && type[i] != ' ' && type[i] != '$') i++;
    return i;
}

/*
    Checks to see if a type is a primitive, built in type*/
static bool isPrimitive(const char* type) {
    if(!strcmp(type,  "int") || !strcmp(type, "char") ||
        !strcmp(type, "boolean") || !strcmp(type, "void") || 
        !strcmp(type, "real") || !strcmp(type, "byte") || 
        !strcmp(type, "struct")) {
        return true;
    } else {
        return false;
    }
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
    Determines if two types are the same. For structs, the expected type may 
    be a parent struct of the actual struct 
    
    USE THIS FUNCTION for comapring two unknown types. Use strcmp ONLY if the 
    expected type is fixed and known. */
static bool typesMatch(const char* expected, const char* actual, const struct symbolNode* scope, const char* filename, int line) {
    if(isPrimitive(expected) || isPrimitive(actual)) {
        return !strcmp(expected, actual);
    } else if (!isPrimitive(expected) && !strcmp(actual, "None")) {
        return true;
    } else if (!strcmp(expected, "Any") && !isPrimitive(actual)) {
        return true;
    } else if(strstr(expected, " array")){
        char expectedBase[255];
        char actualBase[255];
        if(strcmp(expected, actual)){
            return false;
        }
        strcpy(expectedBase, expected);
        strcpy(actualBase, actual);
        removeArray(expectedBase);
        removeArray(actualBase);
        return typesMatch(expectedBase, actualBase, scope, filename, line);
    } else {
        // here type must be struct
        struct symbolNode* dataStruct = map_get(typeMap, actual);
        if(dataStruct == NULL) {
            dataStruct = symbol_find(actual, scope);
        }
        if(dataStruct == NULL || (dataStruct->symbolType != SYMBOL_STRUCT && dataStruct->symbolType != SYMBOL_ENUM)) {
            error(filename, line, "Unknown struct \"%s\" ", actual);
        } else {
            return !strcmp(expected, dataStruct->type);
        }
    }
    return true;
}

/*
    Checks to see if a given type, for a given scope, is valid */
static bool validateType(const char* type, const struct symbolNode* scope) {
    // Check primitives
    int end = findTypeEnd(type);
    char temp[255];
    memset(temp, 0, 254);
    strncpy(temp, type, end);
    // If the type isn't private, check to see if there is a struct defined with the name
    if(!isPrimitive(temp) && strcmp(temp, "Any")) {
        struct symbolNode* dataStruct = map_get(typeMap, temp);
        if(dataStruct == NULL || (dataStruct->symbolType != SYMBOL_STRUCT && dataStruct->symbolType != SYMBOL_ENUM)) {
            return false;
        }
    }
    return true;
}

/*
    Validates that the parameter map of one function matches that of another 
    function. Used to check if a function pointer fits a function */
static int validateFunctionTypesMatch(struct map* paramMap1, struct map* paramMap2, struct symbolNode* scope, const char* filename, int line) {
    struct listElem* paramElem1;
    struct listElem* paramElem2;
    int i = 1;
    for(paramElem1 = list_begin(paramMap1->keyList), paramElem2 = list_begin(paramMap2->keyList); 
        paramElem1 != list_end(paramMap1->keyList) && paramElem2 != list_end(paramMap2->keyList); 
        paramElem1 = list_next(paramElem1), paramElem2 = list_next(paramElem2), i++) {
        if(strstr(paramElem1->data, "_block")) {
            paramElem1 = list_next(paramElem1);
            if(paramElem1 == list_end(paramMap1->keyList)) {
                break;
            }
        }
        if(strstr(paramElem2->data, "_block")) {
            paramElem2 = list_next(paramElem2);
            if(paramElem2 == list_end(paramMap2->keyList)) {
                break;
            }
        }
        char* type1 = ((struct symbolNode*)map_get(paramMap1, (char*)paramElem1->data))->type;
        char* type2 = ((struct symbolNode*)map_get(paramMap2, (char*)paramElem2->data))->type;
        
        if(!typesMatch(type1, type2, scope, filename, line)) {
            error(filename, line, "Type mismatch between function parameters #%d, Expected \"%s\", actual type was \"%s\" ", i, type1, type2);
        }
    }
    return paramMap1->size - paramMap2->size;
}

/*
    Validates that all types in an array literal are the expected type */
static bool validateArrayType(struct list* args, char* expected, struct symbolNode* scope, const char* filename, int line) {
    struct listElem* argElem;
    for(argElem = list_begin(args); argElem != list_end(args); argElem = list_next(argElem)) {
        char* argType = validateExpressionAST((struct astNode*)argElem->data);
        if(!typesMatch(expected, argType, scope, filename, line)) {
            error(filename, line, "Value type mismatch when creating array. Expected \"%s\" type, actual type was \"%s\" ", argType, expected);
        }
    }
    return true;
}

/*
    Takes in a list of AST's representing expressions for arguments, checks each of their types against a map of given
    parameters, represented as symbolNodes.
    
    Returns: 1 when too many args, 
            -1 when too few args,
             0 when okay args. */
static int validateParamType(struct list* args, struct map* paramMap, struct symbolNode* scope, const char* filename, int line) {
    struct list* params = map_getKeyList(paramMap);
    struct listElem* paramElem;
    struct listElem* argElem;
    for(paramElem = list_begin(params), argElem = list_begin(args); 
        paramElem != list_end(params) && argElem != list_end(args); 
        paramElem = list_next(paramElem), argElem = list_next(argElem)) {
        struct symbolNode* symbol = (struct symbolNode*)map_get(paramMap, ((char*)paramElem->data));
        if(symbol->symbolType == SYMBOL_BLOCK) { // functions store blocks along with parameters, this excludes checking that
            continue;
        }
        char* paramType = symbol->type;
        char* argType = validateExpressionAST((struct astNode*)argElem->data);
        if(!typesMatch(paramType, argType, scope, filename, line)) {
            error(filename, line, "Value type mismatch when passing argument. Expected \"%s\" type, actual type was \"%s\" ", paramType, argType);
        }
        if(symbol->symbolType == SYMBOL_FUNCTIONPTR) {
            struct astNode* node = (struct astNode*)argElem->data;
            struct symbolNode* fnptr = symbol_find(node->data, node->scope);
            if(fnptr == NULL) {
                LOG("Here");
            }
            int err = validateFunctionTypesMatch(fnptr->children, symbol->children, scope, filename, line);
            if(err > 0){
                error(filename, line, "Passed pointer to function with more arguments than expected");
            } else if(err < 0){
                error(filename, line, "Passed pointer to function with fewer arguments than expected");
            }
        }
    }
    // count the parameters, arguments. Check that sizes match excluding function's block
    return args->size - paramMap->size;
}

/*
    Checks to see if a struct contains a field, or if a super struct contains the field. */
static char* validateStructField(char* structName, char* fieldName, const char* filename, int line) {
    struct symbolNode* dataStruct = map_get(typeMap, structName);
    if(dataStruct == NULL) {
        error(filename, line, "Unknown struct \"%s\" ", structName);
    }
    if(map_get(dataStruct->children, fieldName) == NULL) {
        error(filename, line, "Unknown field \"%s\" for struct \"%s\" ", fieldName, structName);
    }

    return ((struct symbolNode*) map_get(dataStruct->children, fieldName))->type;
}