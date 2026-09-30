#include "common.h"

/* Unpacked size of packed data (its first word). */
s32 func_80032E7C(s32 *packed) {
    return *packed;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80032E88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80032EB4);

#include "text.h"

extern u8 *func_80033728(u8 *resource, s32 index);
extern s32 func_80033BAC(u8 first, u8 second);

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
    LoadImage(&rect, D_80050190);
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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80033CF0);

void func_80033DD4(u8 *window, s32 value) {
    s32 previous = *(s32 *)(window + 0x1C);

    *(s32 *)(window + 0x1C) = value;
    *(s32 *)(window + 0x20) = previous;
    *(u16 *)(window + 0x10) |= 0x80;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80033DF0);

#include "window.h"

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
extern void func_80033DF0(Window *window);

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

#include "text.h"

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

#include "pad.h"

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
extern u16 D_80059570;
extern u16 D_80059574;
extern u16 D_8005948C;
extern u16 D_80059490;
extern u16 D_800594A4;
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
extern u8 D_80059418; /* seconds */
extern u8 D_80059420; /* minutes */
extern u8 D_80059484; /* hours */

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80035F1C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80035FF8);

extern Actuator D_8005A1BC[2];
extern void func_80040C3C(u8 *data0, s32 size0, u8 *data1, s32 size1);

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
extern void InitPAD(u8 *buffer0, s32 size0, u8 *buffer1, s32 size1);
extern void StartPAD(void);
extern void ChangeClearPAD(s32 clear);

/* Start the controllers and reset the queue, actuators and assignment. */
void func_80036288(void) {
    u8 *entry;
    s32 i;

    InitPAD((u8 *)&D_800625FC[0], 0x22, (u8 *)&D_800625FC[1], 0x22);
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

extern s32 D_80059390;

void func_800363E0(s32 value) {
    D_80059390 = value;
}

extern s32 D_800501FC;

void func_800363F0(s32 value) {
    D_800501FC = value;
}

extern s32 D_80050200;

void func_80036400(s32 value) {
    D_80050200 = value;
}

extern s32 D_80050208;

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80036528);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800365FC);

extern void (*D_80050594)(void);

/* Install the callback run by 800366f0. */
void func_800366E0(void (*callback)(void)) {
    D_80050594 = callback;
}

void func_800366F0(void) {
    D_80050594();
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80036718);

#include "console.h"

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80036E4C);

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

extern void func_80036718(s32 target, char *format, void *args);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800370DC);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80037324);

void func_8003747C(s32 value) {
    D_800593A0 = value;
}

extern void (*D_800592B8)(char *line);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80037DC0);

#include "sound.h"

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
    SpuVoice *voice = D_800508E4;
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

extern s32 D_800595BC;
extern void EnableEvent(s32 event);
extern void DisableEvent(s32 event);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80037FD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800380D0);

extern void func_800393B8(s32 voice, u16 volume);
extern void func_800395B8(s32 voice, s32 fade, u16 volume);

/* Stop a sequence's voice at once or with a fade (0: its own fade, -1:
 * none). */
void func_800381F4(SoundSequence *sequence, s32 fade) {
    if (fade == 0) {
        fade = sequence->fade;
    } else if (fade == -1) {
        fade = 0;
    }
    if (fade == 0) {
        func_800393B8(sequence->voice, sequence->volume);
        return;
    }
    func_800395B8(sequence->voice, sequence->fade, sequence->volume);
}

extern s32 D_80059584;
extern s32 D_80059588;

void func_80038264(s32 a, s32 b) {
    D_80059584 = a;
    D_80059588 = b;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003827C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038310);

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

extern void func_80039FF8(void);

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

extern void func_80039CC4(void);

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
extern u8 D_80059530[4];
extern void CdMix(u8 *attributes);

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
    D_80059530[2] = volume;
    D_80059530[0] = volume;
    D_80059530[3] = cross;
    D_80059530[1] = cross;
    CdMix(D_80059530);
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
extern s32 func_80038F18(s32 size);
extern void func_8003F6B0(s32 error);
extern void func_80038B4C(void);

