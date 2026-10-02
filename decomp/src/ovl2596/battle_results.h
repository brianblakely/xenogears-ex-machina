#ifndef OVL2596_BATTLE_RESULTS_H
#define OVL2596_BATTLE_RESULTS_H

#include "common.h"

/* libgpu primitives (PsyQ layout). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad1;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad2;
} POLY_FT4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 r3, g3, b3, pad3;
    s16 x3, y3;
} POLY_G4;

#define setRGB0(p, r, g, b) ((p)->r0 = (r), (p)->g0 = (g), (p)->b0 = (b))

/* A glyph-table sprite part, one primitive per draw buffer. */
typedef POLY_FT4 Glyph[2];

/* A run of glyphs drawn together: how many parts, and the draw buffer they
 * were built for. */
typedef struct {
    u8 count;
    u8 buffer;
} GlyphRun;

/* Per party member result card (pointers 800d32f8, one per slot). Each
 * glyph part is two primitives, one per draw buffer. */
typedef struct {
    POLY_FT4 portrait[8];    /* 0x0000 */
    POLY_FT4 labels[40];  /* 0x0140, two per glyph (one per buffer) */
    POLY_FT4 field780[6];    /* 0x0780 */
    POLY_FT4 field870[6];    /* 0x0870 */
    POLY_FT4 field960[6];    /* 0x0960 */
    POLY_FT4 fieldA50[6];    /* 0x0A50 */
    POLY_FT4 fieldB40[4];    /* 0x0B40 */
    POLY_FT4 fieldBE0[4];    /* 0x0BE0 */
    POLY_FT4 fieldC80[16];    /* 0x0C80 */
    POLY_FT4 fieldF00[16];    /* 0x0F00 */
    POLY_FT4 field1180[14];   /* 0x1180 */
    POLY_FT4 field13B0[14];   /* 0x13B0 */
    GlyphRun runs[12];    /* 0x15E0 */
    u8 flag15F8;          /* 0x15F8 */
    u8 flag15F9;          /* 0x15F9 */
} MemberCard;

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

/* The battle UI state block (pointer 800d2d28). */
typedef struct {
    u8 pad0[0x7F];
    u8 unk7F;             /* 0x7F */
    u8 unk80;             /* 0x80 */
    u8 unk81;             /* 0x81 */
    u8 pad82[0xD];
    u8 show8F;            /* 0x8F */
    u8 pad90[0x10];
    u8 showCards;         /* 0xA0 */
    u8 showSummary;       /* 0xA1 */
    u8 padA2[0xA];
    u8 showSpoils;        /* 0xAC */
    u8 padAD[3];
    u8 unkB0;             /* 0xB0 */
    u8 unkB1;             /* 0xB1 */
    u8 unkB2;             /* 0xB2 */
    u8 padB3[0x1C];
    u8 waitingCross;      /* 0xCF: the prompt waits for Cross */
} BattleUi;

/* Battle slot info (800c3eb4, 0x1c per slot); the symbol names its
 * character id byte. */
typedef struct {
    u8 id;                /* 0x7f: empty */
    u8 flag01;            /* nonzero: no rewards for this enemy */
    u8 pad[0x1A];
} SlotInfo;

extern BattleUi *D_800D2D28;
extern MemberCard *D_800D32F8[3];
extern SlotInfo D_800C3EB6[];
/* The battle draw state (800ccb00). */
typedef struct {
    void *frame;          /* 0x00: the frame being built */
    u32 *ot;              /* 0x04: its ordering table */
    u8 pad8[0x2C];
    s32 buffer;           /* 0x34: current draw buffer */
} DrawState;

extern DrawState D_800CCB00;
extern s32 D_800CCB34;      /* D_800CCB00.buffer, also addressed directly */

/* func_8008AAA0 writes a value's nine decimal digits to 800c3cf4, leading
 * zeros as 0xff; the cards read the last digits through these two views. */
extern u8 D_800C3CF1[12];
extern u8 D_800C3CED[16];
void func_8008AAA0(u32 value);

/* The digit buffer as the level numbers address it. */
extern u8 D_800C3CE8[];

/* Battle slot levels (8 bytes per slot). */
typedef struct {
    u8 level;
    u8 level2;
    u8 pad[6];
} SlotLevels;

extern SlotLevels D_800D32A5[3];

