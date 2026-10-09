#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libgpu.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "resident/text.h"
#include "resident/window.h"
#include "resident/pad.h"
#include "resident/console.h"
#include "resident/sound.h"
#include "resident/cd.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "own_declarations.h"

/* This unit's own variables: those of up to 8 bytes in its .sbss
 * (8005934c), the larger window, text and controller queue buffers in its
 * .bss (80059fd8), as the original assembler placed them (SBSS_main2 in
 * slus_006.64.mk). */
static s32 D_8005934C;  /* font: first byte of a two-byte character */
static s32 D_80059350;
static s32 D_80059354;
static s32 D_80059358;
static u8 *D_8005935C;  /* font glyph data */
static u8 **D_80059360; /* system data: resource table */
static s32 D_80059364;
static u8 *D_80059368;  /* system data block */
static u16 *D_8005936C; /* font block: halfword 1 glyph offset, 2 first
                         * byte of a two-byte character */
static u8 D_80059370;   /* play time frames */
static u32 D_80059374;  /* held pad buttons of the last frame */
static u32 D_80059378;
static u32 D_8005937C;  /* queued controller states */
static u32 D_80059380;  /* queue write index */
static u32 D_80059384;  /* queue read index */
static u8 D_80059388;   /* kind of the last read controller */
static u8 D_8005938C;
static s32 D_80059390;  /* the vertical-blank callback polls the host */
/* The one-line layout window and its line. */
static Window D_80059FD8;
static WindowLine D_8005A068;
/* Number character codes: color, 10 digits, 0xFFFF, and two that nothing
 * addresses (a word of its own would be a small variable, in .sbss). */
static u16 D_8005A0C8[14];
static u8 D_8005A0E4[0x18]; /* decoded text */
/* Queued controller states (16 entries of the six state words). */
static u16 D_8005A0FC[16];
static u16 D_8005A11C[16];
static u16 D_8005A13C[16];
static u16 D_8005A15C[16];
static u16 D_8005A17C[16];
static u16 D_8005A19C[16];
static Actuator D_8005A1BC[2];

/* The text palette: two 16-colour CLUTs. */
u16 D_80050190[32] = {
    0x0000, 0xF7BD, 0xC086, 0xF7BD, 0x0000, 0xF7BD, 0xC086, 0xF7BD,
    0x0000, 0xF7BD, 0xC086, 0xF7BD, 0x0000, 0xF7BD, 0xC086, 0xF7BD,
    0x0000, 0x0000, 0x0000, 0x0000, 0xF7BD, 0xF7BD, 0xF7BD, 0xF7BD,
    0xC086, 0xC086, 0xC086, 0xC086, 0xF7BD, 0xF7BD, 0xF7BD, 0xF7BD,
};
/* The rows of the special 0xFFFF glyph. */
u16 D_800501D0[11] = {
    0xC07F, 0x60C0, 0xA0A0, 0x2091, 0x208A, 0x2084, 0x208A, 0x2091, 0xA0A0, 0x60C0, 0xC07F,
};
u16 D_800501E8[8] = {0x20, 0x40, 0x10, 0x80, 0x4, 0x1, 0x8, 0x2}; /* button bits */
u8 D_800501F8 = 0;              /* play time stopped at 100 hours */
void (*D_800501FC)(void) = NULL; /* vertical-blank hook */
s32 D_80050200 = 1;
s32 D_80050204 = 0;
s32 D_80050208 = 0; /* queue overflowed */
u8 D_8005020C[16] = {
    0x80, 0x80, 0xFF, 0xFF, 0x80, 0x80, 0xFF, 0x80, 0x00, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x80,
};
u8 D_8005021C[16] = {
    0x80, 0x00, 0x80, 0x00, 0xFF, 0x80, 0xFF, 0x80, 0x80, 0x00, 0x80, 0x80, 0xFF, 0x80, 0x80, 0x80,
};
s32 D_8005022C = 0; /* frames the held buttons have not changed */
s32 D_80050230 = 0;
s32 D_80050234 = 0; /* nothing reads it */
u8 D_80050238[8] = {0, 1, 2, 3, 4, 5, 6, 7}; /* button assignment */

/* Unpacked size of packed data (its first word). */
s32 func_80032E7C(s32 *packed) {
    return *packed;
}

INCLUDE_ASM("decomp/src/resident", func_80032E88);

INCLUDE_ASM("decomp/src/resident", func_80032EB4);

/* Build a message window's two texture halves for each line and display
 * buffer. Lines share a glyph image in pairs, using alternating CLUTs;
 * two draw modes select the texture pages either side of the 256-pixel split. */
void func_80032F54(Window *window, s16 vram_x, s16 vram_y, s16 x, u16 y,
                   u16 columns, u16 rows) {
    s32 i;
    s32 half_row;
    s32 plane;
    s32 uv;
    s16 texture_y;
    SPRT *sprite;

    window->unk4 = x;
    window->flags = 0;
    window->unk84 = 0;
    window->queue = NULL;
    window->queued = 0;
    window->unk14 = 14;
    window->unk68 = 1;
    window->unk69 = 1;
    window->width = columns;
    window->unk6C = 0;
    window->unk6A = 0;
    window->unk6D = 0;
    window->unk6B = 0;
    window->unk6E = 0xFF;
    window->unkE = vram_y;
    window->unk6 = y;
    window->lines = rows;
    window->width |= 1;
    window->unk8 = (u16)window->width * 4;
    window->stride = (u16)window->width + 3;
    func_800324B8(0x29);
    window->layout = func_80031BDC(window->lines * sizeof(WindowLine), 2);
    func_800324B8(0x28);
    window->image = func_80031BDC(window->stride * 28, 2);
    setlen(&window->tile[0], 3);
    *(u32 *)&window->tile[0].r0 = 0x60000000;
    *(u32 *)&window->tile[0].x0 = (window->unk4 - 7) | ((window->unk6 - 5) << 16);
    *(u32 *)&window->tile[0].w = (window->width * 4 + 13) |
                                 ((window->lines * window->unk14 + 10) << 16);
    SetSemiTrans(&window->tile[0], 1);
    window->tile[1] = window->tile[0];
    for (i = 0; i < window->lines; i++) {
        *(u32 *)&window->layout[i].sprite[0][0].x0 = window->unk4 |
                                        ((window->unk6 + window->unk14 * i) << 16);
        *(u32 *)&window->layout[i].sprite[0][1].x0 = (window->unk4 + 256) |
                                        ((window->unk6 + window->unk14 * i) << 16);
        half_row = i / 2;
        plane = i & 1;
        *(u32 *)&window->layout[i].sprite[0][0].w =
            window->unk8 < 257 ? window->unk8 | 0xD0000 : 0xD0100;
        *(u32 *)&window->layout[i].sprite[0][1].w =
            window->unk8 >= 257 ? (window->unk8 - 240) | 0xD0000 : 0xD0000;
        texture_y = vram_y + half_row * 13;
        uv = (vram_x & 0x3F) * 4 | ((texture_y & 0xFF) << 8);
        *(u16 *)&window->layout[i].sprite[0][0].u0 = uv;
        *(u16 *)&window->layout[i].sprite[0][1].u0 = uv;
        setSprt(&window->layout[i].sprite[0][0]);
        setSprt(&window->layout[i].sprite[0][1]);
        setShadeTex(&window->layout[i].sprite[0][0], 1);
        setShadeTex(&window->layout[i].sprite[0][1], 1);
        sprite = (SPRT *)(i * sizeof(WindowLine) + (s32)window->layout);
        window->layout[i].sprite[1][0] = *sprite;
        sprite = (SPRT *)(i * sizeof(WindowLine) + (s32)window->layout + sizeof(SPRT));
        window->layout[i].sprite[1][1] = *sprite;
        window->layout[i].rect.x = vram_x;
        window->layout[i].rect.y = texture_y;
        window->layout[i].rect.w = window->stride;
        window->layout[i].rect.h = 13;
        window->layout[i].width = 0;
        window->layout[i].clut = plane == 0 ? D_800595D4 : D_80059414;
        window->layout[i].row = vram_y + half_row * 13;
        window->layout[i].plane = plane;
        window->layout[i].slot = i;
    }
    SetDrawMode((DR_MODE *)window->unk30, 0, 0, GetTPage(0, 0, vram_x, vram_y), NULL);
    SetDrawMode((DR_MODE *)window->unk3C, 0, 0, GetTPage(0, 0, vram_x + 64, vram_y), NULL);
}