/* Start the reverb work area with parameters a/b, reserving its SPU memory
 * on first use. */
void func_80038AD4(s32 a, s32 b) {
    D_800595DC = a;
    D_800595E0 = b;
    if (D_800595A4 == 0) {
        D_800595A4 = func_80038F18(0x840);
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
extern void SpuSetCommonAttr(SpuCommonAttr *attr);

/* Set the CD audio reverb and mix switches. */
void func_80038DB4(s32 reverb, s32 mix) {
    D_8005A3C0.attr.cd.reverb = reverb;
    D_8005A3C0.attr.cd.mix = mix;
    D_8005A3C0.attr.mask |= 0x300;
    SpuSetCommonAttr(&D_8005A3C0.attr);
}

extern SpuVolume D_8005940C;
extern void func_80038E6C(s32 volume, SpuVolume *out, u8 channel);

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

extern SpuBlock *D_80059410;
extern s32 D_8005951C;
extern u32 D_800595E4;

/* Make [start, start + size) the SPU memory pool, aligned to 16 bytes. */
void func_80038EC0(u32 start, s32 size) {
    SpuBlock *block;

    size &= ~0xF;
    if (start & 0xF) {
        size -= 0x10;
        start = (start + 0xF) & ~0xF;
    }
    D_800595E4 = start + size;
    block = (SpuBlock *)start;
    block->flags = 0x8000;
    D_80059410 = block;
    D_8005951C = size;
    block->unk2 = 0;
    block->unk4 = 0;
    block->next = start + 0x10;
    block->unkC = 0;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038F18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039024);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039144);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039360);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800393B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800394B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800395B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800396E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039748);

s32 func_8003977C(void) {
    return 0;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039784);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800397C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800397FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039850);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039910);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800399D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039A80);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039B68);

extern void func_8003B060(SoundTrack *track);
extern void func_8003F6B0(s32 error);

/* Resume a track (error 5 without one). */
void func_80039C4C(SoundTrack *track) {
    if (track == NULL) {
        func_8003F6B0(5);
        return;
    }
    track->flags &= 0x7FFF;
    func_8003B060(track);
}

extern void func_8003A89C(s32 a, s32 b, s32 c);

void func_80039C8C(s32 a, s32 c) {
    if (a == 0) {
        func_8003F6B0(5);
        return;
    }
    func_8003A89C(a, 0, c);
}

extern SoundTrack *D_80059564;

/* Resume every paused track (flag 1). */
void func_80039CC4(void) {
    SoundTrack *track;

    for (track = D_80059564; track != NULL; track = track->next) {
        if (track->flags & 1) {
            track->flags &= 0x7FFF;
            func_8003B060(track);
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

extern s32 D_80059544;

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

extern s32 D_80059404;
extern s32 D_80059478;
extern void func_8003B644(s16 id, s32 channel, s16 volume, s16 pan);

void func_80039DB8(s32 channel) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644((D_80059478 - 2) | 0x8000, channel, 0x6000, 0x4000);
    }
}

void func_80039E18(s32 channel) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644(0x600C, channel, 0x6000, 0x4000);
    }
}

extern s32 func_8003A65C(s32 channel, s32 b);

void func_80039E60(s32 channel) {
    if (D_8005957C & 0x800) {
        s32 id = func_8003A65C(channel, 2);

        D_80059404 = 2;
        func_8003B644(id | 0x2000, channel, 0x6000, 0x4000);
    }
}

void func_80039EC4(s32 channel, s32 sound) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644(((sound & 0xFE) ^ 8) | 0x2000, channel, 0x6000, 0x4000);
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039F18);

void func_80039F9C(s32 channel, s32 sound, s32 volume, s32 pan) {
    if (D_8005957C & 0x800) {
        D_80059404 = 2;
        func_8003B644(((sound & 0xFE) ^ 8) | 0x2000, channel, volume << 8, pan << 8);
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039FF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A094);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A14C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A20C);

void func_8003A2D4(void) {
}

