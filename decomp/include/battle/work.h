#ifndef BATTLE_WORK_H
#define BATTLE_WORK_H

#include "common.h"
#include "resident/gamedata.h"

/* The battle's work area D_800CCCE8 (within the battle area D_800C3EB0, at
 * +0x8E38): the combatants, their command tables and item lists, and the
 * current action's results. The battle modules, the result screen and the
 * debug pages read it too. */

/* A 16-byte entry of the battle's character item list. */
typedef struct {
    u8 pad0[3];
    u8 durability; /* +3 */
    u8 pad4[2];
    u8 id; /* +6 */
    u8 pad7[3];
    u8 valueA;
    u8 valueB;
    u8 valueC;
    u8 padD[3];
} BattleItem;

/* A 20-byte entry of the battle's gear part list. */
typedef struct {
    u8 pad0[0xC];
    u8 durability; /* +0xC */
    u8 padD;
    u8 valueE;
    u8 id; /* +0xF */
    u8 value10;
    u8 value11;
    u8 pad12[2];
} BattlePart;

/* The item lists share one area: character items, or gear parts. Both hold
 * 50 entries followed by three the party members use. */
typedef union {
    struct {
        BattleItem list[50];
        BattleItem members[3];
    } items;
    struct {
        u8 pad[0x258];
        BattlePart list[50];
        BattlePart members[3];
    } parts;
} BattleItemLists;

/* Combatant record: 11 slots (0-2 party, 3-10 enemies) of 0x170 bytes. */
typedef struct Combatant {
    CharacterRecord pilot;
    GearRecord gear;
    u8 field148;
    u8 field149;
    u8 pad14A[0x14C - 0x14A];
    s32 field14C;       /* 0x14C: an enemy's experience (the results module totals it) */
    u8 field150[6];     /* 0x150: an enemy's two drops: chances [0..1] (percent), item
                         * ids [2..3], inventory lists [4..5] */
    u16 field156;       /* 0x156: an enemy's gold */
    u8 expWeightA; /* 0x158: experience share weights */
    u8 expWeightB;
    u8 flags15A; /* bit 0x80: fighting in a gear */
    u8 pad15B;
    u8 statusTimers[0x10]; /* remaining turns per timed status; [0] the
                            * delay counter of status bit 0x2000 */
    u8 pad16C[0x170 - 0x16C];
} Combatant;

/* Command descriptor (0x28 bytes); the party's command tables hold 38 per
 * member, their gears' 42. */
typedef struct CommandDescriptor {
    u16 state; /* 0x00: 1 usable, 0x2000 sealed */
    u16 name;  /* 0x02: shown while it runs */
    u8 pad4[0x8 - 0x4];
    u16 elements; /* 0x08: element bits */
    u16 flagsA;   /* 0x0A */
    u8 padC[0x10 - 0xC];
    u8 itemKinds; /* 0x10: 0x80 uses entry 0's item, 0x10 entry 3's */
    u8 power; /* 0x11 */
    u8 pad12;
    u8 cost; /* 0x13: EP cost, doubled/halved by statuses at battle start */
    u8 accuracy; /* 0x14 */
    s8 hitBonus; /* 0x15 */
    u8 formula; /* 0x16: index into the formula table */
    u8 apCost; /* 0x17: AP a combo step costs */
    u8 chanceSource; /* 0x18: 0 attacker +0x60, 1 field1C */
    u8 pad19;
    u8 amountKind;  /* 0x1A: what 80096018 writes as the amount */
    u8 defenseKind; /* 0x1B: which defense value 80097610 uses */
    u8 field1C;
    u8 field1D; /* 0x1D: a timed status kind */
    u16 field1E; /* 0x1E: its flag bit */
    u8 attributes[4]; /* 0x20: copied to the battle's current command */
    u16 hudState;     /* 0x24: shown in the gear HUD */
    u8 pad26;
    s8 weight; /* 0x27: turn timer penalty */
} CommandDescriptor;

/* The gear command HUD state (0x30 bytes). */
typedef struct {
    u16 commands[15]; /* 0x00: the gear's first command states */
    s16 attack;       /* 0x1E */
    u8 pad20[0x24 - 0x20];
    u16 status; /* 0x24: warning bits */
    u16 charge; /* 0x26 */
    u8 level;   /* 0x28: attack level */
    u8 field29;
    u8 defense; /* 0x2A */
    u8 pad2B;
    u8 overheat; /* 0x2C */
    u8 pad2D;
    u16 boostChance; /* 0x2E */
} GearHud;

