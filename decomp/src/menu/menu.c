#include "menu.h"
#include "window.h"
#include "gte.h"

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

/* Render a caption's text into its image and centre it on the screen. */
void func_80081094(Caption *caption, s32 text, s32 arg) {
    s32 width;

    width = func_80034EAC(func_80033728(D_80092880, text), caption->image, 0x3F, arg);
    caption->width = width;
    caption->x = (0x140 - width) / 2;
}

/* Show a text in the upper (0) or lower (1) caption; re-render only when
 * the text changes. */
void func_80081100(s32 text, s32 lower) {
    Rect rect;

    if (lower == 0) {
        if (text == D_8009273C) {
            return;
        }
        D_8009273C = text;
        func_80081094(&D_80095510, text, 0);
    } else {
        if (text == D_80092740) {
            return;
        }
        D_80092740 = text;
        func_80081094(&D_80095540, text, 1);
    }
    rect.x = 0x140;
    rect.y = 0x30;
    rect.w = 0x42;
    rect.h = 0xD;
    func_80044894(&rect, (void *)D_80095510.image);
}

/* Link the shown captions into the ordering table. */
void func_800811AC(void *ot) {
    Caption *caption;

    if (D_8009273C != 0) {
        caption = &D_80095510;
        caption->sprite[D_800928A0].w = caption->width;
        caption->sprite[D_800928A0].x0 = caption->x;
        func_80043B48(ot, &caption->sprite[D_800928A0]);
    }
    if (D_80092740 != 0) {
        caption = &D_80095540;
        caption->sprite[D_800928A0].w = caption->width;
        caption->sprite[D_800928A0].x0 = caption->x;
        func_80043B48(ot, &caption->sprite[D_800928A0]);
    }
    if (D_8009273C | D_80092740) {
        func_80043B48(ot, D_80095570[D_800928A0]);
    }
}

/* Measure a menu's lines and size its panel around the widest one. */
void func_800812BC(Menu *menu) {
    MenuItem *item;
    Tile *panel;
    s32 i;
    s32 widest;

    widest = 0;
    for (i = 0; i < menu->count; i++) {
        item = &menu->items[i];
        item->half_width = func_8007EB6C(item->text) / 2;
        widest = (widest < item->half_width) ? item->half_width : widest;
    }
    panel = &menu->panel[0];
    ((PacketTag *)panel)->len = 3;
    panel->w = widest * 2 + 0x14;
    panel->x0 = 0x96 - widest;
    menu->cursor = 0;
    *(u32 *)&panel->r0 = 0x60102020;
    menu->y = 0x6D - menu->count * 10;
    panel->code |= 2;
    panel->h = menu->count * 20 + 0x14;
    panel->y0 = menu->y - 10;
    for (i = 0; i < menu->count; i++) {
        item = &menu->items[i];
        if (item->flags & 2) {
            item->half_width = widest;
        }
    }
    if (menu->title_width != 0) {
        panel->w += menu->title_width;
        panel->x0 -= menu->title_width >> 1;
        menu->x = 0xA0 - (menu->title_width >> 1) - widest;
    }
    menu->panel[1] = menu->panel[0];
    func_8007EE08(0);
}

/* Lay out all eight menus and reset the menu display. */
void func_800814AC(void) {
    u32 i;

    for (i = 0; i < 8; i++) {
        func_800812BC(&D_800915AC[i]);
    }
    D_80092734 = NULL;
    D_80092700 = 0;
    D_80092704 = 1;
    func_80080F04();
}

/* Open a menu: place the cursor and draw every line. */
void func_8008151C(Menu *menu) {
    s32 i;

    if (menu == NULL) {
        return;
    }
    D_80092734 = menu;
    if (menu == &D_800915AC[5]) {
        return;
    }
    if (menu->title_width != 0) {
        func_8007E894(menu->x, menu->y);
    } else {
        func_8007E894(0xA0, menu->y);
    }
    for (i = 0; i < menu->count; i++) {
        func_8007F948(menu, i);
        if (menu->title_width != 0) {
            func_8007EBE0(menu->items[i].text);
        } else {
            func_8007ED84(menu->items[i].text, menu->items[i].half_width);
        }
    }
    if (menu->draw != NULL) {
        menu->draw(menu);
    }
}

/* One frame of menu input from a pad port: caption, stick sound, confirm,
 * cancel and cursor movement (skipping disabled lines, wrapping). Declared
 * with a value it never returns, as the unfilled final delay slot shows. */
s32 func_8008162C(Menu *menu, s32 port) {
    MenuItem *item;
    void (*handler)(s32);
    s32 type;
    s32 x;
    s32 y;

    item = &menu->items[menu->cursor];
    if (port == 0 || menu == &D_800915AC[7]) {
        func_80081100(D_80092744, 0);
        D_80092744 = menu->items[menu->cursor].caption;
    }
    D_80091364 = port;
    type = 0;
    if (port == 1) {
        D_80092748 = D_80059574;
        D_8009274C = D_800594A8;
        D_80092750 = D_80059490;
        type = func_80035734(1);
        x = D_8005943C - 0x80;
        y = D_80059434 - 0x80;
    } else if (port == 0) {
        D_80092748 = D_80059570;
        D_8009274C = D_800594A4;
        D_80092750 = D_8005948C;
        type = func_80035734(0);
        x = D_80059438 - 0x80;
        y = D_80059430 - 0x80;
    }
    if (type == 3 || type == 4) {
        if (func_80048C4C(x * x + y * y) > 0x40) {
            if (D_80092764 == 0) {
                func_8008EB4C(0x24);
                D_80092764 = 1;
            }
            goto stick_done;
        }
    }
    D_80092764 = 0;
stick_done:
    handler = item->handler;
    if (handler != NULL) {
        if (item->flags & 1) {
            handler(item->arg);
        } else if (D_80092750 & 0x20) {
            func_8008EB4C(0x21);
            handler(item->arg);
        }
    }
    if (D_80092734 != NULL) {
        if ((D_80092750 & 0x40) && !(port == 1 && menu == &D_800915AC[6])) {
            if (D_80092734 == &D_800915AC[menu->parent] && menu->parent != 5 && menu->parent != 6) {
                func_8008EB4C(0x24);
            } else {
                func_80080964(menu->parent);
                func_8008EB4C(0x22);
            }
        }
        if (D_8009274C & 0x1000) {
            func_8008EB4C(0x1E);
            if (--menu->cursor < 0) {
                menu->cursor = menu->count - 1;
            }
            if (menu->items[menu->cursor].flags & 4) {
                menu->cursor--;
            }
        }
        if (D_8009274C & 0x4000) {
            func_8008EB4C(0x1E);
            menu->cursor++;
            if (menu->items[menu->cursor].flags & 4) {
                menu->cursor++;
            }
        }
        if (menu->cursor < 0) {
            menu->cursor = menu->count - 1;
        }
        if (menu->cursor >= menu->count) {
            menu->cursor = 0;
        }
        if (menu->items[menu->cursor].flags & 4) {
            menu->cursor--;
        }
    }
}

INCLUDE_RODATA(".local/decomp/menu/asm/nonmatchings/menu", D_8007008C);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081A44);

/* One frame of the menu layer: pending refresh, the shown menu's input
 * (with the extra-speed button) and its drawing. */
void func_80081D2C(void) {
    s32 unused[2]; /* the original frame has 8 bytes of unused locals */
    Menu *menu;

    if (D_8009275C != 0) {
        func_80080AE8();
        D_8009275C = 0;
    }
    func_80036420();
    menu = D_80092734;
    if (menu == &D_800915AC[5]) {
        func_80081A44();
        return;
    }
    if (menu != NULL) {
        if ((D_8005948C & 1) && D_800926FC < 5) {
            D_800926FC++;
            func_80080AE8();
        }
        func_8008162C(menu, D_800928FC);
    }
    func_8008151C(D_80092734);
}

/* Dim the screen below the top band with the half-grey fade tiles. */
void func_80081E00(void) {
    D_8009A1C0.y0 = 0x60;
    D_8009A2B8.y0 = 0x60;
    D_8009A1C0.r0 = 0x7F;
    D_8009A1C0.g0 = 0x7F;
    D_8009A1C0.b0 = 0x7F;
    D_8009A2B8.r0 = 0x7F;
    D_8009A2B8.g0 = 0x7F;
    D_8009A2B8.b0 = 0x7F;
    D_8009A1C0.h = D_8009286C - 0x60;
    D_8009A2B8.h = D_8009286C - 0x60;
}

