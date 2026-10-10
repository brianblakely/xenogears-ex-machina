/*
 * The remaining BIOS calls the game makes and the development host's file
 * server.
 *
 * - The BIOS file calls B(32h)-B(36h), B(41h)-B(45h) on the memory card
 *   devices "bu00:" and "bu10:" go to the host's card service; they block as
 *   the BIOS does, here by suspending until the card's access time passed.
 * - WaitEvent B(0Ah) waits for a ready-mode event (libspu's reverb clear).
 * - GetGp: the game's $gp.
 * - Krom2RawAdd B(51h): the address of a 16x15 Shift-JIS glyph in the BIOS
 *   ROM's Kanji font (docs/original-boundaries.md, Services).
 * - libsn's PC file server (PCinit ... PCwrite) traps into a development host
 *   with `break`; without one attached every call fails.
 */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libsn.h"
#include "xem/card.h"
#include "xem/port.h"

/* Wait until the card's modelled access time has passed, as the BIOS blocks
 * in its file calls. */
static long xem_card_result(long result) {
    while (xem_host_card_busy()) {
        xem_host_yield(XEM_YIELD_POLL);
    }
    return result;
}

/* B(32h): flags 1 read, 2 write, 0x200 create (blocks << 16 the size). */
long open(char *devname, unsigned long flag) {
    if (!XEM_CARD_DEVICE(devname)) {
        return -1;
    }
    return xem_card_result(xem_host_card_open(devname, flag));
}

/* B(34h), B(35h): whole 128-byte sectors at the file position. */
long read(long fd, void *buf, long n) {
    return xem_card_result(xem_host_card_read(fd, buf, n));
}

long write(long fd, void *buf, long n) {
    return xem_card_result(xem_host_card_write(fd, buf, n));
}

long close(long fd) {
    return xem_card_result(xem_host_card_close(fd));
}

/* B(41h): 1 when formatted. */
long format(char *device) {
    if (!XEM_CARD_DEVICE(device)) {
        return 0;
    }
    return xem_card_result(xem_host_card_format(device));
}

/* B(42h), B(43h): the files matching a name with '?' and '*' wildcards (the
 * device alone lists every file). */
struct DIRENTRY *firstfile(char *name, struct DIRENTRY *dir) {
    if (!XEM_CARD_DEVICE(name)) {
        return NULL;
    }
    return xem_card_result(xem_host_card_firstfile(name, dir)) ? dir : NULL;
}

struct DIRENTRY *nextfile(struct DIRENTRY *dir) {
    return xem_card_result(xem_host_card_nextfile(dir)) ? dir : NULL;
}

/* B(44h), B(45h): 1 on success. */
long rename(char *from, char *to) {
    if (!XEM_CARD_DEVICE(from) || !XEM_CARD_DEVICE(to)) {
        return 0;
    }
    return xem_card_result(xem_host_card_rename(from, to));
}

long delete(char *name) {
    if (!XEM_CARD_DEVICE(name)) {
        return 0;
    }
    return xem_card_result(xem_host_card_delete(name));
}

/* B(0Ah): wait until the event is ready, then clear it. */
long WaitEvent(long event) {
    while (!TestEvent(event)) {
        xem_host_yield(XEM_YIELD_POLL);
    }
    return 1;
}

/* The value of $gp, which game code always holds at the linker's _gp
 * (boot_reset_stack_and_gp); the arena stores it in its task context. */
extern u8 _gp;

unsigned long GetGp(void) {
    return (u32)(&_gp - (u8 *)NULL);
}

/* The Kanji font: 16x15 glyphs of 30 bytes, one per assigned JIS X 0208
 * character of rows 1-8 (non-kanji, Shift-JIS 8140-84BE) and rows 16-47 (the
 * first-level kanji, 889F-9872) in code order, 3489 glyphs ending at ROM
 * offset 0x7F8DE. The ROM at 0xBFC00000 lies outside game memory; the port
 * gives its KUSEG address, 0x1FC00000, where the host places the font
 * (xem_core::bios::install_kanji_font). */
#define XEM_KANJI_FONT 0x1FC66000
#define XEM_KANJI_GLYPH_BYTES 30
#define XEM_KANJI_NON_KANJI 524

/* The assigned cells of rows 1-8: {row, first cell, last cell}. */
static const u8 xem_kanji_rows[][3] = {
    {1, 1, 94},  {2, 1, 14},  {2, 26, 33}, {2, 42, 48}, {2, 60, 74}, {2, 82, 89}, {2, 94, 94},
    {3, 16, 25}, {3, 33, 58}, {3, 65, 90}, {4, 1, 83},  {5, 1, 86},  {6, 1, 24},  {6, 33, 56},
    {7, 1, 33},  {7, 49, 81}, {8, 1, 32},
};

/* The glyph index of a Shift-JIS code, or -1 for a code the font lacks. */
s32 xem_kanji_index(u32 code) {
    u32 lead = code >> 8 & 0xFF;
    u32 trail = code & 0xFF;
    u32 row;
    u32 cell;
    s32 index = 0;
    u32 i;

    if ((code >> 16) != 0 || lead < 0x81 || lead > 0x9F || trail < 0x40 || trail == 0x7F || trail > 0xFC) {
        return -1;
    }
    if (trail >= 0x9F) {
        row = (lead - 0x81) * 2 + 2;
        cell = trail - 0x9E;
    } else {
        row = (lead - 0x81) * 2 + 1;
        cell = trail - 0x40 + 1 - (trail > 0x7F);
    }
    if (row <= 8) {
        for (i = 0; i < sizeof(xem_kanji_rows) / sizeof(xem_kanji_rows[0]); i++) {
            const u8 *range = xem_kanji_rows[i];

            if (range[0] == row && cell >= range[1] && cell <= range[2]) {
                return index + (cell - range[1]);
            }
            index += range[2] - range[1] + 1;
        }
        return -1;
    }
    if (row >= 16 && (row < 47 || (row == 47 && cell <= 51))) {
        return XEM_KANJI_NON_KANJI + (row - 16) * 94 + (cell - 1);
    }
    return -1;
}

long Krom2RawAdd(unsigned long sjiscode) {
    s32 index = xem_kanji_index(sjiscode);

    if (index < 0) {
        return -1;
    }
    return XEM_KANJI_FONT + index * XEM_KANJI_GLYPH_BYTES;
}

/* libsn: each call is a `break` the development host answers (its code
 * names the call); with no host attached the call fails. */
static int xem_pc_call(unsigned int code) {
    xem_host_debug_break(code);
    return -1;
}

int PCinit(void) {
    return xem_pc_call(0x101);
}

int PCcreat(char *name, int perms) {
    return xem_pc_call(0x102);
}

int PCopen(char *name, int flags, int perms) {
    return xem_pc_call(0x103);
}

int PCclose(int fd) {
    return xem_pc_call(0x104);
}

int PCread(int fd, char *buff, int len) {
    return xem_pc_call(0x105);
}

int PCwrite(int fd, char *buff, int len) {
    return xem_pc_call(0x106);
}

int PClseek(int fd, int offset, int mode) {
    return xem_pc_call(0x107);
}
