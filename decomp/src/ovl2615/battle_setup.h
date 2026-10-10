#ifndef OVL2615_BATTLE_SETUP_H
#define OVL2615_BATTLE_SETUP_H

/* The battle setup unit (ovl2615.c): the battle overlay's objects the setup
 * fills come from the shared battle headers (the area and work area, the
 * turn, menu, AI, graphics and UI state, the item lists and the set-up
 * flags); here are the setup's own views where its code reads an object
 * differently (the work area with the party ids past it, the slots' states,
 * the formation groups beside its scene data view), the formation record,
 * the command menu sources and the enemy files' read list. The scene data
 * comes from scene.h. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/actions.h"
#include "battle/area.h"
#include "battle/command.h"
#include "battle/enemy_ai.h"
#include "battle/event_script.h"
#include "battle/graphics.h"
#include "battle/groups.h"
#include "battle/item_command.h"
#include "battle/lists.h"
#include "battle/menu_pages.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "battle/work.h"
#include "resident/cd.h"
#include "resident/formation.h"
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

extern u8 D_800D2D44;

/* The battle's allocators, which it declares with integer results. */
void *func_8008AC00(s32 kind);
void *func_8008ABB8(s32 size, s32 flags); /* heap allocation */

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

/* The game data's inventory as the item lists read it, and the battle's item
 * lists (0x30 entries, battle/actions.h). */
#define INVENTORY_SLOTS 150
#define BATTLE_ITEMS 48

extern u8 D_800D2CAA;

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

/* Enemy data file: u16 script offsets per enemy id (each enemy's four AI
 * script offsets, battle/enemy_ai.h), the name table's offset at +0x30, then
 * 0x170-byte combatant records from +0x32. */
extern u8 *D_800C3DD0;

/* The battle's formation (D_8006F9DC, resident/formation.h), as the setup
 * reads it. FORMATION_FLAG6 indexes the enemy groups by slot (3-10), not by
 * enemy: slots 8-10 read the three bytes after the record. */
#define FORMATION_FLAGS D_8006F9DC.flags
#define FORMATION_PARTY_GROUP(member) D_8006F9DC.partyGroups[member]
#define FORMATION_ENEMY_ID(enemy) D_8006F9DC.enemyIds[enemy]
#define FORMATION_ENEMY_FLAGS(enemy) D_8006F9DC.enemyFlags[enemy]
#define FORMATION_ENEMY_GROUP(enemy) D_8006F9DC.enemyGroups[enemy]
#define FORMATION_FLAG6(slot) D_8006F9DC.enemyGroups[slot]
extern u8 *D_800C20F0[];        /* command menu layouts */
extern u8 *D_800C2130;          /* command menu sources */
extern u8 *D_800C2134;
extern u8 *D_800C2138;

u8 func_80085310(u8 slot, u8 target); /* facing towards the target */

/* The formation data D_800D3364 is the scene data (the resident's 8005949c)
 * to the setup, which reads its positions unsigned (the battle's Formation,
 * battle/formation.h, reads them signed). */
extern BattleScene *D_800D3364;
extern u8 D_800C3E3D[SLOT_COUNT];

u16 func_80089C08(s32 index); /* bit of a group member index */

/* The enemy files' disc read list (0x800D33E8): entries of a file number
 * and a destination, ended by file 0. Its fields are separate variables. */
extern u16 D_800D33E8;   /* entry 0 file */
extern void *D_800D33EC; /* entry 0 destination */
extern u16 D_800D33F0;
extern void *D_800D33F4;
extern u16 D_800D33F8;
extern void *D_800D33FC;

void func_80078310(void *portraits, s32 glyph);

#endif
