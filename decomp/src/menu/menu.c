#include "menu.h"
#include "system.h"

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007B210);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007B270);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007B388);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007BACC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007BB7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007BBA0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C100);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C124);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C280);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007C880);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CAA4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CD14);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CD44);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007CF78);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D068);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D0B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D190);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D25C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D274);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D334);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D65C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D6B8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D7A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007D918);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007DB28);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007DC74);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E020);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E24C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E2D8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E31C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E3CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E528);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E574);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E624);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E634);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E894);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E8AC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E954);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007E964);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EB6C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EBE0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EC54);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007ECF0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007ED84);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EE08);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EE68);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EEE8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007EFB4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F05C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F258);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F834);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F854);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F8B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F8E4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F948);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F97C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF5C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF60);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007F9A0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007FB0C);

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8006FF7C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007FBEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007FE48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8007FF70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080054);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080090);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800800CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080108);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080144);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080180);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800801BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800801F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080234);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080268);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800802A4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008040C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080570);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080644);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080780);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800808F4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080920);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080964);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800809BC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800809D8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080A58);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080AA0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080AE8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080B58);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080C48);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080D10);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080D20);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80080F04);

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

/* Once every progress flag 0..48 except 22 is set, mark the options
 * complete and apply the unlock. */
s32 func_800889C8(void) {
    s32 flag;

    for (flag = 0; flag < 49; flag++) {
        if (flag != 22 && !func_800888E4(flag)) {
            return 0;
        }
    }
    D_8006F978.options.complete = 1;
    func_8008895C();
    return 0;
}

/* Store the current option settings in the saved options word. */
void func_80088A40(void) {
    if (D_8005061C) {
        D_8006F978.options.version = 1;
        D_8006F978.options.option4 = D_80099D98.option4;
        D_8006F978.options.option5 = D_80099D98.option5;
        D_8006F978.options.option6 = D_80099D98.option6;
        D_8006F978.options.option13 = D_80099D98.option13;
    }
}

/* Load the option settings from the saved options word, or write the
 * defaults when it was never written; a completed word clears the flags. */
void func_80088AF8(void) {
    s32 i;

    if (D_8005061C) {
        D_800927EC = 0;
        if (D_8006F978.options.version == 1) {
            D_80099D98.option4 = D_8006F978.options.option4;
            D_80099D98.option5 = D_8006F978.options.option5;
            D_80099D98.option6 = D_8006F978.options.option6;
            D_80099D98.option13 = D_8006F978.options.option13;
            if (D_8006F978.options.complete) {
                for (i = 0; i < 8; i++) {
                    D_8006F978.flags[i] = 0;
                }
            }
        } else {
            D_80099D98.option4 = 0;
            D_80099D98.option5 = 0;
            D_80099D98.option6 = 2;
            D_80099D98.option13 = 0;
            func_80088A40();
        }
    }
}

/* Refresh the progress flags, then check for completion. */
void func_80088BD4(void) {
    func_800888B0();
    func_800889C8();
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088BFC);

/* Debug counter: pad bits 0x4/0x1 step it up/down (not below zero), then
 * print it. */
void func_80088C28(void) {
    u16 pad = D_80059570;

    if (pad & 4) {
        D_800927F4++;
    }
    if (pad & 1) {
        D_800927F4--;
    }
    if (D_800927F4 < 0) {
        D_800927F4 = 0;
    }
    func_8003278C(1, D_800927F4, 10, 0x80AD);
}

/* Set up the given window record's defaults. */
void func_80088CBC(s32 index) {
    Window *window = &D_8009A0D8[index];

    window->sprite.len = 3;
    window->sprite.code = 0x7D;
    *(u16 *)&window->sprite.u0 = 0x3000;
    window->sprite.clut = func_80043A58(0x3F0, 0xC0);
}

/* Menu mode start-up: frame callback, display and windows, the start
 * state from the boot word, then the mode's first screen. */