/* Clear the fade tiles back to the full, black screen. */
void func_80081E6C(void) {
    D_8009A1C0.y0 = 0;
    D_8009A2B8.y0 = 0;
    D_8009A1C0.r0 = 0;
    D_8009A1C0.g0 = 0;
    D_8009A1C0.b0 = 0;
    D_8009A2B8.r0 = 0;
    D_8009A2B8.g0 = 0;
    D_8009A2B8.b0 = 0;
    D_8009A1C0.h = D_8009286C;
    D_8009A2B8.h = D_8009286C;
}

/* Build the menu backdrop packets: the sky gradient quads, the backdrop
 * texture pages, the six backdrop sprites; scale the map heights and set
 * up the map drawing pools. */
void func_80081ECC(void) {
    PolyG4 *sky;
    s16 *height;
    s32 i;

    func_800875EC();
    sky = &D_80095580[0];
    ((PacketTag *)sky)->len = 8;
    sky->code = 0x38;
    sky->r0 = 0x10;
    sky->g0 = 0x60;
    sky->b0 = 0x7F;
    *(u16 *)&sky->r1 = 0x6010;
    sky->b1 = 0x7F;
    *(u16 *)&sky->r2 = 0x7F7F;
    sky->b2 = 0x7F;
    *(u16 *)&sky->r3 = 0x7F7F;
    sky->b3 = 0x7F;
    *(u32 *)&sky->x0 = 0;
    *(u32 *)&sky->x1 = 0x140;
    *(u32 *)&sky->x2 = 0x600000;
    *(u32 *)&sky->x3 = 0x600140;
    D_80095580[1] = D_80095580[0];
    func_80043E20(&D_800955C8[0], 0, 0, func_80043A1C(2, 2, 0, 0x100));
    func_80043E20(&D_800955C8[1], 0, 0, func_80043A1C(2, 2, 0, 0));
    func_80043E20(&D_800955C8[2], 0, 0, func_80043A1C(2, 2, 0x100, 0x100));
    func_80043E20(&D_800955C8[3], 0, 0, func_80043A1C(2, 2, 0x100, 0));
    ((PacketTag *)&D_800955F8[0])->len = 4;
    *(u32 *)&D_800955F8[0].r0 = 0x64707070;
    D_800955F8[0].code &= ~1; /* texture not shaded */
    D_800955F8[0].code |= 2;  /* semi-transparent */
    *(u32 *)&D_800955F8[0].x0 = 0;
    *(u16 *)&D_800955F8[0].u0 = 0;
    *(u32 *)&D_800955F8[0].w = 0xDB0080;
    func_800732AC(&D_800955F8[1], &D_800955F8[0], sizeof(Sprite) * 5);
    D_800955F8[3].u0 = 0x80;
    D_800955F8[2].u0 = 0x80;
    D_800955F8[3].x0 = 0x80;
    D_800955F8[2].x0 = 0x80;
    D_800955F8[5].x0 = 0x100;
    D_800955F8[4].x0 = 0x100;
    D_800955F8[5].w = 0x40;
    D_800955F8[4].w = 0x40;
    height = (s16 *)D_800928DC;
    for (i = 0; i < 0x4000; i++) {
        *height *= 12;
        height += 2;
    }
    func_80087830();
}

/* Draw the large direction arrow at a map position (8.8 fixed point). Does not match:
 * the start point is stored and re-read from the stack, and s5/s6 are swapped. */
#ifdef NON_MATCHING
void func_80082178(s32 x, s32 z, s32 direction) {
    Vector start;
    s32 centre_x;
    s32 centre_z;
    s32 last_x;
    s32 last_z;
    s32 next_x;
    s32 next_z;
    s32 angle;
    s32 i;

    centre_x = x >> 8;
    centre_z = z >> 8;
    last_x = start.vx = centre_x + ((func_8003F8B0(direction + 0x280) * 10) >> 12);
    last_z = start.vz = centre_z + ((func_8003F8CC(direction + 0x280) * 10) >> 12);
    angle = direction + 0x580;
    for (i = 0; i < 6; i++) {
        next_x = centre_x + ((func_8003F8B0(angle) * 24) >> 12);
        next_z = centre_z + ((func_8003F8CC(angle) * 24) >> 12);
        func_80087698(last_x, last_z, next_x, next_z);
        last_x = next_x;
        last_z = next_z;
        angle += 0x100;
    }
    next_x = centre_x + ((func_8003F8B0(direction - 0x280) * 10) >> 12);
    next_z = centre_z + ((func_8003F8CC(direction - 0x280) * 10) >> 12);
    func_80087698(last_x, last_z, next_x, next_z);
    func_80087698(start.vx, start.vz, next_x, next_z);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082178);
#endif

/* Draw the small direction arrow at a map position (8.8 fixed point). Does not match:
 * the start point is stored and re-read from the stack, and s5/s6 are swapped. */
#ifdef NON_MATCHING
void func_80082300(s32 x, s32 z, s32 direction) {
    Vector start;
    s32 centre_x;
    s32 centre_z;
    s32 last_x;
    s32 last_z;
    s32 next_x;
    s32 next_z;
    s32 angle;
    s32 i;

    centre_x = x >> 8;
    centre_z = z >> 8;
    last_x = start.vx = centre_x + ((func_8003F8B0(direction + 0x100) * 16) >> 12);
    last_z = start.vz = centre_z + ((func_8003F8CC(direction + 0x100) * 16) >> 12);
    angle = direction + 0x78A;
    for (i = 0; i < 3; i++) {
        next_x = centre_x + ((func_8003F8B0(angle) * 32) >> 12);
        next_z = centre_z + ((func_8003F8CC(angle) * 32) >> 12);
        func_80087698(last_x, last_z, next_x, next_z);
        last_x = next_x;
        last_z = next_z;
        angle += 0x75;
    }
    next_x = centre_x + ((func_8003F8B0(direction - 0x100) * 16) >> 12);
    next_z = centre_z + ((func_8003F8CC(direction - 0x100) * 16) >> 12);
    func_80087698(last_x, last_z, next_x, next_z);
    func_80087698(start.vx, start.vz, next_x, next_z);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082300);
#endif

/* Copy the stored map position. */
void func_80082458(SVector *out) {
    *out = D_80092768;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082488);

/* Ground height of the map cell under a position (cells of 256 units). Does not match:
 * two shift instructions are scheduled differently. */
#ifdef NON_MATCHING
s32 func_80082880(SVector *pos) {
    Vector unused[3]; /* the original frame has 0x30 unused bytes */
    s16 x = pos->vx >> 8;
    s16 z = pos->vz >> 8;

    return *(s16 *)&D_800928DC[z * 128 + x];
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082880);
#endif

/* The map cell word under a position (cells of 256 units). */
s32 func_800828C4(Vector *pos) {
    s32 x = pos->vx >> 8;
    s32 z = pos->vz >> 8;

    return D_800928DC[z * 128 + x];
}

/* Keep a moving position inside the circular arena of the given radius
 * around the scene centre: when the step would leave it, turn the step
 * along the rim and shorten it until the end point is inside. */
void func_800828F8(Vector *pos, Vector *step, s32 radius) {
    Vector local;
    Vector next;
    Vector square;
    Matrix rim;
    Matrix back;
    SVector dir;
    s32 distance;

    local.vx = pos->vx + step->vx - 0x3F80;
    local.vz = pos->vz + step->vz - 0x3F80;
    func_8004A414(&local, &square);
    if (radius < func_80048C4C(square.vx + square.vz)) {
        func_80048D68(&local, &dir);
        rim.m[2][1] = 0;
        rim.m[1][2] = 0;
        rim.m[1][0] = 0;
        rim.m[0][1] = 0;
        rim.m[1][1] = 0x1000;
        rim.m[2][2] = dir.vz;
        rim.m[0][0] = dir.vz;
        rim.m[0][2] = -dir.vx;
        rim.m[2][0] = dir.vx;
        func_8004947C(&rim, step, &local);
        func_8004A8EC(&rim, &back);
        func_80049EFC(&back);
        local.vz = 0;
        for (;;) {
            func_8004998C(&local, step);
            next.vx = pos->vx + step->vx - 0x3F80;
            next.vz = pos->vz + step->vz - 0x3F80;
            func_8004A414(&next, &square);
            distance = func_80048C4C(square.vx + square.vz);
            if (radius >= distance) {
                break;
            }
            local.vz -= distance - radius - 8;
        }
    }
}

