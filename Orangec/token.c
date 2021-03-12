/*  token.c

    A token is used to generalize incoming text given to the compiler. Tokens 
    represent small units of meaning, and can be strung along to create 
    programs.

    Author: Joseph Shimel
    Date: 3/12/21
*/

#include <stdlib.h>
#include <string.h>

#include "./token.h"

#include "../util/debug.h"

/*
    Creates a token with a given type and data */
struct token* token_create(enum tokenType type, char data[], const char* filename, int line) {
    struct token* retval = (struct token*) malloc(sizeof(struct token));
    retval->type = type;
    retval->list = list_create();
    retval->filename = filename;
    retval->line = line;
    strncpy(retval->data, data, 254);
    return retval;
}

/* Returns the precedence a token operator has */
int token_precedence(enum tokenType type) {
    switch(type) {
		case TOKEN_EQUALS:
			return 1;
		case TOKEN_OR:
			return 2;
		case TOKEN_AND:
			return 3;
		case TOKEN_ISNT:
		case TOKEN_IS:
			return 4;
		case TOKEN_GREATER:
		case TOKEN_LESSER:
		case TOKEN_GREATEREQUAL:
		case TOKEN_LESSEREQUAL:
			return 5;
		case TOKEN_PLUS:
		case TOKEN_MINUS:
			return 6;
		case TOKEN_STAR:
		case TOKEN_SLASH:
			return 7;
        case TOKEN_NEW:
        case TOKEN_FREE:
            return 8;
        case TOKEN_CAST:
            return 9;
        case TOKEN_DOT:
        case TOKEN_COLON:
        case TOKEN_INDEX:
            return 10;
		default:
			return 0;
    }
}

/*
    Returns a string representation of a token type */
char* token_toString(enum tokenType type) {
    switch(type) {
    case TOKEN_LPAREN:
        return "token:LPAREN";
    case TOKEN_RPAREN:
        return "token:RPAREN";
    case TOKEN_LSQUARE:
        return "token:LSQUARE";
    case TOKEN_RSQUARE:
        return "token:RSQUARE";
    case TOKEN_LBRACE:
        return "token:LBRACE";
    case TOKEN_RBRACE:
        return "token:RBRACE";
    case TOKEN_COMMA:
        return "token:COMMA";
    case TOKEN_DOT:
        return "token:DOT";
    case TOKEN_SEMICOLON:
        return "token:SEMICOLON";
    case TOKEN_EOF:
        return "token:EOF";
    case TOKEN_IDENTIFIER:
        return "token:IDENTFIER";
    case TOKEN_INTLITERAL:
        return "token:INTLITERAL";
    case TOKEN_REALLITERAL:
        return "token:REALLITERAL";
    case TOKEN_CHARLITERAL:
        return "token:CHARLITERAL";
    case TOKEN_STRINGLITERAL:
        return "token:STRINGLITERAL";
    case TOKEN_TRUE:
        return "token:TRUE";
    case TOKEN_FALSE:
        return "token:FALSE";
    case TOKEN_NULL:
        return "token:NULL";
    case TOKEN_PLUS:
        return "token:PLUS";
    case TOKEN_MINUS: 
        return "token:MINUS";
    case TOKEN_STAR: 
        return "token:STAR";
    case TOKEN_SLASH: 
        return "token:SLASH";
    case TOKEN_EQUALS:
        return "token:EQUALS";
	case TOKEN_IS: 
        return "token:IS";
    case TOKEN_ISNT: 
        return "token:ISNT";
    case TOKEN_GREATER: 
        return "token:GREATER";
    case TOKEN_LESSER:
        return "token:LESSER";
    case TOKEN_GREATEREQUAL: 
        return "token:GREATEREQUAL";
    case TOKEN_LESSEREQUAL:
        return "token:LESSEREQUAL";
	case TOKEN_AND: 
        return "token:AND";
    case TOKEN_OR:
        return "token:OR";
    case TOKEN_CAST:
        return "token:CAST";
    case TOKEN_NEW:
        return "token:NEW";
    case TOKEN_FREE:
        return "token:FREE";
    case TOKEN_VERBATIM:
        return "token:VERBATIM";
    case TOKEN_MODULE:
        return "token:MODULE";
    case TOKEN_STRUCT:
        return "token:STRUCT";
    case TOKEN_ENUM:
        return "token:ENUM";
    case TOKEN_ARRAY:
        return "token:ARRAY";
    case TOKEN_STATIC:
        return "token:STATIC";
    case TOKEN_CONST:
        return "token:CONST";
    case TOKEN_PRIVATE:
        return "token:PRIVATE";
    case TOKEN_IF:
        return "token:IF";
    case TOKEN_ELSE:
        return "token:ELSE";
    case TOKEN_WHILE:
        return "token:WHILE";
    case TOKEN_RETURN:
        return "token:RETURN";
    case TOKEN_CALL:
        return "token:CALL";
    case TOKEN_TILDE:
        return "token:TILDE";
    case TOKEN_COLON:
        return "token:COLON";
    case TOKEN_INDEX:
        return "token:INDEX";
    case TOKEN_LBLOCK:
        return "token:LBLOCK";
    case TOKEN_RBLOCK:
        return "token:RBLOCK";
    case TOKEN_DSLASH:
        return "token:DSLASH";
    }
    ASSERT(0); // Unreachable
    return "";
}