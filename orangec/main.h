/*  main.h

    Author: Joseph Shimel
    Date: 2/2/21
*/

#ifndef MAIN_H
#define MAIN_H

#include "./parser.h"

/*
    Represents a file, holds an array of lines, and the number of lines.
    
    Program structs contain a map of these that is accessed when an error
    message is printed out, so that the line that an error occured on can be
    printed out as well */
struct file {
    char** lines;
    int nLines;
};

struct program* program;

void error(const char* filename, int line, const char* msg, ...);

#endif