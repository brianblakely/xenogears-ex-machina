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

extern SlotInfo D_800C3EB4[SLOT_COUNT];

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

#endif
