#include "worldmap.h"

/* Start the flying vehicle: restore its saved spot and heading, set its
 * turn rate by kind, and place it by movement mode (landed, boarded or
 * flying with the player); scene objects 0 and 1 follow it. */
#ifdef NON_MATCHING /* the result constant is set before the first call */
s32 func_8008E190(s32 index) {
    ActorScratch *scratch;
    WorldmapActor *actor;
    s32 result;

    scratch = (ActorScratch *)0x1F800000;
    actor = &D_8009BE24[index];
    actor->unk24 = 0;
    func_8008DFF4(&actor->position);
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk64 = 0;
    actor->unk60 = 0;
    actor->unk74 = 0;
    actor->unk68 = -0x280000;
    actor->unk70 = 0;
    actor->unk6C = 0;
    actor->heading = D_8006EE66;
    result = 1;
    switch (D_8006EE54.flags & 0x1FFF) {
    case 0:
        result = 3;
        break;
    case 1:
    case 2:
        actor->turn = 0xC;
        break;
    case 3:
    case 4:
        actor->turn = 0x20;
        break;
    }
    switch (D_8009BE10) {
    case 1:
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        break;
    case 2:
    case 3:
        actor->state = 1;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        break;
    case 4:
    case 5:
        actor->state = 3;
        actor->position.vx = D_8009C5AC.vx;
        actor->position.vz = D_8009C5AC.vz;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz) - 0x40000;
        actor->heading = D_8009C584;
        actor->unk74 = D_8009C170;
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        D_8009C620[3].visible = 1;
        D_8009C620[2].visible = 1;
        D_8009C620[1].visible = 1;
        break;
    case 7:
        actor->state = 2;
        actor->position.vx = D_8009C5AC.vx;
        actor->position.vz = D_8009C5AC.vz;
        actor->position.vy = D_8009C5AC.vy;
        actor->heading = D_8009C584;
        actor->unk74 = D_8009C170;
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    }
    D_8009C620[0].position = actor->position;
    D_8009C620[0].visible = actor->unk24;
    scratch->position.vx = -(actor->unk70 >> 12);
    scratch->position.vy = actor->heading;
    scratch->position.vz = D_8009BD38.vz;
    func_8004A92C(&scratch->position, &D_8009C620[0].matrix);
    func_8004A92C(&scratch->position, &D_8009C620[1].matrix);
    func_8008E034(&actor->position);
    D_8006EE54.vehicle_heading = actor->heading;
    switch (D_8009C5A8) {
    case 2:
        actor->state = 0x24;
        break;
    case 3:
        actor->state = 0x28;
        actor->unk7C = 0;
        break;
    case 4:
        actor->state = 0x30;
        actor->unk7C = 0;
        break;
    case 5:
        actor->state = 0x34;
        actor->unk7C = 0;
        break;
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E190);
#endif

/* Move scene object 0 to the vehicle actor and orient objects 0 and 1;
 * modes 4-5 show objects 1-3, modes 6-7 hide them. 3 once the vehicle kind
 * is cleared. */
s32 func_8008E4F4(s32 index) {
    ActorScratch *scratch;
    WorldmapActor *actor;
    s32 result;

    scratch = (ActorScratch *)0x1F800000;
    actor = &D_8009BE24[index];
    D_8009C620[0].position = actor->position;
    result = 1;
    D_8009C620[0].visible = actor->unk24;
    if (!(D_8006EE54.flags & 0x1FFF)) {
        result = 3;
    }
    switch (D_8009BE10) {
    case 1:
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        break;
    case 2:
    case 3:
        actor->state = 1;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz);
        break;
    case 4:
    case 5:
        D_8009C620[3].visible = 1;
        D_8009C620[2].visible = 1;
        D_8009C620[1].visible = 1;
        break;
    case 6:
    case 7:
        D_8009C620[3].visible = 0;
        D_8009C620[2].visible = 0;
        D_8009C620[1].visible = 0;
        break;
    }
    scratch->position.vx = -(actor->unk70 >> 12);
    scratch->position.vy = actor->heading;
    scratch->position.vz = D_8009BD38.vz;
    func_8004A92C(&scratch->position, &D_8009C620[0].matrix);
    func_8004A92C(&scratch->position, &D_8009C620[1].matrix);
    return result;
}

