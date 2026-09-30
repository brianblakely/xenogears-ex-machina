#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80090A84);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80090C68);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80090E14);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80090FB4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80091430);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800914D0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80091B54);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80091C18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80091FF8);

/* Choose the actor's speed for the movement mode; vehicles also get state 1. */
s32 func_80092234(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        D_8009BE0C = 0x8C;
        break;
    case 6:
    case 7:
        D_8009BE0C = 0x78;
        actor->state = 1;
        break;
    }
    actor->u.step = D_8009BE0C << 12;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800922AC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800923A8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800925A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80092BE4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80092C70);

/* Release a resident object. */
void func_80092DD0(void) {
    func_800346D4(D_8009D498);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80092DF8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80092FD8);

/* Release a resident object. */
void func_800931B0(void) {
    func_800346D4(D_8009BD64);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800931D8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093354);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800933EC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093484);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093534);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800935DC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093660);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093740);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093978);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093A5C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093E8C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093F18);

/* Terrain type (low four attribute bits) at a position. */
s32 func_80093FE4(Vec3 *position) {
    return func_80093E8C(position) & 0xF;
}

/* Terrain height class (attribute bits 10-15) at a position. */
u32 func_80094004(Vec3 *position) {
    return ((u32)func_80093E8C(position) << 16) >> 26;
}

/* Terrain cell flags (bits 2-5 of the cell's fourth byte) at a position. */
s32 func_80094028(Vec3 *position) {
    return (func_80093660(position->vx, position->vz)[3] >> 2) & 0xF;
}

/* Look up the table entry for (row, column). */
s16 func_80094060(s16 row, s16 column) {
    return *(s16 *)((u8 *)D_8009BAC8 + row * 16 + column * 2);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094088);

/* Distance between two positions on the ground plane, in world units. */
#ifdef NON_MATCHING /* delta held in the argument register */
s32 func_80094154(Vec3 *a, Vec3 *b) {
    s32 dx;
    s32 dz;

    dx = a->vx - b->vx;
    if (dx < 0) {
        dx = -dx;
    }
    dx >>= 12;
    dz = a->vz - b->vz;
    if (dz < 0) {
        dz = -dz;
    }
    dz >>= 12;
    return func_80048C4C(dx * dx + dz * dz);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094154);
#endif

/* Heading from `from` to `to` (0..0xFFF) and its unit direction. */
void func_800941C4(Vec3 *from, Vec3 *to, Vec3 *direction, s16 *heading) {
    s32 x;

    x = to->vx;
    *heading = (func_8004B32C(to->vz - from->vz, x - from->vx) + 0x400) & 0xFFF;
    direction->vx = func_8003F8B0(*heading);
    direction->vz = -func_8003F8CC(*heading);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094238);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094364);

void func_80094434(void) {
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_8009443C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800945C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094750);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800948D8);
