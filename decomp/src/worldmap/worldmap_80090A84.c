#include "worldmap.h"

void func_80093354(VECTOR *position);

/* Walking input: steer by the d-pad relative to the camera; 3 on a menu request, 1 when a path or its entrance is selected, else 0. */
s32 func_80090A84(WorldmapActor *actor) {
    switch (D_8009CD4C >> 12) {
    case 1:
        actor->heading = D_8009BD38.vy;
        break;
    case 2:
        actor->heading = (D_8009BD38.vy + 0x400) & 0xFFF;
        break;
    case 3:
        actor->heading = (D_8009BD38.vy + 0x200) & 0xFFF;
        break;
    case 4:
        actor->heading = (D_8009BD38.vy + 0x800) & 0xFFF;
        break;
    case 6:
        actor->heading = (D_8009BD38.vy + 0x600) & 0xFFF;
        break;
    case 8:
        actor->heading = (D_8009BD38.vy - 0x400) & 0xFFF;
        break;
    case 9:
        actor->heading = (D_8009BD38.vy - 0x200) & 0xFFF;
        break;
    case 12:
        actor->heading = (D_8009BD38.vy - 0x600) & 0xFFF;
        break;
    }
    if (D_8009CD4C & 0xF000) {
        actor->motion.vx = func_8003F8B0(actor->heading);
        actor->motion.vz = -func_8003F8CC(actor->heading);
    }
    if (D_8009BD10 & 0x20) {
        if (D_8009D738 != 0) {
            return 3;
        }
        if (D_8009BD24 != -1) {
            return 1;
        }
    } else if (D_8009BD24 != -1 && ((PathRegion *)D_8009D7D8)->kind == 1) {
        return 1;
    }
    if ((D_8009BD10 & 0x10) && D_8009CE68 == -1 && D_8009BD24 == D_8009CE68) {
        D_8009D804 = 1;
    }
    func_80090A18();
    return 0;
}

/* Vehicle input: steer by the d-pad relative to the camera; 3 on a menu request, 1 when leaving at a path, else 0. */
s32 func_80090C68(WorldmapActor *actor) {
    switch (D_8009CD4C >> 12) {
    case 1:
        actor->heading = D_8009BD38.vy;
        break;
    case 2:
        actor->heading = (D_8009BD38.vy + 0x400) & 0xFFF;
        break;
    case 3:
        actor->heading = (D_8009BD38.vy + 0x200) & 0xFFF;
        break;
    case 4:
        actor->heading = (D_8009BD38.vy + 0x800) & 0xFFF;
        break;
    case 6:
        actor->heading = (D_8009BD38.vy + 0x600) & 0xFFF;
        break;
    case 8:
        actor->heading = (D_8009BD38.vy - 0x400) & 0xFFF;
        break;
    case 9:
        actor->heading = (D_8009BD38.vy - 0x200) & 0xFFF;
        break;
    case 12:
        actor->heading = (D_8009BD38.vy - 0x600) & 0xFFF;
        break;
    }
    if (D_8009CD4C & 0xF000) {
        actor->motion.vx = func_8003F8B0(actor->heading);
        actor->motion.vz = -func_8003F8CC(actor->heading);
    }
    if (D_8009BD10 & 0x20) {
        if (D_8009D738 != 0) {
            return 3;
        }
        if (D_8009BD24 != -1) {
            return 1;
        }
    }
    if ((D_8009BD10 & 0x10) && D_8009CE68 == -1 && D_8009BD24 == D_8009CE68) {
        D_8009D804 = 1;
    }
    func_80090A18();
    return 0;
}

/* Flying input: steer by the d-pad relative to the camera; 1 when landing at a path, 4 on the take-off button, else 0. */
s32 func_80090E14(WorldmapActor *actor) {
    switch (D_8009CD4C >> 12) {
    case 1:
        actor->heading = D_8009BD38.vy;
        break;
    case 2:
        actor->heading = (D_8009BD38.vy + 0x400) & 0xFFF;
        break;
    case 3:
        actor->heading = (D_8009BD38.vy + 0x200) & 0xFFF;
        break;
    case 4:
        actor->heading = (D_8009BD38.vy + 0x800) & 0xFFF;
        break;
    case 6:
        actor->heading = (D_8009BD38.vy + 0x600) & 0xFFF;
        break;
    case 8:
        actor->heading = (D_8009BD38.vy - 0x400) & 0xFFF;
        break;
    case 9:
        actor->heading = (D_8009BD38.vy - 0x200) & 0xFFF;
        break;
    case 12:
        actor->heading = (D_8009BD38.vy - 0x600) & 0xFFF;
        break;
    }
    if (D_8009CD4C & 0xF000) {
        actor->motion.vx = func_8003F8B0(actor->heading);
        actor->motion.vz = -func_8003F8CC(actor->heading);
    }
    if (D_8009BD10 & 0x20) {
        if (D_8009BD24 != -1) {
            return 1;
        }
    }
    if (D_8009BD10 & 0x40) {
        return 4;
    }
    if ((D_8009BD10 & 0x10) && D_8009CE68 == -1 && D_8009BD24 == D_8009CE68) {
        D_8009D804 = 1;
    }
    return 0;
}

/* Free flight input: bank and turn with the d-pad and shoulder buttons,
 * pitch with triangle/cross and climb with R1 (R2 descends); returns 1 when
 * landing at a path, 4 on take-off when level, else 0. */
