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
    func_80044894(&rect, D_80050190);
    D_800595D4 = func_80043A58(x, y);
    D_80059414 = func_80043A58(x + 16, y);
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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80035C0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80035CDC);

extern s32 D_8005937C;

s32 func_80035DA0(void) {
    return D_8005937C;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80035DB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80035E44);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80036288);

extern u8 D_8005938C;

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80036420);

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
extern void func_800404A4(s32 event);
extern void func_800404B4(s32 event);

/* Enable the driver's tick event once. */
void func_80037F44(void) {
    if (!(D_8005957C & 1)) {
        D_8005957C |= 1;
        func_800404A4(D_800595BC);
    }
}

void func_80037F88(void) {
    if (D_8005957C & 1) {
        func_800404B4(D_800595BC);
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
extern void func_8004138C(u8 *attributes);

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
    func_8004138C(D_80059530);
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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038AD4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038B4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038C68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038D18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038DB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038DF4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038E6C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038EC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80038F18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039024);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039144);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800391CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039248);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800392EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039360);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800393B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800394B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800395B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800396E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039748);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003977C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039784);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800397C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800397FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039850);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039910);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800399D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039A80);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039B68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039C4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039C8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039CC4);

void func_80039D24(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039D2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039D78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039DB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039E18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039E60);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039EC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039F18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80039F9C);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A82C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A838);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A89C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A948);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003A9BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AA30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003AAC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003ABE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003ABF0);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BB08);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BB40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BB64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BC10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BC34);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BC58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BC7C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BCA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BDBC);

void func_8003BDF4(void) {
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BDFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BE68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003BFA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003C010);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003C020);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003C484);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003C4C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003C6E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CC84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD08);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD54);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD7C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CD8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE04);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CE9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CEC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003CED4);

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

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F8E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F918);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F968);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003F99C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003FA08);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003FA38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003FA68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003FA78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003FB20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003FB84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003FBC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8003FBF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040454);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040464);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040474);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040484);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800404A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800404B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800404D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800404E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800404F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040514);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040534);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040554);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800405D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800405F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040690);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800406C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800406FC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040734);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004076C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004077C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004078C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040828);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800408C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800408F4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004092C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800409AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800409E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040A4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040A8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040A9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040AAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040ABC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040ACC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040ADC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040AEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040B00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040B14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040B7C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040BA4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040C20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040C3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040C5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040CBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040CD0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040CE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040D08);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040DA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040DC8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040DF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040E18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040E28);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040E38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040E48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040E58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040E68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040ED4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040EF4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040F0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040F40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040F74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040F94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040FB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040FCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80040FE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004111C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80041248);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004138C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800413AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800413CC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800413EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80041410);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80041430);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80041534);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80018CE4);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80018E28);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80018E38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800415B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80041B3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80041DBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042088);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800424A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004252C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004260C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042700);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004293C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042AA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042BA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042C98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042CA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042D8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042DDC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042E90);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042EC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80042EF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800431C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800432BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800434D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004356C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043670);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004373C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043754);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004376C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043858);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800438C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043928);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800439E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043A1C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043A58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043A70);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80018F88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043AD0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043B10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043B2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043B48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043B84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043BC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043BE4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043BFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043C24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043C4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043C60);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043C74);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043C88);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043C9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043CB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043CC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043CD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043CEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043D00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043D14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043D28);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043D3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043D50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043D64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043D78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043D8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043DA0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043DC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043DE0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043E00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043E20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043E4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043EAC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043F18);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80043F50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044064);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044110);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044294);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800443A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004440C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800444B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800444C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800444D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044534);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800445D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004463C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044764);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800447F8);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_80019180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044894);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800448F8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004495C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044A20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044AD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044B70);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044BD0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044C44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044D48);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044E64);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80044E9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045344);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004537C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800453AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800453E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004546C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800454B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800454DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045534);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004574C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800459DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045A34);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045B00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045BCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045C10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045C94);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045D44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045D5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80045E44);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800460A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800462DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80046560);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80046588);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004659C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800465EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80046638);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80046668);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004668C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004696C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80046C58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80046DB4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80046EFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80046F30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004709C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80047178);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800471A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800471B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800471C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004722C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004726C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80047518);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80047638);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800477D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048AB0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048BBC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048BC4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048C4C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048D68);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048DA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80048DD8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004920C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004931C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004947C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004960C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_800496AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004974C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004987C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80049CEC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80049D3C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80049D9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80049DCC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80049EFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80049F2C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80049F5C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_80049F8C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A0A4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A0B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A0BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A0DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A0EC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A12C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A14C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A19C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A1B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A260);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A280);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A4D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A54C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A64C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A67C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A73C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004A7BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B18C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B32C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B4AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B54C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B694);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B730);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B740);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B770);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B7A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B7D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B894);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B8BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004B9B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004BE24);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004BE50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004BED8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004BEF0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004BF00);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004BF10);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004BF20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C01C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C048);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C2C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C2F0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C308);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C318);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C338);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C348);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C36C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C38C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C398);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C458);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C470);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C548);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C568);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C660);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/main2", D_8001946C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C6DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004C970);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004CBFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004CCA8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004CF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004CFC0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D028);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D070);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D1B0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D1DC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D208);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D270);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D294);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D310);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D364);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D3B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D504);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D590);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D600);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D740);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D784);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D7A8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D818);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D878);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D8D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D930);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D964);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004D988);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004DD1C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004DEF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E3C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E564);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E574);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E5A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E6B8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E774);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E794);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E7E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E850);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E860);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E870);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E8D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004E990);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main2", func_8004EA20);
