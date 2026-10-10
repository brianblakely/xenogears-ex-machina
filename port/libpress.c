/*
 * libpress (PsyQ): the MDEC through its host device (xem/mdec.h,
 * runtime/crates/xem-core/src/devices/mdec.rs) and the software VLC decoder
 * that turns a movie frame's bitstream into the run-level halfwords the MDEC
 * takes.
 *
 * The library's data keeps its original addresses in the movie library's
 * image (decomp/src/mdec): the MDEC command blocks (quantization tables
 * after 0x40000001, the IDCT matrix after 0x60000000), DecDCTvlc's lookup
 * tables, its size limit and its saved state, so DecDCTGetEnv/PutEnv edit
 * the very tables the reset loads and an interrupted decode resumes from
 * the same nine words.
 *
 * DecDCTvlc follows the handwritten original instruction by instruction
 * (bit buffer, refill, the DC predictors, the stores and their order, the
 * per-block limit check and the end padding), so the run-level stream is
 * the original's. DecDCTvlc2, DecDCTvlcSize2, DecDCTvlcBuild and EncSPU are
 * not here: nothing in the game calls them.
 */
#include "common.h"
#include "psyq/libpress.h"
#include "xem/port.h"
#include "xem/mdec.h"

void *DMACallback(int channel, void (*func)(void));
int ResetCallback(void);

extern u32 libpress_set_iq_command[];
extern u32 libpress_default_iq_luma[16];
extern u32 libpress_default_iq_chroma[16];
extern u32 libpress_set_scale_command[];
extern u32 libpress_dct_matrix[32];
extern u8 libpress_vlc_ac_table[];
extern u8 libpress_vlc_table_end[];
extern s32 libpress_vlc_max_size;
extern u32 libpress_vlc_saved_state[9];

/* The original spins 0x100000 times on the hardware; each turn here waits
 * for the next interrupt. */
#define LIBPRESS_SYNC_TRIES 0x100000

/* A sync that never ended: stop both DMAs and reset the MDEC. */
static s32 timeout(char *name) {
    xem_host_mdec_control(0x80000000);
    xem_host_mdec_stop_dma();
    xem_host_mdec_control(0x60000000);
    return 0;
}

/* Wait until the MDEC has taken the previous command's data. */
s32 MDEC_in_sync(void) {
    s32 tries = LIBPRESS_SYNC_TRIES;

    while (xem_host_mdec_status() & 0x20000000) {
        if (--tries == -1) {
            timeout("MDEC_in_sync");
            return -1;
        }
        xem_host_yield(XEM_YIELD_POLL);
    }
    return 0;
}

/* Wait until the output DMA ended. */
s32 MDEC_out_sync(void) {
    s32 tries = LIBPRESS_SYNC_TRIES;

    while (xem_host_mdec_out_busy()) {
        if (--tries == -1) {
            timeout("MDEC_out_sync");
            return -1;
        }
        xem_host_yield(XEM_YIELD_POLL);
    }
    return 0;
}

/* A command: its first word, then (size / 32) blocks of 32 words by DMA0. */
void MDEC_in(u32 *addr, s32 size) {
    MDEC_in_sync();
    xem_host_mdec_in(addr, size);
}

/* Receive (size / 32) blocks of 32 words by DMA1; its end interrupts. */
void MDEC_out(u32 *addr, s32 size) {
    MDEC_out_sync();
    xem_host_mdec_out(addr, size);
}

u32 MDEC_status(void) {
    return xem_host_mdec_status();
}

/* 0: reset the MDEC and its DMAs and load the default tables; 1: reset only. */
void MDEC_reset(s32 mode) {
    switch (mode) {
    case 0:
        xem_host_mdec_control(0x80000000);
        xem_host_mdec_stop_dma();
        xem_host_mdec_control(0x60000000);
        MDEC_in(libpress_set_iq_command, 0x20);
        MDEC_in(libpress_set_scale_command, 0x20);
        break;
    case 1:
        xem_host_mdec_control(0x80000000);
        xem_host_mdec_stop_dma();
        xem_host_mdec_control(0x60000000);
        break;
    }
}

void DecDCTReset(int mode) {
    if (mode == 0) {
        ResetCallback();
    }
    MDEC_reset(mode);
}

/* The environment: both quantization tables and the IDCT matrix. */
u32 *DecDCTGetEnv(u32 *env) {
    s32 i;

    for (i = 0; i < 16; i++) {
        env[i] = libpress_default_iq_luma[i];
        env[16 + i] = libpress_default_iq_chroma[i];
    }
    for (i = 0; i < 32; i++) {
        env[32 + i] = libpress_dct_matrix[i];
    }
    return env;
}

