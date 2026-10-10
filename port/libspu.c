/*
 * libspu: the PsyQ sound library members the resident links (8004c4f8-
 * 8004e7a0), from their original code. The library's state keeps its
 * original addresses (libspu_* in the resident's symbol file) and its
 * registers are written through libspu_spu_registers into the register window
 * in game memory, write for write as the original writes the hardware. Where
 * the original waits for the hardware (_spu_Fw1ts's delay, the status and
 * control polls) the port applies the window at once (xem_host_spu_sync),
 * so each write the hardware would see in turn reaches the SPU in turn. DMA
 * channel 4 and the transfer FIFO go to the host's transfer calls; the
 * channel's interrupt arrives through DMACallback(4) after the modelled
 * transfer time, the SPU IRQ through InterruptCallback(9).
 *
 * The original reports a hardware wait that runs out (0xF00 polls) through an
 * empty print (mode_empty_debug_print); the host's SPU answers every wait at
 * once, so the port checks each condition once and returns the original's
 * timeout value when it does not hold.
 */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libetc.h"
#include "psyq/libspu.h"
#include "xem/spu.h"

/* The library's state at its original addresses. */
extern u16 *libspu_spu_registers;
extern s32 libspu_transfer_event;
extern s32 libspu_voice_key_on_mask;
extern s32 libspu_transfer_mode;
extern s32 libspu_reverb_on;
extern s32 libspu_reverb_work_area_reserved;
extern s32 libspu_reverb_work_area_start;
extern s32 libspu_reverb_mode_type;
extern s16 libspu_reverb_depth_left;
extern s16 libspu_reverb_depth_right;
extern s32 libspu_reverb_delay;
extern s32 libspu_reverb_feedback;
extern s32 libspu_init_word;
extern s32 libspu_voice_attributes;
extern u16 libspu_voice_attribute_end;
extern u8 libspu_reverb_clear_buffer[0x400];
extern s32 libspu_init_word_b;
extern s32 libspu_started;
extern u16 libspu_transfer_start_address;
extern s32 libspu_transfer_by_io;
extern s32 libspu_init_word_c;
extern s32 libspu_memory_shift_unit;
extern s32 libspu_memory_shift;
extern s32 libspu_memory_unit;
extern s32 libspu_memory_unit_mask;
extern s32 libspu_transfer_idle;
extern void (*libspu_transfer_callback)(void);
extern void (*libspu_irq_callback)(void);
extern u8 libspu_voice_init_bytes[0x10];
extern s32 libspu_transfer_is_read;
extern u32 libspu_transfer_ram_address;
extern u32 libspu_transfer_block_count;
extern s32 libspu_malloc_table;
extern s32 libspu_malloc_count;
extern u32 *libspu_malloc_reserved_top;
extern s32 libspu_reverb_work_area_starts[10];
extern u16 libspu_commons[10];

/* A reverb preset: the registers 0x1F801DC0-0x1F801DFF and which of them to
 * write (0: all). */
typedef struct {
    u32 mask;
    u16 regs[32];
} XemSpuReverbParameters;

extern XemSpuReverbParameters libspu_reverb_mode_parameters[10];

void *DMACallback(int channel, void (*func)(void));
void *InterruptCallback(int irq, void (*func)(void));
void DeliverEvent(unsigned long class, unsigned long spec);
long WaitEvent(long event);
void SpuStart(void);
long _SpuIsInAllocateArea_(u32 address);
long SpuClearReverbWorkArea(long mode);
void _spu_setReverbAttr(XemSpuReverbParameters *attr);

/* The register at byte offset `offset` from 0x1F801C00. */
#define R(offset) (*(u16 *)((u8 *)libspu_spu_registers + (offset)))

