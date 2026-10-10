#ifndef BATTLE_POPUP_H
#define BATTLE_POPUP_H

/* The battle's floating numbers (damage and recovery popups, 800BD3AC's
 * unit 800BD3AC-800BE6EC): sprite tasks spinning and shrinking over their
 * lifetime, and the running total. */

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
 * side, then fades; listed from battle_damage_popups. */
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

/* The running total shown during an action (battle_total_popup, a static task
 * pair): its value battle_running_total as glyphs at (x, y). */
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

extern DamagePopup *battle_damage_popups;  /* the damage popups */
extern TotalPopup battle_total_popup;
extern s32 battle_running_total;           /* the running total */
extern s32 battle_total_popup_shown_value; /* the total shown, -1 none */

/* The task drawing a popup. */
typedef struct {
    u8 pad0[4];
    NumberPopup *popup; /* 0x04 */
} PopupTask;

extern struct TotalPopup *battle_current_total_popup; /* the running total's task, if shown */
extern s32 battle_unread_popup_word;
extern MATRIX battle_popup_view_matrix;               /* the popups' view */
extern s16 battle_popup_first_glyph_x_table[];        /* the first glyph's x by digit count */
extern s32 battle_popup_color_kind;                   /* the popup colour kind */
extern u8 battle_hex_digits[];                        /* hexadecimal digit glyphs */
extern u32 battle_powers_of_ten[];                    /* powers of ten */

void battle_damage_popup_show(Sprite *sprite, s32 value, s32 kind); /* show a value over a sprite */
void battle_total_popup_hide(void); /* hide the running total */
void battle_total_popup_reset(void);

#endif