s32 func_80090FB4(WorldmapActor *actor) {
    s32 turn;
    s32 bank_step;
    s32 turn_step;
    s32 delta;
    s32 bankAmount;
    s32 turnAmount;
    s32 bank;
    s32 yaw;

    switch (actor->unk64) {
    case 0:
        actor->unk64 = 1;
        actor->unk5C = 0;
        actor->unk58 = actor->heading << 12;
        break;
    case 1:
        turn = 0;
        bank = 0;
        bank_step = 8;
        turn_step = 0x1200;
        if (D_8009CD4C & 0x8000) {
            bank = -0x60;
            turn = -0x10000;
        }
        if (D_8009CD4C & 0x2000) {
            bank += 0x60;
            turn += 0x10000;
        }
        if (D_8009CD4C & 4) {
            bank -= 0x60;
            turn -= 0x10000;
        }
        if (D_8009CD4C & 8) {
            bank += 0x60;
            turn += 0x10000;
        }
        if (D_8009CD4C & 0xA00C) {
            turn_step = 0x1000;
            delta = D_8009BD38.vz - bank;
            delta = delta >= 0 ? delta : -delta;
            bank_step = delta >> 4;
        }
        if (D_8009BD38.vz != bank) {
            delta = D_8009BD38.vz - bank;
            bankAmount = delta >= 0 ? delta : -delta;
            if (bankAmount < bank_step) {
                bank_step = bankAmount;
            }
            if (D_8009BD38.vz < bank) {
                D_8009BD38.vz += bank_step;
            } else {
                D_8009BD38.vz -= bank_step;
            }
        }
        if (actor->unk5C != turn) {
            delta = actor->unk5C - turn;
            turnAmount = delta >= 0 ? delta : -delta;
            if (turnAmount < turn_step) {
                turn_step = turnAmount;
            }
            if (actor->unk5C < turn) {
                actor->unk5C += turn_step;
            } else {
                actor->unk5C -= turn_step;
            }
        }
        break;
    }
    actor->unk58 += actor->unk5C;
    yaw = (actor->unk58 >> 12) & 0xFFF;
    actor->heading = yaw;
    D_8009BD38.vy = yaw;
    switch (actor->unk6C) {
    case 0:
        actor->unk6C = 1;
        actor->unk70 = 0;
        break;
    case 1:
        if (actor->unk70 != 0) {
            if (actor->unk70 < 0) {
                actor->unk70 += 0x2000;
            } else {
                actor->unk70 -= 0x2000;
            }
        }
        if (D_8009CD4C & 0x4000) {
            actor->unk6C = 2;
        }
        if (D_8009CD4C & 0x1000) {
            actor->unk6C = 3;
        }
        break;
    case 2:
        if (D_8009CD4C & 0x4000) {
            actor->unk70 -= 0x4000;
            if (actor->unk70 < -0x80000) {
                actor->unk70 = -0x80000;
            }
        } else {
            actor->unk6C = 1;
        }
        break;
    case 3:
        if (D_8009CD4C & 0x1000) {
            actor->unk70 += 0x4000;
            if (actor->unk70 > 0x80000) {
                actor->unk70 = 0x80000;
            }
        } else {
            actor->unk6C = 1;
        }
        break;
    }
    if (D_8009CD4C & 0x80) {
        if (D_8009CD4C & 2) {
            actor->unk60 -= 0x1000;
        } else {
            actor->unk60 += 0x1000;
        }
    }
    if (D_8009CD4C & 0x80) {
        if (actor->unk60 < -actor->turn << 12) {
            actor->unk60 = -actor->turn << 12;
        }
        if (actor->unk60 > actor->turn << 12) {
            actor->unk60 = actor->turn << 12;
        }
    } else if (actor->unk60 != 0) {
        if (actor->unk60 < 0) {
            actor->unk60 += 0x1000;
        } else {
            actor->unk60 -= 0x1000;
        }
    }
    actor->motion.vx = func_8003F8B0(D_8009BD38.vy);
    actor->motion.vy = func_8003F8B0(actor->unk70 >> 12);
    actor->motion.vz = -func_8003F8CC(D_8009BD38.vy);
    VectorNormal(&actor->motion, &actor->motion);
    if (D_8009BD10 & 0x20) {
        if (D_8009BD24 != -1) {
            return 1;
        }
    }
    if (D_8009BD10 & 0x40) {
        if ((actor->unk60 == 0) & (D_8009BD38.vz == 0)) {
            return 4;
        }
    }
    if ((D_8009BD10 & 0x10) && D_8009CE68 == -1 && D_8009BD24 == D_8009CE68) {
        D_8009D804 = 1;
    }
    return 0;
}

/* Restore the actor from the saved camera state; vehicles get state 3. */
s32 func_80091430(s32 index) {
    WorldmapActor *actor;
    Camera *saved;
    s16 *yaw;

    actor = &D_8009BE24[index];
    saved = &D_8009D55C;
    actor->position = saved->target;
    yaw = &D_8009BD38.vy;
    D_8009BD38.vz = 0;
    D_8009BD38.vy = D_8009D52C;
    actor->u.step = (s16)D_8009D52C;
    actor->unk58 = *yaw << 12;
    switch (D_8009BE10) {
    case 6:
    case 7:
        actor->state = 3;
        break;
    }
    return 1;
}

/* Camera yaw and target follower: turn by the shoulder buttons in 0x200
 * steps, ease towards requested yaws (commands 9, 10, 15-17), and move the
 * target towards the saved camera, scrolling the terrain origin with it. */
