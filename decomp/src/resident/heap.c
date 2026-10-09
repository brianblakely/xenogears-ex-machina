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
static u16 D_80059318;     /* allocation class of the next block */
static u16 D_8005931C;     /* owner tag of the next block */
static u8 *D_80059320;     /* data address of the first block */
static s32 D_80059324;     /* unreferenced */
static s32 D_80059328;     /* unreferenced */
static s32 D_8005932C;     /* free blocks await coalescing */
static s32 D_80059330;     /* failures return NULL instead of stopping */
/* Loaded host symbols: the complete "SYM1" file and its byte limit. They
 * are separate scalars: func_80031A68 clears them ahead of its block-header
 * stores, which GCC's alias rules allow for scalars but not for members of a
 * global struct. */
static u8 *D_80059334;
static u8 *D_80059338;
static s32 D_8005933C;     /* size of the last request */
static s32 D_80059340;     /* caller of the last request */
static s32 D_80059344;     /* unreferenced */
static s32 D_80059FA4[10]; /* per-tag words */
/* The delayed releases: one list head, but not small data. */
static DelayedFree *D_80059FCC[3];

/* Nothing reads this pointer; its string opens the unit's small data. */
char *D_800591C8 = " ";

/* Owner tag names, by tag. Strings of up to 8 bytes are small data here: GCC
 * emits a static initializer's strings last to first, into .sdata or .rodata. */
char *D_80050110[] = {
    "....", "END ", "HIG ", "KAZM", "MASA", "MIYA", "SUGI", "SUZU", "YOSI", "SIMA", "????", "TEST",
};

/* Names of the built-in allocation classes (kind & 0x1f with kind bit 0x20). */
char *D_80050140[] = {
    "",           "FREE AREA",   "fake malloc", "fake calloc", "MDL Data",
    "MDL Packet", "MDL Light",   "CD CHACE",    "MES IMAGE",   "MES WORK",
    "MES CUE",    "MIMe Work",   "MIMe Vertex", "MIMe Normal", "SYMBOL DATA",
    "SOUND",      "MES FONT",    "MES SYSDATA", "LsFONT",      "DelayFree",
};

/* Usable size of a block: the distance to the next block's header. */
s32 func_80031894(u8 *data) {
    return HEAP_HEADER(data)->next - data - 8;
}

/* Owner tag of a block. */
s32 func_800318A8(u8 *data) {
    return HEAP_HEADER(data)->tag;
}

/* Address of the call that allocated a block. */
u32 func_800318BC(u8 *data) {
    return HEAP_HEADER(data)->caller * 4 + 0x80000000;
}

/* Whether release refuses a block. */
s32 func_800318DC(u8 *data) {
    return HEAP_HEADER(data)->keep;
}

void func_800318F0(void) {
}

/* Load the host symbol file `name` into a heap block and keep it when it
 * starts with "SYM1". Returns -1 when the file cannot be opened. */
s32 func_800318F8(char *name) {
    s32 chunk = 0x8000;
    s32 fd;
    s32 size;
    u8 *p;

    func_8004C38C();
    fd = PCopen(name, 0, 0);
    if (fd == -1) {
        return -1;
    }
    size = PClseek(fd, 0, 2);
    PClseek(fd, 0, 0);
    func_800324B8(0x2E);
    p = func_80031BDC(size, 0);
    D_80059334 = p;
    D_80059338 = p + size;
    while (1) {
        if (size <= 0) {
            break;
        }
        if (size < chunk) {
            chunk = size;
        }
        func_8004C398(fd, p, chunk);
        size -= chunk;
        p += chunk;
    }
    PCclose(fd);
    if (D_80059334[0] != 'S' || D_80059334[1] != 'Y' || D_80059334[2] != 'M' ||
        D_80059334[3] != '1') {
        func_800320E8(D_80059334);
        D_80059334 = NULL;
        D_80059338 = NULL;
    }
    return 0;
}

/* Reset the allocation defaults and forget the symbol data. */
void func_80031A30(void) {
    func_8003747C(0);
    D_80059318 = 0x20;
    D_8005931C = 10;
    D_80059334 = NULL;
    D_80059338 = NULL;
}

/* Make [start, end) one free block followed by the end marker. */
void func_80031A68(HeapHeader *first, u8 *end) {
    first = (HeapHeader *)((u32)first & ~3);
    end = (u8 *)((u32)end & ~3);
    D_80059320 = (u8 *)(first + 1);
    first->tag = 0;
    first->kind = 0x21;
    first->next = end;
    D_80059318 = 0x20;
    D_8005931C = 10;
    D_8005932C = 0;
    D_80059334 = NULL;
    D_80059338 = NULL;
    HEAP_HEADER(end)->next = end;
    HEAP_HEADER(end)->tag = 1;
    HEAP_HEADER(end)->kind = 0x20;
    D_80059FCC[0] = NULL;
    func_80031A30();
}

