#ifndef RESIDENT_GAMEDATA_H
#define RESIDENT_GAMEDATA_H

#include "common.h"

/* The game data game_data (0x2358 bytes, a common of the resident): the
 * saved game, which 8001b970 loads whole from directory 16 file 3 and the
 * file screens copy whole. Every image addresses its parts at these offsets;
 * members carry the names their readers' reviewed uses give them, and a
 * comment names a reader's other reading of the same bytes. */

/* One of a character record's four 8-byte entries at +0 (the menus read
 * entries[0] and [3] as the first and second weapon's values). */
typedef struct {
    u16 field0; /* +0: a weapon's value */
    u8 value2;
    u8 value3;  /* +3: 100 adds the value to +0x8e (slot39) */
    u8 value4;  /* +4: the weapon's level (slot39, from its record +0xc) */
    u8 pad5;
    u8 id;      /* +6 */
    u8 pad7;
} CharacterEntry;

/* A status word pair: the statuses in effect, and those that are permanent
 * (they do not wear off; the menus' accessory bits set them). Timed
 * statuses are tested on the whole word. */
typedef union {
    u32 word;
    struct {
        u16 active;
        u16 permanent;
    } half;
} StatusPair;

/* A character record (0xA4 bytes): the game data's eleven from +0x26C
 * (8006d8a0) and the battle copy at each combatant's start. */
typedef struct {
    CharacterEntry entries[4]; /* 0x00 */
    u8 pad20[0x28 - 0x20];
    u8 equipAttack;       /* 0x28: equipment bonuses of the base values at 0x58 */
    u8 equipDefense;      /* 0x29 */
    u8 equipSpeed;        /* 0x2A */
    u8 equipEther;        /* 0x2B */
    u8 equipEtherDefense; /* 0x2C */
    u8 bodyDefense;       /* 0x2D */
    u8 equip5E;           /* 0x2E */
    u8 equip5F;           /* 0x2F */
    u8 hpBonus;           /* 0x30: maximum HP bonus in twentieths (the menus' accessory kinds 8, 9) */
    u8 epBonus;           /* 0x31: maximum EP bonus in twentieths */
    u16 flags32;          /* 0x32: bit 0x40 doubles status durations (accessory kind 5 bits) */
    u16 flags34;          /* 0x34: bit 0x800 reacts while down */
    u16 flags36;          /* 0x36 */
    u16 weakness;         /* 0x38: weak element bits 0x3f, 0x40 very weak */
    u16 field3A;          /* 0x3A */
    u32 expTotalA;        /* 0x3C: experience totals of levels A and B */
    u32 expTotalB;        /* 0x40 */
    u32 expNextA;         /* 0x44: experience to the next levels (slot39 shows A) */
    u32 expNextB;         /* 0x48 */
    u16 hp;               /* 0x4C */
    u16 maxHp;            /* 0x4E */
    u16 ep;               /* 0x50 */
    u16 maxEp;            /* 0x52 */
    u8 field54;           /* 0x54 */
    u8 field55;
    u8 characterId;       /* 0x56 */
    u8 pad57;
    u8 attack;            /* 0x58: the base values */
    u8 defense;           /* 0x59 */
    u8 speed;             /* 0x5A */
    u8 ether;             /* 0x5B: the ether attack value against etherDefense
                           * (80096FBC, 80097610), the heal of formula 1 (80095690)
                           * and of restoring arts, and added to a command's
                           * accuracy (the ether check 80096824, 8009A258) */
    u8 etherDefense;      /* 0x5C */
    u8 field5D;
    u8 field5E;           /* 0x5E: physical hit chance (80096AB8 adds the command's) */
    u8 field5F;           /* 0x5F: evasion against it (80096AB8) */
    u8 field60;           /* 0x60: chance in percent */
    u8 field61;
    u8 level;             /* 0x62 */
    u8 level2;            /* 0x63 */
    u8 field64[4];        /* 0x64 */
    u8 pad68[0x6A - 0x68];
    u8 weapons[5];        /* 0x6A: equipped weapons; [0] the weapon */
    u8 entryItems[5];     /* 0x6F: special parts, the ammo (weapon ids from 50, 0 none);
                           * [0]-[3] go with entries[0]-[3] */
    u8 accessories[3];    /* 0x74 */
    u8 field77;           /* 0x77: with +78 the whole part and +79 the tenths of the
                           * value slot39's status panel shows times 2.2 (801D7884) */
    u8 field78;           /* 0x78: the part field ext 6b and items set (801E31C0) */
    u8 field79;           /* 0x79 */
    u16 status7A;        /* 0x7A: the battle commands available (the battle setup masks
                           * the command menus with it) */
    u16 status7C;         /* 0x7C: bits 0xC002 mark a member out of action; 0x80 inactive,
                           * 0x1000 slow (ticks every other frame), 0x2000 delay counter
                           * statusTimers[0] active */
    u16 status7E;         /* 0x7E: immunities to status7C bits (accessory kind 1 bits) */
    u16 status80;         /* 0x80: 0x1000 turn timer held */
    u16 status82;         /* 0x82: accessory kind 2 bits */
    StatusPair status84;  /* 0x84: 0x8000 haste; permanent: accessory kind 3 bits */
    StatusPair status88;  /* 0x88: permanent: accessory kind 4 bits */
    StatusPair status8C;  /* 0x8C: permanent: accessory kind 7 bits */
    u16 useCounts[7];     /* 0x90: progress counters (the results screen counts them) */
    u8 pad9E[0xA0 - 0x9E];
    u8 gearId;            /* 0xA0: the piloted gear (gears[]), 0xff none */
    u8 fieldA1;           /* 0xA1: accessory kind 10 (slot39) */
    u8 padA2[0xA4 - 0xA2];
} CharacterRecord;

