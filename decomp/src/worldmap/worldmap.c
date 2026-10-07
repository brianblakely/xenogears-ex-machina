#include "worldmap.h"

/* Overlay entry: set up the display, start a new game's world state if none
 * is set, enter the requested mode and run its main loop until the world
 * map is left, then hand over to the next scene. */
#ifdef NON_MATCHING /* irreducible mode loop; layout and allocation differ */
void func_80070CFC(void) {
    void (*step)(void);
    void *data;
    RECT rect;
    s32 mode;
    s32 i;

    func_800762FC();
    DrawSync(0);
    VSync(0);
    VSyncCallback(D_8003634C);
    InitGeom();
    D_800591AE = 1;
    if (D_8006F954[0] == 0) {
        D_8006F952 = 0xFFF;
        D_8006F950 = 0xC00;
        D_8006F94E = 0x400;
        D_8006F954[0] = 1;
        D_8006EE54.flags = 0x4003;
        D_8006EE54.unk60 = 0x6680;
        D_8006EE54.unk62 = 0xFF00;
        D_8006EE54.unk64 = 0x2A00;
        D_8006EE54.z = 0x2C00;
        D_8006F368[1] = 0xA;
        D_8006D940[0].gear = 0xF;
        D_8006D940[1].gear = 2;
        D_8006D940[3].gear = 4;
        D_8006F368[2] = 5;
        D_8006D940[4].gear = 5;
        D_8006D940[5].gear = 6;
        D_8006D940[7].gear = 7;
        D_8006D940[8].gear = 8;
        D_8006D940[2].gear = 3;
        D_8006D940[9].gear = 3;
        D_8006EE54.unk6A = 1;
        D_8006EE66 = 0;
        D_8006EE54.x = 0x7580;
        D_8006EE54.heading = 0;
        D_8006F368[0] = 0;
        D_8006F8E5 = 0;
        D_8006F8E6 = 0;
        D_8006F8E7 = 0;
        D_8006D940[6].gear = 9;
        D_8006D940[10].gear = 9;
        D_8006EF8E[0].flags = 0x400;
        D_8006EF8E[0].x = 0x7500;
        D_8006EF8E[0].z = 0x2E58;
        D_8006EF8E[1].flags = 0x400;
        D_8006EF8E[1].x = 0x7580;
        D_8006EF8E[1].z = 0x2E58;
        D_8006EF8E[2].flags = 0x400;
        D_8006EF8E[2].x = 0x7600;
        D_8006EF8E[2].z = 0x2E58;
        D_8006EE78[2] = 1;
        D_8006EE78[0] = D_8009AF80[D_8006EE78[1]];
        D_8006F160 = 0x7FFFFFF;
        D_8006EE78[1] = D_8009AF90[D_8006EE78[1]];
    }
    func_80032498(3, 0);
    func_80028470(0x24, 0);
    func_80095F78();
    func_8007369C();
    func_80073300();
    if (D_8006F954[0] & 0x8000) {
        D_8009C894 = 1;
    } else {
        D_8009C894 = 0;
    }
    D_8009BBC4 = 0;
    mode = D_8006F954[0] & 0x7FFF;
    D_8009BD0C = (D_8006F94E & 0x3FFF) - 0x400;
    D_8009D3D4 = D_8006F952;
    D_8009C584 = D_8006F950;
    D_8009C5A8 = mode;
    D_8006F954[0] = mode;
    func_80071B9C(mode, D_8006EF64[0]);
    step = D_8009A058[D_8009C5A8].enter;
    if (step == NULL) {
        goto check;
    }
    goto run;
    do {
        D_8009A058[D_8009C5A8].start();
        func_80097800();
        DrawSync(0);
        VSync(0);
        func_80035DB0();
        D_8009C894 = D_8009D7CC;
        func_800712D0();
        step = D_8009A058[D_8009C5A8].leave;
    run:
        step();
    check:;
    } while (D_8009D7CC >= 2);
    if (D_8009D7CC == 0) {
        func_800199CC(1);
        func_8001996C(1);
        if (D_8009BBC4 == 0) {
            if ((s16)D_8009D7D8->pad == 3) {
                func_80094364(&D_8009D55C.target, 3, D_8006EF64[0]);
            }
            D_8006F950 = D_8009BD38.vy;
            D_8006F94E = ((s16 *)D_8009D7D8->data)[4];
            D_8006F954[0] = ((s16 *)D_8009D7D8->data)[5];
        }
        D_8006EF68 = D_8009BD0C + 0x400;
    } else if (D_8009D7CC == 1) {
        func_800199CC(2);
        func_8001996C(2);
        D_800594F8 = 0;
        for (i = 0; i < 3; i++) {
            (&D_8006EE54.unk70)[i] = (&D_8006F8E5)[i];
        }
        func_80039CC4();
        data = D_8009C614;
        memcpy(D_80062648, data, func_800288EC(D_8009BCC8));
        D_8004F2FC = D_80062528;
        D_80062528 = func_80039850(D_80062648);
        func_80039A80(D_80062528, 0x7F, 0);
    } else {
        func_8001996C(0);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x13F;
        rect.h = 0x1AF;
        ClearImage(&rect, 0, 0, 0x40);
        DrawSync(0);
    }
    D_800591AE = 0;
    func_800762FC();
    func_80019ACC(0);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80070CFC);
