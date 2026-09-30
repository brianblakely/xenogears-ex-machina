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
    u8 pad0[0x8F];
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
    u8 pad[0x1B];
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
    u8 padB6[0x16];
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
} GrowthFile;

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
    u8 pad978[0xE30 - 0x978];
    u8 value_E30;             /* 0xE30 */
    u8 padE31[0x27];
    s32 value_E58;            /* 0xE58 */
    u8 padE5C[8];
    s16 value_E64;            /* 0xE64 */
    s16 value_E66;            /* 0xE66 */
    u8 padE68[0x16C0 - 0xE68];
    CharacterSkills skills[11]; /* 0x16C0 */
} GameData;

/* A battle combatant record (0x170 each from 800ccce8). */
typedef struct {
    u8 pad0[0x4E];
    u16 maxHp;                /* 0x4E */
    u8 pad50[6];
    u8 id;                    /* 0x56 */
    u8 pad57;
    u8 attack;                /* 0x58 */
    u8 pad59[0x37];
    u16 counters[7];          /* 0x90 */
    u8 pad9E[0x170 - 0x9E];
} Combatant;

extern GameData D_8006D634;
extern Combatant D_800CCCE8[];
extern GameData *D_801E44C4;    /* 8006d634 */
extern Combatant *D_801E44C8;   /* 800ccce8 */
extern GrowthFile *D_801E44E8;  /* the growth data file */
extern Combatant *D_801E44EC;   /* the record being processed */
extern u16 D_8006F8EA;          /* option flags */

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

/* The drops rolled for the defeated enemies (800ccce8 + 0x100c). */
typedef struct {
    u8 categories[8];
    u8 ids[8];
} Drops;
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
void *func_8008ABB8(s32 size, s32 top);        /* heap allocate */
void bzero(void *dest, s32 size);
void func_80039FF8(void);
void func_801E0ACC(u8 member);

#endif