s32 func_800914D0(s32 index) {
    WorldmapActor *actor;
    s32 angle;
    VECTOR delta;
    VECTOR step;
    s32 yaw;
    s32 amount;
    s32 absX;
    s32 absZ;

    actor = &D_8009BE24[index];
    switch (actor->unk4) {
    case 9:
        actor->unk4 = 0;
        actor->state = 2;
        actor->u.step = (s16)D_8009D52C;
        actor->unk58 = D_8009BD38.vy << 12;
        break;
    case 10:
        actor->unk4 = 0;
        actor->state = 0;
        actor->u.step = D_8009BD38.vy;
        break;
    case 15:
        actor->state = 0x10;
        actor->unk4 = 0;
        actor->u.step = 0x800;
        break;
    case 16:
        actor->state = 0x10;
        actor->unk4 = 0;
        actor->u.step = 0xA00;
        break;
    case 17:
        actor->state = 0x10;
        actor->unk4 = 0;
        actor->u.step = 0xC00;
        break;
    }
    switch (actor->state) {
    case 0:
        switch ((D_8009CD4C >> 2) & 3) {
        case 1:
        case 3:
            actor->state = 1;
            actor->unk54 = -0x40;
            actor->unk5C = (actor->u.step - 0x200) & 0xFFF;
            break;
        case 2:
            actor->state = 1;
            actor->unk54 = 0x40;
            actor->unk5C = (actor->u.step + 0x200) & 0xFFF;
            break;
        }
    follow:
        if (D_8009BD38.vy != actor->u.step) {
            yaw = actor->u.step - D_8009BD38.vy;
            amount = yaw >= 0 ? yaw : -yaw;
            if (amount > 0xC00) {
                if (yaw < 0) {
                    yaw += 0x1000;
                } else {
                    yaw -= 0x1000;
                }
            }
            actor->unk58 = (actor->unk58 + ((yaw << 12) >> 3)) & 0xFFFFFF;
            D_8009BD38.vy = actor->unk58 >> 12;
        } else {
            actor->unk58 = D_8009BD38.vy << 12;
        }
        break;
    case 1:
        actor->u.step = (actor->u.step + actor->unk54) & 0xFFF;
        if (actor->u.step == actor->unk5C) {
            actor->state = 0;
        }
        goto follow;
    case 2:
        yaw = D_8009BD38.vy;
        yaw = actor->u.step - yaw;
        amount = yaw >= 0 ? yaw : -yaw;
        if (amount > 0x800) {
            if (yaw < 0) {
                yaw += 0x1000;
            } else {
                yaw -= 0x1000;
            }
        }
        amount = yaw >= 0 ? yaw : -yaw;
        if (amount > 0x180) {
            if (yaw < 0) {
                yaw = -0x180;
            } else {
                yaw = 0x180;
            }
        }
        actor->unk58 = (actor->unk58 + ((yaw << 12) >> 3)) & 0xFFFFFF;
        angle = D_8009BD38.vy = actor->unk58 >> 12;
        if ((angle == actor->u.step) & (actor->unk60 == 0)) {
            actor->state = 3;
            func_80097770(7, 0xB);
        }
        break;
    case 0x10:
        yaw = D_8009BD38.vy;
        yaw = actor->u.step - yaw;
        amount = yaw >= 0 ? yaw : -yaw;
        if (amount > 0x800) {
            if (yaw < 0) {
                yaw += 0x1000;
            } else {
                yaw -= 0x1000;
            }
        }
        amount = yaw >= 0 ? yaw : -yaw;
        if (amount > 0x180) {
            if (yaw < 0) {
                yaw = -0x180;
            } else {
                yaw = 0x180;
            }
        }
        actor->unk58 = (actor->unk58 + ((yaw << 12) >> 5)) & 0xFFFFFF;
        angle = D_8009BD38.vy = actor->unk58 >> 12;
        if ((angle == actor->u.step) & (actor->unk60 == 0)) {
            actor->state = 3;
        }
        break;
    case 3 ... 15:
        break;
    }
    switch (actor->state) {
    case 0:
    case 1:
    case 2:
    case 0x10:
        if ((actor->position.vx != D_8009D55C.target.vx) | (actor->position.vy != D_8009D55C.target.vy) |
            (actor->position.vz != D_8009D55C.target.vz)) {
            delta.vx = D_8009D55C.target.vx - actor->position.vx;
            delta.vy = D_8009D55C.target.vy - actor->position.vy;
            delta.vz = D_8009D55C.target.vz - actor->position.vz;
            func_80093484(&delta);
            step.vx = delta.vx >> 3;
            step.vy = delta.vy >> 3;
            step.vz = delta.vz >> 3;
            absX = ABS(step.vx);
            absX = absX < 0x40;
            absZ = ABS(step.vz);
            absZ = absZ < 0x40;
            if (absX & absZ) {
                actor->unk60 = 0;
                TERRAIN_ORIGIN.vx += delta.vx;
                TERRAIN_ORIGIN.vz += delta.vz;
                actor->position.vx = D_8009D55C.target.vx;
                actor->position.vz = D_8009D55C.target.vz;
            } else {
                actor->unk60 = 1;
                actor->position.vx += step.vx;
                actor->position.vz += step.vz;
                TERRAIN_ORIGIN.vx += step.vx;
                TERRAIN_ORIGIN.vz += step.vz;
            }
            actor->position.vy += step.vy;
        }
        break;
    case 3:
        if ((actor->position.vx != D_8009D55C.target.vx) | (actor->position.vy != D_8009D55C.target.vy) |
            (actor->position.vz != D_8009D55C.target.vz)) {
            delta.vx = D_8009D55C.target.vx - actor->position.vx;
            delta.vy = D_8009D55C.target.vy - actor->position.vy;
            delta.vz = D_8009D55C.target.vz - actor->position.vz;
            func_80093484(&delta);
            actor->position.vy += delta.vy >> 4;
            TERRAIN_ORIGIN.vx += delta.vx;
            TERRAIN_ORIGIN.vz += delta.vz;
            actor->position.vx = D_8009D55C.target.vx;
            actor->position.vz = D_8009D55C.target.vz;
        }
        break;
    }
    func_80093354(&actor->position);
    D_8009BE28.target = actor->position;
    return 1;
}

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

/* Camera actor: on commands choose the pitch tables (9 vehicle, 10 on foot)
 * or zoom out (17); ease the distance and pitch towards the pitch that keeps
 * the terrain in view, then place the camera. */
