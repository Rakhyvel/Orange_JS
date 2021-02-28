/*  main.c

    The idea of Orange is a language that is simple and easy enough for beginners
    to learn the basics of programming, and have them be able to construct
    good, useful, small to large sized projects.
    
    It should be so basic and universal that it can be compiled to any other 
    programming language. This in turn will make it so that beginners transfering
    on to other languages will have an easier time transfering concepts over,
    and will think about programming the way they did in Orange. 

    Author: Joseph Shimel
    Date: 2/2/21
*/

#include <stdio.h>
#include <stdlib.h>

#include "./main.h"
#include "./lexer.h"
#include "./parser.h"
#include "./validator.h"

#include "../util/debug.h"
#include "../util/list.h"
#include "../util/map.h"

static struct program* program; // Represents the program as a whole

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
    program = parser_initProgram();

    for(int i = 1; i < argn; i++)
    {
        LOG("Reading file %s", argv[i]);
        FILE* file = fopen(argv[i], "r");
        char* filestring = lexer_readFile(file);
        fclose(file);
        int numLines;
        char** lines = lexer_getLines(filestring, &numLines);
        struct file* fileStruct = malloc(sizeof(struct file));
        fileStruct->lines = lines;
        fileStruct->nLines = numLines;
        map_put(program->fileMap, argv[i], fileStruct);
        LOG("End file reading");

        LOG("\n\nBegin Tokenization.");
        struct list* tokenQueue = lexer_tokenize(filestring, argv[i]);
        LOG("\nEnd Tokenization\n");

        LOG("\n\nBegin Parsing.");
        parser_removeComments(tokenQueue);
        parser_addModules(program, tokenQueue);
        LOG("\nEnd Parsing.\n");
    }

    LOG("\nBegin Validating.");
    validator_validate(program);
    LOG("\nEnd Validating.\n");
    printf("Done.\n");
    return 0;
}

/*
    Takes a pointer to a character, prints characters out until reaches new 
    line or end of string */
void println(const char* line) {
    int i = 0;
    int hasBegun = 0;
    while (line[i] != '\0' && line[i] != '\n'){
        if(line[i] != '\t' && line[i] != ' ') {
            hasBegun = 1;
        }
        if(hasBegun){
            printf("%c", line[i]);
        }
        i ++;
    }
    printf("\n\n");
}

/*
    Prints out an error message, with a filename and line number if one is 
    provided.
    
    Works with varargs to provide cool printing features. */
void error(const char* filename, int line, const char *message, ...) {
    va_list args;
    if(filename != NULL) {
        fprintf(stderr, "%s:%d error: ", filename, (line + 1));
    } else {
        fprintf(stderr, "error: ");
    }

    va_start (args, message);
    vfprintf (stderr, message, args);
    if(filename != NULL) {
        fprintf (stderr, "\n%d |\t", (line + 1));
        println(((struct file*)map_get(program->fileMap, filename))->lines[line]);
    }
    va_end(args);
  
    exit (1);
}