#endif

/* The world-map main loop: gather input, flip the display buffers, run the
 * frame, and handle pause, encounters and leaving for another scene until
 * D_8009D554 clears. */
void func_800712D0(void) {
    WorldmapView *view;
    /* The live gear-byte base also reaches the party IDs 0x57D bytes earlier. */
    u8 *gear_state = &D_8006F8E5;
    RECT rect;
    s32 i;
    s32 found;

    D_8009BE3C = (WorldmapView *)&D_8009BBC8[1];
    D_8009D7F0 = 1;
    D_8009D554 = 1;
    do {
        D_8009BD1C = 0;
        D_8009BD14 = 0;
        D_8009CD50 = 0;
        D_8009BD18 = 0;
        D_8009BD10 = 0;
        D_8009CD4C = 0;
        while (func_80035CDC() != 0) {
            D_8009CD4C |= D_80059570;
            D_8009CD50 |= D_80059574;
            D_8009BD10 |= D_8005948C;
            D_8009BD14 |= D_80059490;
            D_8009BD18 |= D_800594A4;
            D_8009BD1C |= D_800594A8;
        }
        while (func_800967E4() == 3) {
            VSync(0);
        }
        CdSync(1, D_8009C588);
        view = (WorldmapView *)D_8009BBC8;
        if (D_8009BE3C == view) {
            view = (WorldmapView *)&D_8009BBC8[1];
        }
        D_8009BE3C = view;
        D_8009D7F0 = D_8009D7F0 == 0;
        ClearOTagR(view->ot, 0x400);
        func_800250E0(D_8009D7F0);
        func_8001D468();
        func_80097800();
        DrawSync(0);
        VSync(2);
        func_80019CA0();
        PutDispEnv(&((DisplayBuffer *)D_8009BE3C)->disp);
        PutDrawEnv(&((DisplayBuffer *)D_8009BE3C)->draw);
        if (D_80059179 == 0 && D_8009BD34 != 0 && D_8009C178 == 0 && D_8009D804 == 0 &&
            D_8009BD24 == -1 && D_8009CE68 == D_8009BD24 && D_8009D554 != 0 && D_8009D80C == 0) {
            D_8009BD34 = 0;
            if (func_80093F18(&D_8009D55C.target) != 4) {
                for (i = 0; i < 3; i++) {
                    (&D_8006EE70)[i] = (&D_8006F8E5)[i];
                }
                if (gear_state[0] != 0) {
                    gear_state[2] = 0;
                    gear_state[1] = 0;
                    gear_state[0] = 0;
                } else {
                    if (D_8006D940[(gear_state - 0x57D)[0]].gear != 0xFF) {
                        gear_state[0] = 1;
                    }
                    if (D_8006D940[(gear_state - 0x57D)[1]].gear != 0xFF) {
                        gear_state[1] = 1;
                    }
                    if (D_8006D940[(gear_state - 0x57D)[2]].gear != 0xFF) {
                        gear_state[2] = 1;
                    }
                }
                func_80075D4C();
            }
        } else {
            D_8009BD34 = 0;
        }
        if (D_8009C178 == 0) {
            if (D_8009D804 == 0 && D_8009D554 != 0 && D_8009D80C == 0 && (D_8009BD10 & 0x800)) {
                func_8007634C();
            }
            if (D_8009C178 == 0) {
                if (D_8009D804 == 0 && D_8009D554 != 0 && D_8009D80C == 0 && func_80035734(0) == 0) {
                    func_80076594();
                }
                if (D_8009C178 == 0 && D_8009D804 == 0 && D_8009BD24 == -1 &&
                    D_8009CE68 == D_8009BD24 && D_8009D554 != 0 && D_8009D80C != 0) {
                    found = func_80075E7C(&D_8009D55C.target, D_8006EF64[0]);
                    if (found == 1) {
                        D_8009D554 = 0;
                        D_8009D7CC = found;
                        D_8005954C = 0;
                        D_8006EE70 = D_8006F8E5;
                        D_8006EE72 = D_8006F8E6;
                        D_8006EE74 = D_8006F8E7;
                    }
                }
            }
        }
        D_8009D80C = 0;
        if (D_8009BD10 & 0x100) {
            u16 *camera_mode = &D_8006EE76;
            *camera_mode ^= 1;
        }
        if (D_8009C178 == 0 && D_8009D804 != 0 && D_8009D554 != 0) {
            if (D_8009BE10 > 0) {
                if (D_8009BE10 < 4) {
                    func_800758C0();
                    D_80059460 = 0;
                    D_80059178 = 0;
                    D_80059171 = 1;
                    func_800762FC();
                    func_8001C634();
                    func_800762FC();
                    func_80075B58();
                } else if (D_8009BE10 < 8) {
                    D_8009D554 = 0;
                    D_8009D7CC = 0;
                    D_8009D7D8 = &D_8009B6C4[2];
                    D_8006EE54.flags |= 0x2000;
                }
            }
        } else {
            D_8009D804 = 0;
        }
        func_80025044();
        func_80074F2C();
        func_80075104();
        SetGeomOffset(0xA0, D_8009BE0C);
        DrawOTag(D_8009BE3C->ot + 0x3FF);
    } while (D_8009D554 != 0);
    ResetGraph(1);
    if (D_8009D7F0 == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    func_80096694();
    DrawSync(0);
    VSync(0);
    PutDispEnv(&D_8009BBC8[1].disp);
}

/* Mode step that has nothing to do; always reports done. */
s32 func_80071A50(void) {
    return 1;
}

/* One world-map frame: input, actors, camera, terrain, sky and HUD. */
s32 func_80071A58(void) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */

    if (D_8009D144 == 0) {
        func_80097440(&D_8009BD40);
    } else {
        func_80097244(&D_8009BD40);
    }
    func_80089748();
    func_80089C78();
    func_80085CDC();
    func_8008615C();
    func_800747DC();
    func_800848F4();
    func_800980D4(&D_8009BBB4);
    if (D_8009D558 != 0) {
        func_800981C8(&D_8009BE28);
        func_80096130();
        func_80098CC0();
    }
    func_800983A0(&D_8009BE28);
    func_8009932C(D_8009BE3C->ot, D_8009BE3C->unk74, &D_8009BE28);
    D_8009C5BC += 0x40;
    func_80073B04();
    func_800737EC();
    func_80086798();
    if (D_8006EE54.unk76 == 0) {
        func_800740B8();
    }
    return 1;
}

