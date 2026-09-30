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

extern u8 D_800D2D24[3];  /* party character ids (0x7F none) */
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
void func_8003F8E8(void *dest, s32 size);  /* clear memory */

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
typedef struct {
    u8 pad0[0x2D9];
    u8 last_item; /* 0x2D9 */
} TurnState;

extern TurnState *D_800C3EAC;

/* Combatant record (0x800CCCE8, 0x170 bytes per slot). */
typedef struct {
    u8 pad0[0x34];
    u16 flags;  /* 0x34: 0x200 acts first */
    u8 pad36[0x62 - 0x36];
    u8 stat62;  /* 0x62 */
    u8 stat63;  /* 0x63 */
    u8 pad64[0x15A - 0x64];
    u8 state;   /* 0x15A: 0x80 placed alone */
    u8 pad15B[0x170 - 0x15B];
} CombatantRecord;

extern CombatantRecord D_800CCCE8[SLOT_COUNT];

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
    u32 code[2];
} DR_MODE;

void func_80043C9C(POLY_F4 *poly);                  /* SetPolyF4 */
void func_80043BFC(void *prim, s32 semi);           /* SetSemiTrans */
u16 func_80043A1C(s32 tp, s32 abr, s32 x, s32 y);   /* GetTPage */
void func_800454DC(DR_MODE *mode, s32 dfe, s32 dtd, s32 tpage, RECT *tw); /* SetDrawMode */

/* A party member's status panel (0x1E4 bytes). */
typedef struct {
    u8 state; /* 0 absent, 1 shown, 2 placed alone */
    u8 pad[0x1E3];
} MemberPanel;

/* Battle graphics state (pointer 0x800C3EA4). */
typedef struct {
    u8 pad0[0x63C8];
    POLY_F4 panel[2];       /* 0x63C8: semi-transparent panel backdrops */
    DR_MODE panel_mode[2];  /* 0x63F8 */
    s32 panel_alpha;        /* 0x6410 */
    u8 pad6414;
    u8 panel6415;
    u8 panel6416;
    u8 pad6417[0x853D - 0x6417];
    MemberPanel member_panel[3]; /* 0x853D */
    u8 pad8AE9[0xA244 - 0x853D - 3 * 0x1E4];
    s32 tpage_x;            /* 0xA244 */
    s32 tpage_y;            /* 0xA248 */
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
    u8 pad1[3];
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

void func_8003F99C(void *dest, void *src, s32 size); /* memmove */

#endif
