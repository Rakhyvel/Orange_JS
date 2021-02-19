// I copied all of this code from some PintOS github

#include "./debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

void debug_panic (const char *file, int line, const char *function,
             const char *message, ...) {
  va_list args;

  fprintf(stderr, "ERROR: %s:%d in %s(): ", file, line, function);

  va_start (args, message);
  vprintf (message, args);
  printf ("\n");
  va_end(args);
  
  exit (1);
}

void debug_log(const char *file, int line, const char *function,
                  const char *message, ...) {
  #ifdef VERBOSE
  va_list args;

  fprintf(stderr, "LOG: %s:%d in %s(): ", file, line, function);

  va_start (args, message);
  vprintf (message, args);
  printf ("\n");
  va_end(args);
  #endif
}