#ifndef BATTLE_POPUP_H
#define BATTLE_POPUP_H

/* The battle's floating numbers (damage and recovery popups, 800BE11C-
 * 800BE6E8): sprite tasks spinning and shrinking over their lifetime. */

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"

/* A number popup (a 0x130-byte task). */
typedef struct NumberPopup {
    u8 pad0[0xC];
    void (*destroy)(struct NumberPopup *popup); /* 0x0C */
    u8 pad10[0x38 - 0x10];
    SVECTOR angle;        /* 0x38 */
    VECTOR scale;         /* 0x40 */
    u8 pad50[0x60 - 0x50];
    s32 life;             /* 0x60: frames left */
    s32 spin;             /* 0x64: per frame */
    union {
        u8 rgbc[4];       /* 0x68: red, green, blue, primitive code */
        s32 word;
    } colour;
    s32 glyphCount;       /* 0x6C */
    SpritePart glyphs[8]; /* 0x70 */
} NumberPopup;

/* A damage number over a sprite (800BD3AC, 0x1B0 bytes): it drifts to one
 * side, then fades; listed from D_800C3750. */
typedef struct DamagePopup {
    Task task;
    Task draw;              /* 0x1C */
    struct DamagePopup *next;     /* 0x38 */
    u8 pad3C[0x40 - 0x3C];
    SVECTOR angles;               /* 0x40 */
    s32 x, y, z;                  /* 0x48: 16.16 */
    u8 pad54[0x58 - 0x54];
    VECTOR scale;                 /* 0x58 */
    u8 pad68[0x78 - 0x68];
    Sprite *sprite;         /* 0x78: the sprite it shows over */
    u8 right;                     /* 0x7C: drifts right, else left */
    u8 pad7D[0x80 - 0x7D];
    union {
        u8 rgbc[4];               /* red, green, blue, primitive code */
        s32 word;
    } colour;                     /* 0x80 */
    u8 pad84[0x88 - 0x84];
    s32 timer;                    /* 0x88: frames left in this phase */
    s32 glyphCount;               /* 0x8C */
    SpritePart glyphs[12];        /* 0x90 */
} DamagePopup;

/* The running total shown during an action (D_800D30EC, a static task
 * pair): its value D_800C3D38 as glyphs at (x, y). */
typedef struct TotalPopup {
    Task task;
    Task draw;              /* 0x1C */
    SVECTOR angles;               /* 0x38 */
    VECTOR scale;                 /* 0x40 */
    s16 x;                        /* 0x50 */
    s16 y;                        /* 0x52 */
    u8 pad54[0x60 - 0x54];
    union {
        u8 rgbc[4];
        s32 word;
    } colour;                     /* 0x60 */
    s32 glyphCount;               /* 0x64 */
    SpritePart glyphs[1];         /* 0x68: glyphCount of them */
} TotalPopup;

extern DamagePopup *D_800C3750; /* the damage popups */
extern TotalPopup D_800D30EC;
extern s32 D_800C3D38;          /* the running total */
extern s32 D_800D3680;          /* the total shown, -1 none */

/* The task drawing a popup. */
typedef struct {
    u8 pad0[4];
    NumberPopup *popup; /* 0x04 */
} PopupTask;

extern struct TotalPopup *D_800D2D68; /* the running total's task, if shown */
extern s32 D_800C374C;
extern MATRIX D_800C3760; /* the popups' view */
extern s16 D_800C3754[];  /* the first glyph's x by digit count */
extern s32 D_800D3630;    /* the popup colour kind */
extern u8 D_800C3784[];   /* hexadecimal digit glyphs */
extern u32 D_800C37A4[];  /* powers of ten */

void func_800BD3AC(Sprite *sprite, s32 command, s32 kind);
void func_800BE0DC(void);
void func_800BE108(void);

#endif
