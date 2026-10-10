/* Resident game heap: block headers, allocation, release and the heap
 * report. Compiled with -G8: its small globals are $gp-relative. */
#include "common.h"

#include "psyq/libc.h"
#include "psyq/libsn.h"
#include "resident/console.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "own_declarations.h"

/* The heap state, this unit's own variables. GCC emits them after its small
 * data and the assembler allocates those of up to 8 bytes in the unit's
 * .sbss ($gp-relative, 80059318), the larger in its .bss (80059fa4), each
 * in a slot of whole words. */
static u16 heap_next_class;     /* 80059318: allocation class of the next block */
static u16 heap_owner_tag;     /* 8005931C: owner tag of the next block */
static u8 *heap_first_block;     /* 80059320: data address of the first block */
static s32 heap_unreferenced_word_1;     /* 80059324: unreferenced */
static s32 heap_unreferenced_word_2;     /* 80059328: unreferenced */
static s32 heap_needs_coalescing;     /* 8005932C: free blocks await coalescing */
static s32 heap_quiet_failures;     /* 80059330: failures return NULL instead of stopping */
/* Loaded host symbols: the complete "SYM1" file and its byte limit. They
 * are separate scalars: heap_init clears them ahead of its block-header
 * stores, which GCC's alias rules allow for scalars but not for members of a
 * global struct. */
static u8 *heap_symbol_data; /* 80059334 */
static u8 *heap_symbol_data_end; /* 80059338 */
static s32 heap_last_request_size;     /* 8005933C: size of the last request */
static s32 heap_last_request_caller;     /* 80059340: caller of the last request */
static s32 heap_unreferenced_word_3;     /* 80059344: unreferenced */
static char **heap_tag_class_name_tables[10]; /* 80059FA4: per tag, the names of its allocation classes */
/* The delayed releases: one list head, but not small data. */
static DelayedFree *heap_delayed_frees[3]; /* 80059FCC */

/* Nothing reads this pointer; its string opens the unit's small data. */
char *heap_unreferenced_string = " "; /* 800591C8 */

/* Owner tag names, by tag. Strings of up to 8 bytes are small data here: GCC
 * emits a static initializer's strings last to first, into .sdata or .rodata. */
char *heap_tag_names[] = { /* 80050110 */
    "....", "END ", "HIG ", "KAZM", "MASA", "MIYA", "SUGI", "SUZU", "YOSI", "SIMA", "????", "TEST",
};

/* Names of the built-in allocation classes (kind & 0x1f with kind bit 0x20). */
char *heap_class_names[] = { /* 80050140 */
    "",           "FREE AREA",   "fake malloc", "fake calloc", "MDL Data",
    "MDL Packet", "MDL Light",   "CD CHACE",    "MES IMAGE",   "MES WORK",
    "MES CUE",    "MIMe Work",   "MIMe Vertex", "MIMe Normal", "SYMBOL DATA",
    "SOUND",      "MES FONT",    "MES SYSDATA", "LsFONT",      "DelayFree",
};

/* 80031894: Usable size of a block: the distance to the next block's header. */
s32 heap_get_block_size(u8 *data) {
    return HEAP_HEADER(data)->next - data - 8;
}

/* 800318A8: Owner tag of a block. */
s32 heap_get_block_tag(u8 *data) {
    return HEAP_HEADER(data)->tag;
}

/* 800318BC: Address of the call that allocated a block. */
u32 heap_get_block_caller(u8 *data) {
    return HEAP_HEADER(data)->caller * 4 + 0x80000000;
}

/* 800318DC: Whether release refuses a block. */
s32 heap_is_block_protected(u8 *data) {
    return HEAP_HEADER(data)->keep;
}

/* 800318F0: An empty heap entry. */
void heap_empty_entry(void) {
}

/* 800318F8: Load the host symbol file `name` into a heap block and keep it when it
 * starts with "SYM1". Returns -1 when the file cannot be opened. */