void func_80088D1C(void) {
    s32 unused[2]; /* never used; the original frame has these 8 bytes */

    /* The callback starts one word into splat's func_80088BFC, which begins
     * with a data word. */
    func_800444D8((u8 *)func_80088BFC + 4);
    func_80048BC4();
    func_80032498(6, D_80091BB0);
    func_80028470(0x30, 0);
    func_800374E8(4, 2, 0x138, 0xDA, 0x14, 1, 0x3C0, 0x1F0, 0x3C0, 0x1EF, 0);
    func_80088CBC(0);
    func_80088CBC(1);
    switch (D_80010000) {
    case -1:
        D_800928CC = 2;
        break;
    case 0:
        D_800928CC = 1;
        break;
    default:
        D_800928CC = 0;
        break;
    }
    D_80092868 = &D_8009A0D8[0];
    D_80092870 = &D_8009A0D8[1];
    func_8008A110(-1, -1);
    func_8008A128(-1, -1);
    D_80092898 = 2;
    D_8009289C = 1;
    D_80092930 = NULL;
    D_800928E8 = 0;
    D_800928A0 = 0;
    D_80092920 = 0;
    D_80092930 = NULL;
    D_800928D0 = 7;
    func_8008E620();
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_80070284);

#ifdef NON_MATCHING
/* Menu mode entry: start up, load the mode's data, then run the frame loop
 * forever (build one buffer while the other is shown, debug meters).
 * Does not match: the buffer flip stores are scheduled differently, and the
 * original rate string is followed by two non-zero padding bytes (0x0894)
 * that a C literal cannot reproduce. */
void func_80088E90(void) {
    DispEnv disp;
    void *state;
    s32 last;
    s32 fps;
    s32 load;

    func_80088D1C();
    state = func_8008BA2C(((void **)func_80088BFC)[D_80050618], 0, (void *)0x801FE000, 0x400);
    D_80059488;
frame:
    D_800595C0 = 0;
    D_80059578 = 0;
    D_800928A0 = (D_800928E8 + 1) & 1;
    D_80092870 = &D_8009A0D8[D_800928E8 & 1];
    D_800928E8++;
    D_80092868 = &D_8009A0D8[D_800928A0];
    D_80092938 = &D_80092868->ot;
    disp = D_80092868->disp;
    func_80019CA0();
    func_80043BE4(D_80092938);
    if ((D_800928D0 & 0x10) && D_80092930 != NULL) {
        D_80092930(D_80092938);
    }
    func_80037324(D_80092938);
    func_8008BB3C(state);
    func_8008EADC();
    func_80032CB8();
    if (D_80092920 & 1) {
        func_80043B48(D_80092938, &D_80092868->background);
    }
    load = func_8004B54C(1);
    fps = 60 / (u32)(D_80059488 - last);
    last = D_80059488;
    if (D_800928D0 & 8) {
        func_80088C28();
    }
    if (D_800928D0 & 1) {
        func_8003700C("POLYGON:%4d/%4d\n", D_80059578, D_800595C0);
    }
    if (D_800928D0 & 2) {
        func_8003700C("CPU/GPU:%4d/%3d\n", load, D_800927F0);
    }
    if (D_800928D0 & 4) {
        func_8003700C("RATE   : %3dfps\n", fps);
    }
    func_80036DC8(0xFF, 0xFF, 0xFF);
    func_8008ACB8(D_80092898);
    func_8004B54C(D_80092898);
    func_8008AC8C();
    func_800445D0(0);
    func_80044E9C(&disp);
    func_80044D48(D_80092938, D_80092868);
    goto frame;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088E90);
#endif

/* Load a whole file into a new allocation and return it. */
void *func_800891C0(s32 file) {
    void *data = func_80031BDC(func_80028738(file), 1);

    func_800295D8(file, data, 0, 0);
    return data;
}

/* Set the screen scale matrices for a width x height display (320x240 is
 * unit scale). */
void func_80089210(s32 width, s32 height) {
    s32 sx = ((width << 12) / 320) * height / width;
    s32 sy = ((height << 12) / 240) * height / width;

    D_8009A2D8 = D_80091C0C;
    D_80096FE0 = D_80091C0C;
    D_80096FE0.m[0][0] = sx;
    D_80096FE0.m[1][1] = sy;
}

/* Set both buffers' display environments and background tiles for the
 * resolution; taller than 256 lines is interlaced. */
