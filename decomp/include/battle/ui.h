#ifndef BATTLE_UI_H
#define BATTLE_UI_H

#include "common.h"
#include "psyq/libgpu.h"
#include "battle/work.h"

/* The battle UI: its state block, the drawing state, the glyph and quad
 * builders and text images (80070E2C's unit, 800769E8-80076EA4), the menu
 * icons, the direction arrows (80077698-80077990), the decimal digits
 * (battle.c 8008AAA0, 8008AC00) and the cursor glyph (8008CCCC's 80090B90).
 * The event script overlay reads the UI state too. */

/* Battle UI state: the heap block at *800d2d28. */
typedef struct BattleUi {
    RECT textureWindows[6]; /* +0x00 */
    u32 unk30;         /* CLUT cycle position */
    s32 unk34;         /* +0x34 panel cursor window: x */
    s32 unk38;
    u32 unk3C;         /* y */
    s32 unk40;
    s32 unk44;         /* width */
    s32 unk48;
    u32 unk4C;         /* height */
    s32 unk50;
    u32 unk54;         /* x step (8.8) */
    s32 unk58;
    u32 unk5C;         /* step count */
    s32 unk60;
    s32 unk64;
    s32 unk68;
    s32 unk6C;
    u8 unk70[4];
    u8 statusParts[4]; /* +0x74 status glyph parts, [3] party-wide */
    u8 gaugeParts[3];  /* +0x78 */
    u8 unk7B;          /* AP text part count */
    u8 reaction[3];    /* +0x7C */
    u8 barShown[4];    /* +0x7F time bar shown, [3] party-wide */
    u8 portraitBuffer; /* +0x83 */
    u8 statusBuffer[4]; /* +0x84 draw buffer of the status glyphs, [3] party-wide */
    u8 unk88[0x8E - 0x88];
    u8 unk8E;
    u8 showSkill;      /* +0x8F the result screens' learnt skill message (ovl2596) */
    u8 unk90[3];       /* per party member */
    u8 unk93[3];
    u8 unk96;          /* file 3 block loaded */
    u8 unk97;          /* selected list row */
    u8 unk98;
    u8 unk99[3];       /* party panel digit buffers */
    u8 unk9C;
    u8 unk9D;
    u8 cursorShown;    /* +0x9E the cursor glyph (graphics +0x27c8) is shown */
    u8 unk9F;
    u8 showCards;      /* +0xA0 the result screens' member cards, which count up */
    u8 showSummary;    /* +0xA1 their summary window */
    u8 gaugeBuffer;    /* +0xA2 */
    u8 unkA3;
    u8 unkA4;
    u8 unkA5;
    u8 unkA6;
    u8 cursorBuffer;   /* +0xA7 its draw buffer */
    u8 unkA8;
    u8 unkA9;
    u8 unkAA;          /* frame counter */
    u8 unkAB;
    u8 showSpoils;     /* +0xAC their spoils window */
    u8 unkAD;
    u8 unkAE;          /* menu module block loaded */
    u8 unkAF;
    u8 windows[7];     /* +0xB0 window shown */
    u8 unkB7;          /* command window page */
    u8 windowOpening[7]; /* +0xB8 */
    u8 windowOpen[7];    /* +0xBF fully open */
    u8 unkC6;
    u8 unkC7;
    u8 scriptPortraitShown; /* +0xC8 the event script's portrait quad (ScriptState) is drawn */
    u8 messageShown;   /* +0xC9 the message text window battle_message_text_window is drawn */
    u8 scriptLoaded;   /* +0xCA the event script is set up: its threads' waits count down */
    u8 unkCB;
    u8 unkCC[3];       /* per party member: its panel and gauge are drawn */
    u8 waitingCross;   /* +0xCF a prompt waits for a key: the input is kept */
    s32 unkD0[2];
    u8 unkD8[0xE0 - 0xD8];
    s32 unkE0[3];
    s32 unkEC[3];
    s32 unkF8;
    s32 unkFC;
    s32 cursorParts;   /* +0x100 its glyph's primitives */
    u16 unk104;
    u16 unk106;
    u8 unk108[4];
} BattleUi;

extern BattleUi *battle_ui;

/* A run of glyphs drawn together: how many parts, and the draw buffer they
 * were built for. */
typedef struct {
    u8 count;
    u8 buffer;
} GlyphRun;

/* A party member's result card (pointers 800d32f8, one per slot), which the
 * result screens (ovl2596) allocate and build: its glyph parts, each two
 * primitives (one per draw buffer), and the count the battle steps while
 * showCards is set (8008a684). */
