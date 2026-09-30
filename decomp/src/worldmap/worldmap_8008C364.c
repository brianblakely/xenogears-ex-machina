#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008C364);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008C530);

/* Update a kind-0 actor; flag it while riding a vehicle. */
s32 func_8008C6EC(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = func_8008C364(actor, 0);
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        actor->unk24 = 1;
        break;
    }
    return result;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008C75C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008C844);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008D3F0);

/* Update a kind-1 actor; flag it while riding a vehicle. */
s32 func_8008D520(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = func_8008C364(actor, 1);
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        actor->unk24 = 1;
        break;
    }
    return result;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008D590);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008D678);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008DD6C);

/* Update a kind-2 actor; flag it while riding a vehicle. */
s32 func_8008DE9C(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = func_8008C364(actor, 2);
    switch (D_8009BE10) {
    case 1:
    case 2:
    case 3:
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        actor->unk24 = 1;
        break;
    }
    return result;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008DF0C);

/* Restore the saved vehicle position (world units to 20.12). */
void func_8008DFF4(Vec3 *position) {
    position->vx = D_8006EE54.unk60 << 12;
    position->vy = D_8006EE54.unk62 << 12;
    position->vz = D_8006EE54.unk64 << 12;
}

/* Save the vehicle position in world units. */
void func_8008E034(Vec3 *position) {
    D_8006EE54.unk60 = position->vx >> 12;
    D_8006EE54.unk62 = position->vy >> 12;
    D_8006EE54.unk64 = position->vz >> 12;
}

/* Select the path table of scenes 15 and 16. */
void func_8008E078(void) {
    if (D_8009D738 != 0) {
        switch (D_8009BD60) {
        case 15:
            D_8009D7D8 = &D_8009B6C4[0];
            D_8009BD24 = D_8009B6C4[0].count;
            break;
        case 16:
            D_8009D7D8 = &D_8009B6C4[1];
            D_8009BD24 = D_8009B6C4[1].count;
            break;
        }
    }
}

/* First of sixteen headings in which a probe from the position hits; -1 if
 * none does. */
s32 func_8008E0F0(Vec3 *position, s32 unused, s32 range) {
    VECTOR hit;
    Vec3 direction;
    s32 heading;

    for (heading = 0; heading < 0x1000; heading += 0x100) {
        direction.vx = func_8003F8B0(heading);
        direction.vz = -func_8003F8CC(heading);
        if (func_80095414(position, &direction, &hit, range, 2) == 1) {
            return heading;
        }
    }
    return -1;
}