void func_8003A2DC(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A2E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A344);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A3B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A450);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A4FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A55C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A5D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A65C);

/* Whether a sequence is paused (flag bit 15). */
u32 func_8003A82C(SoundSeq *seq) {
    return seq->flags >> 15;
}

/* Set a sequence's tempo (0 means 0x100), at once or over `frames`.
 * Nonmatching: matches when compiled with GCC 2.6.3 (a 2.6.3 translation unit). */
#ifdef NON_MATCHING
void func_8003A838(SoundSeq *seq, s32 tempo, s32 frames) {
    s32 delta;

    if (tempo == 0) {
        tempo = 0x100;
    }
    seq->tempo_target = tempo;
    if (frames == 0) {
        seq->tick_step = seq->resolution * tempo;
        seq->tempo_frames = 0;
        seq->tempo = tempo << 16;
        return;
    }
    delta = (tempo << 16) - seq->tempo;
    if (delta != 0) {
        seq->tempo_frames = frames;
        seq->tempo_step = delta / frames;
    }
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A838);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A89C);

extern void func_8003E680(s32 bits, SoundSeq *seq);

/* Set a sequence's volume, at once or over `frames`. */
void func_8003A948(SoundSeq *seq, s32 volume, s32 frames) {
    s32 delta;

    seq->volume_target = volume << 8;
    if (frames == 0) {
        seq->volume = volume << 24;
        seq->volume_frames = 0;
        func_8003E680(0x200, seq);
        return;
    }
    delta = (volume << 16) - (seq->volume >> 8);
    if (delta != 0) {
        seq->volume_frames = frames;
        seq->volume_step = (delta / frames) << 8;
    }
}

/* Set a sequence's pan, at once or over `frames`. */
void func_8003A9BC(SoundSeq *seq, s32 pan, s32 frames) {
    s32 delta;

    seq->pan_target = pan << 8;
    if (frames == 0) {
        seq->pan = pan << 24;
        seq->pan_frames = 0;
        func_8003E680(0x100, seq);
        return;
    }
    delta = (pan << 16) - (seq->pan >> 8);
    if (delta != 0) {
        seq->pan_frames = frames;
        seq->pan_step = (delta / frames) << 8;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AA30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AAC4);

void func_8003ABE8(SoundSeq *seq, u8 value) {
    seq->unk1B = value;
}

/* A sequence's position: its first word, then frames, seconds and minutes
 * of its tick counter.
 * Nonmatching: matches when compiled with GCC 2.6.3 (a 2.6.3 translation unit). */
#ifdef NON_MATCHING
void func_8003ABF0(SoundSeq *seq, SoundTime *time) {
    u32 ticks = seq->ticks >> 8;
    u32 seconds = ticks / 240;

    time->unk0 = seq->unk24;
    time->frames = ticks % 240;
    time->seconds = seconds % 60;
    time->minutes = seconds / 60;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003ABF0);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AC58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003ACC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AD20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AD98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003ADCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AE84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AF24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AFA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AFFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B060);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B0AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B148);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B1FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B22C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B32C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B370);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B424);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B644);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B930);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B97C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003B9E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BA38);

/* Set `bits` in every active channel of a sequence.
 * Nonmatching: the original reloads the channel flags before the update. */
#ifdef NON_MATCHING
void func_8003BB08(s32 bits, SoundSeq *seq) {
    SoundSeqChannel *channel = seq->channel;
    s32 count = seq->channels;

    do {
        count--;
        if (channel->flags != 0) {
            channel->flags = bits | channel->flags;
        }
        channel++;
    } while (count != 0);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BB08);
#endif

/* Byte offset of channel `index` in a sequence. */
s32 func_8003BB40(s32 index) {
    return index * sizeof(SoundSeqChannel) + 0x94;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BB64);

extern void func_8003BCA0(s32 a, s32 b, s32 c, s32 d, s32 mode);

/* 8003bca0 with modes 1-4, passing the other arguments through. */
void func_8003BC10(s32 a, s32 b, s32 c, s32 d) {
    func_8003BCA0(a, b, c, d, 1);
}