s32 func_80091C18(s32 index) {
    WorldmapActor *actor;
    s16 *pitches;
    s32 pitch;
    s32 delta;
    s32 target;
    s32 current;
    s16 result;

    result = 1;
    actor = &D_8009BE24[index];
    switch (actor->unk4) {
    case 14:
        actor->unk4 = 0;
        actor->state = 0;
        D_8009D144 = 0;
        break;
    case 17:
        D_8009D3F0 = 0x400000;
        actor->state = 3;
        actor->unk4 = 0;
        actor->unk58 = -0x100;
        break;
    case 9:
        actor->unk64 = (s32)D_8009B22C;
        actor->unk4 = 0;
        actor->unk68 = (s32)D_8009B23C;
        if (actor->u.step == 0) {
            actor->u.step = 1;
        }
        break;
    case 10:
        actor->unk64 = (s32)D_8009B224;
        actor->unk4 = 0;
        actor->unk68 = (s32)D_8009B234;
        if (actor->u.step == 0) {
            actor->u.step = 1;
        }
        break;
    }
    pitches = (s16 *)actor->unk64;
    switch (actor->state) {
    case 0:
        pitch = func_80091FF8(actor->u.step, pitches, (s16 *)actor->unk68);
        if (actor->u.step != pitch) {
            actor->state = 1;
            actor->wait = 0;
            actor->u.step = pitch;
            actor->unk54 = D_8009B214[pitch];
            actor->unk58 = pitches[actor->u.step];
            actor->unk60 = D_8009BD38.vx << 12;
        }
        func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        break;
    case 1:
        if (actor->wait >= 5) {
            pitch = func_80091FF8(actor->u.step, pitches, (s16 *)actor->unk68);
            if (actor->u.step != pitch) {
                actor->u.step = pitch;
                actor->unk54 = D_8009B214[pitch];
                actor->unk58 = pitches[actor->u.step];
            }
            actor->wait = 0;
        }
        target = actor->unk54;
        current = D_8009D3F0;
        if (target != current) {
            delta = target - current;
            if (delta > 0) {
                if (delta > 0x40000) {
                    delta = 0x40000;
                }
                delta >>= 3;
                if (delta < 0x40) {
                    D_8009D3F0 = target;
                } else {
                    D_8009D3F0 = current + delta;
                }
            } else {
                D_8009D3F0 = current - 0x2000;
            }
        }
        if (actor->unk58 != D_8009BD38.vx) {
            delta = (actor->unk58 - D_8009BD38.vx) << 12;
            if (delta < 0) {
                actor->unk60 += delta >> 5;
            } else if (actor->u.step == 0) {
                actor->unk60 += 0xF32;
            } else {
                actor->unk60 += 0x799;
            }
            D_8009BD38.vx = actor->unk60 >> 12;
        }
        if ((actor->unk54 == D_8009D3F0) & (actor->unk58 == D_8009BD38.vx)) {
            actor->state = 0;
        }
        func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        break;
    case 2:
        VIEW.at.vx = 0;
        VIEW.at.vz = 0;
        VIEW.at.vy = D_8009BE28.target.vy >> 12;
        break;
    case 3:
        if (actor->unk58 != D_8009BD38.vx) {
            D_8009BD38.vx += 2;
        }
        func_80096F18(D_8009BD40, &D_8009BE28, D_8009D3F0, &D_8009BD38);
        break;
    }
    actor->wait++;
    return result;
}

/* Pick the camera pitch (0-2) whose view stays above the terrain: for each
 * candidate place the camera and find the lowest of the 6x6 cells around
 * the viewed spot; keep the current one when it is within 0x40 of fitting. */
s32 func_80091FF8(s32 current, s16 *pitches, s16 *heights) {
    ActorScratch *scratch;
    s32 *distance;
    s32 i;
    s32 base;
    s32 left;
    s32 lowest;
    s32 floor;
    s32 row;
    s32 column;
    s32 value;
    s32 offsetZ;
    s32 gap;

    scratch = (ActorScratch *)0x1F800000;
    distance = D_8009B214;
    scratch->angle.vy = D_8009BD38.vy;
    base = D_8009D55C.target.vy >> 12;
    scratch->angle.vz = D_8009BD38.vz;
    for (i = 0; i < 3; i++, pitches++, distance++, heights++) {
        scratch->angle.vx = *pitches;
        func_80096F18(D_8009BD40, &D_8009BE28, *distance - ((D_8009BCDC / 2) << 12), &scratch->angle);
        left = D_8009BE28.target.vx + ((VIEW.eye.vx - 0x180) << 12);
        offsetZ = (VIEW.eye.vz + 0x180) << 12;
        value = D_8009BE28.target.vz - offsetZ;
        left &= 0xFFF80000;
        value &= 0xFFF80000;
        scratch->work.vz = value;
        lowest = 0;
        for (row = 0; row < 6; row++) {
            scratch->work.vx = left;
            for (column = 0; column < 6; column++) {
                func_80093354(&scratch->work);
                value = *(s8 *)func_80093660(scratch->work.vx, scratch->work.vz);
                scratch->work.vy = value;
                if (value < lowest) {
                    lowest = value;
                }
                scratch->work.vx += 0x80000;
            }
            scratch->work.vz += 0x80000;
        }
        floor = lowest * 8;
        if (*heights + base + 0x50 < floor) {
            break;
        }
    }
    if (i < current) {
        floor -= 0x50;
        gap = ((s16 *)D_8009B234)[current] + base - floor;
        if (gap < 0) {
            gap = -gap;
        }
        if (gap < 0x41) {
            i = current;
        }
    }
    return i;
}

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

/* Ease the movement speed to 0x78 (command 9) or 0x8C (command 10); ends the step when settled. */
s32 func_800922AC(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    if (actor->unk4 == 9) {
        actor->unk4 = 0;
        actor->state = 1;
    } else if (actor->unk4 == 10) {
        actor->unk4 = 0;
        actor->state = 2;
    }
    switch (actor->state) {
    case 0:
        result = 3;
        break;
    case 1:
        actor->u.step -= 0x1000;
        D_8009BE0C = actor->u.step >> 12;
        if (D_8009BE0C < 0x78) {
            D_8009BE0C = 0x78;
            actor->u.step = 0x78000;
            actor->state = 0;
        }
        break;
    case 2:
        actor->u.step += 0x1000;
        D_8009BE0C = actor->u.step >> 12;
        if (D_8009BE0C >= 0x8C) {
            D_8009BE0C = 0x8C;
            actor->u.step = 0x8C000;
            actor->state = 0;
        }
        break;
    }
    return result;
}