/* Apply the current stage's colours: sky gradient (top and bottom), back
 * and far (fog) colours, fade tiles and the GTE primitive colour. */
void func_80082A70(void) {
    Environment *env;
    s32 top_r;
    s32 top_g;
    s32 top_b;
    s32 bottom_r;
    s32 bottom_g;
    s32 bottom_b;

    env = &D_8009178C[D_800928B4];
    D_8009288C = env;
    top_r = env->top[0];
    top_g = env->top[1];
    top_b = env->top[2];
    D_8009291C = env->unk4;
    D_80092910 = env->unk5;
    D_80092908 = env->unk6;
    bottom_r = env->bottom[0];
    bottom_g = env->bottom[1];
    bottom_b = env->bottom[2];
    func_8002C6E0(env->back[0], env->back[1], env->back[2]);
    func_8004A10C(bottom_r, bottom_g, bottom_b);
    D_80095580[0].r0 = top_r;
    D_80095580[1].r0 = top_r;
    D_80095580[0].g0 = top_g;
    D_80095580[1].g0 = top_g;
    D_80095580[0].b0 = top_b;
    D_80095580[1].b0 = top_b;
    *(u16 *)&D_80095580[0].r1 = top_r | (top_g << 8);
    D_80095580[0].b1 = top_b;
    *(u16 *)&D_80095580[1].r1 = top_r | (top_g << 8);
    D_80095580[1].b1 = top_b;
    *(u16 *)&D_80095580[0].r2 = bottom_r | (bottom_g << 8);
    D_80095580[0].b2 = bottom_b;
    *(u16 *)&D_80095580[1].r2 = bottom_r | (bottom_g << 8);
    D_80095580[1].b2 = bottom_b;
    *(u16 *)&D_80095580[0].r3 = bottom_r | (bottom_g << 8);
    D_80095580[0].b3 = bottom_b;
    *(u16 *)&D_80095580[1].r3 = bottom_r | (bottom_g << 8);
    D_80095580[1].b3 = bottom_b;
    D_8009A1C0.r0 = bottom_r;
    D_8009A1C0.g0 = bottom_g;
    D_8009A1C0.b0 = bottom_b;
    D_8009A2B8.r0 = bottom_r;
    D_8009A2B8.g0 = bottom_g;
    D_8009A2B8.b0 = bottom_b;
    func_80048AB0(0x800, 0x1800, 0xC0);
    D_80059598 = (D_80059598 & 0xFFFFFF) | 0x28000000;
    gte_ldrgb(&D_80059598);
}

/* Load the stage's floor texture (a TIM, palette made semi-transparent)
 * and build the two pools of 64 textured floor quads, alternating the two
 * halves of the texture. */
void func_80082C4C(StageFiles *files) {
    TimImage tim;
    PolyFT4 *quad;
    s16 *clut;
    s32 i;

    func_800471B4(files->floor_tim);
    func_800471C4(&tim);
    clut = (s16 *)tim.caddr;
    for (i = 0; i < 0x100; i++) {
        *clut++ |= 0x8000;
    }
    func_80044894(tim.crect, tim.caddr);
    func_80044894(tim.prect, tim.paddr);
    D_800927A0 = func_80043A58(tim.crect->x, tim.crect->y);
    D_800927A4 = func_80043A1C(1, 0, tim.prect->x, tim.prect->y);
    D_800927A8 = (u8)tim.prect->y;
    D_80092788[0] = func_80031BDC(0xA00, 0);
    D_80092788[1] = func_80031BDC(0xA00, 0);
    quad = D_80092788[0];
    for (i = 0; i < 0x40; i += 2) {
        ((PacketTag *)&quad[0])->len = 9;
        quad[0].code = 0x2C;
        ((PacketTag *)&quad[1])->len = 9;
        quad[1].code = 0x2C;
        quad->clut = D_800927A0;
        quad->tpage = D_800927A4;
        quad->u0 = 0x7F;
        quad->v0 = D_800927A8 + 0x3F;
        quad->u1 = 0x7F;
        quad->v1 = D_800927A8;
        quad->u2 = 0x3F;
        quad->v2 = D_800927A8 + 0x3F;
        quad->u3 = 0x3F;
        quad->v3 = D_800927A8;
        quad++;
        quad->clut = D_800927A0;
        quad->tpage = D_800927A4;
        quad->u0 = 0x3F;
        quad->v0 = D_800927A8 + 0x3F;
        quad->u1 = 0x3F;
        quad->v1 = D_800927A8;
        quad->u2 = 0;
        quad->v2 = D_800927A8 + 0x3F;
        quad->u3 = 0;
        quad->v3 = D_800927A8;
        quad++;
    }
    func_800732AC(D_80092788[1], D_80092788[0], 0xA00);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082E60);

/* Put the look-at point somewhere random around the scene centre and set
 * the idle camera motion parameters. */
void func_800831C8(void) {
    s32 radius;
    s32 angle;

    radius = (func_8003FA38() & 0x1FFF) + 0x800;
    angle = func_8003FA38() % 0x600 + 0x500;
    D_8009871C.vx = ((func_8003F8B0(angle) * radius) >> 12) + 0x4000;
    D_8009871C.vz = ((func_8003F8CC(angle) * radius) >> 12) + 0x4000;
    D_8009871C.vy = -((func_8003FA38() & 0x7FF) + 0x400);
    D_80092770 = 0x100;
    D_80092774 = 0x40;
    D_8009287C = 0x40;
    D_8009290C = 0x400;
}

/* Turn the idle camera with the left/right buttons. */
void func_800832C0(s32 buttons) {
    if (buttons & 0x8000) {
        D_800927AC += 0x20;
    }
    if (buttons & 0x2000) {
        D_800927AC -= 0x20;
    }
}

/* Idle orbit camera: move the eye toward a point between the two actors
 * (further toward the other actor late in the orbit, a third of the way
 * when smoothing) and swing the look-at point around it, kept inside the
 * arena and above the ground. */
void func_80083310(s32 smooth) {
    Vector look;
    Vector step;
    Vector offset;
    Vector unused;   /* the original frame has 0x18 unused bytes */
    SVector unused2;
    Actor *subject;
    Actor *other;
    s32 value; /* the other actor's share, then the orbit angle, then the ground */

    if (D_80092890 != 0) {
        subject = &D_80097010;
        other = &D_8009872C;
    } else {
        subject = &D_8009872C;
        other = &D_80097010;
    }
    func_8004B32C(subject->pos.vx - other->pos.vx, subject->pos.vz - other->pos.vz);
    if (D_800928AC > 0xB0) {
        value = 0x100;
    } else if (D_800928AC > 0xA0) {
        value = (D_800928AC - 0xA0) << 4;
    } else {
        value = 0;
    }
    offset.vx = other->pos.vx;
    offset.vy = other->pos.vy;
    offset.vz = other->pos.vz;
    offset.vx -= subject->pos.vx;
    offset.vy -= subject->pos.vy;
    offset.vz -= subject->pos.vz;
    offset.vx *= value;
    offset.vy *= value;
    offset.vz *= value;
    offset.vx /= 256;
    offset.vy /= 256;
    offset.vz /= 256;
    offset.vx += subject->pos.vx;
    offset.vy += subject->pos.vy;
    offset.vz += subject->pos.vz;
    offset.vy -= 0xA0;
    offset.vx -= D_8009867C.vx;
    offset.vy -= D_8009867C.vy;
    offset.vz -= D_8009867C.vz;
    if (smooth) {
        offset.vx /= 3;
        offset.vy /= 3;
        offset.vz /= 3;
    }
    D_80092770 = 0xC00;
    D_8009867C.vx += offset.vx;
    D_8009867C.vy += offset.vy;
    D_8009867C.vz += offset.vz;
    value = D_800927AC + D_800928AC * D_800927B0;
    look.vx = (func_8003F8B0(value) * D_80092770) >> 12;
    look.vz = (func_8003F8CC(value) * D_80092770) >> 12;
    look.vy = -(D_800928AC * 6 + 0x200);
    look.vx += D_8009867C.vx;
    look.vy += D_8009867C.vy;
    look.vz += D_8009867C.vz;
    step.vx = look.vx - D_8009871C.vx;
    step.vz = look.vz - D_8009871C.vz;
    func_800828F8(&D_8009871C, &step, 0x3D00);
    D_8009871C.vx += step.vx;
    D_8009871C.vz += step.vz;
    value = func_80082488(&D_8009871C, 0);
    if (value < look.vy) {
        look.vy = value;
    }
    D_8009871C.vy = look.vy;
}

