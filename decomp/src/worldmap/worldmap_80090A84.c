#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80090A84);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80090C68);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80090E14);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80090FB4);

/* Restore the actor from the saved camera state; vehicles get state 3. */
#ifdef NON_MATCHING /* saved-camera and yaw addresses not kept in registers */
s32 func_80091430(s32 index) {
    WorldmapActor *actor;
    Camera *saved;

    actor = &D_8009BE24[index];
    saved = &D_8009D55C;
    actor->position = saved->target;
    actor->position.pad = saved->target.pad;
    D_8009BD38.vz = 0;
    D_8009BD38.vy = D_8009D52C;
    actor->u.step = (s16)D_8009D52C;
    actor->unk58 = D_8009BD38.vy << 12;
    switch (D_8009BE10) {
    case 6:
    case 7:
        actor->state = 3;
        break;
    }
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80091430);
#endif

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800914D0);

/* Choose the camera distance for the movement mode (unchanged in mode 6). */
s32 func_80091B54(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    switch (D_8009BE10) {
    case 6:
        break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        actor->u.step = 3;
        D_8009D3F0 = 0x460000;
        D_8009BD38.vx = -0x260;
        actor->unk64 = (s32)D_8009B224;
        actor->unk68 = (s32)D_8009B234;
        break;
    case 7:
        actor->u.step = 3;
        D_8009D3F0 = 0x280000;
        D_8009BD38.vx = -0x260;
        actor->unk64 = (s32)D_8009B22C;
        actor->unk68 = (s32)D_8009B23C;
        break;
    }
    return 1;
}

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

/* Wrap a position (20.12) onto the area's extent. */
void func_80093354(VECTOR *position) {
    if (position->vx >= D_8009D160 << 23) {
        position->vx -= D_8009D160 << 23;
    }
    if (position->vx < 0) {
        position->vx += D_8009D160 << 23;
    }
    if (position->vz >= D_8009D2B4 << 23) {
        position->vz -= D_8009D2B4 << 23;
    }
    if (position->vz < 0) {
        position->vz += D_8009D2B4 << 23;
    }
}

/* Wrap a position (world units) onto the area's extent. */
void func_800933EC(VECTOR *position) {
    if (position->vx >= D_8009D160 << 11) {
        position->vx -= D_8009D160 << 11;
    }
    if (position->vx < 0) {
        position->vx += D_8009D160 << 11;
    }
    if (position->vz >= D_8009D2B4 << 11) {
        position->vz -= D_8009D2B4 << 11;
    }
    if (position->vz < 0) {
        position->vz += D_8009D2B4 << 11;
    }
}

/* Wrap a ground-plane offset (20.12) into half the area extent. */
void func_80093484(VECTOR *offset) {
    if (offset->vx < -0x4000000) {
        offset->vx += D_8009D160 << 23;
    } else if (offset->vx > 0x4000000) {
        offset->vx -= D_8009D160 << 23;
    }
    if (offset->vz < -0x4000000) {
        offset->vz += D_8009D2B4 << 23;
    } else if (offset->vz > 0x4000000) {
        offset->vz -= D_8009D2B4 << 23;
    }
}

/* Wrap a ground-plane offset (world units) into half the area extent. */
void func_80093534(VECTOR *offset) {
    if (offset->vx < -0x4000) {
        offset->vx += D_8009D160 << 11;
    } else if (offset->vx > 0x4000) {
        offset->vx -= D_8009D160 << 11;
    }
    if (offset->vz < -0x4000) {
        offset->vz += D_8009D2B4 << 11;
    } else if (offset->vz > 0x4000) {
        offset->vz -= D_8009D2B4 << 11;
    }
}

/* Set the point's height to lie on the plane through `origin` with normal
 * `normal`. */
void func_800935DC(VECTOR *point, VECTOR *origin, VECTOR *normal) {
    point->vy = (-(normal->vx * (point->vx - origin->vx)) - normal->vz * (point->vz - origin->vz)) / normal->vy;
    point->vy += origin->vy;
}

/* Terrain cell (4 bytes) under a position: blocks hold four quadrants of 9x9 cells. */
u8 *func_80093660(s32 x, s32 z) {
    s16 block;
    s16 quadrant;
    s16 cell;

    block = ((z >> 20) / 8) * D_8009D160 + (x >> 20) / 8;
    x = (x >> 12) & 0x7FF;
    z = (z >> 12) & 0x7FF;
    quadrant = 0;
    if (x >= 0x400) {
        quadrant = 1;
        x -= 0x400;
    }
    if (z >= 0x400) {
        quadrant |= 2;
        z -= 0x400;
    }
    cell = ((z >> 4) / 8) * 9 + (x >> 4) / 8;
    return (u8 *)D_8009C184[block] + quadrant * 0x144 + cell * 4;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093740);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093978);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80093A5C);