enum {
    SPU_MAIN_VOLUME_LEFT = 0x180,
    SPU_MAIN_VOLUME_RIGHT = 0x182,
    SPU_REVERB_DEPTH_LEFT = 0x184,
    SPU_REVERB_DEPTH_RIGHT = 0x186,
    SPU_KEY_ON = 0x188,
    SPU_KEY_OFF = 0x18C,
    SPU_PITCH_MODULATION = 0x190,
    SPU_NOISE = 0x194,
    SPU_REVERB_ON = 0x198,
    SPU_TRANSFER_ADDRESS = 0x1A6,
    SPU_CONTROL = 0x1AA,
    SPU_TRANSFER_CONTROL = 0x1AC,
    SPU_STATUS = 0x1AE,
    SPU_CD_VOLUME_LEFT = 0x1B0,
    SPU_CD_VOLUME_RIGHT = 0x1B2,
    SPU_EXTERN_VOLUME_LEFT = 0x1B4,
    SPU_EXTERN_VOLUME_RIGHT = 0x1B6,
    SPU_REVERB_CONFIG = 0x1C0,
};

/* SPUCNT's transfer mode (bits 4-5): 1 manual write, 2 DMA write, 3 DMA read. */
#define SPU_CONTROL_TRANSFER 0x30

/* The BIOS event libspu delivers when a transfer ends without a callback. */
#define SPU_TRANSFER_EVENT_CLASS 0xF0000009
#define SPU_TRANSFER_EVENT_SPEC 0x20

/* A write the hardware sees before the next: the original's delay loop. */
void _spu_Fw1ts(void) {
    xem_host_spu_sync();
}

/* Whether the register reads `value` under `mask` once the SPU has seen the
 * writes so far (the original polls up to 0xF00 times). */
static s32 xem_spu_reads(u32 offset, u16 mask, u16 value) {
    xem_host_spu_sync();
    return (R(offset) & mask) == value;
}

/* Manual write (the FIFO) of `size` bytes at the transfer start address. */
void _spu_FwriteByIO(void *data, long size) {
    u16 status = R(SPU_STATUS) & 0x7FF;

    R(SPU_TRANSFER_ADDRESS) = libspu_transfer_start_address;
    _spu_Fw1ts();
    if (size != 0) {
        /* The original feeds the FIFO 0x40 bytes at a time, each batch with
         * SPUCNT's manual-write mode and a wait for the busy flag. */
        xem_host_spu_write_io((u32)libspu_transfer_start_address << 3, data, size);
        R(SPU_CONTROL) = (R(SPU_CONTROL) & ~SPU_CONTROL_TRANSFER) | 0x10;
        _spu_Fw1ts();
        _spu_Fw1ts();
    }
    R(SPU_CONTROL) &= ~SPU_CONTROL_TRANSFER;
    xem_spu_reads(SPU_STATUS, 0x7FF, status);
}

/* The DMA-4 completion (libspu_transfer_wait_entry, installed by SpuStart):
 * end the transfer mode, then the transfer callback or the transfer event. */
void xem_spu_transfer_done(void) {
    if (!libspu_transfer_is_read) {
        _spu_Fw1ts();
    }
    R(SPU_CONTROL) &= ~SPU_CONTROL_TRANSFER;
    if (libspu_transfer_callback != NULL) {
        libspu_transfer_callback();
    } else {
        DeliverEvent(SPU_TRANSFER_EVENT_CLASS, SPU_TRANSFER_EVENT_SPEC);
    }
}

/* Start a DMA read of `blocks` 64-byte blocks from SPU address `address`
 * (8-byte units) into `data` (SpuReadDecodedData's path). */
void _spu_Fr_(void *data, u32 address, u32 blocks) {
    R(SPU_TRANSFER_ADDRESS) = address;
    _spu_Fw1ts();
    R(SPU_CONTROL) |= SPU_CONTROL_TRANSFER;
    _spu_Fw1ts();
    libspu_transfer_is_read = 1;
    xem_host_spu_transfer(address << 3, data, blocks << 6, 1);
}