void func_8003BC34(s32 a, s32 b, s32 c, s32 d) {
    func_8003BCA0(a, b, c, d, 2);
}

void func_8003BC58(s32 a, s32 b, s32 c, s32 d) {
    func_8003BCA0(a, b, c, d, 3);
}

void func_8003BC7C(s32 a, s32 b, s32 c, s32 d) {
    func_8003BCA0(a, b, c, d, 4);
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BCA0);

extern u16 D_800594F4; /* command ring write index */
extern u16 D_80059510; /* command ring read index */

/* Whether the eight-entry command ring has at least six entries queued. */
s32 func_8003BDBC(void) {
    u16 write = D_800594F4;

    if (write < D_80059510) {
        write += 8;
    }
    return write - D_80059510 >= 6;
}

void func_8003BDF4(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BDFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BE68);

extern void (*D_8005950C)(void);
extern s32 D_80059514;

/* Driver tick: count it and run the tick callback, flagged busy. */
void func_8003BFA0(void) {
    D_8005957C |= 4;
    D_80059514++;
    if (D_8005950C != NULL) {
        D_8005950C();
    }
    D_8005957C &= ~4;
}

void func_8003C010(void (*callback)(void)) {
    D_8005950C = callback;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003C020);

/* Step a linear slide; on its last frame land exactly on the target. */
void func_8003C484(SoundSlide *slide) {
    if (--slide->frames != 0) {
        slide->value += slide->step;
    } else {
        slide->value = slide->target << 16;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003C4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003C6E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CC84);

/* Sequence opcode handlers: each takes the opcode's operands, the sequence
 * and the channel, and returns the position after the operands. */

/* No operands, no effect. */
u8 *func_8003CD00(u8 *data) {
    return data;
}

/* Nonmatching: the flag updates are scheduled in a different order. */
#ifdef NON_MATCHING
u8 *func_8003CD08(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->unk5C = *data;
    channel->flags2 |= 2;
    channel->flags |= 0x400;
    return data + 1;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD08);
#endif

/* Nonmatching: the flag update is scheduled in a different order. */
#ifdef NON_MATCHING
u8 *func_8003CD30(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->flags |= 0x100;
    channel->unk5C = *data;
    return data + 1;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD30);
#endif

u8 *func_8003CD4C(u8 *data) {
    return data;
}

/* Mark the loop point when the operand matches the sequence's selector.
 * Nonmatching: matches when compiled with GCC 2.6.3 (byte load of the transpose). */
#ifdef NON_MATCHING
u8 *func_8003CD54(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    if (*data++ == seq->unk1B) {
        channel->loop = data;
        channel->unk23 = channel->transpose;
    }
    return data;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD54);
#endif

/* Skip three operand bytes. */
u8 *func_8003CD7C(u8 *data) {
    return data + 3;
}

u8 *func_8003CD84(u8 *data) {
    return data;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD8C);

/* Mark the loop point.
 * Nonmatching: matches when compiled with GCC 2.6.3 (byte load of the transpose). */
#ifdef NON_MATCHING
u8 *func_8003CE04(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->loop = data;
    channel->unk23 = channel->transpose;
    return data;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE04);
#endif

/* Set the octave. */
u8 *func_8003CE18(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose = *data * 12;
    return data + 1;
}

/* Octave up. */
u8 *func_8003CE38(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose += 12;
    return data;
}

/* Octave down. */
u8 *func_8003CE50(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose -= 12;
    return data;
}

/* Set the time signature.
 * Nonmatching: the stores are scheduled in a different order. */
#ifdef NON_MATCHING
u8 *func_8003CE68(u8 *data, SoundSeq *seq) {
    u8 unit = data[1];
    u8 beats = data[0];

    seq->unk3A = 0xC0 / unit;
    seq->unk3C = unit;
    seq->unk38 = beats;
    seq->unk3E = beats;
    seq->unk36 = seq->unk3A;
    return data + 2;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE68);
#endif

/* Nonmatching: the stores are scheduled in a different order. */
#ifdef NON_MATCHING
u8 *func_8003CE9C(u8 *data, SoundSeq *seq) {
    seq->unk32 = data[0];
    seq->unk36 = seq->unk3A;
    seq->unk34 = data[1];
    return data + 2;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE9C);
#endif

u8 *func_8003CEC0(u8 *data, SoundSeq *seq) {
    seq->unk1A = *data;
    return data + 1;
}

u8 *func_8003CED4(u8 *data, SoundSeq *seq) {
    seq->unk1A += *data;
    return data + 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CEF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CFA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CFF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D034);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D070);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D0E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D110);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D13C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D17C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D1BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D208);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D21C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D298);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D2D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D300);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D328);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D340);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D358);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D370);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D3A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D3D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D438);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D4A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D4E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D53C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D59C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D5C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D5CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D5D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D60C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D640);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D65C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D678);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D694);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D6B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D6D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D6F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D714);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D74C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D770);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D79C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D7C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D7FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D854);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D86C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D884);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D8B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003D9A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DAB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DAEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DB0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DB2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DB58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DB98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DBE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DC50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DD24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DE18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DE54);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DE74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DE94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DEB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DEE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DF3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003DF78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E04C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E140);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E160);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E1F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E290);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E308);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E358);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E360);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E3E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E40C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E44C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E4BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E4F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E54C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E680);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E6C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E700);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E724);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E7E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E83C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E8A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003E900);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003EB5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003EBF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003EEA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003EF04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003EFA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003EFE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F190);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F1A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F1EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F240);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F2A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F308);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F354);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F3C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F42C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F43C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F468);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F484);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F4A0);

