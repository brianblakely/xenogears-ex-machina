/* The game functions the port's resident routines call, as test doubles that
 * tests/test_port_lzss.py and tests/test_port_resident.py drive: heap_alloc
 * returns a chosen block and records its arguments; console_vprintf records
 * its target, format and first two integer arguments. */
#include "psyq/stdarg.h"

unsigned int test_heap_block;
int test_heap_calls;
int test_heap_size;
int test_heap_mode;

void *heap_alloc(int size, int mode) {
    test_heap_calls++;
    test_heap_size = size;
    test_heap_mode = mode;
    return (void *)(__UINTPTR_TYPE__)test_heap_block;
}

int test_vprintf_calls;
int test_vprintf_target;
const char *test_vprintf_format;
int test_vprintf_args[2];

int console_vprintf(int target, const char *format, va_list args) {
    test_vprintf_calls++;
    test_vprintf_target = target;
    test_vprintf_format = format;
    test_vprintf_args[0] = va_arg(args, int);
    test_vprintf_args[1] = va_arg(args, int);
    return 0;
}
