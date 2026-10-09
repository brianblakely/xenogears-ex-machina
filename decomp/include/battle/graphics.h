#ifndef BATTLE_GRAPHICS_H
#define BATTLE_GRAPHICS_H

#include "common.h"
#include "psyq/libgpu.h"

/* The battle graphics state: the party panel's gauges, portraits, status
 * glyphs, name glyphs and values, the command panel's block, and the CLUT
 * cycle (battle overlay; the event script overlay ovl3087 draws its portrait
 * quads here). */

/* The 0x670-byte graphics block (*800c3ea4 + 0xa230). */
typedef struct {
    POLY_FT4 unk0[2];
    POLY_FT4 unk50[2];
    POLY_FT4 unkA0[2];
    POLY_FT4 unkF0[2];
    POLY_FT4 unk140[2]; /* page title quads */
    POLY_FT4 unk190[6]; /* second title quad, or up to three cost digits */
    POLY_FT4 unk280[2];
    POLY_FT4 unk2D0[2];
    POLY_FT4 unk320[2];
    POLY_FT4 unk370[2];
    POLY_FT4 unk3C0[2]; /* EP page icons */
    POLY_FT4 unk410[2];
    POLY_FT4 unk460[2]; /* EP digits */
    POLY_FT4 unk4B0[2];
    POLY_FT4 unk500[2]; /* maximum EP digits */
    POLY_FT4 unk550[2];
    POLY_FT4 unk5A0[2]; /* EP label glyphs */
    POLY_G4 unk5F0[2];
    POLY_F4 unk638[2];
    u8 unk668;         /* buffer of the +0x0..+0xf0 quads */
    u8 unk669;
    u8 buffer;         /* +0x66A */
    u8 unk66B;
    u8 unk66C;
    u8 unk66D;
    u8 unk66E;         /* cost digits shown */
    u8 unk66F;
} GraphicsBlock;

/* A party member's status panel of the graphics state (0x1E4 bytes). */
typedef struct {
    POLY_FT4 value[2][2];     /* +0x000 state 1: the value glyphs */
    POLY_FT4 gear[2][2];      /* +0x0A0 state 2: the glyphs of a member in a gear */
    POLY_FT4 unk140[2][2];
    u8 buffer;                /* +0x1E0 */
    u8 state;                 /* +0x1E1 0 absent, 1 on foot, 2 in a gear (ovl2615; not character 7) */
    u8 parts[2];              /* +0x1E2 glyph parts of value and gear */
} MemberPanel;

/* The six outputs of func_80026338 for one sprite (0x18 bytes). */
typedef struct {
    s32 unk0;
    s32 tpageMode;
    s32 clutX;
    s32 clutY;
    s32 pageX;
    s32 pageY;
} SpriteInfo;

/* Battle graphics state (*800c3ea4). */
typedef struct BattleGraphics {
    POLY_FT4 gauge[3][6][2];  /* +0x0000 per member gauge glyphs */
    POLY_GT4 gaugeBars[8];    /* +0x05A0 two per panel slot, one per draw buffer */
    POLY_G4 shade[6];         /* +0x0740 two per member, one per draw buffer */
    POLY_FT4 portrait[3][2];  /* +0x0818 */
    LINE_F2 unk908[12];
    POLY_FT4 unk9C8[6][2];
    POLY_FT4 unkBA8[120];
    POLY_FT4 unk1E68[60];
    POLY_FT4 cursor[2];       /* +0x27C8 the five-frame cursor glyph (0xe0-0xe4),
                               * per draw buffer: the menus' and the event
                               * script messages' */
    u8 unk2818[0x2E08 - 0x2818];
    POLY_FT4 status[4][10][2]; /* +0x2E08 per member status glyphs; the
                                * fourth is the party-wide label */
    POLY_FT4 unk3A88[3][80]; /* party panel name glyphs */
    POLY_FT4 unk6008[3][8];  /* party panel value digits */
    POLY_F4 unk63C8[2];
    DR_MODE unk63F8[2];
    s32 unk6410;       /* shade */
    u8 unk6414;
    u8 unk6415;
    u8 unk6416;        /* fading down */
    u8 unk6417[5];
    POLY_FT4 unk641C[2][100];
    MemberPanel panels[3];   /* +0x835C */
    DR_MODE unk8908[2];      /* +0x8908 per draw buffer */
    DR_MODE unk8920[2];      /* +0x8920 per draw buffer */
    u8 unk8938[0x8950 - 0x8938];
    RECT unk8950[4];         /* the CLUT rows */
    u32 unk8970[4][0x630 / 4]; /* four CLUT strips, cycled */
    GraphicsBlock *unkA230;
    SpriteInfo sprites[5];   /* +0xA234 */
    u16 barCluts[4];         /* +0xA2AC time bar: normal, party-wide, slow, haste */
} BattleGraphics;

extern BattleGraphics *D_800C3EA4;

/* The party panel: its layout, name glyphs and values. */
extern u8 D_800D3280;      /* party panel layout */
extern u16 D_800C3254[]; /* party panel x: [layout * 3 + member] */
extern s16 D_800C3068[]; /* party panel glyph x: [member * 24 + glyph] */
extern u8 D_800D2D88[5];   /* name glyph codes */
extern u8 D_800C3214[5][6]; /* list separator rows by list size (3-7) */
extern s16 D_800D3330;     /* panel member maximum HP */
extern s16 D_800D2FE0;     /* its digits' remainder */
extern s16 D_800D2E58;     /* panel member HP */
extern s16 D_800D2D38;     /* its digits' remainder */
extern s32 D_800D333C;     /* panel gear HP */
extern s32 D_800D3018;     /* its digits' remainder */
extern s32 D_800D3668;     /* panel gear maximum HP */
extern u8 D_800C3E08[3]; /* panel value digits */
extern u8 D_800D2D54[7]; /* panel maximum digits */

/* 80070E2C's unit. */
void func_800728B8(POLY_FT4 *prims, s32 count, s32 first); /* add every other primitive to the OT */
void func_80077610(void); /* allocate and set up the graphics block */
void func_8007765C(void); /* release it after a frame */

#endif
