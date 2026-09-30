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
extern void func_8003278C(s32 a, s32 b, s32 c, s32 d);
extern void func_800379C8(char *line);
extern void func_80032DCC(char *line);
extern void func_800320A4(void *data);

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
    D_80059FCC[0] = NULL;
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
    D_80059FCC[0] = NULL;
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

/* Allocate `size` bytes. Mode 0 takes the first fitting free block, mode 2
 * the smallest fitting one, mode 1 carves from the top of the highest one
 * (or takes the highest exact fit). The block is tagged with the current
 * owner tag and allocation class and records its caller. Exhaustion is fatal
 * unless failures are quiet (then NULL).
 * Nonmatching: the original keeps the intermediate header-word stores and
 * allocates registers differently. */
#ifdef NON_MATCHING
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
    u16 kind;

    GET_RA(&caller);
    caller -= 8;
    D_80059340 = caller;
    caller = (caller & 0x1FFFFFF) >> 2;
    if (D_8005932C != 0) {
        func_80031FF8();
    }
    none = 1;
    D_8005933C = size;
    size = (size + 3) & ~3;
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
                kind = D_80059318;
                D_80059318 = 0x20;
                header->tag = D_8005931C;
                header->kind = kind;
                header->keep = 0;
                header->caller = caller;
                return header + 1;
            }
        } else if (spare >= 5) {
            none = 0;
            if (mode == 1) {
                candidate = header;
            } else if (mode == 2) {
                if (available < best) {
                    candidate_data = data;
                    candidate = header;
                    best = available;
                }
            } else {
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
        kind = D_80059318;
        D_80059318 = 0x20;
        data = candidate->next - (size + 8);
        HEAP_HEADER(data)->next = candidate->next;
        HEAP_HEADER(data)->tag = D_8005931C;
        HEAP_HEADER(data)->kind = kind;
        HEAP_HEADER(data)->keep = 0;
        HEAP_HEADER(data)->caller = caller;
        candidate->next = data;
        return data;
    }
split:
    rest = (HeapHeader *)(candidate_data + size);
    kind = D_80059318;
    D_80059318 = 0x20;
    rest->next = candidate->next;
    rest->tag = candidate->tag;
    rest->kind = candidate->kind;
    rest->keep = candidate->keep;
    rest->caller = candidate->caller;
    candidate->next = (u8 *)(rest + 1);
    candidate->kind = kind;
    candidate->tag = D_8005931C;
    candidate->keep = 0;
    candidate->caller = caller;
    return candidate_data;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80031BDC);
#endif

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

/* Format a heap report line and pass it to the report output. */
void func_80032BDC(char *format, void *args) {
    char line[1024];

    func_8003FBF8(line, format, args);
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

/* Release every delayed block now.
 * Nonmatching: the list head is kept in a callee-saved register. */
#ifdef NON_MATCHING
void func_80032D60(void) {
    DelayedFree *node = D_80059FCC[0];

    if (node != NULL) {
        do {
            func_800320E8(node->data);
            D_80059FCC[0] = node->next;
            func_800320E8(node);
            node = D_80059FCC[0];
        } while (node != NULL);
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032D60);
#endif

/* Report output to the host file. */
void func_80032DCC(char *line) {
    func_8004C470(D_80059348, line, func_8003FBC8(line));
}

/* Write the full heap report to the host file `name`.
 * Nonmatching: the original stores the output hook with lui/sw, not $gp. */
#ifdef NON_MATCHING
void func_80032E04(char *name) {
    func_8004C38C();
    D_80059348 = func_8004C36C(name, 0);
    D_800592B8 = func_80032DCC;
    func_8003278C(1, 0, 0, -1);
    D_800592B8 = func_800379C8;
    func_8004C338(D_80059348);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/heap", func_80032E04);
#endif
