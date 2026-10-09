/* Battle unit from 800BD3AC to 800BFE48 (Cygnus CDK GCC 2.7.2).
 * 800BD3AC's table at 0x80070ADC sits at 4 mod 8 directly after 800B9F78's
 * odd-length table at 0 mod 8; the functions from 800BA4E0 to 800BD2E4 have
 * no rodata, and the boundary is placed at the first function that has. Its
 * own 11 entries are followed directly by 800BFE48's at 0x80070B08 (0 mod 8),
 * so the unit ends before 800BFE48. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "effect.h"
#include "objects.h"
#include "screen.h"
#include "sprite.h"
#include "actor.h"
#include "popup.h"
#include "frame.h"
#include "stage.h"
#include "battle_command.h"

/* The unit's own uninitialized variable (its .bss, after
 * battle_800B8098.c's); the commons follow (battle_common.c). */
static s32 D_800C3CE8; /* finished sprite motions */

/* This unit's data (800c374c-800c37d4). The flags D_800C3780 and D_800C37C8
 * have stray assembler bytes in their padding, so they stay original data. */
s32 D_800C374C = 0;
DamagePopup *D_800C3750 = NULL;
s16 D_800C3754[5] = {-4, -8, -12, -16, -20}; /* the first glyph's x by digit count */
MATRIX D_800C3760 = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0x200}};
INCLUDE_ORIGINAL(".data", D_800C3780, 0x800C3780, 4);
u8 D_800C3784[32] = "0123456789ABCDEF0123456789abcdef";
u32 D_800C37A4[] = {10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
INCLUDE_ORIGINAL(".data", D_800C37C8, 0x800C37C8, 4);
u8 D_800C37CC = 0;
s32 D_800C37D0 = 0;

/* Show value over sprite as a damage popup of kind (replacing the sprite's
 * earlier ones): 1 a prefix glyph, 2 green, 3 magenta with a prefix, 4 a
 * single glyph that only fades, 5 red, 10 and 11 (blue) with a suffix
 * glyph; the digits follow. Only while the battle menu is open. */
void func_800BD3AC(BattleSprite *sprite, s32 value, s32 kind) {
    DamagePopup *popup;
    u8 text[0x38];
    s32 x;
    s32 i;

    if (D_800C3610 == NULL) {
        return;
    }
    func_800BDE58();
    D_800C4929 = 0;
    for (popup = D_800C3750; popup != NULL; popup = popup->next) {
        if (popup->sprite == sprite) {
            popup->task.destroy(&popup->task);
        }
    }
    popup = func_8001D1D8(sizeof(DamagePopup), NULL, func_800BDC78, func_800BDA1C, func_800BD7A0);
    popup->sprite = sprite;
    popup->timer = 8;
    popup->next = D_800C3750;
    popup->x.fixed = sprite->x.fixed;
    D_800C3750 = popup;
    popup->y.fixed = sprite->y.fixed;
    popup->z.fixed = sprite->z.fixed;
    popup->scale.vx = 0x2000;
    popup->scale.vy = 0x2000;
    popup->scale.vz = 0x2000;
    popup->colour.rgbc[3] = 0x2D;
    popup->angles.vx = 0;
    popup->angles.vy = 0;
    popup->angles.vz = 0;
    popup->colour.rgbc[0] = 0x80;
    popup->colour.rgbc[1] = 0x80;
    popup->colour.rgbc[2] = 0x80;
    popup->right = sprite->motion.bits.flip;
    popup->glyphCount = 0;
    if (kind != 4) {
        func_800BE6E8(value, text, 5, 0, 0);
        x = D_800C3754[text[0] - 1];
    }
    switch (kind) {
    case 4:
        popup->timer = 0x40;
        popup->glyphCount = func_80026DCC(D_800D2F5C, 0x7C, popup->glyphs, -0x14, -0x10);
        popup->colour.rgbc[3] = (popup->colour.rgbc[3] & ~1) | 2;
        func_8001CD6C(popup, func_800BDB08);
        break;
    case 5:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0;
        popup->colour.rgbc[3] &= ~1;
        break;
    case 10:
        popup->glyphCount += func_80026DCC(D_800D2F5C, 0x7F, &popup->glyphs[popup->glyphCount], x, 0);
        break;
    case 11:
        popup->glyphCount += func_80026DCC(D_800D2F5C, 0x91, &popup->glyphs[popup->glyphCount], x, 0);
        popup->colour.rgbc[2] = 0x80;
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[3] &= ~1;
        break;
    case 1:
        popup->glyphCount += func_80026DCC(D_800D2F5C, 0x80, &popup->glyphs[popup->glyphCount], x, -0x10);
        x += 0x20;
        break;
    case 3:
        popup->glyphCount += func_80026DCC(D_800D2F5C, 0x80, &popup->glyphs[popup->glyphCount], x, -0x10);
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[2] = 0x80;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[3] &= ~1;
        x += 0x20;
        break;
    case 2:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0;
        popup->colour.rgbc[3] &= ~1;
        break;
    }
    if (kind != 4) {
        for (i = 0; i != text[0]; i++, x += 10) {
            popup->glyphCount += func_80026DCC(D_800D2F5C, text[i + 1] + 0x72, &popup->glyphs[popup->glyphCount], x, -0x10);
        }
    }
}

/* Damage popup destroy: unlink it from D_800C3750 and end it. */
void func_800BD7A0(BattleTask *task) {
    DamagePopup *popup = (DamagePopup *)task;
    DamagePopup *entry = D_800C3750;
    DamagePopup *previous = NULL;

    for (; entry != NULL; entry = entry->next) {
        if (entry == popup) {
            if (previous != NULL) {
                previous->next = entry->next;
            } else {
                D_800C3750 = entry->next;
            }
            break;
        }
        previous = entry;
    }
    func_8001D19C(task);
}

/* Draw a popup glyph as a textured quad in colour (its word also sets the
 * primitive code) through the current matrices, added to the ordering
 * table's first entry, while the primitive buffer has room. */
void func_800BD810(PopupGlyph *glyph, s32 colour) {
    POLY_FT4 *poly = (POLY_FT4 *)D_80059580;
    long p;
    long flag;
    u16 x, y;
    s32 w, h;
    u8 u, v;
    u8 uw, vh;

    if (D_80059580 + sizeof(POLY_FT4) < D_80059534) {
        D_80059580 += sizeof(POLY_FT4);
        setlen(poly, 9);
        *(s32 *)&poly->r0 = colour;
        poly->tpage = glyph->tpage;
        poly->clut = glyph->clut;
        w = glyph->w;
        h = glyph->h;
        x = glyph->x;
        y = glyph->y;
        D_8004FB98[0].vx = x;
        D_8004FB98[0].vy = y;
        D_8004FB98[1].vx = x + w;
        D_8004FB98[1].vy = y;
        D_8004FB98[2].vx = x + w;
        D_8004FB98[2].vy = y + h;
        D_8004FB98[3].vx = x;
        D_8004FB98[3].vy = y + h;
        RotTransPers4(&D_8004FB98[0], &D_8004FB98[1], &D_8004FB98[2], &D_8004FB98[3], (long *)&poly->x0,
                      (long *)&poly->x1, (long *)&poly->x3, (long *)&poly->x2, &p, &flag);
        u = glyph->u;
        v = glyph->v;
        uw = glyph->w;
        vh = glyph->h;
        poly->u0 = u;
        poly->v0 = v;
        poly->u1 = u + uw;
        poly->v1 = v;
        poly->u2 = u;
        poly->v2 = v + vh;
        poly->u3 = u + uw;
        poly->v3 = v + vh;
        addPrim(D_8005956C, poly);
    }
}

/* The camera-space offset of point's projection from the geometry offset
 * (doubled), at the screen distance. */
void func_800BD974(SVECTOR *point, VECTOR *out) {
    SVECTOR unused; /* allocated in the original frame */
    s16 screen[2];
    long p;
    long flag;
    long offsetX;
    long offsetY;

    SetRotMatrix(&D_800D30BC);
    SetTransMatrix(&D_800D30BC);
    RotTransPers(point, (long *)screen, &p, &flag);
    ReadGeomOffset(&offsetX, &offsetY);
    out->vx = (screen[0] - offsetX) * 2;
    out->vy = (screen[1] - offsetY) * 2;
    out->vz = ReadGeomScreen();
}

/* Damage popup draw: its glyphs turned, scaled and placed over its
 * point. */
void func_800BDA1C(BattleTask *draw) {
    MATRIX m;
    SVECTOR point;
    VECTOR offset;
    DamagePopup *popup = draw->data;
    s32 i;
    PopupGlyph *glyph;

    D_800C3760.t[2] = ReadGeomScreen();
    point.vx = popup->x.fixed >> 16;
    point.vy = popup->y.fixed >> 16;
    point.vz = popup->z.fixed >> 16;
    func_800BD974(&point, &offset);
    func_8003F738(&popup->angles, &m);
    TransMatrix(&m, &offset);
    CompMatrix(&D_800C3760, &m, &m);
    ScaleMatrix(&m, &popup->scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0, glyph = popup->glyphs; i != popup->glyphCount; i++, glyph++) {
        func_800BD810(glyph, popup->colour.word);
    }
}

/* Damage popup fade: darken by 2 a frame until its time is up. */
void func_800BDB08(DamagePopup *popup) {
    func_800BDF1C();
    popup->colour.rgbc[0] -= 2;
    popup->colour.rgbc[1] -= 2;
    popup->colour.rgbc[2] -= 2;
    if (--popup->timer < 0) {
        popup->task.destroy(&popup->task);
    }
}

/* Damage popup fade out: darken by 8 a frame (green and blue follow red)
 * until black or its time is up. */
void func_800BDB74(BattleTask *task) {
    DamagePopup *popup = (DamagePopup *)task;

    func_800BDF1C();
    popup->colour.rgbc[0] = func_80021AD8(popup->colour.rgbc[0], -8);
    popup->colour.rgbc[1] = func_80021AD8(popup->colour.rgbc[0], -8);
    popup->colour.rgbc[2] = func_80021AD8(popup->colour.rgbc[0], -8);
    if (--popup->timer < 0 || (popup->colour.rgbc[0] | popup->colour.rgbc[1] | popup->colour.rgbc[2]) == 0) {
        popup->task.destroy(&popup->task);
    }
}

/* Damage popup hold: after 16 frames switch to the fade out (800BDB74). */
void func_800BDC14(DamagePopup *popup) {
    func_800BDF1C();
    if (--popup->timer < 0) {
        popup->timer = 16;
        popup->colour.rgbc[3] = (popup->colour.rgbc[3] | 2) & ~1;
        func_8001CD6C(popup, func_800BDB74);
    }
}

/* Damage popup drift: move 12 a frame to its side, then hold (800BDC14)
 * for 16 frames. */
void func_800BDC78(DamagePopup *popup) {
    func_800BDF1C();
    if (popup->right) {
        popup->x.fixed += 0xC0000;
    } else {
        popup->x.fixed -= 0xC0000;
    }
    if (--popup->timer < 0) {
        popup->timer = 16;
        func_8001CD6C(popup, func_800BDC14);
    }
}

/* Running total destroy: end its draw task and itself. */
void func_800BDCF8(TotalPopup *total) {
    D_800D2D68 = NULL;
    func_8001CB48(&total->draw);
    func_8001CD94(total);
}

void func_800BDD34(void) {
}

/* Running total draw: its label glyph, then its digits turned, scaled and
 * centred on the screen. */
void func_800BDD3C(BattleTask *draw) {
    TotalPopup *total = draw->data;
    MATRIX m;
    SVECTOR unused; /* allocated in the original frame */
    VECTOR offset;
    long x;
    long y;
    s32 i;
    PopupGlyph *glyph;

    ReadGeomOffset(&x, &y);
    func_80026BA4(D_800D2F5C, 0x81, total->x, total->y, D_8005956C);
    offset.vx = (0xA0 - x) * 2;
    offset.vy = (0x46 - y) * 2;
    offset.vz = ReadGeomScreen();
    D_800C3760.t[2] = ReadGeomScreen();
    func_8003F738(&total->angles, &m);
    TransMatrix(&m, &offset);
    CompMatrix(&D_800C3760, &m, &m);
    ScaleMatrix(&m, &total->scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    for (i = 0, glyph = total->glyphs; i != total->glyphCount; i++, glyph++) {
        func_800BD810(glyph, total->colour.word);
    }
}

/* Show the running total (D_800D30EC at (0x90, 0x2A)), unless shown or
 * sprite commands run. */
void func_800BDE58(void) {
    if (D_800D2D68 == NULL && D_800C3780 == 0) {
        D_800D2D68 = &D_800D30EC;
        func_8001CC18(0, &D_800D30EC);
        func_8001CA58(&D_800D30EC, &D_800D30EC.draw);
        func_8001CD6C(&D_800D30EC, func_800BDD34);
        func_8001CD64(&D_800D30EC.draw, func_800BDD3C);
        func_8001CD74(&D_800D30EC, func_800BDCF8);
        D_800D30EC.x = 0x90;
        D_800D30EC.y = 0x2A;
        D_800D30EC.task.data = &D_800D30EC;
        D_800D30EC.draw.data = &D_800D30EC;
        D_800D30EC.glyphCount = 0;
        D_800D3680 = -1;
    }
}

/* Update the running total when D_800C3D38 changed: pop the value
 * (800BE330) and show it coloured by the popup kind (2 green, 3 magenta,
 * 11 blue, else white). */
void func_800BDF1C(void) {
    s32 value = D_800C3D38;
    TotalPopup *total;
    u8 text[8];
    s32 i;
    s32 x;

    if (D_800D2D68 != NULL && D_800C3780 == 0) {
        total = D_800D2D68;
        if (value != D_800D3680) {
            D_800D3680 = value;
            func_800BE330(value);
            total->colour.rgbc[3] = 0x2D;
            total->scale.vx = 0x2000;
            total->scale.vy = 0x2000;
            total->scale.vz = 0x2000;
            total->colour.rgbc[3] &= ~1;
            total->angles.vx = 0;
            total->angles.vy = 0;
            total->angles.vz = 0;
            switch (D_800D3630) {
            case 11:
                total->colour.rgbc[0] = 0;
                total->colour.rgbc[1] = 0;
                total->colour.rgbc[2] = 0x80;
                break;
            case 3:
                total->colour.rgbc[0] = 0x80;
                total->colour.rgbc[1] = 0;
                total->colour.rgbc[2] = 0x80;
                break;
            case 2:
                total->colour.rgbc[0] = 0;
                total->colour.rgbc[1] = 0x80;
                total->colour.rgbc[2] = 0;
                break;
            default:
                total->colour.rgbc[0] = 0x80;
                total->colour.rgbc[1] = 0x80;
                total->colour.rgbc[2] = 0x80;
                break;
            }
            func_800BE6E8(value, text, 5, 0, 0);
            x = D_800C3754[text[0] - 1];
            total->glyphCount = 0;
            for (i = 0; i != text[0]; x += 10) {
                total->glyphCount += func_80026DCC(D_800D2F5C, text[i + 1] + 0x72, &total->glyphs[total->glyphCount], x, -8);
                i++;
            }
        }
    }
}

/* Hide the running total (800BDCF8), if shown. */
void func_800BE0DC(void) {
    if (D_800D2D68 != NULL) {
        func_800BDCF8(D_800D2D68);
    }
}

/* Clear D_800D2D68 and D_800C374C. */
void func_800BE108(void) {
    D_800D2D68 = 0;
    D_800C374C = 0;
}

/* Popup update: spin and shrink it, fade its colour, end it with its life. */
void func_800BE11C(NumberPopup *popup) {
    popup->angle.vz += popup->spin;
    popup->scale.vx += 0x330;
    popup->scale.vy += 0x330;
    popup->scale.vz += 0x330;
    popup->colour.rgbc[0] = func_80021AD8(popup->colour.rgbc[0], -4);
    popup->colour.rgbc[1] = func_80021AD8(popup->colour.rgbc[1], -4);
    popup->colour.rgbc[2] = func_80021AD8(popup->colour.rgbc[2], -4);
    if (--popup->life == 0) {
        popup->destroy(popup);
    }
}

/* Popup drawing: its glyphs six times, each copy turned back 5 more and
 * shrunk by 0x330, centred on the screen from the geometry offset. */
void func_800BE1C4(PopupTask *task) {
    MATRIX m;
    SVECTOR unused; /* allocated in the original frame */
    SVECTOR angle;
    VECTOR offset;
    VECTOR scale;
    s32 x;
    s32 y;
    NumberPopup *popup = task->popup;
    s32 i;
    s32 j;
    PopupGlyph *glyph;

    ReadGeomOffset(&x, &y);
    offset.vx = (0xA0 - x) * 2;
    offset.vy = (0x46 - y) * 2;
    offset.vz = ReadGeomScreen();
    D_800C3760.t[2] = ReadGeomScreen();
    func_80021B24(&angle, &popup->angle);
    scale.vx = popup->scale.vx;
    scale.vy = popup->scale.vy;
    scale.vz = popup->scale.vz;
    for (i = 0; i != 6; i++) {
        func_8003F738(&angle, &m);
        TransMatrix(&m, &offset);
        CompMatrix(&D_800C3760, &m, &m);
        ScaleMatrix(&m, &scale);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
        for (j = 0, glyph = popup->glyphs; j != popup->glyphCount; j++, glyph++) {
            func_800BD810(glyph, popup->colour.word);
        }
        angle.vz -= 5;
        scale.vx -= 0x330;
        scale.vy -= 0x330;
        scale.vz -= 0x330;
    }
}

/* Show value as a number popup, coloured by the popup kind D_800D3630 (2
 * green, 3 magenta, 11 blue, else white), spinning one way at random. */
void func_800BE330(s32 value) {
    NumberPopup *popup;
    u8 text[8];
    s32 i;
    s32 x;

    popup = func_8001D1D8(sizeof(NumberPopup), 0, func_800BE11C, func_800BE1C4, 0);
    popup->spin = -(((rand() & 3) - 2) * 8);
    if (popup->spin == 0) {
        popup->spin = 6;
    }
    popup->life = 32;
    popup->colour.rgbc[3] = 0x2E;
    popup->scale.vx = 0x2000;
    popup->scale.vy = 0x2000;
    popup->scale.vz = 0x2000;
    popup->glyphCount = 0;
    popup->angle.vx = 0;
    popup->angle.vy = 0;
    popup->angle.vz = 0;
    switch (D_800D3630) {
    case 11:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0x80;
        break;
    case 3:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0;
        popup->colour.rgbc[2] = 0x80;
        break;
    case 2:
        popup->colour.rgbc[0] = 0;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0;
        break;
    default:
        popup->colour.rgbc[0] = 0x80;
        popup->colour.rgbc[1] = 0x80;
        popup->colour.rgbc[2] = 0x80;
        break;
    }
    func_800BE6E8(value, text, 5, 0, 0);
    x = D_800C3754[text[0] - 1];
    popup->glyphCount = 0;
    for (i = 0; i != text[0]; x += 10) {
        popup->glyphCount += func_80026DCC(D_800D2F5C, text[i + 1] + 0x72, &popup->glyphs[popup->glyphCount], x, -8);
        i++;
    }
    for (i = 0; i != popup->glyphCount; i++) {
        popup->glyphs[i].w--;
        popup->glyphs[i].h--;
    }
}

/* Run up to three commands (kinds 0, 1 and 10) on slot's sprite outside the
 * battle menu and wait frames until they are done. */
void func_800BE538(s32 slot, s32 first, s32 second, s32 third) {
    BattleSprite *sprite;
    s32 mode;
    BattleMenu *menu;

    D_80059464 = 0;
    D_800591AC = 1;
    sprite = BATTLE_AREA.sprites[slot];
    D_800C3780 = 1;
    if (sprite != NULL) {
        mode = sprite->motion.bytes[3];
        func_800245D8(sprite, 10);
        menu = D_800C3610;
        D_800C3610 = (BattleMenu *)1;
        if (first) {
            func_800BD3AC(sprite, first, 0);
        }
        if (second) {
            func_800BD3AC(sprite, second, 1);
        }
        if (third) {
            func_800BD3AC(sprite, third, 10);
        }
        D_800C3610 = menu;
        while (func_800BF6F8()) {
            func_800BE790();
        }
        while (sprite->motion.bytes[3] == 10) {
            func_800BE790();
        }
        func_800245D8(sprite, mode);
    }
    D_800C3780 = 0;
    func_800BE0DC();
    D_80059464 = 0;
    D_800591AC = 0;
}

/* Write value as digits hexadecimal glyphs (D_800C3784, plus base) after
 * the count in text. */
void func_800BE6A0(s32 value, u8 *text, s32 digits, s32 base) {
    s32 i;
    s32 last;

    i = 0;
    if (digits != 0) {
        last = digits - 1;
        do {
            text[i + 1] = D_800C3784[(value >> ((last - i) * 4)) & 0xF] + base;
        } while (++i != digits);
    }
    text[0] = digits;
}

/* Write value in decimal after the count in text: a '-' for a negative
 * value, then its last digits + 1 digits (plus base), leading zeros only
 * when leading is set. */
void func_800BE6E8(s32 value, u8 *text, s32 digits, u8 leading, s32 base) {
    s32 count = 0;
    u8 *out = text + 1;
    u8 digit;

    if (value < 0) {
        count = 1;
        text[1] = '-';
        out = text + 2;
        value = -value;
        digits--;
    }
    while (value %= D_800C37A4[digits], digits != 0) {
        digits--;
        digit = value / D_800C37A4[digits];
        if (digit) {
            leading = 1;
        }
        if (leading) {
            *out++ = digit + base;
            count++;
        }
    }
    *out = value + base;
    text[0] = count + 1;
}

/* Run one battle frame: swap the display buffers, read the controllers,
 * update the sprites, the stage and the effects (the skipped frames once
 * more each) with the stack in the scratchpad, draw, time the frame and
 * present it; the outermost frame also runs the battle menu, a requested
 * single action and the deferred free of the objects' extra files. */
void func_800BE790(void) {
    BattleArea *frame;
    FrameBuffer *buffer;
    s32 skipped;

    D_800C37D0++;
    func_80019CA0();
    D_800D309C.start = VSync(-1);
    frame = &BATTLE_AREA;
    buffer = &frame->buffers[0];
    if (frame->current == buffer) {
        buffer = &frame->buffers[1];
    }
    frame->current = buffer;
    frame->ot = buffer->ot;
    ClearOTagR(buffer->ot, 0x1000);
    frame->buffer = 1 - frame->buffer;
    if (D_80010000 != -1) {
        func_800BEBC4();
        __asm__ volatile(".word 0x0001000D"); /* break 1: the debugger breakpoint */
        func_80280A9C();
    }
    func_800250E0(frame->buffer);
    func_800BBAB8();
    func_800BB9D4();
    func_80024FF4(&D_800D309C.matrix);
    func_80024FE4(frame->ot);
    if (D_80010000 != -1) {
        func_80037324(frame->ot);
    }
    func_800A9A50(&D_800D309C.matrix, (s32)D_800CCB94, D_8005956C, frame->buffer);
    SPAD_STACK_ENTER();
    func_8001D468();
    func_8001C9F8();
    func_8001C964();
    skipped = D_80059494;
    while (--skipped != -1) {
        func_800BBAB8();
        func_8001C964();
    }
    SPAD_STACK_LEAVE();
    func_80076544();
    func_8008A9C0(0);
    while (--D_80059494 != -1) {
        func_8008A9C0(1);
    }
    D_800D309C.drawn = VSync(1);
    DrawSync(0);
    D_800D309C.synced = VSync(1);
    D_80059494 = VSync(-1) - D_800D309C.start - D_80059198;
    if (D_80059494 < 0) {
        D_80059494 = 0;
    }
    if (D_80059494 >= 5) {
        D_80059494 = 4;
    }
    BATTLE_AREA.frameTicks = D_80059494 + D_80059198;
    if (D_80059198 != 0) {
        VSync(D_80059198 + 1);
    } else {
        VSync(0);
    }
    PutDispEnv(&BATTLE_AREA.current->dispEnv);
    PutDrawEnv(&BATTLE_AREA.current->drawEnv);
    func_80025044();
    DrawOTag(&BATTLE_AREA.current->ot[0xFFF]);
    func_800BEB04();
    if (D_800C37D0 == 1) {
        if (D_800C3610 != NULL) {
            D_800C3610->update(D_800C3610);
        }
        if (D_800591B4 != 0) {
            s32 action = D_800591B4;

            D_800591B4 = 0;
            func_800B8068(action);
        }
        if (D_800C37CC != 0 && func_800286CC() == 0) {
            D_800C37CC = 0;
            func_800B136C();
        }
    }
    D_800C37D0--;
}

/* Load the requested battle module (D_800591B3) into 0x801FC000 when it
 * changed, around the module switch 800B8354, and mark it loaded. */
void func_800BEB04(void) {
    s32 saved0;
    s32 saved1;
    u8 module;

    if (D_800591B2 != (module = D_800591B3)) {
        D_800591B2 = D_800591B3;
        func_800B8354();
        func_800284B4(&saved0, &saved1);
        func_80028470(0xC, 2);
        func_800295D8(module + 2, 0x801FC000, 0, 0x80);
        func_800B8354();
        func_80028470(saved0, saved1);
        DrawSync(0);
        VSync(0);
        EnterCriticalSection();
        FlushCache();
        ExitCriticalSection();
    }
    D_800591B0 = 1;
}

/* Read the controllers; holding select (0x100) slows the frame down. */
void func_800BEBC4(void) {
    func_800BEC18();
    if (BATTLE_AREA.held & 0x100) {
        VSync(8);
        D_80059494 = 0;
    }
}

/* Read both controllers: held, newly pressed and released buttons, and a
 * history of the first controller's last changes. */
void func_800BEC18(void) {
    s32 held;
    u16 old;

    held = func_8003569C(0) & 0xFFFF;
    old = BATTLE_AREA.held;
    BATTLE_AREA.held = held;
    BATTLE_AREA.pressed = ~old & held;
    BATTLE_AREA.released = old & ~held;
    held = func_8003569C(1);
    BATTLE_AREA.pressed2 = ~BATTLE_AREA.held2 & held;
    BATTLE_AREA.held2 = held;
    BATTLE_AREA.heldOnly = BATTLE_AREA.held & ~held;
    if (BATTLE_AREA.held != BATTLE_AREA.history[0].held) {
        BATTLE_AREA.history[3] = BATTLE_AREA.history[2];
        BATTLE_AREA.history[2] = BATTLE_AREA.history[1];
        BATTLE_AREA.history[1] = BATTLE_AREA.history[0];
        BATTLE_AREA.history[0].held = BATTLE_AREA.held;
        BATTLE_AREA.history[0].pressed = BATTLE_AREA.pressed;
        BATTLE_AREA.history[0].released = BATTLE_AREA.released;
        BATTLE_AREA.history[0].time = D_800D30E4;
    }
}

/* Clear the battle menu state. */
void func_800BED30(void) {
    D_800C3E20 = 0;
    D_800C3610 = NULL;
    D_800D2E54 = 0;
}

/* Open the battle menu (800B9F78 its update). */
BattleMenu *func_800BED4C(void) {
    BattleMenu *menu = func_80031BDC(sizeof(BattleMenu), 0);

    D_800C3610 = menu;
    menu->update = func_800B9F78;
    D_800C360C = 0;
    menu->field4A = 0;
    D_800C3610->field30 = 0;
    D_800C3610->field48 = 1;
    D_800C3610->field2C = 1;
    D_800C3610->sprite = NULL;
    D_800C3610->field49 = 0;
    D_800C3610->field34 = 0;
    func_800BF0B4(0);
    D_80059464 = 0;
    D_800591AC = 1;
    return D_800C3610;
}

/* Close the battle menu. */
void func_800BEDE8(void) {
    func_800320E8(D_800C3610);
    D_80059464 = 0;
    D_800C3610 = NULL;
    D_800591AC = 0;
}

/* Run 800AA320 on a 4 KB stack of its own. */
void func_800BEE2C(s32 index, s32 mask, s32 arg2) {
    u8 *stack = func_80031BDC(0x1000, 1);

    STACK_ENTER(stack + 0xF9C);
    func_800AA320(index, mask, arg2);
    STACK_LEAVE();
    func_800320E8(stack);
}

/* List the slot sprites of the slots in mask (up to 11, NULL-terminated),
 * setting their target; their count. */
s32 func_800BEEB4(u32 mask, BattleSprite **list, BattleSprite *target) {
    s32 i;
    s32 count;
    BattleSprite *sprite;

    i = 0;
    count = i;

    for (; i != 11; i++, mask = (mask & 0xFFFF) >> 1) {
        if (mask & 1) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL) {
                sprite->partner = target;
                list[count] = sprite;
                count++;
            }
        }
    }
    list[count] = NULL;
    return count;
}