typedef struct MemberCard {
    POLY_FT4 portrait[8];    /* 0x0000 */
    POLY_FT4 labels[40];     /* 0x0140, two per glyph (one per buffer) */
    POLY_FT4 field780[6];    /* 0x0780 */
    POLY_FT4 field870[6];    /* 0x0870 */
    POLY_FT4 field960[6];    /* 0x0960 */
    POLY_FT4 fieldA50[6];    /* 0x0A50 */
    POLY_FT4 fieldB40[4];    /* 0x0B40 */
    POLY_FT4 fieldBE0[4];    /* 0x0BE0 */
    POLY_FT4 fieldC80[16];   /* 0x0C80 */
    POLY_FT4 fieldF00[16];   /* 0x0F00 */
    POLY_FT4 field1180[14];  /* 0x1180 */
    POLY_FT4 field13B0[14];  /* 0x13B0 */
    GlyphRun runs[12];       /* 0x15E0 */
    u8 secondValue;          /* 0x15F8 (first card) option flag 0x8000 clear: the
                              * second value and its markers are shown, and the
                              * count waits for it too (done[1]); the battle
                              * steps it either way */
    u8 counting;             /* 0x15F9 (first card) set while the battle counts the
                              * values; 801e1aa4 waits for it to clear */
    u8 done[2];              /* 0x15FA each value counted out */
} MemberCard;

LAYOUT_CHECK(MemberCardSize, sizeof(MemberCard) == 0x15FC);

extern MemberCard *battle_member_cards[3];

/* Battle drawing state (800ccb04, the battle area's ordering table pointer
 * on). The battle work area (800ccce8) follows it within one aggregate: some
 * code addresses the work area from here. */
typedef struct {
    u32 *ot;           /* current ordering table */
    u8 unk4[0x2C];
    s32 buffer;        /* +0x30 draw buffer index */
    u8 unk34[0x1E4 - 0x34];
    BattleWork work;   /* +0x1E4 (battle_work_area) */
} BattleDraw;

extern BattleDraw battle_drawing_state;
extern u8 battle_drawing_buffer_byte;   /* battle_drawing_state.buffer's low byte, read on its own */

extern void *battle_glyph_table;   /* glyph table */
extern u8 battle_decimal_digits[9];   /* decimal digits */

/* A menu icon cell of the icon image (4 bytes, 800d2f68). */
typedef struct IconCell {
    u8 w;
    u8 alternate; /* uses the alternate CLUT */
    u8 u;
    u8 v;
} IconCell;

extern IconCell battle_icon_cells[];

/* Direction arrow block (*800c3e24, 0xec bytes). */
typedef struct DirectionArrows {
    POLY_G3 prims[8]; /* per direction, one per draw buffer */
    s32 shade;           /* +0xE0 pulsing red level */
    u8 buffer;           /* +0xE4 */
    u8 fading;           /* +0xE5 the shade is going down */
    u8 arrows[4];        /* +0xE6 a target lies that way */
    u8 unkEA[2];
} DirectionArrows;

extern DirectionArrows *battle_direction_arrows;

/* Images, glyphs and quads (80070E2C's unit). */
void battle_upload_image_and_wait(RECT *rect, u32 *pixels); /* upload an image and wait */
s32 battle_build_glyph(s32 id, POLY_FT4 *prims, s16 x, s16 y); /* build a glyph, full scale */
s32 battle_build_glyph_half_scale(s32 id, POLY_FT4 *prims, s16 x, s16 y); /* half scale */
void battle_quad_init_full_window_blend(POLY_FT4 *prim); /* set up a textured quad at full brightness */
void battle_quad_init_full_additive(POLY_FT4 *prim); /* the same with texture page bit 0x20 */
void battle_quad_init_full_subtractive(POLY_FT4 *prim); /* the same with bit 0x40 */
void battle_quad_init_half_subtractive(POLY_FT4 *prim); /* the same at half brightness */
void battle_quad_place_text_row(POLY_FT4 *prim, u16 x, u16 y, u8 u, u8 v, u8 w); /* place a quad 13 high */
void battle_quad_place(POLY_FT4 *prim, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h); /* place a quad */
void battle_show_direction_arrows(void); /* set up the direction arrows */
void battle_hide_direction_arrows(void);

/* Decimal digits and text images (battle.c), the cursor glyph (8008CCCC's
 * unit). */
void battle_split_decimal_digits(u32 value);  /* split a value into decimal digits */
void battle_animate_cursor_glyph(s32 x, s32 y, s32 *frame, u8 *ticks); /* animate a cursor glyph */

#endif
