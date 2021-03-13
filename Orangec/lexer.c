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

#include "./lexer.h"
#include "./token.h"

#include "../util/list.h"
#include "../util/debug.h"

/* These characters are whole tokens themselves */
static const char oneCharTokens[] = {'{', '}', '(', ')', ';', ',', 
                                      '.', '+', '-', '^', '~', ':', '\n'};
static const char punctuationChars[] = {'<', '>', '=', '[', ']', '&', '|', '!', '/', '*'};

// Private functions
static int nextToken(const char*, int);
static void copyToken(const char* src, char* dst, int start, int end);
static int numIsFloat(const char*);
static void removeQuotes(char* str);
static int nextNonWhitespace(const char*, int);
static bool charIsToken(char);
static bool charIsPunctuation(char c);

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
    char** retval = (char**)malloc(sizeof(char*));
    *numLines = 1;
    int i = 0;
    retval[0] = filestring;
    
    // Go through file string, stop when reached end
    while(filestring[i] != '\0') {
        if(filestring[i] == '\n') {
            (*numLines)++; // Add new char pointer pointer, pointing to just after new line
            retval = realloc(retval, (*numLines + 1) * sizeof(char*));
            retval[*numLines - 1] = filestring + (i + 1);
        }
        i++;
    }

    return retval;
}

/*
    Takes in a file represented as a string, and creates a list of tokens */
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
        if(strcmp("\n", tokenBuffer) == 0) {
            line++;
        } else if(strcmp("(", tokenBuffer) == 0) {
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
        } else if(strcmp("/*", tokenBuffer) == 0) {
            tempType = TOKEN_LBLOCK;
        } else if(strcmp("*/", tokenBuffer) == 0) {
            tempType = TOKEN_RBLOCK;
        } else if(strcmp("//", tokenBuffer) == 0) {
            tempType = TOKEN_DSLASH;
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
        } else if(strcmp("==", tokenBuffer) == 0) {
            tempType = TOKEN_IS;
        } else if(strcmp("!=", tokenBuffer) == 0) {
            tempType = TOKEN_ISNT;
        } else if(strcmp("<", tokenBuffer) == 0) {
            tempType = TOKEN_LESSER;
        } else if(strcmp(">", tokenBuffer) == 0) {
            tempType = TOKEN_GREATER;
        } else if(strcmp("<=", tokenBuffer) == 0) {
            tempType = TOKEN_LESSEREQUAL;
        } else if(strcmp(">=", tokenBuffer) == 0) {
            tempType = TOKEN_GREATEREQUAL;
        } else if(strcmp("&&", tokenBuffer) == 0) {
            tempType = TOKEN_AND;
        } else if(strcmp("||", tokenBuffer) == 0) {
            tempType = TOKEN_OR;
        } else if(strcmp("cast", tokenBuffer) == 0) {
            tempType = TOKEN_CAST;
        } else if(strcmp("new", tokenBuffer) == 0) {
            tempType = TOKEN_NEW;
        } else if(strcmp("free", tokenBuffer) == 0) {
            tempType = TOKEN_FREE;
        } else if(strcmp("verbatim", tokenBuffer) == 0) {
            tempType = TOKEN_VERBATIM;
        } else if(strcmp("true", tokenBuffer) == 0) {
            tempType = TOKEN_TRUE;
        } else if(strcmp("false", tokenBuffer) == 0) {
            tempType = TOKEN_FALSE;
        } else if(strcmp("null", tokenBuffer) == 0) {
            tempType = TOKEN_NULL;
        } else if(strcmp("module", tokenBuffer) == 0) {
            tempType = TOKEN_MODULE;
        } else if(strcmp("struct", tokenBuffer) == 0) {
            tempType = TOKEN_STRUCT;
        } else if(strcmp("enum", tokenBuffer) == 0) {
            tempType = TOKEN_ENUM;
        } else if(strcmp("return", tokenBuffer) == 0) {
            tempType = TOKEN_RETURN;
        } else if(strcmp("[]", tokenBuffer) == 0) {
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
            removeQuotes(tokenBuffer);
        } else if(tokenBuffer[0] == '"'){
            tempType = TOKEN_STRINGLITERAL;
            removeQuotes(tokenBuffer);
        } else {
            tempType = TOKEN_IDENTIFIER;
        }

        if(tempType != -1) {
            tempToken = token_create(tempType, tokenBuffer, filename, line);
            queue_push(tokenQueue, tempToken);
            LOG("Added token: %p %d %s \"%s\"", tempToken, line, token_toString(tempType), tokenBuffer);
        }
        start = nextNonWhitespace(file, end);
        tempToken = NULL;
    } while(end < fileLength);
    queue_push(tokenQueue, token_create(TOKEN_EOF, "EOF", filename, line));
    return tokenQueue;
}

/*
    Runs a basic state machine.

    Symbols are one character long
    Keywords/identifiers start with alpha, contain only alphanumeric
    Numbers contain only digits
    
    Returns the index of the begining of the next token */
static int nextToken(const char* file, int start) {
    enum tokenState {
        BEGIN, TEXT, INTEGER, FLOAT, CHAR, STRING, PUNCTUATION, ESC_CHAR, ESC_STR
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
            // Check punctuation
            else if(charIsPunctuation(nextChar)) {
                state = PUNCTUATION;
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
            if(nextChar == '\\') {
                state = ESC_CHAR;
            }
            if(nextChar == '\''){
                return start + 1;
            }
        } else if (state == ESC_CHAR) {
            state = CHAR;
        } else if (state == STRING) {
            if(nextChar == '"'){
                return start + 1;
            }
            if(nextChar == '\\') {
                state = ESC_STR;
            }
        } else if (state == ESC_STR) {
            state = STRING;
        } else if (state == PUNCTUATION) {
            if(nextChar == ']'){
                return start + 1;
            } else if(isdigit(nextChar) || isalpha(nextChar) || charIsToken(nextChar) || isspace(nextChar)) {
                return start;
            }
        }
    }
    return start;
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

static void removeQuotes(char* str) {
    char temp[255];
    memset(temp, 0, 255);
    int i = 1;
    char first = str[0];

    while (str[i] != first) {
        if(str[i] == '\\') {
            temp[i - 1] = str[i];
            i++;
        }
        temp[i - 1] = str[i];
        i++;
    }
    strcpy(str, temp);
}

/*
    Advances the start of the character stream until a non-whitespace 
    character is found */
static int nextNonWhitespace(const char* file, int start) {
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

/* Determines if the given character is a token all on it's own */
static bool charIsToken(char c) {
    for(int i = 0; i < sizeof(oneCharTokens); i++) {
        if(c == oneCharTokens[i]) return true;
    }
    return false;
}

/* Determines if the given character is a token all on it's own */
static bool charIsPunctuation(char c) {
    for(int i = 0; i < sizeof(punctuationChars); i++) {
        if(c == punctuationChars[i]) return true;
    }
    return false;
}