/* The direction from sprite from to sprite to on the ground. */
s16 func_800BEF24(BattleSprite *from, BattleSprite *to) {
    GroundPoint a;
    GroundPoint b;

    a.x = from->x.fixed >> 16;
    a.z = from->z.fixed >> 16;
    b.x = to->x.fixed >> 16;
    b.z = to->z.fixed >> 16;
    return func_80023124(b, a);
}

/* The direction from sprite to its target point on the ground. */
s16 func_800BEF8C(BattleSprite *sprite) {
    GroundPoint a;
    GroundPoint b;

    a.x = sprite->x.fixed >> 16;
    a.z = sprite->z.fixed >> 16;
    b.x = sprite->target[0];
    b.z = sprite->target[2];
    return func_80023124(b, a);
}

/* Make slot the acting slot, returning the previous acting sprite to idle. */
void func_800BEFF4(s32 slot) {
    BattleSprite *sprite = D_800C3610->sprite;

    if (sprite != NULL && D_800C3610->slot != slot && !BATTLE_AREA.slots[SPRITE_SLOT(sprite)].hidden) {
        func_800245D8(sprite, sprite->idle.mode);
    }
    D_800C3610->slot = slot;
    D_800C3610->sprite = BATTLE_AREA.sprites[slot];
}

/* Set the battle menu's state. */
void func_800BF0B4(s32 state) {
    D_800C3610->state = state;
}

