#ifndef BATTLE_GROUPS_H
#define BATTLE_GROUPS_H

#include "common.h"

/* The formation groups (32 entries of 4 bytes from 800d301c), in four sets
 * of eight: the party's groups (0-7), the enemies' (8-15), and each member
 * fighting in a gear placed alone (party 16-23, enemies 24-31: the flagged
 * slots' bases 0x10 and 0x18). The battle setup (ovl2615 func_801E4160)
 * fills them from the groups of the battle's formation (resident/formation.h);
 * the battle's approach moves keep them (battle/formation.h). */
typedef struct GroupEntry {
    u8 count;
    u8 members;        /* member bits (battle_get_slot_bit) */
    u8 unk2[2];
} GroupEntry;

extern GroupEntry battle_formation_groups[32];

#endif