/* The transfer steps: 2 set the start address (`address`, bytes), 1 enter
 * DMA write mode, 0 enter DMA read mode, 3 start the DMA of `size` bytes at
 * `data` (rounded up to whole blocks). -2 when the SPU does not answer. (The
 * original takes its arguments variadically.) */
long _spu_t(long mode, u32 address, void *data, u32 size) {
    u16 want;

    switch (mode) {
    case 2:
        libspu_transfer_start_address = address >> libspu_memory_shift;
        R(SPU_TRANSFER_ADDRESS) = libspu_transfer_start_address;
        return 0;
    case 1:
        libspu_transfer_is_read = 0;
        if (!xem_spu_reads(SPU_TRANSFER_ADDRESS, 0xFFFF, libspu_transfer_start_address)) {
            return -2;
        }
        R(SPU_CONTROL) = (R(SPU_CONTROL) & ~SPU_CONTROL_TRANSFER) | 0x20;
        return 0;
    case 0:
        libspu_transfer_is_read = 1;
        if (!xem_spu_reads(SPU_TRANSFER_ADDRESS, 0xFFFF, libspu_transfer_start_address)) {
            return -2;
        }
        R(SPU_CONTROL) |= SPU_CONTROL_TRANSFER;
        return 0;
    case 3:
        want = libspu_transfer_is_read == 1 ? 0x30 : 0x20;
        if (!xem_spu_reads(SPU_CONTROL, SPU_CONTROL_TRANSFER, want)) {
            return -2;
        }
        libspu_transfer_ram_address = (u32)(unsigned long)data;
        libspu_transfer_block_count = (size >> 6) + ((size & 0x3F) != 0);
        xem_host_spu_transfer((u32)libspu_transfer_start_address << 3, data, libspu_transfer_block_count << 6,
                              libspu_transfer_is_read == 1);
        return 0;
    }
    return 0;
}

unsigned long _spu_Fw(unsigned char *data, unsigned long size) {
    if (libspu_transfer_by_io) {
        _spu_FwriteByIO(data, size);
        return size;
    }
    _spu_t(2, (u32)libspu_transfer_start_address << libspu_memory_shift, NULL, 0);
    _spu_t(1, 0, NULL, 0);
    _spu_t(3, 0, data, size);
    return size;
}

unsigned long _spu_Fr(unsigned char *data, unsigned long size) {
    _spu_t(2, (u32)libspu_transfer_start_address << libspu_memory_shift, NULL, 0);
    _spu_t(0, 0, NULL, 0);
    _spu_t(3, 0, data, size);
    return size;
}

/* Register `index` (halfwords from 0x1F801C00) = `value`, an address shifted
 * to 8-byte units when `address` is set. */
void _spu_FsetRXX(long index, u32 value, long address) {
    R(index * 2) = address ? value >> libspu_memory_shift : value;
}

/* An SPU address rounded up to the memory unit: -1 returns it in 8-byte
 * units, -2 in bytes; otherwise it is written to register `index`. */
u32 _spu_FsetRXXa(long index, u32 address) {
    u32 units;

    if (libspu_memory_shift_unit != 0 && address % libspu_memory_unit != 0) {
        address = (address + libspu_memory_unit) & ~libspu_memory_unit_mask;
    }
    units = address >> libspu_memory_shift;
    if (index == -2) {
        return address;
    }
    if (index == -1) {
        return units & 0xFFFF;
    }
    R(index * 2) = units;
    return address;
}

void _SpuDataCallback(void (*func)(void)) {
    DMACallback(4, func);
}

void _SpuCallback(void (*func)(void)) {
    InterruptCallback(9, func);
}

/* Reset the SPU: volumes, transfer control, reverb off; with `mode` 0 also
 * every voice (keyed on and off on a silent block at 0x1000) and SPUCNT on. */
