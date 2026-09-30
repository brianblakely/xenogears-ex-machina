#ifndef OVL2615_BATTLE_SETUP_H
#define OVL2615_BATTLE_SETUP_H

#include "common.h"

/* Combatant slots: 0-2 party members, 3-10 enemies. */
#define SLOT_COUNT 11
#define NO_COMBATANT 0x7F

/* Per-slot battle placement (0x800C3EB4, 0x1C bytes per slot). */
typedef struct {
    u8 group;       /* formation group */
    u8 index;       /* index within the group */
    u8 id;          /* character/enemy id, 0x7F for none */
    u8 flag3;
    u8 alone;       /* placed alone rather than with the group */
    u8 flag5;
    u8 flag6;
    u8 pad7[3];
    s16 x;
    s16 z;
    u8 padE[0x1C - 0xE];
} SlotInfo;

/* Battle combatant block (0x800C3EB4): slot placements and more. */
typedef struct {
    SlotInfo slot[SLOT_COUNT]; /* 0x000 */
    u8 pad134[0xA34 - 0x134];
    u16 w48E8;                 /* 0xA34 (0x800C48E8) */
} BattleSlots;

extern BattleSlots D_800C3EB4;

extern u8 D_80059468[3];  /* battle party ids published to resident code */
extern u8 D_8006F8E5[SLOT_COUNT];
extern u8 D_800D3294;     /* demo party: formation flag 0x10 */

extern u8 D_800D3338;
extern u8 D_800C3D44;
extern u8 D_800D2D50;
extern u8 D_800D2FC4;
extern u8 D_800C3D5C;
extern u8 D_800D2D44;
extern u8 D_800C492A;
extern u16 D_8005941C;
extern u8 D_8005942C;
extern u8 D_800C48EA;
extern u8 D_800D2DC0;

extern void *D_800C3E5C[10]; /* battle message text images */

void *func_8008AC00(s32 kind);
void *func_800338D8(s32 message);
void func_80034EAC(void *text, void *image, s32 mode, s32 flags);

void func_801E5924(void);
void func_801E5D2C(void);
void func_801E5E78(void);
void func_801E5EE8(void);

/* Stage light entry (0x0E bytes). */
typedef struct {
    u8 pad0[0xD];
    u8 active;
} StageLight;

extern void *D_800D3344;        /* stage actors */
extern StageLight *D_800D39CC;  /* stage light entries */
extern s32 D_800D3348;          /* stage light count */
extern u8 D_800D2F64;

extern void *D_800C3E24; /* direction arrow state (0xEC bytes) */

void *func_8008ABB8(s32 size, s32 flags); /* heap allocation */
void bzero(void *dest, s32 size);  /* clear memory */

void func_801E4048(void);
void func_801E4160(void);
void func_801E4870(void);
void func_801E4AC0(void);
void func_801E4CD0(void);
void func_801E4E7C(void);
void func_801E5014(void);
void func_801E5384(void);
void func_801E6290(void);
void func_801E62B8(void);

/* Game inventory (resident game data). */
#define INVENTORY_SLOTS 150
extern u8 D_8006F5C4[INVENTORY_SLOTS]; /* item counts */
extern u8 D_8006F65A[INVENTORY_SLOTS]; /* item ids */
extern u8 D_8006F3D0[100];             /* key item ids */
extern u8 D_8006F36C[100];

/* Battle item lists. */
#define BATTLE_ITEMS 48
extern u8 D_800D2CE0[BATTLE_ITEMS]; /* ids */
extern u8 D_800D2CB0[BATTLE_ITEMS]; /* counts */
extern u8 D_800D2FE4[BATTLE_ITEMS];
extern u8 D_800C3D70[BATTLE_ITEMS]; /* special item ids 50..72 */
extern u8 D_800D3688[BATTLE_ITEMS];

