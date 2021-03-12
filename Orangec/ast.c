/*  ast.c

    An Abstract Syntax Tree is a way to represent the syntax and higher level
    relationships in code.

    Author: Joseph Shimel
    Date: 3/12/21
*/

#include <stdio.h>
#include <stdlib.h>

#include "./ast.h"
#include "./lexer.h"

#include "../util/debug.h"

/*
    Allocates and initializes an Abstract Syntax Tree node, with the proper type */
struct astNode* ast_create(enum astType type, const char* filename, int line, struct symbolNode* scope) {
    struct astNode* retval = (struct astNode*) calloc(1, sizeof(struct astNode));
    retval->type = type;
    retval->children = list_create();
    retval->filename = filename;
    retval->line = line;
    retval->scope = scope;
    return retval;
}

/*
    Converts an AST type to a string */
char* ast_toString(enum astType type) {
    switch(type) {
    case AST_BLOCK: 
        return "astType:BLOCK";
    case AST_SYMBOLDEFINE: 
        return "astType.SYMBOLDEFINE";
    case AST_IF: 
        return "astType.IF";
    case AST_IFELSE: 
        return "astType.IFELSE";
    case AST_WHILE: 
        return "astType.WHILE";
    case AST_RETURN: 
        return "astType.RETURN";
    case AST_ADD: 
        return "astType:PLUS";
    case AST_SUBTRACT: 
        return "astType:MINUS";
    case AST_MULTIPLY: 
        return "astType:MULTIPLY";
    case AST_DIVIDE: 
        return "astType:DIVIDE";
    case AST_AND: 
        return "astType.AND";
    case AST_OR: 
        return "astType.OR";
    case AST_CAST: 
        return "astType.CAST";
    case AST_NEW: 
        return "astType.NEW";
    case AST_FREE: 
        return "astType.FREE";
    case AST_GREATER: 
        return "astType.GREATER";
    case AST_LESSER: 
        return "astType.LESSER";
    case AST_GREATEREQUAL: 
        return "astType.GREATEREQUAL";
    case AST_LESSEREQUAL: 
        return "astType.LESSEREQUAL";
    case AST_IS:
        return "astType.IS";
    case AST_ISNT: 
        return "astType.ISNT";
    case AST_ASSIGN: 
        return "astType:ASSIGN";
    case AST_INDEX: 
        return "astType.INDEX";
    case AST_INTLITERAL: 
        return "astType:INTLITERAL";
    case AST_REALLITERAL: 
        return "astType:REALLITERAL";
    case AST_ARRAYLITERAL: 
        return "astType:ARRAYLITERAL";
    case AST_TRUE: 
        return "astType:TRUE";
    case AST_FALSE: 
        return "astType:FALSE";
    case AST_NULL: 
        return "astType:NULL";
    case AST_CALL:
        return "astType.CALL";
    case AST_VERBATIM:
        return "astType.VERBATIM";
    case AST_VAR: 
        return "astType.VAR";
    case AST_STRINGLITERAL: 
        return "astType.STRINGLITERAL";
    case AST_CHARLITERAL: 
        return "astType.CHARLITERAL";
    case AST_DOT: 
        return "astType.DOT";
    case AST_MODULEACCESS: 
        return "astType.MODULEACCESS";
    case AST_NOP:
        return "astType:NOP";
    }
    printf("Unknown astType: %d\n", type);
    NOT_REACHED();
    return "";
}

/*
    Used to convert between token types for operators, and ast types for 
    operators. Must have a one-to-one mapping. TOKEN_LPAREN for example, 
    does not have a one-to-one mapping with any AST type.
    
    Called by createExpression to convert operator tokens into ASTs */
enum astType ast_tokenToAST(enum tokenType type) {
    switch(type) {
    case TOKEN_PLUS:
        return AST_ADD;
    case TOKEN_MINUS: 
        return AST_SUBTRACT;
    case TOKEN_STAR: 
        return AST_MULTIPLY;
    case TOKEN_SLASH: 
        return AST_DIVIDE;
    case TOKEN_EQUALS:
        return AST_ASSIGN;
	case TOKEN_IS: 
        return AST_IS;
    case TOKEN_ISNT: 
        return AST_ISNT;
    case TOKEN_GREATER: 
        return AST_GREATER;
    case TOKEN_LESSER:
        return AST_LESSER;
    case TOKEN_GREATEREQUAL: 
        return AST_GREATEREQUAL;
    case TOKEN_LESSEREQUAL:
        return AST_LESSEREQUAL;
	case TOKEN_AND: 
        return AST_AND;
    case TOKEN_OR:
        return AST_OR;
    case TOKEN_CAST:
        return AST_CAST;
    case TOKEN_NEW:
        return AST_NEW;
    case TOKEN_FREE:
        return AST_FREE;
    case TOKEN_IDENTIFIER:
        return AST_VAR;
    case TOKEN_CALL:
        return AST_CALL;
    case TOKEN_VERBATIM:
        return AST_VERBATIM;
    case TOKEN_DOT:
        return AST_DOT;
    case TOKEN_INDEX:
        return AST_INDEX;
    case TOKEN_COLON:
        return AST_MODULEACCESS;
    case TOKEN_INTLITERAL:
        return AST_INTLITERAL;
    case TOKEN_REALLITERAL:
        return AST_REALLITERAL;
    case TOKEN_CHARLITERAL:
        return AST_CHARLITERAL;
    case TOKEN_STRINGLITERAL:
        return AST_STRINGLITERAL;
    case TOKEN_TRUE:
        return AST_FALSE;
    case TOKEN_FALSE:
        return AST_FALSE;
    case TOKEN_NULL:
        return AST_NULL;
    default:
        printf("Cannot directly convert %s to AST\n", token_toString(type));
        NOT_REACHED();
    }
}