/* Set up the full-screen fade quad (both display copies) at brightness 0xF8. */
s32 func_800923A8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    actor->u.step = 0xF8;
    SetDrawTPage(&D_8009D310, 1, 0, GetTPage(0, D_8009CCA4, 0x380, 0x100));
    setlen(&D_8009CE6C[0], 8);
    D_8009CE6C[0].code = 0x38;
    D_8009CE6C[0].r0 = actor->u.step;
    D_8009CE6C[0].g0 = actor->u.step;
    D_8009CE6C[0].b0 = actor->u.step;
    D_8009CE6C[0].r1 = actor->u.step;
    D_8009CE6C[0].g1 = actor->u.step;
    D_8009CE6C[0].b1 = actor->u.step;
    D_8009CE6C[0].r2 = actor->u.step;
    D_8009CE6C[0].g2 = actor->u.step;
    D_8009CE6C[0].b2 = actor->u.step;
    D_8009CE6C[0].r3 = actor->u.step;
    D_8009CE6C[0].g3 = actor->u.step;
    D_8009CE6C[0].b3 = actor->u.step;
    SetSemiTrans(&D_8009CE6C[0], 1);
    D_8009CE6C[1] = D_8009CE6C[0];
    D_8009CE6C[0].x0 = 0;
    D_8009CE6C[0].y0 = 0;
    D_8009CE6C[0].x1 = 0x140;
    D_8009CE6C[0].y1 = 0;
    D_8009CE6C[0].x2 = 0;
    D_8009CE6C[0].y2 = 0xD8;
    D_8009CE6C[0].x3 = 0x140;
    D_8009CE6C[0].y3 = 0xD8;
    D_8009CE6C[1].x0 = 0;
    D_8009CE6C[1].y0 = 0;
    D_8009CE6C[1].x1 = 0x140;
    D_8009CE6C[1].y1 = 0;
    D_8009CE6C[1].x2 = 0;
    D_8009CE6C[1].y2 = 0xD8;
    D_8009CE6C[1].x3 = 0x140;
    D_8009CE6C[1].y3 = 0xD8;
    return 1;
}

/* Screen fade actor: command 12 fades in (to clear), 13 fades out (to
 * black), stepping the fade quad's brightness by D_8009D3CC; returns 3 when
 * the fade-in has finished. */
s32 func_800925A0(s32 index) {
    WorldmapActor *actor;
    s32 result;

    actor = &D_8009BE24[index];
    result = 1;
    switch (actor->unk4) {
    case 13:
        actor->unk4 = 0;
        actor->state = 1;
        D_8009C178 = 1;
        SetDrawTPage(&D_8009D310, 1, 0, GetTPage(0, D_8009CCA4, 0x380, 0x100));
        break;
    case 12:
        actor->unk4 = 0;
        actor->state = 0;
        D_8009C178 = 1;
        SetDrawTPage(&D_8009D310, 1, 0, GetTPage(0, D_8009CCA4, 0x380, 0x100));
        break;
    }
    switch (actor->state) {
    case 0:
        setRGB0(&D_8009CE6C[D_8009D7F0], actor->u.step, actor->u.step, actor->u.step);
        setRGB1(&D_8009CE6C[D_8009D7F0], actor->u.step, actor->u.step, actor->u.step);
        setRGB2(&D_8009CE6C[D_8009D7F0], actor->u.step, actor->u.step, actor->u.step);
        setRGB3(&D_8009CE6C[D_8009D7F0], actor->u.step, actor->u.step, actor->u.step);
        addPrim(D_8009BE3C->ot, &D_8009CE6C[D_8009D7F0]);
        addPrim(D_8009BE3C->ot, &D_8009D310);
        actor->u.step -= D_8009D3CC;
        if (actor->u.step < 0) {
            result = 3;
            actor->u.step = 0;
            D_8009C178 = 0;
        }
        break;
    case 1:
        setRGB0(&D_8009CE6C[D_8009D7F0], actor->u.step, actor->u.step, actor->u.step);
        setRGB1(&D_8009CE6C[D_8009D7F0], actor->u.step, actor->u.step, actor->u.step);
        setRGB2(&D_8009CE6C[D_8009D7F0], actor->u.step, actor->u.step, actor->u.step);
        setRGB3(&D_8009CE6C[D_8009D7F0], actor->u.step, actor->u.step, actor->u.step);
        addPrim(D_8009BE3C->ot, &D_8009CE6C[D_8009D7F0]);
        addPrim(D_8009BE3C->ot, &D_8009D310);
        actor->u.step += D_8009D3CC;
        if (actor->u.step >= 0xFF) {
            actor->u.step = 0xFF;
            D_8009C178 = 0;
        }
        break;
    default:
        goto end;
    }
    actor->wait++;
end:
    return result;
}

/* Open the text window. */
s32 func_80092BE4(void) {
    D_8009BD24 = -1;
    func_80032F54(&D_8009D498, 0x3C0, 0x180, 0xA0, 0x78, 0x20, 1);
    D_8009D498.unk68 = 8;
    D_8009D498.flags |= 2;
    func_80034614(&D_8009D498);
    return 1;
}

/* Show the current path's name in the text window while it changes. */
s32 func_80092C70(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    switch (actor->state) {
    case 0:
        if (D_8009BD24 != -1) {
            func_80034614(&D_8009D498);
            func_80034714(&D_8009D498, func_80033728(D_8009D784, D_8009BD24));
            actor->state = 1;
            actor->u.step = D_8009BD24;
        }
        break;
    case 1:
        if (D_8009BD24 == -1) {
            func_80034614(&D_8009D498);
            actor->state = 0;
        } else if (D_8009BD24 != actor->u.step) {
            func_80034614(&D_8009D498);
            func_80034714(&D_8009D498, func_80033728(D_8009D784, D_8009BD24));
            actor->u.step = D_8009BD24;
        }
        break;
    }
    func_80034888(&D_8009D498, D_8009BE3C->ot, D_8009D7F0);
    return 1;
}

