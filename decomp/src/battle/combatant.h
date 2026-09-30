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

/* One of a character record's four 8-byte entries at +0. */
typedef struct {
    u16 field0;
    u8 value2;
    u8 value3;
    u8 value4;
    u8 pad5;
    u8 id; /* +6 */
    u8 pad7;
} CharacterEntry;

/* Character record (0xA4 bytes): the game data's (D_8006D8A0) and the battle
 * copy at combatant +0. Fields are named as their uses are recovered; unknown
 * bytes stay padding. */
typedef struct {
    CharacterEntry entries[4]; /* 0x00 */
    u8 pad20[0x32 - 0x20];
    u16 flags32; /* 0x32: bit 0x40 doubles status durations */
    u16 flags34; /* 0x34: bit 0x800 reacts while down */
    u16 flags36; /* 0x36 */
    u16 weakness; /* 0x38: weak element bits 0x3f, 0x40 very weak */
    u16 field3A; /* 0x3A */
    u8 pad3C[0x4C - 0x3C];
    u16 hp;     /* 0x4C */
    u16 maxHp;  /* 0x4E */
    u16 ep;     /* 0x50 */
    u16 maxEp;  /* 0x52 */
    u8 field54; /* 0x54 */
    u8 pad55;
    u8 characterId; /* 0x56 */
    u8 pad57[0x5A - 0x57];
    u8 speed;    /* 0x5A */
    u8 accuracy; /* 0x5B: added to a command's accuracy */
    u8 pad5C[0x5E - 0x5C];
    u8 field5E;
    u8 field5F;
    u8 field60; /* 0x60: chance in percent */
    u8 pad61;
    u8 field62; /* 0x62 */
    u8 pad63[0x6F - 0x63];
    u8 entryItems[4]; /* 0x6F: item slot of each entry */
    u8 pad73[0x7A - 0x73];
    u16 status7A;
    u16 status7C; /* bits 0xC002 mark a member out of action; 0x80 inactive,
                   * 0x1000 slow (ticks every other frame), 0x2000 delay
                   * counter statusTimers[0] active */
    u8 pad7E[0x80 - 0x7E];
    u16 status80; /* 0x1000 turn timer held */
    u16 status82;
    StatusPair status84; /* 0x8000 haste */
    StatusPair status88;
    StatusPair status8C;
    u16 useCounts[7]; /* 0x90 */
    u8 pad9E[0xA0 - 0x9E];
    u8 gearId; /* 0xA0: the pilot's gear */
    u8 padA1[0xA4 - 0xA1];
} CharacterRecord;

/* One of a gear record's four 8-byte part entries at +0x10. */
typedef struct {
    u8 pad0[2];
    u8 valueE;  /* +2 */
    u8 value10; /* +3 */
    u8 value11; /* +4 */
    u8 id;      /* +5 */
    u8 pad6[2];
} GearEntry;

/* Gear record (0xA4 bytes): the game data's (after the characters) and the
 * battle copy at combatant +0xA4. */
typedef struct {
    u8 pad0[4];
    u8 partItems[4]; /* 0x04: item slot of each part */
    u8 pad8[0x10 - 0x8];
    GearEntry entries[4]; /* 0x10 */
    u8 pad30[0x38 - 0x30];
    u16 fuel;    /* 0x38 */
    u16 maxFuel; /* 0x3A */
    u8 attack;   /* 0x3C */
    u8 pad3D[0x3F - 0x3D];
    u8 attackScale; /* 0x3F */
    u8 pad40[0x4F - 0x40];
    u8 field4F; /* 0x4F */
    u8 pad50[0x57 - 0x50];
    u8 chargeRate; /* 0x57 */
    u8 pad58[0x60 - 0x58];
    u32 hp;    /* 0x60 */
    u32 maxHp; /* 0x64 */
    u8 pad68[0x72 - 0x68];
    u16 armor; /* 0x72 */
    u8 pad74[0x7C - 0x74];
    u16 status7C;
    u16 field7E; /* 0x7E: bit 0x80 blocks fuel drain */
    u16 status80;
    u16 status82;
    StatusPair status84;
    u8 resistances[16]; /* 0x88: by element bit */
    u8 speed;   /* 0x98 */
    u8 defense; /* 0x99: damage reduction in percent */
    u8 pad9A[0x9E - 0x9A];
    u8 frameFactor; /* 0x9E: attack scale in quarters */
    u8 pad9F[0xA4 - 0x9F];
} GearRecord;

/* The game data's unit records. */
typedef struct {
    CharacterRecord characters[11];
    GearRecord gears[20];
} UnitRecords;

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
typedef struct {
    CharacterRecord pilot;
    GearRecord gear;
    u8 field148;
    u8 field149;
    u8 pad14A[0x14C - 0x14A];
    s32 field14C;
    u8 field150[6];
    u16 field156;
    u8 pad158[0x15A - 0x158];
    u8 flags15A; /* bit 0x80: fighting in a gear */
    u8 pad15B;
    u8 statusTimers[0x10]; /* remaining turns per timed status; [0] the
                            * delay counter of status bit 0x2000 */
    u8 pad16C[0x170 - 0x16C];
} Combatant;

/* Command descriptor (0x28 bytes); the party's command tables hold 38 per
 * member, their gears' 42. */