/* Turn a resource's offset table (count, then offsets) into pointers.
 * Returns the count. */
u32 func_8003342C(void *data) {
    u32 *table = data;
    u32 i;

    for (i = 1; i <= table[0]; i++) {
        table[i] += (u32)data;
    }
    return table[0];
}

void func_80033474(void *data) {
    u32 *table = data;
    u32 i;

    for (i = 1; i <= table[0]; i++) {
        table[i] += (u32)data;
    }
}

u16 *func_800334B8(void) {
    return D_8005936C;
}

u8 *func_800334C8(void) {
    return D_80059368;
}

/* Release the font. */
void func_800334D8(void) {
    func_800320B8(D_8005936C);
    func_800320E8(D_8005936C);
    D_8005936C = NULL;
}

/* Release the system data. */
void func_80033518(void) {
    func_800320B8(D_80059368);
    func_800320E8(D_80059368);
    D_80059368 = NULL;
}

/* Install a loaded font block (protected from release): its header
 * halfwords are read in turn (glyph offset, then the character ranges). */
void func_80033558(u16 *font) {
    u16 *p;
    s32 offset;

    if (font == NULL) {
        func_800324B8(0x20);
        return;
    }
    func_800320A4(font);
    D_8005936C = font;
    D_8005935C = (u8 *)font;
    p = font + 1;
    offset = *p++;
    D_8005934C = *p++;
    D_80059350 = *p++;
    D_80059354 = *p++;
    D_80059358 = *p++;
    D_80059364 = *p;
    D_8005935C = (u8 *)font + offset;
}

/* Install a loaded system data block (protected from release). */
void func_800335F4(u8 *data) {
    if (data == NULL) {
        func_800324B8(0x20);
        return;
    }
    func_800320A4(data);
    D_80059368 = data;
    D_80059360 = (u8 **)data;
    func_8003342C(data);
    D_80059360++;
}

void func_80033668(u16 *font, u8 *data) {
    func_80033558(font);
    func_800335F4(data);
}

/* Upload the text palette to (x, y) and record its two CLUTs. */
void func_80033698(s16 x, s16 y) {
    RECT rect;

    rect.w = 32;
    rect.x = x;
    rect.y = y;
    rect.h = 1;
    LoadImage(&rect, (u_long *)D_80050190);
    D_800595D4 = GetClut(x, y);
    D_80059414 = GetClut(x + 16, y);
}

/* Entry `index` of a resource whose u16 offsets start at byte 4. */
u8 *func_80033728(u8 *resource, s32 index) {
    return resource + ((u16 *)resource)[index + 2];
}

/* First and second byte of entry `index` in a table after a header of
 * (count + 3) halfwords. */
u8 func_8003373C(u16 *table, s32 index) {
    u8 *entries = (u8 *)table;
    entries += *table * 2 + 6;
    entries += index * 2;
    return entries[0];
}

u8 func_80033760(u16 *table, s32 index) {
    u8 *entries = (u8 *)table;
    entries += *table * 2 + 6;
    entries += index * 2;
    return entries[1];
}

u8 *func_80033784(s32 table, s32 index) {
    return func_80033728(D_80059360[table], index);
}

u8 *func_800337B8(s32 index) {
    return func_80033728(D_80059360[16], index);
}

u8 *func_800337E8(s32 index) {
    return func_80033728(D_80059360[17], index);
}

u8 *func_80033818(s32 index) {
    return func_80033728(D_80059360[22], index);
}

u8 *func_80033848(s32 index) {
    return func_80033728(D_80059360[23], index);
}

u8 *func_80033878(s32 index) {
    return func_80033728(D_80059360[24], index);
}

u8 *func_800338A8(s32 index) {
    return func_80033728(D_80059360[25], index);
}

u8 *func_800338D8(s32 index) {
    return func_80033728(D_80059360[18], index);
}

u8 *func_80033908(s32 index) {
    return func_80033728(D_80059360[20], index);
}

u8 *func_80033938(s32 index) {
    return func_80033728(D_80059360[19], index);
}

u8 *func_80033968(s32 index) {
    return func_80033728(D_80059360[21], index);
}

u8 *func_80033998(s32 index) {
    return func_80033728(D_80059360[27], index);
}

u8 *func_800339C8(s32 table, s32 index) {
    return func_80033728(D_80059360[table + 28], index);
}

u8 *func_800339FC(s32 index) {
    return func_80033728(D_80059360[48], index);
}

u8 *func_80033A2C(s32 index) {
    return func_80033728(D_80059360[50], index);
}

u8 *func_80033A5C(s32 index) {
    return func_80033728(D_80059360[51], index);
}

u8 *func_80033A8C(s32 index) {
    return func_80033728(D_80059360[52], index);
}

/* Decode 0xFFFF-terminated character codes into text bytes (D_8005A0E4). */
void func_80033ABC(u16 *codes) {
    u8 *out = D_8005A0E4;
    CharPair *pairs = (CharPair *)D_80059360[27];
    CharPair *pair;
    u16 code;

    for (code = *codes; code != 0xFFFF; code = *codes) {
        pair = (CharPair *)(code * 2 + (u32)pairs);
        codes++;
        if (pair->first != 0) {
            *out++ = pair->first;
            *out++ = pair->second;
        } else {
            *out++ = pair->second;
        }
    }
    *out = 0;
}

