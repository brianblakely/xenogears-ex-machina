#ifndef RESIDENT_HEAP_H
#define RESIDENT_HEAP_H

#include "common.h"

/* Resident game heap. Every block's data is preceded by this header; the
 * blocks form a list through `next`, the data address of the following
 * block. Tag 0 marks a free block and tag 1 the end of the heap. */
typedef struct {
    u8 *next;
    u32 caller : 21; /* word address of the allocating call */
    u32 tag : 4;     /* owner tag */
    u32 keep : 1;    /* release refuses the block */
    u32 kind : 6;    /* allocation class */
} HeapHeader;

#define HEAP_HEADER(data) ((HeapHeader *)(data) - 1)

/* Store the return address at `p`; consumers must reload the written word. */
#define GET_RA(p) __asm__ volatile("move $15, %0\n\tsw $31, 0($15)" : : "r"(p) : "$15", "memory")

/* A release deferred by `frames` frames ("DelayFree" blocks). */
typedef struct DelayedFree {
    struct DelayedFree *next;
    void *data;
    s32 frames;
} DelayedFree;

extern void (*D_800592B8)(char *line); /* heap report output */

/* Heap blocks (0x80031894-0x80032e7c). */
s32 func_80031894(u8 *data);
s32 func_800318A8(u8 *data);
u32 func_800318BC(u8 *data);
s32 func_800318DC(u8 *data);
void func_800318F0(void);
s32 func_800318F8(char *name);
void func_80031A30(void);
void func_80031A68(HeapHeader *first, u8 *end);
void func_80031B10(HeapHeader *first);
s32 func_80031B9C(void);
void func_80031BA8(s32 tag);
s32 func_80031BB4(s32 quiet);
void func_80031BC4(s32 *caller, s32 *size);
void *func_80031BDC(s32 size, s32 mode); /* allocate `size` bytes */
HeapHeader *func_80031F70(u8 *data, s32 size);
void func_80031FF8(void);
void func_800320A4(void *data); /* keep the block across heap restarts */
void func_800320B8(void *data); /* stop keeping the block */
void func_800320D0(void *data);
s32 func_800320E8(void *data);  /* release a block */
void func_8003223C(void);
void func_800322B4(void);
s32 func_80032340(void);
s32 func_800323B4(void);
u32 func_80032404(void);
void func_80032498(s32 tag, s32 value);
void func_800324C4(u32 address, char *out);
void func_8003278C(s32 mode, s32 skip, s32 count, s32 flags);
void *func_80032B0C(s32 size);
void *func_80032B64(s32 count, s32 size);
void func_80032BAC(void *data);
/* Original calls pass the format alone or one argument word. The shim
 * forwards incoming a1 to sprintf; preserve this C89 unspecified arity. */
void func_80032BDC();
void func_80032C18(void *data, s32 frames);
void func_80032CB8(void);
void func_80032D60(void);
void func_80032DCC(char *line);
void func_80032E04(char *name);

#endif
