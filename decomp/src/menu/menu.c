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
    func_80044894(&rect, D_80095510.image);
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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80081ECC);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800828F8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082A70);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80082C4C);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083310);

/* Start an idle camera orbit at a random angle, speed and direction. */
void func_8008369C(void) {
    D_800927AC = func_8003FA38();
    D_800927B0 = func_8003FA38() % 12 + 4;
    if (func_8003FA38() & 1) {
        D_800927B0 = -D_800927B0;
    }
    func_80083310(0);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083738);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80083DCC);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084BEC);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80084C88);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80085EC8);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800864B4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800866D4);

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800868E0);

/* Link this buffer's overlay packets into the overlay ordering table. */
void func_80086E24(void) {
    func_80043B48(D_80092938, D_8009A2F8[D_800928A0].packets);
}

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_80086E70);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008779C);

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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_800878DC);

/* Build a view's textured backdrop quad (64x64 texels) for both buffers. */
void func_80087AB0(View *view) {
    PolyFT4 *quad = &view->backdrop[0];

    *(u32 *)&quad->r0 = 0x2C101010;
    ((PacketTag *)quad)->len = 9;
    quad->code |= 2;
    quad->clut = D_800927D8;
    quad->tpage = D_800927D4;
    quad->uv0 = D_800927DC | (D_800927E0 << 8);
    quad->uv1 = (D_800927DC + 0x3F) | (D_800927E0 << 8);
    quad->uv2 = D_800927DC | ((D_800927E0 + 0x3F) << 8);
    quad->uv3 = (D_800927DC + 0x3F) | ((D_800927E0 + 0x3F) << 8);
    view->backdrop[1] = view->backdrop[0];
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

INCLUDE_ASM(".local/decomp/menu/asm/nonmatchings/menu", func_8008832C);

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