/* Select the file set of an area (by index, or for the low indices by the
 * position against the threshold table) and derive its file numbers. */
#ifdef NON_MATCHING /* last three loads scheduled differently */
void func_80071B9C(s32 index, s32 position) {
    WorldmapArea *area;
    s32 i;

    if (index < 8) {
        for (i = 1; position >= D_8009B564[i]; i++) {
        }
        area = &D_8009B57C[i];
        D_8009C610 = i - 1;
    } else {
        area = &D_8009B57C[index + 2];
    }
    D_8009D3C4 = area->file + 1;
    D_8009C174 = area->file + 3;
    D_8009C17C = area->file + 2;
    D_8009D3D0 = area->file + 5;
    D_8009CC98 = area->file + 4;
    D_8009D800 = area->file + 7;
    D_8009D3C8 = area->file + 6;
    D_8009BCD8 = area->file + 9;
    D_8009BCC8 = area->file + 8;
    D_8009D160 = area->param2;
    D_8009D2B4 = area->param4;
    D_8009BD08 = area->file + 10;
    D_8009D7CC = area->param6;
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80071B9C);
#endif

/* Allocate buffers for each party member's model and gear model, then read
 * them all with one list. */
#ifdef NON_MATCHING /* second loop's index/pointer registers differ */
void func_80071CDC(void) {
    s32 i;
    s32 j;
    s32 member;
    u8 gear;

    for (i = 0; i < 3; i++) {
        member = D_8006F368[i];
        if (member != 0xFF) {
            D_8009CD34[i] = func_80031BDC(func_800288EC(member + 2), 0);
            gear = D_8006D940[member].gear;
            if (gear != 0xFF) {
                D_8009BDF8[i] = func_80031BDC(func_800288EC(gear + 0x13), 0);
            } else {
                D_8009BDF8[i] = NULL;
            }
        } else {
            D_8009BDF8[i] = NULL;
            D_8009CD34[i] = NULL;
        }
    }
    i = 0;
    D_8009C170 = 0;
    for (j = 0; j < 3; j++) {
        member = D_8006F368[j];
        if (member != 0xFF) {
            D_8009D3F8[i].file = member + 2;
            D_8009D3F8[i].dest = D_8009CD34[j];
            i++;
            D_8009C170++;
            gear = D_8006D940[member].gear;
            if (gear != 0xFF) {
                D_8009D3F8[i].file = gear + 0x13;
                D_8009D3F8[i].dest = D_8009BDF8[j];
                i++;
            }
        }
    }
    D_8009D3F8[i].file = 0;
    D_8009D3F8[i].dest = NULL;
    func_80029AFC(D_8009D3F8, 0, 0);
}
#else
INCLUDE_ASM(".local/decomp/worldmap/asm/nonmatchings/worldmap", func_80071CDC);
#endif