s32 heap_load_host_symbols(char *name) {
    s32 chunk = 0x8000;
    s32 fd;
    s32 size;
    u8 *p;

    PCinit();
    fd = PCopen(name, 0, 0);
    if (fd == -1) {
        return -1;
    }
    size = PClseek(fd, 0, 2);
    PClseek(fd, 0, 0);
    heap_set_next_class(0x2E);
    p = heap_alloc(size, 0);
    heap_symbol_data = p;
    heap_symbol_data_end = p + size;
    while (1) {
        if (size <= 0) {
            break;
        }
        if (size < chunk) {
            chunk = size;
        }
        PCread(fd, p, chunk);
        size -= chunk;
        p += chunk;
    }
    PCclose(fd);
    if (heap_symbol_data[0] != 'S' || heap_symbol_data[1] != 'Y' || heap_symbol_data[2] != 'M' ||
        heap_symbol_data[3] != '1') {
        heap_free(heap_symbol_data);
        heap_symbol_data = NULL;
        heap_symbol_data_end = NULL;
    }
    return 0;
}

/* 80031A30: Reset the allocation defaults and forget the symbol data. */
void heap_reset_defaults(void) {
    console_set_external_block(0);
    heap_next_class = 0x20;
    heap_owner_tag = 10;
    heap_symbol_data = NULL;
    heap_symbol_data_end = NULL;
}

/* 80031A68: Make [start, end) one free block followed by the end marker. */
void heap_init(HeapHeader *first, u8 *end) {
    first = (HeapHeader *)((u32)first & ~3);
    end = (u8 *)((u32)end & ~3);
    heap_first_block = (u8 *)(first + 1);
    first->tag = 0;
    first->kind = 0x21;
    first->next = end;
    heap_next_class = 0x20;
    heap_owner_tag = 10;
    heap_needs_coalescing = 0;
    heap_symbol_data = NULL;
    heap_symbol_data_end = NULL;
    HEAP_HEADER(end)->next = end;
    HEAP_HEADER(end)->tag = 1;
    HEAP_HEADER(end)->kind = 0x20;
    heap_delayed_frees[0] = NULL;
    heap_reset_defaults();
}

/* 80031B10: Move the heap start down to `start`: settle pending frees, then make the
 * new first block a free block reaching the old first block's successor. */
void heap_move_start(HeapHeader *first) {
    u8 *next;

    heap_free_all();
    if (heap_needs_coalescing != 0) {
        heap_coalesce();
    }
    first = (HeapHeader *)((u32)first & ~3);
    next = HEAP_HEADER(heap_first_block)->next;
    first->tag = 0;
    first->kind = 0x21;
    first->next = next;
    heap_first_block = (u8 *)(first + 1);
    heap_delayed_frees[0] = NULL;
}

/* 80031B9C: Owner tag given to the next blocks. */
s32 heap_get_owner_tag(void) {
    return heap_owner_tag;
}

/* 80031BA8: Set it. */
void heap_set_owner_tag(s32 tag) {
    heap_owner_tag = tag;
}

/* 80031BB4: Set whether allocation failures return NULL; returns the old setting. */
s32 heap_set_quiet_failures(s32 quiet) {
    s32 previous = heap_quiet_failures;
    heap_quiet_failures = quiet;
    return previous;
}

/* 80031BC4: Report the caller and size of the last request. */
void heap_get_last_request(s32 *caller, s32 *size) {
    *caller = heap_last_request_caller;
    *size = heap_last_request_size;
}

/* 80031BDC: Allocate `size` bytes. Mode 0 takes the first fitting free block, mode 2
 * the smallest fitting one, mode 1 carves from the top of the highest one
 * (or takes the highest exact fit). The block is tagged with the current
 * owner tag and allocation class and records its caller. Exhaustion is fatal
 * unless failures are quiet (then NULL). The allocation class is reset to
 * 0x20 once used. */
