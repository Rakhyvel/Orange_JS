/*  lexer.h

    Author: Joseph Shimel
    Date: 2/3/21
*/

#ifndef LEXER_H
#define LEXER_H

#include "./util/list.h"

/*
	Tokens have types that distinguish them from other tokens easily */
enum tokenType {
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LSQUARE, TOKEN_RSQUARE,
	// Punctuation
	TOKEN_SEMICOLON, TOKEN_COMMA, TOKEN_DOT, TOKEN_NEWLINE, TOKEN_TILDE, TOKEN_COLON,
	// Literals
	TOKEN_IDENTIFIER, TOKEN_INTLITERAL, TOKEN_REALLITERAL, TOKEN_CHARLITERAL, TOKEN_STRINGLITERAL,
	// Math operators
	TOKEN_PLUS, TOKEN_MINUS, TOKEN_MULTIPLY, TOKEN_DIVIDE, TOKEN_ASSIGN,
	// Branch operators
	TOKEN_IS, TOKEN_ISNT, TOKEN_GREATER, TOKEN_LESSER,
	// Boolean operators
	TOKEN_AND, TOKEN_OR,
	// Programatic structures
	TOKEN_MODULE, TOKEN_STRUCT, TOKEN_INTERFACE, TOKEN_END,
	// Modifiers
	TOKEN_ARRAY, TOKEN_STATIC,
	// Control flow structures
	TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_RETURN,
	// Anonymous tokens
	TOKEN_EOF, TOKEN_CALL, TOKEN_INDEX
};

/*
	Tokens are the basic unit of lexical analysis. They can be easily compared
	and parsed, and can encode complex 2D text into a 1D stream. */
struct token {
    enum tokenType type;
    char data[255];
	struct list* list;
};

char* lexer_readFile(const char *filename);
struct list* lexer_tokenize(const char *file);
struct token* lexer_createToken(enum tokenType, char[]);
int lexer_getTokenPrecedence(enum tokenType);
char* lexer_tokenToString(enum tokenType);

#endif