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

extern void (*heap_report_output)(char *line); /* heap report output */

/* Heap blocks (0x80031894-0x80032e7c). */
s32 heap_get_block_size(u8 *data);
s32 heap_get_block_tag(u8 *data);
u32 heap_get_block_caller(u8 *data);
s32 heap_is_block_protected(u8 *data);
void heap_empty_entry(void);
s32 heap_load_host_symbols(char *name);
void heap_reset_defaults(void);
void heap_init(HeapHeader *first, u8 *end);
void heap_move_start(HeapHeader *first);
s32 heap_get_owner_tag(void);
void heap_set_owner_tag(s32 tag);
s32 heap_set_quiet_failures(s32 quiet);
void heap_get_last_request(s32 *caller, s32 *size);
void *heap_alloc(s32 size, s32 mode);  /* allocate `size` bytes */
HeapHeader *heap_shrink_block(u8 *data, s32 size);
void heap_coalesce(void);
void heap_protect_block(void *data);   /* keep the block across heap restarts */
void heap_unprotect_block(void *data); /* stop keeping the block */
void heap_unprotect_block_copy(void *data);
s32 heap_free(void *data);             /* release a block */
void heap_free_all(void);
void heap_force_free_all(void);
s32 heap_get_free_total(void);
s32 heap_walk_blocks(void);
u32 heap_get_largest_free(void);
void heap_select_owner_tag(s32 tag, char **names);
void heap_find_symbol_name(u32 address, char *out);
void heap_print_report(s32 mode, s32 skip, s32 count, s32 flags);
void *heap_alloc_sound_block(s32 size);
void *heap_fake_calloc(s32 count, s32 size);
void heap_force_free(void *data);
/* Original calls pass the format alone or one argument word. The shim
 * forwards incoming a1 to sprintf; preserve this C89 unspecified arity. */
void heap_report_printf();
void heap_delay_free(void *data, s32 frames);
void heap_update_delayed_frees(void);
void heap_flush_delayed_frees(void);
void heap_write_report_line(char *line);
void heap_write_report_file(char *name);
extern void *mode_battle_heap_reservation; /* the reservation below the heap marker */
extern void *mode_battle_heap_marker; /* the heap marker for the high-memory reservation */

#endif