void func_80089330(s32 width, s32 height) {
    if (height > 256) {
        func_800439E0(&D_8009A0D8[0].disp, 0, 0, width, height);
        func_800439E0(&D_8009A0D8[1].disp, 0, 0, width, height);
        D_8009A0D8[1].disp.isinter = 1;
        D_8009A0D8[0].disp.isinter = 1;
        D_8009A0D8[0].disp.screen.x = 0;
        D_8009A0D8[0].disp.screen.y = 0x10;
        D_8009A0D8[0].disp.screen.w = 0x100;
        D_8009A0D8[0].disp.screen.h = 0xD4;
        D_8009A0D8[1].disp.screen.x = 0;
        D_8009A0D8[1].disp.screen.y = 0x10;
        D_8009A0D8[1].disp.screen.w = 0x100;
        D_8009A0D8[1].disp.screen.h = 0xD4;
    } else {
        func_800439E0(&D_8009A0D8[0].disp, 0, 0x100, width, height);
        func_800439E0(&D_8009A0D8[1].disp, 0, 0, width, height);
        D_8009A0D8[1].disp.isinter = 0;
        D_8009A0D8[0].disp.isinter = 0;
        D_8009A0D8[0].disp.screen.x = 0;
        D_8009A0D8[0].disp.screen.y = 0xA;
        D_8009A0D8[0].disp.screen.w = 0x100;
        D_8009A0D8[0].disp.screen.h = height;
        D_8009A0D8[1].disp.screen.x = 0;
        D_8009A0D8[1].disp.screen.y = 0xA;
        D_8009A0D8[1].disp.screen.w = 0x100;
        D_8009A0D8[1].disp.screen.h = height;
    }
    D_8009285C = width;
    D_8009286C = height;
    func_80089210(D_8009285C, D_8009286C);
    D_8009A0D8[0].background.len = 3;
    D_8009A0D8[0].background.colour = 0x60000000;
    D_8009A0D8[0].background.x0 = 0;
    D_8009A0D8[0].background.y0 = 0;
    D_8009A0D8[0].background.w = width;
    D_8009A0D8[0].background.h = height;
    D_8009A0D8[1].background = D_8009A0D8[0].background;
}

/* Set the geometry and both buffers' drawing environments for the
 * resolution. */
void func_80089534(s32 width, s32 height) {
    func_8004A12C(width / 2, height / 2);
    func_8004A14C(0x180);
    func_8002DFF0(width, height);
    if (height > 256) {
        func_80043928(&D_8009A0D8[0].draw, 0, 0, width, height);
        func_80043928(&D_8009A0D8[1].draw, 0, 0, width, height);
        D_8009A0D8[1].draw.dfe = 0;
        D_8009A0D8[0].draw.dfe = 0;
    } else {
        func_80043928(&D_8009A0D8[0].draw, 0, 0, width, height);
        func_80043928(&D_8009A0D8[1].draw, 0, 0x100, width, height);
    }
    D_8009A0D8[0].draw.dtd = D_8009A0D8[1].draw.dtd = 1;
    D_8009A0D8[0].draw.isbg = D_8009A0D8[1].draw.isbg = 0;
    D_8009A0D8[0].draw.tpage = D_8009A0D8[1].draw.tpage = func_80043A1C(0, 2, 0x280, 0);
    func_80045534(D_8009A0D8[0].draw.dr_env, &D_8009A0D8[0].draw);
    func_80045534(D_8009A0D8[1].draw.dr_env, &D_8009A0D8[1].draw);
    func_800453E8(D_8009A0D8[0].modeD0, &D_8009A0D8[0].draw.clip);
    func_800453E8(D_8009A0D8[1].modeD0, &D_8009A0D8[1].draw.clip);
    func_8004546C(D_8009A0D8[0].modeDC, D_8009A0D8[0].draw.ofs);
    func_8004546C(D_8009A0D8[1].modeDC, D_8009A0D8[1].draw.ofs);
}

/* Set up geometry, screen and scale for a width x height display. */
void func_800896C4(s32 width, s32 height) {
    func_8004A12C(width / 2, height / 2);
    func_8004A14C((width << 8) / width);
    func_8002DFF0(width, height);
    func_80089210(width, height);
}

/* Re-apply the drawing environments at the current resolution. */
void func_8008973C(void) {
    func_80089534(D_8009285C, D_8009286C);
}

/* Set the display and drawing environments for a resolution. */
void func_8008976C(s32 width, s32 height) {
    func_80089330(width, height);
    func_80089534(width, height);
}