/* Walk sprite to the next point of the path, or at its end, to its target. */
void func_800BF0C4(BattleSprite *sprite) {
    if (BATTLE_AREA.path[D_800C3610->field2C].x == 0xFFFF && BATTLE_AREA.path[D_800C3610->field2C].z == 0xFFFF) {
        sprite->target[1] = 0;
        sprite->target[0] = sprite->x.fixed >> 16;
        sprite->target[2] = sprite->z.fixed >> 16;
        func_800BF4F0(sprite, sprite->partner);
        return;
    }
    sprite->target[0] = BATTLE_AREA.path[D_800C3610->field2C].x;
    sprite->target[2] = BATTLE_AREA.path[D_800C3610->field2C].z;
    sprite->target[1] = 0;
    func_800BF1EC(sprite, BATTLE_AREA.path[D_800C3610->field2C].run ? 3 : 2);
    D_800C3610->field2C++;
}

/* Start sprite moving to its target point with motion mode. */
void func_800BF1EC(BattleSprite *sprite, s32 mode) {
    GroundPoint from;
    GroundPoint to;

    from.x = sprite->x.fixed >> 16;
    from.z = sprite->z.fixed >> 16;
    to.x = sprite->target[0];
    to.z = sprite->target[2];
    D_800C3610->field44 = func_800C07CC(from, to);
    func_80021FE0((s32 *)sprite, func_800BEF8C(sprite));
    func_800223B0((s32 *)sprite, func_800BEF8C(sprite));
    func_800245D8(sprite, mode);
    func_800BF0B4(6);
}

