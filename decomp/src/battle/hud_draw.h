#ifndef BATTLE_HUD_DRAW_H
#define BATTLE_HUD_DRAW_H

#include "battle_core.h"

/* The six outputs of func_80026338 for one sprite. */
typedef struct {
    s32 unk0;
    s32 tpageMode;
    s32 clutX;
    s32 clutY;
    s32 pageX;
    s32 pageY;
} SpriteInfo;

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
    u8 pad8938[0xA234 - 0x8938]; /* +0x8950 the CLUT rows and their saved copies */
    SpriteInfo sprites[5];    /* +0xA234 */
    u16 barCluts[4];          /* +0xA2AC time bar: normal, party-wide, slow, haste */
} PanelGraphics;

#define PANEL_GRAPHICS ((PanelGraphics *)D_800C3EA4)

/* Command panel primitives of the graphics block (*800c3ea4 + 0xa230) that
 * the shared GraphicsBlock layout keeps as padding. */
typedef struct {
    u8 pad0[0x3C0];
    POLY_FT4 unk3C0[2];       /* +0x3C0 */
    POLY_FT4 unk410[6][2];    /* +0x410 */
    POLY_G4 unk5F0[2];        /* +0x5F0 */
    DR_MODE unk638[2][2];     /* +0x638 0x18 bytes per draw buffer */
} CommandBlock;

#define COMMAND_BLOCK ((CommandBlock *)D_800C3EA4->unkA230)

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
#define UI_TEXTURE_WINDOWS ((RECT *)D_800D2D28->unk0) /* +0x00 six texture windows */
#define UI_BAR_SHOWN(i)     (D_800D2D28->unk7F[i])       /* +0x7F */
#define UI_STATUS_PARTS(i)  (D_800D2D28->unk70[4 + (i)]) /* +0x74, [3] party-wide */
#define UI_GAUGE_PARTS(m)   (D_800D2D28->unk70[8 + (m)]) /* +0x78 */
#define UI_PORTRAIT_BUFFER  (D_800D2D28->unk7F[4])       /* +0x83 */
#define UI_STATUS_BUFFER(i) (D_800D2D28->unk7F[5 + (i)]) /* +0x84, [3] party-wide */
#define UI_GAUGE_BUFFER     (D_800D2D28->unk9F[3])       /* +0xA2 */

/* The primitive lists of *800d2db4 with their counts and draw buffers; the
 * shared BattleUnk2DB4 layout keeps most of them as padding. */
typedef struct {
    POLY_FT4 extra0[24];      /* +0x0000 count extraCounts[0] */
    POLY_FT4 extra1[60];      /* +0x03C0 count extraCounts[1] */
    POLY_FT4 extra2[16];      /* +0x0D20 count extraCounts[2] */
    POLY_FT4 extra3[48];      /* +0x0FA0 count extraCounts[3] */
    POLY_FT4 extra4[90];      /* +0x1720 count extraCounts[4] */
    POLY_FT4 list9[86];       /* +0x2530 */
    POLY_FT4 unk32A0[52];     /* +0x32A0 count +0x5D96, buffer +0x5D97 */
    POLY_FT4 list0[22];       /* +0x3AC0 */
    POLY_FT4 list1[2];        /* +0x3E30 */
    POLY_FT4 list2[34];       /* +0x3E80 */
    POLY_FT4 list10[18];      /* +0x43D0 */
    POLY_FT4 unk46A0[40];     /* +0x46A0 twenty drawn, buffer +0x5D98 */
    POLY_FT4 unk4CE0[10];     /* +0x4CE0 count +0x5DA1, buffer +0x5DA0 */
    POLY_FT4 list3[8];        /* +0x4E70 */
    POLY_FT4 list4[6];        /* +0x4FB0 */
    POLY_FT4 list5[8];        /* +0x50A0 */
    POLY_FT4 list6[4];        /* +0x51E0 */
    POLY_FT4 list7[8];        /* +0x5280 */
    POLY_FT4 list8[10];       /* +0x53C0 */
    POLY_FT4 list11[6];       /* +0x5550 */
    POLY_FT4 list12[40];      /* +0x5640 */
    POLY_FT4 list13[6];       /* +0x5C80 */
    u8 extraCounts[5];        /* +0x5D70 */
    u8 counts[14];            /* +0x5D75 per list */
    u8 extraBuffer4;          /* +0x5D83 */
    u8 buffers[14];           /* +0x5D84 per list */
    u8 extraBuffers[4];       /* +0x5D92 extra0..extra3 */
    u8 count32A0;             /* +0x5D96 */
    u8 buffer32A0;            /* +0x5D97 */
    u8 buffer46A0;            /* +0x5D98 */
    u8 unk5D99[3];
    s16 unk5D9C;
    s16 unk5D9E;
    u8 buffer4CE0;            /* +0x5DA0 */
    u8 count4CE0;             /* +0x5DA1 */
    s16 blink;                /* +0x5DA2 frame counter of the blinking list */
} ListPrims;

#define LIST_PRIMS ((ListPrims *)D_800D2DB4)

/* A turn slot's five four-glyph number strings (+0x08 of TurnSlot, kept as
 * padding there). */
#define SLOT_DIGITS(slot, k) (&D_800C3EAC->slots[slot].unk0[8 + (k) * 4])
extern u8 D_800D2C0C[3][2]; /* values shown in number strings 2-4 */
void func_8009A2D4(u8 member);
void func_80089AF8(u8 member);
void func_800898F0(u8 member);

extern s32 D_800CCB34; /* the draw buffer index (BattleDraw +0x30) */
extern u32 *D_800C3E5C[];   /* text images of battle messages 0-9 */
void *func_800338D8(s32 id); /* a battle message text */


void func_80026338(void *sheet, s32 id, s32 *a, s32 *tpageMode, s32 *clutX, s32 *clutY,
                   s32 *pageX, s32 *pageY);
extern u8 D_800D3294;
s32 func_80025FA8(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y,
                  s32 scaleX, s32 scaleY, s32 scale);
s32 func_80076A6C(s32 id, POLY_FT4 *prims, s16 x, s16 y);
void func_80076C34(POLY_FT4 *prim);
void func_800765C4(s32 member);
void func_80076710(s32 member);

#endif
