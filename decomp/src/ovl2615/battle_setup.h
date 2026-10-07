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

typedef struct {
    SlotState party[3];
    SlotState enemy[8];
} BattleSlotStates;

extern BattleSlotStates D_800D32A1;

void func_80097D5C(void); /* derive the party's battle stats */
void func_8009B098(void); /* demo battle members */

/* Per-slot AI flags (0x800C3D0C, 4 bytes per slot; used by enemies). */
typedef struct {
    u8 script_armed;
    u8 reaction_armed;
    u8 pad2;
    u8 b3;
} EnemyAiFlags;

typedef struct {
    EnemyAiFlags party[3];
    EnemyAiFlags enemy[8];
} BattleAiFlags;

extern BattleAiFlags D_800C3D0C;

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

/* A stage object (0x28 bytes): lights (types 1, 2), the backdrop (3), fog
 * (5) and animated stage parts (7). */
typedef struct {
    s32 position[4];      /* 0x00: a VECTOR */
    s16 v10;              /* 0x10 */
    s16 v12;              /* 0x12 */
    s16 v14;              /* 0x14 */
    s16 v16;              /* 0x16 */
    u16 type;             /* 0x18 */
    s16 v1A;              /* 0x1A */
    s16 v1C;              /* 0x1C */
    s16 v1E;              /* 0x1E */
    s16 v20;              /* 0x20 */
    s16 v22;              /* 0x22 */
    s16 v24;              /* 0x24 */
    s16 v26;              /* 0x26 */
} StageObject;

/* The scene's stage description (scene data + 0x340). */
typedef struct {
    u8 flags[4];          /* 0x000: published to 800d2d10 */
    u8 pad4[0x1A];
    u8 fog;               /* 0x01E: set by a fog object */
    u8 pad1F;
    StageObject objects[6]; /* 0x020 */
    s16 backdrop[4];      /* 0x110: backdrop tiling and texture position */
    u8 fogColour[4];      /* 0x118: a CVECTOR */
} StageInfo;

typedef struct {
    SceneGroup group[8];  /* 0x000 */
    SceneAlone alone[8];  /* 0x100 */
    u8 pad140[0x340 - 0x140];
    StageInfo info;       /* 0x340 */
    u8 pad45C[0x464 - 0x45C];
    s16 origin[3];        /* 0x464 */
    u8 pad46A[2];
    s16 colours[3];       /* 0x46C: the colour matrix's first column */
    u8 back[3];           /* 0x474: the GTE back colour */
    u8 pad477[0x50C - 0x477];
    s32 actors;           /* 0x50C: offsets in the scene data (0: none) */
    s32 lights;           /* 0x510 */
    s32 motion;           /* 0x514 */
} BattleScene;

extern BattleScene *D_8005949C;
extern BattleScene *D_800D3364;

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

/* PsyQ libgte. */
#define copyVector(v0, v1) (v0)->vx = (v1)->vx, (v0)->vy = (v1)->vy, (v0)->vz = (v1)->vz

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
void func_800320B8(void *block);                /* drop a block's keep flag */
void *memcpy(void *dest, const void *src, u32 size);
void DrawSync(s32 mode);
void SetColorMatrix(s16 *m);
void SetBackColor(s32 r, s32 g, s32 b);

/* A model part (0x7c bytes); the first part heads the model and holds the
 * part count. */
typedef struct {
    u8 pad0[0xA];
    u16 count;            /* 0x0A */
    u8 padC[0x52 - 0xC];
    u16 rotation;         /* 0x52 */
    u8 pad54[0x5C - 0x54];
    s32 x;                /* 0x5C */
    s32 y;                /* 0x60 */
    s32 z;                /* 0x64 */
    u8 pad68[0x7C - 0x68];
} ModelPart;

/* The stage model record (pointer 800d33e4). */
typedef struct {
    void *model;          /* 0x00 */
    ModelPart *parts;     /* 0x04 */
    u8 pad8[0x1C - 8];
    s16 pose;             /* 0x1C */
} StageModel;
extern StageModel *D_800D33E4;

/* The stage file: its texture image list and part positions. */
typedef struct {
    s16 x, y, z;
    u16 rotation;
} PartPosition;

typedef struct {
    u8 pad0[4];
    s32 *images;          /* 0x04 */
    u8 pad8[0x14 - 8];
    PartPosition *positions; /* 0x14 */
} StageFile;

/* Stage part animations (0x18 bytes). */
typedef struct {
    u8 pad0[0x14];
    void *motion;         /* 0x14: nonzero when in use */
} StageAnimation;

