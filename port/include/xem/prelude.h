#ifndef XEM_PRELUDE_H
#define XEM_PRELUDE_H

/*
 * Force-included first by every port-build unit (-include). Each header here
 * replaces the original of the same path under decomp/include and carries its
 * include guard, so the original, which other headers include by quotes from
 * its own directory, expands to nothing when reached.
 */
#include "../include_asm.h"
#include "../psyq/stdarg.h"
#include "prototypes.h"

#endif
