#ifndef COMMON_H
#define COMMON_H

#include "include_asm.h"

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;

#ifndef NULL
#define NULL ((void *)0)
#endif

/* A 16.16 fixed-point value whose whole part code also reads on its own: the
 * field's positions and camera, the sound driver's rates and levels. */
typedef union {
    s32 value;
    struct {
        u16 fraction;
        s16 whole;
    } part;
} Fixed;

/* A member's byte offset and a compile-time layout check (a negative array
 * size fails the build); typedefs emit nothing. */
#define OFFSET_OF(type, member) ((u32) & ((type *)0)->member)
#define LAYOUT_CHECK(name, condition) typedef char name[(condition) ? 1 : -1]

#endif