long _spu_init(long mode) {
    s32 i;

    libspu_transfer_by_io = 0;
    libspu_init_word_c = 0;
    libspu_transfer_start_address = 0;
    R(SPU_MAIN_VOLUME_LEFT) = 0;
    R(SPU_MAIN_VOLUME_RIGHT) = 0;
    R(SPU_CONTROL) = 0;
    _spu_Fw1ts();
    R(SPU_MAIN_VOLUME_LEFT) = 0;
    R(SPU_MAIN_VOLUME_RIGHT) = 0;
    xem_spu_reads(SPU_STATUS, 0x7FF, 0);
    libspu_memory_shift_unit = 2;
    libspu_memory_shift = 3;
    libspu_memory_unit = 8;
    libspu_memory_unit_mask = 7;
    R(SPU_TRANSFER_CONTROL) = 4;
    R(SPU_REVERB_DEPTH_LEFT) = 0;
    R(SPU_REVERB_DEPTH_RIGHT) = 0;
    R(SPU_KEY_OFF) = 0xFFFF;
    R(SPU_KEY_OFF + 2) = 0xFFFF;
    R(SPU_REVERB_ON) = 0;
    R(SPU_REVERB_ON + 2) = 0;
    for (i = 0; i < 10; i++) {
        libspu_commons[i] = 0;
    }
    if (mode == 0) {
        libspu_transfer_start_address = 0x200;
        R(SPU_PITCH_MODULATION) = 0;
        R(SPU_PITCH_MODULATION + 2) = 0;
        R(SPU_NOISE) = 0;
        R(SPU_NOISE + 2) = 0;
        R(SPU_CD_VOLUME_LEFT) = 0;
        R(SPU_CD_VOLUME_RIGHT) = 0;
        R(SPU_EXTERN_VOLUME_LEFT) = 0;
        R(SPU_EXTERN_VOLUME_RIGHT) = 0;
        _spu_FwriteByIO(libspu_voice_init_bytes, 0x10);
        for (i = 0; i < 24; i++) {
            R(i * 0x10 + 0x0) = 0;
            R(i * 0x10 + 0x2) = 0;
            R(i * 0x10 + 0x4) = 0x3FFF;
            R(i * 0x10 + 0x6) = 0x200;
            R(i * 0x10 + 0x8) = 0;
            R(i * 0x10 + 0xA) = 0;
        }
        R(SPU_KEY_ON) = 0xFFFF;
        R(SPU_KEY_ON + 2) = 0xFF;
        for (i = 0; i < 4; i++) {
            _spu_Fw1ts();
        }
        R(SPU_KEY_OFF) = 0xFFFF;
        R(SPU_KEY_OFF + 2) = 0xFF;
        for (i = 0; i < 4; i++) {
            _spu_Fw1ts();
        }
    }
    libspu_transfer_idle = 1;
    R(SPU_CONTROL) = 0xC000;
    libspu_transfer_callback = NULL;
    libspu_irq_callback = NULL;
    return 0;
}

void SpuStart(void) {
    if (libspu_started != 0) {
        return;
    }
    libspu_started = 1;
    EnterCriticalSection();
    _SpuDataCallback((void (*)(void))XEM_LIBSPU_TRANSFER_WAIT_ENTRY);
    libspu_transfer_event = OpenEvent(SPU_TRANSFER_EVENT_CLASS, SPU_TRANSFER_EVENT_SPEC, 0x2000, NULL);
    EnableEvent(libspu_transfer_event);
    ExitCriticalSection();
}