void *heap_alloc(s32 size, s32 mode) {
    u32 caller;
    s32 none;
    u32 best;
    u32 available;
    s32 spare;
    u8 *data;
    HeapHeader *header;
    u8 *candidate_data;
    HeapHeader *candidate;
    HeapHeader *exact;
    HeapHeader *rest;

    GET_RA(&caller);
    caller -= 8;
    heap_last_request_caller = caller;
    caller = (caller & 0x1FFFFFF) >> 2;
    if (heap_needs_coalescing != 0) {
        heap_coalesce();
    }
    none = 1;
    heap_last_request_size = size;
    size += 3;
    size &= ~3;
    candidate_data = NULL;
    best = 0x800000;
    candidate = NULL;
    data = heap_first_block;
    exact = NULL;
    header = HEAP_HEADER(data);
    for (;;) {
        if (header->tag != 0) {
            do {
                if (header->tag == 1) {
                    goto end;
                }
                data = header->next;
                header = HEAP_HEADER(data);
            } while (header->tag != 0);
        }
        available = header->next - (u8 *)header - 0x10;
        spare = available - size;
        if (spare == 4 || spare == 0) {
            none = 0;
            if (mode == 1) {
                exact = header;
            } else {
            whole:
                header->tag = heap_owner_tag;
                header->kind = heap_next_class;
                header->keep = 0;
                header->caller = caller;
                heap_next_class = 0x20;
                return header + 1;
            }
        } else if (spare >= 5) {
            none = 0;
            switch (mode) {
            case 2:
                if (available < best) {
                    candidate_data = data;
                    candidate = header;
                    best = available;
                }
                break;
            case 1:
                candidate = header;
                break;
            default:
                candidate_data = data;
                candidate = header;
                goto split;
            }
        }
        data = header->next;
        header = HEAP_HEADER(data);
    }
end:
    if (none) {
        if (heap_quiet_failures != 0) {
            return NULL;
        }
        mode_dispatch(0x82);
    }
    if (mode == 1) {
        if (candidate < exact) {
            header = exact;
            goto whole;
        }
        {
            u8 *block = candidate->next - (size + 8);

            HEAP_HEADER(block)->next = candidate->next;
            HEAP_HEADER(block)->tag = heap_owner_tag;
            HEAP_HEADER(block)->kind = heap_next_class;
            HEAP_HEADER(block)->keep = 0;
            HEAP_HEADER(block)->caller = caller;
            heap_next_class = 0x20;
            candidate->next = block;
            return block;
        }
    }
split:
    rest = (HeapHeader *)(candidate_data + size);
    rest->next = candidate->next;
    rest->tag = candidate->tag;
    rest->kind = candidate->kind;
    rest->keep = candidate->keep;
    rest->caller = candidate->caller;
    candidate->next = (u8 *)(rest + 1);
    candidate->kind = heap_next_class;
    heap_next_class = 0x20;
    candidate->tag = heap_owner_tag;
    candidate->keep = 0;
    candidate->caller = caller;
    return candidate_data;
}

/* 80031F70: Shrink a block to `size` bytes, splitting the rest off as a free block.
 * Returns the block's header, or NULL when the rest would be too small. */
HeapHeader *heap_shrink_block(u8 *data, s32 size) {
    u8 *next = HEAP_HEADER(data)->next;
    HeapHeader *header = HEAP_HEADER(data);
    HeapHeader *rest;

    if ((u32)(next - (u8 *)header - 0x10) <= (u32)(size + 0x10)) {
        return NULL;
    }
    rest = (HeapHeader *)(data + size);
    rest->next = next;
    heap_needs_coalescing = 1;
    rest->tag = 0;
    rest->kind = 0x21;
    rest->caller = 0;
    rest->keep = 0;
    header->next = (u8 *)(rest + 1);
    return header;
}

/* 80031FF8: Merge every run of adjacent free blocks. */
void heap_coalesce(void) {
    HeapHeader *header;

    for (header = HEAP_HEADER(heap_first_block); header->tag != 1;
         header = HEAP_HEADER(header->next)) {
        while (header->tag == 0 && HEAP_HEADER(header->next)->tag == 0) {
            header->next = HEAP_HEADER(header->next)->next;
        }
    }
    heap_needs_coalescing = 0;
}

/* 800320A4: Protect a block from release. */
void heap_protect_block(void *data) {
    HEAP_HEADER(data)->keep = 1;
}

