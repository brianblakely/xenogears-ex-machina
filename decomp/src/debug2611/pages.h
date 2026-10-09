#ifndef DEBUG2611_PAGES_H
#define DEBUG2611_PAGES_H

/* The battle state pages (pages.c): the battle overlay objects they print
 * beyond the shared battle area and work area, by their own views. */

#include "common.h"
#include "battle/area.h"
#include "battle/work.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/mode.h"

/* The battle's action list (8 bytes, 32 from 800d2e5c; the battle overlay's
 * BattleAction); the state page names its fields. */
typedef struct {
    u8 code;         /* Cd */
    u8 cls;          /* Cl */
    u8 anim;         /* An */
    u8 param[3];     /* P1..P3 */
    u16 target;      /* Tg */
} Effect;
extern Effect D_800D2E5C[23];

/* Enemy AI flags (0x40 per enemy slot 3..10; the battle overlay's EnemyAi). */
typedef struct {
    u8 unk0[0x10];
    s32 lflag[4];    /* +10 */
    u16 hflag[8];    /* +20 */
    u8 bflag[16];    /* +30 */
} EnemyFlags;
extern EnemyFlags D_800D3400[8];

/* Battle turn state; only the acting slot. */
typedef struct {
    u8 unk0[0x2D3];
    u8 actor;        /* +2d3 */
} TurnState;
extern TurnState *D_800C3EAC;

#endif