void func_8003F4BC(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F4E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F4FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F518);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F560);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F588);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F5BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F5EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F614);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F67C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F684);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F6B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F738);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F8B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F8CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", bzero);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", memchr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", memcpy);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", memmove);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", memset);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", rand);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", srand);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", strcat);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", strcmp);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", strcpy);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", strlen);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", sprintf);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", FlushCache);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _bu_init);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", OpenEvent);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CloseEvent);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", EnableEvent);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DisableEvent);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", EnterCriticalSection);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ExitCriticalSection);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SwEnterCriticalSection);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SwExitCriticalSection);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", open);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", write);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ChangeClearPAD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetRCnt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetRCnt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", StartRCnt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", StopRCnt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ResetRCnt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetInitPadFlag);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ReadInitPadFlag);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PAD_init);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", InitPAD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", StartPAD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", StopPAD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPatchPad);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", RemovePatchPad);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _Pad1);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _IsVSync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040A8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040A9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040AAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040ABC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SysEnqIntRP);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SysDeqIntRP);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", EnablePAD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DesablePAD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _patch_pad);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _SendPAD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _send_pad);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040C20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040C3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _remove_ChgclrPAD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DsSyncCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DsReadyCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DsDataCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdInit);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", def_cbsync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", def_cbready);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", def_cbread);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DeliverEvent);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdStatus);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdLastCom);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdLastPos);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdReset);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdFlush);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdSetDebug);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdComstr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdIntstr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdSync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdReady);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdSyncCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdReadyCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdControl);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdControlF);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdControlB);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdMix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdGetSector);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdGetSector2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdDataCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdDataSync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdIntToPos);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdPosToInt);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80018CE4);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80018E28);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80018E38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", getintr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_sync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_ready);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_cw);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_vol);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_flush);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_initvol);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_initintr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_init);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_datasync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_getsector);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_getsector2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CD_set_test_parmnum);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", callback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", puts);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", putchar);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", toupper);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", tolower);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", cb_read);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", cb_data);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", cd_read_retry);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdReadBreak);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdRead);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdReadSync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdReadCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CdReadMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", LoadTPage);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", LoadClut);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", LoadClut2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDefDrawEnv);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDefDispEnv);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetTPage);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetClut);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043A70);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80018F88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DumpClut);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", NextPrim);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", IsEndPrim);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", AddPrim);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", AddPrims);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CatPrim);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", TermPrim);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetSemiTrans);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetShadeTex);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPolyF3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPolyFT3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPolyG3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPolyGT3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPolyF4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPolyFT4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPolyG4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPolyGT4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetSprt8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetSprt16);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetSprt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetTile1);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetTile8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetTile16);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetTile);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetLineF2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetLineG2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetLineF3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043DC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetLineF4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043E00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDrawTPage);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDrawMove);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDrawLoad);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", MargePrim);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043F50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044064);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ResetGraph);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetGraphReverse);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetGraphDebug);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetGraphQueue);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetGraphType);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetGraphDebug);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DrawSyncCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDispMask);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DrawSync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", checkRECT);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ClearImage);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ClearImage2);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80019180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", LoadImage);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", StoreImage);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", MoveImage);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ClearOTag);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ClearOTagR);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DrawPrim);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DrawOTag);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PutDrawEnv);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DrawOTagEnv);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetDrawEnv);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PutDispEnv);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetDispEnv);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetODE);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetTexWindow);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDrawArea);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDrawOffset);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetPriority);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDrawMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDrawEnv);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDrawEnv2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_mode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_cs);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_ce);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_ofs);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_tw);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_dx);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _status);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _otc);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _clr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _dws);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _drs);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _ctl);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _getctl);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _cwb);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _cwc);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _param);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _addque);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _addque2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _exeque);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _reset);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _sync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", set_alarm);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_alarm);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _version);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80047178);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GPU_cw);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", OpenTIM);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ReadTIM);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", OpenTMD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ReadTMD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_tim_addr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", get_tmd_addr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", unpack_packet);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetFogNearFar);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048BBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", InitGeom);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SquareRoot0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", VectorNormalS);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", VectorNormalSS);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048DD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", MulMatrix0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CompMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ApplyMatrixLV);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PushMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PopMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ScaleMatrixL);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetMulMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ApplyMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ApplyMatrixSV);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", TransMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ScaleMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetRotMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetLightMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetColorMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetTransMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDQA);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetDQB);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ReadGeomOffset);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ReadGeomScreen);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetBackColor);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetGeomOffset);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetGeomScreen);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", NormalColor);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", NormalColor3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", NormalColorCol);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", NormalColorCol3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", OuterProduct0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", RotTransSV);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", RotTransPers);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", RotTransPers3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", RotTransPers4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", RotAverage4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", RotMatrixZ);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ratan2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _patch_gte);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", VSync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", v_wait);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ChangeClearRCnt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ResetCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", InterruptCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", DMACallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", VSyncCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", CheckCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetIntrMask);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", trapIntr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", memclr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", setjmp);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _96_remove);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ReturnFromException);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", ResetEntryInt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", HookEntryInt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", startIntrVSync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C01C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", startIntrDMA);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C2C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SetVideoMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", GetVideoMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PCopen);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PCclose);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PClseek);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", PCcreat);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C38C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C398);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _SN_read);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C470);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _SN_write);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuInit);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _SpuInit);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuStart);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_8001946C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_init);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_FwriteByIO);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_Fr_);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_t);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_Fw);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_Fr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_FsetRXX);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_FsetRXXa);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_FsetDelayW);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_FsetDelayR);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_Fw1ts);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _SpuDataCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuQuit);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuInitMalloc);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetNoiseClock);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetReverb);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _SpuIsInAllocateArea_);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuReadDecodedData);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetIRQ);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetIRQCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _SpuCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuGetVoiceEnvelopeAttr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D818);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D878);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetTransferStartAddr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetTransferMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetTransferCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetCommonAttr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetReverbModeType);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _spu_setReverbAttr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuClearReverbWorkArea);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", WaitEvent);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetReverbModeDepth);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetReverbModeDelayTime);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuSetReverbModeFeedback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", SpuGetReverbModeType);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", InitCARD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", StartCARD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E850);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E860);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", StopCARD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _patch_card);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _patch_card2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", _ExitCard);
