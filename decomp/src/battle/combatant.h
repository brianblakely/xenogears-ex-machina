#ifndef BATTLE_COMBATANT_H
#define BATTLE_COMBATANT_H

#include "common.h"

/* A status word pair: the statuses in effect, and those that are permanent
 * (they do not wear off). Timed statuses are tested on the whole word. */
typedef union {
    u32 word;
    struct {
        u16 active;
        u16 permanent;
    } half;
} StatusPair;

/* One of a unit record's four 8-byte entries at +0. */
typedef struct {
    u8 pad0[2];
    u8 value2;
    u8 value3;
    u8 value4;
    u8 pad5;
    u8 id;                  /* +6 */
    u8 pad7;
} UnitEntry;

/* Unit record (0xA4 bytes): the game data's character and gear records
 * (D_8006D8A0, 31 of them: 11 characters, then 20 gears) and their battle
 * copies, the pilot's at combatant +0 and the gear's at +0xA4. Fields are named
 * as their uses are recovered; unknown bytes stay padding. */
typedef struct {
    UnitEntry entries[4];   /* 0x00 */
    u8 pad20[0x32 - 0x20];
    u16 flags32;            /* 0x32: bit 0x40 doubles status durations */
    u8 pad34[0x36 - 0x34];
    u16 flags36;            /* 0x36 */
    u16 field38;            /* 0x38: a gear's fuel */
    u16 field3A;            /* 0x3A: a gear's maximum fuel */
    u8 pad3C[0x4C - 0x3C];
    u16 hp;                 /* 0x4C */
    u16 maxHp;              /* 0x4E */
    u16 ep;                 /* 0x50 */
    u16 maxEp;              /* 0x52 */
    u8 pad54[0x56 - 0x54];
    u8 characterId;         /* 0x56 */
    u8 pad57[0x5B - 0x57];
    u8 accuracy;            /* 0x5B: added to a command's accuracy */
    u8 pad5C[0x5E - 0x5C];
    u8 field5E;
    u8 field5F;
    u32 gearHp;             /* 0x60: gear records */
    u32 maxGearHp;          /* 0x64 */
    u8 pad68[0x6F - 0x68];
    u8 entryItems[4];       /* 0x6F: item slot of each entry */
    u8 pad73[0x7A - 0x73];
    u16 status7A;
    u16 status7C;           /* bits 0xC002 mark a member out of action */
    u8 pad7E[0x80 - 0x7E];
    u16 status80;
    u16 status82;
    StatusPair status84;
    StatusPair status88;
    StatusPair status8C;
    u16 useCounts[7];       /* 0x90 */
    u8 pad9E[0xA0 - 0x9E];
    u8 gearId;              /* 0xA0: the pilot's gear (game-data record 11 + id) */
    u8 padA1[0xA4 - 0xA1];
} UnitRecord;

/* A 16-byte item entry of the battle work area. */
typedef struct {
    u8 pad0[3];
    u8 durability;          /* +3 */
    u8 pad4[2];
    u8 id;                  /* +6 */
    u8 pad7[3];
    u8 valueA;
    u8 valueB;
    u8 valueC;
    u8 padD[3];
} BattleItem;

/* Combatant record: 11 slots (0-2 party, 3-10 enemies) of 0x170 bytes. */
typedef struct {
    UnitRecord pilot;
    UnitRecord gear;
    u8 field148;
    u8 field149;
    u8 pad14A[0x15A - 0x14A];
    u8 flags15A;            /* bit 0x80: fighting in a gear */
    u8 pad15B;
    volatile u8 statusTimers[0x10]; /* remaining turns per timed status */
    u8 pad16C[0x170 - 0x16C];
} Combatant;

/* Command descriptor (0x28 bytes); the party's command tables hold 38 per
 * member, their gears' 42. */