void _SpuInit(long mode) {
    s32 i;

    ResetCallback();
    _spu_init(mode);
    if (mode == 0) {
        for (i = 0; i < 24; i++) {
            (&libspu_voice_attribute_end)[-i] = 0xC000;
        }
    }
    SpuStart();
    libspu_reverb_on = 0;
    libspu_reverb_work_area_reserved = 0;
    libspu_reverb_mode_type = 0;
    libspu_reverb_depth_left = 0;
    libspu_reverb_depth_right = 0;
    libspu_reverb_delay = 0;
    libspu_reverb_feedback = 0;
    libspu_reverb_work_area_start = libspu_reverb_work_area_starts[0];
    _spu_FsetRXX(0xD1, libspu_reverb_work_area_starts[0], 0);
    libspu_malloc_table = 0;
    libspu_malloc_count = 0;
    libspu_malloc_reserved_top = NULL;
    libspu_transfer_mode = 0;
    libspu_transfer_by_io = 0;
    libspu_voice_key_on_mask = 0;
    libspu_voice_attributes = 0;
    libspu_init_word = 0;
    libspu_init_word_b = 0;
}

void SpuInit(void) {
    _SpuInit(0);
}

void SpuQuit(void) {
    if (libspu_started != 1) {
        return;
    }
    libspu_started = 0;
    EnterCriticalSection();
    libspu_transfer_callback = NULL;
    libspu_irq_callback = NULL;
    _SpuDataCallback(NULL);
    CloseEvent(libspu_transfer_event);
    DisableEvent(libspu_transfer_event);
    ExitCriticalSection();
}

/* The memory manager's table: one free block from 0x1010 to the end of SPU
 * RAM, its terminator flag (0x40000000) set. */
long SpuInitMalloc(long num, char *top) {
    u32 *table = (u32 *)top;

    if (num <= 0) {
        return 0;
    }
    table[0] = 0x40001010;
    libspu_malloc_reserved_top = table;
    libspu_malloc_count = 0;
    libspu_malloc_table = num;
    table[1] = (0x10000 << libspu_memory_shift) - 0x1010;
    return num;
}

long SpuSetNoiseClock(long clock) {
    if (clock < 0) {
        clock = 0;
    } else if (clock >= 0x40) {
        clock = 0x3F;
    }
    R(SPU_CONTROL) = (R(SPU_CONTROL) & 0xC0FF) | (clock & 0x3F) << 8;
    return clock;
}

/* Whether `address` (8-byte units) lies in an allocated block of the memory
 * manager's table, or below one. */
long _SpuIsInAllocateArea_(u32 address) {
    u32 *entry = libspu_malloc_reserved_top;
    u32 start;

    address <<= libspu_memory_shift;
    if (entry == NULL) {
        return 0;
    }
    for (;; entry += 2) {
        if (entry[0] & 0x80000000) {
            continue;
        }
        if (entry[0] & 0x40000000) {
            return 0;
        }
        start = entry[0] & 0x0FFFFFFF;
        if (start >= address || address < start + entry[1]) {
            return 1;
        }
    }
}

long SpuSetReverb(long on_off) {
    switch (on_off) {
    case 0:
        libspu_reverb_on = 0;
        R(SPU_CONTROL) &= ~0x80;
        break;
    case 1:
        if (libspu_reverb_work_area_reserved != on_off &&
            _SpuIsInAllocateArea_(libspu_reverb_work_area_start)) {
            libspu_reverb_on = 0;
            R(SPU_CONTROL) &= ~0x80;
        } else {
            libspu_reverb_on = on_off;
            R(SPU_CONTROL) |= 0x80;
        }
        break;
    }
    return libspu_reverb_on;
}

/* Start a DMA read of the CD input capture buffers (SPU 0x0000-0x0FFF): flag 5
 * the left half (0x800 bytes), 6 the right half into `data` + 0x800, others
 * both. Returns SPUSTAT's capture half bit. */
long SpuReadDecodedData(void *data, long flag) {
    if (flag == 5) {
        _spu_Fr_(data, 0, 0x20);
    } else if (flag == 6) {
        _spu_Fr_((u8 *)data + 0x800, 0x100, 0x20);
    } else {
        _spu_Fr_(data, 0, 0x40);
    }
    xem_host_spu_sync();
    return (R(SPU_STATUS) & 0x800) != 0;
}

