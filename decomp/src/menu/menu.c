#include "menu.h"
#include "sparkle.h"
#include "scene.h"

/* Start the menu camera: mode 3 setup and its script block. */
void func_800707A8(void) {
    func_80083C0C(3);
    func_800346D4(D_80092954);
}

/* One easing step from current toward target: the remaining distance
 * (rounded away from zero) divided by the number of steps. */
s32 func_800707D8(s32 target, s32 current, s32 steps) {
    s32 delta = target - current;

    if (delta < 0) {
        delta++;
        delta -= steps;
    } else {
        delta--;
        delta += steps;
    }
    return delta / steps;
}

/* Ease the camera eye toward target over the given number of steps; the
 * eye height is compared including the current lift. */
void func_80070808(Vector *target, s32 steps) {
    D_8009867C.vx += func_800707D8(target->vx, D_8009867C.vx, steps);
    D_8009867C.vz += func_800707D8(target->vz, D_8009867C.vz, steps);
    D_8009867C.vy += func_800707D8(target->vy, D_8009867C.vy + D_800925F4, steps);
}

/* Ease the camera look-at point toward target, limited by the collision
 * step check. */
void func_800708C4(Vector *target, s32 steps) {
    Vector step;

    step.vx = func_800707D8(target->vx, D_8009871C.vx, steps);
    step.vy = func_800707D8(target->vy, D_8009871C.vy, steps);
    step.vz = func_800707D8(target->vz, D_8009871C.vz, steps);
    func_800828F8(&D_8009871C, &step, 0x3D00);
    D_8009871C.vx += step.vx;
    D_8009871C.vy += step.vy;
    D_8009871C.vz += step.vz;
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FAF0);

#ifdef NON_MATCHING
/* Place the menu camera for one of the view modes. Does not match: GCC
 * 8-aligns the jump table (original at 0x8006faf4), and cases 3/4 are
 * cross-jumped after the look-at copy rather than before the lift store. */
void func_8007099C(u32 mode) {
    Vector target;

    switch (mode) {
    case 0:
        func_80083738(&D_8009872C, &D_80097010);
        break;
    case 1:
        target = D_8009872C.pos;
        D_800925F4 = 0;
        target.vy -= 0xA0;
        func_80070808(&target, 4);
        break;
    case 2:
        target = D_80097010.pos;
        D_800925F4 = 0;
        target.vy -= 0xA0;
        func_80070808(&target, 4);
        break;
    case 3:
        D_800925F4 = 0xA0;
        target = D_80099078;
        target.vy += D_800925F4;
        func_80070808(&target, 0x10);
        target.vx = D_8009872C.pos.vx + ((func_8003F8B0(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        target.vy = D_8009872C.pos.vy - 0x20 - D_800925F4;
        target.vz = D_8009872C.pos.vz + ((func_8003F8CC(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        func_800708C4(&target, 0x46);
        {
            s32 top = func_80082488(&D_8009871C, 0) - 0x40 - D_800925F4;
            if (top < D_8009871C.vy) {
                D_8009871C.vy = top;
            }
        }
        break;
    case 4:
        D_800925F4 = 0x80;
        target = D_80099078;
        target.vy += D_800925F4;
        func_80070808(&target, 0x10);
        target.vx = D_8009872C.pos.vx + ((func_8003F8B0(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        target.vy = D_8009872C.pos.vy - 0x20 - D_800925F4;
        target.vz = D_8009872C.pos.vz + ((func_8003F8CC(D_8009872C.angle + 0xA80) * 0xD0) >> 12);
        func_800708C4(&target, 0x46);
        {
            s32 top = func_80082488(&D_8009871C, 0) - 0x40 - D_800925F4;
            if (top < D_8009871C.vy) {
                D_8009871C.vy = top;
            }
        }
        break;
    case 5:
        D_8009867C.vx = D_80097010.pos.vx;
        D_8009867C.vy = D_80097010.pos.vy - 0xC0;
        D_8009867C.vz = D_80097010.pos.vz;
        D_8009871C.vx = D_8009867C.vx + ((func_8003F8B0(D_80097010.angle + 0x900) * 0xE0) >> 12);
        D_8009871C.vy = D_80097010.pos.vy - 0xD0;
        D_8009871C.vz = D_8009867C.vz + ((func_8003F8CC(D_80097010.angle + 0x900) * 0xE0) >> 12);
        break;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007099C);
#endif

/* Re-centre the two actors and the look-at point on a fixed scene spot:
 * the midpoint of the actors moves to the layout's anchor, actors on the
 * floor and the look-at point at a fixed height. */
void func_80070C7C(s32 layout) {
    Vector first = D_8009872C.pos;
    Vector second = D_80097010.pos;
    Vector look = D_8009871C;
    Vector centre = first;

    centre.vx += second.vx;
    centre.vy += second.vy;
    centre.vz += second.vz;
    centre.vx /= 2;
    centre.vy /= 2;
    centre.vz /= 2;
    first.vx -= centre.vx;
    first.vy -= centre.vy;
    first.vz -= centre.vz;
    second.vx -= centre.vx;
    second.vy -= centre.vy;
    second.vz -= centre.vz;
    look.vx -= centre.vx;
    look.vy -= centre.vy;
    look.vz -= centre.vz;
    switch (layout) {
    case 0:
        centre.vx = 0x4000;
        centre.vy = 0;
        centre.vz = 0x4000;
        break;
    case 1:
        centre.vx = 0x6000;
        centre.vy = 0;
        centre.vz = 0x6000;
        break;
    case 2:
        centre.vx = 0x2000;
        centre.vy = 0;
        centre.vz = 0x2000;
        break;
    case 3:
        centre.vx = 0x4000;
        centre.vy = 0;
        centre.vz = 0x2400;
        break;
    }
    first.vx += centre.vx;
    first.vy += centre.vy;
    first.vz += centre.vz;
    second.vx += centre.vx;
    second.vy += centre.vy;
    second.vz += centre.vz;
    look.vx += centre.vx;
    look.vy += centre.vy;
    look.vz += centre.vz;
    first.vy = 0;
    second.vy = 0;
    look.vy = -0x300;
    D_8009872C.pos = first;
    D_80097010.pos = second;
    D_8009871C = look;
    func_8007E24C();
}

/* Reset both actors' states and clear their 0x8000 flag. */
void func_80070F80(s32 arg) {
    D_800925F8 = arg;
    D_8009872C.state = 0;
    D_80097010.state = 0;
    D_800925FC = 0;
    D_8009872C.flags &= ~0x8000;
    D_80097010.flags &= ~0x8000;
}

/* Turn an actor toward one of two headings depending on which side of the
 * scene centre it stands, and reset its state. */
s32 func_80070FD8(Actor *actor) {
    Vector pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if ((func_8004B32C(pos.vx, pos.vz) & 0xFFF) > 0x200) {
        actor->target_angle = 0x800 - D_80092934;
    } else {
        actor->target_angle = 0xC00 - D_80092934;
    }
    actor->state = 0xFF;
    actor->unkCE = 0;
    return 0;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007107C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80071724);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80071794);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800718C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007191C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800719F0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80071AD0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80071DA4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80071F8C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800720C4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800720D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80072170);

void func_800725A8(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800725B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800726B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007273C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80072858);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80072D18);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073064);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800730AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800730F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007313C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800731F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800732AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800732CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007334C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073424);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073644);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073B7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073CA4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073CEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073DE4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073E2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80073F34);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800740E4);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC10);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074678);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074998);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074AB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80074BA4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075060);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800751C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007570C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075738);