/* Start an idle camera orbit at a random angle, speed and direction. */
void func_8008369C(void) {
    D_800927AC = func_8003FA38();
    D_800927B0 = func_8003FA38() % 12 + 4;
    if (func_8003FA38() & 1) {
        D_800927B0 = -D_800927B0;
    }
    func_80083310(0);
}

/* Frame two actors: put the eye between them, pick the side of the pair
 * the look-at point is nearer to, and move the look-at point toward a spot
 * beside the pair (further back when they are far apart), kept inside the
 * arena and above the ground. */
void func_80083738(Actor *first, Actor *second) {
    Vector side;
    Vector other_side;
    Vector unused[2]; /* the original frame has 0x20 unused bytes */
    s32 heading;
    s32 distance;
    s32 angle;
    s32 value; /* the second angle, then a side's distance, then the ground */

    heading = func_8004B32C(first->pos.vx - second->pos.vx, first->pos.vz - second->pos.vz);
    distance = func_800887A4(&first->pos, &second->pos);
    angle = heading - 0x400;
    D_80092770 = distance * 2 / 3 + 0xC0;
    D_8009867C.vx = (first->pos.vx + second->pos.vx) / 2;
    D_8009867C.vy = (first->pos.vy + second->pos.vy) / 2 - 0xA0;
    D_8009867C.vz = (first->pos.vz + second->pos.vz) / 2;
    side.vx = D_8009867C.vx + ((func_8003F8B0(angle) * D_80092770) >> 12);
    side.vz = D_8009867C.vz + ((func_8003F8CC(angle) * D_80092770) >> 12);
    value = heading + 0x400;
    other_side.vx = D_8009867C.vx + ((func_8003F8B0(value) * D_80092770) >> 12);
    other_side.vz = D_8009867C.vz + ((func_8003F8CC(value) * D_80092770) >> 12);
    side.vx -= D_8009871C.vx;
    side.vy -= D_8009871C.vy;
    side.vz -= D_8009871C.vz;
    other_side.vx -= D_8009871C.vx;
    other_side.vy -= D_8009871C.vy;
    other_side.vz -= D_8009871C.vz;
    value = func_80088754(&side);
    if (func_80088754(&other_side) < value) {
        D_8009290C = 0x400;
        D_800928F4 = 0;
    } else {
        D_8009290C = -0x400;
        D_800928F4 = 1;
    }
    distance /= 4;
    if (distance > 0x300) {
        distance = 0x300;
    }
    side.vy = D_8009867C.vy - D_80092774 - distance;
    side.vx = D_8009867C.vx + ((func_8003F8B0(heading + D_8009290C) * D_80092770) >> 12);
    side.vz = D_8009867C.vz + ((func_8003F8CC(heading + D_8009290C) * D_80092770) >> 12);
    value = func_80082488(&side, 0) - 0x100;
    if (value < side.vy) {
        side.vy = value;
    }
    side.vx = (side.vx - D_8009871C.vx) / D_8009287C;
    side.vy = (side.vy - D_8009871C.vy) / D_8009287C;
    side.vz = (side.vz - D_8009871C.vz) / D_8009287C;
    D_8009277C = heading;
    func_800828F8(&D_8009871C, &side, 0x3D00);
    D_8009287C = 100;
    D_8009871C.vx += side.vx;
    D_8009871C.vy += side.vy;
    D_8009871C.vz += side.vz;
}

/* Read the camera's look-at point and eye. */
void func_80083B54(Vector *look, Vector *eye) {
    *look = D_8009871C;
    *eye = D_8009867C;
}

/* Clear the display area (one or both 320-wide buffers) and wait. */
void func_80083BB4(s32 both) {
    Rect rect;

    rect.x = 0;
    rect.y = 0;
    if (both) {
        rect.w = 0x280;
    } else {
        rect.w = 0x140;
    }
    rect.h = 0x1E0;
    func_80044764(&rect, 0, 0, 0);
    func_800445D0(0);
}

/* Enter a camera/scene mode, running its setup. */
void func_80083C0C(s32 mode) {
    D_80092794 = mode;
    switch (mode) {
    case 3:
        func_80081E6C();
        break;
    case 4:
        func_8007A21C(D_8009294C);
        break;
    case 8:
        func_8007AC3C();
        break;
    case 6:
        if (D_8009872C.unkF2 < D_80097010.unkF2) {
            func_800725B0(&D_80097010);
        } else {
            func_800725B0(&D_8009872C);
        }
        break;
    }
}

/* The scene state word. */
s32 func_80083CD8(void) {
    return D_80092790;
}

/* Draw the elapsed time (frames at 30 per second) as minutes, seconds and
 * hundredths. Does not match:
 * the minutes are computed into another register and copied. */
#ifdef NON_MATCHING
void func_80083CE8(void) {
    char text[32];
    s32 minutes;
    s32 seconds;

    minutes = D_80092944 / 1800;
    seconds = D_80092944 - minutes * 1800;
    func_8003FBF8(text, "%02d'%02d''%02d", minutes, seconds / 30, D_80092944 % 30 * 99 / 30);
    func_8007EBE0((s32)text);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083CE8);
#endif

/* Update an actor's glow light (fading it) at its position relative to its
 * opponent, and the spot light at its position relative to the camera. */
void func_80083DCC(LightSet *set, Actor *actor, s32 index) {
    Vector unused[2]; /* the original frame has 0x20 unused bytes */
    LightRef *ref = set->lights[index];
    u8 glow = actor->glow;
    s32 level = glow;

    if (level != 0) {
        actor->glow = glow - 0x18;
        if (level < actor->glow) {
            actor->glow = 0;
        }
    }
    if (actor->unkD4 & 0x20) {
        ref->data->r = actor->opponent->unk15D4[0] * level / 16;
        ref->data->g = actor->opponent->unk15D4[1] * level / 16;
        ref->data->b = actor->opponent->unk15D4[2] * level / 16;
    } else {
        ref->data->r = level << 4;
        ref->data->g = level << 3;
        ref->data->b = 0;
    }
    if (D_8009288C->dim) {
        ref->data->r /= 2;
        ref->data->g /= 2;
        ref->data->b /= 2;
    }
    ref->data->x = actor->pos.vx;
    ref->data->y = actor->pos.vy;
    ref->data->z = actor->pos.vz;
    ref->data->x -= actor->opponent->pos.vx;
    ref->data->y -= actor->opponent->pos.vy;
    ref->data->z -= actor->opponent->pos.vz;
    func_80030A30(index, ref->data);
    ref = set->lights[2];
    ref->data->r = ref->data->g = ref->data->b = 0;
    ref->data->x = actor->pos.vx;
    ref->data->y = actor->pos.vy;
    ref->data->z = actor->pos.vz;
    ref->data->x -= D_8009871C.vx;
    ref->data->y -= D_8009871C.vy;
    ref->data->z -= D_8009871C.vz;
    func_80030A30(2, ref->data);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800840CC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800846A0);

/* Draw a 3D panel: update it, link this buffer's packets, finish. */
s32 func_800849E0(PanelOwner *owner) {
    Panel *panel = owner->panel;

    func_8008AC0C(panel);
    func_80080D20(&panel->buffers[D_800928A0]->unk4);
    func_8008AE1C(panel);
    func_80086E24();
    return 0;
}

/* Update a 3D panel without drawing it. */
void func_80084A40(PanelOwner *owner) {
    func_8008AC0C(owner->panel);
}

