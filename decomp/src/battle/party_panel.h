#ifndef BATTLE_PARTY_PANEL_H
#define BATTLE_PARTY_PANEL_H

#include "battle_core.h"

/* A party member's status panel of the graphics state (0x1E4 bytes). */
typedef struct {
    POLY_FT4 value[2][2];     /* +0x000 state 1: the value glyphs */
    POLY_FT4 alone[2][2];     /* +0x0A0 state 2: the glyphs placed alone */
    POLY_FT4 unk140[2][2];
    u8 buffer;                /* +0x1E0 */
    u8 state;                 /* +0x1E1 0 absent, 1 shown, 2 placed alone */
    u8 parts[2];              /* +0x1E2 glyph parts of value and alone */
} MemberPanel;

/* Party panel regions of the battle graphics state (*800c3ea4) that the
 * shared BattleGraphics layout still keeps as padding. */
typedef struct {
    POLY_FT4 gauge[3][6][2];  /* +0x0000 per member gauge glyphs */
    POLY_GT4 gaugeBars[8];    /* +0x05A0 two per panel slot, one per draw buffer */
    POLY_G4 shade[6];         /* +0x0740 two per member, one per draw buffer */
    POLY_FT4 portrait[3][2];  /* +0x0818 */
    u8 pad908[0x2E08 - 0x908];
    POLY_FT4 status[4][10][2]; /* +0x2E08 per member status glyphs; the
                                * fourth is the party-wide label */
    u8 pad3A88[0x835C - 0x3A88];
    MemberPanel panels[3];    /* +0x835C */
    DR_MODE unk8908[2];       /* +0x8908 per draw buffer */
    DR_MODE unk8920[2];       /* +0x8920 per draw buffer */
} PanelGraphics;

#define PANEL_GRAPHICS ((PanelGraphics *)D_800C3EA4)

/* A window's primitives (*800d2e38[window], 0x5a8 bytes). */
typedef struct {
    POLY_FT4 fill[8];         /* +0x000 */
    POLY_FT4 frame[8][2];     /* +0x140 edge pieces, one per draw buffer */
    POLY_G4 shade[2];         /* +0x3C0 background */
    DR_MODE mode[2];          /* +0x408 */
    u8 unk420[0x5A0 - 0x420];
    s32 fillCount;            /* +0x5A0 */
    u8 buffer;                /* +0x5A4 */
    u8 unk5A5[3];
} WindowPrims;

/* Party panel bytes of the UI state (*800d2d28) kept as padding there. */
#define UI_BAR_SHOWN(i)     (D_800D2D28->unk7F[i])       /* +0x7F */
#define UI_STATUS_PARTS(i)  (D_800D2D28->unk70[4 + (i)]) /* +0x74, [3] party-wide */
#define UI_GAUGE_PARTS(m)   (D_800D2D28->unk70[8 + (m)]) /* +0x78 */
#define UI_PORTRAIT_BUFFER  (D_800D2D28->unk7F[4])       /* +0x83 */
#define UI_STATUS_BUFFER(i) (D_800D2D28->unk7F[5 + (i)]) /* +0x84, [3] party-wide */
#define UI_GAUGE_BUFFER     (D_800D2D28->unk9F[3])       /* +0xA2 */

s32 func_80025FA8(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y,
                  s32 scaleX, s32 scaleY, s32 scale);
s32 func_80076A6C(s32 id, POLY_FT4 *prims, s16 x, s16 y);
void func_80076C34(POLY_FT4 *prim);
void func_800765C4(s32 member);
void func_80076710(s32 member);

#endif
