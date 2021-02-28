/*  lexer.c

    Lexical analysis is done before the compiler can do anything else.
    
    The characters in the text file are grouped together into tokens,
    which can be manipulated and parsed easier in later stages of the
    compilation. 

    - The lexer DOES NOT care if the tokens are in a proper order
    - The lexer ONLY turns the text data into a token queue

    Author: Joseph Shimel
    Date: 2/3/21
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"
#include "../util/list.h"
#include "../util/debug.h"

/* These characters are whole tokens themselves */
static const char oneCharTokens[] = {'{', '}', '[', ']', '(', ')', ';', ',', 
                                      '.', '+', '-', '*', '/', '^', '>', '<', 
                                      '=', '~', ':'};

// Private functions
char* readLine(FILE* file);
static bool charIsToken(char);
static int nextToken(const char*, int);
static int numIsFloat(const char*);
static int nextNonWhitespace(const char*, int);
static void copyToken(const char* src, char* dst, int start, int end);

/*
 * Read in a file given a filename, and extracts the characters to a single string
 */
char* lexer_readFile(FILE* file) {
    int buffer;
    char* fileContents;
    int size = 0;

    /* Add char to end of content, reallocate when necessary */
    fileContents = malloc(sizeof(char));
    while((buffer = fgetc(file)) != EOF)
    {
        size++;
        fileContents = realloc(fileContents, size + 1);
        fileContents[size - 1] = (char)buffer;  /* Convert int buffer to char, append */
    }
    fileContents[size] = '\0'; /* Add the sentinel value */
    
    return fileContents;
}

/*
    Takes in a string representing a file, returns an array of pointers to 
    characters just after newlines.
    
    Used for setting up the data structure for error message printing, where
    errors print out the line where an error occured */
char** lexer_getLines(char* filestring, int* numLines) {
    char** retval = malloc(sizeof(char*));
    *numLines = 1;
    int i = 0;
    retval[0] = filestring;
    
    while(filestring[i] != '\0') {
        if(filestring[i] == '\n') {
            (*numLines)++;
            retval = realloc(retval, (*numLines + 1) * sizeof(char*));
            retval[*numLines - 1] = filestring + (i + 1);
        }
        i++;
    }

    return retval;
}

/*
    Takes in a file represented as a string, and creates a list of tokens 
    
    Extend end until reach end of token
    substring(start, end) is token, interpret
    move start up to begining of next token
    if start or end is at the end of file, end. Otherwise, repeat. */
struct list* lexer_tokenize(const char *file, const char* filename) {
    struct list* tokenQueue = list_create();
    struct token* tempToken = NULL;
    enum tokenType tempType;
    int start = 0, end;
    char tokenBuffer[64];
    int fileLength = strlen(file);
    int line = 0;