/* Battle turn and menu state (pointer 0x800C3EAC). */
/* A slot's command menu state (0x40 bytes). */
typedef struct {
    u8 layout[8];      /* 0x00: menu layout */
    u8 menu8[4];       /* 0x08 */
    u8 menuC[4];       /* 0x0C */
    u8 pad10[0xC];
    u16 commands[16];  /* 0x1C: per-command masks */
    u8 target;         /* 0x3C: default target slot */
    u8 pad3D[3];
} TurnSlot;

typedef struct {
    TurnSlot slot[SLOT_COUNT]; /* 0x000 */
    u8 pad2C0[0x2D9 - 0x2C0];
    u8 last_item;              /* 0x2D9 */
} TurnState;

extern TurnState *D_800C3EAC;

/* Combatant record (0x800CCCE8, 0x170 bytes per slot). */
typedef struct {
    u8 pad0[0x34];
    u16 flags;  /* 0x34: 0x200 acts first */
    u8 pad36[0x4C - 0x36];
    s16 pos4C;  /* 0x4C */
    s16 pos4E;  /* 0x4E */
    u8 pad50[0x56 - 0x50];
    u8 menu_layout; /* 0x56 */
    u8 pad57[0x62 - 0x57];
    u8 stat62;  /* 0x62 */
    u8 stat63;  /* 0x63 */
    u8 pad64[0x7A - 0x64];
    u16 commands; /* 0x7A: available command mask */
    u8 pad7C[0xA0 - 0x7C];
    u8 bA0;     /* 0xA0: gear record index; 0xFF none, forces command 7 */
    u8 padA1[3];
    u8 gear[0xA4]; /* 0xA4: the gear record */
    u8 pad148[0x15A - 0x148];
    u8 state;   /* 0x15A: 0x80 placed alone */
    u8 pad15B[0x170 - 0x15B];
} CombatantRecord;

/* Battle data (0x800CCCE8): combatant records, the members' battle data
 * blocks from the setup archive and the party. */
typedef struct {
    CombatantRecord record[SLOT_COUNT]; /* 0x0000 */
    u8 padFD0[0x1058 - 0xFD0];
    u8 member_data[3][0x5F0];           /* 0x1058 */
    u8 member_gear[3][0x690];           /* 0x2228 */
    u8 data35D8[0x1F40];                /* 0x35D8 */
    u8 data5518[0x300];                 /* 0x5518 */
    u8 data5818[0x300];                 /* 0x5818 */
    u8 pad5B18[0x603C - 0x5B18];
    u8 party_ids[3];                    /* 0x603C: party character ids (0x7F none) */
} BattleData;

extern BattleData D_800CCCE8;

/* Slot presence and the turn order and timers (0x800D2DCC). */
typedef struct {
    u8 present[SLOT_COUNT];     /* 0x00 */
    u8 order_pos;               /* 0x0B */
    u8 order[SLOT_COUNT];       /* 0x0C: slots in drawn order */
    u8 pad17;
    u8 ready[SLOT_COUNT];       /* 0x18 */
    u8 pad23;
    s16 timer_reset[SLOT_COUNT]; /* 0x24 */
    s16 timer[SLOT_COUNT];      /* 0x3A */
    s16 alternate[SLOT_COUNT];  /* 0x50 */
} TurnOrder;

extern TurnOrder D_800D2DCC;
extern u8 D_800D2CAA;

void func_80078508(u8 *drawn); /* initial turn timers; clears the drawn flags */
s32 func_8001BD40(s32 low, s32 high); /* random number in [low, high] */

/* libgpu primitives. */
typedef struct {
    s16 x, y, w, h;
} RECT;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
    s16 x2, y2;
    s16 x3, y3;
} POLY_F4;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LINE_F2;

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

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, p1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, p2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad2;
    u8 r3, g3, b3, p3;
    s16 x3, y3;
    u8 u3, v3;
    u16 pad3;
} POLY_GT4;

typedef struct {
    u32 tag;
    u32 code[2];
} DR_MODE;

