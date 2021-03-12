/*
    Author: Joseph Shimel
    Date: 3/12/21
*/

#ifndef TOKEN_H
#define TOKEN_H

#include "../util/list.h"

/*
	Tokens have types that distinguish them from other tokens easily */
enum tokenType {
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LSQUARE, TOKEN_RSQUARE, TOKEN_LBRACE, TOKEN_RBRACE,
	// Punctuation
	TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON, TOKEN_TILDE, TOKEN_COLON,
	// Literals
	TOKEN_IDENTIFIER, TOKEN_INTLITERAL, TOKEN_REALLITERAL, TOKEN_CHARLITERAL, 
	TOKEN_STRINGLITERAL, TOKEN_TRUE, TOKEN_FALSE, TOKEN_NULL, TOKEN_VERBATIM,
	// Math operators
	TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_EQUALS,
	// Branch operators
	TOKEN_IS, TOKEN_ISNT, TOKEN_GREATER, TOKEN_LESSER, TOKEN_GREATEREQUAL, TOKEN_LESSEREQUAL,
	// Boolean operators
	TOKEN_AND, TOKEN_OR,
	// Type operators
	TOKEN_CAST, TOKEN_NEW, TOKEN_FREE,
	// Programatic structures
	TOKEN_MODULE, TOKEN_STRUCT, TOKEN_ENUM,
	// Modifiers
	TOKEN_ARRAY, TOKEN_STATIC, TOKEN_CONST, TOKEN_PRIVATE,
	// Control flow structures
	TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_RETURN,
	// Anonymous tokens
	TOKEN_EOF, TOKEN_CALL, TOKEN_INDEX,
	// Comment tokens
	TOKEN_LBLOCK, TOKEN_RBLOCK, TOKEN_DSLASH
};

/*
	Tokens are the basic unit of lexical analysis. They can be easily compared
	and parsed, and can encode complex 2D text into a 1D stream. */
struct token {
    enum tokenType type;
    char data[255];
	struct list* list;
	
	const char* filename;
	int line;
};

struct token* token_create(enum tokenType, char[], const char*, int);
int token_precedence(enum tokenType);
char* token_toString(enum tokenType);

#endif