/* Set a layer's drawing areas and offsets for both buffers (the second
 * buffer lies `second` lines lower) and its black background tiles. */
void func_800897AC(Layer *layer, s32 x, s32 y, s32 w, s32 h, s32 second) {
    Rect area;
    s16 offset[2];

    area.x = x;
    area.y = y;
    area.w = w;
    area.h = h;
    func_800453E8(layer->area[0], &area);
    area.y = y + second;
    func_800453E8(layer->area[1], &area);
    offset[0] = x;
    offset[1] = y;
    func_8004546C(layer->offset[0], offset);
    offset[1] = y + second;
    func_8004546C(layer->offset[1], offset);
    layer->tile[0].len = 3;
    layer->tile[0].colour = 0x60000000;
    layer->tile[0].x0 = 0;
    layer->tile[0].y0 = 0;
    layer->tile[0].w = w;
    layer->tile[0].h = h;
    layer->tile[1] = layer->tile[0];
    layer->flags |= 0xC;
}

/* Build a view matrix looking from eye to at with the given up vector. */
void func_800898BC(Matrix *m, SVector *eye, SVector *at, SVector *up) {
    D_8009A0C8.vx = at->vx - eye->vx;
    D_8009A0C8.vy = at->vy - eye->vy;
    D_8009A0C8.vz = at->vz - eye->vz;
    D_8009A918.vx = up->vx;
    D_8009A918.vy = up->vy;
    D_8009A918.vz = up->vz;
    func_80048D7C(&D_8009A0C8, &D_80096F98);
    func_8004A480(&D_8009A918, &D_80096F98, &D_8009A0C8);
    func_80048D7C(&D_8009A0C8, &D_80097000);
    func_8004A480(&D_80096F98, &D_80097000, &D_8009A0C8);
    func_80048D7C(&D_8009A0C8, &D_8009A918);
    m->m[0][0] = D_80097000.vx;
    m->m[0][1] = D_80097000.vy;
    m->m[0][2] = D_80097000.vz;
    m->m[1][0] = D_8009A918.vx;
    m->m[1][1] = D_8009A918.vy;
    m->m[1][2] = D_8009A918.vz;
    m->m[2][0] = D_80096F98.vx;
    m->m[2][1] = D_80096F98.vy;
    m->m[2][2] = D_80096F98.vz;
    func_80049CEC(m, eye, &D_8009A0C8);
    func_80049BDC(&D_80096FE0, m);
    m->t[0] = -D_8009A0C8.vx;
    m->t[1] = -D_8009A0C8.vy;
    m->t[2] = -D_8009A0C8.vz;
}

/* Point the owner's view from eye toward target (eye kept as the last eye
 * position). */
void func_80089A98(NodeOwner *owner, Vector *target, Vector *eye) {
    SVector up;
    SVector from;
    SVector origin;

    up.vy = 0x1000;
    up.vz = 0;
    up.vx = 0;
    D_80096FA8 = *eye;
    from.vx = target->vx - eye->vx;
    from.vy = target->vy - eye->vy;
    from.vz = target->vz - eye->vz;
    origin.vz = 0;
    origin.vx = 0;
    origin.vy = 0;
    func_800898BC(&owner->node->view, &from, &origin, &up);
}

/* Reset a node: unlinked, no payload, zero position and angles, identity
 * matrices. */
Node *func_80089B44(Node *node) {
    node->type = 0;
    node->parent = NULL;
    node->child = NULL;
    node->next = NULL;
    node->unk8 = 0;
    node->data = NULL;
    node->position.vz = 0;
    node->position.vy = 0;
    node->position.vx = 0;
    node->unk44.vz = 0;
    node->unk44.vy = 0;
    node->unk44.vx = 0;
    node->rotation.vz = 0;
    node->rotation.vy = 0;
    node->rotation.vx = 0;
    node->unk6C = D_80091C0C;
    node->unk4C = node->unk6C;
    node->view = node->unk4C;
    return node;
}

/* Allocate a reset scene node. */
Node *func_80089C54(void) {
    func_800324B8(8);
    return func_80089B44(func_80031BDC(sizeof(Node), 0));
}