void SetPolyGT4(POLY_GT4 *poly);                 /* SetPolyGT4 */
void SetPolyG4(POLY_G4 *poly);                  /* SetPolyG4 */
void SetLineF2(LINE_F2 *line);                  /* SetLineF2 */
void SetShadeTex(void *prim, s32 tge);            /* SetShadeTex */
u16 GetClut(s32 x, s32 y);                    /* GetClut */

void SetPolyF4(POLY_F4 *poly);                  /* SetPolyF4 */
void SetSemiTrans(void *prim, s32 semi);           /* SetSemiTrans */
u16 GetTPage(s32 tp, s32 abr, s32 x, s32 y);   /* GetTPage */
void SetDrawMode(DR_MODE *mode, s32 dfe, s32 dtd, s32 tpage, RECT *tw); /* SetDrawMode */

/* A glyph's primitives, double-buffered (0x28 bytes per buffer). */
typedef struct {
    u8 data[0x28];
} GlyphPrim;

/* A party member's gauge glyph primitives (0x1E0 bytes). */
typedef struct {
    GlyphPrim prim[12];
} MemberGauge;

/* A party member's status panel (0x1E4 bytes). */
typedef struct {
    GlyphPrim digit[3][4];    /* digit glyphs: two parts, double-buffered */
    u8 buffer;                /* 0x1E0 */
    u8 state;                 /* 0x1E1: 0 absent, 1 shown, 2 placed alone */
    u8 digit_parts[2];        /* 0x1E2 */
} MemberPanel;

/* Battle graphics state (pointer 0x800C3EA4). */
typedef struct {
    MemberGauge member_gauge[3]; /* 0x000 */
    POLY_GT4 gauge[8];      /* 0x5A0: gauge bars */
    POLY_G4 gauge_shade[6]; /* 0x740; the gauge setup initialises eight,
                             * the last two overlapping the portraits */
    GlyphPrim portrait[3][2]; /* 0x818: member portraits */

    LINE_F2 gauge_line[12]; /* 0x908 */
    u8 pad9C8[0x63C8 - 0x9C8];
    POLY_F4 panel[2];       /* 0x63C8: semi-transparent panel backdrops */
    DR_MODE panel_mode[2];  /* 0x63F8 */
    s32 panel_alpha;        /* 0x6410 */
    u8 pad6414;
    u8 panel6415;
    u8 panel6416;
    u8 pad6417[0x835C - 0x6417];
    MemberPanel member_panel[3]; /* 0x835C */
    u8 pad8AE8[0xA234 - 0x835C - 3 * 0x1E4];
    s32 glyph_a234;         /* 0xA234: the gauge glyph's fields */
    s32 tpage_tp;           /* 0xA238 */
    s32 clut_x;             /* 0xA23C */
    s32 clut_y;             /* 0xA240 */
    s32 tpage_x;            /* 0xA244 */
    s32 tpage_y;            /* 0xA248 */
    u8 padA24C[0xA2AC - 0xA24C];
    u16 gauge_clut[4];      /* 0xA2AC */
} GraphicsState;

extern GraphicsState *D_800C3EA4;

extern s16 D_800D2D30; /* stage texture bounds: left */
extern s16 D_800D2D34; /* top */
extern s16 D_800D2D2C; /* width */
extern s16 D_800C3EA8; /* height */

void func_8003342C(s32 *table); /* relocate an offset table in place */

/* Per-slot battle state (0x800D32A1, 8 bytes per slot). */
typedef struct {
    u8 alone;   /* placed alone */
    u8 pad1[2];
    u8 character_b; /* from the character table */
    u8 stat62;  /* copied from the record */
    u8 stat63;
    u8 pad6[2];
} SlotState;

extern SlotState D_800D32A1[SLOT_COUNT];

void func_80097D5C(void); /* derive the party's battle stats */
void func_8009B098(void); /* demo battle members */

/* Per-slot AI flags (0x800C3D0C, 4 bytes per slot; used by enemies). */
typedef struct {
    u8 script_armed;
    u8 reaction_armed;
    u8 pad2;
    u8 b3;
} EnemyAiFlags;

extern EnemyAiFlags D_800C3D0C[SLOT_COUNT];