/* Move the heap start down to `start`: settle pending frees, then make the
 * new first block a free block reaching the old first block's successor. */
void func_80031B10(HeapHeader *first) {
    u8 *next;

    func_8003223C();
    if (D_8005932C != 0) {
        func_80031FF8();
    }
    first = (HeapHeader *)((u32)first & ~3);
    next = HEAP_HEADER(D_80059320)->next;
    first->tag = 0;
    first->kind = 0x21;
    first->next = next;
    D_80059320 = (u8 *)(first + 1);
    D_80059FCC[0] = NULL;
}

/* Owner tag given to the next blocks. */
s32 func_80031B9C(void) {
    return D_8005931C;
}

void func_80031BA8(s32 tag) {
    D_8005931C = tag;
}

/* Set whether allocation failures return NULL; returns the old setting. */
s32 func_80031BB4(s32 quiet) {
    s32 previous = D_80059330;
    D_80059330 = quiet;
    return previous;
}

/* Report the caller and size of the last request. */
void func_80031BC4(s32 *caller, s32 *size) {
    *caller = D_80059340;
    *size = D_8005933C;
}

/* Allocate `size` bytes. Mode 0 takes the first fitting free block, mode 2
 * the smallest fitting one, mode 1 carves from the top of the highest one
 * (or takes the highest exact fit). The block is tagged with the current
 * owner tag and allocation class and records its caller. Exhaustion is fatal
 * unless failures are quiet (then NULL). The allocation class is reset to
 * 0x20 once used. */