/* Draw a 3D panel with its shading packet at brightness 0xC0. */
s32 func_80084A64(PanelOwner *owner) {
    Panel *panel = owner->panel;

    func_8008E3CC(&panel->buffers[D_800928A0]->unk8, 0xC0, 0);
    func_80080D20(&panel->buffers[D_800928A0]->unk4);
    func_8008AE1C(panel);
    func_80086E24();
    return 0;
}

/* Draw the fading overlay while a fade is running. */
void func_80084AE0(void) {
    if (D_80092780 != 0) {
        func_8008E120();
        func_8007F258(D_80092938, 0);
        func_80080D20(D_80092938);
        func_8008E3CC(D_80092938, 0xC0, 1);
        func_8008BC04();
    }
}

/* Step the overlay fade down by 4; when it ends, reset it. */
void func_80084B48(void) {
    if (D_80092780 != 0) {
        D_80092780 -= 4;
        if (D_80092780 <= 0) {
            D_80092780 = 0;
            D_80092784 = 0;
            func_8003A838(D_80092948, 0x100, 0);
            D_8009292C = 0x100;
            func_8008E064();
        } else {
            func_8008E3CC(D_80092938, D_80092780, 1);
        }
    } else {
        D_80092784 = 0;
    }
}

/* Attach an extra object (model D_80091FB0) to the actor's model, turned
 * by (0, 0xC00, 0x400). */
void func_80084BEC(Actor *actor) {
    void *parent = actor->model->next->next->unk30;
    SceneObject *object = func_80089C54();
    void *part = func_80089FC4();

    func_80089E2C(object, part);
    func_8008A184(part, D_80091FB0);
    func_80089C88(parent, object);
    object->rotation.vy = 0xC00;
    object->rotation.vx = 0;
    object->rotation.vz = 0x400;
}

/* Set up an actor from its loaded model file on one side of the scene:
 * opponent link, model object, kind flags from the model id, part counts,
 * and the palette/emblem images in VRAM (mirrored for side 0). */
void func_80084C88(Actor *actor, ModelData *data, s32 side) {
    Rect rect;
    void *block; /* the model object, later the mirrored emblem */
    ModelHeader *header;
    u8 *source;
    s32 i;
    s32 j;
    u8 *out;
    s32 row;

    actor->flags = (actor->flags & ~0x08000000) | ((side & 1) << 27);
    if (side) {
        func_8008A140(0x380, 0, 0, 0x1FE);
        actor->opponent = &D_8009872C;
    } else {
        func_8008A140(0x3C0, 0, 0, 0x1FF);
        actor->opponent = &D_80097010;
    }
    func_8008AF6C(data);
    block = func_8008B38C(data);
    actor->object = func_80089C54();
    func_80089C88(actor->object, block);
    actor->model = block;
    actor->record = &D_80092874[actor->model_id];
    actor->kind = 0;
    switch (actor->model_id) {
    case 36:
    case 37:
        actor->kind |= 1;
    case 38:
        actor->kind |= 2;
        break;
    case 3:
    case 14:
    case 27:
    case 34:
    case 35:
    case 39:
    case 41:
    case 42:
        actor->kind |= 4;
        break;
    case 13:
        func_8008A168();
        func_80084BEC(actor);
        break;
    }
    func_8008E6F8(actor);
    header = data->header;
    actor->header = header;
    actor->unk7C = data->unk14;
    actor->parts = data->parts;
    actor->unk900 = (u8 *)header + 0x34;
    actor->unk904 = (u8 *)(header->unk30 + (s32)header);
    actor->unk908 = header->unkE;
    actor->unk15D4[0] = header->unk10[0];
    actor->unk15D4[1] = header->unk10[1];
    actor->unk15D4[2] = header->unk10[2];
    actor->parts_a = 0;
    actor->parts_b = 0;
    for (i = 0; i < 14; i++) {
        if (actor->record->parts[i]) {
            if (actor->parts[i][3]) {
                actor->parts_a++;
            } else {
                actor->parts_b++;
            }
        }
    }
    row = 0x100;
    rect.x = 0;
    rect.y = side + 0x1F6;
    rect.w = row;
    rect.h = 1;
    func_80044894(&rect, data->image);
    rect.x = side * 16 + 0x380;
    rect.y = row;
    rect.w = 0xB;
    rect.h = 0x16;
    if (side) {
        func_80044894(&rect, data->image + 0x200);
    } else {
        source = data->image + 0x200;
        block = func_80031BDC(0x1E4, 0);
        out = block;
        for (i = 0; i < 0x16; i++) {
            for (j = 0; j < 0x16; j++) {
                *out++ = source[0x15 - j];
            }
            source += 0x16;
        }
        func_80044894(&rect, block);
        func_80032C18(block, 1);
    }
    rect.x = 0x3A0;
    rect.y = side * 8 + 0x100;
    rect.w = 0x10;
    rect.h = 8;
    func_80044894(&rect, data->image + 0x3E4);
}

/* While the overlay fade runs, redraw it when button bit 0 is down. */
void func_80084FD0(void) {
    if (D_80092784 != 0 && (D_80059488 & 1)) {
        func_8008E120();
    }
}

/* Leave the menu screen for scene mode 5. */
void func_80085014(void) {
    D_80092920 &= ~1;
    func_80083BB4(0);
    func_80083C0C(5);
    D_80099D9E = 1;
    D_80099D9D = 1;
    D_800928C8 = 6;
}

/* Return from scene mode 5 to the menu screen. */
void func_80085070(void) {
    D_80099D9E = 0;
    D_80099D9D = 0;
    D_80092920 |= 1;
}

/* Give actor slot `which` (1 = D_80097010, 0 = D_8009872C) a new model
 * id and load its model, replacing the previous one. */
void func_8008509C(s32 which, s32 id) {
    if (which != 0) {
        D_80097010.model_id = id;
    } else {
        D_8009872C.model_id = id;
    }
    if (D_800927B4[which] != NULL) {
        func_800320E8(D_800927B4[which]);
        D_800927B4[which] = NULL;
    }
    func_80028470(0x30, 1);
    D_800927B4[which] = func_800891C0(id + 2);
    func_80028470(0x30, 0);
}

/* Release actor slot `which`'s model. */
void func_80085134(s32 which) {
    func_80028A60(0);
    if (D_800927B4[which] != NULL) {
        func_800320E8(D_800927B4[which]);
        D_800927B4[which] = NULL;
    }
}

/* Load a resource by its file number. */
void func_8008518C(Resource *resource, s32 arg) {
    resource->data = func_80031BDC(func_800288EC(resource->file), arg);
}

/* Leave the menu mode: stop its sound and streams, wait for drawing and
 * dispatch the next mode. */
void func_800851D4(void) {
    func_8003852C(D_800927C4);
    if (D_800917F0 != 0) {
        func_80039C4C(D_80092948);
        func_800399D4(D_80092948);
    }
    func_80088A40();
    func_8001996C(1);
    func_800445D0(0);
    func_8004B54C(2);
    D_8005061C = 1;
    func_80019ACC(0);
}

/* Whether the scene is in a state that ends the menu mode. */
s32 func_80085264(void) {
    s32 done = 0;

    if (D_80092794 == 5 || D_80092794 == 7 ||
        (D_80092794 == 1 && D_800928C8 == 4 && D_8005061C == 0)) {
        done = 1;
    }
    return done;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800852C4);

/* Screen position of the left-hand gauge for a layout point. */
void func_80085E34(DVector *point, DVector *out) {
    out->vx = point->vx + 0x18;
    out->vy = point->vy + 6;
    out->vx += 0x4F;
}

/* Screen position of the right-hand (mirrored) gauge for a layout point. */
void func_80085E60(DVector *point, DVector *out) {
    out->vx = 0x8B - point->vx;
    out->vy = 0x20 - point->vy;
    out->vx += 0x4F;
}

/* Gauge x for a side (nonzero = mirrored). */
void func_80085E90(s32 mirrored, s16 *out, s32 x) {
    if (mirrored) {
        *out = 0xDA - x;
    } else {
        *out = x + 0x67;
    }
}

/* Gauge y for a side (nonzero = mirrored). */
void func_80085EAC(s32 mirrored, s16 *out, s32 y) {
    if (mirrored) {
        *out = 0x20 - y;
    } else {
        *out = y + 6;
    }
}