/* Decode `count` character codes into text bytes at `out`. */
void func_80033B34(u16 *codes, u8 *out, u32 count) {
    CharPair *pairs = (CharPair *)D_80059360[27];
    CharPair *pair;

    while (count--) {
        pair = (CharPair *)(*codes * 2 + (u32)pairs);
        codes++;
        if (pair->first != 0) {
            *out++ = pair->first;
            *out++ = pair->second;
        } else {
            *out++ = pair->second;
        }
    }
    *out = 0;
}

/* Character code of a byte pair, or 0x8000 when there is none. */
s32 func_80033BAC(u8 first, u8 second) {
    CharPair *pairs = (CharPair *)D_80059360[27];
    CharPair *pair;
    s16 code;

    for (code = 0; code < 0x144; code++) {
        pair = (CharPair *)(code * 2 + (u32)pairs);
        if (pair->first == first && pair->second == second) {
            return code;
        }
    }
    return 0x8000;
}

/* Encode text into character codes. Returns -1 for a byte pair with no
 * code, else 0. */
s32 func_80033C20(u8 *text, u16 *codes) {
    u8 c;
    u8 first;
    u8 second;

    while ((c = *text++) != 0) {
        first = 0;
        if (c < D_8005934C) {
            second = c;
        } else {
            first = c;
            second = *text++;
        }
        *codes = func_80033BAC(first, second);
        if (*codes++ == 0x8000) {
            return -1;
        }
    }
    return 0;
}

u8 func_80033CD0(u8 *window) {
    return (*(u16 *)(window + 0x10) & 8) ? window[0x6B] : 0;
}

/* Decode `value` as ten decimal digit codes in palette `color` (with a
 * sign code when `sign` is set) into text; leading zeros are dropped for
 * plain palettes. The leading-zero scan tests its end first in an
 * unrotated loop, as the original does. */
void func_80033CF0(u32 value, s32 color, s32 sign) {
    u32 divisor = 1000000000;
    u32 remaining = value;
    u16 *p;
    s32 i;

    color <<= 4;
    if (sign != 0) {
        sign = 11;
        if ((s32)remaining < 0) {
            remaining = -remaining;
            sign = 10;
        }
    }
    for (i = 0; i < 10; i++) {
        D_8005A0C8[i + 1] = remaining / divisor + color;
        remaining %= divisor;
        divisor /= 10;
    }
    D_8005A0C8[11] = 0xFFFF;
    p = D_8005A0C8;
    D_8005A0C8[0] = color;
    if ((color & 0xFFF0) == color) {
        while (1) {
            if (p == &D_8005A0C8[10]) {
                break;
            }
            if (*++p != color) {
                break;
            }
        }
    }
    if (sign != 0) {
        *--p = sign + color;
    }
    func_80033ABC(p);
}

void func_80033DD4(Window *window, u8 *text) {
    u8 *previous = window->text;

    window->text = text;
    window->resume = previous;
    window->flags |= 0x80;
}

/* Reveal a window's next text bytes. Line images alternate between two glyph
 * planes; text controls pause, change reveal speed, insert resource/name/number
 * text and return to the byte after an inserted message's saved position.
 * Pointer increments below retain the control stream's original resume slots.
 * The control parameter reuses `first`. Resource controls resolve their
 * entries before sharing the text insertion and budget update. */