/* 800320B8: Allow a block's release again. */
void heap_unprotect_block(void *data) {
    HEAP_HEADER(data)->keep = 0;
}

/* 800320D0: The same (a second entry). */
void heap_unprotect_block_copy(void *data) {
    HEAP_HEADER(data)->keep = 0;
}

/* 800320E8: Release a block: mark it free for the next coalescing pass. Returns 0, or
 * -1 for a protected block; a NULL block is fatal unless failures are quiet
 * (then 1). */
s32 heap_free(void *data) {
    u32 caller;

    if (data == NULL) {
        if (heap_quiet_failures != 0) {
            return 1;
        }
        GET_RA(&caller);
        heap_last_request_size = 0;
        heap_last_request_caller = caller - 8;
        mode_dispatch(0x83);
    }
    if (HEAP_HEADER(data)->keep) {
        return -1;
    }
    HEAP_HEADER(data)->kind = 0x21;
    HEAP_HEADER(data)->tag = 0;
    HEAP_HEADER(data)->caller = 0;
    heap_needs_coalescing = 1;
    return 0;
}

/* 8003218C: Release every block with owner tag `tag`. */
void heap_free_tag(u8 tag) {
    HeapHeader *header;
    HeapHeader *current;

    for (header = HEAP_HEADER(heap_first_block); header->tag != 1;) {
        if (header->tag == tag) {
            current = header;
            header = HEAP_HEADER(header->next);
            heap_free(current + 1);
        } else {
            header = HEAP_HEADER(header->next);
        }
    }
}

/* 8003223C: Release every block. */
void heap_free_all(void) {
    HeapHeader *header;
    HeapHeader *current;

    for (header = HEAP_HEADER(heap_first_block); header->tag != 1;) {
        current = header;
        header = HEAP_HEADER(current->next);
        heap_free(current + 1);
    }
}

/* 800322B4: Release every block, protected ones included. */
void heap_force_free_all(void) {
    HeapHeader *header;
    HeapHeader *current;

    for (header = HEAP_HEADER(heap_first_block); header->tag != 1;) {
        current = header;
        header = HEAP_HEADER(current->next);
        heap_unprotect_block(current + 1);
        heap_free(current + 1);
    }
}

/* 80032340: Total usable bytes in free blocks. */
s32 heap_get_free_total(void) {
    HeapHeader *header;
    s32 total = 0;

    for (header = HEAP_HEADER(heap_first_block); header->tag != 1; header = HEAP_HEADER(header->next)) {
        if (header->tag == 0) {
            total += header->next - (u8 *)header - 0x10;
        }
    }
    return total;
}

/* 800323B4: Walk the block list (its report output is compiled out). */
s32 heap_walk_blocks(void) {
    HeapHeader *header;

    for (header = HEAP_HEADER(heap_first_block); header->tag != 1; header = HEAP_HEADER(header->next)) {
    }
    return 0;
}

/* 80032404: Usable bytes of the largest free block, less a header. */
u32 heap_get_largest_free(void) {
    HeapHeader *header;
    u32 largest = 0;
    u32 size;

    for (header = HEAP_HEADER(heap_first_block); header->tag != 1; header = HEAP_HEADER(header->next)) {
        if (header->tag == 0) {
            size = header->next - (u8 *)header - 0x10;
            if (largest < size) {
                largest = size;
            }
        }
    }
    if (largest < 8) {
        largest = 8;
    }
    return largest - 8;
}

/* 80032498: Select owner tag `tag` for the next blocks and record the names of its
 * allocation classes, which the heap report prints. */
void heap_select_owner_tag(s32 tag, char **names) {
    heap_owner_tag = tag;
    heap_tag_class_name_tables[tag] = names;
    heap_quiet_failures = 0;
}

/* 800324B8: Allocation class of the next block. */
void heap_set_next_class(s16 kind) {
    heap_next_class = kind;
}

/* 800324C4: Copy into `out` the name of the last symbol below `address` from the
 * loaded symbol data (entries: little-endian address, length, name). */
