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

/* Open the text window. */
s32 func_80092BE4(void) {
    D_8009BD24 = -1;
    func_80032F54(&D_8009D498, 0x3C0, 0x180, 0xA0, 0x78, 0x20, 1);
    D_8009D498.unk68 = 8;
    D_8009D498.flags |= 2;
    func_80034614(&D_8009D498);
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80092C70);

/* Release a resident object. */
void func_80092DD0(void) {
    func_800346D4(&D_8009D498);
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

/* Set the point's height to lie on the plane through `origin` with normal
 * `normal`. */
#ifdef NON_MATCHING /* matches once maspsx expands div (--expand-div) */
void func_800935DC(Vec3 *point, Vec3 *origin, Vec3 *normal) {
    point->vy = (-(normal->vx * (point->vx - origin->vx)) - normal->vz * (point->vz - origin->vz)) / normal->vy;
    point->vy += origin->vy;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800935DC);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093660);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093740);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093978);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093A5C);

/* Terrain attribute of the cell under a position. */
#ifdef NON_MATCHING /* final address sum operands swapped */
s32 func_80093E8C(Vec3 *position) {
    s32 x;
    s32 z;
    s32 row;
    s16 block;
    TerrainBlock *terrain;

    z = position->vz;
    row = (z >> 20) / 8;
    x = position->vx;
    block = row * D_8009D160 + (x >> 20) / 8;
    terrain = D_8009C184[block];
    return terrain->attributes[(((z & 0x7FF000) >> 19) << 4) | ((x & 0x7FF000) >> 19)];
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093E8C);
#endif

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