/* 0 disables the SPU IRQ (acknowledging it), 1 enables it, 3 does both. */
long SpuSetIRQ(long on_off) {
    if (on_off == 0 || on_off == 3) {
        R(SPU_CONTROL) &= ~0x40;
        if (!xem_spu_reads(SPU_CONTROL, 0x40, 0)) {
            return -1;
        }
    }
    if (on_off == 1 || on_off == 3) {
        R(SPU_CONTROL) |= 0x40;
        if (!xem_spu_reads(SPU_CONTROL, 0x40, 0x40)) {
            return -1;
        }
    }
    return on_off;
}

SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc func) {
    SpuIRQCallbackProc previous = libspu_irq_callback;

    if (func != previous) {
        libspu_irq_callback = func;
        _SpuCallback(func);
    }
    return previous;
}

/* The voice's current envelope level and its state: keyed on (by libspu's
 * own key calls, which the game does not make) 1 sounding or 3 silent,
 * otherwise 2 sounding or 0 silent. */
void SpuGetVoiceEnvelopeAttr(int voice, long *status, short *level) {
    s16 current;

    xem_host_spu_sync();
    current = R(voice << 4 | 0xC);
    *level = current;
    if (libspu_voice_key_on_mask & (1 << voice)) {
        *status = current > 0 ? 1 : 3;
    } else {
        *status = current > 0 ? 2 : 0;
    }
}

unsigned long SpuRead(unsigned char *addr, unsigned long size) {
    if (size > 0x7EFF0) {
        size = 0x7EFF0;
    }
    _spu_Fr(addr, size);
    if (libspu_transfer_callback == NULL) {
        libspu_transfer_idle = 0;
    }
    return size;
}

unsigned long SpuWrite(unsigned char *addr, unsigned long size) {
    if (size > 0x7EFF0) {
        size = 0x7EFF0;
    }
    _spu_Fw(addr, size);
    if (libspu_transfer_callback == NULL) {
        libspu_transfer_idle = 0;
    }
    return size;
}

/* The transfer start address, rounded up to 8 bytes; 0 outside
 * 0x1010-0x7FFF8. */
unsigned long SpuSetTransferStartAddr(unsigned long addr) {
    u32 units;

    if (addr - 0x1010 > 0x7EFE8) {
        return 0;
    }
    units = _spu_FsetRXXa(-1, addr);
    libspu_transfer_start_address = units;
    return (units & 0xFFFF) << libspu_memory_shift;
}

/* 0 DMA, 1 I/O (the FIFO). */
long SpuSetTransferMode(long mode) {
    long by_io = mode == 1;

    libspu_transfer_mode = mode;
    libspu_transfer_by_io = by_io;
    return by_io;
}

SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func) {
    SpuTransferCallbackProc previous = libspu_transfer_callback;

    if (func != previous) {
        libspu_transfer_callback = func;
    }
    return previous;
}

/* A main volume register value: a sweep mode 1-7 (SPU_VOICE_LINEARIncN ...)
 * gives 0x8000 + (mode - 1) * 0x1000 with the volume clamped to 0-0x7F,
 * otherwise the fixed volume. */
static u16 xem_spu_main_volume(s16 volume, s16 mode, s32 use_mode) {
    u16 sweep = 0;
    s32 v;

    if (use_mode && (u16)mode < 8 && mode != 0) {
        sweep = 0x8000 + (mode - 1) * 0x1000;
    }
    if (sweep == 0) {
        return volume & 0x7FFF;
    }
    v = volume;
    if (v >= 0x80) {
        v = 0x7F;
    } else if (v < 0) {
        v = 0;
    }
    return (v & 0x7FFF) | sweep;
}

/* Bit `bit` of SPUCNT from a flag. */
static void xem_spu_control_bit(u16 bit, long on) {
    if (on) {
        R(SPU_CONTROL) |= bit;
    } else {
        R(SPU_CONTROL) &= ~bit;
    }
}

/* SpuCommonAttr: the master volume pair and its sweep modes, the CD and
 * external inputs' volumes, reverb and mix bits; mask 0 sets everything. */