extern ModelPart *D_800C3E38;   /* the stage model's parts */
extern void *D_800C3E48;
extern void *D_800C3EA0;        /* the stage backdrop */
extern void *D_800C3D50[2];     /* stage lights */
extern StageAnimation D_800C3DA0[2];
extern s16 D_800D361A;
extern u8 *D_800D2FD0;
extern u16 D_800D2FC8;
extern s16 *D_800D2FC0;         /* the stage colour matrix */
extern u8 D_800D2D10[4];
extern BattleScene *D_800658C8; /* the scene data */
void func_800A8BF0(s32 a0, s32 a1, void *a2, void *a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8);
void func_800AA898(StageModel *model, void *state, void *motion, s32 a3);
void func_800AA934(StageModel *model, StageModel *model2, void *state, s32 a3);
void func_8009EF3C(ModelPart *parts, s32 pose);
void *func_8002709C(u16 a0, u16 a1, u16 a2, u16 a3, u16 a4, u16 a5, u16 a6, u16 a7,
                    StageObject *object, void *colour, s32 a10, s32 a11, s32 a12);
void func_80027D64(StageAnimation *animation, s32 a1, s32 a2, s32 a3, s32 a4, s32 a5,
                   s32 a6, s32 a7, void *motion);

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

StageBackdrop *func_801E7914(s16 texX, s16 texY, s16 width, s16 height, s16 size, s16 step,
                             s16 v0A, s16 clutX, s16 clutY, s16 v10, s16 v12, VECTOR *position,
                             CVECTOR *colour, s16 v0C, s16 v0E);
void func_801E7EC4(void *actors, StageLight *lights, s32 count);

/* The later-compiler units (battle_loader.c, load_modes.c, burst_modes.c). */
typedef struct {
    s16 m[3][3];
    s32 t[3];
} MATRIX;

typedef struct {
    RECT disp;
    RECT screen;
    u8 isinter, isrgb24, pad0, pad1;
} DISPENV;

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
} POLY_FT3;

typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    u8 u0, v0;
    u16 clut;
    u8 r1, g1, b1, pad1;
    s16 x1, y1;
    u8 u1, v1;
    u16 tpage;
    u8 r2, g2, b2, pad2;
    s16 x2, y2;
    u8 u2, v2;
    u16 pad3;
} POLY_GT3;

/* Resident task system: a task node (update) followed by its drawing node;
 * both callbacks receive their node, whose +4 names the task's object. */
typedef struct TaskNode {
    u32 unk0;
    void *object;
    void (*update)(struct TaskNode *node);
    void (*destroy)(struct TaskNode *node);
    u32 unk10;
    u32 unk14;
    struct TaskNode *next;
} TaskNode;

/* One battle display buffer (0x4070 bytes). */
typedef struct {
    DRAWENV draw;      /* +0000 */
    DISPENV disp;      /* +005c */
    u32 ot[0x1000];    /* +0070: reverse ordering table */
} DrawBuffer;

/* A slot's sprite source: its data, image position and variant. */
typedef struct {
    void *data;        /* +0 */
    s16 x;             /* +4: image column */
    s16 y;             /* +6: image row */
    s32 variant;       /* +8 */
} SpriteRow;

/* A sprite's renderer; only its part block. */
typedef struct {
    u8 pad0[0x2C];
    void *parts;       /* +2c */
} SpriteRenderer;

/* A battle sprite; only the members the loader uses. */
typedef struct {
    s16 pad0;
    s16 x;             /* +02: whole parts of 16.16 coordinates */
    s16 pad4;
    s16 y;             /* +06 */
    s16 pad8;
    s16 z;             /* +0a */
    u8 padC[0x14];
    SpriteRenderer *renderer; /* +20 */
    u16 *binding;      /* +24: +4 image x, +6 image y */
    u8 pad28[0x54];
    u32 *sequence;     /* +7c: +0 sequencer word, +e image position */
    u8 pad80[4];
    s16 ground;        /* +84: resting height */
    u8 pad86[0x18];
    s16 v9E;           /* +9e */
    s16 home[3];       /* +a0 */
    u8 padA6[2];
    u32 flagsA8 : 30;  /* +a8 */
    u32 slotLow : 2;   /* +a8 bits 30-31: the slot's low bits */
    u32 slotHigh : 2;  /* +ac bits 0-1: the slot's high bits */
    u32 flagsAC : 30;
} BattleSprite;

