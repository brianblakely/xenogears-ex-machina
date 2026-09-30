/* Mode 6, the movie mode (overlay image at 0x8006faf0, packed in disc 1 file
 * 40 / disc 2 file 35). The resident mode dispatcher loads it into the mode
 * overlay area and enters 800737ec: that loads the movie library (disc file
 * 19 at 0x801d3000), plays the movie that the request bytes 8004fe44..47
 * name and selects the next mode. Without a request it runs a development
 * menu: movie test settings, a CD-ROM monitor, a CD-ROM read check, a FAT
 * check and a disc change test, drawn with the resident debug font. */
#include "common.h"
#include "movie_mode.h"

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FAF0);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_800704E8);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80070DCC);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FC6C);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FC70);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FC8C);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FC98);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FCA4);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FCAC);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FCB4);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FCD8);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_800712C4);

/* FAT check step: read file 40h into the check buffer (allocated once) and
 * count the read; a pass without errors keeps its tally. */
void func_80071BA0(void) {
    s32 tally;

    if (D_80076EA0 == NULL) {
        D_80076EA0 = func_80031BDC(0x2000, 0);
    }
    if (D_80076EA0 != NULL) {
        func_8002954C(0x40, D_80076EA0, 0x2000, 0, 0);
    }
    D_80076F3C[0]++;
    if (D_80076E4C == 0) {
        tally = D_80076EB0;
        D_80076EB0 = 0;
        D_80076EAC = tally;
    }
}

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80071C34);

/* Vertical-blank tick: count frames and whole seconds. */
void func_80072428(void) {
    D_80076E5C++;
    if (++D_80076ECC >= 60) {
        D_80076ECC = 0;
        D_80076EC8++;
    }
}

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80072480);

/* Stop the resident disc read and wait until the drive reports its status. */
void func_8007293C(void) {
    if (func_8002C3D8() == 0) {
        func_8002A498(0);
        func_80028A60(0);
        func_8002A428(0);
        func_80028A60(0);
        VSync(3);
        while (CdControlB(8, NULL, D_80076F84) == 0) {
        }
    }
}

/* Write `size` bytes of `buffer` to the host file `name`. */
void func_800729A8(char *name, void *buffer, s32 size) {
    s32 fd;

    fd = PCopen(name, 0, 0);
    func_8004C398(fd, buffer, size);
    PCclose(fd);
}

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80072A08);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80072D84);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80072F98);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80073328);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_800734B8);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_800737EC);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_800747AC);

/* Next pseudo-random number (two mixed linear congruential sequences). */
s32 func_80074AF0(void) {
    D_80076EF4 = D_80076EF4 * 5 + 1;
    D_80076EF8 = D_80076EF8 * 7 + 3;
    D_80076EF4 = (D_80076EF4 ^ D_80076EF8) + 1;
    if (D_80076EF4 < 0) {
        D_80076EF4 = -D_80076EF4;
    }
    return D_80076EF4;
}

/* Clear all of VRAM to black. */
void func_80074B58(void) {
    RECT rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = 640;
    rect.h = 480;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80074BA4);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_8007519C);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_800753B8);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_8007548C);

void func_80075508(void) {
    func_80039A80(D_8007700C, 0x7F, 0);
}

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80075534);

/* The first sector of directory record `index` (bytes 3..6). */
u32 func_80075D4C(s32 index) {
    u8 *record;

    record = D_8004FDF0 + index * 7;
    return ((record[6] << 24) + (record[5] << 16) + (record[4] << 8)) | record[3];
}

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8007042C);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_80070430);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80075D8C);

INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_8007625C);

/* Play the requested movie when its directory list holds it; `keep` stops
 * the buttons from ending it. Declared int without a value, as the original
 * (its return register stays live on every path). */
s32 func_800763BC(u8 keep) {
    u8 unused[0x18];

    D_80077028 = keep;
    D_800773AC = -1;
    if (D_80077448 == 0) {
        func_80028470(0x18, 0);
        if (D_8007711C >= func_80028928(2)) {
            return;
        }
    } else if (D_80077448 == 1) {
        func_80028470(0x18, 1);
        if (D_8007711C >= func_80028928(1)) {
            return;
        }
    } else if (D_80077448 == 2) {
        return;
    }
    D_80077124[0].draw.isbg = 0;
    D_80077124[1].draw.isbg = 0;
    D_80077124[0].disp.isrgb24 = 1;
    D_80077124[1].disp.isrgb24 = 1;
    func_80076488();
}