/* The game data's character records (8006d634 + 0x26c, 0xa4 each). */
typedef struct {
    u8 pad0[0x3A];
    u16 value_3A;         /* 0x3A */
    u32 value3C;          /* 0x3C */
    u32 value40;          /* 0x40 */
    u8 pad44[8];
    u16 hp;               /* 0x4C */
    u16 maxHp;            /* 0x4E */
    u16 ep;               /* 0x50 */
    u16 maxEp;            /* 0x52 */
    u8 pad54[0xE];
    u8 level;             /* 0x62 */
    u8 level2;            /* 0x63 */
    u8 pad64[0x2C];
    u16 counters[7];      /* 0x90 */
    u8 pad9E[0xA4 - 0x9E];
} Character;

extern Character D_8006D8A0[];
extern u8 D_800D2D24[3];    /* battle party character ids */

/* Per member combatant values (0x170 per member; the symbol names +0x4c). */
typedef struct {
    u16 value4C;
    u16 pad4E;
    u16 value50;
    u8 pad52[0x170 - 6];
} MemberStats;

extern MemberStats D_800CCD34[3];

/* Two pairs of 32-bit values per member (8 digits and 7 digits wide). */
typedef struct {
    u32 value;
    u32 value2;
} MemberWide;

extern MemberWide D_800CDCB8[3];
extern MemberWide D_800CDCD0[3];
extern u8 D_800C3CE3[];
extern u8 D_800C3CFA[];
extern u8 D_800C3CFB[];
extern u8 D_800C3CDF[];     /* digit buffer views, see D_800C3CF1 */
extern u8 D_800C3CD7[];

/* Two further values per member. */
typedef struct {
    u16 valueA;
    u16 valueB;
} MemberPair;

extern MemberPair D_800CDCE8[3];

/* The level gauge animation (module data). */
extern s32 D_801E44CC;      /* start value */
extern s32 D_801E44D0;      /* end value */
extern s32 D_801E44D4;      /* distance */
extern s32 D_801E44D8;      /* start length */
extern s32 D_801E44DC;      /* distance length */
extern u8 D_801E44E0;       /* bar colour */
extern u8 D_801E44E4;       /* arrow glyph */

/* Per slot byte tables, 8 bytes per slot; the second set starts 3 slots on. */
extern u8 D_800CDD10[6][8];

/* The summary window's text: 27 glyph entries and their positions. */
typedef struct {
    u8 glyph;             /* 0xff: none */
    u8 shaded;
    u8 colour;            /* index into the shading colours */
} SummaryGlyph;

extern SummaryGlyph D_800C32C4[27];
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

void AddPrim(void *ot, void *prim);                   /* AddPrim */
void SetShadeTex(void *prim, s32 textured);               /* SetShadeTex */
void SetPolyG4(POLY_G4 *prim);                          /* SetPolyG4 */
void func_800728B8(POLY_FT4 *prims, s32 count, s32 buffer);
s32 func_80076A10(s32 id, POLY_FT4 *prims, s16 x, s16 y);   /* glyph sprite */

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

/* The growth data file: one block per character. */
typedef struct {
    Growth characters[11];
    s32 experience[99];       /* 0xBB0: experience to the next level, per level - 1 */
} GrowthFile;

/* A gear record (0xa4 bytes), in the game data and in each combatant. */
typedef struct {
    u8 pad0[0x38];
    u16 fuel;                 /* 0x38 */
    u16 maxFuel;              /* 0x3A */
    u8 pad3C[0x24];
    u32 hp;                   /* 0x60 */
    u32 maxHp;                /* 0x64 */
    u8 pad68[0x14];
    u16 flags;                /* 0x7C: 0x8000 destroyed */
    u8 pad7E[0xA4 - 0x7E];
} Gear;

/* Per character skill state in the game data (0x20 each, at +0x16c0). */
typedef struct {
    u16 counterSkills;        /* 0x00 */
    u16 levelSkills;          /* 0x02 */
    u16 unlocksA;             /* 0x04 */
    u16 unlocksB;             /* 0x06 */
    u8 pad8[0xF];
    u8 tier;                  /* 0x17 */
    u8 pad18[8];
} CharacterSkills;