/* Terrain attribute of the cell under a position. */
s32 func_80093E8C(VECTOR *position) {
    s32 x, z;
    s16 block;
    s32 row;
    s16 *attributes;

    z = position->vz;
    row = (z >> 20) / 8;
    x = position->vx;
    block = row * D_8009D160 + (x >> 20) / 8;
    attributes = ((TerrainBlock *)D_8009C184[block])->attributes;
    return attributes[(((z & 0x7FF000) >> 19) << 4) | ((x & 0x7FF000) >> 19)];
}

/* Terrain layer (0-7) at a position: the cell's split plane picks attribute bits 4-6 or 7-9. */
s16 func_80093F18(VECTOR *position) {
    u32 attribute;
    s32 type;
    s32 dx, dz;

    attribute = func_80093E8C(position);
    type = attribute & 0xF;
    dx = (u16)(position->vx / 8) - D_8009B464[type].vx;
    dz = (u16)(position->vz / 8) - D_8009B464[type].vz;
    if ((dx * D_8009B364[type].vx >> 12) + (dz * D_8009B364[type].vz >> 12) > 0) {
        return (attribute >> 4) & 7;
    }
    return (attribute >> 7) & 7;
}

/* Terrain type (low four attribute bits) at a position. */
s32 func_80093FE4(VECTOR *position) {
    return func_80093E8C(position) & 0xF;
}

/* Terrain height class (attribute bits 10-15) at a position. */
u32 func_80094004(VECTOR *position) {
    return ((u32)func_80093E8C(position) << 16) >> 26;
}

/* Terrain cell flags (bits 2-5 of the cell's fourth byte) at a position. */
s32 func_80094028(VECTOR *position) {
    return (func_80093660(position->vx, position->vz)[3] >> 2) & 0xF;
}

/* Look up the table entry for (row, column). */
s16 func_80094060(s16 row, s16 column) {
    return *(s16 *)((u8 *)D_8009BAC8 + row * 16 + column * 2);
}

/* Slide `direction` along the slope under the position; 0 when level. */
s32 func_80094088(VECTOR *position, VECTOR *direction, VECTOR *out) {
    s32 type;
    s32 dot;

    type = (s16)func_80093FE4(position);
    dot = direction->vx * D_8009B264[type].nx + direction->vz * D_8009B264[type].nz;
    if (dot == 0) {
        out->vx = direction->vx;
        out->vz = direction->vz;
        return 0;
    }
    if (dot < 0) {
        out->vx = -D_8009B264[type].nx;
        out->vz = -D_8009B264[type].nz;
    } else {
        out->vx = D_8009B264[type].nx;
        out->vz = D_8009B264[type].nz;
    }
    return 1;
}

/* Distance between two positions on the ground plane, in world units. */
#ifdef NON_MATCHING /* delta held in the argument register */
s32 func_80094154(VECTOR *a, VECTOR *b) {
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
    return SquareRoot0(dx * dx + dz * dz);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094154);
#endif

/* Heading from `from` to `to` (0..0xFFF) and its unit direction. */
void func_800941C4(VECTOR *from, VECTOR *to, VECTOR *direction, s16 *heading) {
    s32 x;

    x = to->vx;
    *heading = (ratan2(to->vz - from->vz, x - from->vx) + 0x400) & 0xFFF;
    direction->vx = func_8003F8B0(*heading);
    direction->vz = -func_8003F8CC(*heading);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094238);

/* Find the region of path table `table` of kind `kind` containing the
 * position; it becomes the current path. */
#ifdef NON_MATCHING /* position loads scheduled after the table index */
s32 func_80094364(VECTOR *position, s32 table, s32 kind) {
    PathRegion *region;
    u32 world_x;
    u32 world_z;
    s32 x;
    s32 z;

    world_x = (u32)position->vx >> 12;
    world_z = (u32)position->vz >> 12;
    region = ((PathRegion **)D_8009BD00)[table];
    if (region->id != -1) {
        x = (u16)world_x;
        z = (u16)world_z;
        do {
            if (((x >= region->x) & (region->x + region->w >= x) & (z >= region->z) & (region->z + region->h >= z)) &&
                region->kind == kind) {
                D_8009D7D8 = (PathTable *)region;
                return 1;
            }
            region++;
        } while (region->id != -1);
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094364);
#endif

void func_80094434(void) {
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_8009443C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800945C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094750);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800948D8);