#ifdef NON_MATCHING
/* Clear the screen, reopen the movie library and stream the movie, running
 * three decode steps per frame (their VSync counters are kept for the
 * monitor) until the frame callback or a button ends it; a movie that ended
 * on the second buffer is copied to the first. The two unused arrays
 * reproduce the original's frame. Same instructions except one delay slot:
 * the original leaves the short-file exit's jump to the final return 0 unfilled. */
s32 func_80076488(void) {
    u8 unused0[0x90];
    RECT screen;
    u8 unused1[0x200];
    RECT copy;
    s32 file;
    s32 select;
    s32 budget;
    s32 i;
    s32 before;
    s32 after;

    screen = D_800704E0;
    D_80077014 = 0;
    D_80077020 = 0;
    if (D_80077448 == 0) {
        select = 0;
        file = D_8007711C + 3;
    } else {
        select = 1;
        file = D_8007711C + 2;
    }
    if (func_80028738(file) == 0x18) {
        SetDispMask(1);
        D_8004FE46 = 0;
    } else {
        VSync(0);
        ClearImage(&screen, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        D_80077018 = 0;
        D_8007701C = 0;
        func_801D43B0();
        if (D_800773A0 > 0) {
            D_80077024 = (240 - D_800773A0) / 2;
        } else {
            D_80077024 = 0;
        }
        func_801D3538(320, 240, 0x80, 16, 32, 0x800, D_80077454);
        D_801D68B4 = 0;
        if (D_80077438 != 0) {
            func_801D37CC(file, D_800773A8, D_800773A4, D_8007739C, D_80077398, select, 1, 0,
                          D_80077024, 0, D_80077024 + 240, D_800773A0, func_800768D8);
        } else {
            func_801D37CC(file, D_800773A8, D_800773A4, D_8007739C, D_80077398, select, 0, 0,
                          D_80077024, 0, D_80077024 + 240, D_800773A0, func_800768D8);
        }
        budget = 30;
        PutDrawEnv(&D_80077124[D_80077018].draw);
        PutDispEnv(&D_80077124[D_80077018].disp);
        SetDispMask(1);
        while (1) {
            if (D_80077020 == 0) {
                for (i = 0; i < budget / 10; i++) {
                    before = VSync(1);
                    func_801D3F7C();
                    after = VSync(1);
                    if (i * 2 + 1 < 32) {
                        D_800773B8[i * 2] = before;
                        D_800773B8[i * 2 + 1] = after;
                    }
                }
                budget %= 10;
            }
            budget += 30;
            func_800769A4();
            func_80019CA0();
            VSync(0);
            PutDispEnv(&D_80077124[D_8007701C].disp);
            D_8007701C = D_80077018;
            if (D_80077014 == 1) {
                break;
            }
            if (D_80077014 >= 2) {
                D_80077014--;
            }
        }
        func_801D4318();
        if (D_8007701C == 0) {
            DrawSync(0);
            VSync(0);
            copy.x = 0;
            copy.y = 240;
            copy.w = 480;
            copy.h = 240;
            MoveImage(&copy, 0, 0);
            DrawSync(0);
            VSync(0);
            PutDispEnv(&D_80077124[1].disp);
        }
    }
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80076488);
#endif

/* The movie library's frame callback: the buffer the frame went to; the
 * last frame ends the movie. */
void func_800768D8(u16 frame, u16 x, u16 y) {
    D_80077010 = frame;
    if (D_801D68B4 == 1) {
        if (y != D_80077024) {
            D_80077018 = 0;
        } else {
            D_80077018 = 1;
        }
    } else if (y != D_80077024) {
        D_80077018 = 0;
        D_8007701C = 0;
    } else {
        D_80077018 = 1;
        D_8007701C = 1;
    }
    if (frame >= D_8007739C && D_80077438 == 0) {
        D_80077014 = 1;
    }
}

/* Read controller port 0 with a repeat after 90 frames held; Circle or Start
 * ends the movie in five frames, fading the CD volume, unless it is kept. */
void func_800769A4(void) {
    D_800773B4 = D_800773AC;
    D_800773AC = func_8003569C(0);
    if (D_800773B4 == D_800773AC && D_800773B4 != 0) {
        D_80076F04++;
        if (D_80076F08 < D_80076F04) {
            D_800773B4 = 0;
            D_80076F08 = 1;
            D_80076F04 = 0;
        }
    } else {
        D_80076F08 = 90;
        D_80076F04 = 0;
    }
    if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40) && D_80077028 == 0) {
        func_80038D18(0, 10);
        D_80077014 = 5;
    }
    if (!(D_800773B4 & 0x800) && (D_800773AC & 0x800) && D_80077028 == 0) {
        func_80038D18(0, 10);
        D_80077014 = 5;
    }
}