/* Battle overlay work area (800c3eb0) at battle setup. */
typedef struct {
    BattleScene *scene;          /* +0000 */
    SlotInfo slots[SLOT_COUNT];  /* +0004 (also D_800C3EB4) */
    u8 pad138[0xB70 - 0x138];
    DrawBuffer buffers[2];       /* +0b70 */
    DrawBuffer *current;         /* +8c50 */
    u32 *ot;                     /* +8c54 */
    u8 pad8C58[0x2C];
    s32 buffer;                  /* +8c84: double-buffer index being drawn */
    u8 pad8C88[4];
    BattleSprite *sprites[SLOT_COUNT]; /* +8c8c */
    TaskNode *tasks[SLOT_COUNT]; /* +8cb8 */
    u8 pad8CE4[0x40];
    SpriteRow rows[SLOT_COUNT];  /* +8d24 */
    u8 loaded;                   /* +8da8: set when loading ends */
} BattleWork;
extern BattleWork D_800C3EB0;

/* A file list entry for the disc reader (ended by file 0). */
typedef struct {
    u16 file;
    void *dest;
} FileEntry;

/* The loading task (801e7098), run once per frame until the sprites, the
 * battle images and the effect bank are loaded. */
typedef struct {
    TaskNode task;       /* +00 */
    u32 unk1C;
    u8 *data;            /* +20: the enemy set file */
    void *shared;        /* +24: battle file 2 */
    void *images;        /* +28: battle file 1 */
    void *effects;       /* +2c: battle file 3 */
    FileEntry members[4]; /* +30: the members' sprite files */
    FileEntry files[4];  /* +50: battle files 1-3 */
    u8 pad70[0x20];
    s32 timer;           /* +90: frames before the settle check */
} LoaderTask;

/* An enemy set file entry (12 bytes). */
typedef struct {
    s32 offset;          /* +0: sprite data offset */
    u32 images;          /* +4: image list offset, or an earlier list's index */
    u8 model;            /* +8: nonzero for a 3D model */
    u8 pad9[2];
    u8 variant;          /* +b */
} EnemyEntry;

/* Member sprite files by type (directory 2c). */
typedef struct {
    s32 file;
    u32 sequence;        /* the sprite's sequencer word */
} MemberFile;

extern u8 D_800591AF;           /* battle setup running */
extern void *D_8005919C;        /* the effect bank */
extern u8 D_8006BE10[];         /* resident binding of battle file 2 */
extern s32 D_800C35D8;          /* members still moving */
extern void *D_800D2D54;        /* battle file 2 */
extern u8 D_800D36B8;
extern void *D_800D39C8;        /* the enemy set data copy */
extern MemberFile D_801E95BC[]; /* 8 per type */
extern u8 D_801E962C[];         /* image columns per type */
extern u16 D_801E9638;          /* the next member image column */
extern void *D_801E96B4;        /* the images 801e6dc8 uploads */

typedef struct {
    s16 x, y;
} Point;

void *func_8001CD08(s32 owner, s32 size);                  /* create a task */
void func_8001CD6C(TaskNode *node, void (*update)(TaskNode *)); /* next task state */
void func_8001CE44(TaskNode *node);                        /* end a task */
void func_8001CB48(TaskNode *node);                        /* unlink a drawing node */
void func_8001CD94(TaskNode *node);                        /* unlink a task */
void func_80025180(void *block);                           /* release after the frame */
s32 func_800286CC(void);                                   /* disc busy */
s32 func_80022A00(void *images);                           /* image count */
void func_80022A70(void *images, s32 x, s32 y);            /* upload an image list */
BattleSprite **func_800BA984(void *data, s32 a1, s32 palette, s16 x, s16 y, s32 a5, s32 a6,
                             s32 a7, s32 a8, s32 animation, s32 a10, s32 a11, s32 a12,
                             s32 variant); /* a sprite task (its +4 the sprite) */
void func_80021D3C(BattleSprite *sprite, s16 x, s16 z);   /* place a sprite */
void func_800223B0(BattleSprite *sprite, s32 angle);       /* sprite orientation */
void func_80021FE0(BattleSprite *sprite, s32 angle);       /* sprite heading */
void func_800BB350(s32 slot);
void func_800BB760(s32 slot);
void func_800BA8F4(BattleSprite *sprite);                  /* place on the stage floor */
void func_800245D8(BattleSprite *sprite, s32 animation);
s16 func_8003BDFC(s32 arg);                                /* sound transfer busy */
void func_80022224(void *binding, void *data, Point image, Point clut, s32 a4);
void func_80038428(void *bank);                            /* link an effect bank */
void func_800B14B8(void);

/* Screen transitions (load_modes.c, burst_modes.c): the screen split into
 * cells that fly apart (shatter, 801e8588) or ripple (burst, 801e91e8). */
typedef struct {
    SVECTOR rot;         /* +00 */
    u8 pad8[8];
    VECTOR trans;        /* +10 */
    u8 pad20[4];
    POLY_FT3 prim[2];    /* +24: per display buffer */
    u8 pad64[0x18];
} ShatterCell;           /* 0x7c */

