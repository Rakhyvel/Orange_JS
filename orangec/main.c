/*  main.c

    The idea of Orange is a language that is simple and easy enough for beginners
    to learn the basics of programming, and have them be able to construct
    good, useful, small to large sized projects.
    
    It should be so basic and universal that it can be compiled to any other 
    programming language. This in turn will make it so that beginners transfering
    on to other languages will have an easier time transfering concepts over,
    and will think about programming the way they did in Orange. 

    Modules:
        Modules contain functions and sometimes globals.
        Modules can be static. Static modules can define global varibales.
        Non static modules can define global constants
        Modules provide a namespace for functions and global variables

        <static >module name
            ... code ...
        end
    Structs:
        Structs are a collection of named, bundled variables
        Structs are neccesarily defined in modules, but belong to the program as a whole
        Structs can be private
        Private structs can only be used in their containing module
        Structs are initialized by calling the function with their name
        Struct types are always references to the location of a struct on the heap (unlike C structs)
        Struct's fields can be accessed by using the dot (.) operator
        Structs can extend other structs, and in doing so inherit their fields
        Struct's inherited fields come before their own
        Therefore, a reference to a struct is also a refrence to all their parents
        Struct definition is nebulous

        <private >struct name<[extended struct]>(parameters)
    Globals:
        Global variables belong to modules.
        Globals can be private, meaning they can only be accessed by code in the same module
        Globals can be constant, meaning their value cannot be changed
        Globals must be initialized
        Globals cannot be initialized using other non-constant globals, or functions from static modules
        Public globals can be accessed using the colon (:) operator
        Global definition is order dependent

        <private ><const >type name = expr
    Functions:
        Functions take in arguments, perform code, and give output
        Two functions cannot have the same name
        When calling a function, the argument types and number must match
        Arguments and function variables cannot have the same name
        A function must return its declared type
    Return:
        Returns set their function's return value, and end the function
        Void functions can be returns with a simple "return" and no expression
    While:
    If:
    Arrays:
    Variables:
    Math:

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

        LOG("\nBegin Validating.");
        validator_validate(program);
        LOG("\nEnd Validating.\n");
    }
    printf("Done.\n");
    return 0;
}

void error(const char *message, ...) {
    va_list args;

    fprintf(stderr, "error: ");

    va_start (args, message);
    vfprintf (stderr, message, args);
    fprintf (stderr, "\n");
    va_end(args);
  
    exit (1);
}