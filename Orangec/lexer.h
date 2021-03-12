/*  lexer.h

    Author: Joseph Shimel
    Date: 2/3/21
*/

#ifndef LEXER_H
#define LEXER_H

char* lexer_readFile(FILE*);
char** lexer_getLines(char*, int*);
struct list* lexer_tokenize(const char*, const char*);

#endif