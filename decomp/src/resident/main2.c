#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libgpu.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "text.h"
#include "window.h"
#include "pad.h"
#include "console.h"
#include "sound.h"

/* Unpacked size of packed data (its first word). */
s32 func_80032E7C(s32 *packed) {
    return *packed;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80032E88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80032EB4);

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

/* Install a loaded font block (protected from release).
 * Nonmatching: the font field loads are scheduled above the global stores. */
#ifdef NON_MATCHING
void func_80033558(u16 *font) {
    if (font == NULL) {
        func_800324B8(0x20);
        return;
    }
    func_800320A4(font);
    D_8005936C = font;
    D_8005935C = (u8 *)font;
    D_8005934C = *(font + 2);
    D_80059350 = *(font + 3);
    D_80059354 = *(font + 4);
    D_80059358 = *(font + 5);
    D_8005935C = (u8 *)font + *(font + 1);
    D_80059364 = *(font + 6);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80033558);
#endif

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
 * (count + 3) halfwords.
 * Nonmatching: the table and index additions are emitted in the other operand order. */
#ifdef NON_MATCHING
u8 func_8003373C(u16 *table, s32 index) {
    u8 *entries = (u8 *)table + (*table * 2 + 6);

    return entries[index * 2];
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003373C);
#endif

/* Nonmatching: the table and index additions are emitted in the other operand order. */
#ifdef NON_MATCHING
u8 func_80033760(u16 *table, s32 index) {
    u8 *entries = (u8 *)table + (*table * 2 + 6);

    return entries[index * 2 + 1];
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80033760);
#endif

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
 * plain palettes.
 * Nonmatching: GCC reverses the digit loop counter, which the original
 * counts up. */
#ifdef NON_MATCHING
void func_80033CF0(u32 value, s32 color, s32 sign) {
    u32 divisor = 1000000000;
    u16 *p;
    s32 i;

    color <<= 4;
    if (sign != 0) {
        sign = 11;
        if ((s32)value < 0) {
            value = -value;
            sign = 10;
        }
    }
    for (i = 0, p = &D_8005A0C8[1]; i < 10; i++) {
        *p++ = value / divisor + color;
        value %= divisor;
        divisor /= 10;
    }
    D_8005A0C8[11] = 0xFFFF;
    p = D_8005A0C8;
    D_8005A0C8[0] = color;
    if ((color & 0xFFF0) == color) {
        while (p != &D_8005A0C8[10]) {
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80033CF0);
#endif

void func_80033DD4(u8 *window, s32 value) {
    s32 previous = *(s32 *)(window + 0x1C);

    *(s32 *)(window + 0x1C) = value;
    *(s32 *)(window + 0x20) = previous;
    *(u16 *)(window + 0x10) |= 0x80;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80033DF0);

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
        line->sprite[0].r0 = line->sprite[1].r0 = line->sprite[2].r0 = line->sprite[3].r0 = r;
        line->sprite[0].g0 = line->sprite[1].g0 = line->sprite[2].g0 = line->sprite[3].g0 = g;
        line->sprite[0].b0 = line->sprite[1].b0 = line->sprite[2].b0 = line->sprite[3].b0 = b;
    }
}

void func_80034874(Window *window, u8 value) {
    window->unk6E = value;
}

void func_8003487C(Window *window) {
    window->unk6E = 0xFF;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80034888);

/* The one-line layout window (0x80059FD8) and its line (0x8005A068). */
extern s16 D_80059FD8; /* x */
extern s16 D_80059FDA; /* y */
extern s16 D_80059FE0; /* unk8 */
extern s16 D_80059FE2; /* width */
extern s16 D_80059FE4; /* lines */
extern u16 D_80059FE8; /* flags */
extern s16 D_80059FEA; /* stride */
extern u8 *D_80059FF4; /* text */
extern WindowLine *D_8005A000; /* layout */
extern void *D_8005A004; /* image */
extern u8 D_8005A040;
extern u8 D_8005A041;
extern u8 D_8005A042;
extern u8 D_8005A044;
extern s16 D_8005A05C;
extern WindowLine D_8005A068;

/* Lay out one line of `text` into `image` in the layout window, `width`
 * made odd. Returns the laid-out width in pixels.
 * Nonmatching: the original stores in a different order and sign-extends the width. */
#ifdef NON_MATCHING
s32 func_80034EAC(u8 *text, void *image, s16 width, s32 flags) {
    D_80059FE2 = width;
    width |= 1;
    D_80059FE4 = 1;
    D_80059FE2 = width;
    D_80059FE0 = width << 2;
    D_80059FF4 = text;
    D_8005A040 = 1;
    D_80059FEA = width + 3;
    D_8005A05C = 0;
    D_8005A044 = 0;
    D_8005A042 = 0;
    D_8005A004 = image;
    D_80059FE8 = 0;
    D_80059FDA = 0;
    D_80059FD8 = 0;
    D_8005A041 = 100;
    D_8005A000 = &D_8005A068;
    D_8005A068.unk58 = 0;
    D_8005A068.unk5A = flags & 1;
    func_80033DF0((Window *)&D_80059FD8);
    return D_8005A000->unk58 * 4;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80034EAC);
