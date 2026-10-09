#ifndef BATTLE_FORMATION_H
#define BATTLE_FORMATION_H

#include "common.h"

/* The battle formation: its areas, group positions and the route points
 * between groups (the formation data D_800D3364), the slots' formation
 * groups, and the approach routes (battle.c 80085310-80088490). */

/* Formation data (*800d3364). */
typedef struct {
    u8 distance;
    u8 points[7];      /* route points to the other group (formation area
                        * index in bits 0-2, bit 0x80 a flag), 0xFF ends */
} GroupLink;

/* A formation group's position (8 bytes). */
typedef struct {
    s16 x;
    s16 z;
    s16 enemyX; /* +0x04 the position for enemies */
    s16 enemyZ;
} GroupPosition;

/* A formation point. */
typedef struct {
    s16 x;
    s16 z;
} FormationPoint;

/* A formation area (0x20 bytes): its centre and its member places. */
typedef struct {
    FormationPoint centre;
    FormationPoint party[3];   /* +0x04 */
    FormationPoint enemies[4]; /* +0x10 */
} FormationArea;

typedef struct Formation {
    FormationArea areas[8];
    GroupPosition positions[8]; /* +0x100 */
    GroupLink links[8][8];      /* +0x140 per formation-group pair */
} Formation;

extern Formation *D_800D3364;

/* Formation group entries (4 bytes from 800d301c): four eight-group
 * sets, with flagged slots using bases 0x10 and 0x18. */
typedef struct GroupEntry {
    u8 count;
    u8 members;        /* member bits */
    u8 unk2[2];
} GroupEntry;

extern GroupEntry D_800D301C[32];

/* Plan the approach route into the battle area's path (BattleArea): the
 * actor's position, then up to seven formation points; unused points are
 * 0xFFFF. */
s32 func_800877E0(u8 actor, u8 target);
void func_80087EDC(u8 actor, u8 target); /* move actor into target's group */
void func_800881B8(u8 actor, u8 target); /* move actor alone into target's empty group */
void func_80088490(s32 slot); /* give slot a formation group of its own */
u8 func_800885D0(u8 slot);               /* the slot's group's members among the flagged groups */

#endif