/* Build one buffer's overlay packets: texture page modes, the frame
 * outlines and gauge quads of both sides (left from the corner layout,
 * right mirrored), the arrow triangles and the marks. The mirror loop runs
 * over six arrows and so also writes three past the array into the marks,
 * which are set afterwards. Does not match:
 * the arrow mirroring and final mark stores are scheduled differently. */
#ifdef NON_MATCHING
void func_80085EC8(OverlayBuffer *buf) {
    s32 i;

    func_80043E20(&buf->tpage[0], 0, 1, func_80043A1C(0, 2, 0, 0));
    func_80043E20(&buf->tpage[1], 0, 0, func_80043A1C(0, 1, 0, 0));
    ((PacketTag *)&buf->frame[0])->len = 6;
    *(u32 *)&buf->frame[0].r0 = 0x4C000000;
    buf->frame[0].pad = 0x55555555;
    ((PacketTag *)&buf->frame[1])->len = 6;
    *(u32 *)&buf->frame[1].r0 = 0x4C000000;
    buf->frame[1].pad = 0x55555555;
    ((PacketTag *)&buf->frame[2])->len = 6;
    *(u32 *)&buf->frame[2].r0 = 0x4C000000;
    buf->frame[2].pad = 0x55555555;
    ((PacketTag *)&buf->frame[3])->len = 6;
    *(u32 *)&buf->frame[3].r0 = 0x4C000000;
    buf->frame[3].pad = 0x55555555;
    func_80085E34(&D_800917F4[0], (DVector *)&buf->frame[0].x0);
    func_80085E34(&D_800917F4[1], (DVector *)&buf->frame[0].x1);
    func_80085E34(&D_800917F4[2], (DVector *)&buf->frame[0].x2);
    func_80085E34(&D_800917F4[3], (DVector *)&buf->frame[0].x3);
    func_80085E34(&D_800917F4[3], (DVector *)&buf->frame[1].x0);
    func_80085E34(&D_800917F4[4], (DVector *)&buf->frame[1].x1);
    func_80085E34(&D_800917F4[5], (DVector *)&buf->frame[1].x2);
    func_80085E34(&D_800917F4[0], (DVector *)&buf->frame[1].x3);
    func_80085E60(&D_800917F4[0], (DVector *)&buf->frame[2].x0);
    func_80085E60(&D_800917F4[1], (DVector *)&buf->frame[2].x1);
    func_80085E60(&D_800917F4[2], (DVector *)&buf->frame[2].x2);
    func_80085E60(&D_800917F4[3], (DVector *)&buf->frame[2].x3);
    func_80085E60(&D_800917F4[3], (DVector *)&buf->frame[3].x0);
    func_80085E60(&D_800917F4[4], (DVector *)&buf->frame[3].x1);
    func_80085E60(&D_800917F4[5], (DVector *)&buf->frame[3].x2);
    func_80085E60(&D_800917F4[0], (DVector *)&buf->frame[3].x3);
    func_80043F18(&buf->frame[0], &buf->frame[1]);
    func_80043F18(&buf->frame[2], &buf->frame[3]);
    buf->frame[0].x0 = 0x1D;
    buf->frame[2].x0 = 0x121;
    ((PacketTag *)&buf->bars[0])->len = 5;
    *(u32 *)&buf->bars[0].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[1])->len = 5;
    *(u32 *)&buf->bars[1].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[2])->len = 5;
    *(u32 *)&buf->bars[2].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[3])->len = 5;
    *(u32 *)&buf->bars[3].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[4])->len = 5;
    *(u32 *)&buf->bars[4].r0 = 0x280000FF;
    ((PacketTag *)&buf->bars[5])->len = 5;
    *(u32 *)&buf->bars[5].r0 = 0x280000FF;
    func_80085E34(&D_800917F4[0], (DVector *)&buf->bars[0].x0);
    func_80085E34(&D_800917F4[7], (DVector *)&buf->bars[0].x1);
    func_80085E34(&D_800917F4[5], (DVector *)&buf->bars[0].x2);
    func_80085E34(&D_800917F4[4], (DVector *)&buf->bars[0].x3);
    func_80085E34(&D_800917F4[7], (DVector *)&buf->bars[1].x0);
    func_80085E34(&D_800917F4[6], (DVector *)&buf->bars[1].x1);
    func_80085E34(&D_800917F4[4], (DVector *)&buf->bars[1].x2);
    func_80085E34(&D_800917F4[3], (DVector *)&buf->bars[1].x3);
    func_80085E34(&D_800917F4[6], (DVector *)&buf->bars[2].x0);
    func_80085E34(&D_800917F4[1], (DVector *)&buf->bars[2].x1);
    func_80085E34(&D_800917F4[3], (DVector *)&buf->bars[2].x2);
    func_80085E34(&D_800917F4[2], (DVector *)&buf->bars[2].x3);
    func_80085E60(&D_800917F4[0], (DVector *)&buf->bars[3].x0);
    func_80085E60(&D_800917F4[7], (DVector *)&buf->bars[3].x1);
    func_80085E60(&D_800917F4[5], (DVector *)&buf->bars[3].x2);
    func_80085E60(&D_800917F4[4], (DVector *)&buf->bars[3].x3);
    func_80085E60(&D_800917F4[7], (DVector *)&buf->bars[4].x0);
    func_80085E60(&D_800917F4[6], (DVector *)&buf->bars[4].x1);
    func_80085E60(&D_800917F4[4], (DVector *)&buf->bars[4].x2);
    func_80085E60(&D_800917F4[3], (DVector *)&buf->bars[4].x3);
    func_80085E60(&D_800917F4[6], (DVector *)&buf->bars[5].x0);
    func_80085E60(&D_800917F4[1], (DVector *)&buf->bars[5].x1);
    func_80085E60(&D_800917F4[3], (DVector *)&buf->bars[5].x2);
    func_80085E60(&D_800917F4[2], (DVector *)&buf->bars[5].x3);
    func_80043E20(&buf->bar_tpage, 0, 1, func_80043A1C(0, 1, 0, 0));
    func_800732AC(buf->bars_dim, buf->bars, sizeof(buf->bars));
    func_800732AC(buf->bars_lit, buf->bars, sizeof(buf->bars));
    for (i = 0; i < 6; i++) {
        ((PacketTag *)&buf->bars_dim[i])->len = 5;
        *(u32 *)&buf->bars_dim[i].r0 = 0x28806060;
    }
    for (i = 0; i < 6; i++) {
        ((PacketTag *)&buf->bars_lit[i])->len = 5;
        *(u32 *)&buf->bars_lit[i].r0 = 0x280000FF;
    }
    *(u32 *)&buf->arrows[0][0].x0 = 0x200014;
    *(u32 *)&buf->arrows[0][0].x1 = 0x20001C;
    *(u32 *)&buf->arrows[0][0].x2 = 0x28001C;
    *(u32 *)&buf->arrows[0][1].x0 = 0x200013;
    *(u32 *)&buf->arrows[0][1].x1 = 0x290013;
    *(u32 *)&buf->arrows[0][1].x2 = 0x29001B;
    *(u32 *)&buf->arrows[0][2].x0 = 0x320013;
    *(u32 *)&buf->arrows[0][2].x1 = 0x2A0013;
    *(u32 *)&buf->arrows[0][2].x2 = 0x2A001B;
    for (i = 0; i < 6; i++) {
        ((PacketTag *)&buf->arrows[0][i])->len = 4;
        *(u32 *)&buf->arrows[0][i].r0 = 0x2000FF00;
        ((PacketTag *)&buf->arrows[1][i])->len = 4;
        *(u32 *)&buf->arrows[1][i].r0 = 0x2000FF00;
        buf->arrows[1][i].y0 = buf->arrows[0][i].y0;
        buf->arrows[1][i].y1 = buf->arrows[0][i].y1;
        buf->arrows[1][i].x0 = 0x140 - buf->arrows[0][i].x0;
        buf->arrows[1][i].y2 = buf->arrows[0][i].y2;
        buf->arrows[1][i].x1 = 0x140 - buf->arrows[0][i].x1;
        buf->arrows[1][i].x2 = 0x140 - buf->arrows[0][i].x2;
    }
    *(u32 *)&buf->marks[1].r0 = *(u32 *)&buf->marks[0].r0 = 0x28000000;
    buf->marks[0].x1 = 0x63;
    buf->marks[0].x3 = 0x5E;
    buf->marks[0].y0 = buf->marks[0].y1 = 9;
    buf->marks[0].y2 = buf->marks[0].y3 = 0x14;
    buf->marks[1].x0 = buf->marks[1].x2 = 0x122;
    buf->marks[1].x1 = 0xE3;
    buf->marks[1].x3 = 0xDE;
    buf->marks[1].y0 = buf->marks[1].y1 = 0x13;
    buf->arrows[1][1].y0--;
    buf->marks[1].y2 = buf->marks[1].y3 = buf->marks[0].x0 = buf->marks[0].x2 = 0x1E;
    buf->arrows[1][1].x2--;
    ((PacketTag *)&buf->marks[2])->len = ((PacketTag *)&buf->marks[1])->len =
        ((PacketTag *)&buf->marks[0])->len = 5;
    *(u32 *)&buf->marks[2].r0 = 0x280000FF;
    *(u32 *)&buf->marks[2].x0 = 0x320006;
    *(u32 *)&buf->marks[2].x1 = 0x36000A;
    *(u32 *)&buf->marks[2].x2 = 0x4C0006;
    *(u32 *)&buf->marks[2].x3 = 0x48000A;
    ((PacketTag *)&buf->marks[3])->len = 5;
    *(u32 *)&buf->marks[3].r0 = 0x280000FF;
    *(u32 *)&buf->marks[3].x0 = 0x32013A;
    *(u32 *)&buf->marks[3].x1 = 0x360136;
    *(u32 *)&buf->marks[3].x2 = 0x4C013A;
    *(u32 *)&buf->marks[3].x3 = 0x480136;
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085EC8);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800864B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800866D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800868E0);