/* Per-enemy AI state (0x800D3400, 0x40 bytes per enemy). */
typedef struct {
    u8 *main;       /* script entry points in the enemy data file */
    u8 *sub;
    u8 *script;
    u8 *reaction;
    s32 vars[4];    /* 0x10 */
    s16 hvars[8];   /* 0x20 */
    u8 bvars[16];   /* 0x30 */
} EnemyAi;

extern EnemyAi D_800D3400[8];

/* Enemy data file: u16 script offsets per enemy id, the offset at +0x30,
 * then 0x170-byte combatant records from +0x32. */
extern u8 *D_800C3DD0;
extern u8 *D_800C3DDC;
extern s16 D_800D39E0;

void memmove(void *dest, void *src, s32 size); /* memmove */

/* The battle formation record (resident game data at 0x8006F9DC). Its
 * per-combatant bytes are laid out relative to the slot number. */
extern u8 D_8006F9DC[];

#define FORMATION_FLAGS D_8006F9DC[1] /* 0x20 alternate module, 0x40/0x80 commands 7/8 */
#define FORMATION_PARTY_GROUP(member) D_8006F9DC[4 + (member)]
#define FORMATION_ID(slot) D_8006F9DC[5 + (slot)]      /* 0x80: placed alone */
#define FORMATION_FLAGS3(slot) D_8006F9DC[0xD + (slot)]
#define FORMATION_GROUP(slot) D_8006F9DC[0x15 + (slot)]
#define FORMATION_FLAG6(slot) D_8006F9DC[0x18 + (slot)]
extern u8 D_800C3D48;
extern u8 D_8006ED0B[][0x20];   /* character table (+0xB) */
extern u8 *D_800C20F0[];        /* command menu layouts */
extern u8 *D_800C2130;          /* command menu sources */
extern u8 *D_800C2134;
extern u8 *D_800C2138;
extern u16 D_800C3234[16];      /* command masks */

u8 func_800841E0(u8 slot);            /* default target */
u8 func_80085310(u8 slot, u8 target); /* facing towards the target */

/* Battle scene data (resident pointer 0x8005949C): standing positions of
 * each formation group, then the positions of the members placed alone. */
typedef struct {
    u16 x, z;
} ScenePos;

typedef struct {
    u8 pad0[4];
    ScenePos party[3];  /* 0x04 */
    ScenePos enemy[4];  /* 0x10 */
} SceneGroup;

typedef struct {
    ScenePos party;
    ScenePos enemy;
} SceneAlone;

typedef struct {
    SceneGroup group[8];  /* 0x000 */
    SceneAlone alone[8];  /* 0x100 */
} BattleScene;

extern BattleScene *D_8005949C;
extern BattleScene *D_800D3364;
extern BattleScene *D_800C3EB0;

/* Formation groups: member count and member bits (party 0-7, enemies 8-15,
 * members placed alone 16-23 and 24-31). */
typedef struct {
    u8 count;
    u8 mask;
    u8 pad2[2];
} FormationGroup;

extern FormationGroup D_800D301C[32];
extern u8 D_800D3280;        /* present party members - 1 */
extern u8 D_800C3E3D[SLOT_COUNT];

u16 func_80089C08(s32 index); /* bit of a group member index */

/* Game data party (0x8006F364): availability masks and the party order. */
typedef struct {
    u16 available;
    u16 available2;
    u8 party[3];
} GameParty;

extern GameParty D_8006F364;
extern u8 D_8006D8A0[][0xA4]; /* game data character records */
extern u8 D_8006DFAC[][0xA4]; /* game data gear records */

extern u32 *D_800595A8;   /* the setup archive (file 3) */
extern void *D_800D2F5C;  /* glyph sprite table */
extern void *D_800D329C;
extern void *D_800D39F0;
extern void *D_800C3DEC;  /* enemy model file */

/* The enemy files' disc read list (0x800D33E8): entries of a file number
 * and a destination, ended by file 0. Its fields are separate variables. */