#endif

/* Draw class of a character: 2 for a narrow glyph, else 3.
 * Nonmatching: register allocation of the loaded limits. */
#ifdef NON_MATCHING
s32 func_80034F98(u16 first, u16 second) {
    s32 result = 3;

    if (first == 0) {
        if (second - D_80059364 < D_80059354) {
            result = 2;
        }
    } else if (first == D_8005934C) {
        if (second < D_80059358) {
            result = 2;
        }
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80034F98);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80034FFC);

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

/* Remap the low button byte through the configured assignment.
 * Nonmatching: the argument and result registers are swapped. */
#ifdef NON_MATCHING
s16 func_800357C0(s32 buttons) {
    s32 result = buttons & 0xFF00;
    s32 i;

    for (i = 0; i < 8; i++) {
        if (buttons & D_800501E8[i]) {
            result |= D_800501E8[D_80050238[i]];
        }
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800357C0);
#endif

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800358BC);

/* Queued controller states (16 entries of the six state words). */
extern u32 D_8005937C; /* queued count */
extern u32 D_80059380; /* write index */
extern u32 D_80059384; /* read index */
extern s32 D_80050208; /* queue overflowed */
extern u16 D_80059574;
extern u16 D_80059490;
extern u16 D_800594A8;
extern u16 D_8005A0FC[16];
extern u16 D_8005A11C[16];
extern u16 D_8005A13C[16];
extern u16 D_8005A15C[16];
extern u16 D_8005A17C[16];
extern u16 D_8005A19C[16];

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

extern s32 D_80050200;
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

extern u8 D_800501F8; /* play time stopped at 100 hours */
extern u8 D_80059370; /* frames */

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

extern Actuator D_8005A1BC[2];

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

extern s32 D_80050204;
extern s32 D_80059390;

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003634C);

void func_800363E0(s32 value) {
    D_80059390 = value;
}

extern s32 D_800501FC;

void func_800363F0(s32 value) {
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

extern u8 D_80059430, D_80059434, D_80059438, D_8005943C; /* actuator values */

/* Print a controller receive buffer in hex, and a digital pad's buttons.
 * Kept as assembly: its C matches, but the string literals then end the
 * rodata where 80036718's table, still assembly, needs the alignment its C
 * literal would carry. */
#ifdef NON_MATCHING
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
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80036528);
#endif

/* Print both controller buffers, the actuator values, the held buttons and
 * every queued pad entry.
 * Kept as assembly: its C matches, but the string literals then end the
 * rodata where 80036718's table, still assembly, needs the alignment its C
 * literal would carry. */