/* Start the flying vehicle at its saved position, height 0x80 and heading;
 * the camera and scene object 0 follow it. */
s32 func_8008E680(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    func_8008DFF4(&actor->position);
    actor->position.vy = 0x80000;
    actor->heading = D_8006EE54.vehicle_heading;
    actor->turn = 0x20;
    actor->unk74 = 3;
    actor->unk68 = -0x280000;
    actor->motion.vz = 0;
    actor->motion.vy = 0;
    actor->motion.vx = 0;
    actor->unk24 = 0;
    actor->unk60 = 0;
    actor->unk64 = 0;
    actor->unk6C = 0;
    actor->state = 8;
    D_8009D55C.target = actor->position;
    D_8009D52C = actor->heading;
    D_8009C620[0].position = actor->position;
    return 1;
}

#ifdef NON_MATCHING /* control flow: the original shares the tails GCC here keeps separate (688 bytes larger) */
/* Turn the vehicle towards `goal` (heading units), by at most `step` per frame
 * from the heading it had last frame (D_8006EE66). */
#define VEHICLE_TURN(goal, step)                                              \
    do {                                                                      \
        delta = (goal) - (s16)D_8006EE66;                                     \
        if (ABS(delta) > 0x800) {                                             \
            if (delta < 0) {                                                  \
                delta += 0x1000;                                              \
            } else {                                                          \
                delta -= 0x1000;                                              \
            }                                                                 \
        }                                                                     \
        if (ABS(delta) > (step)) {                                            \
            if (delta < 0) {                                                  \
                actor->heading = D_8006EE66 - (step);                         \
            } else {                                                          \
                actor->heading = D_8006EE66 + (step);                         \
            }                                                                 \
        }                                                                     \
        actor->heading &= 0xFFF;                                              \
    } while (0)

/* Tilt and heading of the vehicle model into scene objects 0 and 1. */
#define VEHICLE_ORIENT()                                                      \
    do {                                                                      \
        VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);                 \
        VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;                         \
        VEHICLE_SCRATCH->rotation.vy = actor->heading;                        \
        func_8004A92C(&VEHICLE_SCRATCH->rotation, &D_8009C620[0].matrix);     \
        func_8004A92C(&VEHICLE_SCRATCH->rotation, &D_8009C620[1].matrix);     \
    } while (0)

#define VEHICLE_SPOT(y)                                                       \
    do {                                                                      \
        VEHICLE_SCRATCH->spot.vx = actor->position.vx >> 12;                  \
        VEHICLE_SCRATCH->spot.vy = (y) >> 12;                                 \
        VEHICLE_SCRATCH->spot.vz = actor->position.vz >> 12;                  \
    } while (0)

/* The flying vehicle (Gear transport): boarding, flight with terrain and
 * landing checks, the scripted take-offs and landings, and the scene exits.
 * Returns 2 when the party leaves on foot. */