extern u16 D_800D33E8;   /* entry 0 file */
extern void *D_800D33EC; /* entry 0 destination */
extern u16 D_800D33F0;
extern void *D_800D33F4;
extern u16 D_800D33F8;
extern void *D_800D33FC;

u16 func_80089C9C(u16 mask, u8 id);             /* the character's bit within a mask */
void *func_80032E88(u32 item, s32 unpack);      /* unpack an archive item */
void func_800320E8(void *block);                /* heap release */
void func_8002DDE4(void *images, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5, s32 a6);
void func_80033698(s32 x, s32 y);               /* upload the text palettes */
void func_80078310(void *portraits, s32 glyph);
void func_80028470(s32 directory, s32 mode);    /* select a disc directory */
s32 func_800288EC(s32 file);                    /* a file's size */
void func_80029AFC(u16 *list, s32 a1, s32 a2); /* read a file list */

/* Battle UI state (pointer 0x800D2D28). */
typedef struct {
    u8 pad0[0x78];
    u8 gauge_parts[3]; /* 0x78: gauge glyph parts per member */
    u8 pad7B;
    u8 gauge_shown[3]; /* 0x7C */
    u8 pad7F[0x83 - 0x7F];
    u8 b83;            /* 0x83: draw buffer */
    u8 pad84[0xA2 - 0x84];
    u8 bA2;            /* 0xA2: draw buffer */
} UiState;

extern UiState *D_800D2D28;

void func_80026338(void *glyphs, s32 glyph, s32 *a, s32 *tp, s32 *clut_x, s32 *clut_y,
                   s32 *tpage_x, s32 *tpage_y); /* a glyph's texture fields */

extern u16 D_800C3254[]; /* member panel columns, three per party layout */
extern s32 D_800CCB34;      /* the draw buffer index */

s32 func_80076A6C(s32 glyph, GlyphPrim *prims, s16 x, s16 y); /* half-scale glyph */
s32 func_80076A10(s32 glyph, GlyphPrim *prims, s16 x, s16 y); /* glyph; returns its parts */
void func_80076C34(GlyphPrim *prim);                          /* dim a glyph part */

/* Stage setup (stage.c). */
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
    s16 vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    u8 r, g, b, cd;
} CVECTOR;

typedef struct {
    s32 vx, vy, vz, pad;
} VECTOR;

typedef struct {
    u32 tag;
    u32 code[15];
} DR_ENV;

typedef struct {
    RECT clip;
    s16 ofs[2];
    RECT tw;
    u16 tpage;
    u8 dtd;
    u8 dfe;
    u8 isbg;
    u8 r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;

void SetPolyFT4(POLY_FT4 *poly);                /* SetPolyFT4 */
DRAWENV *GetDrawEnv(DRAWENV *env);              /* GetDrawEnv */
void func_80032498(s32 tag, s32 arg1);          /* select the heap tag */
void *func_80031BDC(s32 size, s32 top);         /* heap allocation */

/* The stage backdrop (func_801E7914, 0x17cc bytes): a floor grid of 9 x 9
 * vertices and 128 tiles, and the fills and fades around it. */
typedef struct {
    s16 x;                    /* 0x00 */
    s16 y;                    /* 0x02 */
    s16 width;                /* 0x04 */
    s16 height;               /* 0x06 */
    s16 v08;                  /* 0x08 */
    s16 v0A;                  /* 0x0A */
    s16 v0C;                  /* 0x0C */
    s16 v0E;                  /* 0x0E */
    s16 v10;                  /* 0x10 */
    s16 v12;                  /* 0x12 */
    s16 position[3];          /* 0x14: the object's position */
    s16 pad1A;
    CVECTOR colours[2];       /* 0x1C */
    DR_MODE modes[4];         /* 0x24 */
    SVECTOR grid[81];         /* 0x54 */
    POLY_FT4 tiles[128];      /* 0x2DC */
    POLY_F4 fills[4];         /* 0x16DC */
    POLY_G4 fades[4];         /* 0x173C */
} StageBackdrop;

#endif