void func_80075748(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075750);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075888);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075A4C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80075B50);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007639C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800763E4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80076424);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC3C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC48);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC54);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC58);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC5C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC64);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC6C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC74);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC78);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC8C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FC94);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FCA8);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FCB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80076438);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800764CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007661C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800767C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80076884);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077038);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077584);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007762C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800776A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077770);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077A38);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077A88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80077A9C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078154);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078194);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078704);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078920);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078D20);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078E94);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078ED4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80078F00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007920C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800796B8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079A8C);

void func_80079B04(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079B0C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079B44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079D08);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079D6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079DE0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80079DF0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A21C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A344);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A6D0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A730);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A768);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A884);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007A958);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007AC3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007AE10);

/* Set up a scene model with the given mode and place it. */
void func_8007B210(SceneModel *model, s32 mode) {
    model->unk15CC = model->unk9CC;
    model->unk4F = 0x10;
    model->unk4C = mode;
    model->unk52 = 0;
    D_8009292C = 0x100;
    func_80074BA4(model);
    func_80074678(model, model->unk998, model->unk99A);
}

/* Build a four-entry palette from two colours (components biased by 0x80,
 * clamped at zero) and load it, keeping the returned CLUT id. */
void func_8007B270(u8 *first, u8 *second) {
    s32 r, g, b;

    r = first[0] - 0x80;
    g = first[1] - 0x80;
    b = first[2] - 0x80;
    if (r < 0) {
        r = 0;
    }
    if (g < 0) {
        g = 0;
    }
    if (b < 0) {
        b = 0;
    }
    D_800926A8[0] = (((r >> 2) & 0x1F) + ((g << 3) & 0x3E0) + ((b << 8) & 0x7C00)) | -0x8000;
    r = second[0] - 0x80;
    g = second[1] - 0x80;
    b = second[2] - 0x80;
    if (r < 0) {
        r = 0;
    }
    if (g < 0) {
        g = 0;
    }
    if (b < 0) {
        b = 0;
    }
    D_800926A8[1] = (((r >> 2) & 0x1F) + ((g << 3) & 0x3E0) + ((b << 8) & 0x7C00)) | -0x8000;
    D_800926A8[2] = 0;
    D_800926A8[3] = 0x1111;
    D_800926A0 = func_800438C0(D_800926A8, D_80092698, D_8009269C);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007B388);

/* Advance every live sparkle one frame (expiring it after its last frame,
 * letting it fall otherwise) and latch the per-frame counters. */
void func_8007BACC(void) {
    Sparkle *sparkle;
    s32 i;
    s32 count;

    sparkle = D_80092AD8;
    for (i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (sparkle->active) {
            if (sparkle->frame == sparkle->frame_count) {
                sparkle->active = 0;
            } else {
                sparkle->frame++;
                if (sparkle->type == 0) {
                    sparkle->u.fall.fall_speed += sparkle->u.fall.gravity;
                    sparkle->y += sparkle->u.fall.fall_speed;
                }
            }
        }
    }
    D_800926A4++;
    count = D_800926B0;
    D_800926B0 = 0;
    D_800926B4 = count;
}