/* The persistent game data (8006d634). */
typedef struct {
    u8 pad0[0x26C];
    Character characters[11]; /* 0x26C */
    union {
        Gear gears[17];       /* 0x978: per gear id; 7 is not a gear */
        struct {
            u8 pad[0xE30 - 0x978];
            u8 value_E30;     /* 0xE30 */
            u8 padE31[0x27];
            s32 value_E58;    /* 0xE58 */
            u8 padE5C[8];
            s16 value_E64;    /* 0xE64 */
            s16 value_E66;    /* 0xE66 */
        } id7;                /* character 7's derived values */
    } u978;
    u8 pad145C[0x16C0 - 0x145C];
    CharacterSkills skills[11]; /* 0x16C0 */
    u8 pad1820[0x22B6 - 0x1820];
    u16 options;              /* 0x22B6: 8006f8ea */
} GameData;

/* A battle combatant record (0x170 each from 800ccce8). It begins with the
 * character record's layout, and D_801E44EC also points at game data
 * character records (func_801E2ACC). */
typedef struct {
    u8 pad0[4];
    u8 value04;               /* 0x04 */
    u8 pad5[0x17];
    u8 value1C;               /* 0x1C */
    u8 pad1D[0x10];
    u8 value2D;               /* 0x2D */
    u8 pad2E[4];
    u16 flags32;              /* 0x32: 0x2000 and 0x1000 raise the experience gained */
    u8 pad34[6];
    u16 value_3A;             /* 0x3A */
    s32 totalA;               /* 0x3C: experience totals (levels A and B) */
    s32 totalB;               /* 0x40 */
    s32 nextA;                /* 0x44: experience to the next level */
    s32 nextB;                /* 0x48 */
    u16 hp;                   /* 0x4C */
    u16 maxHp;                /* 0x4E */
    u16 ep;                   /* 0x50 */
    u16 maxEp;                /* 0x52 */
    u8 pad54[2];
    u8 id;                    /* 0x56 */
    u8 pad57;
    u8 attack;                /* 0x58: grown stats 58, 59, 5e, 5f (level A) */
    u8 stat59;                /* 0x59 */
    u8 stat5A;                /* 0x5A */
    u8 stat5B;                /* 0x5B: grown stats 5b, 5c (level B) */
    u8 stat5C;                /* 0x5C */
    u8 pad5D;
    u8 stat5E;                /* 0x5E */
    u8 stat5F;                /* 0x5F */
    u8 pad60[2];
    u8 level;                 /* 0x62 */
    u8 level2;                /* 0x63 */
    u8 pad64[0x7C - 0x64];
    u16 flags7C;              /* 0x7C: 0x8000 knocked out */
    u8 pad7E[0x90 - 0x7E];
    u16 counters[7];          /* 0x90 */
    u8 pad9E[2];
    u8 gearId;                /* 0xA0 */
    u8 padA1[3];
    Gear gear;                /* 0xA4 */
    u8 pad148[4];
    u32 experience;           /* 0x14C: enemy experience */
    u8 dropChances[2];        /* 0x150: enemy drop chances (percent) */
    u8 dropIds[2];            /* 0x152 */
    u8 dropCategories[2];     /* 0x154 */
    u16 gold;                 /* 0x156: enemy gold */
    u8 weightA;               /* 0x158: experience share weights of levels A and B */
    u8 weightB;               /* 0x159 */
    u8 flags15A;              /* 0x15A: 0x80 (character 7) HP from the gear HP */
    u8 pad15B[0x170 - 0x15B];
} Combatant;

extern GameData D_8006D634;
extern Combatant D_800CCCE8[];
extern GameData *D_801E44C4;    /* 8006d634 */
/* The drops rolled for the defeated enemies (800ccce8 + 0x100c). */
typedef struct {
    u8 categories[8];
    u8 ids[8];
} Drops;

/* The battle work area (800ccce8): the combatant records, then the
 * results state. */
typedef struct {
    Combatant records[11];    /* 0x0000 */
    u8 padFD0[0xFE8 - 0xFD0];
    MemberWide gained[3];     /* 0xFE8: experience pools per slot (800cdcd0) */
    u8 pad1000[0x100C - 0x1000];
    Drops drops;              /* 0x100C: 800cdcf4 */
    u8 learntCounter[3];      /* 0x101C: counter skill learnt per slot */
    u8 learntLevel[3];        /* 0x101F: level skill learnt per slot */
    u8 levelGains[3][2];      /* 0x1022: levels A and B gained per slot */
    u8 pad1028[0x1040 - 0x1028];
    u8 stats[3][8];           /* 0x1040: the result stats per slot */
    u8 pad1058[0x5F20 - 0x1058];
    GrowthFile *growth;       /* 0x5F20 */
    u8 pad5F24[0x5F9C - 0x5F24];
    u32 experience;           /* 0x5F9C: experience won */
    u8 pad5FA0[0x5FB4 - 0x5FA0];
    u16 defeated;             /* 0x5FB4: enemies defeated, bit per enemy */
    u8 pad5FB6[0x5FC4 - 0x5FB6];
    s8 penalty;               /* 0x5FC4: experience lost in quarters */
} BattleWork;

