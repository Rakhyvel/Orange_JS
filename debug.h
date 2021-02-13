#ifndef DEBUG_H
#define DEBUG_H

/* GCC lets us add "attributes" to functions, function
   parameters, etc. to indicate their properties.
   See the GCC manual for details. */
#define UNUSED __attribute__ ((unused))
#define NO_RETURN __attribute__ ((noreturn))
#define NO_INLINE __attribute__ ((noinline))
#define PRINTF_FORMAT(FMT, FIRST) __attribute__ ((format (printf, FMT, FIRST)))

/* Halts the OS, printing the source file name, line number, and
   function name, plus a user-specific message. */
#define PANIC(...) debug_panic (__FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG(...) debug_log (__FILE__, __LINE__, __func__, __VA_ARGS__)

void debug_panic (const char *file, int line, const char *function,
                  const char *message, ...) PRINTF_FORMAT (4, 5) NO_RETURN;
void debug_log (const char *file, int line, const char *function,
                  const char *message, ...) PRINTF_FORMAT (4, 5);
void debug_backtrace (void);
void debug_backtrace_all (void);

typedef __builtin_va_list va_list;

#define va_start(LIST, ARG)	__builtin_va_start (LIST, ARG)
#define va_end(LIST)            __builtin_va_end (LIST)
#define va_arg(LIST, TYPE)	__builtin_va_arg (LIST, TYPE)
#define va_copy(DST, SRC)	__builtin_va_copy (DST, SRC)

#endif



/* This is outside the header guard so that debug.h may be
   included multiple times with different settings of NDEBUG. */
#undef ASSERT
#undef NOT_REACHED

#ifndef NDEBUG
#define ASSERT(CONDITION)                                       \
        if (CONDITION) { } else {                               \
                PANIC ("assertion '%s' failed.", #CONDITION);   \
        }
#define NOT_REACHED() PANIC ("executed an unreachable statement");
#else
#define ASSERT(CONDITION) ((void) 0)
#define NOT_REACHED() for (;;)
#endif /* lib/debug.h */