/* Battle work area D_800CCCE8; D_800C34B0 points at it. */
typedef struct {
    Combatant records[11]; /* 0x0000 */
    u32 expTotals[3][2];   /* 0x0FD0: each member's experience totals, saved at
                            * battle start and counted up on the result screen */
    s32 toCount[3][2];     /* 0x0FE8: result values still to count */
    u16 savedMax[3][2]; /* 0x1000: each member's maximum HP and EP */
    u8 dropCategories[8]; /* 0x100C: the drop rolled per enemy (the results module):
                           * its inventory list */
    u8 dropIds[8];        /* 0x1014: and its item id, 0 none */
    u8 learntCounter[3];  /* 0x101C: the counter skill each member learnt */
    u8 learntLevel[3];    /* 0x101F: the level skill each member learnt */
    u8 levelGains[3][2];  /* 0x1022: the levels A and B each member gained */
    u8 savedStats[3][8]; /* 0x1028: each member's base stats */
    u8 resultStats[3][8]; /* 0x1040: and after the battle's growth (the results screen) */
    CommandDescriptor partyCommands[3][38]; /* 0x1058 */
    CommandDescriptor gearCommands[3][42];  /* 0x2228 */
    CommandDescriptor enemyCommands[199];   /* 0x35D8 */
    u8 pad54F0[0x54F8 - 0x54F0];
    BattleItemLists lists; /* 0x54F8 */
    u8 pad5B74[0x5F20 - 0x5B74];
    struct GrowthFile *growth; /* 0x5F20: the growth data file (the results module) */
    GearHud gearHud; /* 0x5F24 */
    s32 field5F54[3]; /* 0x5F54: per party member */
    s32 field5F60[3]; /* 0x5F60: per party member, in a gear */
    u32 damage[12]; /* 0x5F6C */
    u32 experience; /* 0x5F9C: the experience won */
    u8 resultCode[12]; /* 0x5FA0: 0xFF untouched */
    u16 targetMask;    /* 0x5FAC: effect target mask */
    u16 targetMask2;   /* 0x5FAE */
    u16 shownCommand;  /* 0x5FB0 */
    u8 pad5FB2[0x5FB4 - 0x5FB2];
    u16 defeated;      /* 0x5FB4: the enemies defeated, a bit per enemy */
    u16 revived;       /* 0x5FB6: slots an item revived */
    u8 pad5FB8[0x5FBC - 0x5FB8];
    u8 commandAttributes[4]; /* 0x5FBC */
    u8 commandIndexCopy;     /* 0x5FC0 */
    u8 attackerIndex;        /* 0x5FC1 */
    u8 commandIndex;         /* 0x5FC2 */
    u8 pad5FC3;
    s8 penalty;              /* 0x5FC4: experience lost, in quarters */
    u8 pad5FC5[0x5FC7 - 0x5FC5];
    u8 message; /* 0x5FC7: battle message code */
} BattleWork;

LAYOUT_CHECK(BattleWorkLayout, sizeof(Combatant) == 0x170 && sizeof(CommandDescriptor) == 0x28 &&
                                   sizeof(BattleItem) == 0x10 && sizeof(BattlePart) == 0x14 &&
                                   OFFSET_OF(BattleWork, targetMask) == 0x5FAC &&
                                   OFFSET_OF(BattleWork, message) == 0x5FC7 &&
                                   OFFSET_OF(Combatant, statusTimers) == 0x15C &&
                                   OFFSET_OF(CommandDescriptor, weight) == 0x27 &&
                                   OFFSET_OF(BattleWork, field5F54) == 0x5F54);
LAYOUT_CHECK(BattleWorkResults, OFFSET_OF(BattleWork, dropCategories) == 0x100C &&
                                    OFFSET_OF(BattleWork, levelGains) == 0x1022 &&
                                    OFFSET_OF(BattleWork, resultStats) == 0x1040 &&
                                    OFFSET_OF(BattleWork, growth) == 0x5F20 &&
                                    OFFSET_OF(BattleWork, experience) == 0x5F9C &&
                                    OFFSET_OF(BattleWork, defeated) == 0x5FB4 &&
                                    OFFSET_OF(BattleWork, penalty) == 0x5FC4);

extern BattleWork D_800CCCE8;

#endif