typedef struct {
    TaskNode task;       /* +00 */
    TaskNode draw;       /* +1c */
    s32 frame;           /* +38 */
    ShatterCell cells[2][7][10]; /* +3c: two triangles per 32x32 cell */
} ShatterTask;           /* 0x440c */

typedef struct {
    u32 pad0;
    POLY_GT3 prim[2];    /* +04 */
    SVECTOR corner[3];   /* +54 */
    s32 distance[3];     /* +6c */
    u32 pad78;
} BurstCell;             /* 0x7c */

typedef struct {
    TaskNode task;       /* +00 */
    TaskNode draw;       /* +1c */
    s32 brightness;      /* +38 */
    s32 angle;           /* +3c */
    s32 twist;           /* +40 */
    s32 frame;           /* +44 */
    s32 speed;           /* +48 */
    VECTOR trans;        /* +4c */
    SVECTOR rot;         /* +5c */
    BurstCell cells[2][14][20]; /* +64: two triangles per 16x16 cell */
} BurstTask;             /* 0x10fa4 */

extern u8 D_801E963C;           /* shatter variant */
extern SVECTOR D_801E9640[3];   /* shatter triangles */
extern SVECTOR D_801E9658[3];
extern u8 D_801E9680;           /* burst variant */
extern SVECTOR D_801E9684[3];   /* burst triangles */
extern SVECTOR D_801E969C[3];
extern u32 *D_801E96B8;         /* shatter ordering table */
extern u32 *D_801E96BC;         /* burst ordering table */
extern u32 *D_8005956C;         /* current ordering table */

void func_8001C944(void);
void func_8001BB0C(void);
void func_80019CA0(void);                                  /* soft reset check */
s32 func_80028A60(s32 mode);                               /* wait for the disc */
void SetDispMask(s32 mask);
u8 func_80021AD8(u8 value, s32 delta);                     /* add, clamped to 0..255 */
s32 VSync(s32 mode);
s32 ClearOTagR(u32 *ot, s32 n);
void DrawOTag(u32 *ot);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
s32 StoreImage(RECT *rect, void *p);
s32 LoadImage(RECT *rect, void *p);
void SetPolyFT3(POLY_FT3 *p);
void SetPolyGT3(POLY_GT3 *p);
void AddPrim(void *ot, void *p);
void ReadGeomOffset(s32 *ofx, s32 *ofy);
s32 ReadGeomScreen(void);
void SetGeomOffset(s32 ofx, s32 ofy);
void SetGeomScreen(s32 h);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
s32 RotTransPers3(SVECTOR *v0, SVECTOR *v1, SVECTOR *v2, s32 *sxy0, s32 *sxy1, s32 *sxy2,
                  s32 *p, s32 *flag);
s32 SquareRoot0(s32 a);
MATRIX *func_8003F738(SVECTOR *rotation, MATRIX *m); /* RotMatrix */
void func_8004A414(VECTOR *v0, VECTOR *v1);         /* Square0 */
s32 func_8003F8B0(s32 angle);                       /* sine (4096 = 1.0) */
s32 func_8003F8CC(s32 angle);                       /* cosine (4096 = 1.0) */
void func_801E5840(u8 phase);

void func_801E6314(u8 *data);
void func_801E6710(u8 *data);
void func_801E67A4(s32 slot, s32 row, s32 animation);
void func_801E693C(FileEntry *list);
void func_801E6A4C(void);
void func_801E6AC4(void);
void func_801E6C80(TaskNode *node);
void func_801E6D34(TaskNode *node);
void func_801E6D6C(TaskNode *node);
void func_801E6DC8(void);
void func_801E6E48(TaskNode *node);
void func_801E6F00(TaskNode *node);
void func_801E6FEC(TaskNode *node);
void func_801E7098(u8 *data);
void func_801E7F4C(TaskNode *node);
void func_801E80B4(TaskNode *node);
void func_801E827C(void *block);
ShatterTask *func_801E82EC(void);
ShatterTask *func_801E8320(ShatterTask *task);
void func_801E8588(void);
void func_801E8964(TaskNode *node);
void func_801E8A64(TaskNode *node);
void func_801E8D48(void *block);
BurstTask *func_801E8DB8(void);
BurstTask *func_801E8DF0(BurstTask *task);
void func_801E91E8(void);

/* Load modes (load_modes.c, burst_modes.c): flip to the other display
 * buffer and clear its ordering table. */
static inline void swap_buffers(void) {
    BattleWork *work = &D_800C3EB0;
    DrawBuffer *next = &work->buffers[0];

    if (work->current == next) {
        next = &work->buffers[1];
    }
    work->current = next;
    work->ot = next->ot;
    ClearOTagR(next->ot, 0x1000);
}

#endif