/* Release a resident object. */
void func_80092DD0(void) {
    func_800346D4(&D_8009D498);
}

/* Open the destination-name window and build its marker quad (both display copies). */
s32 func_80092DF8(void) {
    D_8009CE68 = -1;
    func_80032F54(&D_8009BD64, 0x3C0, 0x18D, 0xA0, 0x78, 0x20, 4);
    D_8009BD64.unk68 = 8;
    D_8009D498.flags |= 2;
    func_80034614(&D_8009BD64);
    setlen(&D_8009D2B8[0], 9);
    D_8009D2B8[0].code = 0x2C;
    setXY4(&D_8009D2B8[0], 0x98, 0x70, 0x128, 0x70, 0x98, 0xB4, 0x128, 0xB4);
    D_8009D2B8[0].u0 = 0x80;
    D_8009D2B8[0].v0 = 0;
    D_8009D2B8[0].u1 = 0xFF;
    D_8009D2B8[0].v1 = 0;
    D_8009D2B8[0].u2 = 0x80;
    D_8009D2B8[0].v2 = 0x3F;
    D_8009D2B8[0].u3 = 0xFF;
    D_8009D2B8[0].v3 = 0x3F;
    setRGB0(&D_8009D2B8[0], 0x80, 0x80, 0x80);
    D_8009D2B8[0].tpage = GetTPage(0, 0, 0x380, 0x100);
    D_8009D2B8[0].clut = GetClut(0x130, 0x1E4);
    SetSemiTrans(&D_8009D2B8[0], 1);
    D_8009D2B8[1] = D_8009D2B8[0];
    return 1;
}

/* Show the destination name while it changes and draw its marker. */
s32 func_80092FD8(s32 index) {
    WorldmapActor *actor;

    actor = &D_8009BE24[index];
    switch (actor->state) {
    case 0:
        if (D_8009CE68 != -1) {
            func_80034614(&D_8009BD64);
            func_80034714(&D_8009BD64, func_80033728(D_8009D784, D_8009CE68));
            actor->state = 1;
            actor->u.step = D_8009CE68;
        }
        break;
    case 1:
        if (D_8009CE68 == -1) {
            func_80034614(&D_8009BD64);
            actor->state = 0;
        } else if (D_8009CE68 != actor->u.step) {
            func_80034614(&D_8009BD64);
            func_80034714(&D_8009BD64, func_80033728(D_8009D784, D_8009CE68));
            actor->u.step = D_8009CE68;
        }
        break;
    }
    func_80034888(&D_8009BD64, D_8009BE3C->ot, D_8009D7F0);
    if (actor->state == 1) {
        addPrim(D_8009BE3C->ot, &D_8009D2B8[D_8009D7F0]);
    }
    return 1;
}

/* Release a resident object. */
void func_800931B0(void) {
    func_800346D4(&D_8009BD64);
}