s32 func_8008E76C(s32 index) {
    s32 result;
    WorldmapActor *actor;
    s16 command;
    s32 kind;
    s32 hit;
    s32 delta;
    s32 terrain;
    s32 height;

    result = 1;
    actor = &D_8009BE24[index];
    command = actor->unk4;
    if (command == 4) {
        actor->unk4 = 0;
        if (D_8009C170 == ++actor->unk74) {
            func_80097770(8, 9);
            D_8009D55C.target = actor->position;
            D_8006EE68 |= 0x4000;
            kind = D_8006EE68 & 0x1FFF;
            D_8009D52C = actor->heading;
            switch (kind) {
            case 1:
                actor->state = 0xC;
                D_8009BE10 = command;
                actor->unk68 = func_80093978(actor->position.vx, actor->position.vz) + 0x18000;
                VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
                VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;
                VEHICLE_SCRATCH->rotation.vy = actor->heading;
                VEHICLE_SPOT(actor->position.vy);
                func_80089160(2, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
                D_8009BD04 = 0;
                break;
            case 2:
                actor->state = 0xC;
                D_8009BE10 = 5;
                actor->unk68 = func_80093978(actor->position.vx, actor->position.vz) + 0x18000;
                VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
                VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;
                VEHICLE_SCRATCH->rotation.vy = actor->heading;
                VEHICLE_SPOT(actor->position.vy);
                func_80089160(2, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
                D_8009BD04 = 0;
                break;
            case 3:
                func_8003A89C(D_80062528, 0, 0xF0);
                VEHICLE_SCRATCH->rotation.vx = actor->position.vx >> 12;
                VEHICLE_SCRATCH->rotation.vy = actor->position.vy >> 12;
                VEHICLE_SCRATCH->rotation.vz = actor->position.vz >> 12;
                func_80089160(func_80093F18(&actor->position) == kind ? 0x3E : 0x3D,
                              &VEHICLE_SCRATCH->rotation, NULL);
                func_80097770(9, 9);
                func_80097770(0xA, 9);
                func_80097770(0xB, 9);
                result = 2;
                actor->state = 8;
                actor->command_arg = 0x3C;
                D_8009BE10 = 7;
                D_8009BD04 = 0;
                break;
            }
            D_8006F8E5 = 1;
            if (D_8006F368[1] != 0xFF) {
                D_8006F8E6 = 1;
            }
            if (D_8006F368[2] != 0xFF) {
                D_8006F8E7 = 1;
            }
            func_80075228();
            D_8009D7D8 = (PathTable *)-1;
            D_8009BD24 = -1;
            D_8009CE68 = -1;
        }
    }
    switch (actor->state) {
    case 0:
    case 1:
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz) - 0x1000;
        func_8008BFD4(index, &actor->position, 0x40, 0x400);
        break;
    case 2:
        hit = func_80090FB4(actor);
        if (hit == 1) {
            D_8009D554 = 0;
            D_8009D7CC = 0;
            break;
        }
        if (hit == 4) {
            if ((s16)func_80094060(2, func_80093F18(&actor->position)) != 0) {
                hit = func_8008E0F0(&actor->position, (s32)&actor->motion, 0x68000);
                if (hit != -1) {
                    actor->state = 0x10;
                    actor->unk78 = hit;
                    actor->unk68 = func_80093978(actor->position.vx, actor->position.vz);
                    func_80097770(0xB, 0xA);
                    func_8003A89C(D_80062528, 0, 0xF0);
                    actor->unk7C = 1;
                    func_800894C8(0x3C);
                    func_800894C8(0x3F);
                    D_8009D7D8 = (PathTable *)-1;
                    D_8009BD24 = -1;
                    D_8009CE68 = -1;
                    D_8009BD60 = 0;
                    D_8009D738 = 0;
                    D_8009BD04 = 0;
                }
            }
            break;
        }
        hit = func_80095CD4(&actor->position, &actor->motion, SCRATCH_HIT, actor->unk60, D_8009BE10);
        if (hit == 0) {
            actor->unk60 = 0;
            actor->motion.vz = 0;
            actor->motion.vx = 0;
        }
        if (hit == 1) {
            func_8008C040(SCRATCH_HIT, 0x40, 0x20, &D_8009D738, &D_8009BD60);
            if (D_8009D738 != 2) {
                actor->position = *SCRATCH_HIT;
            } else {
                actor->unk60 = 0;
            }
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        func_80094238(&actor->position, 2);
        func_8008E078();
        VEHICLE_ORIENT();
        height = func_80093978(actor->position.vx, actor->position.vz) - actor->position.vy;
        if (ABS(height) < 0x60000 && actor->unk60 != 0) {
            VEHICLE_SPOT(actor->position.vy);
            terrain = func_80093F18(&actor->position);
            if (terrain == 2) {
                func_80089160(0x3F, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
                func_800894C8(0x3C);
            } else if (terrain == 3) {
                func_80089160(0x3C, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
                func_800894C8(0x3F);
            } else {
                func_800894C8(0x3C);
                func_800894C8(0x3F);
            }
        } else {
            func_800894C8(0x3C);
            func_800894C8(0x3F);
        }
        actor->motion.vz = 0;
        actor->motion.vy = 0;
        actor->motion.vx = 0;
        D_8009BD04 = 0;
        break;
    case 3:
        switch (func_80090E14(actor)) {
        case 1:
            if (D_8009D7D8->pad == 2) {
                actor->state = 0x20;
                actor->unk68 = 0x30000;
                VEHICLE_SPOT(actor->position.vy);
                func_80089160(2, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
            } else {
                D_8009D554 = 0;
                D_8009D7CC = 0;
            }
            break;
        case 4:
            hit = func_8008E0F0(&actor->position, (s32)&actor->motion, 0x68000);
            if (hit != -1) {
                actor->state = 0x14;
                actor->unk78 = hit;
                actor->unk68 = func_80093978(actor->position.vx, actor->position.vz);
                VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
                VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;
                VEHICLE_SCRATCH->rotation.vy = actor->heading;
                VEHICLE_SPOT(actor->position.vy);
                func_80089160(2, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
                func_800894C8(1);
                D_8009D7D8 = (PathTable *)-1;
                D_8009BD24 = -1;
                D_8009CE68 = -1;
            }
            break;
        default:
            VEHICLE_TURN(actor->heading, 0x80);
            hit = func_80095414(&actor->position, &actor->motion, SCRATCH_HIT, actor->turn << 12, D_8009BE10);
            if (hit == 0) {
                actor->motion = *SCRATCH_HIT;
                hit = func_80095414(&actor->position, &actor->motion, SCRATCH_HIT, actor->turn << 12,
                                    D_8009BE10);
                if (hit == 0) {
                    actor->motion.vz = 0;
                    actor->motion.vx = 0;
                }
            }
            if (hit == 1) {
                SCRATCH_HIT->vy = func_80093978(SCRATCH_HIT->vx, SCRATCH_HIT->vz) + 0x18000;
                func_8008C040(SCRATCH_HIT, 0x40, 0x20, &D_8009D738, &D_8009BD60);
                if (D_8009D738 != 2) {
                    actor->position = *SCRATCH_HIT;
                } else {
                    actor->unk60 = 0;
                }
            }
            D_8009D55C.target = actor->position;
            D_8009D52C = actor->heading;
            func_80094238(&actor->position, 2);
            VEHICLE_ORIENT();
            if (actor->motion.vx | actor->motion.vz) {
                VEHICLE_SPOT(actor->position.vy);
                func_80089160(1, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
            } else {
                func_800894C8(1);
            }
            actor->motion.vz = 0;
            actor->motion.vy = 0;
            actor->motion.vx = 0;
            if (func_80093F18(&actor->position) != 3) {
                func_800894C8(1);
                func_80089160(3, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
                actor->state = 4;
                actor->wait = 0x28;
            }
            break;
        }
        break;
    case 4:
        if (func_80090E14(actor) == 1) {
            if (D_8009D7D8->pad == 2) {
                actor->state = 0x20;
                actor->unk68 = 0x30000;
                VEHICLE_SPOT(actor->position.vy);
                func_80089160(2, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
            } else {
                D_8009D554 = 0;
                D_8009D7CC = 0;
            }
            break;
        }
        VEHICLE_TURN(actor->heading, 0x80);
        hit = func_80095414(&actor->position, &actor->motion, SCRATCH_HIT, actor->turn << 12, D_8009BE10);
        if (hit == 0) {
            actor->motion = *SCRATCH_HIT;
            hit = func_80095414(&actor->position, &actor->motion, SCRATCH_HIT, actor->turn << 12,
                                D_8009BE10);
            if (hit == 0) {
                actor->motion.vz = 0;
                actor->motion.vx = 0;
            }
        }
        if (hit == 1) {
            SCRATCH_HIT->vy = func_80093A5C(SCRATCH_HIT->vx, SCRATCH_HIT->vz) + 0x18000;
            func_8008C040(SCRATCH_HIT, 0x40, 0x20, &D_8009D738, &D_8009BD60);
            if (D_8009D738 != 2) {
                actor->position = *SCRATCH_HIT;
                if (actor->motion.vx | actor->motion.vz) {
                    func_8007528C();
                }
            } else {
                actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) + 0x18000;
                actor->unk60 = 0;
            }
        } else {
            actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) + 0x18000;
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        func_80094238(&actor->position, 2);
        func_8008E078();
        VEHICLE_ORIENT();
        if (--actor->wait > 0) {
            VEHICLE_SPOT(actor->position.vy);
            func_80089160(3, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
        } else {
            actor->wait = 0;
            func_800894C8(3);
        }
        if (actor->motion.vx | actor->motion.vz) {
            VEHICLE_SPOT(actor->position.vy);
            func_80089160(4, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
        } else {
            func_800894C8(4);
            actor->wait = 0;
        }
        actor->motion.vz = 0;
        actor->motion.vy = 0;
        actor->motion.vx = 0;
        if (func_80093F18(SCRATCH_HIT) == 3) {
            func_800894C8(3);
            func_800894C8(4);
            actor->state = 3;
        }
        D_8009BD04 = 0;
        break;
    case 8:
        actor->position.vy -= 0x8000;
        if (actor->position.vy <= -0x1E0000) {
            actor->position.vy = -0x1E0000;
            if (actor->unk4 == 0xB) {
                actor->unk64 = 0;
                actor->state = 2;
                actor->unk4 = 0;
                func_800767D4(D_8009C888, D_8009D800);
            }
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0xC:
        actor->position.vy += 0x800;
        if (actor->position.vy >= actor->unk68) {
            actor->position.vy = actor->unk68;
            actor->state = 3;
            D_8009C620[3].visible = 1;
            D_8009C620[2].visible = 1;
            D_8009C620[1].visible = 1;
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x10:
        actor->position.vy += 0x8000;
        if (actor->position.vy >= actor->unk68) {
            actor->position.vy = actor->unk68;
            actor->state = 1;
            func_800767D4(D_8009C884, D_8009D3D0);
            func_80097770(8, 0xA);
            func_80097770(9, 0xA);
            func_80097770(0xA, 0xA);
            func_80097770(4, 3);
            func_80097770(1, 3);
            D_8006F8E5 = 1;
            if (D_8006F368[1] != 0xFF) {
                func_80097770(5, 3);
                func_80097770(2, 3);
                D_8006F8E6 = 1;
            }
            if (D_8006F368[2] != 0xFF) {
                func_80097770(6, 3);
                func_80097770(3, 3);
                D_8006F8E7 = 1;
            }
            actor->motion.vz = 0;
            actor->motion.vy = 0;
            actor->motion.vx = 0;
            actor->unk74 = 0;
            D_8006EE68 &= 0x3FFF;
            func_800894C8(0);
            func_800894C8(1);
            func_80075228();
        } else if (actor->position.vy > -0x200000 && actor->unk7C != 0) {
            actor->unk7C = 0;
            VEHICLE_SCRATCH->rotation.vx = actor->position.vx >> 12;
            VEHICLE_SCRATCH->rotation.vy = actor->unk68 >> 12;
            VEHICLE_SCRATCH->rotation.vz = actor->position.vz >> 12;
            func_80089160(func_80093F18(&actor->position) == 3 ? 0x3E : 0x3D, &VEHICLE_SCRATCH->rotation,
                          NULL);
        }
        D_8009D55C.target.vy = actor->position.vy;
        break;
    case 0x14:
        actor->position.vy -= 0x800;
        if (actor->unk68 >= actor->position.vy) {
            actor->position.vy = actor->unk68;
            actor->state = 1;
            func_80097770(8, 0xA);
            func_80097770(1, 3);
            func_80097770(4, 3);
            D_8006F8E5 = 1;
            if (D_8006F368[1] != 0xFF) {
                func_80097770(2, 3);
                func_80097770(5, 3);
                D_8006F8E6 = 1;
            }
            if (D_8006F368[2] != 0xFF) {
                func_80097770(3, 3);
                func_80097770(6, 3);
                D_8006F8E7 = 1;
            }
            actor->motion.vz = 0;
            actor->motion.vy = 0;
            actor->motion.vx = 0;
            actor->unk74 = 0;
            D_8006EE68 &= 0x3FFF;
            D_8009C620[3].visible = 0;
            D_8009C620[2].visible = 0;
            D_8009C620[1].visible = 0;
            func_80075228();
        }
        D_8009D55C.target.vy = actor->position.vy;
        break;
    case 0x20:
        actor->position.vy += 0x2000;
        if (actor->position.vy >= actor->unk68) {
            actor->position.vy = actor->unk68;
            func_800894C8(2);
            D_8009C620[3].visible = 1;
            D_8009C620[2].visible = 1;
            D_8009C620[1].visible = 1;
            D_8009C620[0].visible = 1;
            actor->state++;
            VEHICLE_SCRATCH->target.vx = 0x4B40000;
            VEHICLE_SCRATCH->target.vz = 0x1C00000;
            VEHICLE_SCRATCH->target.vy = actor->unk68;
            func_800941C4(&actor->position, &VEHICLE_SCRATCH->target, &actor->motion, &actor->heading);
            actor->u.step = VEHICLE_SCRATCH->target.vx >> 12;
            actor->unk54 = VEHICLE_SCRATCH->target.vz >> 12;
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x21:
        if (func_8008BEC8(actor) == 3) {
            actor->state++;
        }
        actor->position.vy = actor->unk68;
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x22:
        actor->state = 0x40;
        D_8009D554 = 0;
        D_8009D7CC = 0;
        break;
    case 0x24:
        actor->position.vy = 0x30000;
        actor->state++;
        VEHICLE_SCRATCH->target.vx = 0x4937000;
        VEHICLE_SCRATCH->target.vz = 0x21E2000;
        VEHICLE_SCRATCH->target.vy = 0x30000;
        func_800941C4(&actor->position, &VEHICLE_SCRATCH->target, &actor->motion, &actor->heading);
        actor->u.step = VEHICLE_SCRATCH->target.vx >> 12;
        actor->unk54 = VEHICLE_SCRATCH->target.vz >> 12;
        D_8009D55C.target = actor->position;
        D_8009C620[3].visible = 1;
        D_8009C620[2].visible = 1;
        D_8009C620[1].visible = 1;
        D_8009C620[0].visible = 1;
        VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
        D_8009D52C = actor->heading;
        VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;
        VEHICLE_SCRATCH->rotation.vy = actor->heading;
        func_8004A92C(&VEHICLE_SCRATCH->rotation, &D_8009C620[0].matrix);
        func_8004A92C(&VEHICLE_SCRATCH->rotation, &D_8009C620[1].matrix);
        break;
    case 0x25:
        if (func_8008BEC8(actor) == 3) {
            actor->position.vy = 0x30000;
            actor->state++;
            actor->unk68 = func_80093978(actor->position.vx, actor->position.vz) + 0x18000;
            VEHICLE_SPOT(actor->unk68);
            func_80089160(2, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
            D_8009C620[0].visible = 0;
        }
        actor->position.vy = 0x30000;
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x26:
        actor->position.vy -= 0x2000;
        if (actor->unk68 >= actor->position.vy) {
            actor->position.vy = actor->unk68;
            func_800894C8(2);
            actor->state = 3;
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x28:
        actor->state++;
        actor->position.vy = func_80093978(actor->position.vx, actor->position.vz) + 0x18000;
        VEHICLE_SCRATCH->target.vx = D_8009B1AC[actor->unk7C].vx << 12;
        VEHICLE_SCRATCH->target.vz = D_8009B1AC[actor->unk7C].vz << 12;
        func_800941C4(&actor->position, &VEHICLE_SCRATCH->target, &actor->motion, &actor->heading);
        actor->u.step = VEHICLE_SCRATCH->target.vx >> 12;
        actor->unk78 = actor->heading;
        if (++actor->unk7C == 2) {
            func_80097770(9, 0xF);
        }
        actor->unk54 = VEHICLE_SCRATCH->target.vz >> 12;
        if (actor->unk7C == 4) {
            actor->wait = 0x40;
            actor->state++;
            func_80097770(0, 0xD);
            D_8009CCA4 = 2;
            D_8009D3CC = actor->unk7C;
        }
    case 0x29:
        if (func_8008BEC8(actor) == 3) {
            actor->state--;
        }
    circle:
        VEHICLE_TURN(actor->unk78, 0x10);
        VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
        VEHICLE_SCRATCH->rotation.vy = actor->heading;
        VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;
        VEHICLE_SPOT(actor->position.vy);
        func_80089160(1, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
        VEHICLE_ORIENT();
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x2A:
        if (--actor->wait < 0) {
            D_8006F94E = 0x50;
            D_8006F954[0] = 1;
            D_8009D554 = 0;
            D_8009D7CC = 0;
            D_8009BBC4 = 1;
            D_8006F950 = D_8009BD38.vy;
        }
        goto circle;
    case 0x30:
        actor->state++;
        actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) + 0x18000;
        VEHICLE_SCRATCH->target.vx = D_8009B1D4[actor->unk7C].vx << 12;
        VEHICLE_SCRATCH->target.vz = D_8009B1D4[actor->unk7C].vz << 12;
        func_800941C4(&actor->position, &VEHICLE_SCRATCH->target, &actor->motion, &actor->heading);
        actor->u.step = VEHICLE_SCRATCH->target.vx >> 12;
        actor->unk78 = actor->heading;
        if (++actor->unk7C == 2) {
            func_80097770(9, 0x10);
        }
        actor->unk54 = VEHICLE_SCRATCH->target.vz >> 12;
        if (actor->unk7C == 7) {
            actor->wait = 0x40;
            actor->state++;
            func_80097770(0, 0xD);
            D_8009CCA4 = 2;
            D_8009D3CC = 4;
        }
    case 0x31:
    walk:
        if (func_8008BEC8(actor) == 3) {
            actor->state--;
        }
        VEHICLE_TURN(actor->unk78, 0x10);
        actor->position.vy = func_80093A5C(actor->position.vx, actor->position.vz) + 0x18000;
        VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
        VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;
        VEHICLE_SCRATCH->rotation.vy = actor->heading;
        VEHICLE_SPOT(actor->position.vy);
        func_80089160(4, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
        VEHICLE_ORIENT();
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x32:
        if (--actor->wait < 0) {
            D_8006F94E = 0x120;
            D_8006F954[0] = 6;
            D_8009D554 = 0;
            D_8009D7CC = 0;
            D_8009BBC4 = 1;
            D_8006F950 = D_8009BD38.vy;
        }
        goto walk;
    case 0x34:
        actor->state++;
        func_80097770(0xA, 0x11);
        actor->position.vy = -0x100000;
        actor->heading = 0x200;
        actor->motion.vx = func_8003F8B0(0x200);
        actor->motion.vz = -func_8003F8CC(actor->heading);
        actor->wait = 0x5A;
        actor->unk78 = 0;
        VEHICLE_ORIENT();
    case 0x35:
        if (--actor->wait < 0) {
            actor->wait = 0xB4;
            actor->state++;
            func_80097770(9, 0x11);
        }
    depart:
        actor->position.vx += actor->motion.vx * 12;
        actor->position.vz += actor->motion.vz * 12;
        func_80093354(&actor->position);
        actor->position.vy += 0x1000;
        if ((u32)(actor->position.vy + 0xFFFF) < 0x1FFFF) {
            VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
            VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;
            VEHICLE_SCRATCH->rotation.vy = actor->heading;
            VEHICLE_SPOT(actor->position.vy);
            func_80089160(0x3F, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
        } else {
            func_800894C8(0x3F);
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x36:
        if (--actor->wait < 0) {
            actor->wait = 0x78;
            actor->state++;
            func_80039E60((D_8006259C->id << 16) | 0x38);
        }
        goto depart;
    case 0x37:
        if (--actor->wait < 0) {
            actor->wait = 0x5A;
            actor->state++;
            func_800894C8(0x2B);
        }
        actor->position.vx += actor->motion.vx * 8;
        actor->position.vz += actor->motion.vz * 8;
        func_80093354(&actor->position);
        actor->position.vy += 0x1000;
        if (actor->state == 0x38) {
            func_800894C8(0x2B);
        } else {
            VEHICLE_SCRATCH->rotation.vx = -(actor->unk70 >> 12);
            VEHICLE_SCRATCH->rotation.vz = D_8009BD38.vz;
            VEHICLE_SCRATCH->rotation.vy = actor->heading;
            VEHICLE_SCRATCH->spot.vy = 0;
            VEHICLE_SCRATCH->spot.vx = actor->position.vx >> 12;
            VEHICLE_SCRATCH->spot.vz = actor->position.vz >> 12;
            func_80089160(0x2B, &VEHICLE_SCRATCH->spot, &VEHICLE_SCRATCH->rotation);
        }
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x38:
        if (--actor->wait < 0) {
            actor->wait = 0x80;
            actor->state++;
            func_80097770(0, 0xD);
            D_8009CCA4 = 2;
            D_8009D3CC = 4;
        }
    glide:
        actor->position.vx += actor->motion.vx * 8;
        actor->position.vz += actor->motion.vz * 8;
        D_8009D55C.target = actor->position;
        D_8009D52C = actor->heading;
        break;
    case 0x39:
        if (--actor->wait < 0) {
            D_8006F94E = 0x1F0;
            D_8009D554 = 0;
            D_8009D7CC = 0;
            D_8006F954[0] = 0;
            D_8009BBC4 = 1;
            D_8006F950 = D_8009BD38.vy;
        }
        goto glide;
    }
    D_8009C620[1].position.vx = D_8009C620[0].position.vx = actor->position.vx >> 12;
    D_8009C620[1].position.vy = D_8009C620[0].position.vy = actor->position.vy >> 12;
    D_8009C620[1].position.vz = D_8009C620[0].position.vz = actor->position.vz >> 12;
    func_8008E034(&actor->position);
    D_8006EE66 = actor->heading;
    switch (actor->state) {
    case 2:
    case 8:
    case 0x10:
        func_80074794(2, &actor->position);
        break;
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_8008E190", func_8008E76C);
#endif

/* Restore the actor from scene objects 2 and 3 after linking them to 0. */
s32 func_800906E0(s32 index) {
    WorldmapActor *actor;

    func_800848B4(0, 2);
    func_800848B4(0, 3);
    actor = &D_8009BE24[index];
    actor->state = 0;
    actor->position = D_8009C620[2].position;
    actor->motion = D_8009C620[3].position;
    actor->u.step = 0;
    actor->unk54 = 0x40;
    switch (D_8009BE10) {
    case 4:
    case 5:
    case 6:
    case 7:
        actor->state = 3;
        actor->unk5C = 0x80;
        actor->unk58 = 0x80;
        break;
    }
    return 1;
}

/* Link scene objects 2 and 3 to object 0. */
s32 func_800907C4(void) {
    func_800848B4(0, 2);
    func_800848B4(0, 3);
    return 1;
}

/* Spin the vehicle's two rotors (scene objects 2 and 3): command 9 spins
 * up, 10 spins down; the second rotor follows the first. */
s32 func_800907F4(s32 index) {
    ActorScratch *scratch;
    WorldmapActor *actor;
    SceneObject *rotor;
    SceneObject *tail;

    scratch = (ActorScratch *)0x1F800000;
    actor = &D_8009BE24[index];
    rotor = &D_8009C620[2];
    tail = &D_8009C620[3];
    if (actor->unk4 == 9) {
        actor->unk4 = 0;
        actor->state = 1;
    } else if (actor->unk4 == 10) {
        actor->unk4 = 0;
        actor->state = 2;
    }
    switch (actor->state) {
    case 0:
        actor->unk58 = 0;
        actor->unk5C = 0;
        break;
    case 1:
        actor->unk58 += 4;
        if (actor->unk58 > 0x10) {
            actor->unk5C += 4;
        }
        if (actor->unk58 >= 0x80) {
            actor->unk58 = 0x80;
        }
        if (actor->unk5C >= 0x80) {
            actor->unk5C = 0x80;
        }
        if ((actor->unk58 >= 0x80) & (actor->unk5C >= 0x80)) {
            actor->state = 3;
        }
        break;
    case 2:
        actor->unk58 -= 4;
        if (actor->unk58 < 0x70) {
            actor->unk5C -= 4;
        }
        if (actor->unk58 < 0) {
            actor->unk58 = 0;
        }
        if (actor->unk5C < 0) {
            actor->unk5C = 0;
        }
        if ((actor->unk58 == 0) & (actor->unk5C == 0)) {
            actor->state = 0;
        }
        break;
    }
    actor->u.step += actor->unk58;
    actor->unk54 -= actor->unk5C;
    scratch->angle.vx = 0;
    scratch->position.vx = 0;
    scratch->position.vy = actor->u.step;
    scratch->angle.vy = actor->unk54;
    scratch->position.vz = rotor->angle.vz;
    scratch->angle.vz = tail->angle.vz;
    func_8004ABBC(&scratch->position, &rotor->matrix);
    func_8004ABBC(&scratch->angle, &tail->matrix);
    return 1;
}

/* Track the two-button combination and latch its press edge. */
void func_80090A18(void) {
    if ((D_8009CD4C & 1) && (D_8009CD4C & 2)) {
        D_8009CEC0 = 1;
    } else {
        D_8009CEC0 = 0;
    }
    D_8009BD34 = (D_8009CEC0 ^ D_8009C7E8) & D_8009CEC0;
    D_8009C7E8 = D_8009CEC0;
}
