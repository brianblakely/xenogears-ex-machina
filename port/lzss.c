/*
 * The resident LZSS decoder and its allocate-and-decode entry
 * (decomp/src/resident/text_unpack_lzss.s, text_unpack_lzss_alloc.s) as
 * portable C; tools/analysis/packed.py is the format.
 *
 * It reads and writes game memory in the original's order: the flag byte that
 * opens a group is read before the end test, so the final flag read lies past
 * a stream that ends exactly at the end of its last group (docs/later-phases.md,
 * "Decompressors read past files"); the end is tested only between groups and
 * only for equality, so a stream that overshoots goes on decoding; references
 * copy a byte at a time and may overlap their own output (a zero distance
 * copies the output position onto itself). The decoder trusts its input like
 * the original: the game's packed data ends exactly at a group boundary.
 */
#include "common.h"
#include "xem/memory.h"

#define XEM_POINTER(p) ((u32)(XemUintptr)(p))

/* The flag byte, read even when the end test that follows returns. */
#define XEM_FLAG_BYTE(address) (*(volatile u8 *)(XemUintptr)xem_address(address))

void *heap_alloc(s32 size, s32 mode);

/* Decode `source` (its first word the unpacked size) into `destination`;
 * returns `destination`. */
void *text_unpack_lzss(void *source, void *destination) {
    u32 in = XEM_POINTER(source);
    u32 out = XEM_POINTER(destination);
    u32 end = out + XEM_U32(in);
    u32 flags;
    u32 low;
    u32 high;
    u32 from;
    u32 stop;
    s32 tokens;

    in += 4;
    for (;;) {
        flags = XEM_FLAG_BYTE(in);
        in++;
        if (out == end) {
            return destination;
        }
        for (tokens = 8; tokens != 0; tokens--) {
            low = XEM_U8(in);
            in++;
            if (!(flags & 1)) {
                XEM_U8(out) = (u8)low;
                out++;
            } else {
                high = XEM_U8(in);
                in++;
                from = out - (low | (high & 0xF) << 8);
                stop = from + (high >> 4) + 3;
                do {
                    XEM_U8(out) = XEM_U8(from);
                    from++;
                    out++;
                } while (from != stop);
            }
            flags >>= 1;
        }
    }
}

/* Allocate the unpacked size (the first word of `data`) with
 * heap_alloc(size, mode) and decode into the block; returns the block, or
 * NULL without decoding when the allocation fails. */
void *text_unpack_lzss_alloc(void *data, s32 mode) {
    void *block = heap_alloc(XEM_U32(XEM_POINTER(data)), mode);

    if (block == NULL) {
        return NULL;
    }
    return text_unpack_lzss(data, block);
}