/* Load the file of sprite's resource for its slot's command. */
void func_800BF2B8(BattleSprite *sprite) {
    s32 file;
    void *block;

    func_800B8D7C();
    func_80028470(0x2C, 1);
    file = sprite->resource->file;
    block = func_80031BDC(func_800288EC(file), 1);
    func_800295D8(file, (s32)block, 0, 0x80);
    D_800C3618 = block;
    D_800C361C = SPRITE_SLOT(sprite);
}

/* Start the loaded command file once. */
s32 func_800BF354(void) {
    s32 result;

    if (D_800D3350 == 0) {
        result = (s32)func_800C0FAC(D_800C3618);
        D_800D3350 = 1;
    }
    return result;
}

/* Stop the started command file. */
void func_800BF3A4(void) {
    if (D_800D3350 != 0) {
        func_800C1140(D_800C3618);
        D_800D3350 = 0;
    }
}

/* Face sprite and the first target of the current event at each other. */
void func_800BF3E8(BattleSprite *sprite) {
    BattleSprite *first;
    s32 slot;
    BattleSprite *target;

    D_800D3634 = BATTLE_AREA.events[D_800C360C].targetMask;
    if ((D_800D3678 = func_800BEEB4(BATTLE_AREA.events[D_800C360C].targetMask, D_800D363C, sprite)) == 0) {
        D_800D363C[0] = sprite;
    }
    target = D_800D363C[0];
    sprite->partner = target;
    target->partner = sprite;
    first = D_800D363C[0];
    slot = SPRITE_SLOT(target);
    D_800C3610->target = first;
    D_800C3610->targetSlot = slot;
    func_800223B0((s32 *)sprite, func_800BEF24(sprite, sprite->partner));
    if (target->motion.bytes[3] != 0x15) {
        func_800223B0((s32 *)target, func_800BEF24(sprite->partner, sprite));
    }
}

