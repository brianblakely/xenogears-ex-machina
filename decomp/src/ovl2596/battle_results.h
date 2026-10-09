#ifndef OVL2596_BATTLE_RESULTS_H
#define OVL2596_BATTLE_RESULTS_H

/* The post-battle module: the battle overlay's result screen primitives and
 * digit buffer as it reads them, the growth data file, the module's own
 * functions, and the battle overlay and resident calls no shared header
 * declares. The battle overlay's other objects and calls (its area, work
 * area, UI state, turn state, item lists and the set-up and exit state) come
 * from the shared battle headers, the game data from resident/gamedata.h. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "battle/actions.h"
#include "battle/area.h"
#include "battle/combatant.h"
#include "battle/event_script.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/input.h"
#include "battle/lists.h"
#include "battle/menu_pages.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "battle/work.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"

/* Callers convert arguments/result differently from the resident definition
 * (u8 tags, ids and codes passed where the resident takes words, or the
 * reverse): heap tag reset, sound effect, text rendering and item names. */
void func_8003218C(s32 arg);
void func_80039DB8(s32 code);
s32 func_80034EAC(void *text, void *image, s32 mode, s32 flags); /* render text */
void *func_80033784(u8 id, s32 k);              /* counter skill names */
void *func_800337E8(u8 id);                     /* item names per list */
void *func_80033818(u8 id);
void *func_80033848(u8 id);
void *func_80033A2C(u8 id);
void *func_80033A5C(u8 id);

/* --- The result screens --------------------------------------------------- */

/* A glyph-table sprite part, one primitive per draw buffer. */
typedef POLY_FT4 Glyph[2];

/* The member cards and their glyph runs (GlyphRun, MemberCard, D_800D32F8)
 * are in battle/ui.h: the battle counts them up. */

/* The result summary windows' primitives (pointer 800d334c). */
typedef struct {
    Glyph title[4];           /* 0x0000, runs[0] */
    POLY_FT4 text[134];       /* 0x0140, runs[1]; two per glyph part */
    POLY_FT4 glyphs1630[6];   /* 0x1630, runs[2] */
    POLY_FT4 glyphs1720[4];   /* 0x1720, runs[4] */
    POLY_FT4 glyphs17C0[8];   /* 0x17C0, runs[3] */
    POLY_FT4 glyphs1900[6];   /* 0x1900, runs[5] */
    POLY_FT4 rowA[7][6];      /* 0x19F0 */
    POLY_FT4 rowB[7][8];      /* 0x2080 */
    POLY_G4 barA[7][2];       /* 0x2940 */
    POLY_G4 barB[7][2];       /* 0x2B38 */
    Glyph glyphs2D30[7];      /* 0x2D30 */
    Glyph glyphs2F60[6];      /* 0x2F60 */
    Glyph glyphs3140[9];      /* 0x3140 */
    Glyph glyphs3410[2];      /* 0x3410 */
    Glyph glyphs34B0[2];      /* 0x34B0 */
    Glyph listA[8];           /* 0x3550 */
    Glyph listB[8];           /* 0x37D0 */
    GlyphRun runs[6];         /* 0x3A50 */
    u8 rowACount[7];          /* 0x3A5C */
    u8 rowABuffer[7];         /* 0x3A63 */
    u8 rowBCount[7];          /* 0x3A6A */
    u8 rowBBuffer[7];         /* 0x3A71 */
    u8 barBuffer[7];          /* 0x3A78 */
    u8 count2D30;             /* 0x3A7F */
    u8 buffer2D30;            /* 0x3A80 */
    GlyphRun run2F60;         /* 0x3A81 */
    GlyphRun run3140;         /* 0x3A83 */
    u8 listCount;             /* 0x3A85 */
    u8 listBuffer;            /* 0x3A86 */
    u8 buffer3410;            /* 0x3A87 */
    u8 buffer34B0[2];         /* 0x3A88 */
} ResultSummary;

extern ResultSummary *D_800D334C;

/* func_8008AAA0 writes a value's nine decimal digits to 800c3cf4, leading
 * zeros as 0xff; the screens read the last digits through these views. */
extern u8 D_800C3CF1[12];
extern u8 D_800C3CED[16];
extern u8 D_800C3CE8[];     /* as the level numbers address it */
extern u8 D_800C3CE3[];
extern u8 D_800C3CFA[];
extern u8 D_800C3CFB[];
extern u8 D_800C3CDF[];
extern u8 D_800C3CD7[];
extern u8 D_800C3CDC[];     /* the spoils window's */

/* Battle slot levels (8 bytes per slot). */
typedef struct {
    u8 level;
    u8 level2;
    u8 pad[6];
} SlotLevels;

extern SlotLevels D_800D32A5[3];