void heap_find_symbol_name(u32 address, char *out) {
    u8 *p = heap_symbol_data + 4;
    u8 *entry;
    u32 value;
    u32 length;

    if (p != NULL) {
        while (p < heap_symbol_data_end) {
            value = *p++;
            value |= *p++ << 8;
            value |= *p++ << 16;
            value |= *p++ << 24;
            if (value >= address) {
                break;
            }
            entry = p;
            length = *p++;
            p += length;
        }
        length = *entry++;
        while (length-- != 0) {
            *out++ = *entry++;
        }
    }
    *out = 0;
}

/* 80032584: Print one heap-report row: selected addresses, usable size, owner tag,
 * caller address/name and allocation-class contents. A grouped row uses its
 * last header, first data address, and sum of usable sizes. */
void heap_print_report_row(HeapHeader *header, u8 *data, s32 size, s32 flags) {
    char name[64];
    char *contents;

    if (flags & 2) {
        heap_report_printf("%06x ", (void *)((u32)header & 0xFFFFFF));
    }
    if (flags & 4) {
        heap_report_printf("%06x ", (void *)((u32)data & 0xFFFFFF));
    }
    if (flags & 8) {
        heap_report_printf("%6x ", (void *)size);
    }
    if (flags & 0x10) {
        heap_report_printf("%s ", heap_tag_names[header->tag]);
    }
    if (flags & 0x20) {
        heap_report_printf("%06x ", (void *)(header->caller * 4));
    }
    if ((flags & 0x40) && header->tag != 0) {
        heap_find_symbol_name(header->caller * 4 + 0x80000000U, name);
        heap_report_printf("%s", name);
        if ((flags & 0x80) && (header->kind & 0x1F)) {
            heap_report_printf(" / ");
        }
    }
    if ((flags & 0x80) && (header->kind & 0x1F)) {
        if (header->kind & 0x20) {
            contents = heap_class_names[header->kind & 0x1F];
        } else {
            contents = heap_tag_class_name_tables[header->tag][header->kind];
        }
        heap_report_printf("%s", contents);
    }
    heap_report_printf("\n");
}

/* 8003278C: Report the heap, coalescing free blocks for nonzero modes. Mode 2 groups
 * equal owner tags/allocation classes (ignoring keep/caller); mode 3 groups
 * equal callers. Skip/count apply to finished rows; zero count is unlimited.
 * Column flags: 1 number, 2 header, 4 data, 8 size, 0x10 owner, 0x20 caller,
 * 0x40 caller symbol, 0x80 contents, 0x8000 total free bytes. */
void heap_print_report(s32 mode, s32 skip, s32 count, s32 flags) {
    char unused[64]; /* unused in the original; reserves 64 bytes */
    s32 number = 0;
    s32 limited = 0;
    HeapHeader *header;
    u8 *data;
    s32 size;

    if (flags == 0) {
        flags = 0x808D;
    }
    if (mode != 0) {
        heap_coalesce();
    }
    if (count != 0) {
        limited = 1;
    }
    if (heap_symbol_data == NULL) {
        flags &= ~0x40;
    }
    if (flags & 1) {
        heap_report_printf("No- ");
    }
    if (flags & 2) {
        heap_report_printf("MCB--- ");
    }
    if (flags & 4) {
        heap_report_printf("ADDR-- ");
    }
    if (flags & 8) {
        heap_report_printf("SIZE-- ");
    }
    if (flags & 0x10) {
        heap_report_printf("USER ");
    }
    if (flags & 0x20) {
        heap_report_printf("GETADD ");
    }
    if (flags & 0x40) {
        heap_report_printf("FUNCTION/");
    }
    if (flags & 0x80) {
        heap_report_printf("CONTENTS");
    }
    heap_report_printf("\n");
    header = HEAP_HEADER(heap_first_block);
    data = heap_first_block;
    size = 0;
    while (header->tag != 1) {
        size += header->next - (u8 *)header - 0x10;
        if (mode == 2 && header->tag == HEAP_HEADER(header->next)->tag &&
            header->kind == HEAP_HEADER(header->next)->kind) {
            number++;
            header = HEAP_HEADER(header->next);
            continue;
        }
        if (mode == 3 && header->caller == HEAP_HEADER(header->next)->caller) {
            number++;
            header = HEAP_HEADER(header->next);
            continue;
        }
        if (skip != 0) {
            skip--;
        } else {
            if (flags & 1) {
                heap_report_printf("%3d ", (void *)number);
            }
            heap_print_report_row(header, data, size, flags);
            count--;
        }
        if (limited && count == 0) {
            break;
        }
        number++;
        header = HEAP_HEADER(header->next);
        size = 0;
        data = (u8 *)(header + 1);
    }
    if (flags & 1) {
        heap_report_printf("--- ");
    }
    if (flags & 2) {
        heap_report_printf("------ ");
    }
    if (flags & 4) {
        heap_report_printf("------ ");
    }
    if (flags & 8) {
        heap_report_printf("------ ");
    }
    if (flags & 0x10) {
        heap_report_printf("---- ");
    }
    if (flags & 0x8000) {
        heap_report_printf("\nFree %6x", (void *)heap_get_free_total());
    }
    heap_report_printf("\n");
}