    do {
        end = nextToken(file, start);
        copyToken(file, tokenBuffer, start, end);
        tempType = -1;
        if(strcmp("(", tokenBuffer) == 0) {
            tempType = TOKEN_LPAREN;
        } else if(strcmp(")", tokenBuffer) == 0) {
            tempType = TOKEN_RPAREN;
        } else if(strcmp("[", tokenBuffer) == 0) {
            tempType = TOKEN_LSQUARE;
        } else if(strcmp("]", tokenBuffer) == 0) {
            tempType = TOKEN_RSQUARE;
        } else if(strcmp("{", tokenBuffer) == 0) {
            tempType = TOKEN_LBRACE;
        } else if(strcmp("}", tokenBuffer) == 0) {
            tempType = TOKEN_RBRACE;
        } else if(strcmp("~", tokenBuffer) == 0) {
            tempType = TOKEN_TILDE;
        } else if(strcmp(",", tokenBuffer) == 0) {
            tempType = TOKEN_COMMA;
        } else if(strcmp(";", tokenBuffer) == 0) {
            tempType = TOKEN_SEMICOLON;
        } else if(strcmp(".", tokenBuffer) == 0) {
            tempType = TOKEN_DOT;
        } else if(strcmp(":", tokenBuffer) == 0) {
            tempType = TOKEN_COLON;
        } else if(strcmp("+", tokenBuffer) == 0) {
            tempType = TOKEN_PLUS;
        } else if(strcmp("-", tokenBuffer) == 0) {
            tempType = TOKEN_MINUS;
        } else if(strcmp("/", tokenBuffer) == 0) {
            tempType = TOKEN_SLASH;
        } else if(strcmp("*", tokenBuffer) == 0) {
            tempType = TOKEN_STAR;
        } else if(strcmp("=", tokenBuffer) == 0) {
            tempType = TOKEN_EQUALS;
        } else if(strcmp("is", tokenBuffer) == 0) {
            tempType = TOKEN_IS;
        } else if(strcmp("isnt", tokenBuffer) == 0) {
            tempType = TOKEN_ISNT;
        } else if(strcmp("<", tokenBuffer) == 0) {
            tempType = TOKEN_LESSER;
        } else if(strcmp(">", tokenBuffer) == 0) {
            tempType = TOKEN_GREATER;
        } else if(strcmp("and", tokenBuffer) == 0) {
            tempType = TOKEN_AND;
        } else if(strcmp("or", tokenBuffer) == 0) {
            tempType = TOKEN_OR;
        } else if(strcmp("true", tokenBuffer) == 0) {
            tempType = TOKEN_TRUE;
        } else if(strcmp("false", tokenBuffer) == 0) {
            tempType = TOKEN_FALSE;
        } else if(strcmp("module", tokenBuffer) == 0) {
            tempType = TOKEN_MODULE;
        } else if(strcmp("struct", tokenBuffer) == 0) {
            tempType = TOKEN_STRUCT;
        } else if(strcmp("interface", tokenBuffer) == 0) {
            tempType = TOKEN_INTERFACE;
        } else if(strcmp("return", tokenBuffer) == 0) {
            tempType = TOKEN_RETURN;
        } else if(strcmp("end", tokenBuffer) == 0) {
            tempType = TOKEN_END;
        } else if(strcmp("array", tokenBuffer) == 0) {
            tempType = TOKEN_ARRAY;
        } else if(strcmp("static", tokenBuffer) == 0) {
            tempType = TOKEN_STATIC;
        } else if(strcmp("const", tokenBuffer) == 0) {
            tempType = TOKEN_CONST;
        } else if(strcmp("private", tokenBuffer) == 0) {
            tempType = TOKEN_PRIVATE;
        } else if(strcmp("if", tokenBuffer) == 0) {
            tempType = TOKEN_IF;
        } else if(strcmp("else", tokenBuffer) == 0) {
            tempType = TOKEN_ELSE;
        } else if(strcmp("while", tokenBuffer) == 0) {
            tempType = TOKEN_WHILE;
        } else if(isdigit(tokenBuffer[0])) {
            if(numIsFloat(tokenBuffer)) {
                tempType = TOKEN_REALLITERAL;
            } else {
                tempType = TOKEN_INTLITERAL;
            }
        } else if(tokenBuffer[0] == '\''){
            tempType = TOKEN_CHARLITERAL;
        } else if(tokenBuffer[0] == '"'){
            tempType = TOKEN_STRINGLITERAL;
        } else {
            tempType = TOKEN_IDENTIFIER;
        }

        if(tempType != -1) {
            tempToken = lexer_createToken(tempType, tokenBuffer);
            queue_push(tokenQueue, tempToken);
            tempToken->filename = filename;
            tempToken->line = line;
            LOG("Added token: %s %s", lexer_tokenToString(tempType), tokenBuffer);
        }
        start = nextNonWhitespace(file, end);
        tempToken = NULL;
    } while(end < fileLength);
    queue_push(tokenQueue, lexer_createToken(TOKEN_EOF, "EOF"));
    return tokenQueue;
}

/*
    Creates a token with a given type and data */