/* Allocate and read the three area files into their resident buffers. */
void func_80071EF0(void) {
    D_8009C59C = func_80031BDC(func_800288EC(D_8009C17C), 1);
    D_8009BD20 = func_80031BDC(func_800288EC(D_8009C174), 1);
    D_8009C180 = func_80031BDC(func_800288EC(D_8009D3C4), 1);
    D_8009D3F8[0].file = D_8009D3C4;
    D_8009D3F8[1].file = D_8009C17C;
    D_8009D3F8[2].file = D_8009C174;
    D_8009D3F8[3].file = 0;
    D_8009D3F8[0].dest = D_8009C180;
    D_8009D3F8[1].dest = D_8009C59C;
    D_8009D3F8[2].dest = D_8009BD20;
    D_8009D3F8[3].dest = NULL;
    func_80029AFC(D_8009D3F8, 0, 0);
}

/* Allocate and read the two shared world-map files (0x25, 0x26). */
void func_80071FEC(void) {
    D_8005945C = func_80031BDC(func_800288EC(0x26), 1);
    D_8009D528 = func_80031BDC(func_800288EC(0x25), 1);
    D_8009D3F8[0].file = 0x25;
    D_8009D3F8[0].dest = D_8009D528;
    D_8009D3F8[1].file = 0x26;
    D_8009D3F8[1].dest = D_8005945C;
    D_8009D3F8[2].file = 0;
    D_8009D3F8[2].dest = NULL;
    func_80029AFC(WORLD_READ_LIST, 0, 0);
}

/* Allocate the area's five file buffers and read the zero-terminated list. */
void func_80072090(void) {
    D_8004F304++;
    D_8009D3F8[0].file = D_8009CC98;
    D_8009D3F8[0].dest = D_8009C88C = func_80031BDC(func_800288EC(D_8009CC98), 1);
    D_8009D3F8[1].file = D_8009D3D0;
    D_8009D3F8[1].dest = D_8009C884 = func_80031BDC(func_800288EC(D_8009D3D0), 0);
    D_8009D3F8[2].file = D_8009D3C8;
    D_8009D3F8[2].dest = D_8006259C = func_80031BDC(func_800288EC(D_8009D3C8), 0);
    D_8009D3F8[3].file = D_8009D800;
    D_8009D3F8[3].dest = D_8009C888 = func_80031BDC(func_800288EC(D_8009D800), 0);
    D_8009D3F8[4].file = D_8009BCC8;
    D_8009D3F8[4].dest = D_8009C614 = func_80031BDC(func_800288EC(D_8009BCC8), 0);
    D_8009D3F8[5].file = 0;
    D_8009D3F8[5].dest = NULL;
    func_80029AFC(WORLD_READ_LIST, 0, 0);
}

/* Allocate and read the area's sixth file (kept, mode 0). */
void func_800721E4(void) {
    D_8006259C = func_80031BDC(func_800288EC(D_8009D3C8), 0);
    func_800295D8(D_8009D3C8, D_8006259C, 0, 0);
}

INCLUDE_RODATA(".local/decomp/worldmap/asm/nonmatchings/worldmap", D_8006FAF0);