/* Free every sparkle. */
void func_8007BB7C(void) {
    Sparkle *sparkle = D_80092AD8;
    s32 i;

    for (i = SPARKLE_COUNT - 1; i >= 0; i--, sparkle++) {
        sparkle->active = 0;
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007BBA0);

/* Set the colour of kind-2 sparkles. */
void func_8007C100(Color *color) {
    D_800926B8 = *color;
}

/* Start a sparkle of the given kind at a position, in the first free slot. */
void func_8007C124(SVector *pos, s32 kind) {
    Sparkle *sparkle = D_80092AD8;
    SparkleKind *info;
    PolyFT4 *prim;
    s32 i;

    for (i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (!sparkle->active) {
            break;
        }
    }
    if (i == SPARKLE_COUNT) {
        return;
    }
    sparkle->active = 1;
    info = &D_80092A74[kind];
    sparkle->type = 0;
    sparkle->frame = 0;
    sparkle->u.fall.kind = info;
    sparkle->frame_count = info->frame_count;
    sparkle->u.fall.gravity = info->gravity;
    sparkle->u.fall.fall_speed = 0;
    sparkle->x = pos->vx;
    sparkle->y = pos->vy;
    sparkle->z = pos->vz;
    prim = sparkle->prim;
    prim->tpage = info->tpage;
    if (kind == 2) {
        prim->code &= ~1;
        prim->r0 = D_800926B8.r;
        prim->g0 = D_800926B8.g;
        prim->b0 = D_800926B8.b;
    } else {
        prim->code |= 1;
    }
    prim->clut = info->clut;
    sparkle->prim[1] = *prim;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C280);

#ifdef NON_MATCHING
/* Start a trail segment of a key at a position for the current owner
 * (once per key and owner), with the given texture column and size, linked
 * to the segment started on the previous frame.
 * Does not match: the texture and header stores are scheduled in a different order. */
void func_8007C880(s32 column, Vector *pos, s32 key, s32 size) {
    Sparkle *sparkle;
    Sparkle *other;
    PolyFT4 *prim;
    s32 i;

    for (i = 0, sparkle = D_80092AD8; i < SPARKLE_COUNT; i++, sparkle++) {
        if (sparkle->active && sparkle->u.trail.key == key && sparkle->u.trail.owner == D_800928E8) {
            return;
        }
    }
    sparkle = D_80092AD8;
    for (i = 0; i < SPARKLE_COUNT; i++, sparkle++) {
        if (!sparkle->active) {
            break;
        }
    }
    if (i == SPARKLE_COUNT) {
        return;
    }
    prim = sparkle->prim;
    prim->u0 = prim->u1 = prim->u2 = prim->u3 = (u8)D_80092698 * 4 + 8 + column * 4;
    prim->v0 = prim->v1 = prim->v2 = prim->v3 = D_8009269C;
    prim->code &= ~1;
    prim->tpage = D_80092694;
    prim->clut = D_800926A0;
    sparkle->prim[1] = *prim;
    sparkle->frame_count = 7;
    sparkle->type = 1;
    sparkle->active = 1;
    sparkle->frame = 0;
    sparkle->x = pos->vx;
    sparkle->y = pos->vy;
    sparkle->u.trail.owner = D_800928E8;
    sparkle->u.trail.key = key;
    sparkle->u.trail.prev = NULL;
    sparkle->u.trail.stamp = D_800926A4;
    sparkle->z = pos->vz;
    sparkle->u.trail.size = D_80091228[size];
    for (i = 0, other = D_80092AD8; i < SPARKLE_COUNT; i++, other++) {
        if (other->active && other->u.trail.key == key && other != sparkle && other->type == 1 &&
            other->u.trail.stamp == (u16)(D_800926A4 - 1)) {
            sparkle->u.trail.prev = other;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C880);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CAA4);

/* Pack four fields into one word: top byte, 16-bit middle, flag bit 7 and
 * a 7-bit low field. */
u32 func_8007CD14(s32 flag, s32 top, s32 middle, s32 low) {
    return (low & 0x7F) | ((flag << 7) & 0x80) | (top << 24) | ((middle << 8) & 0xFFFF00);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CD44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CF78);

/* Copy the camera position to the scratchpad and run the scene pass. */
void func_8007D068(void *arg) {
    SCENE_SCRATCH->camera = D_80096FA8;
    func_8007E3CC(arg);
}

/* Jitter a short position by -24..23 on each axis. */
void func_8007D0B4(SVector *pos) {
    pos->vx += func_8003FA38() % 48 - 24;
    pos->vy += func_8003FA38() % 48 - 24;
    pos->vz += func_8003FA38() % 48 - 24;
}

/* Start a sparkle of kind 0..4 at a position; kinds 8..12 are the same
 * sparkles with the position jittered first. Declared int without a return
 * value, as the original's unfilled branch delay slot shows. */
s32 func_8007D190(Vector *pos, u32 kind) {
    SVector at;

    at.vx = pos->vx;
    at.vy = pos->vy;
    at.vz = pos->vz;
    switch (kind) {
    case 8:
        func_8007D0B4(&at);
    case 0:
        func_8007C124(&at, 0);
        break;
    case 9:
        func_8007D0B4(&at);
    case 1:
        func_8007C124(&at, 1);
        break;
    case 10:
        func_8007D0B4(&at);
    case 2:
        func_8007C124(&at, 2);
        break;
    case 11:
        func_8007D0B4(&at);
    case 3:
        func_8007C124(&at, 3);
        break;
    case 12:
        func_8007D0B4(&at);
    case 4:
        func_8007C124(&at, 4);
        break;
    }
}

/* Whether a code lies in 0x10..0x1f. */
s32 func_8007D25C(s32 code) {
    if (code < 0x10) {
        return 0;
    }
    return code < 0x20;
}

/* Jitter a position by -32..31 on each axis. */
void func_8007D274(Vector *from, Vector *to) {
    to->vx = from->vx + func_8003FA38() % 64 - 32;
    to->vy = from->vy + func_8003FA38() % 64 - 32;
    to->vz = from->vz + func_8003FA38() % 64 - 32;
}

#ifdef NON_MATCHING
/* Queue a three-strand bolt of jittered 7-segment lines between two points,
 * coloured by kind: 0 green-blue flicker, 1 random grey-yellow, 2
 * alternating white and red segments.
 * Does not match: the address of prev is kept in a saved register. */
void func_8007D334(Vector *from, Vector *to, s32 kind) {
    Vector point;
    Vector step;
    Vector prev;
    Vector next;
    Color color;
    s32 strand;
    s32 i;
    s32 value;

    step = *to;
    step.vx -= from->vx;
    step.vy -= from->vy;
    step.vz -= from->vz;
    func_800886FC(&step);
    step.vx /= 7;
    step.vy /= 7;
    step.vz /= 7;
    for (strand = 0; strand < 3; strand++) {
        point = *from;
        prev = *from;
        for (i = 0; i < 7; i++) {
            point.vx += step.vx;
            point.vy += step.vy;
            point.vz += step.vz;
            func_8007D274(&point, &next);
            switch (kind) {
            case 0:
                color.r = func_8003FA38() & 0x3F;
                color.g = func_8003FA38() % 191 + 0x40;
                color.b = 0xFF;
                break;
            case 1:
                value = func_8003FA38() % 256 + 0x40;
                if (value < 0x100) {
                    color.g = value;
                } else {
                    color.g = 0xFF;
                }
                color.r = color.g = color.g;
                color.b = value / 3;
                break;
            case 2:
                if (i & 1) {
                    color.b = 0xFF;
                    color.g = 0xFF;
                    color.r = 0xFF;
                } else {
                    color.r = 0xFF;
                    color.b = 0;
                    color.g = 0;
                }
                break;
            }
            func_8007E31C(&prev, i == 6 ? to : &next, &color);
            prev = next;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D334);
#endif

/* Map codes 0x11..0x13 to kinds 0..2 and forward them. */
void func_8007D65C(Vector *from, Vector *to, s32 code) {
    switch (code) {
    case 0x11:
        func_8007D334(from, to, 0);
        break;
    case 0x12:
        func_8007D334(from, to, 1);
        break;
    case 0x13:
        func_8007D334(from, to, 2);
        break;
    }
}

#ifdef NON_MATCHING
/* Allocate the scene cell table and both buffers' point primitives.
 * Does not match: the primitive pointer loads are hoisted over the
 * stores (the original keeps every access in order). */
void func_8007D6B8(void) {
    SceneCell10 *cell;
    s32 i;

    D_800926BC = func_80031BDC(0x9F6, 0);
    D_800926C0 = func_80031BDC(0xBF4, 0);
    D_800926C4 = func_80031BDC(0xBF4, 0);
    cell = D_800926BC;
    for (i = 0; i < 0xFF; i++) {
        D_800926C0[i].len = 2;
        D_800926C0[i].rgbc = 0x6880B0F0;
        D_800926C4[i].len = 2;
        D_800926C4[i].rgbc = 0x6880B0F0;
        cell->unk0 = cell->unk2 = cell->unk4 = 0;
        cell->unk0 = cell->unk4 = cell->unk6 = 0;
        cell++;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D6B8);
#endif

#ifdef NON_MATCHING
/* Spawn up to count ground particles in free cells around a position: on
 * the ground below a random point within 32 units, rising for 20 frames.
 * Does not match: the x coordinate is kept pre-shifted for the map index. */
void func_8007D7A8(Vector *pos, s32 count) {
    SceneCell10 *cell = D_800926BC;
    s32 i;
    s32 x;
    s32 z;

    for (i = 0; i < 251; i++, cell++) {
        if (count == 0) {
            break;
        }
        if (cell->unk6 == 0) {
            x = pos->vx + (func_8003FA38() % 64 - 32);
            cell->unk0 = x;
            z = pos->vz + (func_8003FA38() % 64 - 32);
            cell->unk4 = z;
            cell->unk2 = cell->unk8 = D_800928DC[((s16)z >> 8) * 128 + ((s16)x >> 8)].height;
            cell->unk7 = -(func_8003FA38() % 10 + 10);
            count--;
            cell->unk6 = 20;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D7A8);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D918);

/* Allocate the 540 scene cells and their small tiles (2..4 pixels square,
 * pale blue), with a copy of the tiles for the other draw buffer. */
void func_8007DB28(void) {
    SceneCell12 *cell;
    TileWords *tile;
    s32 i;

    D_800926C8 = func_80031BDC(0x1950, 0);
    tile = func_80031BDC(0x21C0, 0);
    D_800926CC = tile;
    D_800926D0 = func_80031BDC(0x21C0, 0);
    cell = D_800926C8;
    for (i = 0; i < 540; i++, cell++, tile++) {
        tile->len = 3;
        tile->rgbc = 0x60FFD0A0;
        tile->w = func_8003FA38() % 3 + 2;
        tile->h = func_8003FA38() % 3 + 2;
        cell->unk0 = cell->unk2 = cell->unk4 = 0;
        cell->unk8 = cell->unkA = cell->unk9 = cell->unk6 = 0;
    }
    func_800732AC(D_800926D0, D_800926CC, 0x21C0);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007DC74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E020);

/* Clear both scene cell tables and the actors' 0x90b bytes. */
void func_8007E24C(void) {
    SceneCell12 *cell;
    s32 i;

    cell = D_800926C8;
    for (i = 0; i < 0x21C; i++) {
        cell[i].unk6 = 0;
        cell[i].unk4 = 0;
        cell[i].unk2 = 0;
        cell[i].unk0 = 0;
    }
    D_80097010.unk90B = 0;
    D_8009872C.unk90B = 0;
    for (i = 0; i < 0xFF; i++) {
        D_800926BC[i].unk6 = 0;
        D_800926BC[i].unk0 = D_800926BC[i].unk2 = D_800926BC[i].unk4 = 0;
    }
}

/* Initialise the scene's line primitives and clear the per-frame counters. */
void func_8007E2D8(void) {
    SceneLine *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        line = &D_80094818[i];
        line->line.len = 3;
        line->line.code = 0x40;
    }
    D_800926B0 = 0;
    D_800926B4 = 0;
}

/* Queue a coloured 3D line segment for this frame (at most 100). */
void func_8007E31C(Vector *from, Vector *to, Color *color) {
    SceneLine *line;

    if (D_800926B0 < 100) {
        line = &D_80094818[D_800926B0];
        line->from.vx = from->vx;
        line->from.vy = from->vy;
        line->from.vz = from->vz;
        line->to.vx = to->vx;
        line->to.vy = to->vy;
        line->to.vz = to->vz;
        line->line.r0 = color->r;
        line->line.g0 = color->g;
        line->line.b0 = color->b;
        D_800926B0++;
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E3CC);

/* Set the scene state, playing sound 0x24 when state 10 starts from 0. */
void func_8007E528(s32 state) {
    if (D_80092708 == 0 && state == 10) {
        func_8008EB4C(0x24);
    }
    D_80092708 = state;
}

/* While the scene state counts down, draw its sprite (when flag 2 is set). */
void func_8007E574(void *ot) {
    if (D_80092708 != 0) {
        if (D_800928E8 & 2) {
            func_80043B48(ot, &D_800954D8[D_800928A0].sprite);
            func_80043B48(ot, &D_800954D8[D_800928A0].tpage);
        }
        D_80092708--;
    }
}

s32 func_8007E624(void) {
    return D_800926DC;
}

#ifdef NON_MATCHING
/* Allocate the text quads, load the font (with its palette's colours 0, 2
 * and 3 replaced) and the banner image, and build the banner sprite.
 * Does not match: the banner sprite address is taken from its length byte. */
void func_8007E634(MenuFiles *files) {
    TimImage image;
    s32 unused[2]; /* never used; the original frame keeps its slot */
    s16 *palette;
    s32 i;

    D_800926D4[0] = func_80031BDC(0xFA0, 0);
    D_800926D4[1] = func_80031BDC(0xFA0, 0);
    for (i = 0; i < 100; i++) {
        ((u8 *)&D_800926D4[0][i].tag)[3] = 0;
        ((u8 *)&D_800926D4[1][i].tag)[3] = 0;
    }
    func_800471B4(files->font);
    func_800471C4(&image);
    palette = image.caddr;
    palette[2] = -0x6F9D;
    palette[0] = 0;
    palette[3] = -1;
    func_80044894(&image.crect->x, image.caddr);
    func_80044894(&image.prect->x, image.paddr);
    D_800926E4 = func_80043A58(image.crect->x, image.crect->y);
    D_800926E0 = func_80043A1C(0, 1, image.prect->x, image.prect->y);
    D_800926DC = 0;
    func_800471B4(files->banner);
    func_800471C4(&image);
    palette = image.caddr;
    palette[0] = 0;
    func_80044894(&image.crect->x, image.caddr);
    func_80044894(&image.prect->x, image.paddr);
    D_800954D8[0].sprite.len = 4;
    D_800954D8[0].sprite.code = 0x65;
    func_80043E20(&D_800954D8[0].tpage, 0, 0, func_80043A1C(0, 1, image.prect->x, image.prect->y));
    D_800954D8[0].sprite.clut = func_80043A58(image.crect->x, image.crect->y);
    D_800954D8[0].sprite.x0 = 0x40;
    D_800954D8[0].sprite.y0 = 0xBE;
    D_800954D8[0].sprite.w = 0xC4;
    D_800954D8[0].sprite.h = 0xD;
    D_800954D8[0].sprite.u0 = image.prect->x * 4;
    D_800954D8[0].sprite.v0 = image.prect->y;
    D_800954D8[1] = D_800954D8[0];
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E634);
#endif

void func_8007E894(s32 x, s32 y) {
    D_800926E8 = x;
    D_800926EC = y;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E8AC);

void func_8007E954(s32 value) {
    D_800912DC = value;
}

#ifdef NON_MATCHING
/* Draw one character at the text cursor (at most 101 quads a frame; '('
 * only advances) and move the cursor right by its scaled width. Declared
 * int without a return value, as the original's unfilled delay slot shows.
 * Does not match: the original loads the glyph width before storing the
 * first vertex, and the texture page/CLUT before the packet length. */
s32 func_8007E964(s32 ch) {
    PolyFT4Words *quad;
    Glyph *glyph;
    s32 right;

    if (D_800926DC < 101) {
        quad = D_800926D4[D_800928A0];
        quad += D_800926DC;
        glyph = func_8007E8AC(ch);
        if (glyph != NULL) {
            if (ch != '(') {
                quad->xy0 = D_800926E8 | (D_800926EC << 16);
                ch = glyph->width | 3; /* the quad width, in the same variable */
                right = D_800926E8 + ((ch * D_800912DC) >> 8);
                quad->xy1 = right | (D_800926EC << 16);
                quad->xy2 = D_800926E8 | ((D_800926EC + glyph->height) << 16);
                quad->xy3 = right | ((D_800926EC + glyph->height) << 16);
                quad->uv0 = glyph->u | (glyph->v << 8);
                quad->uv1 = (glyph->u + ch) | (glyph->v << 8);
                quad->uv2 = glyph->u | ((glyph->v + (glyph->height + 1)) << 8);
                quad->uv3 = (glyph->u + ch) | ((glyph->v + (glyph->height + 1)) << 8);
                quad->len = 9;
                quad->rgbc = D_800926F0 | (D_800926F4 << 8) | (D_800926F8 << 16) | 0x2C000000;
                quad->tpage = D_800926E0;
                quad->clut = D_800926E4;
                D_800926DC++;
            }
            D_800926E8 += ((glyph->width * D_800912DC) >> 8) + 2;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E964);
#endif

/* Width of a text string in pixels at the current text scale. */
s32 func_8007EB6C(u8 *text) {
    s32 width = 0;

    while (*text != 0) {
        width += ((func_8007E8AC(*text++)->width * D_800912DC) >> 8) + 2;
    }
    return width;
}

/* Draw a line of text at the cursor and move the cursor to the next line. */
void func_8007EBE0(u8 *text) {
    s32 x = D_800926E8;

    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text centred on the cursor, then move to the next line. */
void func_8007EC54(u8 *text) {
    s32 x = D_800926E8;

    D_800926E8 -= func_8007EB6C(text) / 2;
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text ending at the cursor, then move to the next line. */
void func_8007ECF0(u8 *text) {
    s32 x = D_800926E8;

    D_800926E8 -= func_8007EB6C(text);
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

/* Draw a line of text shifted left by an offset, then move to the next line. */
void func_8007ED84(u8 *text, s32 offset) {
    s32 unused[2]; /* never used; the original frame keeps its slot */
    s32 x = D_800926E8;

    D_800926E8 = x - offset;
    while (*text != 0) {
        func_8007E964(*text++);
    }
    D_800926E8 = x;
    D_800926EC += 0x14;
}

#ifdef NON_MATCHING
/* Set the text colour: highlighted (fading red) or plain white.
 * Does not match: the original reloads 0xff in the highlight branch. */
void func_8007EE08(s32 highlight) {
    if (highlight) {
        D_800926F0 = D_80059488 * 20;
        D_800926F4 = 0xFF;
        D_800926F8 = 0;
    } else {
        D_800926F0 = 0xFF;
        D_800926F4 = 0xFF;
        D_800926F8 = 0xFF;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EE08);
#endif

#ifdef NON_MATCHING
/* Set the text colour: highlighted (fading toward blue) or plain white.
 * Does not match: the original reloads 0xff in the highlight branch. */
void func_8007EE68(s32 highlight) {
    if (highlight) {
        D_800926F8 = 0xFF;
        D_800926F0 = 0xFF - D_80059488 * 20;
        D_800926F4 = 0xFF - D_80059488 * 20;
        return;
    }
    D_800926F0 = 0xFF;
    D_800926F4 = 0xFF;
    D_800926F8 = 0xFF;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EE68);
#endif

#ifdef NON_MATCHING
/* Build the list of the 49 entries (or, when filtering, of those whose
 * required level the current level reaches) and order it when filtering.
 * Does not match: the source and entry pointers get swapped registers. */
void func_8007EEE8(s32 filter) {
    s32 level = D_8006EF64;
    ListEntry **list = func_80031BDC(0xC4, 1);
    ListSource *source;
    s32 i;

    source = D_80092874;
    D_800928EC = list;
    D_80092888 = 0;
    for (i = 0; i < 49; i++, source++) {
        if (!filter || source->level <= level) {
            list[D_80092888++] = &D_80091964[i];
        }
    }
    if (filter) {
        func_8008895C();
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EEE8);
#endif

#ifdef NON_MATCHING
/* Allocate and lay out the 49 portrait slots: palette rows 511 down and
 * a 7x7 grid of image areas.
 * Does not match: the x shift is scheduled after the first stores. */
void func_8007EFB4(void) {
    u8 unused[0x30]; /* never used; the original frame keeps its slot */
    GridCell *cell;
    s32 id;
    s32 row;
    s32 col;
    s16 top;

    cell = D_8009270C = func_80031BDC(0x3D4, 0);
    id = 0x1FF;
    for (row = 0; row < 7; row++) {
        top = row * 0x20 + 0x1A0;
        for (col = 0; col < 7; col++) {
            cell->clut_x = 0x200;
            cell->clut_y = id--;
            cell->clut_w = 0x80;
            cell->clut_h = 1;
            cell->image_x = top;
            cell->image_y = col << 6;
            cell->image_w = 0x1E;
            cell->image_h = 0x40;
            cell++;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EFB4);
#endif

#ifdef NON_MATCHING
/* Draw the portrait of a list entry (the index wraps around the list) on
 * the left or right side. At fade 64 it is shown full size and unshaded;
 * below, it is shaded and shrunk by fade / 16.
 * Does not match: the vertex arithmetic is scheduled differently. */
void func_8007F05C(s32 index, PolyFT4Words *quad, s32 right_side, s32 x, s32 fade) {
    GridCell *cell;
    s32 shrink;
    s32 top;
    s32 bottom;
    s32 left;
    s32 right;
    s32 u;

    if (right_side) {
        x += 0xD3;
    } else {
        x += 0x33;
    }
    if (index > D_80092888 - 1) {
        index -= D_80092888;
    }
    if (index < 0) {
        index += D_80092888;
    }
    cell = &D_8009270C[D_800928EC[index]->id];
    if (fade == 0x40) {
        quad->len = 9;
        ((u8 *)&quad->rgbc)[3] = 0x2D;
        quad->xy0 = x | 0x300000;
        right = x + 0x3C;
        quad->xy1 = right | 0x300000;
        quad->xy2 = x | 0x700000;
        quad->xy3 = right | 0x700000;
    } else {
        fade += 0x40;
        shrink = (fade - 0x40) >> 4;
        quad->len = 9;
        quad->rgbc = fade | (fade << 8) | (fade << 16) | 0x2C000000;
        left = x - (shrink - 4);
        top = 0x34 - shrink;
        quad->xy0 = left | (top << 16);
        right = left + 0x34 + shrink * 2;
        bottom = top + 0x38 + shrink * 2;
        quad->xy1 = right | (top << 16);
        quad->xy2 = left | (bottom << 16);
        quad->xy3 = right | (bottom << 16);
    }
    u = cell->image_x * 2;
    quad->uv0 = u | (cell->image_y << 8);
    quad->uv1 = (u + 0x3B) | (cell->image_y << 8);
    quad->uv2 = u | ((cell->image_y + 0x3F) << 8);
    quad->uv3 = (u + 0x3B) | ((cell->image_y + 0x3F) << 8);
    quad->clut = func_80043A58(cell->clut_x, cell->clut_y);
    quad->tpage = func_80043A1C(1, 0, cell->image_x & 0xFF80, cell->image_y);
    func_80043B48(D_80092938, quad);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F05C);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F258);

void func_8007F834(void) {
    D_80092740 = 0;
    D_8009273C = 0;
    D_80092744 = 0;
}

#ifdef NON_MATCHING
/* Leave the settings screen: camera mode 1 and flags 0xc on both actors.
 * Does not match: the original addresses both flag words through two
 * address registers in the opposite register order. */
void func_8007F854(void) {
    s32 unused[2]; /* never used; the original frame keeps its slot */

    D_800912F0 = 1;
    func_80083C0C(1);
    D_80092734 = NULL;
    func_8007F834();
    D_8009872C.unkD4 |= 0xC;
    D_80097010.unkD4 |= 0xC;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F854);
#endif

void func_8007F8B4(void) {
    s32 unused[2]; /* never used; the original frame keeps its slot */

    func_80083C0C(1);
    D_80092734 = NULL;
    func_8007F834();
}

void func_8007F8E4(void) {
    func_80080C48(0);
    if (D_80099D98[7] != 0 || D_80092950 == 1) {
        func_80083C0C(3);
    } else {
        D_80092950--;
        func_80083C0C(6);
    }
}

/* Highlight the text of a page's entry when it is under the cursor. */
void func_8007F948(MenuPage *page, s32 entry) {
    if (page->cursor == entry) {
        func_8007EE08(1);
    } else {
        func_8007EE08(0);
    }
}

/* Name of the chosen first setting. */
char *func_8007F97C(void) {
    return D_800912F4[D_80099D98[0]];
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF5C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF60);

/* Draw the values column of the settings page, right-aligned, applying the
 * chosen speed as it is shown. */
void func_8007F9A0(MenuPage *page) {
    char text[8];

    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007EE08(0);
    func_8007F948(page, 0);
    func_8007ECF0(func_8007F97C());
    func_8007F948(page, 1);
    func_8003FBF8(text, D_8006FF5C, D_80099D98[9] + 1);
    D_80099DA4 = D_8009292C = D_8009130C[D_80099D98[9]];
    func_8007ECF0(text);
    func_8007F948(page, 2);
    func_8003FBF8(text, D_8006FF60, D_80091300[D_80099D98[2]]);
    func_8007ECF0(text);
    func_8007F948(page, 3);
    func_8007ECF0(D_80099D98[5] ? "COM" : "USER1");
    func_8007F948(page, 4);
    func_8007ECF0(D_80099D98[6] ? "COM" : "USER2");
    func_8007EE08(0);
}

/* Draw the values column of the second settings page; the chosen entry of
 * setting 10 is also passed to 80081100 as 0x15 + entry. */
void func_8007FB0C(MenuPage *page) {
    char text[8];

    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007EE08(0);
    func_8007ECF0(D_8006FF7C);
    func_8007F948(page, 1);
    func_8007ECF0(D_8009132C[D_80099D98[10]]);
    func_80081100(D_80099D98[10] + 0x15, 1);
    func_8007F948(page, 2);
    func_8003FBF8(text, D_8006FF60, D_80091300[D_80099D98[2]]);
    func_8007ECF0(text);
    func_8007EE08(0);
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF7C);

/* Draw the vibration page: per controller port, the vibration setting when
 * a type-4 controller without the "COM" setting is connected (the entry is
 * hidden otherwise). */
void func_8007FBEC(void) {
    MenuPage *page;
    s32 active;
    s32 unused[2]; /* never used; the original frame keeps its slot */

    func_8007E894(0xA0, 0x8C);
    active = D_80092710 ^ 1;
    active &= 1;
    page = &D_800915AC[5];
    if (active && D_800915AC[5].cursor == 0) {
        D_8009272C = 1;
    } else {
        D_8009272C = 0;
    }
    func_8007E894(0x50, 0x8C);
    if (func_80035734(0) == 4 && D_80099D98[5] == 0) {
        if (active) {
            func_8007F948(page, 1);
        }
        func_8007EC54((D_80099D98[3] & 1) ? "VIBRATION ON" : "VIBRATION OFF");
        D_800915AC[5].item->flags &= ~4;
    } else {
        D_800915AC[5].item->flags |= 4;
    }
    func_8007EE08(0);

    active = D_80092710 >> 1;
    active ^= 1;
    active &= 1;
    if (active && D_80092754 == 0) {
        active = 0;
    }
    page = &D_800915AC[6];
    if (active && D_800915AC[6].cursor == 0) {
        D_80092730 = 1;
    } else {
        D_80092730 = 0;
    }
    func_8007E894(0xF0, 0x8C);
    if (func_80035734(1) == 4 && D_80099D98[6] == 0) {
        if (active) {
            func_8007F948(page, 1);
        }
        func_8007EC54((D_80099D98[4] & 1) ? "VIBRATION ON" : "VIBRATION OFF");
        D_800915AC[6].item->flags &= ~4;
    } else {
        D_800915AC[6].item->flags |= 4;
    }
    func_8007EE08(0);
    func_8007F258(D_80092938, 1);
}

/* Draw the values column of the options page. */
void func_8007FE48(MenuPage *page) {
    char text[16];
    char *value;

    func_8007EE08(0);
    func_8007E894(page->frame[0].x0 + page->frame[0].w - 10, page->y);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007ECF0(D_8006FF7C);
    func_8007F948(page, 4);
    if (D_80099D98[7] != 0) {
        func_8003FBF8(text, D_8006FF5C, D_80099D98[7]);
        value = text;
    } else {
        value = "#";
    }
    func_8007ECF0(value);
    func_8007F948(page, 5);
    func_8007ECF0(D_800912F4[D_80099D98[0]]);
    func_8007F948(page, 6);
    func_8007ECF0(D_80092884 ? "ON" : "OFF");
    func_8007EE08(0);
}

/* Step a settings value with left/right: flag 4 reverses the direction,
 * flag 2 uses the repeating buttons, flag 1 wraps around (else clamps
 * silently). Plays the cursor sound when moved. */
s32 func_8007FF70(s32 value, s32 max, s32 flags) {
    s32 step = 1;
    u32 buttons;
    s32 moved;

    if (flags & 4) {
        step = -1;
    }
    moved = 0;
    if (flags & 2) {
        buttons = D_8009274C;
    } else {
        buttons = D_80092750;
    }
    if (buttons & 0x2000) {
        value += step;
    }
    if (buttons & 0x8000) {
        value -= step;
    }
    if (buttons & 0xA000) {
        moved = 1;
    }
    if (flags & 1) {
        if (value == -1) {
            value = max;
        }
        if (value > max) {
            value = 0;
        }
    } else {
        if (value == -1) {
            moved = 0;
            value = 0;
        }
        if (value > max) {
            moved = 0;
            value = max;
        }
    }
    if (moved) {
        func_8008EB4C(0x20);
    }
    return value;
}

void func_80080054(void) {
    D_80099D98[0] = func_8007FF70(D_80099D98[0], 2, 0);
}

void func_80080090(void) {
    D_80099D98[9] = func_8007FF70(D_80099D98[9], 7, 2);
}

void func_800800CC(void) {
    D_80099D98[2] = func_8007FF70(D_80099D98[2], 4, 2);
}

void func_80080108(void) {
    D_80099D98[3] = func_8007FF70(D_80099D98[3], 1, 1);
}

void func_80080144(void) {
    D_80099D98[4] = func_8007FF70(D_80099D98[4], 1, 1);
}

void func_80080180(void) {
    D_80099D98[5] = func_8007FF70(D_80099D98[5], 1, 1);
}

void func_800801BC(void) {
    D_80099D98[6] = func_8007FF70(D_80099D98[6], 1, 1);
}

void func_800801F8(void) {
    D_80099D98[7] = func_8007FF70(D_80099D98[7], 3, 2);
}

void func_80080234(void) {
    D_80092884 = func_8007FF70(D_80092884, 1, 1);
}

void func_80080268(void) {
    D_80099D98[10] = func_8007FF70(D_80099D98[10], 13, 3);
}

/* First side's selection: cancel, move (skipping the other side's pick
 * unless shared picks are allowed or both already coincide) and confirm. */
void func_800802A4(void) {
    s32 same;

    if (D_80091364 == 0 && (D_8005948C & 0x40)) {
        D_80092710 &= ~1;
        func_80085134(0);
        func_8008EB4C(0x22);
    }
    if (!(D_80092710 & 1)) {
        same = D_80092700 == D_80092704;
        do {
            D_80092700 = func_8007FF70(D_80092700, D_80092888 - 1, 3);
        } while (D_80092700 == D_80092704 && !(D_80092748 & 1) && !same);
        if (D_80091364 == 0 && (D_8005948C & 0x20)) {
            D_80092710 |= 1;
            func_8008EB4C(0x21);
            func_8008509C(0, D_800928EC[D_80092700]->id);
        }
    }
}

/* Second side's selection; cancelling outside mode 2 leaves the screen. */
void func_8008040C(void) {
    s32 same;

    if (D_80092750 & 0x40) {
        if (D_800928C8 != 2) {
            D_80092710 = 0;
            func_80085134(1);
            func_8008EB4C(0x22);
            return;
        }
        D_80092710 &= ~2;
    }
    if (!(D_80092710 & 2)) {
        same = D_80092700 == D_80092704;
        do {
            D_80092704 = func_8007FF70(D_80092704, D_80092888 - 1, 3);
        } while (D_80092700 == D_80092704 && !(D_80092748 & 1) && !same);
        if (D_80092750 & 0x20) {
            D_80092710 |= 2;
            func_8008EB4C(0x21);
            func_8008509C(1, D_800928EC[D_80092704]->id);
        }
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080570);

/* Load both picks' portraits (palette and image) into their VRAM slots and
 * mark both sides confirmed. */
void func_80080644(s32 first, s32 second) {
    u8 *data = func_80031BDC(0x2000, 0);
    u8 *other;
    GridCell *cell;

    func_8002954C((u16 *)func_800289D0(6) + first, data, 0x1000, 0, 0);
    other = data + 0x1000;
    func_8002954C((u16 *)func_800289D0(6) + second, other, 0x1000, 0, 0);
    D_80092700 = first;
    D_80092704 = second;
    D_80092714 = first;
    D_80092720 = second;
    D_80092710 = 3;
    func_80028A60(0);
    cell = &D_8009270C[first];
    func_80044894(&cell->clut_x, data);
    func_80044894(&cell->image_x, data + 0x100);
    cell = &D_8009270C[second];
    func_80044894(&cell->clut_x, other);
    func_80044894(&cell->image_x, data + 0x1100);
    func_80032C18(data, 2);
}

#ifdef NON_MATCHING
/* Enter the selection screen in a mode: upload every portrait once, set
 * the pages' entry counts and labels, and reset both sides.
 * Does not match: the original reloads 4 into the branch delay slot. */
void func_80080780(s32 mode) {
    GridCell *cell;
    s32 i;
    s32 count;

    if (D_80092940 == 0) {
        func_80028A60(0);
        cell = D_8009270C;
        for (i = 0; i < 49; i++, cell++) {
            func_80044894(&cell->clut_x, D_800928D8 + (i << 12));
            func_80044894(&cell->image_x, D_800928D8 + (i << 12) + 0x100);
        }
        func_800320E8(D_800928D8);
        D_80092940 = 1;
    }
    D_800928C8 = mode;
    count = 4;
    if (mode == 4) {
        count = 3;
    }
    D_800915AC[5].count = count;
    D_800915AC[6].count = 5;
    if (mode == 3) {
        D_80091369 = 0x27;
        D_80091391 = 0x28;
    } else {
        D_80091369 = 0x25;
        D_80091391 = 0x26;
    }
    func_80083C0C(1);
    D_80092710 = 0;
    D_80092728 = 0;
    D_80092724 = 0;
    D_8009271C = 0;
    D_80092718 = 0;
    D_80092714 = D_80092700;
    D_80092720 = D_80092704;
    func_80080964(5);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080780);
#endif

void func_800808F4(void) {
    D_80092924 = 1;
    func_8007F834();
}

void func_80080920(void) {
    D_80092924 = 1;
    func_800719F0();
    func_8008509C(0, 0);
    func_8008509C(1, 1);
}

/* Show a page, remembering the current one; 0xff returns to it. */
void func_80080964(s32 page) {
    MenuPage *previous;

    if (page == 0xFF) {
        D_80092734 = D_80092738;
        return;
    }
    previous = D_80092734;
    D_80092734 = &D_800915AC[page];
    D_80092738 = previous;
}

/* Whether page 3 is shown. */
s32 func_800809BC(void) {
    return D_80092734 == &D_800915AC[3];
}

/* Enter the settings/system menu at page 3 with every state reset. */
void func_800809D8(void) {
    func_80039FF8();
    D_80092734 = NULL;
    func_80080964(3);
    D_800915AC[3].cursor = 0;
    D_800915AC[4].cursor = 0;
    D_800928C8 = 0;
    D_80092758 = 0;
    func_8007F834();
    D_80092924 = 0;
    func_80080AA0(0);
    D_80092940 = 0;
    D_800928D8 = func_800891C0(6);
}

void func_80080A58(void) {
    if (D_80092940 == 0) {
        func_80028A60(0);
        func_800320E8(D_800928D8);
        D_80092940 = 1;
    }
}

/* Free the loaded image data (or just forget it). */
void func_80080AA0(s32 forget) {
    if (forget) {
        D_80092760 = NULL;
    }
    if (D_80092760 != NULL) {
        func_800320E8(D_80092760);
        D_80092760 = NULL;
    }
}

/* Unpack the loaded image data and upload it to VRAM (320,256)-(640,474). */
void func_80080AE8(void) {
    s16 rect[4];

    if (D_80092760 != NULL) {
        func_800445D0(0);
        rect[0] = 0x140;
        rect[1] = 0x100;
        rect[2] = 0x140;
        rect[3] = 0xDA;
        func_8007313C(D_80092760, (u8 *)D_80092760 + 0x21E80);
        func_80044894(rect, D_80092760);
    }
}

/* Keep a copy of the shown screen: allocate the image buffer once, copy
 * the displayed buffer's area to (320,256) and read it back. */
void func_80080B58(void) {
    Rect area;

    if (D_80092760 == NULL) {
        func_80031BB4(1);
        D_80092760 = func_80031BDC(0x22100, 0);
        func_80031BB4(0);
    }
    func_800445D0(0);
    area = D_8009A0D8[(D_800928A0 + 1) & 1].area;
    func_8004495C(&area, 0x140, 0x100);
    if (D_80092760 != NULL) {
        func_800448F8(&area, D_80092760);
    }
    func_800445D0(0);
}

#ifdef NON_MATCHING
/* Open the system menu: mode 1 at page 0, mode 2 at page 7, else close.
 * Does not match: the shared tail is cross-jumped one instruction early. */
void func_80080C48(s32 mode) {
    func_80039FF8();
    func_8008EB4C(0x1F);
    if (mode == 1) {
        D_80092734 = NULL;
        func_80080964(0);
        D_800915AC[0].cursor = 0;
        D_800915AC[2].cursor = 1;
    } else if (mode == 2) {
        D_80092734 = NULL;
        func_80080964(7);
        D_800915AC[7].cursor = 1;
    } else {
        func_8007F8B4();
        return;
    }
    func_80083C0C(0);
    D_80092758 = 1;
    D_800926FC = 0;
    D_8009275C = 1;
    func_80080B58();
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080C48);
#endif

void func_80080D10(void) {
    D_800926DC = 0;
}

/* Link this frame's text quads and the menu overlay: the shown page's box
 * with its texture page and, while a page or the copy request is active, a
 * move of the kept screen copy into the draw buffer. */
void func_80080D20(void *ot) {
    PolyFT4 *quad = D_800926D4[D_800928A0];
    Rect area;
    s32 i;

    for (i = 0; i < D_800926DC; i++, quad++) {
        func_80043B48(ot, quad);
    }
    D_800926DC = 0;
    func_800811AC(ot);
    if ((D_80092734 != NULL && D_80092758 != 0) || D_800912F0 != 0) {
        if (D_80092734 != NULL) {
            func_80043B48(ot, &D_80092734->frame[D_800928A0]);
            func_80043E20(&D_800954C8[D_800928A0], 0, 0, func_80043A1C(0, 2, 0, 0));
            func_80043B48(ot, &D_800954C8[D_800928A0]);
        }
        area.x = 0x140;
        area.y = 0x100;
        area.w = 0x140;
        area.h = 0xDA;
        func_80043E4C(&D_80095498[D_800928A0], &area, D_8009A0D8[D_800928A0].area.x,
                      D_8009A0D8[D_800928A0].area.y);
        func_80043B48(ot, &D_80095498[D_800928A0]);
    }
    D_800912F0 = 0;
}

/* Set up the two semi-transparent sprite strips (at y 180 and 195) sharing
 * one pixel buffer, and their texture page. */
void func_80080F04(void) {
    u8 *pixels = func_80031BDC(0x6B4, 0);

    D_80095510[0].pixels = D_80095510[1].pixels = pixels;
    D_80095510[0].sprite[0].xy0 = 0xB40000;
    D_80095510[0].sprite[0].uv0 = 0x3000;
    func_80043D14(&D_80095510[0].sprite[0]);
    func_80043C24(&D_80095510[0].sprite[0], 1);
    D_80095510[0].sprite[0].h = 0xD;
    D_80095510[0].sprite[0].clut = D_800595D4;
    D_80095510[0].sprite[1] = D_80095510[0].sprite[0];
    D_80095510[1].sprite[0].xy0 = 0xC30000;
    D_80095510[1].sprite[0].uv0 = 0x3000;
    func_80043D14(&D_80095510[1].sprite[0]);
    func_80043C24(&D_80095510[1].sprite[0], 1);
    D_80095510[1].sprite[0].h = 0xD;
    D_80095510[1].sprite[0].clut = D_80059414;
    D_80095510[1].sprite[1] = D_80095510[1].sprite[0];
    func_80043E20(&D_80095570[0], 0, 0, func_80043A1C(0, 0, 0x140, 0x30));
    D_80095570[1] = D_80095570[0];
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081094);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081100);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800811AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800812BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800814AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008151C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008162C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8007008C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081A44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081D2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081E00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081E6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081ECC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082178);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082300);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082458);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082488);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082880);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800828C4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800828F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082A70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082C4C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082E60);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800831C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800832C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083310);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008369C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083738);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083B54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083BB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083C0C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083CD8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083CE8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083DCC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800840CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800846A0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800849E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084A40);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084A64);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084AE0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084B48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084BEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084C88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084FD0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085014);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085070);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008509C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085134);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008518C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800851D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085264);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800852C4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085E34);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085E60);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085E90);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085EAC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085EC8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800864B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800866D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800868E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80086E24);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80086E70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80086FF8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087068);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800875EC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087650);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087698);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008779C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087830);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800878DC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087AB0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087B74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087E38);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087EA0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008820C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800882D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088308);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008832C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800884E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008859C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088658);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800886FC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088754);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800887A4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088838);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800888B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800888E4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088908);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088940);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008895C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800889C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088A40);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088AF8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088BD4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088BFC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088C28);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088CBC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088D1C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_80070284);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088E90);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800891C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089210);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089330);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089534);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800896C4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008973C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008976C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800897AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800898BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089A98);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089B44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089C54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089C88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089CD8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089D5C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E64);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089E74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089EB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089F8C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089FC4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80089FF8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A040);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A0B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A0F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A110);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A128);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A140);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A168);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A184);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A254);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A298);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A2B8);