/* Link this buffer's overlay packets into the overlay ordering table. */
void func_80086E24(void) {
    func_80043B48(D_80092938, &D_8009A2F8[D_800928A0].tpage[1]);
}

/* Link a gauge bar filled to `value`: the first part up to 0x38, a sloped
 * second part up to 0x48, then the third part. */
void func_80086E70(void *ot, GaugeBar *bar, s32 value, s32 mirrored) {
    s32 over;

    if (value >= 0x38) {
        func_80085E90(mirrored, &bar->parts[0].x1, 0x38);
        func_80085E90(mirrored, &bar->parts[0].x3, 0x34);
        func_80043B48(ot, &bar->parts[0]);
    } else {
        func_80085E90(mirrored, &bar->parts[0].x1, value);
        func_80085E90(mirrored, &bar->parts[0].x3, value - 4);
        func_80043B48(ot, &bar->parts[0]);
        return;
    }
    if (value >= 0x48) {
        func_80085E90(mirrored, &bar->parts[1].x1, 0x48);
        func_80085E90(mirrored, &bar->parts[1].x3, 0x40);
        func_80085EAC(mirrored, &bar->parts[1].y3, 0x10);
        func_80043B48(ot, &bar->parts[1]);
    } else {
        func_80085E90(mirrored, &bar->parts[1].x1, value);
        over = value - 0x38;
        func_80085E90(mirrored, &bar->parts[1].x3, value - (over / 4 + 4));
        func_80085EAC(mirrored, &bar->parts[1].y3, over / 2 + 8);
        func_80043B48(ot, &bar->parts[1]);
        return;
    }
    func_80085E90(mirrored, &bar->parts[2].x1, value);
    func_80085E90(mirrored, &bar->parts[2].x3, value - 8);
    func_80043B48(ot, &bar->parts[2]);
}

/* Colour a marker packet by the state of an entry: none (returns 0),
 * yellow when set, red otherwise. */
s32 func_80086FF8(s32 entry, PolyF4 *packet) {
    if (func_8008F530(entry, 0)) {
        return 0;
    }
    if (func_8008F530(entry, 1)) {
        ((PacketTag *)packet)->len = 5;
        *(u32 *)&packet->r0 = 0x2800FFFF;
    } else {
        ((PacketTag *)packet)->len = 5;
        *(u32 *)&packet->r0 = 0x280000FF;
    }
    return 1;
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087068);

/* Build the overlay packets for buffer 0 and copy them to buffer 1. */
void func_800875EC(void) {
    func_80085EC8(&D_8009A2F8[0]);
    D_8009A2F8[1] = D_8009A2F8[0];
}

/* Empty the map's row spans (left 0xFF, right 0). */
void func_80087650(void) {
    s32 row;

    for (row = 0; row < 0x80; row++) {
        D_800927CC[row] = 0;
        D_800927D0[row] = 0xFF;
    }
}

/* Widen the map's row spans along a line, clamped to each row's limits. */
void func_80087698(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 x;
    s32 step;
    s32 row;
    s32 column;
    s32 swap;

    if (y0 == y1) {
        return;
    }
    if (y1 < y0) {
        swap = x1;
        x1 = x0;
        x0 = swap;
        swap = y1;
        y1 = y0;
        y0 = swap;
    }
    x = x0 << 8;
    step = ((x1 - x0) << 8) / (y1 - y0);
    for (row = y0; row < y1; row++, x += step) {
        if (row < 0) {
            continue;
        }
        if (row >= 0x80) {
            return;
        }
        column = x >> 8;
        if (column < D_800927D0[row]) {
            if (column < D_80091834[row]) {
                column = D_80091834[row];
            }
            D_800927D0[row] = column;
        }
        if (D_800927CC[row] < column) {
            if (D_800918B4[row] + 1 < column) {
                column = D_800918B4[row] + 1;
            }
            D_800927CC[row] = column;
        }
    }
}

/* Draw the map triangles: load the map colour (as a textured-triangle
 * code) into the GTE, copy the 0x30-byte map table into the scratchpad and
 * run the triangle loop. */
void func_8008779C(s32 arg0, s32 arg1, s32 arg2) {
    D_80059598 = (D_80059598 & 0xFFFFFF) | 0x24000000;
    gte_ldrgb(&D_80059598);
    func_800732AC((void *)0x1F800120, &D_80091934, sizeof(MapTable));
    func_80072D18(arg0, arg1, arg2);
}

/* Set up the map row spans in the scratchpad and the two textured
 * triangle packet pools (0x708 triangles each). */
void func_80087830(void) {
    PolyFT3 *poly;
    s32 i;

    D_800927CC = (u8 *)0x1F800000;
    D_800927D0 = (u8 *)0x1F800080;
    D_80092854[0] = func_80031BDC(0xE100, 0);
    D_80092854[1] = func_80031BDC(0xE100, 0);
    poly = D_80092854[0];
    for (i = 0; i < 0x708; i++) {
        ((PacketTag *)poly)->len = 7;
        poly->code = 0x24;
        poly++;
    }
    func_800732AC(D_80092854[1], D_80092854[0], 0xE100);
}

/* Load the stage's icon, backdrop and extra TIM images into VRAM, noting
 * the icon and backdrop palettes and texture pages; the backdrop palette's
 * first entry is transparent and the rest semi-transparent. Does not match:
 * the icon table is walked with two pointers and the loop counter is kept. */