/* Set up the geometry screen and the light and color matrices. */
void func_80076AF0(void) {
    SetGeomScreen(512);
    SetGeomOffset(160, 120);
    D_800770B8.m[0][0] = 0x1000;
    D_800770B8.m[1][1] = 0x1000;
    D_800770B8.m[2][2] = 0x1000;
    D_800770F8.m[0][0] = 0x1000;
    D_800770F8.m[1][1] = 0x1000;
    D_800770F8.m[2][2] = 0x1000;
    D_80077090.m[0][0] = 0x93D;
    D_80077090.m[0][1] = -0x93D;
    D_80077090.m[0][2] = -0x93D;
    D_800770B8.m[0][1] = 0;
    D_800770B8.m[0][2] = 0;
    D_800770B8.m[1][0] = 0;
    D_800770B8.m[1][2] = 0;
    D_800770B8.m[2][0] = 0;
    D_800770B8.m[2][1] = 0;
    D_800770F8.m[0][1] = 0;
    D_800770F8.m[0][2] = 0;
    D_800770F8.m[1][0] = 0;
    D_800770F8.m[1][2] = 0;
    D_800770F8.m[2][0] = 0;
    D_800770F8.m[2][1] = 0;
    D_80077090.m[1][0] = 0;
    D_80077090.m[1][1] = 0;
    D_80077090.m[1][2] = 0;
    D_80077090.m[2][0] = 0;
    D_80077090.m[2][1] = 0;
    D_80077090.m[2][2] = 0;
    D_80077070.m[0][0] = 0x969;
    D_80077070.m[0][1] = 0;
    D_80077070.m[0][2] = 0;
    D_80077070.m[1][0] = 0x969;
    D_80077070.m[1][1] = 0;
    D_80077070.m[1][2] = 0;
    D_80077070.m[2][0] = 0x969;
    D_80077070.m[2][1] = 0;
    D_80077070.m[2][2] = 0;
    SetColorMatrix(&D_80077070);
    SetBackColor(0x60, 0x60, 0x60);
}

/* Update the camera and load it as the rotation and translation. The unused
 * local reproduces the original's frame. */
void func_80076C68(void) {
    u8 unused[0x18];

    func_80076CA4();
    SetRotMatrix(&D_80077050);
    SetTransMatrix(&D_80077050);
}

#ifdef NON_MATCHING
/* Aim the camera from the eye at the target, rolled, and compose the world
 * to screen matrix. Same instructions except that the original keeps
 * &D_8007703C (not &D_8007703C.vz) in $s5 for its two target.vx reads. */
void func_80076CA4(void) {
    s32 dz;
    s32 dx;
    s32 dy;
    s32 dz2;
    s32 dx2;

    dz = D_8007703C.vz - D_8007702C.vz;
    dz2 = dz * dz;
    dx = D_8007703C.vx - D_8007702C.vx;
    dx2 = dx * dx;
    dy = D_8007703C.vy - D_8007702C.vy;
    D_800770B0.vx = ratan2(dy, SquareRoot0(dz2 + dx2));
    D_800770B0.vy = -ratan2(dx, dz);
    func_8003F738(&D_800770B0, &D_800770D8);
    RotMatrixZ(D_8007704C, &D_800770D8);
    func_80049BDC(&D_800770F8, &D_800770D8);
    D_800770D8.t[0] = 0;
    D_800770D8.t[1] = 0;
    D_800770D8.t[2] = SquareRoot0(dx2 + dy * dy + dz2);
    D_800770B8.t[0] = D_80076F2C - D_8007703C.vx;
    D_800770B8.t[1] = D_80076F30 - D_8007703C.vy;
    D_800770B8.t[2] = D_80076F34 - D_8007703C.vz;
    CompMatrix(&D_800770D8, &D_800770B8, &D_80077050);
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80076CA4);
#endif

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_800704E0);