/* Append child as the last child of parent. */
void func_80089C88(Node *parent, Node *child) {
    Node *last;

    child->parent = parent;
    if (parent->child == NULL) {
        parent->child = child;
    } else {
        last = parent->child;
        while (last->next != NULL) {
            last = last->next;
        }
        last->next = child;
    }
}

/* Unlink a node from its parent's child list. */
void func_80089CD8(Node *node) {
    Node *parent;
    Node *first;
    Node *prev;

    if (node == NULL) {
        return;
    }
    parent = node->parent;
    if (parent == NULL) {
        return;
    }
    node->parent = NULL;
    first = parent->child;
    if (first == NULL) {
        return;
    }
    if (first == node) {
        parent->child = node->next;
    } else {
        prev = first;
        while (prev->next != node) {
            prev = prev->next;
        }
        prev->next = node->next;
    }
    node->next = NULL;
}

/* Free a node, its children and following siblings, and its payload. */
void func_80089D5C(Node *node) {
    if (node == NULL) {
        return;
    }
    func_80089D5C(node->child);
    func_80089D5C(node->next);
    switch (node->type) {
    case 1:
        func_80089FF8(node->data);
        break;
    case 2:
        func_80089EB4(node->data);
        break;
    case 5:
        func_8008C120(node->data);
        break;
    }
    if (node->data != NULL) {
        func_800320E8(node->data);
    }
    func_800320E8(node);
}

/* Make a node a model node. */
void func_80089E2C(Node *node, Model *model) {
    node->data = model;
    node->type = 1;
}

/* Make a node a type 3 node. */
void func_80089E3C(Node *node) {
    node->type = 3;
}

/* Make a node a type 4 node. */
void func_80089E48(Node *node) {
    node->type = 4;
}

/* Make a node a model set node. */
void func_80089E54(Node *node, ModelSet *set) {
    node->data = set;
    node->type = 2;
}

/* Make a node a type 6 node. */
void func_80089E64(Node *node, void *data) {
    node->data = data;
    node->type = 6;
}

/* Allocate a unit scale payload. */
Scale *func_80089E74(void) {
    Scale *scale;

    func_800324B8(4);
    scale = func_80031BDC(sizeof(Scale), 0);
    scale->scale[2] = 0x1000;
    scale->scale[1] = 0x1000;
    scale->scale[0] = 0x1000;
    scale->unk4 = 0;
    scale->unk8 = 0;
    return scale;
}

/* Free a model set's data and, when owned, its entries' data. */
void func_80089EB4(ModelSet *set) {
    s32 i;

    if (set->unk4 != NULL) {
        func_800320E8(set->unk4);
    }
    if (set->entries != NULL) {
        if (!D_80091C2C) {
            i = set->count;
            while (--i != -1) {
                if (set->entries[i].loaded) {
                    func_800320E8(set->entries[i].data);
                }
            }
        }
        func_800320E8(set->entries);
    }
}

/* Reset a model payload: grey, nothing loaded. */
Model *func_80089F8C(Model *model) {
    model->unk4 = 1;
    model->flags = 0;
    model->resource = NULL;
    model->unkC = 0;
    model->unk10 = 0;
    model->unk14 = NULL;
    model->unk1C = 0;
    model->b = 0x40;
    model->g = 0x40;
    model->r = 0x40;
    return model;
}

/* Allocate a reset model payload. */
Model *func_80089FC4(void) {
    func_800324B8(1);
    return func_80089F8C(func_80031BDC(sizeof(Model), 0));
}

/* Release a model payload's resources. */
void func_80089FF8(Model *model) {
    if (model->resource != NULL) {
        func_80032C18(model->resource, 2);
    }
    func_8002CBBC(model->unk14);
}

/* Apply the depth cue settings to target, or switch it off when the first
 * is not positive. */
void func_8008A040(void *target) {
    if (D_80092800 > 0) {
        func_8002DDE4(target, 1, D_80092800, D_80092804, 1, D_80092808, D_8009280C);
    } else {
        func_8002DDE4(target, 0, 0, 0, 0, 0, 0);
    }
}

/* Set a model node's colour. */
void func_8008A0B4(Node *node, u8 r, u8 g, u8 b) {
    ((Model *)node->data)->r = r;
    ((Model *)node->data)->g = g;
    ((Model *)node->data)->b = b;
    ((Model *)node->data)->flags |= 0x10;
}

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