/* The summary window's text: 27 glyph entries of three bytes (glyph, 0xff:
 * none; shaded flag; index into the shading colours) and their positions. */
extern u8 D_800C32C4[27 * 3];
extern s16 D_800C3318[27];
extern s16 D_800C3350[27];

/* The seven glyphs of the summary's 2d30 label: ids and positions. */
extern u8 D_800C3388[8];
extern s16 D_800C3390[8];
extern s16 D_800C33A0[8];

/* The member card's label glyphs: ids and positions. */
extern u8 D_800C3268[18];
extern s16 D_800C327C[18];
extern s16 D_800C32A0[18];

void func_8008F8F4(s32 id, u16 x, u16 y, u16 width, u16 height, s32 style, s32 arg6); /* open a window */
void func_8008FA60(s32 id);     /* close a window */
void func_80076D58(POLY_FT4 *prims, s32 arg1, s32 arg2);
void *func_8008AC00(s32 kind);                  /* allocate a text image */
void func_800716D8(void);       /* run one battle frame */

extern u8 D_800D2F90[8];        /* two icon records: arg5, -, arg3, arg4 */
extern u8 D_800D2FA0[4];        /* the skill mark icon: width, -, u, v */
extern void *D_800D2C08[1];     /* the results text; the original addresses it as a table */

void func_801DE1C4(void);
void func_801DE408(void);
void func_801DFF50(u8 member);
void func_801DF710(POLY_G4 *bar, u8 colour);
void func_801DF840(POLY_FT4 *prims, u8 blue, u8 count, u8 buffer);
void func_801DF910(u8 from, u8 to, s32 max);
s32 func_801DFA38(u8 slot);
void func_801E0184(u8 member);
void func_801E0ACC(u8 member);
void func_801E1370(u8 id, u8 count, u8 *ids, u8 *counts, u8 size);
void func_801E1690(void);

/* --- Experience, growth and skills --------------------------------------- */

/* Per character growth data (0x110 each; the block 801e44e8 points to). */
typedef struct {
    u16 requirements[13][7];  /* 0x00: counter thresholds per counter skill */
    u8 padB6[2];
    u16 maxHpTargets[2];      /* 0xB8: below level 100, from 100 */
    u8 statTargets[6][2];     /* 0xBC: stats 58 59 5e 5f 5b 5c, per level range */
    u8 maxEpTargets[2];       /* 0xC8 */
    u8 padCA[2];
    u8 tierLevels[3];         /* 0xCC: levels for tiers 4, 5 and 6 */
    u8 padCF;
    u8 unlocksA[16];          /* 0xD0: 0xff ends */
    u8 unlocksB[16];          /* 0xE0: 0 ends */
    u8 levelSkills[16];       /* 0xF0: 0xff ends */
    u8 counterLevels[16];     /* 0x100 */
} Growth;

/* The growth data file (BattleWork.growth): one block per character. */
typedef struct GrowthFile {
    Growth characters[11];
    s32 experience[99];       /* 0xBB0: experience to the next level, per level - 1 */
} GrowthFile;

void func_801E2794(void);
void func_801E2ACC(void);
void func_801E2EB0(u32 experience, s16 slot, s16 reserve);
void func_801E308C(void);          /* experience pool for level B */
void func_801E335C(void);
void func_801E3500(void);
u8 func_801E3610(u8 stat, u8 target, u8 cap, u8 level);
u16 func_801E3700(u16 maxHp, u8 level);
u8 func_801E38CC(u8 maxEp, u8 level);
void func_801E3A18(void);
u8 func_801E3BE0(u8 id);
u8 func_801E3D54(u8 id);
void func_801E3E14(u8 id);
void func_801E3EA4(void);
void func_801E3F28(u8 id);
void func_801E3FB0(void);
void func_801E403C(void);
void func_801E41B4(void);
void func_801E42C4(void);
void func_801E2888(void);

/* --- Rewards, the results resources and the battle exit ------------------ */

extern u8 D_800C3D1B[8][4];     /* per enemy: [0] nonzero, no rewards */
u16 func_80089C08(u8 enemy);
void func_800BCD98(s32 arg);
void func_801E1FB8(u32 experience);
void func_801E211C(void);
void func_801E24B0(void);

/* The results archive (directory 0x10 file 2): a count, then its items. */
typedef struct {
    s32 count;
    void *items[4];
} ResultArchive;
void func_80078310(void *portraits, s32 glyph);
void *func_8008ABB8(s32 size, s32 top);        /* heap allocate */

/* Battle exit (func_801E252C). */
typedef struct {
    void *data;
    u8 pad[0x5C];
} BattleBlock;
extern BattleBlock D_800D3720[8]; /* every other one is released */
void func_800B8774(void);     /* levels A and B per slot before the battle */

#endif