/* One of a gear record's four 8-byte part entries at +0x10 (the menus' weapon
 * slots). */
typedef struct {
    u16 field0; /* +0: a status flag bit; the weapon's value */
    u8 valueE;  /* +2 */
    u8 value10; /* +3 */
    u8 value11; /* +4 */
    u8 id;      /* +5 */
    u8 pad6[2];
} GearEntry;

/* A gear record (0xA4 bytes): the game data's twenty from +0x978
 * (8006dfac) and the battle copy at each combatant's +0xA4. Record 7 holds
 * character 7's derived values (the results screen). */
typedef struct GearRecord {
    u8 pad0[2];
    u8 engine;            /* 0x02: entry of the 0x18-byte engine table */
    u8 field3;            /* 0x03: entry of the 0x10-byte table */
    u8 partItems[4];      /* 0x04: special part of each weapon slot, its ammo (gear part
                           * ids from 50, 0 none) */
    u8 frame;             /* 0x08: entry of the 0x14-byte frame table */
    u8 parts[3];          /* 0x09: entries of the 0x1c-byte part table */
    u8 weapons[4];        /* 0x0C: weapons below item 0x32; [0] entry of the weapon table */
    GearEntry entries[4]; /* 0x10 */
    u8 pad30[0x38 - 0x30];
    u16 fuel;             /* 0x38 */
    u16 maxFuel;          /* 0x3A */
    u8 attack;            /* 0x3C */
    u8 pad3D;
    u8 field3E;
    u8 attackScale;       /* 0x3F */
    u16 equipBodyDefense; /* 0x40 */
    u16 equipArmor;       /* 0x42 */
    u16 equip68a;         /* 0x44 */
    u16 equip68b;         /* 0x46 */
    u16 field48;          /* 0x48: part kind 9 bits (slot39) */
    u8 speedPenalty;      /* 0x4A (slot39 reads a level here) */
    u8 pad4B;
    u8 equipGuard;        /* 0x4C */
    u8 equipHitBonus;     /* 0x4D */
    u8 equipSpeed;        /* 0x4E */
    u8 field4F;           /* 0x4F: part kind 5 amount; nonzero sets the pilot's flag 0x8000 */
    u8 speedBonus[4];     /* 0x50: speed of each part */
    u8 equipFrameFactor;  /* 0x54 */
    u8 pad55;
    u8 equipAttackScale;  /* 0x56 */
    u8 chargeRate;        /* 0x57 */
    u8 pad58[0x5C - 0x58];
    u8 fileVariant;       /* 0x5C: the gear's extra file (battle battle_gear_file_table), 0 none */
    u8 spriteVariants[3]; /* 0x5D: added (less one) to its animations' sprite kinds */
    u32 hp;               /* 0x60 */
    u32 maxHp;            /* 0x64 */
    u16 field68;
    u16 field6A;
    u16 field6C;
    u16 field6E;          /* 0x6E: part kind 4 bits (slot39) */
    u16 bodyDefense;      /* 0x70 */
    u16 armor;            /* 0x72 */
    u8 field74;
    u8 field75;
    u8 pad76[0x7C - 0x76];
    u16 status7C;         /* 0x7C: 0x8000 destroyed */
    u16 field7E;          /* 0x7E: bit 0x80 blocks fuel drain (part kind 1 bits) */
    u16 status80;
    u16 status82;         /* 0x82: part kind 2 bits */
    StatusPair status84;  /* 0x84: permanent: part kind 3 bits (low 12), weapon value */
    u8 resistances[16];   /* 0x88: by element bit (part amounts per kind 4 bit) */
    u8 speed;             /* 0x98 */
    u8 defense;           /* 0x99: damage reduction in percent */
    u8 pad9A[0x9C - 0x9A];
    u8 guard;             /* 0x9C: tenths a half hit loses (at most 9) */
    u8 field9D;
    u8 frameFactor;       /* 0x9E: attack scale in quarters */
    s8 hitBonus;          /* 0x9F: accuracy while its first ammo slot reads 0 rounds, as
                           * an empty slot does (battle battle_resolve_gear_hit_outcome); half as evasion */
    u8 padA0[0xA4 - 0xA0];
} GearRecord;