extern BattleWork *D_801E44C8;  /* 800ccce8 */
extern GrowthFile *D_801E44E8;  /* the growth data file */
extern Combatant *D_801E44EC;   /* the record being processed */
extern u16 D_8006F8EA;          /* option flags */
extern u32 D_801E44F0;          /* experience pool for level A */
extern u32 D_801E44F4;
extern u8 D_801E44F8[3][2];

/* The battle's winnings (800ccce8 + 0x5f9c). */
typedef struct {
    u32 experience;           /* 0x00 */
    u8 pad4[0x14];
    u16 defeated;             /* 0x18: 800d2c9c */
} Winnings;
extern Winnings D_800D2C84;
extern u8 D_800D2FC4;           /* rewards are skipped */
extern u8 D_800D2DCC[11];       /* per slot: present */
extern u8 D_800C3D1B[8][4];     /* per enemy: [0] nonzero, no rewards */
extern u8 D_800D3294;
extern u8 D_800D2D50;
extern u8 D_8006F9DD;
u16 func_80089C08(u8 enemy);
void func_800BCD98(s32 arg);
void func_801E1FB8(u32 experience);
void func_801E2794(void);
void func_801E211C(void);
void func_801E24B0(void);

/* Battle exit (func_801E252C). */
typedef struct {
    void *data;
    u8 pad[0x5C];
} BattleBlock;
extern BattleBlock D_800D3720[8]; /* every other one is released */
typedef struct {
    void *data;
} BattleHandle;
extern BattleHandle D_800C3E5C[10];
extern u8 D_800C3D48;
extern void *D_800D3284;
extern void *D_800D328C;
extern void *D_800D329C;
extern void *D_800C3E24;
extern void *D_800D39F0;
extern void *D_800C3EA4;
extern u8 D_800594F8;
extern u8 D_800D3338;
extern u8 D_8005947C;
extern u16 D_8006F94E;
extern u8 D_800594D0;
extern s32 D_800C3E54;
void func_8001ACA4(void);
void func_800199CC(s32 mode);
void func_800BFBA0(void);
void func_8003218C(s32 arg);
void func_80039C4C(s32 arg);
void func_800399D4(s32 arg);
void func_800B8774(void);     /* levels A and B per slot before the battle */
void func_801E2EB0(u32 experience, s16 slot, s16 reserve);
void func_801E308C(void);          /* experience pool for level B */
void func_801E335C(void);
u8 func_801E3BE0(u8 id);
u8 func_801E3D54(u8 id);
void func_801E3E14(u8 id);
void func_801E3EA4(void);
void func_801E3F28(u8 id);
void func_801E3FB0(void);
void func_801E3500(void);

/* Sound and input. */
typedef struct {
    u8 pad[0x14];
    u16 id;               /* 0x14 */
} SoundBank;

extern SoundBank *D_8005919C;   /* the system effect bank */
extern u8 D_80059180;
extern u8 D_801E44C0;           /* the result fanfare has started */
extern u8 D_800D3014;           /* decoded input command, 4 = Cross */
void func_80039E60(s32 code);   /* start a sound effect */
void func_80039DB8(s32 code);
void func_8008F8F4(s32 id, u16 x, u16 y, u16 width, u16 height, s32 style, s32 arg6); /* open a window */
void func_8008FA60(s32 id);     /* close a window */
void func_80076D58(POLY_FT4 *prims, s32 arg1, s32 arg2);
void func_80076C78(POLY_FT4 *prim, u16 x, u16 y, u8 u, u8 v, u8 width);

typedef struct {
    s16 x, y, w, h;
} RECT;