/* Build `steps` fades of 256 CLUT entries towards `colour` (entry 0 stays transparent). */
#ifdef NON_MATCHING /* the original keeps colour*t products inside the inner loop */
void func_800931D8(u16 *clut, u16 *out, s32 steps, u8 *colour) {
    CVECTOR c;
    s32 r, g, b;
    s32 i, j;
    u32 t, inv;
    u16 *src;

    r = colour[0];
    g = colour[1];
    b = colour[2];
    for (i = 0; i < steps; i++) {
        t = (i << 12) / steps;
        src = clut;
        inv = 0x1000 - t;
        for (j = 0; j < 0x100; j++) {
            if (*src == 0) {
                *out = *src;
            } else {
                c.r = (*src & 0x1F) << 3;
                c.g = (*src >> 2) & 0xF8;
                c.b = (*src >> 7) & 0xF8;
                *out = (*src & 0x8000) | ((c.r * inv + r * t) >> 15) | (((c.g * inv + g * t) >> 15) << 5) |
                       (((c.b * inv + b * t) >> 15) << 10);
            }
            src++;
            out++;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_800931D8);
#endif

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

/* Unit normal of the terrain triangle under a position (cells split along one diagonal). */
void func_80093740(VECTOR *normal, s32 x, s32 z) {
    u8 *cell;
    TerrainScratch *scratch;

    scratch = TERRAIN_SCRATCH;
    cell = func_80093660(x, z);
    x = (u16)(x / 8);
    z = (u16)(z / 8);
    if (cell[1] & 0x80) {
        if (x * D_8009B244[1].vx + -z * D_8009B244[1].vz < 0) {
            scratch->edge0.vx = 0x10;
            scratch->edge0.vz = -0x10;
            scratch->edge1.vx = 0;
            scratch->edge1.vz = -0x10;
            scratch->edge0.vy = (s8)cell[0x28] - (s8)cell[0];
            scratch->edge1.vy = (s8)cell[0x24] - (s8)cell[0];
        } else {
            scratch->edge0.vx = 0x10;
            scratch->edge1.vx = 0x10;
            scratch->edge0.vz = 0;
            scratch->edge1.vz = -0x10;
            scratch->edge0.vy = (s8)cell[4] - (s8)cell[0];
            scratch->edge1.vy = (s8)cell[0x28] - (s8)cell[0];
        }
    } else {
        if ((x - 0x10000) * D_8009B244[0].vx + -z * D_8009B244[0].vz < 0) {
            scratch->edge0.vx = -0x10;
            scratch->edge0.vz = -0x10;
            scratch->edge1.vx = -0x10;
            scratch->edge1.vz = 0;
            scratch->edge0.vy = (s8)cell[0x24] - (s8)cell[4];
            scratch->edge1.vy = (s8)cell[0] - (s8)cell[4];
        } else {
            scratch->edge0.vx = 0;
            scratch->edge0.vz = -0x10;
            scratch->edge1.vx = -0x10;
            scratch->edge1.vz = -0x10;
            scratch->edge0.vy = (s8)cell[0x28] - (s8)cell[4];
            scratch->edge1.vy = (s8)cell[0x24] - (s8)cell[4];
        }
    }
    OuterProduct0(&scratch->edge1, &scratch->edge0, &scratch->normal);
    VectorNormal(&scratch->normal, normal);
}

/* Terrain surface height at a position (world units): the cell triangle's plane. */
s32 func_80093978(s32 x, s32 z) {
    VECTOR point;
    VECTOR origin;
    VECTOR normal;
    u8 *cell;

    cell = func_80093660(x, z);
    point.vx = (u16)(x / 8);
    point.vz = -(u16)(z / 8);
    if (cell[1] & 0x80) {
        origin.vx = 0;
        origin.vz = 0;
        origin.vy = (s8)cell[0] << 12;
    } else {
        origin.vx = 0x10000;
        origin.vz = 0;
        origin.vy = (s8)cell[4] << 12;
    }
    func_80093740(&normal, x, z);
    func_800935DC(&point, &origin, &normal);
    return point.vy * 8;
}

/* Wave-displaced terrain height at a position (20.12): corners bob with two phase-scrolled sines. */
s32 func_80093A5C(s32 x, s32 z) {
    u8 *cell;
    TerrainScratch *scratch;
    s32 ix, iz;
    s32 x0, x1, z0, z1;
    s32 amplitude;
    s32 phase_x, phase_z;
    s32 cx, cz;

    scratch = TERRAIN_SCRATCH;
    cell = func_80093660(x, z);
    ix = ((u32)x >> 19) & 7;
    x0 = ix << 9;
    iz = ((u32)z >> 19) & 7;
    z0 = iz << 9;
    phase_x = D_8009C5BC + x0;
    phase_z = D_8009C618 + z0;
    amplitude = (func_8003F8B0(phase_z) << 4) >> 3;
    scratch->corners[0].vy = (func_8003F8B0(phase_x) * amplitude >> 20) + ((s8 *)cell)[0] * 8;
    x1 = (ix + 1) << 9;
    phase_x = D_8009C5BC + x1;
    phase_z = D_8009C618 + z0;
    amplitude = (func_8003F8B0(phase_z) << 4) >> 3;
    scratch->corners[1].vy = (func_8003F8B0(phase_x) * amplitude >> 20) + ((s8 *)cell)[4] * 8;
    z1 = (iz + 1) << 9;
    phase_x = D_8009C5BC + x0;
    phase_z = D_8009C618 + z1;
    amplitude = (func_8003F8B0(phase_z) << 4) >> 3;
    scratch->corners[2].vy = (func_8003F8B0(phase_x) * amplitude >> 20) + ((s8 *)cell)[0x24] * 8;
    phase_x = D_8009C5BC + x1;
    phase_z = D_8009C618 + z1;
    amplitude = (func_8003F8B0(phase_z) << 4) >> 3;
    scratch->corners[3].vy = (func_8003F8B0(phase_x) * amplitude >> 20) + ((s8 *)cell)[0x28] * 8;
    cx = (u16)(x / 8);
    cz = (u16)(z / 8);
    if (cell[1] & 0x80) {
        if (cx * D_8009B244[1].vx + -cz * D_8009B244[1].vz < 0) {
            scratch->edge0.vx = 0x80;
            scratch->edge0.vz = -0x80;
            scratch->edge1.vz = -0x80;
            scratch->edge1.vx = 0;
            scratch->edge0.vy = scratch->corners[3].vy - scratch->corners[0].vy;
            scratch->edge1.vy = scratch->corners[2].vy - scratch->corners[0].vy;
        } else {
            scratch->edge0.vx = 0x80;
            scratch->edge1.vx = 0x80;
            scratch->edge0.vz = 0;
            scratch->edge1.vz = -0x80;
            scratch->edge0.vy = scratch->corners[1].vy - scratch->corners[0].vy;
            scratch->edge1.vy = scratch->corners[3].vy - scratch->corners[0].vy;
        }
    } else {
        if ((cx - 0x10000) * D_8009B244[0].vx + -cz * D_8009B244[0].vz < 0) {
            scratch->edge0.vx = -0x80;
            scratch->edge0.vz = -0x80;
            scratch->edge1.vx = -0x80;
            scratch->edge1.vz = 0;
            scratch->edge0.vy = scratch->corners[2].vy - scratch->corners[1].vy;
            scratch->edge1.vy = scratch->corners[0].vy - scratch->corners[1].vy;
        } else {
            scratch->edge0.vx = 0;
            scratch->edge0.vz = -0x80;
            scratch->edge1.vx = -0x80;
            scratch->edge1.vz = -0x80;
            scratch->edge0.vy = scratch->corners[3].vy - scratch->corners[1].vy;
            scratch->edge1.vy = scratch->corners[2].vy - scratch->corners[1].vy;
        }
    }
    OuterProduct0(&scratch->edge1, &scratch->edge0, &scratch->normal);
    VectorNormal(&scratch->normal, &scratch->edge0);
    scratch->edge1.vx = (x >> 12) & 0x7F;
    scratch->edge1.vz = -((z >> 12) & 0x7F);
    if (cell[1] & 0x80) {
        scratch->normal.vx = 0;
        scratch->normal.vz = 0;
        scratch->normal.vy = scratch->corners[0].vy;
    } else {
        scratch->normal.vx = 0x80;
        scratch->normal.vz = 0;
        scratch->normal.vy = scratch->corners[1].vy;
    }
    func_800935DC(&scratch->edge1, &scratch->normal, &scratch->edge0);
    return scratch->edge1.vy << 12;
}

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
s32 func_80094154(VECTOR *a, VECTOR *b) {
    s32 x;
    s32 z;

    x = abs(a->vx - b->vx) >> 12;
    z = abs(a->vz - b->vz) >> 12;
    return SquareRoot0(x * x + z * z);
}

/* Heading from `from` to `to` (0..0xFFF) and its unit direction. */
void func_800941C4(VECTOR *from, VECTOR *to, VECTOR *direction, s16 *heading) {
    s32 x;

    x = to->vx;
    *heading = (ratan2(to->vz - from->vz, x - from->vx) + 0x400) & 0xFFF;
    direction->vx = func_8003F8B0(*heading);
    direction->vz = -func_8003F8CC(*heading);
}

/* Find the region of path table `table` containing the position: a path becomes current, a destination (kind 4) is recorded. */
#ifdef NON_MATCHING /* the destination and failure tails are cross-jumped */
s32 func_80094238(VECTOR *position, s32 table) {
    PathRegion *region;
    u16 x;
    u16 z;

    x = (u32)position->vx >> 12;
    region = ((PathRegion *)D_8009BD00[table]);
    z = (u32)position->vz >> 12;
    if (region->id != -1) {
        do {
            if ((x >= region->x) & (region->x + region->w >= x) & (z >= region->z) & (region->z + region->h >= z)) {
                if (region->kind == 4) {
                    D_8009D7D8 = (PathTable *)-1;
                    D_8009BD24 = -1;
                    D_8009CE68 = region->link;
                    return 1;
                }
                D_8009D7D8 = (PathTable *)region;
                D_8009CE68 = -1;
                D_8009BD24 = region->link;
                return 1;
            }
            region++;
        } while (region->id != -1);
    }
    D_8009D7D8 = (PathTable *)-1;
    D_8009BD24 = -1;
    D_8009CE68 = -1;
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80090A84", func_80094238);
#endif

/* Find the region of path table `table` of kind `kind` containing the
 * position; it becomes the current path. */
s32 func_80094364(VECTOR *position, s32 table, s32 kind) {
    PathRegion *region;
    u16 x;
    u16 z;

    x = (u32)position->vx >> 12;
    z = (u32)position->vz >> 12;
    region = ((PathRegion **)D_8009BD00)[table];
    if (region->id != -1) {
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

void func_80094434(void) {
}

/* Step along the direction to the next cell boundary in +x: 1 when the far side is walkable (step[0] moves there), 3 when only the near side is, else 0. */
s32 func_8009443C(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row) {
    s32 slope;

    slope = (direction->vz << 12) / direction->vx;
    step[0].vx = (step[1].vx & 0xFFF80000) - origin->vx;
    step[2].vx = step[0].vx - 1;
    step[3].vx = step[0].vx;
    step[2].vz = (slope * step[2].vx >> 12) + origin->vz;
    step[3].vz = (slope * step[3].vx >> 12) + origin->vz;
    step[2].vx += origin->vx;
    step[3].vx += origin->vx;
    func_80093354(&step[2]);
    if (func_80094060(row, func_80093F18(&step[2])) == 0) {
        step[0] = step[2];
        return 1;
    }
    func_80093354(&step[3]);
    return func_80094060(row, func_80093F18(&step[3])) == 0 ? 3 : 0;
}

/* Step to the cell boundary in -x (see func_8009443C). */
s32 func_800945C8(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row) {
    s32 slope;

    slope = (direction->vz << 12) / direction->vx;
    step[0].vx = (origin->vx & 0xFFF80000) - origin->vx;
    step[2].vx = step[0].vx;
    step[3].vx = step[0].vx - 1;
    step[2].vz = (slope * step[2].vx >> 12) + origin->vz;
    step[3].vz = (slope * step[3].vx >> 12) + origin->vz;
    step[2].vx += origin->vx;
    step[3].vx += origin->vx;
    func_80093354(&step[2]);
    if (func_80094060(row, func_80093F18(&step[2])) == 0) {
        step[0] = step[2];
        return 1;
    }
    func_80093354(&step[3]);
    return func_80094060(row, func_80093F18(&step[3])) == 0 ? 3 : 0;
}

/* Step to the cell boundary in +z: 1 far side walkable, 2 near side only, else 0. */
s32 func_80094750(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row) {
    s32 slope;

    slope = (direction->vx << 12) / direction->vz;
    step[0].vz = (step[1].vz & 0xFFF80000) - origin->vz;
    step[2].vz = step[0].vz - 1;
    step[3].vz = step[0].vz;
    step[2].vx = (slope * step[2].vz >> 12) + origin->vx;
    step[3].vx = (slope * step[3].vz >> 12) + origin->vx;
    step[2].vz += origin->vz;
    step[3].vz += origin->vz;
    func_80093354(&step[2]);
    if (func_80094060(row, func_80093F18(&step[2])) == 0) {
        step[0] = step[2];
        return 1;
    }
    func_80093354(&step[3]);
    return (func_80094060(row, func_80093F18(&step[3])) == 0) * 2;
}

/* Step to the cell boundary in -z (see func_80094750). */
s32 func_800948D8(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row) {
    s32 slope;

    slope = (direction->vx << 12) / direction->vz;
    step[0].vz = (origin->vz & 0xFFF80000) - origin->vz;
    step[2].vz = step[0].vz;
    step[3].vz = step[0].vz - 1;
    step[2].vx = (slope * step[2].vz >> 12) + origin->vx;
    step[3].vx = (slope * step[3].vz >> 12) + origin->vx;
    step[2].vz += origin->vz;
    step[3].vz += origin->vz;
    func_80093354(&step[2]);
    if (func_80094060(row, func_80093F18(&step[2])) == 0) {
        step[0] = step[2];
        return 1;
    }
    func_80093354(&step[3]);
    return (func_80094060(row, func_80093F18(&step[3])) == 0) * 2;
}