struct token* lexer_createToken(enum tokenType type, char data[]) {
    struct token* retval = (struct token*) malloc(sizeof(struct token));
    retval->type = type;
    retval->list = list_create();
    strncpy(retval->data, data, 254);
    return retval;
}

/* Returns the precedence a token operator has */
int lexer_getTokenPrecedence(enum tokenType type) {
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
			return 5;
		case TOKEN_PLUS:
		case TOKEN_MINUS:
			return 6;
		case TOKEN_STAR:
		case TOKEN_SLASH:
			return 7;
        case TOKEN_DOT:
        case TOKEN_COLON:
        case TOKEN_INDEX:
            return 8;
		default:
			return 0;
    }
}

/*
    Returns a string representation of a token type */
char* lexer_tokenToString(enum tokenType type) {
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
	case TOKEN_AND: 
        return "token:AND";
    case TOKEN_OR:
        return "token:OR";
    case TOKEN_MODULE:
        return "token:MODULE";
    case TOKEN_STRUCT:
        return "token:STRUCT";
    case TOKEN_INTERFACE:
        return "token:INTERFACE";
    case TOKEN_END:
        return "token:END";
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
    }
    ASSERT(0); // Unreachable
    return "";
}

/* Determines if the given character is a token all on it's own */
static bool charIsToken(char c) {
    for(int i = 0; i < sizeof(oneCharTokens); i++) {
        if(c == oneCharTokens[i]) return true;
    }
    return false;
}

/*
    Advances the start of the character stream until a non-whitespace 
    character is found */
static int nextNonWhitespace(const char* file, int start) {
    char nextChar = file[start];
    while (nextChar != '\0') {
        nextChar = file[start];
        if(!isspace(nextChar)) {
            return start;
        }
        start++;
    }
    return start;
}

/*
    Runs a basic state machine.

    Symbols are one character long
    Keywords/identifiers start with alpha, contain only alphanumeric
    Numbers contain only digits
    
    Returns the index of the begining of the next token */
static int nextToken(const char* file, int start) {
    enum tokenState {
        BEGIN, TEXT, INTEGER, FLOAT, CHAR, STRING
    };

    enum tokenState state = BEGIN;
    char nextChar;

    for( ; nextChar != '\0'; start++) {
        nextChar = file[start];
        if(state == BEGIN) {
            // Check one character tokens
            if(charIsToken(nextChar)) {
                return start + 1;
            } 
            // Check keyword/identifier
            else if(isalpha(nextChar)) {
                state = TEXT;
            } 
            // Check number
            else if(isdigit(nextChar)) {
                state = INTEGER;
            }
            // Check char
            else if(nextChar == '\'') {
                state = CHAR;
            }
            // Check string
            else if(nextChar == '"') {
                state = STRING;
            }
        } else if(state == TEXT) {
            // Ends on non-alphanumeric character
            if(!isalpha(nextChar) && !isdigit(nextChar) && nextChar != '_') {
                return start;
            }
        } else if (state == INTEGER) {
            // Ends on non-numeric character
            if(nextChar == '.') {
                state = FLOAT;
            } else if(!isdigit(nextChar)) {
                return start;
            }
        } else if (state == FLOAT) {
            // Ends on non-numeric character
            if(!isdigit(nextChar)) {
                return start;
            }
        } else if (state == CHAR) {
            if(nextChar == '\''){
                return start + 1;
            }
        } else if (state == STRING) {
            if(nextChar == '"'){
                return start + 1;
            }
        }
    }
    return start;
}

/*
    Determines if a given string is a float */
static int numIsFloat(const char* test) {
    for(int i = 0; test[i] != '\0'; i++) {
        if(test[i] == '.') {
            return 1;
        }
    }
    return 0;
}

/*
    Copies a substring from src into destination */
static void copyToken(const char* src, char* dst, int start, int end) {
    int i;
    for(i = 0; i < end-start; i++) {
        dst[i] = src[i + start];
    }
    dst[i] = '\0';
}