void SpuSetCommonAttr(SpuCommonAttr *attr) {
    u32 mask = attr->mask;
    s32 all = mask == 0;

    if (all || (mask & 1)) {
        R(SPU_MAIN_VOLUME_LEFT) = xem_spu_main_volume(attr->mvol.left, attr->mvolmode.left, all || (mask & 4));
    }
    if (all || (mask & 2)) {
        R(SPU_MAIN_VOLUME_RIGHT) = xem_spu_main_volume(attr->mvol.right, attr->mvolmode.right, all || (mask & 8));
    }
    if (all || (mask & 0x40)) {
        R(SPU_CD_VOLUME_LEFT) = attr->cd.volume.left;
    }
    if (all || (mask & 0x80)) {
        R(SPU_CD_VOLUME_RIGHT) = attr->cd.volume.right;
    }
    if (all || (mask & 0x400)) {
        R(SPU_EXTERN_VOLUME_LEFT) = attr->ext.volume.left;
    }
    if (all || (mask & 0x800)) {
        R(SPU_EXTERN_VOLUME_RIGHT) = attr->ext.volume.right;
    }
    if (all || (mask & 0x100)) {
        xem_spu_control_bit(0x4, attr->cd.reverb);
    }
    if (all || (mask & 0x200)) {
        xem_spu_control_bit(0x1, attr->cd.mix);
    }
    if (all || (mask & 0x1000)) {
        xem_spu_control_bit(0x8, attr->ext.reverb);
    }
    if (all || (mask & 0x2000)) {
        xem_spu_control_bit(0x2, attr->ext.mix);
    }
}

/* The reverb registers 0x1F801DC0-0x1F801DFF the mask selects (0: all). */
void _spu_setReverbAttr(XemSpuReverbParameters *attr) {
    u32 mask = attr->mask;
    s32 i;

    for (i = 0; i < 32; i++) {
        if (mask == 0 || (mask & (1u << i))) {
            R(SPU_REVERB_CONFIG + i * 2) = attr->regs[i];
        }
    }
}

/* Select reverb preset `mode` (0-9; | 0x100 also clears its work area): the
 * depth goes to 0, the preset's registers and work area base are written
 * with reverb off, and reverb is re-enabled when it was on. */
long SpuSetReverbModeType(long mode) {
    XemSpuReverbParameters parameters;
    s32 clear = 0;
    s32 was_on;

    if (mode & 0x100) {
        mode &= ~0x100;
        clear = 1;
    }
    if ((u32)mode >= 10 || _SpuIsInAllocateArea_(libspu_reverb_work_area_starts[mode])) {
        return -1;
    }
    parameters = libspu_reverb_mode_parameters[mode];
    libspu_reverb_mode_type = mode;
    libspu_reverb_work_area_start = libspu_reverb_work_area_starts[mode];
    parameters.mask = 0;
    if (mode == 7) {
        libspu_reverb_feedback = 0x7F;
        libspu_reverb_delay = 0x7F;
    } else if (mode == 8) {
        libspu_reverb_feedback = 0;
        libspu_reverb_delay = 0x7F;
    } else {
        libspu_reverb_feedback = 0;
        libspu_reverb_delay = 0;
    }
    was_on = (R(SPU_CONTROL) >> 7) & 1;
    if (was_on) {
        R(SPU_CONTROL) &= ~0x80;
        xem_host_spu_sync();
    }
    R(SPU_REVERB_DEPTH_LEFT) = 0;
    R(SPU_REVERB_DEPTH_RIGHT) = 0;
    libspu_reverb_depth_left = 0;
    libspu_reverb_depth_right = 0;
    _spu_setReverbAttr(&parameters);
    if (clear) {
        SpuClearReverbWorkArea(mode);
    }
    _spu_FsetRXX(0xD1, libspu_reverb_work_area_start, 0);
    if (was_on) {
        xem_host_spu_sync();
        R(SPU_CONTROL) |= 0x80;
    }
    return 0;
}

