#ifndef BATTLE_RESOLVER_H
#define BATTLE_RESOLVER_H

/* The action resolver's rolls and formulas (80096018-8009E53C). */

#include "combatant.h"

extern u8 D_800C34AE; /* a weapon-using attack found its item broken */
extern u8 D_800D2D10[4];  /* speeds 8009892c replaces by each gear part speed */

/* The persistent game data (8006d634), as far as the resolver uses it; its
 * character records are D_8006D8A0. */
typedef struct {
    u8 pad0[0x26C];
    CharacterRecord characters[11]; /* 0x26C */
    GearRecord gears[24];           /* 0x978 */
    u8 pad18D8[0x1930 - 0x18D8];
    u16 value1930; /* 0x1930: 8009892c adjusts gears below 0xbb, 80097d5c
                    * raises character 9 from 231 */
    u8 pad1932[0x2355 - 0x1932];
    u8 flags2355; /* 0x2355: 0x80 once character 9 was raised */
} GameData;

extern GameData D_8006D634;

typedef char GameDataLayoutCheck[(BATTLE_OFFSET(GameData, value1930) == 0x1930 &&
                                  BATTLE_OFFSET(GameData, flags2355) == 0x2355)
                                     ? 1
                                     : -1];

s32 func_80099498(void);
void func_8009B684(u8 kind, u16 flag);
void func_8009B46C(u16 *damage);
s8 func_8009D3A0(void);
u16 func_8009D948(void);
u16 func_8009DA04(void);
s32 func_8009DB54(s32 damage);
void func_8009E868(u8 kind, u16 flag);

#endif