#ifdef NON_MATCHING
void func_800365FC(void) {
    func_80036528(&D_800625FC[0]);
    func_80036528(&D_800625FC[1]);
    func_8003700C("vect0 %02x %02x\n", D_80059430, D_80059438);
    func_8003700C("vect1 %02x %02x\n", D_80059434, D_8005943C);
    func_8003700C("PADD %04x %04x\n", D_80059570, D_80059574);
    while (func_80035CDC() != 0) {
        func_8003700C("%04x %04x %04x %04x\n", func_8003569C(0), D_80059570, D_8005948C, D_800594A4);
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800365FC);
#endif

extern void (*D_80050594)(void);

/* Install the callback run by 800366f0. */
void func_800366E0(void (*callback)(void)) {
    D_80050594 = callback;
}

void func_800366F0(void) {
    D_80050594();
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80036718);

void func_80036CD8(s32 bits) {
    D_80059394->flags |= bits;
}

void func_80036CF8(s32 bits) {
    D_80059394->flags &= ~bits;
}

s16 func_80036D18(void) {
    return D_80059394->flags;
}

void func_80036D30(s32 bits) {
    D_80059394->flags2E |= bits;
}

void func_80036D50(s32 bits) {
    D_80059394->flags2E &= ~bits;
}

s16 func_80036D70(void) {
    return D_80059394->flags2E;
}

void func_80036D88(s16 value) {
    D_80059394->unk14 = value;
}

void func_80036D98(s16 value) {
    D_80059394->unk16 = value;
}

/* Move the console cursor. */
void func_80036DA8(s16 value) {
    D_80059394->x = value;
}

void func_80036DB8(s16 value) {
    D_80059394->y = value;
}

/* Set the console text colour; any channel below 0x80 clears bright mode. */
void func_80036DC8(s32 r, s32 g, s32 b) {
    D_80059394->r = r;
    D_80059394->g = g;
    D_80059394->b = b;
    if (r < 0x80 || g < 0x80 || b < 0x80) {
        D_80059394->mode &= ~1;
    } else {
        D_80059394->mode |= 1;
    }
}

extern u16 D_80050598[64]; /* console font CLUTs */
extern RECT D_80059398;    /* their VRAM rectangle */

/* Build and upload the console font CLUTs: four 16-color rows of
 * foreground/background stripes 1, 2, 4 and 8 entries wide. */
void func_80036E4C(u16 foreground, u16 background) {
    u16 *p = D_80050598;
    s32 i;
    s32 j;

    for (i = 0; i < 8; i++) {
        *p++ = background;
        *p++ = foreground;
    }
    for (; i < 12; i++) {
        *p++ = background;
        *p++ = background;
        *p++ = foreground;
        *p++ = foreground;
    }
    for (; i < 14; i++) {
        for (j = 0; j < 4; j++) {
            *p++ = background;
        }
        for (j = 0; j < 4; j++) {
            *p++ = foreground;
        }
    }
    for (j = 0; j < 8; j++) {
        *p++ = background;
    }
    for (; j < 16; j++) {
        *p++ = foreground;
    }
    LoadImage(&D_80059398, (u_long *)D_80050598);
}

s16 func_80036F44(void) {
    return D_80059394->unk14;
}

s16 func_80036F5C(void) {
    return D_80059394->unk16;
}

s16 func_80036F74(void) {
    return D_80059394->x;
}

s16 func_80036F8C(void) {
    return D_80059394->y;
}

s16 func_80036FA4(void) {
    return D_80059394->unk34;
}

/* Save the console cursor. */
void func_80036FBC(void) {
    D_80059394->saved_x = D_80059394->x;
    D_80059394->saved_y = D_80059394->y;
    D_80059394->saved_36 = D_80059394->unk36;
}

/* Restore the saved console cursor. */
void func_80036FE4(void) {
    D_80059394->x = D_80059394->saved_x;
    D_80059394->y = D_80059394->saved_y;
    D_80059394->unk36 = D_80059394->saved_36;
}

/* printf to the console, when there is one. */
void func_8003700C(char *format, ...) {
    if (D_80059394 != NULL) {
        func_80036718(0, format, &format + 1);
    }
}

/* Place the console cursor relative to its origin. */
void func_80037058(s32 x, s32 y) {
    if (D_80059394 != NULL) {
        D_80059394->x = D_80059394->left + x;
        D_80059394->y = D_80059394->top + y;
    }
}

/* Place the console cursor and line start relative to the origin. */
void func_8003708C(s32 x, s32 y) {
    if (D_80059394 != NULL) {
        if (x < 0) {
            x = 0;
        }
        if (y < 0) {
            y = 0;
        }
        D_80059394->unk36 = D_80059394->x = D_80059394->left + x;
        D_80059394->y = D_80059394->top + y;
    }
}

/* Put a character on the console: a font sprite for printable characters
 * (wrapping, or stopping, at the right edge) and newlines; nothing once the
 * window or the sprite budget is full. */
void func_800370DC(s32 c) {
    Console *con = D_80059394;
    s32 width;

    if (con == NULL) {
        return;
    }
    if (con->y + con->unk16 > con->top + con->height) {
        return;
    }
    if (con->unk34 > con->capacity) {
        return;
    }
    if (c < 0x20) {
        if (c == '\n') {
            con->x = con->unk36;
            con->y += con->unk16;
        }
        return;
    }
    if ((con->flags2E & 4) && c >= 0x60) {
        c -= 0x20;
    }
    c -= 0x20;
    if (con->flags2E & 8) {
        width = con->widths[c];
    } else {
        width = con->unk14;
    }
    if (con->x + width >= con->left + con->width) {
        if (con->flags & 8) {
            return;
        }
        con->x = con->unk36;
        con->y += con->unk16;
    }
    if (c != 0) {
        *(u32 *)&((SPRT_8 *)con->current)->r0 = *(u32 *)&con->r;
        *(u32 *)&((SPRT_8 *)con->current)->x0 = con->x | (con->y << 16);
        if (con->flags2E & 2) {
            /* glyphs on a 16-pixel grid, 8 to a row */
            ((SPRT_8 *)con->current)->clut = con->cluts[(c & 0x18) >> 3];
            *(u16 *)&((SPRT_8 *)con->current)->u0 =
                ((c & 7) << 4) | ((con->texture_v + ((c & 0x60) >> 1)) << 8);
        } else {
            /* glyphs on an 8-pixel grid, 16 to a row */
            ((SPRT_8 *)con->current)->clut = con->cluts[(c & 0x30) >> 4];
            *(u16 *)&((SPRT_8 *)con->current)->u0 =
                ((c & 0xF) << 3) | ((con->texture_v + ((c & 0xC0) >> 3)) << 8);
        }
        con->current += sizeof(SPRT_8);
        con->unk34++;
    }
    con->x += width;
}

/* Home the console cursor and select the active text buffer.
 * Nonmatching: the original loads every field before the stores. */
#ifdef NON_MATCHING
void func_800372CC(void) {
    Console *console = D_80059394;

    console->unk34 = 0;
    console->y = console->top;
    console->saved_y = console->top;
    console->mode &= ~1;
    console->x = console->left;
    console->saved_x = console->left;
    console->unk36 = console->left;
    console->saved_36 = console->left;
    console->current = console->buffer[console->flags2E & 1];
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800372CC);
#endif

/* Flush this frame's console sprites (and texture page, and background
 * tile) into `ot`, or into the console's own ordering table, drawn at once,
 * when `ot` is NULL or -1; then flip the sprite buffers and home the cursor.
 * Nonmatching: the original copies the buffer index and the own-table flag
 * into fresh registers before their last uses. */
#ifdef NON_MATCHING
void func_80037324(u_long *ot) {
    Console *con = D_80059394;
    s32 own;
    s32 index;
    s32 n;
    SPRT_8 *sprite;

    if (con != NULL) {
        own = 0;
        if (con->flags & 1) {
            index = 0;
            con->flags2E &= ~1;
        } else {
            index = con->flags2E & 1;
            if (index) {
                con->flags2E &= ~1;
            } else {
                con->flags2E |= 1;
            }
        }
        if (ot == NULL || ot == (u_long *)-1) {
            DrawSync(0);
            ot = &con->ot[index];
            own = 1;
            TermPrim(ot);
        }
        n = con->unk34;
        sprite = (SPRT_8 *)con->buffer[index];
        while (n != 0) {
            func_800317E0(ot, sprite++);
            n--;
        }
        AddPrim(ot, &con->tpage[index]);
        if (con->flags & 0x10) {
            func_80031804(ot, &con->tile[index]);
        }
        func_800372CC();
        if (own) {
            DrawOTag(ot);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80037324);
#endif

void func_8003747C(s32 value) {
    D_800593A0 = value;
}

/* Close the console: restore the default report output and release the
 * console block unless it is not owned. */
void func_8003748C(void) {
    if (D_80059394 != NULL) {
        D_800592B8 = (void (*)(char *))func_8003700C;
        if (D_800593A0 == 0) {
            func_800320E8(D_80059394);
        }
        D_80059394 = NULL;
    }
    D_800593A0 = 0;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800374E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80037878);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800379B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800379C8);

void func_800379D0(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800379D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80037B88);

/* Shut the sound driver down: remove its SPU interrupt, transfer callback,
 * timer and event, release every voice and clear the reverb (error 0x29
 * when it is not running). */
void func_80037DC0(void) {
    s32 i;

    if (D_8005957C == 0) {
        func_8003F6B0(0x29);
        return;
    }
    EnterCriticalSection();
    D_8005957C = 0;
    SpuSetIRQ(0);
    SpuSetTransferCallback(NULL);
    SpuSetIRQCallback(NULL);
    StopRCnt(0xF2000002);
    CloseEvent(D_800595BC);
    ExitCriticalSection();
    for (i = 0; i < 24; i++) {
        func_8003F5BC(i, 6, 3);
    }
    func_8003F484(0xFFFFFF);
    SpuSetReverbModeDepth(0, 0);
    SpuSetReverbModeType(0);
    D_80059500 = 0;
}

/* Mark every voice's channel for a full register update and clear the
 * voices-silenced state. */
void func_80037E8C(void) {
    SoundChannel **channel = D_8006252C;
    SoundChannel *current;
    s32 i = 0;

    do {
        current = *channel;
        i++;
        if (current != NULL) {
            current->flags |= 0x1F5;
        }
        channel++;
    } while (i < 24);
    D_8005957C &= ~0x40;
}

/* Silence every SPU voice.
 * Nonmatching: the voice fields are addressed from a different base. */
#ifdef NON_MATCHING
void func_80037EE4(void) {
    SpuVoice *voice = D_800508E4->voice;
    s32 i;

    D_8005957C |= 0x40;
    for (i = 0; i < 24; i++) {
        voice->volume_left = 0;
        voice->volume_right = 0;
        voice->pitch = 0;
        voice->adsr2 = 0x1FDF;
        voice->adsr1 = (voice->adsr1 & 0xFF) + 0x7F00;
        voice++;
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80037EE4);
#endif


/* Enable the driver's tick event once. */
void func_80037F44(void) {
    if (!(D_8005957C & 1)) {
        D_8005957C |= 1;
        EnableEvent(D_800595BC);
    }
}

void func_80037F88(void) {
    if (D_8005957C & 1) {
        DisableEvent(D_800595BC);
        D_8005957C &= ~1;
    }
}

SoundSequence *func_800383EC(s32 key);
void func_80038264(s32 address, s32 size);
s32 func_8003827C(u8 *data, s32 size);

/* Load a wave bank: allocate SPU memory for its samples (800381F4),
 * transfer them and add a copy of its header to the loaded banks. Returns
 * the copy, or NULL (error 0x1F without SPU memory, 0x1E without memory
 * for the copy). */
SoundSequence *func_80037FD8(SoundSequence *bank, s32 mode) {
    s32 address = func_800381F4(bank, mode);
    SoundSequence *copy;
    SoundSequence **link;

    if (address == 0) {
        func_8003F6B0(0x1F);
        return NULL;
    }
    func_8003BC10(address, (u8 *)bank + bank->offset, bank->size, NULL);
    copy = func_80039024(bank->header_size);
    if (copy == NULL) {
        func_800396E0(address);
        func_8003F6B0(0x1E);
        return NULL;
    }
    func_80039248(copy, bank, bank->header_size);
    copy->address = address;
    DisableEvent(D_800595BC);
    link = &D_80059558;
    if (D_80059558 != NULL) {
        do {
            link = &(*link)->next;
        } while (*link != NULL);
    }
    *link = copy;
    copy->next = NULL;
    EnableEvent(D_800595BC);
    return copy;
}

/* Start loading a wave bank whose samples arrive in parts: allocate its SPU
 * memory, transfer the samples among the first `size` bytes of the file
 * (8003827C takes the rest) and add a copy of its header to the loaded
 * banks. Returns the copy, or NULL (error 0x16 when a bank with its key is
 * loaded, 0x1F without SPU memory, 0x1E without memory for the copy). */
SoundSequence *func_800380D0(SoundSequence *bank, s32 size, s32 mode) {
    s32 address;
    SoundSequence *copy;
    SoundSequence **link;

    if (func_800383EC(bank->key) != NULL) {
        func_8003F6B0(0x16);
        return NULL;
    }
    address = func_800381F4(bank, mode);
    if (address == 0) {
        func_8003F6B0(0x1F);
        return NULL;
    }
    func_80038264(address, bank->size);
    func_8003827C((u8 *)bank + bank->offset, size - bank->header_size);
    copy = func_80039024(bank->header_size);
    if (copy == NULL) {
        func_800396E0(address);
        func_8003F6B0(0x1E);
        return NULL;
    }
    func_80039248(copy, bank, bank->header_size);
    copy->address = address;
    DisableEvent(D_800595BC);
    link = &D_80059558;
    if (D_80059558 != NULL) {
        do {
            link = &(*link)->next;
        } while (*link != NULL);
    }
    *link = copy;
    copy->next = NULL;
    EnableEvent(D_800595BC);
    return copy;
}

/* Allocate SPU memory for a wave bank's samples: anywhere for mode -1, or
 * for mode 0 when the bank asks for no address; otherwise at the bank's
 * address. Returns the address, 0 when there is no room. */
s32 func_800381F4(SoundSequence *bank, s32 mode) {
    if (mode == 0) {
        mode = bank->address;
    } else if (mode == -1) {
        mode = 0;
    }
    /* The allocator's result is returned as the value it leaves; the
     * function has no return expression. */
    if (mode == 0) {
        func_800393B8(bank->size, bank->volume);
        return;
    }
    func_800395B8(bank->size, bank->address, bank->volume);
}

extern s32 D_80059584;
extern s32 D_80059588;

void func_80038264(s32 a, s32 b) {
    D_80059584 = a;
    D_80059588 = b;
}

/* Transfer the next part of a streamed wave bank's samples (at most what
 * is still missing). Returns the bytes still missing.
 * Nonmatching: the original takes the minimum through an extra register
 * copy. */
#ifdef NON_MATCHING
s32 func_8003827C(u8 *data, s32 size) {
    s32 left = D_80059588;
    s32 address;
    s32 n;

    if (left == 0) {
        return 0;
    }
    n = left;
    if (size < left) {
        n = size;
    }
    address = D_80059584;
    func_8003BC10(address, data, n, NULL);
    D_80059584 = address + n;
    return D_80059588 = left - n;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003827C);
#endif


/* Release a loaded wave bank: unlink it, free its SPU memory (error 0x24
 * when that fails, 0x11 when the bank is not loaded) and its copy. */
void func_80038310(SoundSequence *bank) {
    SoundSequence *entry;
    SoundSequence *prev = NULL;

    for (entry = D_80059558; entry != NULL; entry = entry->next) {
        if (entry == bank) {
            break;
        }
        prev = entry;
    }
    if (entry == NULL) {
        func_8003F6B0(0x11);
        return;
    }
    DisableEvent(D_800595BC);
    if (prev != NULL) {
        prev->next = bank->next;
    } else {
        D_80059558 = bank->next;
    }
    EnableEvent(D_800595BC);
    if (bank->address != func_800396E0(bank->address)) {
        func_8003F6B0(0x24);
    }
    func_80039144(bank);
}

/* The playing sequence with `key`, or NULL. */
SoundSequence *func_800383EC(s32 key) {
    SoundSequence *sequence;

    for (sequence = D_80059558; sequence != NULL; sequence = sequence->next) {
        if (sequence->key == key) {
            break;
        }
    }
    return sequence;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038428);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003852C);

void func_80038624(void) {
    func_80039FF8();
    D_80059440 = NULL;
}

/* The loaded bank with the id of `bank` (or `id` when NULL). */
SoundBank *func_8003864C(SoundBank *bank, s16 id) {
    SoundBank *entry;

    if (bank != NULL) {
        id = bank->id;
    }
    for (entry = D_80059440; entry != NULL; entry = entry->next) {
        if (id == entry->id) {
            break;
        }
    }
    return entry;
}

void func_8003869C(void) {
    func_80039CC4();
    func_80039FF8();
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800386C4);

/* 0 without reverb, 1 or 2 by reverb mode. */
s32 func_80038824(void) {
    s32 mode;

    if (D_8005957C & 0x700) {
        mode = 1;
        if (D_8005957C & 0x600) {
            mode = 2;
        }
    } else {
        mode = 0;
    }
    return mode;
}

extern s16 D_8005A3EE;
extern CdlATV D_80059530;

/* Set the CD audio volume (halved into the right channels without
 * reverb).
 * Nonmatching: the attribute stores are addressed and scheduled differently. */
#ifdef NON_MATCHING
void func_8003885C(s32 volume) {
    s32 cross;

    D_8005A3EE = volume;
    if (D_8005957C & 0x700) {
        cross = 0;
    } else {
        cross = volume >> 1;
        volume = cross;
    }
    D_80059530.val2 = volume;
    D_80059530.val0 = volume;
    D_80059530.val3 = cross;
    D_80059530.val1 = cross;
    CdMix(&D_80059530);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003885C);
#endif

void func_800388D4(s32 enable) {
    if (enable != 0) {
        D_8005957C |= 0x1000;
    } else {
        D_8005957C &= ~0x1000;
    }
}

void func_8003890C(SoundBank *bank, s32 enable) {
    if (enable != 0) {
        bank->flags &= ~1;
    } else {
        bank->flags |= 1;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038934);

extern s32 D_800595A4;
extern s32 D_800595DC;
extern s32 D_800595E0;

/* Start the reverb work area with parameters a/b, reserving its SPU memory
 * on first use. */
void func_80038AD4(s32 a, s32 b) {
    D_800595DC = a;
    D_800595E0 = b;
    if (D_800595A4 == 0) {
        D_800595A4 = (s32)func_80038F18(0x840);
        if (D_800595A4 == 0) {
            func_8003F6B0(0x1E);
        }
    }
    D_8005957C |= 0x20;
    func_80038B4C();
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038B4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038C68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038D18);

/* The driver's SPU common attributes and the volumes they are built from. */
typedef struct {
    SpuCommonAttr attr;
    s16 master;
    s16 cd;
    s16 unk2C;
    s16 cd_request;
} SoundVolumes;

extern SoundVolumes D_8005A3C0;

/* Set the CD audio reverb and mix switches. */
void func_80038DB4(s32 reverb, s32 mix) {
    D_8005A3C0.attr.cd.reverb = reverb;
    D_8005A3C0.attr.cd.mix = mix;
    D_8005A3C0.attr.mask |= 0x300;
    SpuSetCommonAttr(&D_8005A3C0.attr);
}

extern SpuVolume D_8005940C;

/* Apply the master and CD volumes. */
void func_80038DF4(void) {
    func_80038E6C(D_8005A3C0.master, &D_8005A3C0.attr.mvol, 0);
    D_8005A3C0.attr.cd.volume.left = D_8005A3C0.attr.cd.volume.right = D_8005A3C0.cd;
    func_80038E6C(D_8005A3C0.unk2C, &D_8005940C, 1);
    D_8005A3C0.attr.mask |= 0xC3;
}

/* Set a stereo volume pair, inverting one side for the surround modes.
 * Nonmatching: register allocation and branch layout differ. */
#ifdef NON_MATCHING
void func_80038E6C(s32 volume, SpuVolume *out, u8 channel) {
    out->right = volume;
    out->left = volume;
    if (D_8005957C & 0x600) {
        if (!(D_8005957C & 0x200)) {
            if (channel == 1) {
                out->right = -volume;
            } else {
                out->left = -volume;
            }
        } else if (channel != 0) {
            out->left = -volume;
        } else {
            out->right = -volume;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038E6C);
#endif

extern s32 D_8005951C;

/* Make [start, start + size) the driver memory pool, aligned to 16 bytes. */
void func_80038EC0(u32 start, s32 size) {
    SoundBlock *block;

    size &= ~0xF;
    if (start & 0xF) {
        size -= 0x10;
        start = (start + 0xF) & ~0xF;
    }
    D_800595E4 = start + size;
    block = (SoundBlock *)start;
    block->flags = 0x8000;
    D_80059410 = block;
    D_8005951C = size;
    block->unk2 = 0;
    block->unk4 = 0;
    block->end = start + 0x10;
    block->next = NULL;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038F18);

/* Allocate `size` bytes of driver memory, cleared, from the highest gap
 * that fits (between blocks or after the last one). Returns the data or
 * NULL (then the driver event stays disabled).
 * Nonmatching: the block pointers take other registers. */
#ifdef NON_MATCHING
void *func_80039024(s32 size) {
    s32 need;
    SoundBlock *after;
    u32 limit;
    SoundBlock *block;
    u8 *data;

    DisableEvent(D_800595BC);
    need = ((size + 0xF) & ~0xF) + 0x10;
    after = NULL;
    limit = 0;
    for (block = D_80059410;; block = block->next) {
        if (block->next == NULL) {
            if ((s32)(D_800595E4 - block->end) >= need) {
                after = block;
                limit = D_800595E4;
            }
            break;
        }
        if ((s32)((u32)block->next - block->end) >= need) {
            after = block;
            limit = (u32)block->next;
        }
    }
    limit -= need;
    if (after == NULL) {
        return NULL;
    }
    block = (SoundBlock *)((limit + 0xF) & ~0xF);
    data = (u8 *)(block + 1);
    block->end = (u32)(data + size);
    block->next = NULL;
    block->unk4 = 0;
    block->flags = 2;
    block->unk2 = 0;
    block->next = after->next;
    after->next = block;
    EnableEvent(D_800595BC);
    func_800392EC((u32 *)data, size);
    return data;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039024);
#endif

/* Release a block of driver memory.
 * Nonmatching: the original keeps the pool head and the block address in
 * separate registers from the walk. */
#ifdef NON_MATCHING
void func_80039144(void *data) {
    SoundBlock *head = D_80059410;
    SoundBlock *block = (SoundBlock *)data - 1;
    SoundBlock *entry;
    SoundBlock *prev;

    DisableEvent(D_800595BC);
    entry = head;
    prev = NULL;
    while (entry != block) {
        prev = entry;
        entry = entry->next;
    }
    if (prev != NULL) {
        prev->next = block->next;
    }
    EnableEvent(D_800595BC);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039144);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800391CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039248);

/* Clear `size` bytes: sixteen at a time, then words, then bytes.
 * Nonmatching: the pointer and its offset copy swap registers. */
#ifdef NON_MATCHING
void func_800392EC(u32 *p, s32 size) {
    s32 n;

    for (n = size >> 4; n != 0; n--) {
        p[3] = 0;
        p[2] = 0;
        p[1] = 0;
        p[0] = 0;
        p += 4;
    }
    for (n = (size >> 2) & 3; n != 0; n--) {
        *p++ = 0;
    }
    for (n = size & 3; n != 0; n--) {
        *(u8 *)p = 0;
        p = (u32 *)((u8 *)p + 1);
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800392EC);
#endif

/* Reset the SPU memory map to one reserved entry for the first 0x1010
 * bytes. */
void func_80039360(void) {
    s32 i;

    for (i = 11; i >= 0; i--) {
        D_8006F9FC[i].flags = 0;
    }
    D_8006F9FC[0].flags = 0x81;
    D_8006F9FC[0].unk1 = 5;
    D_8006F9FC[0].address = 0;
    D_8006F9FC[0].size = 0x1010;
    D_8006F9FC[0].next = 0;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800393B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800394B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800395B8);

/* Release the SPU memory map entry at `address`, unlinking it. Returns the
 * address, 0 when no entry has it. */
u32 func_800396E0(u32 address) {
    SpuMemBlock *entry = D_8006F9FC;
    SpuMemBlock *prev = NULL;

    for (;;) {
        if (entry->address == address) {
            prev->next = entry->next;
            entry->flags = 0;
            entry->unk1 = 0;
            entry->address = 0;
            entry->next = 0;
            return address;
        }
        prev = entry;
        if (entry->next == 0) {
            return 0;
        }
        entry = &D_8006F9FC[entry->next];
    }
}

/* Set the second byte of the SPU memory map entry at `address`. */
void func_80039748(u32 address, u8 value) {
    SpuMemBlock *entry = func_800397C0(address);

    if (entry != NULL) {
        entry->unk1 = value;
    }
}

s32 func_8003977C(void) {
    return 0;
}

/* The index of an unused SPU memory map entry, 0 when all are in use. */
s32 func_80039784(void) {
    s32 i;

    for (i = 0; i < 12; i++) {
        if (D_8006F9FC[i].flags == 0) {
            return i;
        }
    }
    return 0;
}

/* The SPU memory map entry at `address`, or NULL. Only the first entry is
 * examined: the walk returns as soon as that entry has a successor.
 * Nonmatching: GCC sees that the walk never leaves the first entry and
 * drops the entry pointer, which the original keeps. */
#ifdef NON_MATCHING
SpuMemBlock *func_800397C0(u32 address) {
    SpuMemBlock *entry = D_8006F9FC;
    s32 i;

    while (entry->address != address) {
        i = entry->next;
        if (i != 0) {
            return NULL;
        }
        entry = &D_8006F9FC[i];
    }
    return entry;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800397C0);
#endif

/* Create a sequence for `header` and play it. Returns the sequence. */
SoundSeq *func_800397FC(SoundSeqHeader *header, s32 fade, s32 frames) {
    SoundSeq *seq = func_80039850(header);

    func_80039A80(seq, fade, frames);
    return seq;
}

/* Create a sequence for valid sequence data in driver memory (with room
 * for a snapshot when the data has a table). Returns it, or NULL (the
 * data's error, or 0x1E without memory).
 * Nonmatching: the original moves the data pointer to another register and
 * keeps a separate error call for a failed allocation. */
#ifdef NON_MATCHING
SoundSeq *func_80039850(SoundSeqHeader *header) {
    s16 error = func_8003F67C(header);
    s32 size;
    SoundSeq *seq;

    if (error == 0) {
        size = func_8003BB40(header->channels);
        if (header->entries != 0) {
            size += 0x180;
        }
        seq = func_80038F18(size);
        if (seq == NULL) {
            func_8003F6B0(0x1E);
            return NULL;
        }
        seq->header = header;
        if (header->entries != 0) {
            func_8003B0AC(seq, header);
        }
        func_8003B22C(seq);
        func_8003B424(seq);
        seq->muted = 0;
        func_8003B9E4(seq);
        return seq;
    }
    func_8003F6B0(error);
    return NULL;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039850);
#endif

/* Create a sequence for valid sequence data in memory the caller provides
 * (flag 0x4000: not released with it). Returns it, or NULL. */
SoundSeq *func_80039910(SoundSeqHeader *header, SoundSeq *seq) {
    s16 error = func_8003F67C(header);
    s32 size;

    if (error != 0) {
        func_8003F6B0(error);
        return NULL;
    }
    size = func_8003BB40(header->channels);
    if (header->entries != 0) {
        size += 0x180;
    }
    func_800392EC((u32 *)seq, size);
    seq->header = header;
    if (header->entries != 0) {
        func_8003B0AC(seq, header);
    }
    func_8003B22C(seq);
    func_8003B424(seq);
    seq->muted = 0;
    func_8003B9E4(seq);
    seq->flags |= 0x4000;
    return seq;
}

/* Stop and release a sequence (its memory unless the caller provided it). */
void func_800399D4(SoundSeq *seq) {
    if ((s16)seq->flags & 0x8000) {
        func_80039C4C((SoundTrack *)seq);
    }
    if (func_8003F67C(seq->header) != 0) {
        func_8003F6B0(0xA);
        return;
    }
    if (func_8003BA38(seq) != 0) {
        func_8003F6B0(5);
        return;
    }
    func_8003B930(seq);
    if (!(seq->flags & 0x4000)) {
        func_80039144(seq);
    }
}

/* Play a sequence from its start, fading in over `frames`. */
void func_80039A80(SoundSeq *seq, s32 fade, s32 frames) {
    if (seq == NULL) {
        func_8003F6B0(5);
        return;
    }
    seq->flags &= 0x7FFF;
    if (func_8003F67C(seq->header) != 0) {
        func_8003F6B0(0xA);
        return;
    }
    if ((s16)seq->flags & 0x8000) {
        func_80039C4C((SoundTrack *)seq);
    }
    DisableEvent(D_800595BC);
    func_8003B22C(seq);
    func_8003B424(seq);
    seq->fade.value = 0;
    func_8003A89C(seq, fade, frames);
    seq->flags |= 0x8000;
    EnableEvent(D_800595BC);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039B68);

/* Resume a track (error 5 without one). */
void func_80039C4C(SoundTrack *track) {
    if (track == NULL) {
        func_8003F6B0(5);
        return;
    }
    track->flags &= 0x7FFF;
    func_8003B060((SoundSeq *)track);
}

void func_80039C8C(s32 a, s32 c) {
    if (a == 0) {
        func_8003F6B0(5);
        return;
    }
    func_8003A89C((SoundSeq *)a, 0, c);
}

extern SoundTrack *D_80059564;

/* Resume every paused track (flag 1). */
void func_80039CC4(void) {
    SoundTrack *track;

    for (track = D_80059564; track != NULL; track = track->next) {
        if (track->flags & 1) {
            track->flags &= 0x7FFF;
            func_8003B060((SoundSeq *)track);
        }
    }
}

void func_80039D24(void) {
}

/* Enable or disable (and flush) the sound effect channel. */
void func_80039D2C(s32 enable) {
    if (enable != 0) {
        D_8005957C |= 0x800;
    } else {
        func_80039FF8();
        D_8005957C &= ~0x800;
    }
}


/* Set the voice count (even, 4 to 16) unless 0; returns the setting. */
s32 func_80039D78(s32 count) {
    if (count != 0) {
        if (count > 16) {
            count = 16;
        }
        if (count < 4) {
            count = 4;
        }
        D_80059544 = count & 0xFE;
    }
    return D_80059544;
}


void func_80039DB8(s32 channel) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644((D_80059478 - 2) | 0x8000, channel, 0x6000, 0x4000);
    }
}