void func_80033DF0(Window *window) {
    s32 remaining = window->unk69;
    s32 line_slot;
    s32 current_line;
    u16 first;
    u16 second;
    u16 glyph_width;
    u8 byte;
    u8 old_speed;
    s32 text_bytes;
    s32 category;
    s32 palette;
    s32 sign;
    u8 *resource;
    u8 *cursor;
    s32 index;
    RECT unused; /* unused in the original; reserves 8 bytes */

    if (window->x > window->width) {
        window->x = 0;
        window->y++;
        window->unk18++;
        if (window->y >= window->lines) {
            window->y = 0;
            window->flags |= 1;
        }
        if (window->flags & 1) {
            window->layout[window->unk16].width = 0;
            if (++window->unk16 >= window->lines) {
                window->unk16 = 0;
            }
        }
        current_line = window->y;
        line_slot = window->unk18 % (window->lines + 1);
        window->layout[current_line].row = (line_slot / 2) * 13 + window->unkE;
        window->layout[current_line].clut = !(line_slot & 1) ? D_800595D4 : D_80059414;
        window->layout[current_line].plane = line_slot & 1;
        window->layout[current_line].slot = line_slot;
        window->layout[current_line].rect.y = window->unkE + (line_slot / 2) * 13;
    }
    remaining--;
    if (window->unk6C != 0) {
        window->unk6C = 0;
        window->flags &= ~4;
        return;
    }
    while (remaining != -1) {
        byte = *window->text;
        first = byte;
        /* 00 end(), 1 byte: end of text: return after an inserted text's
         * control; else state 1 (unk6B), flag 8 (wait) and unk6C, so once the
         * wait ends the window moves on to its next queued text. The pointer
         * stays on the 00. */
        if (first == 0) {
            if (window->flags & 0x80) {
                window->flags &= ~0x80;
                window->text = window->resume + 1;
                goto next_byte;
            }
            window->flags |= 8;
            window->unk6B = 1;
            window->unk6C = 1;
            return;
        }
        /* 03 pause(), 1 byte: state 3, flag 8: wait, keeping the lines. */
        if (first == 3) {
            window->unk6B = 3;
            window->flags |= 8;
            window->text++;
            return;
        }
        /* 0F nn: sub-code nn through the jump table at 80018a7c (cases 0-15); 16
         * and up match no case and leave the pointer on the 0F. */
        if (first == 15) {
            switch (window->text[1]) {
            /* 0F 00 wait(frames), 3 bytes: wait `frames` (unk84) and end this
             * step. */
            case 0:
                window->unk84 = window->text[2];
                window->text += 3;
                return;
            /* 0F 01 speed(speed), 3 bytes: glyphs per step = speed, saving the
             * old speed; 0 restores the saved speed. */
            case 1:
                first = window->text[2];
                if (first != 0) {
                    remaining += first;
                    window->unk6A = window->unk68;
                    window->unk68 = first;
                    window->unk69 = first;
                } else {
                    window->unk68 = window->unk6A;
                    window->unk69 = window->unk6A;
                    window->unk6A = 0;
                }
                window->text += 3;
                break;
            /* 0F 02 wait_done(frames), 3 bytes: wait `frames`; the following
             * step only clears window flag 4 (unk6C), so the window takes its
             * next queued text. */
            case 2:
                window->unk84 = window->text[2];
                window->text += 3;
                window->unk6C = 1;
                return;
            /* 0F 03 insert(resource, entry), 4 bytes: insert entry `entry` of
             * system resource `resource`. */
            case 3:
                first = window->text[2];
                second = window->text[3];
                window->text += 3;
                resource = D_80059360[first];
                remaining++;
                resource = func_80033728(resource, second);
                goto resource_ready;
            /* 0F 04 insert_selection(), 2 bytes: insert entry selection & 0xFF
             * of resource 22, 23, 17, 51 or 50 for selection kinds 0x000-0x400
             * (another kind: the 04 is read as text). */
            case 4:
                cursor = window->text;
                cursor++;
                category = window->selection;
                second = category;
                category &= 0xFF00;
                window->text = cursor;
                switch (category) {
                case 0x000:
                    resource = D_80059360[22];
                    second &= 0xFF;
                    resource = func_80033728(resource, second);
                    goto resource_ready;
                case 0x100:
                    resource = D_80059360[23];
                    second &= 0xFF;
                    resource = func_80033728(resource, second);
                    goto resource_ready;
                case 0x200:
                    resource = D_80059360[17];
                    second &= 0xFF;
                    resource = func_80033728(resource, second);
                    goto resource_ready;
                case 0x300:
                    resource = D_80059360[51];
                    second &= 0xFF;
                    resource = func_80033728(resource, second);
                    goto resource_ready;
                case 0x400:
                    resource = D_80059360[50];
                    second &= 0xFF;
                    resource = func_80033728(resource, second);
                    goto resource_ready;
                }
                break;
            /* 0F 05 insert_name(name), 3 bytes: insert character name `name`
             * (0x80 and up: the party member's; slot 0xFF: resource 26 entry 0). */
            case 5:
                first = window->text[2];
                window->text += 2;
                index = first;
                if (first >= 0x80) {
                    index = D_8006D634.party[first - 0x80];
                    if (index == 0xFF) {
                        func_80033DD4(window, func_80033728(D_80059360[26], 0));
                    } else {
                        func_80033DD4(window, D_8006D634.names[index]);
                    }
                } else {
                    func_80033DD4(window, D_8006D634.names[index]);
                }
                remaining++;
                break;
            /* 0F 06 insert_23(entry), 3 bytes: insert entry `entry` of system
             * resource 23. */
            case 6:
                remaining++;
                first = window->text[2];
                window->text += 2;
                resource = D_80059360[23];
                resource = func_80033728(resource, first);
                goto resource_ready;
            /* 0F 07 insert_24(entry), 3 bytes: insert entry `entry` of system
             * resource 24. */
            case 7:
                remaining++;
                first = window->text[2];
                window->text += 2;
                resource = D_80059360[24];
                resource = func_80033728(resource, first);
                goto resource_ready;
            /* 0F 08 insert_25(entry), 3 bytes: insert entry `entry` of system
             * resource 25. */
            case 8:
                remaining++;
                first = window->text[2];
                window->text += 2;
                resource = D_80059360[25];
                resource = func_80033728(resource, first);
                goto resource_ready;
            /* 0F 09 insert_number(value), 3 bytes: insert window value `value`
             * in decimal, palette 0. */
            case 9:
                palette = 0;
                sign = 0;
                cursor = window->text;
                goto insert_number;
            /* 0F 0A insert_number_1(value), 3 bytes: insert window value `value`
             * in decimal, palette 1. */
            case 10:
                palette = 1;
                cursor = window->text;
                sign = 0;
                goto insert_number;
            /* 0F 0B set_6d(value), 2 bytes: window byte 0x6D = value; the
             * pointer stops on the operand, which is read again as text. */
            case 11:
                window->unk6D = window->text[2];
                window->text += 2;
                break;
            /* 0F 0C insert_signed(value), 3 bytes: insert window value `value`
             * as signed decimal, palette 1. */
            case 12:
                palette = 1;
                cursor = window->text;
                sign = 1;
insert_number:
                first = cursor[2];
                cursor += 2;
                window->text = cursor;
                remaining++;
                func_80033CF0(window->values[first], palette, sign);
                func_80033DD4(window, D_8005A0E4);
                break;
            /* 0F 0D wait_done_skippable(frames), 3 bytes: as wait_done, also
             * setting window flag 0x200 (800345e0 then drops the wait and the
             * pending clear). */
            case 13:
                window->unk84 = window->text[2];
                window->text += 3;
                window->unk6C = 1;
                window->flags |= 0x200;
                return;
            /* 0F 0E slow(frames), 3 bytes: one glyph per step every `frames`
             * frames (unk86/unk88), saving the speed. */
            case 14:
                old_speed = window->unk68;
                window->unk68 = 1;
                window->unk69 = 1;
                window->unk6A = old_speed;
                window->unk86 = window->unk88 = window->text[2];
                window->text += 3;
                return;
            /* 0F 0F insert_button(action), 3 bytes: insert the name of the
             * button assigned to `action` (D_80050238) from resource 49. */
            case 15:
                first = window->text[2];
                window->text += 2;
                resource = D_80059360[49];
                second = D_80050238[first];
                remaining++;
                resource = func_80033728(resource, second);
resource_ready:
                remaining--;
                func_80033DD4(window, resource);
                goto check_budget;
            }
        } else if (first == 2) {
            /* 02 page(), 1 byte: state 2, flags 0x48: wait, then clear the
             * window; a following 01 is skipped. */
            window->unk6B = 2;
            window->flags |= 0x48;
            window->text++;
            if (*window->text == 1) {
                window->text++;
            }
            return;
        } else if (first == 1) {
            /* 01 newline(), 1 byte: x = 100 and end this step, so the next step
             * starts a new line. */
            window->x = 100;
            window->text++;
            return;
        } else {
            /* A glyph: one byte below D_8005934C (the font's two-byte
             * threshold), else that byte and the next. */
            text_bytes = 1;
            if (first < D_8005934C) {
                first = 0;
                second = byte;
            } else {
                second = window->text[1];
                text_bytes = 2;
            }
            glyph_width = func_80034F98(first, second);
            if (window->x + glyph_width > window->width) {
                window->x += glyph_width;
                return;
            }
            func_80034FFC(first, second, (u16 *)window->image + window->x,
                         window->stride, window->layout[window->y].plane);
            window->text = (u8 *)(text_bytes + (u32)window->text);
            window->x += glyph_width;
            window->layout[window->y].width = window->x;
        }
next_byte:
        remaining--;
check_budget:
        ;
    }
}

/* Clear flag 8; a window with flag 0x200 also drops its pending state. */
void func_800345E0(Window *window) {
    u16 flags = window->flags;

    window->flags = flags & ~8;
    if (flags & 0x200) {
        window->unk84 = 0;
        window->unk6C = 0;
        window->flags &= ~0x200;
    }
}

/* Unless it is busy, reset a window to flag 2 only. */
void func_80034614(Window *window) {
    if (window->unk84 == 0) {
        window->unk6C = 0;
        window->flags &= 2;
    }
}

/* Unless it is busy, release a window's queued messages. */
void func_8003463C(Window *window) {
    WindowQueue *entry;
    WindowQueue *current;

    if (window->unk84 == 0) {
        entry = window->queue;
        while (entry != NULL) {
            current = entry;
            entry = entry->next;
            func_800320E8(current);
        }
        window->queue = NULL;
        window->queued = 0;
    }
}

