#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "mode.h"
#include "menu.h"
#include "sprite.h"
#include "cd.h"
#include "stream.h"
#include "model.h"
#include "heap.h"
#include "text.h"
#include "pad.h"
#include "console.h"
#include "sound.h"

extern s32 D_80050108; /* texture page override: 0 none, 1 page, 2 raw */
extern s32 D_8005010C; /* CLUT override: 0 on */
extern s32 D_80059310;
extern s32 D_80059314;

/* Override model texture pages with the page at (x, y). */
void func_8002CC10(u16 x, u16 y) {
    D_80059310 = GetTPage(0, 0, x, y) & 0x1F;
    D_80050108 = 1;
}

void func_8002CC54(u16 tpage) {
    D_80059310 = tpage;
    D_80050108 = 2;
}

/* Override model CLUTs with the CLUT at (x, y). */
void func_8002CC74(u16 x, u16 y) {
    D_80059314 = GetClut(x, y) & 0xFFF0;
    D_8005010C = 0;
}

void func_8002CCAC(void) {
    D_80050108 = 0;
    D_8005010C = 1;
}

extern u16 D_80059308;
extern u16 D_8005930C;

/* Apply the texture page override to a primitive's page. */
void func_8002CCC8(u16 *tpage) {
    u16 value = *tpage;

    D_80059308 = value;
    if (D_80050108 == 1) {
        D_80059308 = value & 0xFFE0;
        D_80059308 = (value & 0xFFE0) | D_80059310;
    } else if (D_80050108 == 2) {
        D_80059308 = D_80059310;
    }
}

/* Apply the CLUT override to a primitive's CLUT. */
void func_8002CD24(u16 *clut) {
    u16 value = *clut;

    D_8005930C = value;
    if (D_8005010C == 0) {
        D_8005930C = value & 0xF;
        D_8005930C = (value & 0xF) | D_80059314;
    }
}

/* Handle a texture page (0xC4) or CLUT (0xC8) command. Returns 1 for any
 * other command. */
s32 func_8002CD64(u8 *command) {
    if ((command[3] & 0xF0) != 0xC0) {
        return 1;
    }
    switch (command[3]) {
    case 0xC4:
        func_8002CCC8((u16 *)command);
        return 0;
    case 0xC8:
        func_8002CD24((u16 *)command);
        return 0;
    }
    return 1;
}

/* Build a flat triangle's color: lit by the face normal of `vertices`
 * (flag 1; with flag 2 the color and normal are also recorded in the
 * lit-color cache), lit from the cache (flag 4), or copied. */
s32 func_8002CDCC(CVECTOR *color, s16 *vertices, s32 flags) {
    POLY_F3 *poly = (POLY_F3 *)D_80059424;
    SVECTOR normal;

    setlen(poly, 4);
    if (flags & 1) {
        if (flags & 2) {
            *D_80059498 = *(s32 *)color;
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], (SVECTOR *)++D_80059498);
            NormalColorCol((SVECTOR *)D_80059498, color, (CVECTOR *)&poly->r0);
            D_80059498 += 2;
        } else {
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], &normal);
            NormalColorCol(&normal, color, (CVECTOR *)&poly->r0);
        }
        poly->code = color->cd;
    } else if (flags & 4) {
        D_80059498++;
        NormalColorCol((SVECTOR *)D_80059498, color, (CVECTOR *)&poly->r0);
        D_80059498 += 2;
        poly->code = color->cd;
    } else {
        *(s32 *)&poly->r0 = *(s32 *)color;
    }
    return 1;
}

s32 func_8002CF34(s32 *value) {
    RenderPacket *packet = D_80059424;

    packet->code = 4;
    packet->value = *value;
    return 1;
}

