/*  lexer.c

    Lexical analysis is done before the compiler can do anything else.
    
    The characters in the text file are grouped together into tokens,
    which can be manipulated and parsed easier in later stages of the
    compilation. 

    Author: Joseph Shimel
    Date: 2/3/21
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"
#include "./util/list.h"
#include "./debug.h"

/* These characters are whole tokens themselves */
static const char oneCharTokens[21] = {'{', '}', '[', ']', '(', ')', ';', ',', 
                                      '.', '+', '-', '*', '/', '^', '>', '<', 
                                      '=', '"', '\'', '\n', '~'};

bool charIsToken(char);
int nextToken(const char*, int);
int nextNonWhitespace(const char*, int);
void copyToken(const char* src, char* dst, int start, int end);
int matchComment(struct list* list, struct listElem* elem);

/*
 * Read in a file given a filename, and extracts the characters to a single string
 */
char* lexer_readFile(const char *filename)
{
    int buffer;
    char* fileContents;
    int capacity = 10;
    int size = 0;
    FILE* file = fopen(filename, "r");

    /* Add char to end of content, reallocate when necessary */
    fileContents = malloc(sizeof(char) * capacity);
    while((buffer = fgetc(file)) != EOF)
    {
        size++;
        fileContents = realloc(fileContents, size + 1);
        fileContents[size - 1] = (char)buffer;  /* Convert int buffer to char, append */
    }
    fileContents[size] = '\0'; /* Add the sentinel value */
    fclose(file);
    
    return fileContents;
}

/*
    Takes in a file represented as a string, and creates a list of tokens 
    
    Extend end until reach end of token
    substring(start, end) is token, interpret
    move start up to begining of next token
    if start or end is at the end of file, end. Otherwise, repeat. */
struct list* lexer_tokenize(const char *file) {
    struct list* tokenQueue = list_create();
    struct token* tempToken = NULL;
    enum tokenType tempType;
    int start = 0, end;
    char tokenBuffer[64];
    int fileLength = strlen(file);