/* At the path's end, step sprite beside target; else walk the path on. */
void func_800BF4F0(BattleSprite *sprite, BattleSprite *target) {
    s16 x;

    if (BATTLE_AREA.path[D_800C3610->field2C].x == 0xFFFF && BATTLE_AREA.path[D_800C3610->field2C].z == 0xFFFF) {
        sprite->x.fixed = sprite->target[0] << 16;
        sprite->z.fixed = sprite->target[2] << 16;
        x = target->x.fixed >> 16;
        sprite->target[0] = (s16)(sprite->x.fixed >> 16) >= x ? x + 0x50 : x - 0x50;
        sprite->target[2] = target->z.fixed >> 16;
        sprite->target[1] = 0;
        if (sprite->target[0] == (s16)(sprite->x.fixed >> 16) && sprite->target[2] == (s16)(sprite->z.fixed >> 16)) {
            func_800B9C00(sprite);
            return;
        }
        func_800BF1EC(sprite, 3);
        func_800BF0B4(2);
        return;
    }
    func_800BF0C4(sprite);
}

/* Count a finished sprite motion. */
void func_800BF5E8(void) {
    D_800C3CE8++;
}

/* Run command with sprite playing its motion, then wait for the motion's end. */
void func_800BF600(s32 command, BattleSprite *sprite) {
    if (sprite->field48 == 0) {
        func_800B7C34(command);
        return;
    }
    D_800C3CE8 = 0;
    if (sprite->motion.bytes[3] != 0) {
        func_80021BF8(sprite, func_800BF5E8);
        func_800245D8(sprite, sprite->motion.bytes[3]);
    }
    func_800B7C34(command);
    if (sprite->motion.bytes[3] != 0) {
        while (D_800C3CE8 == 0) {
            func_800BE790();
        }
        func_80021BF8(sprite, NULL);
    }
}

