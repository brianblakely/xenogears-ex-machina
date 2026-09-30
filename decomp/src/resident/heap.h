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

/* Heap state ($gp-relative in the heap unit). */
extern s16 D_80059318;     /* allocation class of the next block */
extern u16 D_8005931C;     /* owner tag of the next block */
extern u8 *D_80059320;     /* data address of the first block */
extern s32 D_8005932C;     /* free blocks await coalescing */
extern s32 D_80059330;     /* failures return NULL instead of stopping */
extern u8 *D_80059334;     /* loaded symbol data ("SYM1") and its end */
extern u8 *D_80059338;
extern s32 D_8005933C;     /* size of the last request */
extern s32 D_80059340;     /* caller of the last request */
extern s32 D_80059FA4[];   /* per-tag words */

/* PsyQ libsn host file access (SDK region): PCinit, PCopen, PClseek, PCread,
 * PCclose. */
extern s32 func_8004C38C(void);
extern s32 func_8004C318(char *name, s32 flags, s32 perms);
extern s32 func_8004C348(s32 fd, s32 offset, s32 mode);
extern s32 func_8004C398(s32 fd, void *buffer, s32 size);
extern s32 func_8004C338(s32 fd);

/* Allocate `size` bytes with an allocation mode, free a block. */
extern void *func_80031BDC(s32 size, s32 mode);
extern void func_800320E8(void *block);

#endif