/* A character's skill record (0x20 bytes, eleven from +0x16C0, 8006ecf4):
 * its deathblow and ability bits and its tier. 801e5178 (slot39) resets
 * them. */
typedef struct {
    u16 counterSkills; /* 0x00 */
    u16 levelSkills;   /* 0x02: the arts known (slot39) */
    u16 unlocksA;      /* 0x04: battle's combo bits (8006ecf8), the pilot ability bits (ovl2602) */
    u16 unlocksB;      /* 0x06 */
    u8 pad8[0x17 - 0x8];
    u8 tier;           /* 0x17 */
    u8 pad18[0x1A - 0x18];
    u16 flags1A;       /* 0x1A: the fuel arts known, bit 0x8000 >> i for the gear's
                        * art i, command 37 + i (battle 8008CFB8, slot39 801DC3D8;
                        * field ext de) */
    u8 pad1C[0x20 - 0x1C];
} CharacterSkills;

/* The world map's return state (+0x1820, 8006ee54). */
typedef struct {
    u16 x;
    u16 z;
    u16 heading;
    u16 unk5A;
    u16 unk5C;
    u16 unk5E;
    u16 unk60;           /* 0x0C: saved vehicle position */
    u16 unk62;
    u16 unk64;
    u16 vehicle_heading; /* 0x12 */
    u16 flags;           /* 0x14: 0x4000 vehicle, 0x2000 restore, low bits kind */
    s16 unk6A;
    u16 unk6C;
    u16 unk6E;
    u16 unk70;
    u16 unk72;
    u16 unk74;
    u16 unk76;           /* 0x22 */
} WorldmapReturn;

/* The world map's saved flight position: fraction and world-unit halves. */
typedef struct {
    u16 x_frac;
    s16 x;
    u16 z_frac;
    s16 z;
    u16 count; /* flights started */
} FlightSave;

/* The saved system options word (+0x234C, 8006f980). */
typedef struct {
    u32 version : 4;   /* 1 once written */
    u32 option4 : 1;
    u32 option5 : 1;
    u32 option6 : 7;
    u32 option13 : 3;  /* the menu's level */
    u32 complete : 1;  /* every tracked flag was set */
    u32 unused : 15;
} SystemOptions;