/* The same flat triangle color handler for a second primitive command. */
s32 func_8002CF58(CVECTOR *color, s16 *vertices, s32 flags) {
    POLY_F3 *poly = (POLY_F3 *)D_80059424;
    SVECTOR normal;

    setlen(poly, 4);
    if (flags & 1) {
        if (flags & 2) {
            *D_80059498 = *(s32 *)color;
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], (SVECTOR *)++D_80059498);
            NormalColorCol((SVECTOR *)D_80059498, color, (CVECTOR *)&poly->r0);
            D_80059498 += 2;
        } else {
            func_8002DB84(&D_8005953C[vertices[0]], &D_8005953C[vertices[1]],
                          &D_8005953C[vertices[2]], &normal);
            NormalColorCol(&normal, color, (CVECTOR *)&poly->r0);
        }
        poly->code = color->cd;
    } else if (flags & 4) {
        D_80059498++;
        NormalColorCol((SVECTOR *)D_80059498, color, (CVECTOR *)&poly->r0);
        D_80059498 += 2;
        poly->code = color->cd;
    } else {
        *(s32 *)&poly->r0 = *(s32 *)color;
    }
    return 1;
}

s32 func_8002D0C0(s32 *value) {
    RenderPacket *packet = D_80059424;

    packet->code = 5;
    packet->value = *value;
    return 1;
}

/* Build a textured quad (after any texture page/CLUT override command):
 * its color, CLUT and texture page (with the overrides) and coordinates. */
s32 func_8002D0E4(u16 *command) {
    POLY_FT4 *poly;

    if (func_8002CD64((u8 *)command) == 0) {
        return 0;
    }
    poly = (POLY_FT4 *)D_80059424;
    setlen(poly, 9);
    *(s32 *)&poly->r0 = *(s32 *)command;
    *(s32 *)&poly->u0 = command[2] | (D_8005930C << 16);
    *(s32 *)&poly->u1 = command[3] | (D_80059308 << 16);
    *(u16 *)&poly->u2 = command[4];
    *(u16 *)&poly->u3 = command[5];
    return 1;
}

/* Build a Gouraud quad: each corner's color lit by its vertex normal. */
s32 func_8002D180(CVECTOR *color, s16 *vertices) {
    POLY_G4 *poly = (POLY_G4 *)D_80059424;

    setlen(poly, 8);
    NormalColorCol3(&D_8005952C[vertices[0]], &D_8005952C[vertices[1]], &D_8005952C[vertices[2]],
                    color, (CVECTOR *)&poly->r0, (CVECTOR *)&poly->r1, (CVECTOR *)&poly->r2);
    NormalColorCol(&D_8005952C[vertices[3]], color, (CVECTOR *)&poly->r3);
    poly->code = color->cd;
    return 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002D244);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002D354);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002D420);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002D530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002D6AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002D77C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002D814);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002D984);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002DA14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002DAFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002DB84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002DC9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002DD20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002DDE4);

/* The shared unpack buffer. */
u8 *func_8002DFE0(void) {
    return D_8006FAF0;
}

extern s32 D_800500F8;
extern s32 D_800500FC;

void func_8002DFF0(s32 a, s32 b) {
    D_800500FC = (b - 1) << 16;
    D_800500F8 = a;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002E010);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002E448);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002E64C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002E8B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002EAB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002ED20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002EEF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002F0E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002F2E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002F4B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002F6B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002F8D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002FAE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002FCFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8002FF0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8003014C);

/* Copy the vertices listed in `indices` (last first) from `in` to `out`. */
void func_800301C8(SVECTOR *out, SVECTOR *in, s32 count, s16 *indices) {
    s32 i;
    s32 k;

    for (i = count - 1; i != -1; i--) {
        k = indices[i];
        out[k].vx = in[k].vx;
        out[k].vy = in[k].vy;
        out[k].vz = in[k].vz;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030228);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800302D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800303C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800305D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800306D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030988);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030A30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030B14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030C40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030C78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030C98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80030EE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8003101C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800315A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800315C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800315E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8003160C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031630);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031654);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031678);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8003169C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800316C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800316E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031708);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8003172C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031774);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031798);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800317BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_800317E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031804);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031828);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_8003184C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002CC10", func_80031870);