void *func_80031BDC(s32 size, s32 mode) {
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
    D_80059340 = caller;
    caller = (caller & 0x1FFFFFF) >> 2;
    if (D_8005932C != 0) {
        func_80031FF8();
    }
    none = 1;
    D_8005933C = size;
    size += 3;
    size &= ~3;
    candidate_data = NULL;
    best = 0x800000;
    candidate = NULL;
    data = D_80059320;
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
                header->tag = D_8005931C;
                header->kind = D_80059318;
                header->keep = 0;
                header->caller = caller;
                D_80059318 = 0x20;
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
        if (D_80059330 != 0) {
            return NULL;
        }
        func_80019ACC(0x82);
    }
    if (mode == 1) {
        if (candidate < exact) {
            header = exact;
            goto whole;
        }
        {
            u8 *block = candidate->next - (size + 8);

            HEAP_HEADER(block)->next = candidate->next;
            HEAP_HEADER(block)->tag = D_8005931C;
            HEAP_HEADER(block)->kind = D_80059318;
            HEAP_HEADER(block)->keep = 0;
            HEAP_HEADER(block)->caller = caller;
            D_80059318 = 0x20;
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
    candidate->kind = D_80059318;
    D_80059318 = 0x20;
    candidate->tag = D_8005931C;
    candidate->keep = 0;
    candidate->caller = caller;
    return candidate_data;
}

/* Shrink a block to `size` bytes, splitting the rest off as a free block.
 * Returns the block's header, or NULL when the rest would be too small. */
HeapHeader *func_80031F70(u8 *data, s32 size) {
    u8 *next = HEAP_HEADER(data)->next;
    HeapHeader *header = HEAP_HEADER(data);
    HeapHeader *rest;

    if ((u32)(next - (u8 *)header - 0x10) <= (u32)(size + 0x10)) {
        return NULL;
    }
    rest = (HeapHeader *)(data + size);
    rest->next = next;
    D_8005932C = 1;
    rest->tag = 0;
    rest->kind = 0x21;
    rest->caller = 0;
    rest->keep = 0;
    header->next = (u8 *)(rest + 1);
    return header;
}

/* Merge every run of adjacent free blocks. */
void func_80031FF8(void) {
    HeapHeader *header;

    for (header = HEAP_HEADER(D_80059320); header->tag != 1;
         header = HEAP_HEADER(header->next)) {
        while (header->tag == 0 && HEAP_HEADER(header->next)->tag == 0) {
            header->next = HEAP_HEADER(header->next)->next;
        }
    }
    D_8005932C = 0;
}

/* Protect a block from release. */
void func_800320A4(void *data) {
    HEAP_HEADER(data)->keep = 1;
}

/* Allow a block's release again. */
void func_800320B8(void *data) {
    HEAP_HEADER(data)->keep = 0;
}

void func_800320D0(void *data) {
    HEAP_HEADER(data)->keep = 0;
}

/* Release a block: mark it free for the next coalescing pass. Returns 0, or
 * -1 for a protected block; a NULL block is fatal unless failures are quiet
 * (then 1). */
s32 func_800320E8(void *data) {
    u32 caller;

    if (data == NULL) {
        if (D_80059330 != 0) {
            return 1;
        }
        GET_RA(&caller);
        D_8005933C = 0;
        D_80059340 = caller - 8;
        func_80019ACC(0x83);
    }
    if (HEAP_HEADER(data)->keep) {
        return -1;
    }
    HEAP_HEADER(data)->kind = 0x21;
    HEAP_HEADER(data)->tag = 0;
    HEAP_HEADER(data)->caller = 0;
    D_8005932C = 1;
    return 0;
}

/* Release every block with owner tag `tag`. */
void func_8003218C(u8 tag) {
    HeapHeader *header;
    HeapHeader *current;

    for (header = HEAP_HEADER(D_80059320); header->tag != 1;) {
        if (header->tag == tag) {
            current = header;
            header = HEAP_HEADER(header->next);
            func_800320E8(current + 1);
        } else {
            header = HEAP_HEADER(header->next);
        }
    }
}

/* Release every block. */
void func_8003223C(void) {
    HeapHeader *header;
    HeapHeader *current;

    for (header = HEAP_HEADER(D_80059320); header->tag != 1;) {
        current = header;
        header = HEAP_HEADER(current->next);
        func_800320E8(current + 1);
    }
}

/* Release every block, protected ones included. */
void func_800322B4(void) {
    HeapHeader *header;
    HeapHeader *current;

    for (header = HEAP_HEADER(D_80059320); header->tag != 1;) {
        current = header;
        header = HEAP_HEADER(current->next);
        func_800320B8(current + 1);
        func_800320E8(current + 1);
    }
}

/* Total usable bytes in free blocks. */
s32 func_80032340(void) {
    HeapHeader *header;
    s32 total = 0;

    for (header = HEAP_HEADER(D_80059320); header->tag != 1; header = HEAP_HEADER(header->next)) {
        if (header->tag == 0) {
            total += header->next - (u8 *)header - 0x10;
        }
    }
    return total;
}

/* Walk the block list (its report output is compiled out). */
s32 func_800323B4(void) {
    HeapHeader *header;

    for (header = HEAP_HEADER(D_80059320); header->tag != 1; header = HEAP_HEADER(header->next)) {
    }
    return 0;
}

/* Usable bytes of the largest free block, less a header. */
u32 func_80032404(void) {
    HeapHeader *header;
    u32 largest = 0;
    u32 size;

    for (header = HEAP_HEADER(D_80059320); header->tag != 1; header = HEAP_HEADER(header->next)) {
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

/* Select owner tag `tag` for the next blocks and record `value` for it. */
void func_80032498(s32 tag, s32 value) {
    D_8005931C = tag;
    D_80059FA4[tag] = value;
    D_80059330 = 0;
}

/* Allocation class of the next block. */
void func_800324B8(s16 kind) {
    D_80059318 = kind;
}

/* Copy into `out` the name of the last symbol below `address` from the
 * loaded symbol data (entries: little-endian address, length, name). */
void func_800324C4(u32 address, char *out) {
    u8 *p = D_80059334 + 4;
    u8 *entry;
    u32 value;
    u32 length;

    if (p != NULL) {
        while (p < D_80059338) {
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

/* Print one heap-report row: selected addresses, usable size, owner tag,
 * caller address/name and allocation-class contents. A grouped row uses its
 * last header, first data address, and sum of usable sizes. */
void func_80032584(HeapHeader *header, u8 *data, s32 size, s32 flags) {
    char name[64];
    char *contents;

    if (flags & 2) {
        func_80032BDC("%06x ", (void *)((u32)header & 0xFFFFFF));
    }
    if (flags & 4) {
        func_80032BDC("%06x ", (void *)((u32)data & 0xFFFFFF));
    }
    if (flags & 8) {
        func_80032BDC("%6x ", (void *)size);
    }
    if (flags & 0x10) {
        func_80032BDC("%s ", D_80050110[header->tag]);
    }
    if (flags & 0x20) {
        func_80032BDC("%06x ", (void *)(header->caller * 4));
    }
    if ((flags & 0x40) && header->tag != 0) {
        func_800324C4(header->caller * 4 + 0x80000000U, name);
        func_80032BDC("%s", name);
        if ((flags & 0x80) && (header->kind & 0x1F)) {
            func_80032BDC(" / ");
        }
    }
    if ((flags & 0x80) && (header->kind & 0x1F)) {
        if (header->kind & 0x20) {
            contents = D_80050140[header->kind & 0x1F];
        } else {
            contents = ((char **)D_80059FA4[header->tag])[header->kind];
        }
        func_80032BDC("%s", contents);
    }
    func_80032BDC("\n");
}

/* Report the heap, coalescing free blocks for nonzero modes. Mode 2 groups
 * equal owner tags/allocation classes (ignoring keep/caller); mode 3 groups
 * equal callers. Skip/count apply to finished rows; zero count is unlimited.
 * Column flags: 1 number, 2 header, 4 data, 8 size, 0x10 owner, 0x20 caller,
 * 0x40 caller symbol, 0x80 contents, 0x8000 total free bytes. */
void func_8003278C(s32 mode, s32 skip, s32 count, s32 flags) {
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
        func_80031FF8();
    }
    if (count != 0) {
        limited = 1;
    }
    if (D_80059334 == NULL) {
        flags &= ~0x40;
    }
    if (flags & 1) {
        func_80032BDC("No- ");
    }
    if (flags & 2) {
        func_80032BDC("MCB--- ");
    }
    if (flags & 4) {
        func_80032BDC("ADDR-- ");
    }
    if (flags & 8) {
        func_80032BDC("SIZE-- ");
    }
    if (flags & 0x10) {
        func_80032BDC("USER ");
    }
    if (flags & 0x20) {
        func_80032BDC("GETADD ");
    }
    if (flags & 0x40) {
        func_80032BDC("FUNCTION/");
    }
    if (flags & 0x80) {
        func_80032BDC("CONTENTS");
    }
    func_80032BDC("\n");
    header = HEAP_HEADER(D_80059320);
    data = D_80059320;
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
                func_80032BDC("%3d ", (void *)number);
            }
            func_80032584(header, data, size, flags);
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
        func_80032BDC("--- ");
    }
    if (flags & 2) {
        func_80032BDC("------ ");
    }
    if (flags & 4) {
        func_80032BDC("------ ");
    }
    if (flags & 8) {
        func_80032BDC("------ ");
    }
    if (flags & 0x10) {
        func_80032BDC("---- ");
    }
    if (flags & 0x8000) {
        func_80032BDC("\nFree %6x", (void *)func_80032340());
    }
    func_80032BDC("\n");
}

/* Allocate a protected block owned by tag 7 (class 0x2F), from the top. */
void *func_80032B0C(s32 size) {
    u16 tag = D_8005931C;
    void *block;

    D_8005931C = 7;
    D_80059318 = 0x2F;
    block = func_80031BDC(size, 1);
    func_800320A4(block);
    D_8005931C = tag;
    return block;
}

/* Allocate `count * size` bytes owned by tag 7 (class 0x23). */
void *func_80032B64(s32 count, s32 size) {
    u16 tag = D_8005931C;
    void *block;

    D_8005931C = 7;
    D_80059318 = 0x23;
    block = func_80031BDC(size * count, 0);
    D_8005931C = tag;
    return block;
}

/* Release a block even when it is protected. */
void func_80032BAC(void *data) {
    func_800320B8(data);
    func_800320E8(data);
}

/* The heap report output: the console printf until 80032e04 sends a report
 * to a host file. */
void (*D_800592B8)(char *line) = (void (*)(char *))func_8003700C;

/* Format a heap report line and pass it to the report output. */
void func_80032BDC(char *format, void *args) {
    char line[1024];

    sprintf(line, format, args);
    D_800592B8(line);
}

/* Release a block after `frames` more frames (immediately for 0). */
void func_80032C18(void *data, s32 frames) {
    u32 caller;
    DelayedFree *node;

    if (data == NULL) {
        GET_RA(&caller);
        D_8005933C = 0;
        D_80059340 = caller - 8;
        func_80019ACC(0x83);
    }
    if (frames == 0) {
        func_800320E8(data);
        return;
    }
    func_800324B8(0x33);
    node = func_80031BDC(sizeof(DelayedFree), 1);
    node->next = D_80059FCC[0];
    node->data = data;
    node->frames = frames;
    D_80059FCC[0] = node;
}

/* Count down the delayed releases and release the expired ones. */
void func_80032CB8(void) {
    DelayedFree **link = &D_80059FCC[0];
    DelayedFree *node;

    while ((node = *link) != NULL) {
        if (--node->frames == -1) {
            func_800320E8(node->data);
            *link = node->next;
            func_800320E8(node);
            if (*link == NULL) {
                break;
            }
        } else {
            link = &(*link)->next;
        }
    }
}

/* Release every delayed block now. Keep the head alias live across both
 * releases so the list update and next-node test follow the original order. */
void func_80032D60(void) {
    DelayedFree **head = D_80059FCC;
    DelayedFree *node;
    DelayedFree *next;

    node = *head;
    while (node != NULL) {
        func_800320E8(node->data);
        *head = node->next;
        func_800320E8(node);
        next = *head;
        if (next == NULL) {
            break;
        }
        node = next;
    }
}