/* Load the quantization tables of `env` (the matrix stays the default). */
u32 *DecDCTPutEnv(u32 *env) {
    s32 i;

    for (i = 0; i < 16; i++) {
        libpress_default_iq_luma[i] = env[i];
        libpress_default_iq_chroma[i] = env[16 + i];
    }
    MDEC_in(libpress_set_iq_command, 0x20);
    MDEC_in(libpress_set_scale_command, 0x20);
    return env;
}

int DecDCTBufSize(u_long *bs) {
    return *(u16 *)bs;
}

/* Decode a run-level buffer: mode bit 0 24-bit output (else 15-bit), bit 1
 * the 15-bit mask bit. */
void DecDCTin(u_long *buf, int mode) {
    if (mode & 1) {
        buf[0] &= ~0x08000000;
    } else {
        buf[0] |= 0x08000000;
    }
    if (mode & 2) {
        buf[0] |= 0x02000000;
    } else {
        buf[0] &= ~0x02000000;
    }
    MDEC_in((u32 *)buf, *(u16 *)buf);
}

void DecDCTout(u_long *buf, int size) {
    MDEC_out((u32 *)buf, size);
}

/* 0: wait; else whether the MDEC is busy. */
int DecDCTinSync(int mode) {
    if (mode == 0) {
        return MDEC_in_sync();
    }
    return (MDEC_status() >> 29) & 1;
}

int DecDCToutSync(int mode) {
    if (mode == 0) {
        return MDEC_out_sync();
    }
    return (MDEC_status() >> 24) & 1;
}

int DecDCTinCallback(void (*func)()) {
    return (int)DMACallback(0, (void (*)(void))func);
}

int DecDCToutCallback(void (*func)()) {
    return (int)DMACallback(1, (void (*)(void))func);
}

/* Halfwords one DecDCTvlc call writes at most (per block checked), as
 * twice `size`; 0 or 1 lifts the limit. Returns the previous setting. */
int DecDCTvlcSize(int size) {
    s32 previous = libpress_vlc_max_size;

    libpress_vlc_max_size = size - 1 > 0 ? size << 1 : 0xFFFFFF;
    return previous;
}

#define SLL(x, n) ((u32)(x) << ((n) & 31))
#define SRL(x, n) ((u32)(x) >> ((n) & 31))

/* Decode a BS frame (`bs`, version 2 or 3) into run-level halfwords at
 * `buf`, or with `bs` NULL continue the decode the last call stopped.
 * Returns 1 when it stopped at the size limit, 0 at the end of the frame. */