/* Reset a window and release its queue. */
void func_800346A4(Window *window) {
    window->unk6C = 0;
    window->unk84 = 0;
    window->flags &= 2;
    func_8003463C(window);
}

/* Close a window: reset it and release its layout and image. */
void func_800346D4(Window *window) {
    func_800346A4(window);
    func_800320E8(window->layout);
    func_800320E8(window->image);
}

/* Queue `message` after the window's current one. Returns the queue
 * length. */
s16 func_80034714(Window *window, s32 message) {
    WindowQueue *last = window->queue;
    WindowQueue *entry;

    window->queued++;
    func_800324B8(0x2A);
    entry = func_80031BDC(sizeof(WindowQueue), 2);
    entry->message = message;
    entry->next = NULL;
    if (last == NULL) {
        window->queue = entry;
        return window->queued;
    }
    while (last->next != NULL) {
        last = last->next;
    }
    last->next = entry;
    return window->queued;
}

s32 func_800347AC(Window *window) {
    return window->unk4 + window->x * 4;
}

/* Image row of the cursor line (wrapping to the last line). */
s32 func_800347C0(Window *window) {
    s32 row = window->y - window->unk16;

    if (row < 0) {
        row = window->lines - 1;
    }
    return window->unk6 + row * window->unk14;
}

/* Set the colour of every line's sprites. */
void func_80034800(Window *window, u8 r, u8 g, u8 b) {
    WindowLine *line;
    s32 i;

    for (i = 0; i < window->lines; i++) {
        line = &window->layout[i];
        line->sprite[0][0].r0 = line->sprite[0][1].r0 = line->sprite[1][0].r0 = line->sprite[1][1].r0 = r;
        line->sprite[0][0].g0 = line->sprite[0][1].g0 = line->sprite[1][0].g0 = line->sprite[1][1].g0 = g;
        line->sprite[0][0].b0 = line->sprite[0][1].b0 = line->sprite[1][0].b0 = line->sprite[1][1].b0 = b;
    }
}

void func_80034874(Window *window, u8 value) {
    window->unk6E = value;
}

void func_8003487C(Window *window) {
    window->unk6E = 0xFF;
}

/* Draw a window into `ot` for draw buffer `buffer`: start its next queued
 * message when the current one is done, link each line's two sprites
 * (from the first shown line, the highlighted line lit), reveal the next
 * glyphs when the wait is over and link its background. */
void func_80034888(Window *window, u_long *ot, s32 buffer) {
    WindowQueue *entry;
    s32 i;
    s32 line;

    if (!(window->flags & 4)) {
        if (window->queued == 0) {
            return;
        }
        entry = window->queue;
        window->text = (u8 *)entry->message;
        window->queue = window->queue->next;
        func_800320E8(entry);
        window->queued--;
        window->flags = (window->flags & 2) | 0x24;
        if (window->unk6A != 0) {
            window->unk68 = window->unk6A;
            window->unk69 = window->unk6A;
            window->unk6A = 0;
        }
        window->unk88 = 0;
        window->unk86 = 0;
        window->unk69 = window->unk68;
    }
    if (window->flags & 0x100) {
        window->unk69 = window->unk68 * 3;
    } else {
        window->unk69 = window->unk68;
    }
    if (window->flags & 0x40) {
        if (!(window->flags & 8)) {
            window->flags = (window->flags & ~0x40) | 0x20;
        }
    }
    if (window->flags & 0x20) {
        window->unk16 = 0;
        window->unk18 = 0;
        window->y = 0;
        window->x = 0;
        window->layout[0].row = window->unkE;
        window->layout[0].clut = D_800595D4;
        window->layout[0].plane = 0;
        window->layout[0].rect.y = window->unkE;
        for (line = 0; line < window->lines; line++) {
            window->layout[line].width = 0;
        }
        window->flags &= ~0x21;
    }

    line = window->unk16;
    for (i = 0; i < window->lines; line++, i++) {
        if (line >= window->lines) {
            line = 0;
        }
        setShadeTex(&window->layout[line].sprite[buffer][1], window->unk6E != i);
        if (window->layout[line].width > 0x40) {
            window->layout[line].sprite[buffer][1].v0 = window->layout[line].row;
            window->layout[line].sprite[buffer][1].clut = window->layout[line].clut;
            window->layout[line].sprite[buffer][1].y0 = window->unk6 + window->unk14 * i;
            window->layout[line].sprite[buffer][1].w = (window->layout[line].width - 0x40) * 4;
            func_80031798(ot, &window->layout[line].sprite[buffer][1]);
        }
    }
    AddPrim(ot, window->unk3C);
    line = window->unk16;
    for (i = 0; i < window->lines; line++, i++) {
        if (line >= window->lines) {
            line = 0;
        }
        setShadeTex(&window->layout[line].sprite[buffer][0], window->unk6E != i);
        if (window->layout[line].width != 0) {
            window->layout[line].sprite[buffer][0].v0 = window->layout[line].row;
            window->layout[line].sprite[buffer][0].clut = window->layout[line].clut;
            window->layout[line].sprite[buffer][0].y0 = window->unk6 + window->unk14 * i;
            if (window->layout[line].width > 0x40) {
                window->layout[line].sprite[buffer][0].w = 0x100;
            } else {
                window->layout[line].sprite[buffer][0].w = window->layout[line].width * 4;
            }
            func_80031798(ot, &window->layout[line].sprite[buffer][0]);
        }
    }

    if (window->unk84 != 0) {
        window->unk84--;
    } else if (window->unk86 != 0) {
        window->unk86--;
    } else {
        window->unk86 = window->unk88;
        if (!(window->flags & 0x58)) {
            func_80033DF0(window);
            LoadImage(&window->layout[window->y].rect, window->image);
        }
    }
    if (window->unk84 != 0) {
        if (--window->unk84 == -1) {
            window->flags &= ~0x10;
        }
    }
    if (!(window->flags & 2)) {
        *(u32 *)&window->tile[buffer].x0 = (window->unk4 - 7) | ((window->unk6 - 5) << 16);
        *(u32 *)&window->tile[buffer].w = ((s16)(window->width | 1) * 4 + 0xD) |
                                          ((window->lines * window->unk14 + 10) << 16);
        addPrim(ot, &window->tile[buffer]);
    }
    window->flags &= ~0x100;
    AddPrim(ot, window->unk30);
}

/* Lay out one line of `text` into `image` in the layout window, `width`
 * made odd. Returns the laid-out width in pixels. */
s32 func_80034EAC(u8 *text, void *image, s16 width, s32 flags) {
    Window *window = &D_80059FD8;

    window->width = width;
    width |= 1;
    window->lines = 1;
    window->width = width;
    window->unk8 = width * 4;
    window->stride = width + 3;
    window->text = text;
    window->unk68 = 1;
    window->unk84 = 0;
    window->unk6C = 0;
    window->unk6A = 0;
    window->image = image;
    window->flags = 0;
    window->y = 0;
    window->x = 0;
    window->unk69 = 100;
    window->layout = &D_8005A068;
    D_8005A068.width = 0;
    D_8005A068.plane = flags & 1;
    func_80033DF0(window);
    return window->layout->width * 4;
}