void *func_8008AC00(s32 kind);                  /* allocate a text image */
s32 func_80034EAC(void *text, void *image, s32 mode, s32 flags); /* render text */
void func_800769E8(RECT *rect, void *image);   /* load an image to VRAM */
void func_800320E8(void *block);                /* heap release */
void *func_80033848(u8 id);                     /* item names per list */
void *func_800337E8(u8 id);
void *func_80033818(u8 id);
void *func_80033A5C(u8 id);
void *func_80033A2C(u8 id);

/* The spoils window: experience and gold digits, the item icons. */
extern u8 D_800C3CDC[];       /* digit buffer view (see D_800C3CF1) */
extern u32 D_8006EF58;        /* party gold */
extern u8 D_800D2F90[8];      /* two icon records: arg5, -, arg3, arg4 */
extern u8 D_800D2FE4[48];     /* battle item ids */
extern u8 D_800D2CB0[48];     /* battle item counts */
/* The inventory: five lists, each its counts then its ids. */
typedef struct {
    u8 counts0[100];
    u8 ids0[100];
    u8 counts1[200];
    u8 ids1[200];
    u8 counts2[150];
    u8 ids2[150];
    u8 counts3[100];
    u8 ids3[100];
    u8 counts4[150];
    u8 ids4[150];
} Inventory;
extern Inventory D_8006F36C;

extern Drops D_800CDCF4;

void func_801E1370(u8 id, u8 count, u8 *ids, u8 *counts, u8 size);
extern u8 D_8006F65A[150];    /* inventory list 2 ids */
extern u8 D_8006F5C4[150];    /* inventory list 2 counts */
void func_800716D8(void);       /* run one battle frame */

void func_801DE1C4(void);
void func_801DE408(void);
void func_801DFF50(u8 member);
void func_801DF710(POLY_G4 *bar, u8 colour);
void func_801DF840(POLY_FT4 *prims, u8 blue, u8 count, u8 buffer);
void func_801DF910(u8 from, u8 to, s32 max);
u8 func_801DFA38(u8 slot);
void func_801E0184(u8 member);
void func_801E1690(void);

/* The battle state (pointer 800c3eac), only the field this module uses. */
typedef struct {
    u8 pad0[0x2DB];
    u8 unk2DB;
} BattleState;
extern BattleState *D_800C3EAC;

extern u8 D_800CDD0A[3][2];   /* per member: [0] a stat changed */
extern u8 D_800C48EA;
extern u8 D_800C3E4C;
extern void *D_800D2F5C;        /* glyph sprite table */
extern void *D_800D2C08[1];     /* the results text; the original addresses it as a table */

/* The results archive (directory 0x10 file 2): a count, then its items. */
typedef struct {
    s32 count;
    void *items[4];
} ResultArchive;
extern u8 D_8006DB2C;           /* scenario byte */
extern u8 D_8006E7AB;
extern u16 D_8006ED6E;
void func_80028470(s32 directory, s32 mode);    /* select a disc directory */
s32 func_800288EC(s32 file);                    /* a file's size */
void func_800295D8(s32 file, void *dest, s32 arg2, s32 arg3); /* load a file */
void func_8008AC50(void);
void func_8003342C(void *table);                /* relocate an offset table in place */
void *func_80032E88(void *item, s32 unpack);    /* unpack an archive item */
void func_8002DD20(void *data);
void func_80078310(void *portraits, s32 glyph);
void func_80076EA4(void);
void func_801E2ACC(void);
void func_801E3A18(void);
void func_801E2888(void);
void func_801E42C4(void);
void func_801E403C(void);
void func_801E41B4(void);
void *func_8008ABB8(s32 size, s32 top);        /* heap allocate */
void bzero(void *dest, s32 size);
void func_80039FF8(void);
s32 rand(void);                                 /* libc */
u8 func_801E3610(u8 stat, u8 target, u8 cap, u8 level);
u16 func_801E3700(u16 maxHp, u8 level);
u8 func_801E38CC(u8 maxEp, u8 level);
void func_801E0ACC(u8 member);

/* The skills each member knew before the battle (counter, level bits). */
typedef struct {
    u16 counterSkills;
    u16 levelSkills;
} KnownSkills;
extern KnownSkills D_800C3E0C[3];
extern u8 D_800D2FA0[4];        /* the skill mark icon: width, -, u, v */
u16 func_80089C6C(u16 bits, u8 k);              /* bit k of a skill set */
void *func_80033784(u8 id, s32 k);              /* counter skill names */
void *func_80033908(s32 index);                 /* level skill names */

#endif