#ifdef NON_MATCHING
void func_800878DC(StageFiles *files) {
    TimImage tim;
    s16 *clut;
    s32 i;

    for (i = 0; i < 4; i++) {
        func_800471B4(files->icon_tims[i]);
        func_800471C4(&tim);
        D_80091934.icons[i].clut = func_80043A58(tim.crect->x, tim.crect->y);
        D_80091934.icons[i].tpage = func_80043A1C(1, 1, tim.prect->x, tim.prect->y);
        func_80044894(tim.crect, tim.caddr);
        func_80044894(tim.prect, tim.paddr);
    }
    func_800471B4(files->backdrop_tim);
    func_800471C4(&tim);
    D_800927D8 = func_80043A58(tim.crect->x, tim.crect->y);
    D_800927D4 = func_80043A1C(0, 2, tim.prect->x, tim.prect->y);
    D_800927DC = tim.prect->x << 2;
    D_800927E0 = tim.prect->y;
    clut = (s16 *)tim.caddr;
    clut[0] = 0;
    for (i = 1; i < 16; i++) {
        clut[i] |= 0x8000;
    }
    func_80044894(tim.crect, tim.caddr);
    func_80044894(tim.prect, tim.paddr);
    for (i = 0x1C; i < 0x25; i++) {
        func_800471B4(files->extra_tims[i - 0x1C]);
        func_800471C4(&tim);
        func_80044894(tim.crect, tim.caddr);
        func_80044894(tim.prect, tim.paddr);
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800878DC);
#endif

/* Build an actor's textured backdrop quad (64x64 texels) for both buffers. */
void func_80087AB0(Actor *actor) {
    PolyFT4 *quad = &actor->backdrop[0];

    *(u32 *)&quad->r0 = 0x2C101010;
    ((PacketTag *)quad)->len = 9;
    quad->code |= 2;
    quad->clut = D_800927D8;
    quad->tpage = D_800927D4;
    *(u16 *)&quad->u0 = D_800927DC | (D_800927E0 << 8);
    *(u16 *)&quad->u1 = (D_800927DC + 0x3F) | (D_800927E0 << 8);
    *(u16 *)&quad->u2 = D_800927DC | ((D_800927E0 + 0x3F) << 8);
    *(u16 *)&quad->u3 = (D_800927DC + 0x3F) | ((D_800927E0 + 0x3F) << 8);
    actor->backdrop[1] = actor->backdrop[0];
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087B74);

/* Record a position in the path list (up to 31 entries). Does not match:
 * the entry address is formed base-first and registers differ. */
#ifdef NON_MATCHING
void func_80087E38(Vector *pos) {
    PathPoint *point;

    if (D_800928F8 < 0x1F) {
        point = &D_8009A988[D_800928F8];
        point->x = pos->vx;
        point->y = pos->vy;
        D_800928F8++;
        point->z = pos->vz;
    }
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087E38);
#endif

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80087EA0);

/* Start a debug line between two points in one of eight colours (bit 0
 * blue, bit 1 red, bit 2 green). Returns the line, or NULL when all 100
 * are in use. */
Line3D *func_8008820C(Vector *from, Vector *to, s32 colour) {
    Line3D *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        line = &D_80095938[i];
        if (line->timer == 0) {
            line->timer = 1;
            line->from.vx = from->vx;
            line->from.vy = from->vy;
            line->from.vz = from->vz;
            line->to.vx = to->vx;
            line->to.vy = to->vy;
            line->to.vz = to->vz;
            line->packets[0].r0 = (colour & 2) * 0x7F;
            line->packets[0].g0 = (colour & 4) * 0x3F;
            line->packets[0].b0 = (colour & 1) * 0xFF;
            line->packets[1].r0 = (colour & 2) * 0x7F;
            line->packets[1].g0 = (colour & 4) * 0x3F;
            line->packets[1].b0 = (colour & 1) * 0xFF;
            return line;
        }
    }
    return NULL;
}

/* Start a debug line that stays for the given number of frames. */
void func_800882D4(Vector *from, Vector *to, s32 colour, s32 frames) {
    Line3D *line = func_8008820C(from, to, colour);

    if (line != NULL) {
        line->timer = frames;
    }
}

/* Stop every debug line. */
void func_80088308(void) {
    s32 i;

    for (i = 0; i < 100; i++) {
        D_80095938[i].timer = 0;
    }
}

/* Project and link every live debug line, counting its frames down. */
void func_8008832C(void *ot) {
    SVector ends[2];
    Line3D *line;
    s32 i;

    for (i = 0; i < 100; i++) {
        if (D_80095938[i].timer != 0) {
            line = &D_80095938[i];
            line->timer--;
            ends[0] = line->from;
            ends[1] = line->to;
            ends[0].vx -= D_80096FA8.vx;
            ends[0].vy -= D_80096FA8.vy;
            ends[0].vz -= D_80096FA8.vz;
            ends[1].vx -= D_80096FA8.vx;
            ends[1].vy -= D_80096FA8.vy;
            ends[1].vz -= D_80096FA8.vz;
            gte_ldv01(&ends[0], &ends[1]);
            gte_rtpt();
            gte_stsxy01(&line->packets[D_800928A0].x0, &line->packets[D_800928A0].x1);
            ((PacketTag *)&line->packets[D_800928A0])->len = 3;
            ((PacketTag *)&line->packets[D_800928A0])->code = 0x40;
            func_800316C0(ot, &line->packets[D_800928A0]);
        }
    }
}

/* Scale a vector down by the square root of its (absolute) length measure
 * and pass it on. */
void func_800884E0(Vector *vector, void *out) {
    Vector scaled = *vector;
    s32 square;
    s32 length;

    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = func_80048C4C(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    func_80048D7C(&scaled, out);
}

/* Scale a vector down by the square root of its (absolute) length measure
 * and pass it to func_80048D68. */
void func_8008859C(Vector *vector, void *out) {
    Vector scaled = *vector;
    s32 square;
    s32 length;

    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = func_80048C4C(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    func_80048D68(&scaled, out);
}

/* The same for a short vector. */
void func_80088658(SVector *vector, void *out) {
    Vector scaled;
    s32 square;
    s32 length;

    scaled.vx = vector->vx;
    scaled.vy = vector->vy;
    scaled.vz = vector->vz;
    square = func_8002DC9C(scaled.vx, scaled.vy, scaled.vz);
    if (square < 0) {
        square = -square;
    }
    length = func_80048C4C(square);
    scaled.vx /= length;
    scaled.vy /= length;
    scaled.vz /= length;
    func_80048D68(&scaled, out);
}

/* Length of a vector. */
s32 func_800886FC(Vector *vector) {
    Vector square;

    gte_ldlvl(vector);
    gte_sqr0();
    gte_stlvnl(&square);
    return func_80048C4C(square.vx + square.vy + square.vz);
}

/* Horizontal (x/z) length of a vector. */
s32 func_80088754(Vector *vector) {
    Vector square;

    gte_ldlvl(vector);
    gte_sqr0();
    gte_stlvnl(&square);
    return func_80048C4C(square.vx + square.vz);
}

/* Distance between two points. */
s32 func_800887A4(Vector *from, Vector *to) {
    Vector delta;

    delta.vx = to->vx - from->vx;
    delta.vy = to->vy - from->vy;
    delta.vz = to->vz - from->vz;
    gte_ldlvl(&delta);
    gte_sqr0();
    gte_stlvnl(&delta);
    return func_80048C4C(delta.vx + delta.vy + delta.vz);
}

/* Horizontal (x/z) distance between two points. */
s32 func_80088838(Vector *from, Vector *to) {
    Vector delta;

    delta.vx = to->vx - from->vx;
    delta.vz = to->vz - from->vz;
    gte_ldlvl(&delta);
    gte_sqr0();
    gte_stlvnl(&delta);
    return func_80048C4C(delta.vx + delta.vz);
}

/* Set a bit of the resident flag array. Does not match:
 * the constant 1 is loaded first and registers differ. */
#ifdef NON_MATCHING
void func_800888B0(s32 flag) {
    D_8006F978[flag >> 3] |= 1 << (flag & 7);
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800888B0);
#endif

/* Test a bit of the resident flag array. Does not match:
 * the constant 1 is loaded first. */
#ifdef NON_MATCHING
s32 func_800888E4(s32 flag) {
    return D_8006F978[flag >> 3] & (1 << (flag & 7));
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800888E4);
#endif

/* Clear a bit of the resident flag array. Does not match:
 * the constant 1 is loaded first and registers differ. */
#ifdef NON_MATCHING
void func_80088908(s32 flag) {
    D_8006F978[flag >> 3] &= ~(1 << (flag & 7));
}
#else
INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80088908);
#endif

/* Set bit 16 of the resident state word. */
void func_80088940(void) {
    s32 *state = &D_8006F980;

    *state |= 0x10000;
}

/* Once bit 16 of the resident state word is set, queue the D_80091A6C
 * entry (only once). */
void func_8008895C(void) {
    if ((D_8006F980 & 0x10000) && D_800927EC == 0) {
        D_800927EC = 1;
        D_800928EC[D_80092888++] = &D_80091A6C;
    }
}

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