/* 80032B0C: Allocate a protected block owned by tag 7 (class 0x2F), from the top. */
void *heap_alloc_sound_block(s32 size) {
    u16 tag = heap_owner_tag;
    void *block;

    heap_owner_tag = 7;
    heap_next_class = 0x2F;
    block = heap_alloc(size, 1);
    heap_protect_block(block);
    heap_owner_tag = tag;
    return block;
}

/* 80032B64: Allocate `count * size` bytes owned by tag 7 (class 0x23). */
void *heap_fake_calloc(s32 count, s32 size) {
    u16 tag = heap_owner_tag;
    void *block;

    heap_owner_tag = 7;
    heap_next_class = 0x23;
    block = heap_alloc(size * count, 0);
    heap_owner_tag = tag;
    return block;
}

/* 80032BAC: Release a block even when it is protected. */
void heap_force_free(void *data) {
    heap_unprotect_block(data);
    heap_free(data);
}

/* The heap report output: the console printf until 80032e04 sends a report
 * to a host file. */
void (*heap_report_output)(char *line) = (void (*)(char *))console_printf; /* 800592B8 */

/* 80032BDC: Format a heap report line and pass it to the report output. */
void heap_report_printf(char *format, void *args) {
    char line[1024];

    sprintf(line, format, args);
    heap_report_output(line);
}

/* 80032C18: Release a block after `frames` more frames (immediately for 0). */
void heap_delay_free(void *data, s32 frames) {
    u32 caller;
    DelayedFree *node;

    if (data == NULL) {
        GET_RA(&caller);
        heap_last_request_size = 0;
        heap_last_request_caller = caller - 8;
        mode_dispatch(0x83);
    }
    if (frames == 0) {
        heap_free(data);
        return;
    }
    heap_set_next_class(0x33);
    node = heap_alloc(sizeof(DelayedFree), 1);
    node->next = heap_delayed_frees[0];
    node->data = data;
    node->frames = frames;
    heap_delayed_frees[0] = node;
}

/* 80032CB8: Count down the delayed releases and release the expired ones. */
void heap_update_delayed_frees(void) {
    DelayedFree **link = &heap_delayed_frees[0];
    DelayedFree *node;

    while ((node = *link) != NULL) {
        if (--node->frames == -1) {
            heap_free(node->data);
            *link = node->next;
            heap_free(node);
            if (*link == NULL) {
                break;
            }
        } else {
            link = &(*link)->next;
        }
    }
}

/* 80032D60: Release every delayed block now. Keep the head alias live across both
 * releases so the list update and next-node test follow the original order. */
void heap_flush_delayed_frees(void) {
    DelayedFree **head = heap_delayed_frees;
    DelayedFree *node;
    DelayedFree *next;

    node = *head;
    while (node != NULL) {
        heap_free(node->data);
        *head = node->next;
        heap_free(node);
        next = *head;
        if (next == NULL) {
            break;
        }
        node = next;
    }
}
