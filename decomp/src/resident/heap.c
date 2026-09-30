/* Resident game heap: block headers, allocation, release and the heap
 * report. Compiled with -G8: its small globals are $gp-relative. */
#include "common.h"

#include "heap.h"

extern void func_8003747C(s32 arg);
extern void func_800324B8(s16 kind);
extern void func_800320B8(void *data);
extern void func_80031A30(void);
extern void func_80031FF8(void);
extern void func_8003223C(void);

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
    fd = func_8004C318(name, 0, 0);
    if (fd == -1) {
        return -1;
    }
    size = func_8004C348(fd, 0, 2);
    func_8004C348(fd, 0, 0);
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
    func_8004C338(fd);
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

/* Make [start, end) one free block followed by the end marker.
 * Nonmatching: register allocation of the header update. */
#ifdef NON_MATCHING
void func_80031A68(HeapHeader *first, u8 *end) {
    first = (HeapHeader *)((u32)first & ~3);
    end = (u8 *)((u32)end & ~3);
    first->tag = 0;
    first->kind = 0x21;
    D_80059320 = (u8 *)(first + 1);
    D_80059318 = 0x20;
    first->next = end;
    D_8005932C = 0;
    D_80059334 = NULL;
    D_80059338 = NULL;
    D_8005931C = 10;
    HEAP_HEADER(end)->next = end;
    HEAP_HEADER(end)->tag = 1;
    HEAP_HEADER(end)->kind = 0x20;
    D_80059FA4[10] = 0;
    func_80031A30();
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80031A68);
#endif

/* Move the heap start down to `start`: settle pending frees, then make the
 * new first block a free block reaching the old first block's successor.
 * Nonmatching: register allocation of the header update. */
#ifdef NON_MATCHING
void func_80031B10(HeapHeader *first) {
    func_8003223C();
    if (D_8005932C != 0) {
        func_80031FF8();
    }
    first = (HeapHeader *)((u32)first & ~3);
    first->tag = 0;
    first->kind = 0x21;
    first->next = HEAP_HEADER(D_80059320)->next;
    D_80059320 = (u8 *)(first + 1);
    D_80059FA4[10] = 0;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80031B10);
#endif

/* Owner tag given to the next blocks. */
u16 func_80031B9C(void) {
    return D_8005931C;
}

void func_80031BA8(s16 tag) {
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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80031BDC);

/* Shrink a block to `size` bytes, splitting the rest off as a free block.
 * Returns the block's header, or NULL when the rest would be too small.
 * Nonmatching: two independent instructions are scheduled in swapped order. */
#ifdef NON_MATCHING
HeapHeader *func_80031F70(u8 *data, s32 size) {
    u8 *next = HEAP_HEADER(data)->next;
    HeapHeader *header = HEAP_HEADER(data);
    HeapHeader *rest;

    if ((u32)(next - (u8 *)header - 0x10) <= (u32)(size + 0x10)) {
        return NULL;
    }
    rest = (HeapHeader *)(data + size);
    D_8005932C = 1;
    rest->next = next;
    rest->tag = 0;
    rest->kind = 0x21;
    rest->caller = 0;
    rest->keep = 0;
    header->next = (u8 *)(rest + 1);
    return header;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80031F70);
#endif

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032584);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/heap", D_80018998);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_8003278C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032B0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032B64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032BAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032BDC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032C18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032CB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032D60);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032DCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032E04);
