#ifndef PSYQ_STDARG_H
#define PSYQ_STDARG_H

/* The port build reads variable arguments through the compiler's own list:
 * the PsyQ macros walk the MIPS argument words in memory. */
typedef __builtin_va_list va_list;

#define va_start(AP, LASTARG) __builtin_va_start(AP, LASTARG)
#define va_end(AP) __builtin_va_end(AP)
#define va_arg(AP, TYPE) __builtin_va_arg(AP, TYPE)

#endif
