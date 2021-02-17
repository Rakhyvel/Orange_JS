/*  main.c

    1. Simple components, complex combinations
    2. Lower level control

    Todo:
    x indexing operator
    x array literals
    x else statements
    x return statements
    - array initialize
    - character literal
    - string literal
    - struct definitions
    - struct extension
    - interface definitions
    - struct implements
    - struct interface function
    - global variable definitions
    - static modifier for modules
    - private modifier for higher levels
    - constants
    - imports

    - type validation

    Author: Joseph Shimel
    Date: 2/2/21
*/

#include <stdio.h>
#include <stdlib.h>

#include "./main.h"
#include "./util/list.h"
#include "./lexer.h"
#include "./parser.h"
#include "./debug.h"

/*
 * Takes in an array of files to compile
 * 
 * 1. Lex: Read in files specified to compiler
 * 2.      Create token list from file, remove the comments
 * 3. Parse: Look through each file, add code to functions to modules to the program
 * 4. Validation: Look through AST's, validate type, struct members, module members, state access, etc.
 * 5. Generate code (compile, release) or evaluate tree (interpret, debug)
 */
int main(int argn, char** argv)
{
    struct program* program = parser_initProgram();

    for(int i = 1; i < argn; i++)
    {
        LOG("Reading file %s", argv[i]);
        char* file = lexer_readFile(argv[i]);

        LOG("\n\nBegin Tokenization.");
        struct list* tokenQueue = lexer_tokenize(file);
        lexer_removeComments(tokenQueue);
        LOG("\nEnd Tokenization\n");

        LOG("\n\nBegin Parsing.");
        parser_addModules(program, tokenQueue);
        LOG("\nEnd Parsing.\n");
    }
    printf("Done.\n");
    return 0;
}