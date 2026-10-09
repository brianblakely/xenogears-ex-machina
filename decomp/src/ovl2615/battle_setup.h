#ifndef OVL2615_BATTLE_SETUP_H
#define OVL2615_BATTLE_SETUP_H

/* The battle setup unit (ovl2615.c): the battle overlay's turn, menu, AI,
 * gauge and UI state as the setup fills them, the formation record, the
 * setup archive and the enemy files' read list. The battle area, the work
 * area and the game data come from battle/area.h, battle/work.h and
 * resident/gamedata.h, the scene data from scene.h. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/area.h"
#include "battle/work.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "ovl2615.h"
#include "scene.h"

/* Callers convert arguments/result differently from the resident definition
 * (a narrow result, u16 coordinates or another parameter count there):
 * random numbers, text rendering and the text palettes. */
s32 func_8001BD40(s32 low, s32 high); /* random number in [low, high] */
void func_80034EAC(void *text, void *image, s32 mode, s32 flags);
void func_80033698(s32 x, s32 y);     /* upload the text palettes */

/* The game data's inGear bytes (+0x22B1) as the setup reads them, one per
 * slot: past the three party entries they are the bytes that follow. */
extern u8 D_8006F8E5[SLOT_COUNT];
/* The work area as the setup declares it: BattleWork, then (past it) the
 * battle overlay's item lists and more, to the party's character ids at
 * 0x603C (D_800D2D24, 0x7F none). The setup addresses the ids as members of
 * the work area, from its symbol or a register holding part of it, which
 * neither the separate symbol nor a cast of BattleWork reproduces, so this
 * view of the whole object shares the work area's assembler name. */
typedef struct {
    BattleWork work;           /* 0x0000 */
    u8 pad5FC8[0x603C - 0x5FC8];
    u8 partyIds[3];            /* 0x603C */
} SetupWork;
extern SetupWork D_800CCCE8_setup __asm__("D_800CCCE8");
extern u8 D_800D3294;     /* demo party: formation flag 0x10 */

extern u8 D_800D3338;
extern u8 D_800C3D44;
extern u8 D_800D2D50;
extern u8 D_800D2FC4;
extern u8 D_800C3D5C;
extern u8 D_800D2D44;
extern u8 D_800C492A;
extern u8 D_800D2DC0;

extern void *D_800C3E5C[10]; /* battle message text images */

void *func_8008AC00(s32 kind);
void *func_8008ABB8(s32 size, s32 flags); /* heap allocation */
extern void *D_800C3E24; /* direction arrow state (0xEC bytes) */

void func_801E4048(void);
void func_801E4160(void);
void func_801E4870(void);
void func_801E4AC0(void);
void func_801E4CD0(void);
void func_801E4E7C(void);
void func_801E5014(void);
void func_801E5384(void);
void func_801E5924(void);
void func_801E5D2C(void);
void func_801E5E78(void);
void func_801E5EE8(void);
void func_801E6290(void);
void func_801E62B8(void);

/* The game data's inventory as the item lists read it. */
#define INVENTORY_SLOTS 150

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

/* Per-slot battle state (0x800D32A1, 8 bytes per slot). */
typedef struct {
    u8 in_gear; /* the slot fights in a gear (battle: D_800D32A0.unk1) */
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

/* Per-enemy AI state (0x800D3400, 0x40 bytes per enemy). */
typedef struct {
    u8 *main;       /* script entry points in the enemy data file: the turn */
    u8 *sub;        /* script (battle 800799c8), one no code runs, the */
    u8 *script;     /* reaction script (80079ab0) and the script run after */
    u8 *reaction;   /* a party turn targeting the enemy (80079c24) */
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

/* The battle formation record (the resident's D_8006F9DC). Its
 * per-combatant bytes are laid out relative to the slot number. */
#define FORMATION_FLAGS D_8006F9DC[1] /* 0x20 alternate module, 0x40/0x80 commands 7/8 */
#define FORMATION_PARTY_GROUP(member) D_8006F9DC[4 + (member)]
#define FORMATION_ENEMY_ID(enemy) D_8006F9DC[8 + (enemy)] /* 0x80: placed alone */
#define FORMATION_ENEMY_FLAGS(enemy) D_8006F9DC[0x10 + (enemy)]
#define FORMATION_ENEMY_GROUP(enemy) D_8006F9DC[0x18 + (enemy)]
#define FORMATION_FLAG6(slot) D_8006F9DC[0x18 + (slot)]
extern u8 D_800C3D48;
extern u8 *D_800C20F0[];        /* command menu layouts */
extern u8 *D_800C2130;          /* command menu sources */
extern u8 *D_800C2134;
extern u8 *D_800C2138;
extern u16 D_800C3234[16];      /* command masks */

u8 func_800841E0(u8 slot);            /* default target */
u8 func_80085310(u8 slot, u8 target); /* facing towards the target */

extern BattleScene *D_800D3364; /* the scene data (the resident's 8005949c) */

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
void func_80078310(void *portraits, s32 glyph);

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

extern u16 D_800C3254[]; /* member panel columns, three per party layout */
s32 func_80076A6C(s32 glyph, GlyphPrim *prims, s16 x, s16 y); /* half-scale glyph */
s32 func_80076A10(s32 glyph, GlyphPrim *prims, s16 x, s16 y); /* glyph; returns its parts */
void func_80076C34(GlyphPrim *prim);                          /* dim a glyph part */

#endif
