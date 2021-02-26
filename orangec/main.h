#ifndef MAIN_H
#define MAIN_H

struct file {
    char** lines;
    int nLines;
};

void error(const char* msg, ...);

#endif