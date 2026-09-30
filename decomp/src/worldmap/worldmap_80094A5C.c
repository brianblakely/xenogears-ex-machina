#include "worldmap.h"

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80094A5C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800951A8);

/* Orient `normal` towards `direction` on the ground plane (zero when
 * perpendicular). */
void func_800952B0(Vec3 *direction, Vec3 *out, Vec3 *normal) {
    s32 dot;

    dot = normal->vx * direction->vx + normal->vz * direction->vz;
    if (dot < 0) {
        out->vx = -normal->vx;
        out->vz = -normal->vz;
    } else if (dot > 0) {
        out->vx = normal->vx;
        out->vz = normal->vz;
    } else {
        out->vz = 0;
        out->vx = 0;
    }
    out->vy = 0;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80095324);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80095414);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80095CD4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80095F78);

/* Free the effect command buffers. */
void func_800960BC(void) {
    s32 first;
    s32 second;

    first = func_8002C3D8();
    second = func_8002C3D8();
    if ((first == 0) | (second == -1)) {
        func_800320E8(D_8009BE08);
    } else {
        func_800320E8(D_8009D3C0);
    }
    func_800320E8(D_8009D7D4);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80096130);

/* Append a three-word effect command to the current frame's list. */
s32 func_8009623C(s32 a, s32 b, s32 c) {
    EffectCommand3 *command;
    s32 count;

    count = D_8009D808;
    if (count < 0x58) {
        D_8009D808 = count + 1;
        command = (EffectCommand3 *)((u8 *)D_8009BE08 + D_8009BE44 * 0x420) + count;
        command->a = a;
        command->b = b;
        command->c = c;
        return 0;
    }
    return -1;
}

/* Append a four-word effect command to the current frame's list. */
s32 func_800962B0(s32 a, s32 b, s32 c, s32 d) {
    EffectCommand4 *command;
    s32 count;

    count = D_8009D808;
    if (count < 0x58) {
        D_8009D808 = count + 1;
        command = (EffectCommand4 *)((u8 *)D_8009D3C0 + D_8009BE44 * 0x580) + count;
        command->a = a;
        command->b = b;
        command->c = c;
        command->d = d;
        return 0;
    }
    return -1;
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80096328);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800963E4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800964B0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800965A4);

/* Frames queued between the writer and reader (ring of 16). */
s32 func_80096668(void) {
    s32 pending;

    pending = D_8009BE44 - D_8009BCB8;
    if (pending < 0) {
        pending += 0x10;
    }
    return pending;
}

/* Drain the queued frames, waiting for vertical sync between them. */
void func_80096694(void) {
    do {
        func_8004B54C(0);
        func_800967E4();
    } while (func_80096668() != 0);
}

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800966CC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800967E4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800968E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_8009699C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80096A6C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80096C0C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80096F18);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097070);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097244);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097440);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_8009766C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800976A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800976C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800976FC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097718);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097770);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800977A8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800977C4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800977E0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097800);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800978FC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800979C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097BC0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097CB8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097D64);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80097DC0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80098044);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800980D4);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800981C8);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800983A0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_800987AC);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80098CC0);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_8009932C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80099708);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_8009980C);

INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap_80094A5C", func_80099BFC);
