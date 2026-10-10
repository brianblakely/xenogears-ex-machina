#ifndef BATTLE_FORMATION_H
#define BATTLE_FORMATION_H

#include "common.h"

/* The battle formation: its areas, group positions and the route points
 * between groups (the formation data battle_formation) and the approach routes
 * (battle.c 80085310-80088490), which move the slots between the formation
 * groups (battle/groups.h). */

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

extern Formation *battle_formation;

/* Plan the approach route into the battle area's path (BattleArea): the
 * actor's position, then up to seven formation points; unused points are
 * 0xFFFF. */
s32 battle_plan_approach_route(u8 actor, u8 target);
void battle_join_target_group(u8 actor, u8 target);       /* move actor into target's group */
void battle_join_empty_target_group(u8 actor, u8 target); /* move actor alone into target's empty group */
void battle_give_slot_own_group(s32 slot);                /* give slot a formation group of its own */
u8 battle_count_enemy_gear_group_members(u8 slot);        /* the slot's group's members among the flagged groups */

#endif