/* Update the battle menu when it is open. */
void func_800BF6CC(void) {
    if (D_800C3610 != NULL) {
        func_800BD2E4();
    }
}

/* D_80059464 less one while a number popup shows. */
s32 func_800BF6F8(void) {
    return D_80059464 - func_800BF720();
}

/* Whether a number popup shows. */
s32 func_800BF720(void) {
    return D_800D2D68 != 0;
}

/* Set D_800C3628. */
void func_800BF730(s32 value) {
    D_800C3628 = value;
}

/* Watch a sprite's value; on a rise or a fall under the threshold call back
 * and end. */
void func_800BF73C(EffectSprite *task) {
    SlotWatch *watch = (SlotWatch *)task;
    s32 last = watch->value;

    watch->value = func_800B57E4(watch->sprite);
    if (last < watch->value || watch->value < watch->threshold) {
        watch->callback(watch->sprite);
        watch->destroy(watch);
    }
}

/* Start watching sprite's value against threshold with callback. */
void func_800BF7C8(BattleSprite *sprite, s32 threshold, void (*callback)(BattleSprite *sprite)) {
    SlotWatch *watch = func_8001CD08(sprite->task, sizeof(SlotWatch) - 0x1C);

    func_8001CD6C((EffectSprite *)watch, func_800BF73C);
    watch->callback = callback;
    watch->sprite = sprite;
    watch->mode = sprite->motion.bytes[3];
    watch->value = func_800B57E4(sprite);
    watch->threshold = threshold;
    sprite->motion.word |= 0x20;
}