/* Draw class of a character: 2 for a narrow glyph, else 3. */
s32 func_80034F98(u16 first, u16 second) {
    if (first == 0) {
        if ((s32)((u32)second - (u32)D_80059364) < D_80059354) {
            return 2;
        }
        return 3;
    }
    if (first == D_8005934C && second < D_80059358) {
        return 2;
    }
    return 3;
}

/* Expand the font's twelve bits per row into the window's packed 4-bit
 * pixels. Each glyph has eleven rows; its three-row stencil also sets the
 * neighbouring pixels. The two glyph planes occupy opposite two-bit pairs
 * in each nibble, so clearing/drawing one preserves the other.
 *
 * Each row ORs an outline word into the rows above and below and the drawn
 * pixels with their outline into its own row. `carry` is the outline that a
 * column's first pixel and the previous column's last two pixels spread
 * into the column. `first` arrives as a full word; each use takes its low
 * half. */
#define DRAW_GLYPH_PLANE(keep, shift) do { \
    image[0] &= keep; \
    image[1] &= keep; \
    image[2] &= keep; \
    image += stride; \
    image[0] &= keep; \
    image[1] &= keep; \
    image[2] &= keep; \
    do { \
        u16 *upper = &image[-stride]; \
        u16 *lower = &image[stride]; \
        lower[0] &= keep; \
        lower[1] &= keep; \
        lower[2] &= keep; \
        bits = *glyph++; \
        edge = -((bits & 0x80) != 0) & (0x222 << shift); \
        if (bits & 0x40) edge |= 0x2220 << shift; \
        if (bits & 0x20) edge |= 0x2200 << shift; \
        if (!(bits & 0x10)) spread = edge; \
        else spread = edge | (0x2000 << shift); \
        upper[0] |= spread; \
        centre = -((bits & 0x80) != 0) & (0x212 << shift); \
        lower[0] |= spread; \
        if (bits & 0x40) centre |= 0x2120 << shift; \
        edge = centre; \
        if (bits & 0x20) edge |= 0x1200 << shift; \
        previous = image[0]; \
        if (bits & 0x10) image[0] = previous | (0x2000 << shift) | edge; \
        else image[0] = previous | edge; \
        if (!(bits & 8)) { \
            if (!(bits & 0x10)) carry = (bits >> (4 - shift)) & (2 << shift); \
            else carry = 0x22 << shift; \
        } else carry = 0x222 << shift; \
        edge = carry; \
        if (bits & 4) edge |= 0x2220 << shift; \
        if (bits & 2) edge |= 0x2200 << shift; \
        if (!(bits & 1)) spread = edge; \
        else spread = edge | (0x2000 << shift); \
        upper[1] |= spread; \
        lower[1] |= spread; \
        edge = (bits >> (4 - shift)) & (2 << shift); \
        if (bits & 0x10) edge |= 0x21 << shift; \
        if (bits & 8) edge |= 0x212 << shift; \
        if (bits & 4) edge |= 0x2120 << shift; \
        if (bits & 2) edge |= 0x1200 << shift; \
        previous = image[1]; \
        if (bits & 1) image[1] = previous | (0x2000 << shift) | edge; \
        else image[1] = previous | edge; \
        if (!(bits & 0x8000)) { \
            if (!(bits & 1)) carry = (bits << shift) & (2 << shift); \
            else carry = 0x22 << shift; \
        } else carry = 0x222 << shift; \
        edge = carry; \
        if (bits & 0x4000) edge |= 0x2220 << shift; \
        if (bits & 0x2000) edge |= 0x2200 << shift; \
        if (!(bits & 0x1000)) spread = edge; \
        else spread = edge | (0x2000 << shift); \
        upper[2] |= spread; \
        lower[2] |= spread; \
        edge = (bits << shift) & (2 << shift); \
        if (bits & 1) edge |= 0x21 << shift; \
        if (bits & 0x8000) edge |= 0x212 << shift; \
        if (bits & 0x4000) edge |= 0x2120 << shift; \
        if (bits & 0x2000) edge |= 0x1200 << shift; \
        previous = image[2]; \
        if (bits & 0x1000) image[2] = previous | (0x2000 << shift) | edge; \
        else image[2] = previous | edge; \
        image += stride; \
        row++; \
    } while (row < 11); \
} while (0)

void func_80034FFC(s32 first, u16 second, u16 *image, s16 stride, s32 plane) {
    u16 *glyph;
    u16 bits;
    u16 previous;
    s32 edge;
    s32 spread;
    s32 centre;
    s32 carry;
    s32 row;

    if ((u16)first == 0) {
        glyph = (u16 *)(D_8005935C + (second - D_80059364) * 22);
    } else if ((u16)first == 0xFF && second == 0xFF) {
        glyph = D_800501D0;
    } else {
        glyph = (u16 *)(D_8005935C + second * 22 + D_80059350 +
                       ((u16)first - D_8005934C) * 0x1600);
    }
    row = 0;
    if (plane == 0) {
        DRAW_GLYPH_PLANE(0xCCCC, 0);
    } else {
        DRAW_GLYPH_PLANE(0x3333, 2);
    }
}

#undef DRAW_GLYPH_PLANE


/* Buttons held on controller `port` (active high), or 0 without a digital
 * or analog pad. */
s32 func_8003569C(s32 port) {
    PadBuffer *pad = &D_800625FC[port];

    D_80059388 = 0;
    if (pad->status != 0) {
        return 0;
    }
    D_80059388 = pad->type & 0xF0;
    if (D_80059388 == 0x40 || D_80059388 == 0x50 || D_80059388 == 0x70) {
        return (u8)~pad->buttons[1] | ((pad->buttons[0] << 8) ^ 0xFF00);
    }
    return 0;
}

/* Kind of controller on `port`: 0 none, 1 digital, 2 mouse, 3 analog stick,
 * 4 analog pad, -1 other. */
s32 func_80035734(s32 port) {
    if (D_800625FC[port].status == 0xFF) {
        return 0;
    }
    switch (D_800625FC[port].type & 0xF0) {
    case 0x40:
        return 1;
    case 0x10:
        return 2;
    case 0x50:
        return 3;
    case 0x70:
        return 4;
    }
    return -1;
}

/* Remap the low button byte through the configured assignment. */
s16 func_800357C0(s32 buttons) {
    s32 held = buttons;
    s32 i;

    buttons &= 0xFF00;
    for (i = 0; i < 8; i++) {
        if (held & D_800501E8[i]) {
            buttons |= D_800501E8[D_80050238[i]];
        }
    }
    return buttons;
}