typedef struct {
    u16 state; /* 0x00: 1 usable, 0x2000 sealed */
    u16 name;  /* 0x02: shown while it runs */
    u8 pad4[0x8 - 0x4];
    u16 elements; /* 0x08: element bits */
    u16 flagsA;   /* 0x0A */
    u8 padC[0x10 - 0xC];
    u8 itemKinds; /* 0x10: 0x80 uses entry 0's item, 0x10 entry 3's */
    u8 power; /* 0x11 */
    u8 pad12[0x14 - 0x12];
    u8 accuracy; /* 0x14 */
    s8 hitBonus; /* 0x15 */
    u8 formula; /* 0x16: index into the formula table */
    u8 pad17;
    u8 chanceSource; /* 0x18: 0 attacker +0x60, 1 field1C */
    u8 pad19;
    u8 amountKind; /* 0x1A: what 80096018 writes as the amount */
    u8 pad1B;
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
    u8 padFD0[0x1058 - 0xFD0];
    CommandDescriptor partyCommands[3][38]; /* 0x1058 */
    CommandDescriptor gearCommands[3][42];  /* 0x2228 */
    CommandDescriptor enemyCommands[199];   /* 0x35D8 */
    u8 pad54F0[0x54F8 - 0x54F0];
    BattleItemLists lists; /* 0x54F8 */
    u8 pad5B74[0x5F24 - 0x5B74];
    GearHud gearHud; /* 0x5F24 */
    s32 field5F54[3]; /* 0x5F54: per party member */
    s32 field5F60[3]; /* 0x5F60: per party member, in a gear */
    u32 damage[12]; /* 0x5F6C */
    u8 pad5F9C[0x5FA0 - 0x5F9C];
    u8 resultCode[12]; /* 0x5FA0: 0xFF untouched */
    u16 targetMask;    /* 0x5FAC: effect target mask */
    u16 targetMask2;   /* 0x5FAE */
    u16 shownCommand;  /* 0x5FB0 */
    u8 pad5FB2[0x5FBC - 0x5FB2];
    u8 commandAttributes[4]; /* 0x5FBC */
    u8 commandIndexCopy;     /* 0x5FC0 */
    u8 attackerIndex;        /* 0x5FC1 */
    u8 commandIndex;         /* 0x5FC2 */
    u8 pad5FC3[0x5FC7 - 0x5FC3];
    u8 message; /* 0x5FC7: battle message code */
} BattleWork;

/* Layout checks (a negative array size fails the build). */
#define BATTLE_OFFSET(type, field) ((u32) & ((type *)0)->field)
typedef char BattleLayoutCheck[(sizeof(CharacterRecord) == 0xA4 && sizeof(GearRecord) == 0xA4 &&
                                sizeof(Combatant) == 0x170 && sizeof(CommandDescriptor) == 0x28 &&
                                sizeof(BattleItem) == 0x10 && sizeof(BattlePart) == 0x14 &&
                                BATTLE_OFFSET(BattleWork, targetMask) == 0x5FAC &&
                                BATTLE_OFFSET(BattleWork, message) == 0x5FC7 &&
                                BATTLE_OFFSET(CharacterRecord, speed) == 0x5A &&
                                BATTLE_OFFSET(GearRecord, speed) == 0x98 &&
                                BATTLE_OFFSET(Combatant, statusTimers) == 0x15C &&
                                BATTLE_OFFSET(CommandDescriptor, weight) == 0x27 &&
                                BATTLE_OFFSET(BattleWork, field5F54) == 0x5F54)
                                   ? 1
                                   : -1];

extern UnitRecords D_8006D8A0;
extern u8 D_8006F8BA[]; /* item durability by slot */
extern u8 D_8006F8EA[]; /* gear part durability by slot */
extern u8 D_8006F5C4[]; /* inventory counts */
extern u8 D_8006F65A[]; /* inventory items */

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

extern u8 D_800C34AD;                 /* formation mode */
extern CommandDescriptor *D_800C3DFC; /* current command descriptor */
extern Combatant *D_800C3E00;         /* attacker record */
extern GearRecord *D_800D2D6C;        /* attacker's gear record */
extern u8 D_800C3E04;                 /* attacker slot */
extern Combatant *D_800C3E34;         /* target record */
extern u8 *D_800C3D60;                /* the target's field 0x148 */
extern u8 *D_800C3D3C;                /* the attacker's attack level and maximum (+0x148) */
extern u8 D_800D2DC4;                 /* an ether check failed */
extern void (*D_800C34DC[])(void);    /* gear formula table */
extern u8 D_800C3E50;                 /* target slot */
extern GearRecord *D_800D2DC8;        /* target's gear record */
extern u8 D_800D2C34;
extern u8 D_800D2D24[3]; /* party character ids, 0x7F none */


void func_80099CF0(GearRecord *gear, Combatant *record, volatile u8 *timers);
void func_8009B104(u8 slot, Combatant *chuchu);
void func_8009BE0C(void);
s32 func_8009C050(u8 slot);
void func_80096824(void);
void func_8009AC48(u8 member, u8 checked);
void func_8009C4B4(void);
void func_8009C9C4(void);
void func_800995A0(u8 slot, u8 kind, u16 flag, u8 amount);
void func_8009CA90(void);
void func_8009CB68(u8 slot);
void func_8009E788(void);
s8 func_8009DBFC(s32 arg0);
u16 func_80096FBC(void);
u16 func_80097610(void);

#endif