/* Make slot's sprite act on the sprite of slot target alone. */
void func_800BF85C(s32 slot, s32 target) {
    BattleSprite *sprite = BATTLE_AREA.sprites[slot];

    if (sprite != NULL) {
        D_800C3E1C = sprite;
        sprite->partner = BATTLE_AREA.sprites[target];
        D_800D3634 = 1 << target;
        D_800D363C[0] = BATTLE_AREA.sprites[target];
        D_800D363C[1] = NULL;
    }
}

/* Move sprite's target to the next of the event's targets. */
void func_800BF8CC(BattleSprite *sprite) {
    s32 i;

    for (i = 0; i != D_800D3678; i++) {
        if (D_800D363C[i] == sprite->partner) {
            break;
        }
    }
    if (i >= D_800D3678) {
        sprite->partner = D_800D363C[0];
    } else {
        sprite->partner = D_800D363C[i + 1];
    }
}

/* The index of sprite among the event's targets. */
s32 func_800BF954(BattleSprite *sprite) {
    s32 i;

    for (i = 0; i != D_800D3678; i++) {
        if (D_800D363C[i] == sprite) {
            break;
        }
    }
    return i;
}

/* Count an effect hit; at the second, signal event 11. */
void func_800BF998(void) {
    D_800D2D4C++;
    D_800D36BC++;
    if (D_800D2D4C == 2) {
        func_800A9FF0(0xB);
    }
}