int DecDCTvlc(u_long *bs, u_long *buf) {
    u8 *ac_table = libpress_vlc_ac_table;
    u8 *long_table = libpress_vlc_table_end;
    u16 *in;    /* a0 */
    u8 *out;    /* a1: the halfword written last */
    u32 bits;   /* v0: the next 32 bits */
    u32 used;   /* v1: bits of the halfword in front taken */
    u32 quant;  /* t4: quantization << 10 */
    s32 block;  /* t5: 0 for version 2, else the block (1 Cr, 2 Cb, 3-6 Y) */
    s32 dc_cr;  /* t7 */
    s32 dc_cb;  /* t8 */
    s32 dc_y;   /* t9 */
    u8 *limit;  /* t6 */
    u32 entry, second, length, size, value;
    s32 dc;
    u8 *dc_table;
    s32 i;

    if (bs == NULL) {
        in = (u16 *)libpress_vlc_saved_state[0];
        out = (u8 *)libpress_vlc_saved_state[1];
        bits = libpress_vlc_saved_state[2];
        used = libpress_vlc_saved_state[3];
        quant = libpress_vlc_saved_state[4];
        block = libpress_vlc_saved_state[5];
        dc_cr = libpress_vlc_saved_state[6];
        dc_cb = libpress_vlc_saved_state[7];
        dc_y = libpress_vlc_saved_state[8];
        limit = out + libpress_vlc_max_size * 2;
        goto ac;
    }
    block = 0;
    dc_cr = 0;
    dc_cb = 0;
    dc_y = 0;
    out = (u8 *)buf;
    limit = out + libpress_vlc_max_size * 2;
    in = (u16 *)bs;
    if ((s32)in[3] - 3 >= 0) {
        block = 1;
    }
    quant = (u32)in[2] << 10;
    bits = (u32)in[4] << 16 | in[5];
    used = 0;
    ((u16 *)out)[0] = in[0];
    ((u16 *)out)[1] = in[1];
    in += 6;
    out += 2;

dc:
    value = SRL(bits, 22);
    if (block != 0) {
        out += 2;
        if (value == 0x3FF) {
            goto end;
        }
        dc_table = block - 3 < 0 ? ac_table - 0x400 : ac_table - 0x800;
        entry = (u32)(SRL(bits, 24) << 2);
        length = *(u16 *)(dc_table + entry);
        size = *(u16 *)(dc_table + entry + 2);
        dc = 0;
        if (size != 0) {
            u32 sign = SLL(bits, length);

            bits = sign;
            dc = SRL(bits, 32 - size);
            bits = SLL(bits, size);
            if ((s32)sign >= 0) {
                dc -= SRL(0xFFFFFFFF, 32 - size);
            }
            used += size;
        } else {
            bits = SLL(bits, length);
        }
        used += length;
        if (used & 0x10) {
            used &= 0xF;
            bits |= SLL(*in++, used);
        } else {
            used &= 0xF;
        }
        if (block - 2 > 0) {
            dc_y += dc;
            value = dc_y;
        } else if (block - 2 == 0) {
            dc_cb += dc;
            value = dc_cb;
        } else {
            dc_cr += dc;
            value = dc_cr;
        }
        value = ((value << 2) & 0x3FF) | quant;
        block++;
        *(u16 *)out = value;
        if (block - 7 == 0) {
            block -= 6;
        }
    } else {
        out += 2;
        if (value == 0x1FF) {
            goto end;
        }
        bits = SLL(bits, 10);
        used += 10;
        if (used & 0x10) {
            used &= 0xF;
            bits |= SLL(*in++, used);
        } else {
            used &= 0xF;
        }
        *(u16 *)out = quant | value;
    }
    /* The limit is checked once per block. */
    if ((s32)(out - limit) >= 0) {
        out += 2;
        libpress_vlc_saved_state[0] = (u32)in;
        libpress_vlc_saved_state[1] = (u32)out;
        libpress_vlc_saved_state[2] = bits;
        libpress_vlc_saved_state[3] = used;
        libpress_vlc_saved_state[4] = quant;
        libpress_vlc_saved_state[5] = block;
        libpress_vlc_saved_state[6] = dc_cr;
        libpress_vlc_saved_state[7] = dc_cb;
        libpress_vlc_saved_state[8] = dc_y;
        return 1;
    }
    out += 2;

ac:
    /* `out` is the next free halfword. */
    for (;;) {
        u8 *slot = ac_table + (SRL(bits, 19) << 3);

        entry = *(u32 *)slot;
        if (entry == 0) {
            bits = SLL(bits, 8);
            used += 8;
            if (used & 0x10) {
                used &= 0xF;
                bits |= SLL(*in++, used);
            } else {
                used &= 0xF;
            }
            entry = *(u32 *)(long_table + (SRL(bits, 23) << 2));
            second = 0;
        } else {
            second = *(u32 *)(slot + 4);
        }
        length = entry & 0xFF;
        bits = SLL(bits, length);
        used += length;
        if (used & 0x10) {
            used &= 0xF;
            bits |= SLL(*in++, used);
        } else {
            used &= 0xF;
        }
        value = entry >> 16;
        if (value == 0x7C1F) {
            goto escape;
        }
        *(u16 *)out = value;
        if (value == 0xFE00) {
            goto dc;
        }
        out += 2;
        if (second == 0) {
            continue;
        }
        value = second & 0xFFFF;
        if (value == 0x7C1F) {
            goto escape;
        }
        *(u16 *)out = value;
        if (value == 0xFE00) {
            goto dc;
        }
        value = second >> 16;
        out += 2;
        if (value == 0) {
            continue;
        }
        if (value == 0x7C1F) {
            goto escape;
        }
        *(u16 *)out = value;
        if (value == 0xFE00) {
            goto dc;
        }
        out += 2;
        continue;
    escape:
        /* Run and level follow as the 16 bits themselves. */
        *(u16 *)out = SRL(bits, 16);
        out += 2;
        value = *in++;
        bits = SLL(bits, 16) | SLL(value, used);
    }

end:
    /* Pad with end-of-block codes. */
    for (i = 0x40; ; i--) {
        *(u16 *)out = 0xFE00;
        out += 2;
        if (i == 0) {
            break;
        }
    }
    return 0;
}