/* Swap the shoulder and face button bits between the two layouts. */
s16 func_8003582C(s32 buttons) {
    s16 result = buttons & ~0x9E;

    if (buttons & 8) {
        result |= 0x10;
    }
    if (buttons & 2) {
        result |= 4;
    }
    if (buttons & 0x10) {
        result |= 8;
    }
    if (buttons & 0x80) {
        result |= 2;
    }
    if (buttons & 4) {
        result |= 0x80;
    }
    return result;
}

u8 func_80035884(s32 buttons) {
    return D_8005020C[(buttons >> 12) & 0xF];
}

u8 func_800358A0(s32 buttons) {
    return D_8005021C[(buttons >> 12) & 0xF];
}

extern u16 D_80059574;       /* held pad buttons, second port */
extern u16 D_80059490;       /* pad buttons pressed, second port */
extern u16 D_800594A8;       /* pad buttons repeated, second port */
extern s32 D_80059488;       /* vertical blank count */
extern u8 D_80059444, D_8005944C, D_80059430, D_80059438; /* sticks, first port */
extern u8 D_80059448, D_80059450, D_80059434, D_8005943C; /* sticks, second port */

/* Read both controllers: held buttons (remapped; an analog stick's layout
 * swapped), the stick positions (the directional buttons' on a digital
 * pad), newly pressed buttons and the auto-repeating buttons (the newly
 * pressed ones; once the held buttons have not changed for 32 frames, all
 * held buttons every fourth frame). */
void func_800358BC(void) {
    D_80059570 = func_8003569C(0);
    D_80059570 = func_800357C0((s16)D_80059570);
    if (D_80059388 != 0) {
        if (D_80059388 == 0x50) {
            D_80059570 = func_8003582C((s16)D_80059570);
            goto analog0;
        }
        if (D_80059388 == 0x70) {
        analog0:
            D_80059444 = D_800625FC[0].data[0];
            D_8005944C = D_800625FC[0].data[1];
            D_80059430 = D_800625FC[0].data[2];
            D_80059438 = D_800625FC[0].data[3];
        } else {
            D_8005944C = 0;
            D_80059444 = 0;
            D_80059430 = D_8005020C[D_80059570 >> 12];
            D_80059438 = D_8005021C[D_80059570 >> 12];
        }
    } else {
        D_8005944C = 0;
        D_80059444 = 0;
        D_80059438 = 0;
        D_80059430 = 0;
    }
    D_8005948C = D_80059570 ^ D_80059374;
    D_8005948C &= D_80059570;
    D_80059374 = D_80059570;
    if (D_8005948C) {
        D_8005022C = 0;
    }
    D_800594A4 = D_80059570;
    if (D_8005022C < 0x20) {
        D_8005022C++;
        D_800594A4 = D_8005948C;
    } else if (D_80059488 & 3) {
        D_800594A4 = D_8005948C;
    }

    D_80059574 = func_8003569C(1);
    D_80059574 = func_800357C0((s16)D_80059574);
    if (D_80059388 != 0) {
        if (D_80059388 == 0x50) {
            D_80059574 = func_8003582C((s16)D_80059574);
            goto analog1;
        }
        if (D_80059388 == 0x70) {
        analog1:
            D_80059448 = D_800625FC[1].data[0];
            D_80059450 = D_800625FC[1].data[1];
            D_80059434 = D_800625FC[1].data[2];
            D_8005943C = D_800625FC[1].data[3];
        } else {
            D_80059450 = 0;
            D_80059448 = 0;
            D_80059434 = D_8005020C[D_80059574 >> 12];
            D_8005943C = D_8005021C[D_80059574 >> 12];
        }
    } else {
        D_80059450 = 0;
        D_80059448 = 0;
        D_8005943C = 0;
        D_80059434 = 0;
    }
    D_80059490 = D_80059574 ^ D_80059378;
    D_80059490 &= D_80059574;
    D_80059378 = D_80059574;
    if (D_80059490) {
        D_80050230 = 0;
    }
    D_800594A8 = D_80059574;
    if (D_80050230 < 0x20) {
        D_80050230++;
        D_800594A8 = D_80059490;
    } else if (D_80059488 & 3) {
        D_800594A8 = D_80059490;
    }
}

/* Queue the current controller state (flag an overflow when full). */
void func_80035C0C(void) {
    s32 i;

    if (D_8005937C < 16) {
        D_8005937C++;
        i = D_80059380 & 0xF;
        D_8005A0FC[i] = D_80059570;
        D_8005A11C[i] = D_80059574;
        D_8005A13C[i] = D_8005948C;
        D_8005A15C[i] = D_80059490;
        D_8005A17C[i] = D_800594A4;
        D_8005A19C[i] = D_800594A8;
        D_80059380++;
        return;
    }
    D_80050208 = 1;
}

/* Take the oldest queued controller state as the current one. Returns the
 * count before, 0 when empty. */
u32 func_80035CDC(void) {
    u32 count = D_8005937C;
    s32 i;

    if (count == 0) {
        return 0;
    }
    D_8005937C = count - 1;
    i = D_80059384 & 0xF;
    D_80059384++;
    D_80059570 = D_8005A0FC[i];
    D_80059574 = D_8005A11C[i];
    D_8005948C = D_8005A13C[i];
    D_80059490 = D_8005A15C[i];
    D_800594A4 = D_8005A17C[i];
    D_800594A8 = D_8005A19C[i];
    return count;
}

/* Number of queued controller states. */
u32 func_80035DA0(void) {
    return D_8005937C;
}

extern u16 D_800594DC;
extern u16 D_800594E0;
extern u16 D_800594E8;
extern u16 D_800594EC;
extern u16 D_800595C8;
extern u16 D_800595CC;

/* Clear the controller queue and states. */
void func_80035DB0(void) {
    D_8005937C = 0;
    D_80059380 = 0;
    D_80059384 = 0;
    D_80050208 = 0;
    D_80050200 = 1;
    D_800594EC = 0;
    D_800594E8 = 0;
    D_800594E0 = 0;
    D_800594DC = 0;
    D_800595CC = 0;
    D_800595C8 = 0;
    D_800594A8 = 0;
    D_800594A4 = 0;
    D_80059490 = 0;
    D_8005948C = 0;
    D_80059574 = 0;
    D_80059570 = 0;
}

/* Advance the play time by one frame. */
void func_80035E44(void) {
    if (D_800501F8 == 0) {
        if (++D_80059370 == 60) {
            D_80059370 = 0;
            D_80059418++;
        }
        if (D_80059418 == 60) {
            D_80059418 = 0;
            D_80059420++;
        }
        if (D_80059420 == 60) {
            D_80059420 = 0;
            D_80059484++;
        }
        if (D_80059484 == 100) {
            D_800501F8 = 1;
        }
    }
}

