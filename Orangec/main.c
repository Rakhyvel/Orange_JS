/*  main.c

    The idea of Orange is a language that is simple and easy enough for beginners
    to learn the basics of programming, and have them be able to construct
    good, useful, small to large sized projects.
    
    It should be so basic and universal that it can be compiled to any other 
    programming language. This in turn will make it so that beginners transfering
    on to other languages will have an easier time transfering concepts over,
    and will think about programming the way they did in Orange. 

    My goal is to keep the source code under 3,600 lines of code.

    Author: Joseph Shimel
    Date: 2/2/21
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Each module, in order of execution
#include "./main.h"
#include "./lexer.h"
#include "./parser.h"
#include "./validator.h"
#include "./generator.h"

#include "../util/debug.h"
#include "../util/list.h"
#include "../util/map.h"

static void readInputFile(char* filename);

/*
 * Takes in an array of files to compile
 * 
 * 1. Lex: Read in files specified to compiler
 * 2.      Create token list from file, remove the comments
 * 3. Parse: Look through each file, add code to functions to modules to the program
 * 4. Validation: Look through AST's, validate type, struct members, module members, state access, etc.
 * 5. Generate code (compile, release) or evaluate tree (interpret, debug)
 */
int main(int argn, char** argv) {
    if(argn < 2) {
        printf("Usage: orangec filename_1 filename_2 ... filename_n\n");
        exit(1);
    }

    enum argState {
        NORMAL, TARGET, OUTPUT
    };
    enum argState state = NORMAL;
    program = parser_createSymbolNode(SYMBOL_PROGRAM, NULL, NULL, -1);
    fileMap = map_create();

    for(int i = 1; i < argn; i++) {
        switch(state) {
        case NORMAL:
            if(!strcmp(argv[i], "-o")) {
                state = OUTPUT;
            } else if(!strcmp(argv[i], "-t")) {
                state = TARGET;
            } else {
                readInputFile(argv[i]);
            }
            break;
        case TARGET:
            strncpy(program->type, argv[i], 255);
            state = NORMAL;
            break;
        case OUTPUT:
            strncpy(program->name, argv[i], 255);
            state = NORMAL;
            break;
        }
    }

    LOG("\nBegin Validating.");
    validator_validate(program);
    LOG("\nEnd Validating.\n");

    LOG("\nBegin Generation.");
    FILE* out = fopen(program->name, "w");
    if(out == NULL) {
        perror(program->name);
        exit(1);
    }
    generator_generate(out);
    if(fclose(out) == EOF) {
        perror(program->name);
        exit(1);
    }
    LOG("\nEnd Generation.");

    printf("Done.\n");
    return 0;
}

static void readInputFile(char* filename) {
    LOG("Reading file %s", filename);
    FILE* file = fopen(filename, "r");
    if(file == NULL) {
        perror(filename);
        exit(1);
    }
    char* filestring = lexer_readFile(file);
    if(fclose(file) == EOF) {
        perror(filename);
        exit(1);
    }
    int numLines;
    char** lines = lexer_getLines(filestring, &numLines);
    struct file* fileStruct = malloc(sizeof(struct file));
    fileStruct->lines = lines;
    fileStruct->nLines = numLines;
    map_put(fileMap, filename, fileStruct);
    LOG("End file reading");

    LOG("\n\nBegin Tokenization.");
    struct list* tokenQueue = lexer_tokenize(filestring, filename);
    LOG("\nEnd Tokenization\n");

    LOG("\n\nBegin Parsing.");
    parser_removeComments(tokenQueue);
    parser_condenseArrayIdentifiers(tokenQueue);
    parser_addModules(program, tokenQueue);
    LOG("\nEnd Parsing.\n");
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
        println(((struct file*)map_get(fileMap, filename))->lines[line]);
    }
    va_end(args);
  
    exit (1);
}