typedef struct GameData {
    u8 names[31][0x14];           /* 0x0000: text codes, two bytes per code; 0-10 the
                                   * characters' names (slot39 reads ten line pairs) */
    CharacterRecord characters[11]; /* 0x026C: 8006d8a0 */
    GearRecord gears[20];           /* 0x0978: 8006dfac */
    u8 unk1648[0x16C0 - 0x1648];
    CharacterSkills skills[11];     /* 0x16C0: 8006ecf4 */
    WorldmapReturn worldmap;        /* 0x1820: 8006ee54 */
    u16 unk1844[3];                 /* 0x1844: 8006ee78 (the world map) */
    u16 unk184A;                    /* 0x184A */
    FlightSave flight;              /* 0x184C: 8006ee80 */
    u16 unk1856;                    /* 0x1856 */
    u8 unk1858[0x1924 - 0x1858];
    u32 gold;                       /* 0x1924: at most 999999999 */
    u8 unk1928[0x1930 - 0x1928];
    u16 vars[0x200];              /* 0x1930: the saved event variables (the field's 800c3a68);
                                   * [0] the scene id (8006ef64) */
    u16 joined;                   /* 0x1D30: characters who may join (game_data_party_state) */
    u16 available;                /* 0x1D32: characters available */
    u8 party[3];                  /* 0x1D34: character per party slot, 0xff empty */
    u8 unk1D37;
    u8 weaponCounts[100];         /* 0x1D38: the inventory lists, counts then ids */
    u8 weaponIds[100];            /* 0x1D9C */
    u8 accessoryCounts[200];      /* 0x1E00 */
    u8 accessoryIds[200];         /* 0x1EC8 */
    u8 itemCounts[150];           /* 0x1F90 */
    u8 itemIds[150];              /* 0x2026 */
    u8 gearPartCounts[100];       /* 0x20BC */
    u8 gearPartIds[100];          /* 0x2120 */
    u8 gearAccessoryCounts[150];  /* 0x2184 */
    u8 gearAccessoryIds[150];     /* 0x221A */
    u8 unk22B0;
    u8 inGear[3];                 /* 0x22B1: per party slot: riding its gear */
    u8 unk22B4[2];
    u16 flags;                    /* 0x22B6: option flags (0x4000 battle's boost, ext d1;
                                   * 0x2000/0x1000 a copy to character 9/10, ext d0) */
    /* The rounds left of each ammo id, by id - 50 for ids 50-97 (the 48 weapon
     * records ovl2615 battle_setup_load_party_and_enemy_files copies for the
     * battle). The special parts are ammo: system texts 23 and 51 name ids 50-72
     * "... Ammo", and only character 4 and its gears 5 and 13 may use them
     * (docs/scripts/field-events.md). The field menu sets an id's byte to 100 when
     * it is loaded (slot39 menu_equip_screen_commit_part), each action takes one
     * from the slots its command number names (battle battle_wear_weapon_items,
     * battle_wear_down_attacker_gear_parts), and a command whose descriptor names a
     * slot at 0 misses (battle_resolve_hit_outcome,
     * battle_resolve_gear_hit_outcome). The code forms an id's address from 50
     * bytes before each array, 8006f8ba (+0x2286) and 8006f8ea (+0x22B6, the
     * address of `flags`). An empty slot (id 0) reads gearAccessoryIds[108] or the
     * low byte of `flags`, which no code sets; ids 98 and 99 would reach
     * gearAmmo[0-1] and `locked`. */
    u8 ammo[48];                  /* 0x22B8: character 4's (weapon ids) */
    u8 gearAmmo[48];              /* 0x22E8: its gears' (gear part ids) */
    u16 locked;                   /* 0x2318: characters locked in place */
    u16 map;                      /* 0x231A: the saved map (scene) */
    u16 entry[3];                 /* 0x231C: its entry parameters (heading, area); the world
                                   * map reads entry[2] as its first flag word (game_data_worldmap_flag_word) */
    u16 flagWords[17];            /* 0x2322 */
    u8 progress[8];               /* 0x2344: progress flags, one bit each (8006f978) */
    SystemOptions options;        /* 0x234C */
    u8 unk2350[0x2355 - 0x2350];
    u8 flags2355;                 /* 0x2355: 0x80 once character 9 was raised */
    u8 unk2356[0x2358 - 0x2356];
} GameData;

extern GameData game_data;
extern GameData *game_current_data; /* the game data in use (&game_data) */

LAYOUT_CHECK(GameDataRecordSizes, sizeof(CharacterRecord) == 0xA4 && sizeof(GearRecord) == 0xA4 &&
                                      sizeof(CharacterSkills) == 0x20 &&
                                      sizeof(WorldmapReturn) == 0x24 &&
                                      OFFSET_OF(CharacterRecord, weapons) == 0x6A &&
                                      OFFSET_OF(CharacterRecord, status7A) == 0x7A &&
                                      OFFSET_OF(CharacterRecord, useCounts) == 0x90 &&
                                      OFFSET_OF(CharacterRecord, gearId) == 0xA0 &&
                                      OFFSET_OF(GearRecord, entries) == 0x10 &&
                                      OFFSET_OF(GearRecord, field48) == 0x48 &&
                                      OFFSET_OF(GearRecord, hp) == 0x60 &&
                                      OFFSET_OF(GearRecord, status7C) == 0x7C &&
                                      OFFSET_OF(GearRecord, resistances) == 0x88);
LAYOUT_CHECK(GameDataLayout, OFFSET_OF(GameData, characters) == 0x26C &&
                                 OFFSET_OF(GameData, gears) == 0x978 &&
                                 OFFSET_OF(GameData, skills) == 0x16C0 &&
                                 OFFSET_OF(GameData, flight) == 0x184C &&
                                 OFFSET_OF(GameData, gold) == 0x1924 &&
                                 OFFSET_OF(GameData, vars) == 0x1930 &&
                                 OFFSET_OF(GameData, joined) == 0x1D30 &&
                                 OFFSET_OF(GameData, weaponCounts) == 0x1D38 &&
                                 OFFSET_OF(GameData, gearAccessoryIds) == 0x221A &&
                                 OFFSET_OF(GameData, inGear) == 0x22B1 &&
                                 OFFSET_OF(GameData, flags) == 0x22B6 &&
                                 OFFSET_OF(GameData, ammo) == 0x22B8 &&
                                 OFFSET_OF(GameData, gearAmmo) == 0x22E8 &&
                                 OFFSET_OF(GameData, locked) == 0x2318 &&
                                 OFFSET_OF(GameData, map) == 0x231A &&
                                 OFFSET_OF(GameData, progress) == 0x2344 &&
                                 OFFSET_OF(GameData, flags2355) == 0x2355 &&
                                 sizeof(GameData) == 0x2358);

#endif