typedef struct {
    u16 state;              /* 0x00: 1 usable, 0x2000 sealed */
    u8 pad2[0xA - 0x2];
    u16 flagsA;             /* 0x0A */
    u8 padC[0x10 - 0xC];
    u8 pad10;
    u8 power;               /* 0x11 */
    u8 pad12[0x14 - 0x12];
    u8 accuracy;            /* 0x14 */
    u8 pad15[0x20 - 0x15];
    u8 attributes[4];       /* 0x20: copied to the battle's current command */
    u8 pad24[0x28 - 0x24];
} CommandDescriptor;

/* Battle work area D_800CCCE8; D_800C34B0 points at it. */
typedef struct {
    Combatant records[11];                  /* 0x0000 */
    u8 padFD0[0x1058 - 0xFD0];
    CommandDescriptor partyCommands[3][38]; /* 0x1058 */
    CommandDescriptor gearCommands[3][42];  /* 0x2228 */
    u8 pad35D8[0x54F8 - 0x35D8];
    BattleItem items[50];                   /* 0x54F8 */
    BattleItem gearItems[3];                /* 0x5818 */
    u8 pad5848[0x5F6C - 0x5848];
    u32 damage[12];                         /* 0x5F6C */
    u8 pad5F9C[0x5FA0 - 0x5F9C];
    u8 resultCode[12];                      /* 0x5FA0: 0xFF untouched */
    u16 targetMask;                         /* 0x5FAC */
    u16 targetMask2;                        /* 0x5FAE */
    u8 pad5FB0[0x5FBC - 0x5FB0];
    u8 commandAttributes[4];                /* 0x5FBC */
    u8 commandIndexCopy;                    /* 0x5FC0 */
    u8 attackerIndex;                       /* 0x5FC1 */
    u8 commandIndex;                        /* 0x5FC2 */
    u8 pad5FC3[0x5FC7 - 0x5FC3];
    u8 defendEffect;                        /* 0x5FC7 */
} BattleWork;

/* Layout checks (a negative array size fails the build). */
#define BATTLE_OFFSET(type, field) ((u32)&((type *)0)->field)
typedef char BattleLayoutCheck[(sizeof(UnitRecord) == 0xA4 && sizeof(Combatant) == 0x170
                               && sizeof(CommandDescriptor) == 0x28 && sizeof(BattleItem) == 0x10
                               && BATTLE_OFFSET(BattleWork, targetMask) == 0x5FAC
                               && BATTLE_OFFSET(BattleWork, defendEffect) == 0x5FC7) ? 1 : -1];

extern UnitRecord D_8006D8A0[31];
extern u8 D_8006F8BA[];                 /* item durability by slot */
extern u8 D_8006F5C4[];                 /* inventory counts */
extern u8 D_8006F65A[];                 /* inventory items */

/* Per-character battle data in the game data (0x20 bytes). */
typedef struct {
    u16 mask0;
    u16 mask2;
    u16 mask4;
    u16 mask6;
    u8 pad8[0x17 - 0x8];
    u8 field17;
    u8 pad18[0x1A - 0x18];
    u16 flags1A;
    u8 pad1C[0x20 - 0x1C];
} CharacterBattleData;

extern CharacterBattleData D_8006ECF4[11];

extern BattleWork D_800CCCE8;
extern BattleWork *D_800C34B0;

extern u8 D_800C34AD;                   /* formation mode */
extern CommandDescriptor *D_800C3DFC;   /* current command descriptor */
extern Combatant *D_800C3E00;           /* attacker record */
extern u8 D_800C3E04;                   /* attacker slot */
extern Combatant *D_800C3E34;           /* target record */
extern u8 D_800C3E50;                   /* target slot */
extern UnitRecord *D_800D2DC8;          /* target's gear record */
extern u8 D_800D2C34;
extern u8 D_800D2D24[3];                /* party character ids, 0x7F none */

s32 func_8003FA38(void);                /* resident rand: 0..0x7FFF */

void func_80099CF0(UnitRecord *gear, Combatant *record, volatile u8 *timers);
void func_8009B104(u8 slot, Combatant *chuchu);
void func_8009BE0C(void);

#endif
