#include "worldmap.h"

/* Place a party member's vehicle actor: parked at its spot, with the
 * player when riding, or hidden (3) when the member has no vehicle. */
s32 func_8008C364(WorldmapActor *actor, s32 member) {
    SVECTOR unused; /* unreferenced local: the original frame reserves it */
    s32 result;
    u32 state;

    result = 1;
    state = D_8006F368[member] != 0xFF;
    if ((D_8006EF8E[member].flags & 0x3FFF) >= 0x400) {
        state |= 2;
    }
    if ((&D_8006F8E5)[member] == 1) {
        state |= 4;
    }
    switch (state) {
    case 0:
    case 2:
    case 4:
    case 6:
    hidden:
        result = 3;
        actor->unk24 = 1;
        actor->position.vy = 0;
        actor->position.vz = 0;
        actor->position.vx = 0;
        break;
    case 1:
        if (D_8006D940[D_8006F368[member]].gear == 0xFF) {
            goto hidden;
        }
        func_8008C28C(actor, member);
        actor->unk24 = 1;
        actor->position.vy = 0;
        actor->position.vz = 0;
        actor->position.vx = 0;
        break;
    case 3:
        func_8008C28C(actor, member);
        actor->unk24 = 0;
        actor->position.vx = D_8006EF8E[member].x << 12;
        actor->position.vz = D_8006EF8E[member].z << 12;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        break;
    case 5:
    case 7:
        func_8008C28C(actor, member);
        actor->unk24 = 0;
        actor->position.vx = D_8009C5AC.vx;
        actor->position.vz = D_8009C5AC.vz;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        D_8006EF8E[member].flags = 0x400;
        break;
    }
    return result;
}

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

/* Start the parked vehicle 0: its model at the saved spot and heading. */
s32 func_8008C75C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009BDF8[0], 0x100, 0x1FD, 0x140, 0x140, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x2000);
    ((ModelInstance *)actor->handle)->flags &= ~4;
    actor->position.vx = D_8006EF8E[0].x << 12;
    actor->position.vz = D_8006EF8E[0].z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->unk24 = 1;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->turn = 0xC;
    actor->heading = D_8006EE5A;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008C844);

/* Start party vehicle 1: place it, and while its member rides (movement
 * modes 1-3) put it under the player; modes 4-7 mark it boarded. Save its
 * spot and heading. */
#ifdef NON_MATCHING /* store scheduling around the heading */
s32 func_8008D3F0(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = func_8008C364(actor, 1);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->turn = 0xC;
    actor->unk58 = 0xF;
    actor->heading = D_8006EE54.unk5C;
    actor->unk5C = actor->heading;
    if (D_8009BE10 > 0) {
        if (D_8009BE10 < 4) {
            if (D_8006F8E6 == 1) {
                actor->state = 1;
                actor->position.vx = D_8009C5AC.vx;
                actor->position.vz = D_8009C5AC.vz;
                actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
                actor->heading = D_8009C584;
            }
        } else if (D_8009BE10 < 8) {
            actor->state = 2;
            actor->unk24 = 1;
        }
    }
    D_8006EF8E[1].x = actor->position.vx >> 12;
    D_8006EF8E[1].z = actor->position.vz >> 12;
    D_8006EE54.unk5C = actor->heading;
    return result;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008D3F0);
#endif

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

/* Start the parked vehicle 1: its model at the saved spot and heading. */
s32 func_8008D590(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009BDF8[1], 0x100, 0x1FC, 0x160, 0x140, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x2000);
    ((ModelInstance *)actor->handle)->flags &= ~4;
    actor->position.vx = D_8006EF8E[1].x << 12;
    actor->position.vz = D_8006EF8E[1].z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->unk24 = 1;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->turn = 0xC;
    actor->heading = D_8006EE5C;
    return 1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008D678);

/* Start party vehicle 2: place it, and while its member rides (movement
 * modes 1-3) put it under the player; modes 4-7 mark it boarded. Save its
 * spot and heading. */
#ifdef NON_MATCHING /* store scheduling around the heading */
s32 func_8008DD6C(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = func_8008C364(actor, 2);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->turn = 0xC;
    actor->unk58 = 0x1F;
    actor->heading = D_8006EE54.unk5E;
    actor->unk5C = actor->heading;
    if (D_8009BE10 > 0) {
        if (D_8009BE10 < 4) {
            if (D_8006F8E7 == 1) {
                actor->state = 1;
                actor->position.vx = D_8009C5AC.vx;
                actor->position.vz = D_8009C5AC.vz;
                actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
                actor->heading = D_8009C584;
            }
        } else if (D_8009BE10 < 8) {
            actor->state = 2;
            actor->unk24 = 1;
        }
    }
    D_8006EF8E[2].x = actor->position.vx >> 12;
    D_8006EF8E[2].z = actor->position.vz >> 12;
    D_8006EE54.unk5E = actor->heading;
    return result;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008C364", func_8008DD6C);
#endif

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

/* Start the parked vehicle 2: its model at the saved spot and heading. */
s32 func_8008DF0C(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->handle = func_80024524(D_8009BDF8[2], 0x100, 0x1FB, 0x280, 0x100, 0x40);
    func_800245D8(actor->handle, 0);
    func_80022000(actor->handle, 0x2000);
    ((ModelInstance *)actor->handle)->flags &= ~4;
    actor->position.vx = D_8006EF8E[2].x << 12;
    actor->position.vz = D_8006EF8E[2].z << 12;
    actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
    actor->unk24 = 1;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->turn = 0xC;
    actor->heading = D_8006EE5E;
    return 1;
}

/* Restore the saved vehicle position (world units to 20.12). */
void func_8008DFF4(VECTOR *position) {
    position->vx = D_8006EE54.unk60 << 12;
    position->vy = D_8006EE54.unk62 << 12;
    position->vz = D_8006EE54.unk64 << 12;
}

/* Save the vehicle position in world units. */
void func_8008E034(VECTOR *position) {
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
s32 func_8008E0F0(VECTOR *position, s32 unused, s32 range) {
    VECTOR hit;
    VECTOR direction;
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