void func_8008A3A0(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A3A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A3E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A5BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A618);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A62C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A63C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A6F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A78C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A7E0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008ABAC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AC0C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AC7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AC8C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008ACB8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AE1C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008AF6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B070);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B0D8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B13C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B38C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B5DC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B5FC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B650);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B730);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BA2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BAE0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB1C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BC04);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BCC8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BD70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BE4C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C0BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C0CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C120);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C188);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C298);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C2C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C2E8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C3A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C4B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C620);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C7C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C828);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C8B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C9B8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CA00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CA84);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CC2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CC54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CCB0);

void func_8008CD54(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CD5C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CE0C);

void func_8008CED4(void) {
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CEDC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CF30);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CF9C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008CFC4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D0A4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D14C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D208);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D304);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D3F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D580);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D5C0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D680);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D980);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008D9F0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DA48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DBC0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DC28);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DCA8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DCB8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DDFC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DE54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DF30);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008DF50);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E064);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E0C8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E120);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E2B8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E3CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E620);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E67C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E6F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E78C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008E8B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EADC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EB4C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EB88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EBD0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008ECEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008ED6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EE1C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EF00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EF30);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EF74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008EFA8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F014);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F060);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F094);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F17C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F260);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F280);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F4F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F530);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F570);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F580);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F5B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F720);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F7B8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F900);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008F9B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FA2C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FACC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FBD8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FC7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FCC8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FE80);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FF24);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008FFEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090174);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090258);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8009031C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090504);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090580);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090894);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090990);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090C88);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090CC0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80090E10);
