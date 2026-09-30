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
/* Menu mode entry: start up, start the mode's task, then run the frame
 * loop forever (resume the task, build one buffer while the other is
 * shown, debug meters).
 * Does not match: the buffer flip stores are scheduled differently, and the
 * original rate string is followed by two non-zero padding bytes (0x0894)
 * that a C literal cannot reproduce. */
void func_80088E90(void) {
    DispEnv disp;
    Task *task;
    s32 last;
    s32 fps;
    s32 load;

    func_80088D1C();
    task = func_8008BA2C(((void (**)(s32))func_80088BFC)[D_80050618], 0, (u32 *)0x801FE000, 0x400);
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
    func_8008BB3C(task);
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
void func_800897AC(OtPair *layer, s32 x, s32 y, s32 w, s32 h, s32 second) {
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

/* Allocate an empty model set payload at unit scale. */
ModelSet *func_80089E74(void) {
    ModelSet *set;

    func_800324B8(4);
    set = func_80031BDC(sizeof(ModelSet), 0);
    set->scale[2] = 0x1000;
    set->scale[1] = 0x1000;
    set->scale[0] = 0x1000;
    set->nodes = NULL;
    set->players = NULL;
    return set;
}

/* Free a model set's node table and players (and, when owned, the
 * players' keys). */
void func_80089EB4(ModelSet *set) {
    s32 i;

    if (set->nodes != NULL) {
        func_800320E8(set->nodes);
    }
    if (set->players != NULL) {
        if (!D_80091C2C) {
            i = set->count;
            while (--i != -1) {
                if (set->players[i].header != NULL) {
                    func_800320E8(set->players[i].keys);
                }
            }
        }
        func_800320E8(set->players);
    }
}

/* Reset a model payload: grey, nothing loaded. */
Model *func_80089F8C(Model *model) {
    model->unk4 = 1;
    model->flags = 0;
    model->packets[0] = NULL;
    model->packets[1] = NULL;
    model->unk10 = 0;
    model->file = NULL;
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
    if (model->packets[0] != NULL) {
        func_80032C18(model->packets[0], 2);
    }
    func_8002CBBC(model->file);
}

/* Pass the model texture page and CLUT positions to target, or zeros when
 * no texture page is set. */
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

/* Clear a model node's colour override. */
void func_8008A0F4(Node *node) {
    ((Model *)node->data)->flags &= ~0x10;
}

/* Set the texture page position used for loaded models (-1 = none). */
void func_8008A110(s16 x, s16 y) {
    D_80092800 = x;
    D_80092804 = y;
}

/* Set the CLUT position used for loaded models (-1 = none). */
void func_8008A128(s16 x, s16 y) {
    D_80092808 = x;
    D_8009280C = y;
}

/* Set the texture page and CLUT positions used for loaded models. */
void func_8008A140(s16 tx, s16 ty, s16 cx, s16 cy) {
    D_80092800 = tx;
    D_80092804 = ty;
    D_80092808 = cx;
    D_8009280C = cy;
}

/* Use no texture page or CLUT override for loaded models. */
void func_8008A168(void) {
    D_80092800 = D_80092808 = -1;
}

/* Load a model file into a model payload, applying the texture page and
 * CLUT overrides. */
void func_8008A184(Model *model, ModelFile *file) {
    model->file = file;
    model->unk10 = func_800303C8(file, 1);
    func_8002CB54(model->file, &model->packets[0], &model->packets[1]);
    if (D_80092800 >= 0) {
        func_8002CC54(func_80043A1C(0, 1, D_80092800, D_80092804));
    }
    if (D_80092808 >= 0) {
        func_8002CC74(D_80092808, D_8009280C);
    }
    func_8002C8CC(model->file, model->packets[0], 2);
    func_800732AC(model->packets[1], model->packets[0], model->file->unk34);
    model->flags |= 2;
}

/* Allocate a dim light pointing along zero angles. */
Light *func_8008A254(void) {
    Light *light;

    func_800324B8(0xB);
    light = func_80031BDC(sizeof(Light), 0);
    light->colour[0] = light->colour[1] = light->colour[2] = 0x10;
    light->direction[0] = light->direction[1] = light->direction[2] = 0;
    return light;
}

/* Free a payload. */
void func_8008A298(void *p) {
    func_800320E8(p);
}

/* Allocate an ordering table pair of the given length and its depth
 * shift (the length should be a power of two up to 0x4000). */
OtPair *func_8008A2B8(u16 length) {
    OtPair *pair;
    u32 *ot;
    s32 bit;

    func_800324B8(0xC);
    pair = func_80031BDC(sizeof(OtPair), 0);
    func_800324B8(0xC);
    ot = func_80031BDC(length * 8, 0);
    pair->ot[0] = ot;
    pair->ot[1] = ot + length;
    pair->flags = 1;
    pair->shift = 14;
    pair->unk0 = 0;
    pair->length = length;
    pair->last[0] = &pair->ot[0][length - 1];
    pair->last[1] = &pair->ot[1][length - 1];
    for (bit = 1; bit != length;) {
        bit <<= 1;
        if (bit > 0x4000) {
            pair->shift = 14;
            break;
        }
        pair->shift--;
    }
    return pair;
}

void func_8008A3A0(void) {
}

/* Free a holder and its resource. */
void func_8008A3A8(Holder *holder) {
    func_80032C18(holder->resource, 3);
    func_800320E8(holder);
}

/* Allocate a light rig: a root and three light nodes (key light turned
 * round, fill lights level), grey ambient, owning holder. */
LightRig *func_8008A3E0(Holder *holder) {
    LightRig *rig;

    func_800324B8(9);
    rig = func_80031BDC(sizeof(LightRig), 0);
    rig->unk0 = 0;
    rig->nodes[0] = &rig->storage[0];
    rig->nodes[1] = &rig->storage[1];
    rig->nodes[2] = &rig->storage[2];
    rig->nodes[3] = &rig->storage[3];
    func_80089B44(&rig->storage[0]);
    func_80089B44(&rig->storage[1]);
    func_80089B44(&rig->storage[2]);
    func_80089B44(&rig->storage[3]);
    func_80089E64(rig->nodes[1], func_8008A254());
    func_80089E64(rig->nodes[2], func_8008A254());
    func_80089E64(rig->nodes[3], func_8008A254());
    rig->nodes[1]->position.vy = rig->nodes[2]->position.vy = rig->nodes[3]->position.vy = -2;
    rig->nodes[1]->position.vx = 0;
    rig->nodes[1]->position.vz = 1;
    rig->nodes[2]->position.vx = -1;
    rig->nodes[2]->position.vz = -1;
    rig->nodes[3]->position.vx = 1;
    rig->nodes[3]->position.vz = -1;
    ((Light *)rig->nodes[1]->data)->direction[0] = ((Light *)rig->nodes[1]->data)->direction[1] =
        ((Light *)rig->nodes[1]->data)->direction[2] = 0x800;
    ((Light *)rig->nodes[2]->data)->direction[0] = ((Light *)rig->nodes[2]->data)->direction[1] =
        ((Light *)rig->nodes[2]->data)->direction[2] = 0;
    *(Light *)rig->nodes[3]->data = *(Light *)rig->nodes[2]->data;
    rig->holder = holder;
    rig->r = rig->g = rig->b = 0;
    rig->r = rig->g = rig->b = 0x10;
    func_8008ABAC(&rig->nodes[1]);
    return rig;
}

/* Free a light rig, its lights and its holder. */
void func_8008A5BC(LightRig *rig) {
    func_800320E8(rig->storage[1].data);
    func_800320E8(rig->storage[2].data);
    func_800320E8(rig->storage[3].data);
    func_8008A3A8(rig->holder);
    func_800320E8(rig);
}

/* Enable model colour overrides. */
void func_8008A618(void) {
    D_80092810 = 1;
}

/* Disable model colour overrides. */
void func_8008A62C(void) {
    D_80092810 = 0;
}

/* Draw a model into the current ordering table, with its colour override
 * (or grey) as the GTE back colour when overrides are enabled. */
void func_8008A63C(Model *model) {
    D_80050104 = 0;
    if (D_80092810) {
        if (model->flags & 0x10) {
            gte_SetBackColor(model->r, model->g, model->b);
        } else {
            gte_SetBackColor(0x40, 0x40, 0x40);
        }
    }
    func_8002C700(model->file, model->packets[D_800928A0], D_800928E4, model->unk4);
}

/* Set the current buffer's colour, noting whether it changed. */
void func_8008A6F8(CVector *colour) {
    CVector *current = &D_80092818[D_800928A0];

    if (colour->r == current->r && colour->g == current->g && colour->b == current->b) {
        D_80092914 = 0;
    } else {
        D_80092914 = 1;
        *current = *colour;
        current->cd = 0x20;
    }
}

/* Write the current buffer's colour into every primitive of an instance. */
void func_8008A78C(Node *node) {
    ModelPrims *prims = ((Instance *)node->data)->prims;
    s32 i = prims->count;
    ModelPrim *prim = prims->prims[D_800928A0];
    u32 colour = *(u32 *)&D_80092818[D_800928A0];

    while (--i != -1) {
        prim->colour = colour;
        prim++;
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008A7E0);

/* Load the three rig lights into the light slots. */
void func_8008ABAC(Node **lights) {
    func_80030A30(0, lights[0]->data);
    func_80030A30(1, lights[1]->data);
    func_80030A30(2, lights[2]->data);
}

/* Clear the current buffer's ordering table of a pair and make it the one
 * primitives are added to. */
void func_8008AC0C(OtPair *pair) {
    func_80044AD8(pair->ot[D_800928A0], pair->length);
    D_800928E4 = pair->ot[D_800928A0];
    D_80050100 = pair->shift;
}

/* Choose the ordering table pair to compact at the end of the frame. */
void func_8008AC7C(OtPair *pair) {
    D_80091C30 = pair;
}

/* Note the frame's start time. */
void func_8008AC8C(void) {
    D_80092820 = func_80040690(0xF2000001);
}

/* While time remains in the frame budget (frames x 240 ticks, default
 * 192), link the tags the chosen table's entries point at past runs of
 * empty primitives, from the deepest entry down to entry 4. */
void func_8008ACB8(s32 frames) {
    OtPair *pair = D_80091C30;
    s32 start;
    s32 limit;
    s32 elapsed;
    s32 i;
    u32 *entry;
    u32 *tag;

    if (pair == NULL) {
        return;
    }
    start = D_80092820;
    D_80091C30 = NULL;
    if (frames != 0) {
        limit = frames * 240;
    } else {
        limit = 0xC0;
    }
    for (i = pair->length - 1; i >= 4; i--) {
        elapsed = func_80040690(0xF2000001) - start;
        if (elapsed < 0) {
            elapsed += 0x10000;
        }
        if (elapsed > limit) {
            return;
        }
        entry = (u32 *)pair->ot[D_800928A0][i];
        tag = (u32 *)((*entry & 0xFFFFFF) - 0x80000000);
        if (TAG_LEN(tag) == 0) {
            while (i >= 5) {
                tag = (u32 *)((*tag & 0xFFFFFF) - 0x80000000);
                i--;
                if (TAG_LEN(tag) != 0) {
                    break;
                }
            }
            *entry = (*entry & 0xFF000000) | ((u32)tag & 0xFFFFFF);
        }
    }
}

/* Link a layer's table into the frame's ordering table with its area,
 * offset and background packets for the current buffer. */
void func_8008AE1C(OtPair *layer) {
    func_8008AC7C(layer);
    func_80043B84(D_80092938, layer->last[D_800928A0], layer->ot[D_800928A0]);
    if (!(layer->flags & 4)) {
        func_800453E8(layer->area[D_800928A0], &D_80092868->draw.clip);
    }
    if (!(layer->flags & 8)) {
        func_8004546C(layer->offset[D_800928A0], D_80092868->draw.ofs);
    }
    if (layer->flags & 0x10) {
        func_80043B48(D_80092938, &layer->tile[D_800928A0]);
    }
    func_80043B48(D_80092938, layer->offset[D_800928A0]);
    func_80043B48(D_80092938, layer->area[D_800928A0]);
}

/* Relocate a scene file's pointers to where it was loaded. */
SceneFile *func_8008AF6C(SceneFile *scene) {
    s32 delta = (u8 *)scene - scene->base;
    u32 i;

    scene->base = (u8 *)scene;
    scene->unk0 += delta;
    scene->unk4 += delta;
    scene->unk10 += delta;
    scene->unk14 += delta;
    scene->unk18 += delta;
    scene->unk20 += delta;
    scene->unk24 += delta;
    if (scene->target != NULL) {
        scene->target += delta;
        func_8008A040(scene->target);
    }
    if (scene->table != NULL) {
        scene->table = (u32 *)((u8 *)scene->table + delta);
        for (i = 1; i < scene->table[0] + 1; i++) {
            if (scene->table[i] != 0) {
                scene->table[i] += delta;
            }
        }
    }
    return scene;
}

/* Load and relocate a scene file. */
SceneFile *func_8008B070(s32 file) {
    SceneFile *scene;

    func_800324B8(0xA);
    scene = func_80031BDC(func_80028738(file), 0);
    func_800295D8(file, scene, 0, 0);
    func_80028A60(0);
    return func_8008AF6C(scene);
}

/* Rewind every channel of a player. */
void func_8008B0D8(Player *player) {
    s32 unused; /* never used; the original frame has these bytes */
    Channel *channel;
    s32 i;

    player->unk10 = 0;
    player->frame = 0;
    channel = player->channels;
    for (i = 0; i < player->header->channels; i++) {
        channel->hold = 0;
        channel->value = 0;
        channel->current = channel->start;
        channel->delta = 0;
        channel++;
    }
}

#ifdef NON_MATCHING
/* Bind an animation to a model set node: its constant keys and streamed
 * channels drive node angle (short way round) or 0x2C components.
 * Does not match: the data pointer's register copies (t1/s0/s1) differ. */
void func_8008B13C(u8 *data, Player *player, Node *root) {
    Node **nodes = ((ModelSet *)root->data)->nodes;
    AnimHeader *anim = (AnimHeader *)data;
    AnimRecord *record;
    Key *key;
    Channel *channel;
    Node *node;
    u32 i;

    player->header = anim;
    func_800324B8(0x10);
    key = func_80031BDC(anim->keys * sizeof(Key) + anim->channels * sizeof(Channel), 0);
    player->keys = key;
    record = anim->records;
    channel = player->channels = (Channel *)(key + anim->keys);
    for (i = 0; i < anim->keys; i++) {
        node = nodes[record->node];
        key->value = record->value;
        switch (record->kind & 0x7F) {
        case 3:
            key->target = &node->unk44.vx;
            key->angular = 1;
            break;
        case 4:
            key->target = &node->unk44.vy;
            key->angular = 1;
            break;
        case 5:
            key->target = &node->unk44.vz;
            key->angular = 1;
            break;
        case 6:
            key->target = &node->rotation.vx;
            key->angular = 0;
            break;
        case 7:
            key->target = &node->rotation.vy;
            key->angular = 0;
            break;
        case 8:
            key->target = &node->rotation.vz;
            key->angular = 0;
            break;
        }
        key++;
        record++;
    }
    for (i = 0; i < anim->channels; i++) {
        node = nodes[record->node];
        channel->current = data + record->value;
        channel->start = data + record->value;
        switch (record->kind & 0x7F) {
        case 3:
            channel->target = &node->unk44.vx;
            channel->angular = 1;
            break;
        case 4:
            channel->target = &node->unk44.vy;
            channel->angular = 1;
            break;
        case 5:
            channel->target = &node->unk44.vz;
            channel->angular = 1;
            break;
        case 6:
            channel->target = &node->rotation.vx;
            channel->angular = 0;
            break;
        case 7:
            channel->target = &node->rotation.vy;
            channel->angular = 0;
            break;
        case 8:
            channel->target = &node->rotation.vz;
            channel->angular = 0;
            break;
        }
        channel++;
        record++;
    }
    func_8008B0D8(player);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B13C);
#endif

#ifdef NON_MATCHING
/* Build a model set node tree from a model set file: a node per hierarchy
 * record (with its model, parent, angle and offset) and a player per
 * animation. Returns the root node. Does not match: register allocation
 * spills the models pointer where the original spills the animations. */
Node *func_8008B38C(ModelSetFile *file) {
    u32 i;
    u8 *models = file->models;
    u32 *hierarchy = file->hierarchy;
    u32 *animations = file->animations;
    u32 count = hierarchy[0];
    HierarchyRecord *records = (HierarchyRecord *)(hierarchy + 1);
    Node **nodes;
    ModelSet *set;
    Node *root;
    Node *node;
    Model *model;
    Player *player;

    func_8002C3E8(models);
    func_800324B8(0x12);
    nodes = func_80031BDC(count * 4, 0);
    set = func_80089E74();
    root = func_80089C54();
    func_80089E54(root, set);
    set->nodes = nodes;
    set->nodeCount = count;
    set->records = records;
    for (i = 0; i < count; i++) {
        node = nodes[i] = func_80089C54();
        if (records[i].model != -1) {
            model = func_80089FC4();
            func_80089E2C(node, model);
            func_8008A184(model, (ModelFile *)(models + records[i].model * 0x38 + 0x10));
        }
        func_80089C88(records[i].parent == -1 ? root : nodes[records[i].parent], node);
        node->unk44.vx = records[i].angle.vx;
        node->unk44.vy = records[i].angle.vy;
        node->unk44.vz = records[i].angle.vz;
        node->rotation.vx = records[i].offset[0];
        node->rotation.vy = records[i].offset[1];
        node->rotation.vz = records[i].offset[2];
    }
    if (animations != NULL) {
        func_800324B8(0x11);
        player = set->players = func_80031BDC(animations[0] * sizeof(Player), 0);
        set->count = animations[0];
        for (i = 0; i < animations[0]; i++) {
            if (animations[i + 1] != 0) {
                func_8008B13C((u8 *)animations[i + 1], player, root);
            } else {
                player->header = NULL;
            }
            player++;
        }
    }
    return root;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B38C);
#endif

/* Advance a player by some frames, snapping to the keys. */
s32 func_8008B5DC(Player *player, s32 frames) {
    return func_8008B730(player, frames, 1);
}

/* Ease an angle toward target by a fraction (1/steps) of the shorter way
 * round (angles are 12-bit). */
s16 func_8008B5FC(s32 angle, s32 target, s32 steps) {
    s32 diff;
    s16 result;

    angle &= 0xFFF;
    diff = (angle - target) & 0xFFF;
    result = angle;
    if (diff != 0) {
        if (diff < 0x800) {
            result = angle - diff / steps;
        } else {
            result = angle + (0x1000 - diff) / steps;
        }
    }
    return result;
}

/* Turn an angle toward target by a fixed step the shorter way round
 * (a random way when opposite), stopping on the target. */
s16 func_8008B650(s32 from, s32 to, s32 step) {
    s16 angle = from;
    s16 target = to;
    s32 diff = (from - to) & 0xFFF;

    if (diff != 0) {
        if (diff == 0x800 ? (func_8003FA38() & 1) : diff < 0x800) {
            angle -= step;
            if (((angle - target) & 0xFFF) > 0x800) {
                angle = target;
            }
        } else {
            angle += step;
            if (((angle - target) & 0xFFF) < 0x800) {
                angle = target;
            }
        }
    }
    return angle;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008B730);

/* Create a task running entry(arg) on its own stack of `words` words and
 * run it until it first yields. */
Task *func_8008BA2C(void (*entry)(s32), s32 arg, u32 *stack, s32 words) {
    Task *task;
    s32 i;

    func_800324B8(3);
    task = func_80031BDC(sizeof(Task), 2);
    for (i = 0; i < 32; i++) {
        task->regs[i] = 0;
    }
    task->stack = stack;
    task->regs[28] = func_800405E4();
    task->regs[31] = (u32)entry;
    task->regs[4] = arg;
    task->regs[30] = task->regs[29] = (u32)(task->stack + words);
    func_8008BB3C(task);
    return task;
}

/* Free a task. */
void func_8008BAE0(Task *task) {
    func_800320E8(task);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB00);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB1C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BB3C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BC04);

/* Set the mesh light direction (a fixed down-left vector) and light a
 * mesh's vertices. */
void func_8008BCC8(Mesh *mesh, s32 arg) {
    Vector direction;
    Vector unused; /* never used; the original frame has these bytes */

    direction.vx = -8;
    direction.vy = -8;
    direction.vz = -0x10;
    func_80048D7C(&direction, &D_8009A2C8);
    D_8009A2C8.vx <<= 4;
    D_8009A2C8.vy <<= 4;
    D_8009A2C8.vz <<= 4;
    func_8008C3A8(mesh->data, arg, mesh->count);
}

#ifdef NON_MATCHING
/* Draw a mesh's primitive groups (flag 8: quads, else triangles) into the
 * given packets and ordering table using the vertex work area. Does not match: the next-group pointer and
 * the primitive count swap registers (t0/v1 vs v1/a0). */
void func_8008BD70(Mesh *mesh, ModelPrim *prims, u32 *ot, u8 *work) {
    u8 *group;
    s32 groups = mesh->groups;
    u8 *next = mesh->groupData;

    D_80059424 = (s32)prims;
    D_80059568 = (s32)ot;
    D_8005953C = (s32)work;
    D_800595C0 += mesh->prims;
    while (D_80059528 = next, --groups != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            func_8008C620(D_80059528, ((s16 *)group)[1]);
        } else {
            func_8008C4B0(D_80059528, ((s16 *)group)[1]);
        }
        next = D_80059528 + ((s16 *)group)[1] * 8;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008BD70);
#endif

/* Build a mesh's packet buffers: a vertex work area and, per display
 * buffer, a flat grey quad (0x18 bytes) or triangle (0x14 bytes) packet
 * for every primitive. */
void func_8008BE4C(ModelPrims *mp, Mesh *mesh) {
    s32 n; /* vertex, then group counter, then packet bytes per buffer */
    s32 i;
    s32 j;
    s32 triangles;
    s32 quads;
    u8 *group;
    u8 *vertex;
    u8 *packet;

    mp->vertices = mesh->count;
    mp->count = mesh->prims;
    mp->vertexData = mesh->data;
    mp->mesh = mesh;
    D_80059528 = mesh->groupData;
    func_800324B8(0x13);
    vertex = mp->work = func_80031BDC(mp->vertices * 8, 2);
    n = mp->vertices;
    while (--n != -1) {
        ((s16 *)vertex)[1] = 0;
        vertex += 8;
    }
    func_800324B8(5);
    triangles = 0;
    quads = 0;
    n = mesh->groups;
    while (--n != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            quads += ((s16 *)group)[1];
        } else {
            triangles += ((s16 *)group)[1];
        }
        D_80059528 += ((s16 *)group)[1] * 8;
    }
    n = triangles * 0x14 + quads * 0x18;
    packet = func_80031BDC(n * 2, 2);
    mp->prims[0] = (ModelPrim *)packet;
    mp->prims[1] = (ModelPrim *)(packet + n);
    i = mesh->groups;
    D_80059528 = mesh->groupData;
    while (--i != -1) {
        group = D_80059528;
        D_80059528 = group + 4;
        if (group[0] & 8) {
            j = ((s16 *)group)[1];
            while (--j != -1) {
                TAG_LEN(packet) = 5;
                ((u32 *)packet)[1] = 0x28403030;
                packet += 0x18;
            }
        } else {
            j = ((s16 *)group)[1];
            while (--j != -1) {
                TAG_LEN(packet) = 4;
                ((u32 *)packet)[1] = 0x20403030;
                packet += 0x14;
            }
        }
        D_80059528 += ((s16 *)group)[1] * 8;
    }
    func_800732AC(mp->prims[1], mp->prims[0], n);
}

/* Make a node an instance node. */
void func_8008C0BC(Node *node, Instance *instance) {
    node->type = 5;
    node->data = instance;
}

/* Allocate an instance payload drawing source. */
Instance *func_8008C0CC(Node *source) {
    Instance *instance;

    func_800324B8(7);
    instance = func_80031BDC(sizeof(Instance), 2);
    instance->type = source->type;
    instance->unk8 = (s32)D_80092828;
    instance->source = source;
    instance->prims = NULL;
    return instance;
}

/* Free an instance payload's packet buffers. */
void func_8008C120(Instance *instance) {
    ModelPrims *prims = instance->prims;

    if (prims != NULL) {
        if (prims->work != NULL) {
            func_800320E8(prims->work);
        }
        if (prims->prims[0] != NULL) {
            func_80032C18(prims->prims[0], 2);
        }
        func_800320E8(prims);
    }
}

/* Copy a node tree as instance nodes (model sources get their own packet
 * buffers); following siblings are appended to parent. */
Node *func_8008C188(Node *source, Node *parent) {
    Node *node;
    Instance *instance;
    Mesh *mesh;

    func_800324B8(6);
    node = func_80089C54();
    instance = func_8008C0CC(source);
    func_8008C0BC(node, instance);
    if (instance->type == 1) {
        mesh = (Mesh *)((Model *)source->data)->file;
        func_800324B8(5);
        instance->prims = func_80031BDC(sizeof(ModelPrims), 2);
        func_8008BE4C(instance->prims, mesh);
    }
    D_80092824++;
    if (source->child != NULL) {
        func_80089C88(node, func_8008C188(source->child, node));
    }
    if (source->next != NULL) {
        func_80089C88(parent, func_8008C188(source->next, parent));
    }
    return node;
}

/* Copy a node tree as instances. */
Node *func_8008C298(Node *source) {
    D_80092824 = 0;
    return func_8008C188(source, NULL);
}

/* Copy a node tree as instances of itself. */
Node *func_8008C2C0(Node *source) {
    D_80092828 = source;
    return func_8008C298(source);
}

/* Draw a tree of instance nodes whose model sources are shown. */
void func_8008C2E8(Node *node) {
    Instance *instance = node->data;
    ModelPrims *prims;

    if (instance->type == 1 && !(((Model *)instance->source->data)->flags & 1)) {
        prims = instance->prims;
        func_8008BD70(prims->mesh, prims->prims[D_800928A0], D_800928E4 + 1, prims->work);
    }
    if (node->child != NULL) {
        func_8008C2E8(node->child);
    }
    if (node->next != NULL) {
        func_8008C2E8(node->next);
    }
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C3A8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C4B0);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008C620);

/* Shift a vector history: entries 4, 3 and 2 all take entry 0. */
void func_8008C7C0(SVector *history) {
    history[4] = history[0];
    history[3] = history[4];
    history[2] = history[3];
}

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