/* Upload the images of file 1 of directory 0x2C once requested. */
void func_800BF9EC(void) {
    void *file;

    if (D_800C3621 != 0) {
        func_800B8354();
        func_80028470(0x2C, 0);
        file = func_80031BDC(func_800288EC(1), 0);
        func_800295D8(1, (s32)file, 0, 0x80);
        func_800B8354();
        func_8002DDE4(file, 0, 0, 0, 0, 0, 0);
        func_800BE790();
        func_800320E8(file);
        D_800C3621 = 0;
    }
}

/* Once requested, restart the party slots in gears (in mode 2 all but the
 * acting one) and wait for them to stop moving. */
void func_800BFA9C(void) {
    s32 i;
    s32 acting;

    if (D_800C362C != 0) {
        func_800B8D04();
        func_800BE790();
        func_800BE790();
        acting = SPRITE_SLOT(D_800C3E1C);
        for (i = 0; i != 3; i++) {
            if (BATTLE_AREA.slots[i].gear != 0 && (D_800C362C != 2 || i != acting) && BATTLE_AREA.slots[i].field2 < 0x11) {
                func_800BB760(i);
            }
        }
        while (D_800C35D8 != 0) {
            func_800BE790();
        }
        D_800C362C = 0;
    }
}

/* Load and transfer the sound bank of file 5 of directory 0x2C once. */
void func_800BFBA0(void) {
    u8 *file;

    if (D_800C3620 == 0) {
        func_800B8354();
        func_80028470(0x2C, 0);
        file = func_80031BDC(func_800288EC(5), 0);
        func_800295D8(5, (s32)file, 0, 0x80);
        func_800B8354();
        if (func_800383EC(*(u16 *)(file + 0x20)) == 0) {
            func_800C0F70();
            D_800C3A6C = func_80037FD8(file, 0);
            while (func_8003BDFC(0) != 0) {
                func_800BE790();
            }
            D_800C3620 = 1;
            D_800C3622 = 0;
        }
        func_800320E8(file);
    }
}

/* Find the resident effect sprites of sprite with motion mode (any with
 * action 2): action 0 returns the first, the others destroy them. */
BattleSprite *func_800BFC80(BattleSprite *sprite, s32 mode, s32 action) {
    ActorTask *owner = sprite->task;
    ActorTask *task;
    BattleSprite *child;

    for (task = D_8005958C; task != NULL; task = task->next) {
        if (task->owner == owner && (task->link & 0x1FFFFFFF) == (owner->id & 0x1FFFFFFF) && (task->link >> 29 & 1)) {
            child = task->data;
            if (child->base == D_8006BE10) {
                if (action != 2) {
                    if (child->motion.bytes[3] != mode) {
                        continue;
                    }
                    if (action == 0) {
                        return child;
                    }
                }
                child->task->destroy(child->task);
            }
        }
    }
    return NULL;
}

/* Destroy the resident effect sprites of sprite with motion mode. */
void func_800BFD88(BattleSprite *sprite, s32 mode) {
    func_800BFC80(sprite, mode, 1);
}

/* Give sprite a resident effect sprite playing motion mode, unless it has. */
void func_800BFDA8(BattleSprite *sprite, s32 mode) {
    u8 saved;
    BattleSprite *child;

    if (sprite->field48 != 0 && func_800BFC80(sprite, mode, 0) == NULL) {
        void *motion = (void *)(SPRITE_RESOURCE->motions[mode + 1] + (s32)SPRITE_RESOURCE->motions);
        saved = D_800591AC;
        D_800591AC = 0;
        child = func_80023B84(sprite, motion, D_8006BE10);
        child->motion.bytes[3] = mode;
        child->partner = sprite;
        D_800591AC = saved;
        child->idle.word |= 0x100;
    }
}
