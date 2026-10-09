#ifndef MENU_TABLES_H
#define MENU_TABLES_H

#include "common.h"

/* The menu screens' data tables (mode 5; see menu/screen.h): records of the
 * equipment, accessories, items and gear parts, unpacked from the menu's
 * resource file, and a directory of them in the menu state. Unknown fields
 * keep their offsets. */

/* An equipment record (0x10 bytes): weapons below 0x32, armour from 0x32. */
typedef struct EquipInfo {
    u16 users;   /* party bits of the members who can equip it */
    u16 unk2;
    u16 price;   /* 0x04 */
    u8 kind;     /* 0x06: weapon class; special parts share their weapon's */
    u8 unk7;
    u16 value;   /* 0x08 */
    u8 a;        /* 0x0a */
    u8 b;        /* 0x0b */
    u8 power;    /* 0x0c: attack or defence (slot39 keeps it in the character's
                  * entries[0].value4) */
    u8 unkD[3];
} EquipInfo;

/* An accessory record (0x10 bytes). */
typedef struct AccessoryInfo {
    u16 users;   /* party bits of the members who can wear it */
    u16 price;   /* 0x02 */
    u8 unk4[4];
    u8 amount;   /* 0x08: added to the wearer's +0x2d (the shops' defence) */
    u8 kind;     /* 0x09 */
    s16 value;   /* 0x0a */
    s16 stats;   /* 0x0c: bonuses raised by the amount */
    u16 groups;  /* 0x0e: accessories of one group do not add up */
} AccessoryInfo;

/* An item record (0x10 bytes). */
typedef struct ItemInfo {
    u16 unk0;
    u16 price;   /* 0x02 */
    u16 target;  /* 0x04: 0x4000 all, 0x1000 none, else one; low bits the kind */
    u8 use;      /* 0x06: 0x80 usable in the menu, 0x40 in battle, 0x20 on the
                  * field only, 0x10 cannot be sold */
    u8 unk7;
    u8 amount;   /* 0x08 */
    u8 unk9;
    s16 flags;   /* 0x0a: 0x8000 HP, 0x4000 EP, 4 stats, 2 +0x78, 1 debug */
    s16 stats;   /* 0x0c: stats raised (flag 4) or the +0x78 change (flag 2) */
    u8 unkE[2];
} ItemInfo;

/* A gear engine record (0x18 bytes). */
typedef struct GearEngineInfo {
    u32 users;   /* party bits of the members who can use it */
    u32 unk4;
    u16 unk8;
    u16 price;   /* 0x0a */
    u8 unkC[8];
    u8 unk14;
    u8 unk15;
    u8 unk16;
    u8 unk17;
} GearEngineInfo;

/* A gear part record (0x10 bytes). */
typedef struct GearPartInfo {
    u32 users;
    u8 unk4[2];
    u16 unk6;
    u8 unk8[2];
    u16 price;   /* 0x0a */
    u8 unkC;
    u8 unkD;
    u8 unkE;
    u8 unkF;
} GearPartInfo;

/* A gear frame record (0x14 bytes). */
typedef struct GearFrameInfo {
    u32 users;
    u16 price;   /* 0x04 */
    u8 unk6[2];
    u16 unk8;
    u16 unkA;
    u8 unkC[8];
} GearFrameInfo;

/* A gear accessory record (0x1c bytes); its values add to the gear's
 * equipment fields (801e433c). */
typedef struct GearAccessoryInfo {
    u32 users;
    u16 price;   /* 0x04 */
    u16 unk6;    /* 0x06: added to the gear's +0x44 */
    u16 groups;  /* 0x08 */
    u8 unkA[3];
    u8 unkD;     /* 0x0d: added to the gear's +0x40 */
    u8 unkE;     /* 0x0e: to its +0x42 */
    u8 unkF;
    u8 unk10[4]; /* 0x10: to its +0x50 */
    u8 unk14;    /* 0x14: to its +0x4d */
    u8 kind;     /* 0x15: effect kind 1-11 */
    u16 value;   /* 0x16: effect bits or amount */
    u8 unk18;    /* 0x18: to its +0x4c */
    u8 unk19;
    u8 unk1A;    /* 0x1a: to its +0x88 entries of the kind 4 bits */
    u8 unk1B;    /* 0x1b: to its +0x54 */
} GearAccessoryInfo;

/* A gear weapon (or special part) record (0x14 bytes). */
typedef struct GearWeaponInfo {
    u8 attrs[4]; /* copied to the gear's fileVariant and spriteVariants */
    u32 users;   /* 0x04: gears that can equip it */
    u16 price;   /* 0x08 */
    u8 unkA[4];
    u8 unkE;
    u8 kind;     /* 0x0f */
    u8 unk10;
    u8 unk11;
    u16 unk12;
} GearWeaponInfo;

/* A gear's stats as the equipment screens show them (801e3c2c). */
typedef struct GearSummary {
    u32 hp;
    u32 max_hp;        /* 0x04 */
    u16 defense;       /* 0x08: body defence with the parts' */
    u16 ether_defense; /* 0x0a: the pilot's ether defence with the armour */
    u16 value68;       /* 0x0c: +0x68 with the parts' */
    u16 value6a;       /* 0x0e */
    u16 fuel;          /* 0x10 */
    u16 max_fuel;      /* 0x12 */
    u16 attack;        /* 0x14 */
    u8 hit;            /* 0x16 */
    u8 speed;          /* 0x17: less the frame's penalty */
    u8 frame_factor;   /* 0x18 */
    u8 value9d;        /* 0x19 */
    u8 guard;          /* 0x1a */
    u8 pad1b;
} GearSummary;

/* The data table directory (MenuState tables). */
typedef struct MenuTables {
    EquipInfo *equipment;                /* 0x00 */
    AccessoryInfo *accessories;          /* 0x04 */
    GearEngineInfo *engines;             /* 0x08 */
    GearPartInfo *parts;                 /* 0x0c */
    GearFrameInfo *frames;               /* 0x10 */
    GearAccessoryInfo *gear_accessories; /* 0x14 */
    GearWeaponInfo *gear_weapons;        /* 0x18 */
    ItemInfo *items;                     /* 0x1c */
    struct ArtInfo *arts[31];            /* 0x20: per character, then per gear from 11 (slot39) */
    GearSummary gear;                    /* 0x9c */
    u16 stats[9];                        /* 0xb8: a member's or gear's stats as shown */
    u8 padca[2];
} MenuTables;

LAYOUT_CHECK(MenuTablesSizes, sizeof(EquipInfo) == 0x10 && sizeof(AccessoryInfo) == 0x10 &&
                                  sizeof(ItemInfo) == 0x10 && sizeof(GearEngineInfo) == 0x18 &&
                                  sizeof(GearPartInfo) == 0x10 && sizeof(GearFrameInfo) == 0x14 &&
                                  sizeof(GearAccessoryInfo) == 0x1C && sizeof(GearWeaponInfo) == 0x14 &&
                                  sizeof(GearSummary) == 0x1C && sizeof(MenuTables) == 0xCC);

#endif