    do {
        end = nextToken(file, start);
        copyToken(file, tokenBuffer, start, end);
        tempType = -1;
        if(strcmp("(", tokenBuffer) == 0) {
            tempType = TOKEN_LPAREN;
        } else if(strcmp(")", tokenBuffer) == 0) {
            tempType = TOKEN_RPAREN;
        } else if(strcmp("{", tokenBuffer) == 0) {
            tempType = TOKEN_LBRACE;
        } else if(strcmp("}", tokenBuffer) == 0) {
            tempType = TOKEN_RBRACE;
        } else if(strcmp("[", tokenBuffer) == 0) {
            tempType = TOKEN_LSQUARE;
        } else if(strcmp("]", tokenBuffer) == 0) {
            tempType = TOKEN_RSQUARE;
        } else if(strcmp("\n", tokenBuffer) == 0) {
            tempType = TOKEN_NEWLINE;
        } else if(strcmp("~", tokenBuffer) == 0) {
            tempType = TOKEN_TILDE;
        } else if(strcmp(",", tokenBuffer) == 0) {
            tempType = TOKEN_COMMA;
        } else if(strcmp(".", tokenBuffer) == 0) {
            tempType = TOKEN_DOT;
        } else if(strcmp("\"", tokenBuffer) == 0) {
            tempType = TOKEN_DQUOTE;
        } else if(strcmp("'", tokenBuffer) == 0) {
            tempType = TOKEN_SQUOTE;
        } else if(strcmp("+", tokenBuffer) == 0) {
            tempType = TOKEN_PLUS;
        } else if(strcmp("-", tokenBuffer) == 0) {
            tempType = TOKEN_MINUS;
        } else if(strcmp("/", tokenBuffer) == 0) {
            tempType = TOKEN_DIVIDE;
        } else if(strcmp("*", tokenBuffer) == 0) {
            tempType = TOKEN_MULTIPLY;
        } else if(strcmp("=", tokenBuffer) == 0) {
            tempType = TOKEN_ASSIGN;
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
        } else if(strcmp("module", tokenBuffer) == 0) {
            tempType = TOKEN_MODULE;
        } else if(strcmp("function", tokenBuffer) == 0) {
            tempType = TOKEN_FUNCTION;
        } else if(strcmp("var", tokenBuffer) == 0) {
            tempType = TOKEN_VAR;
        } else if(strcmp("end", tokenBuffer) == 0) {
            tempType = TOKEN_END;
        } else if(strcmp("if", tokenBuffer) == 0) {
            tempType = TOKEN_IF;
        } else if(strcmp("else", tokenBuffer) == 0) {
            tempType = TOKEN_ELSE;
        } else if(strcmp("while", tokenBuffer) == 0) {
            tempType = TOKEN_WHILE;
        } else if(isdigit(tokenBuffer[0])) {
            tempType = TOKEN_NUMLITERAL;
        } else {
            tempType = TOKEN_IDENTIFIER;
        }

        if(tempType != -1) {
            tempToken = lexer_createToken(tempType, tokenBuffer);
            queue_push(tokenQueue, tempToken);
            if(tempType == TOKEN_NEWLINE) {
                LOG("Added token: %s %s", lexer_tokenToString(tempType), "\\n");
            } else {
                LOG("Added token: %s %s", lexer_tokenToString(tempType), tokenBuffer);
            }
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
		case TOKEN_ASSIGN:
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
		case TOKEN_MULTIPLY:
		case TOKEN_DIVIDE:
			return 7;
        case TOKEN_DOT:
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
    case TOKEN_LBRACE:
        return "token:LBRACE";
    case TOKEN_RBRACE:
        return "token:RBRACE";
    case TOKEN_LSQUARE:
        return "token:LSQUARE";
    case TOKEN_RSQUARE:
        return "token:RSQUARE";
    case TOKEN_SEMICOLON:
        return "token:SEMICOLON";
    case TOKEN_COMMA:
        return "token:COMMA";
    case TOKEN_DOT:
        return "token:DOT";
    case TOKEN_DQUOTE:
        return "token:DQUOTE";
    case TOKEN_SQUOTE:
        return "token:SQUOTE";
    case TOKEN_NEWLINE:
        return "token:NEWLINE";
    case TOKEN_EOF:
        return "token:EOF";
    case TOKEN_IDENTIFIER:
        return "token:IDENTFIER";
    case TOKEN_NUMLITERAL:
        return "token:NUMLITERAL";
    case TOKEN_PLUS:
        return "token:PLUS";
    case TOKEN_MINUS: 
        return "token:MINUS";
    case TOKEN_MULTIPLY: 
        return "token:MULTIPLY";
    case TOKEN_DIVIDE: 
        return "token:DIVIDE";
    case TOKEN_ASSIGN:
        return "token:ASSIGN";
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
    case TOKEN_FUNCTION:
        return "token:FUNCTION";
    case TOKEN_VAR:
        return "token:VAR";
    case TOKEN_END:
        return "token:END";
    case TOKEN_IF:
        return "token:IF";
    case TOKEN_ELSE:
        return "token:ELSE";
    case TOKEN_WHILE:
        return "token:WHILE";
    case TOKEN_CALL:
        return "token:CALL";
    case TOKEN_TILDE:
        return "token:TILDE";
    case TOKEN_INDEX:
        return "token:INDEX";
    }
    ASSERT(0); // Unreachable
    return "";
}

/*
    Removes tokens from a list that are surrounded by comments */
void lexer_removeComments(struct list* tokenQueue) {
    struct listElem* elem;
    int withinComment = 0;

    for(elem=list_begin(tokenQueue);elem!=list_end(tokenQueue);elem=list_next(elem)) {
        if(withinComment) {
            if(matchComment(tokenQueue, elem)) {
                withinComment = 0;
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
                free(list_remove(tokenQueue, list_next(elem)));
            } else {
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
            }
        } else {
            if(matchComment(tokenQueue, elem)) {
                withinComment = 1;
                elem = elem->prev;
                free(list_remove(tokenQueue, list_next(elem)));
                free(list_remove(tokenQueue, list_next(elem)));
            }
        }
    }
}

/* Determines if the given character is a token all on it's own */
bool charIsToken(char c) {
    for(int i = 0; i < sizeof(oneCharTokens); i++) {
        if(c == oneCharTokens[i]) return true;
    }
    return false;
}

/*
    Advances the start of the character stream until a non-whitespace 
    character is found */
int nextNonWhitespace(const char* file, int start) {
    char nextChar = file[start];
    while (nextChar != '\0') {
        nextChar = file[start];
        if(!isspace(nextChar) || nextChar == '\n') {
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
int nextToken(const char* file, int start) {
    enum TokenState {
        BEGIN, TEXT, NUMBER
    };

    enum TokenState state = BEGIN;
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
                state = NUMBER;
            }
        } else if(state == TEXT) {
            // Ends on non-alphanumeric character
            if(!isalpha(nextChar) && !isdigit(nextChar)) {
                return start;
            }
        } else if (state == NUMBER) {
            // Ends on non-numeric character
            if(!isdigit(nextChar)) {
                return start;
            }
        }
    }
    return start;
}

/*
    Copies a substring from src into destination */
void copyToken(const char* src, char* dst, int start, int end) {
    int i;
    for(i = 0; i < end-start; i++) {
        dst[i] = src[i + start];
    }
    dst[i] = '\0';
}

/*
    Determines if the list element is the beginning of a comment delimiter */
int matchComment(struct list* list, struct listElem* elem) {
    if(elem != list_end(list) && list_next(elem) != list_end(list)) {
        if( ((struct token*)elem->data)->type == TOKEN_TILDE &&  
            ((struct token*)list_next(elem)->data)->type == TOKEN_TILDE) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}