/* Zero reverb preset `mode`'s work area by DMA from the zeroed buffer,
 * 0x400 bytes per transfer, waiting for the transfer event after each. */
long SpuClearReverbWorkArea(long mode) {
    void (*callback)(void) = NULL;
    s32 by_io;
    s32 restore = 0;
    s32 more = 1;
    u32 size;
    u32 address;
    u32 chunk;

    if ((u32)mode >= 10 || _SpuIsInAllocateArea_(libspu_reverb_work_area_starts[mode])) {
        return -1;
    }
    if (mode == 0) {
        size = 0x10 << libspu_memory_shift;
        address = 0xFFF0 << libspu_memory_shift;
    } else {
        size = (0x10000 - libspu_reverb_work_area_starts[mode]) << libspu_memory_shift;
        address = libspu_reverb_work_area_starts[mode] << libspu_memory_shift;
    }
    by_io = libspu_transfer_by_io;
    if (by_io == 1) {
        libspu_transfer_by_io = 0;
        restore = 1;
    }
    if (libspu_transfer_callback != NULL) {
        callback = libspu_transfer_callback;
        libspu_transfer_callback = NULL;
    }
    do {
        if (size <= 0x400) {
            chunk = size;
            more = 0;
        } else {
            chunk = 0x400;
        }
        _spu_t(2, address, NULL, 0);
        _spu_t(1, 0, NULL, 0);
        _spu_t(3, 0, libspu_reverb_clear_buffer, chunk);
        WaitEvent(libspu_transfer_event);
        size -= 0x400;
        address += 0x400;
    } while (more);
    if (restore) {
        libspu_transfer_by_io = by_io;
    }
    if (callback != NULL) {
        libspu_transfer_callback = callback;
    }
    return 0;
}

void SpuSetReverbModeDepth(short depth_left, short depth_right) {
    R(SPU_REVERB_DEPTH_LEFT) = depth_left;
    R(SPU_REVERB_DEPTH_RIGHT) = depth_right;
    libspu_reverb_depth_left = depth_left;
    libspu_reverb_depth_right = depth_right;
}

/* Echo (7) and Delay (8) only: the same-side reflections and comb taps move
 * with `delay` (0-127). */
void SpuSetReverbModeDelayTime(long delay) {
    XemSpuReverbParameters p;
    s32 d13;
    s32 d12;

    if (libspu_reverb_mode_type < 7 || libspu_reverb_mode_type > 8) {
        return;
    }
    p = libspu_reverb_mode_parameters[libspu_reverb_mode_type];
    d13 = (s32)(delay << 13) / 127;
    d12 = (s32)(delay << 12) / 127;
    libspu_reverb_delay = delay;
    p.mask = 0x0C011C00;
    p.regs[10] = d13 - p.regs[0];
    p.regs[11] = d12 - p.regs[1];
    p.regs[12] = p.regs[13] + d12;
    p.regs[16] = p.regs[17] + d12;
    p.regs[27] = p.regs[29] + d12;
    p.regs[26] = p.regs[28] + d12;
    _spu_setReverbAttr(&p);
}

/* Echo (7) and Delay (8) only: the reflection volume vWALL from `feedback`
 * (0-127). */
void SpuSetReverbModeFeedback(long feedback) {
    XemSpuReverbParameters p;

    if (libspu_reverb_mode_type < 7 || libspu_reverb_mode_type > 8) {
        return;
    }
    p = libspu_reverb_mode_parameters[libspu_reverb_mode_type];
    libspu_reverb_feedback = feedback;
    p.mask = 0x80;
    p.regs[7] = (s32)(feedback * 0x8100) / 127;
    _spu_setReverbAttr(&p);
}

void SpuGetReverbModeType(long *type) {
    *type = libspu_reverb_mode_type;
}