/* Save a VRAM rectangle as a 16-bit TIM file on the PC file server. */
void func_80035F1C(RECT *rect, char *name) {
    TimHeader header;
    s32 fd;

    StoreImage(rect, (u_long *)0x80700000);
    header.id = 0x10;
    header.flag = 2;
    header.bytes = rect->w * rect->h * 2 + 12;
    header.rect.x = rect->x;
    header.rect.y = rect->y;
    header.rect.w = rect->w;
    header.rect.h = rect->h;
    fd = PCcreat(name, 0);
    func_8004C470(fd, (char *)&header, sizeof(header));
    func_8004C470(fd, (char *)0x80700000, rect->w * rect->h * 2);
    PCclose(fd);
}

/* Save a VRAM rectangle as a PPM (P6) image on the PC file server.
 * Returns 0, or -1 when the file cannot be created. */
s32 func_80035FF8(RECT *rect, char *name) {
    char header[256];
    u16 *src;
    u8 *dst;
    s32 count;
    s32 i;
    s32 fd;

    StoreImage(rect, (u_long *)0x80600000);
    DrawSync(0);
    sprintf(header, "P6\r%d %d\r255\r", rect->w, rect->h);
    count = rect->w * rect->h;
    src = (u16 *)0x80600000;
    dst = (u8 *)0x80700000;
    i = count;
    while (i--) {
        *dst++ = (*src & 0x1F) << 3;
        *dst++ = (*src >> 2) & 0xF8;
        *dst++ = (*src++ >> 7) & 0xF8;
    }
    fd = PCcreat(name, 0);
    if (fd == -1) {
        return -1;
    }
    func_8004C470(fd, header, strlen(header));
    func_8004C470(fd, (char *)0x80700000, count * 3);
    PCclose(fd);
    return 0;
}

/* Stop both controllers' actuators and register their data with libpad. */
void func_8003611C(void) {
    D_8005A1BC[0].act[0] = 0;
    D_8005A1BC[0].timer = 0;
    D_8005A1BC[0].state = 0;
    D_8005A1BC[0].disabled = 0;
    D_8005A1BC[1] = D_8005A1BC[0];
    func_80040C3C(D_8005A1BC[0].act, 4, D_8005A1BC[1].act, 4);
}

/* Step one actuator: run while its timer lasts, then wind down. */
void func_80036188(Actuator *actuator) {
    if (actuator->disabled == 0) {
        if (actuator->timer != 0) {
            actuator->act[0] = 1;
            actuator->act[1] = 0x40;
            actuator->act[2] = 1;
            actuator->act[3] = 0;
            actuator->state = 1;
            actuator->timer--;
        } else if (actuator->state == 1) {
            actuator->act[0] = 1;
            actuator->act[1] = 0x40;
            actuator->act[2] = 0;
            actuator->act[3] = 0;
            actuator->state = 2;
        } else if (actuator->state == 2) {
            actuator->act[0] = 0;
            actuator->state = 0;
        }
    }
}

void func_80036220(void) {
    func_80036188(&D_8005A1BC[0]);
    func_80036188(&D_8005A1BC[1]);
}

/* Run the actuator of `port` for `frames` frames. */
void func_80036258(s32 port, s16 frames) {
    D_8005A1BC[port].timer = frames;
}

void func_80036270(s32 port, u8 disabled) {
    D_8005A1BC[port].disabled = disabled;
}

/* Start the controllers and reset the queue, actuators and assignment. */
void func_80036288(void) {
    u8 *entry;
    s32 i;

    InitPAD((char *)&D_800625FC[0], 0x22, (char *)&D_800625FC[1], 0x22);
    StartPAD();
    ChangeClearPAD(0);
    func_80035DB0();
    func_8003611C();
    D_80050204 = 0;
    D_8005938C = 1;
    D_80059390 = 0;
    for (i = 7, entry = &D_80050238[7]; i >= 0; i--) {
        *entry-- = i;
    }
    D_80050238[0] = 1;
    D_80050238[2] = 3;
    D_80050238[1] = 0;
    D_80050238[3] = 2;
}

void func_8003633C(u8 value) {
    D_8005938C = value;
}

/* Vertical-blank callback: counts frames, polls the controllers, input queue
 * and play clock, runs the installed hook, and on a development (host)
 * configuration with the debugger request set traps into the debugger.
 * The frame holds 40 bytes of locals that the code never touches. */
void func_8003634C(void) {
    u8 unused[40];

    D_80059488++;
    func_800358BC();
    func_80035C0C();
    func_80035E44();
    func_80036220();
    if (D_800501FC != NULL) {
        D_800501FC();
    }
    if (D_80010000 != -1 && D_80059390 != 0) {
        pollhost();
    }
}

void func_800363E0(s32 value) {
    D_80059390 = value;
}

void func_800363F0(void (*value)(void)) {
    D_800501FC = value;
}

void func_80036400(s32 value) {
    D_80050200 = value;
}

s32 func_80036410(void) {
    return D_80050208;
}

/* Merge every queued controller state into the current one (or reset
 * after an overflow). */
void func_80036420(void) {
    u16 s0, s1, s2, s3, s4, s5;

    s0 = s1 = s2 = s3 = s4 = s5 = 0;

    if (func_80036410() != 0) {
        func_80035DB0();
    } else {
        while (func_80035CDC() != 0) {
            s0 |= D_80059570;
            s1 |= D_80059574;
            s2 |= D_8005948C;
            s3 |= D_80059490;
            s4 |= D_800594A4;
            s5 |= D_800594A8;
        }
    }
    D_80059570 = s0;
    D_80059574 = s1;
    D_8005948C = s2;
    D_80059490 = s3;
    D_800594A4 = s4;
    D_800594A8 = s5;
}

/* Print a controller receive buffer in hex, and a digital pad's buttons. */
void func_80036528(PadBuffer *pad) {
    s32 count = (pad->type & 0xF) * 2 + 2;
    s32 i;

    for (i = 0; i < count; i++) {
        func_8003700C("%02x ", ((u8 *)pad)[i]);
    }
    func_8003700C("\n");
    if (pad->status == 0 && (pad->type & 0xF0) == 0x40) {
        func_8003700C("%04x\n", (~pad->buttons[1] & 0xFF) | ((pad->buttons[0] << 8) ^ 0xFF00));
    }
}

/* Print both controller buffers, the actuator values, the held buttons and
 * every queued pad entry. The final format occupies a full word-aligned
 * slot before the formatter's digit tables. */
void func_800365FC(void) {
    func_80036528(&D_800625FC[0]);
    func_80036528(&D_800625FC[1]);
    func_8003700C("vect0 %02x %02x\n", D_80059430, D_80059438);
    func_8003700C("vect1 %02x %02x\n", D_80059434, D_8005943C);
    func_8003700C("PADD %04x %04x\n", D_80059570, D_80059574);
    while (func_80035CDC() != 0) {
        static const char queued_format[24] = "%04x %04x %04x %04x\n";

        func_8003700C((char *)queued_format, func_8003569C(0), D_80059570, D_8005948C, D_800594A4);
    }
}
