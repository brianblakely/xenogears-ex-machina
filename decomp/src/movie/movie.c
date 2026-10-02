/* Mode 6, the movie mode (overlay image at 0x8006faf0, packed in disc 1 file
 * 40 / disc 2 file 35). The resident mode dispatcher loads it into the mode
 * overlay area and enters 800737ec: that loads the movie library (disc file
 * 19 at 0x801d3000), plays the movie that the request bytes 8004fe44..47
 * name and selects the next mode. Without a request it runs a development
 * menu: movie test settings, a CD-ROM monitor, a CD-ROM read check, a FAT
 * check and a disc change test, drawn with the resident debug font. */
#include "common.h"
#include "movie_mode.h"

/* The menu's CD-ROM monitor: at 640x240, show the read statistics, the
 * resident's error counters and stream state, a dump of the stream buffer
 * and the reads per result class, run the monitor's input every frame and
 * return to the 320-wide menu on Start once no read is running. The unused
 * locals (mode included) reproduce the original's frame; the original also
 * reads the menu cursor before the exit test and stores it back after. */
void func_800704E8(void) {
    u8 unused[8];
    char mode[3] = {0, 2, 2};
    u8 unused2[0x18];
    s32 button;
    s32 i;
    s32 hours;
    s32 cursor;
    MovieBuffer *buffer;
    u32 *ot;

    func_80028A94(NULL);
    func_80074B58();
    D_80076EA0 = NULL;
    D_80076EB8 = 1;
    for (i = 15; i >= 0; i--) {
        D_80076F3C[i] = 0;
    }
    SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 640, 240);
    SetDefDispEnv(&D_80077124[0].disp, 0, 240, 640, 240);
    SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 640, 240);
    SetDefDispEnv(&D_80077124[1].disp, 0, 0, 640, 240);
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.isbg = 1;
    func_80028470(0xC, 3);
    for (;;) {
        if (D_80077120 == &D_80077124[0]) {
            buffer = &D_80077124[1];
        } else {
            buffer = &D_80077124[0];
        }
        ot = buffer->ot;
        D_80077120 = buffer;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        func_800712C4();
        func_800747AC(0, 0, &button);
        func_80070DCC();
        if (D_80076EBC != 0) {
            func_8003700C("Random Mode\n");
        }
        if (D_80076EB8 == 0) {
            func_8003700C("Stream Pause\n");
        }
        func_8003700C("Read %3d Error %3d VSync %8d EC %2d ST %2d ", D_80076E48, D_80076E4C,
                      D_80076E5C, D_8005A4DC, D_8004FE1C);
        switch (D_80076E64) {
        case 0:
            func_8003700C("Waiting\n");
            break;
        case 1:
            func_8003700C("Reading\n");
            break;
        case 2:
            func_8003700C("Verifing\n");
            break;
        }
        func_8003700C("C1 %3d C2 %3d C3 %3d C4 %3d C5 %3d C6 %3d C7 %3d C8 %3d C9 %3d\n",
                      D_8005A488, D_8005A48C, D_8005A490, D_8005A494, D_8005A498, D_8005A49C,
                      D_8005A4A4, D_8005A4A8, D_8005A4B4);
        func_8003700C("RestFile %7d RestSize %7d N1 %4d N2 %4d N3 %4d R%3d D%3d\n",
                      func_800286CC(), func_800286BC(), D_8004FDE4, D_8004FDE8, D_8004FDEC,
                      D_8004FE26, D_8004FE28);
        func_8003700C("ErrorAddress %8x ErrorSize %8x N%3d N%3d N%3d\n", D_80076E68, D_80076E6C,
                      D_80076E70, D_80076E74, D_80076E78);
        func_8003700C("FrdPtr1 %8x FrdPtr2 %8x Buf1 %8x Buf2 %8x\n", D_80076E7C, D_80076E80,
                      D_80076E88, D_80076E8C);
        if (D_80076E48 >= 11 || D_80076E94 > 0) {
            func_8003700C("S %8x Adrs %8x Write %8x Rest %8x\n", D_80076E98, D_80076E9C,
                          D_80076E84, D_80076E94);
            for (i = 0; i < 7; i++) {
                func_8003700C("%08x ", D_80076E98[i]);
            }
            func_8003700C(D_8006FC6C);
            for (i = 0; i < 7; i++) {
                func_8003700C("%08x ", D_80076E98[i + 7]);
            }
            func_8003700C(D_8006FC6C);
            for (i = 0; i < 7; i++) {
                func_8003700C("%08x ", D_80076E98[i + 14]);
            }
            func_8003700C(D_8006FC6C);
            for (i = 0; i < 7; i++) {
                func_8003700C("%08x ", D_80076E98[i + 21]);
            }
            func_8003700C(D_8006FC6C);
            if (D_80076E7C != NULL) {
                func_8003700C(D_8006FC70, D_80076E7C[0].dest, D_80076E7C[1].dest,
                              D_80076E7C[2].dest, D_80076E7C[3].dest);
            }
        }
        D_80076EA8 = 0;
        func_8003700C(D_8006FC8C, D_80076EA4);
        for (i = 0; i < 13; i++) {
            func_8003700C(D_8006FC98, i, D_80076F3C[i]);
            D_80076EA8 += D_80076F3C[i];
            if (D_80076EB0 == i) {
                func_8003700C(D_8006FCA4);
            }
            if (D_80076EAC == i) {
                func_8003700C(D_8006FCAC);
            }
            func_8003700C(D_8006FC6C);
        }
        hours = D_80076EC8 / 3600;
        func_8003700C(D_8006FCB4, D_80076EA8, hours, D_80076EC8 / 60 - hours * 60,
                      D_80076EC8 % 60);
        func_8003700C(D_8006FCD8);
        if (D_80077394 > 0) {
            func_8003278C(1, 0, 6, 0x808D);
        }
        func_80037324(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 8, 12, 624, 216);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 7, 11, 626, 218);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
        cursor = D_80077118;
        if (!(D_800773B4 & 0x800) && (D_800773AC & 0x800) && D_80076E48 == 0) {
            D_80077394 = 0;
            func_8002A498(0);
            func_80028A60(0);
            if (D_80076EA0 != NULL) {
                func_800320E8(D_80076EA0);
            }
            D_80076EA0 = NULL;
            if (D_80076E98 != NULL) {
                func_800320E8(D_80076E98);
            }
            D_80076E98 = NULL;
            func_80074B58();
            SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 320, 240);
            SetDefDispEnv(&D_80077124[0].disp, 0, 240, 320, 240);
            SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 320, 240);
            SetDefDispEnv(&D_80077124[1].disp, 0, 0, 320, 240);
            D_80077124[0].disp.screen.x = 0;
            D_80077124[0].disp.screen.y = 10;
            D_80077124[0].disp.screen.w = 256;
            D_80077124[0].disp.screen.h = 216;
            D_80077124[1].disp.screen.x = 0;
            D_80077124[1].disp.screen.y = 10;
            D_80077124[1].disp.screen.w = 256;
            D_80077124[1].disp.screen.h = 216;
            D_80077124[0].draw.isbg = 1;
            D_80077124[1].draw.isbg = 1;
            return;
        }
        D_80077118 = cursor;
    }
}

/* CD-ROM monitor input: newly pressed buttons issue the monitor's CD
 * commands or read steps; R1 toggles the stream copy (1) or host read (2),
 * Select random commands. A running stream copies each arrived chunk
 * into the destination and moves to the next list entry when it ends. */
void func_80070DCC(void) {
    s32 i;
    s32 command;

    if (!(D_800773B4 & 0x20) && (D_800773AC & 0x20)) {
        func_80071C34(1);
    }
    if (!(D_800773B4 & 0x10) && (D_800773AC & 0x10)) {
        func_80071C34(2);
    }
    if (!(D_800773B4 & 0x80) && (D_800773AC & 0x80)) {
        func_80071C34(3);
    }
    if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40)) {
        D_80076EA4++;
        func_8002A498(0);
    }
    if (!(D_800773B4 & 0x1000) && (D_800773AC & 0x1000)) {
        func_80071C34(7);
    }
    if (!(D_800773B4 & 0x4000) && (D_800773AC & 0x4000)) {
        func_80071C34(9);
    }
    if (!(D_800773B4 & 8) && (D_800773AC & 8)) {
        D_80076EB8 = 1 - D_80076EB8;
    }
    if (!(D_800773B4 & 4) && (D_800773AC & 4) && D_80076E48 < 11) {
        func_80071BA0();
    }
    if (D_80076EB8 == 1) {
        D_80076E9C = func_80028B14();
        if (D_80076E9C != NULL) {
            i = 0;
            if (D_80076E94 > 0x800) {
                do {
                    *D_80076E84++ = D_80076E9C[i++];
                } while (i < 0x200);
            } else {
                while (i < D_80076E94 / 4) {
                    *D_80076E84++ = D_80076E9C[i++];
                }
            }
            D_80076E94 -= 0x800;
            if (D_80076E94 <= 0 && D_80076E48 == 12) {
                i = D_80076E7C[++D_80076EB4].file;
                if (i != 0) {
                    D_80076E94 = func_800288EC(i);
                }
                D_80076E84 = D_80076E7C[D_80076EB4].dest;
            }
            func_8002945C(D_80076E9C);
        }
    }
    if (D_80076EB8 == 2 && func_80028F30(&D_80076F7C, &D_80076F80) == 0) {
        func_800294B4(D_80076F80);
    }
    if (!(D_800773B4 & 0x2000) && (D_800773AC & 0x2000)) {
        func_80071C34(11);
    }
    if (!(D_800773B4 & 0x8000) && (D_800773AC & 0x8000)) {
        func_80071C34(13);
    }
    if (D_80076EBC != 0) {
        command = func_80074AF0() & 0xFF;
        if (command == 0 && D_80076E48 < 11) {
            func_80071BA0();
        }
        if ((u32)(command - 1) < 12) {
            func_80071C34(command);
        }
    }
    if (!(D_800773B4 & 0x100) && (D_800773AC & 0x100)) {
        D_80076EBC = 1 - D_80076EBC;
    }
}

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FC6C);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FC70);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FC8C);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FC98);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FCA4);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FCAC);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FCB4);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8006FCD8);

/* Fill `size` bytes of `words` with `value`, counting in `n`. */
#define FILL_WORDS(words, size, n, value)  \
    for (n = 0; n < (size) / 4; n++) {    \
        (words)[n] = (value);              \
    }

/* Record a verify mismatch at byte `offset` of a `size`-byte read: the
 * first one keeps its place and the resident's stream counters. */
#define VERIFY_ERROR(offset, size)         \
    {                                      \
        if (D_80076E4C == 0) {             \
            D_80076E68 = (offset);         \
            D_80076E6C = (size);           \
            D_80076E70 = D_8004FDE4;       \
            D_80076E74 = D_8004FDE8;       \
            D_80076E78 = D_8004FDEC;       \
        }                                  \
        D_80076E4C++;                      \
    }

/* Monitor read check, run every frame: once the command's read (phase 1)
 * ends, read the same data again into a second buffer or file list filled
 * with -1 (phase 2); once that ends, compare both copies word by word,
 * record the first mismatch and release the copies. */
void func_800712C4(void) {
    s32 i;
    s32 *dest;
    s32 index;
    s32 file;
    s32 *buffer;
    s32 *copy;

    if (D_80076E64 == 0) {
        return;
    }
    if (func_800286CC() == 0 && D_80076E64 == 1) {
        D_80076E64 = 2;
        switch (D_80076E48) {
        case 1:
            D_80076E90 = 0x2000;
            D_80076E8C = buffer = func_80031BDC(0x2000, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            func_8002954C(0x40, D_80076E8C, D_80076E90, 0, 0);
            break;
        case 2:
            D_80076E90 = func_800288EC(7);
            D_80076E8C = buffer = func_80031BDC(D_80076E90, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            func_800295D8(7, D_80076E8C, 0, 0);
            break;
        case 3:
        case 12:
            D_80076E80 = func_8002A57C(2, 0);
            if (D_80076E80 == NULL) {
                D_80076E64 = 0;
                D_80076E48 = 0;
                break;
            }
            for (index = 0; (file = D_80076E80[index].file) > 0; index++) {
                copy = D_80076E80[index].dest;
                D_80076E90 = func_800288EC(file);
                FILL_WORDS(copy, D_80076E90, i, -1);
            }
            func_80029AFC(D_80076E80, 0, 0);
            break;
        case 4:
            D_80076E90 = 0x2000;
            D_80076E8C = buffer = func_80031BDC(0x2000, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            func_8002954C(0x40, D_80076E8C, D_80076E90, 0, 0);
            break;
        case 5:
            D_80076E90 = func_800288EC(7);
            D_80076E8C = buffer = func_80031BDC(D_80076E90, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            func_800295D8(7, D_80076E8C, 1, 0);
            break;
        case 6:
            D_80076E80 = func_8002A57C(2, 0);
            if (D_80076E80 == NULL) {
                D_80076E64 = 0;
                D_80076E48 = 0;
                break;
            }
            for (index = 0; (file = D_80076E80[index].file) > 0; index++) {
                copy = D_80076E80[index].dest;
                D_80076E90 = func_800288EC(file);
                FILL_WORDS(copy, D_80076E90, i, -1);
            }
            func_80029AFC(D_80076E80, 1, 0);
            break;
        case 7:
        case 8:
            D_80076E64 = 0;
            D_80076E48 = 0;
            break;
        case 11:
            D_80076E90 = func_800288EC(6);
            D_80076E8C = buffer = func_80031BDC(D_80076E90, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            func_800295D8(6, D_80076E8C, 0, 0);
            break;
        }
    }
    if (func_800286CC() == 0 && D_80076E64 == 2) {
        D_80076E64 = 0;
        switch (D_80076E48) {
        case 1:
        case 4:
            for (i = 0; i < 0x800; i++) {
                if (D_80076E88[i] != D_80076E8C[i]) {
                    VERIFY_ERROR(i * 4, 0x2000);
                    break;
                }
            }
            func_800320E8(D_80076E88);
            func_800320E8(D_80076E8C);
            D_80076E64 = 0;
            break;
        case 2:
        case 5:
            D_80076E90 = func_800288EC(7);
            for (i = 0; i < D_80076E90 / 4; i++) {
                if (D_80076E88[i] != D_80076E8C[i]) {
                    VERIFY_ERROR(i * 4, func_800288EC(7));
                    break;
                }
            }
            func_800320E8(D_80076E88);
            func_800320E8(D_80076E8C);
            D_80076E64 = 0;
            break;
        case 3:
        case 6:
        case 12:
            index = 0;
            file = D_80076E7C[0].file;
            if (file > 0) {
                do {
                    copy = D_80076E80[index].dest;
                    dest = D_80076E7C[index].dest;
                    D_80076E90 = func_800288EC(file);
                    for (i = 0; i < D_80076E90 / 4; i++) {
                        if (dest[i] != copy[i]) {
                            VERIFY_ERROR(i * 4, func_800288EC(file));
                            break;
                        }
                    }
                } while ((file = D_80076E7C[++index].file) > 0);
            }
            func_8002A524(D_80076E7C);
            func_8002A524(D_80076E80);
            func_800320E8(D_80076E7C);
            func_800320E8(D_80076E80);
            D_80076E64 = 0;
            break;
        case 7:
        case 8:
        case 9:
        case 10:
            D_80076E64 = 0;
            break;
        case 11:
            if (D_80076E94 > 0) {
                D_80076E64 = 2;
            } else {
                D_80076E90 = func_800288EC(6);
                for (i = 0; i < D_80076E90 / 4; i++) {
                    if (D_80076E88[i] != D_80076E8C[i]) {
                        VERIFY_ERROR(i * 4, func_800288EC(6));
                        break;
                    }
                }
                func_800320E8(D_80076E88);
                func_800320E8(D_80076E8C);
            }
            D_80076E64 = 0;
            break;
        }
        D_80076E48 = 0;
    }
}

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

/* Clear `size` bytes of words at `dest`, counting in `n`. */
#define ZERO_WORDS(dest, size, n)          \
    {                                      \
        s32 *word;                         \
        n = 0;                             \
        word = (dest);                     \
        for (; n < (size) / 4; n++) {      \
            *word++ = 0;                   \
        }                                  \
    }

/* Start monitor command `command` when no read is running: 1/4 read file
 * 40h into a fresh 8 KB buffer, 2/5 file 7 through the host-file stream,
 * 3/6 the directory's file list, 7/8 the stream ring, 11 stream file 6 and
 * 12 the file list into the ring, 13 file 3 into a new 64-block ring. Each
 * destination is cleared first; the command counts in its class. */
void func_80071C34(s32 command) {
    s32 i;
    s32 index;
    s32 file;
    s32 *dest;

    if (D_80076E48 != 0 || D_80076E64 != 0) {
        return;
    }
    func_80028470(0xC, 3);
    D_80076E64 = 1;
    D_80076E48 = command;
    switch (command) {
    case 1:
        D_80076E90 = 0x2000;
        D_80076F3C[1]++;
        D_80076E88 = func_80031BDC(0x2000, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        func_8002954C(0x40, D_80076E88, D_80076E90, 0, 0);
        break;
    case 2:
        D_80076F3C[2]++;
        D_80076E90 = func_800288EC(7);
        D_80076E88 = func_80031BDC(D_80076E90, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        func_800295D8(7, D_80076E88, 0, 0);
        break;
    case 3:
        D_80076F3C[3]++;
        D_80076E7C = func_8002A57C(2, 0);
        if (D_80076E7C == NULL) {
            D_80076E64 = 0;
            D_80076E48 = 0;
            break;
        }
        for (index = 0; (file = D_80076E7C[index].file) > 0; index++) {
            dest = D_80076E7C[index].dest;
            D_80076E90 = func_800288EC(file);
            ZERO_WORDS(dest, D_80076E90, i);
        }
        func_80029AFC(D_80076E7C, 0, 0);
        break;
    case 4:
        D_80076E90 = 0x2000;
        D_80076F3C[4]++;
        D_80076E88 = func_80031BDC(0x2000, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        func_8002954C(0x40, D_80076E88, D_80076E90, 0, 0);
        break;
    case 5:
        D_80076F3C[5]++;
        D_80076E90 = func_800288EC(7);
        D_80076E88 = func_80031BDC(D_80076E90, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        func_800295D8(7, D_80076E88, 1, 0);
        break;
    case 6:
        D_80076F3C[6]++;
        D_80076E7C = func_8002A57C(2, 0);
        if (D_80076E7C == NULL) {
            D_80076E64 = 0;
            D_80076E48 = 0;
            break;
        }
        for (index = 0; (file = D_80076E7C[index].file) > 0; index++) {
            dest = D_80076E7C[index].dest;
            D_80076E90 = func_800288EC(file);
            ZERO_WORDS(dest, D_80076E90, i);
        }
        func_80029AFC(D_80076E7C, 1, 0);
        break;
    case 7:
        D_80076F3C[7]++;
        if (D_80076E98 == NULL) {
            D_80076E98 = func_8002A260(4, 0);
        }
        func_80029EB0(1, D_80076E98, 0, 0, 1, 0, 0, 0, 0, 0);
        break;
    case 8:
        D_80076F3C[8]++;
        if (D_80076E98 == NULL) {
            D_80076E98 = func_8002A260(4, 0);
        }
        func_80029EB0(1, D_80076E98, 1, 0, 1, 0, 0, 0, 0, 0);
        break;
    case 11:
        if (D_80076EB8 == 2) {
            D_80076EB8 = 1;
        }
        D_80076F3C[11]++;
        if (D_80076E98 == NULL) {
            D_80076E98 = func_8002A260(4, 0);
        }
        D_80076E94 = D_80076E90 = func_800288EC(6);
        D_80076E84 = D_80076E88 = func_80031BDC(D_80076E90, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        func_800295D8(6, D_80076E98, 1, 0x100);
        break;
    case 12:
        if (D_80076EB8 == 2) {
            D_80076EB8 = 1;
        }
        D_80076F3C[12]++;
        if (D_80076E98 == NULL) {
            D_80076E98 = func_8002A260(4, 0);
        }
        D_80076E7C = func_8002A57C(2, 0);
        if (D_80076E7C == NULL) {
            D_80076E64 = 0;
            D_80076E48 = 0;
            break;
        }
        file = D_80076E7C[0].file;
        D_80076E94 = func_800288EC(file);
        D_80076EB4 = 0;
        D_80076E84 = D_80076E7C[0].dest;
        for (index = 0; file > 0; file = D_80076E7C[++index].file) {
            dest = D_80076E7C[index].dest;
            D_80076E90 = func_800288EC(file);
            ZERO_WORDS(dest, D_80076E90, i);
        }
        func_80028A94(D_80076E98);
        func_80029AFC(D_80076E7C, 1, 0x100);
        break;
    case 13:
        D_80076EB8 = 2;
        D_80076F3C[13]++;
        if (D_80076E98 != NULL) {
            func_800320E8(D_80076E98);
        }
        D_80076E98 = func_8002A260(0x40, 0);
        func_80028470(0x18, 0);
        func_800295D8(3, D_80076E98, 0, 0x200);
        break;
    }
    if (D_80076E4C == 0) {
        D_80076EAC = D_80076EB0;
        D_80076EB0 = D_80076E48;
    }
}

/* Vertical-blank tick: count frames and whole seconds. */
void func_80072428(void) {
    D_80076E5C++;
    if (++D_80076ECC >= 60) {
        D_80076ECC = 0;
        D_80076EC8++;
    }
}

/* The menu's disc change test: show the test state (the steps reached, the
 * error and the last CD command result); Start begins a test from a stopped
 * or failed state, Cross steps a waiting one, and each frame runs one step
 * of it for the other disc. Circle returns to the menu. */
void func_80072480(void) {
    s32 button;
    s32 error;
    s32 done;
    s32 step;
    s32 state;
    s32 frames;
    MovieBuffer *buffer;
    u32 *ot;

    frames = 0;
    state = 0;
    error = 0;
    done = 1;
    for (;;) {
        if (D_80077120 == &D_80077124[0]) {
            buffer = &D_80077124[1];
        } else {
            buffer = &D_80077124[0];
        }
        ot = buffer->ot;
        D_80077120 = buffer;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        func_8003700C("\n[ DISC CHANGE TEST NOW DISC %2d ]\n\n", func_80028530());
        func_800747AC(0, 0, &button);
        func_8003700C("  STATUS ");
        if (error == 1) {
            func_8003700C("[ IT IS NOT PLAY STATION DISC ]\n");
        } else if (error == 2) {
            func_8003700C("[ NOT XENOGEARS DISC ]\n");
        } else if (error == 3) {
            func_8003700C("[ NO CHANGE DISC ]\n");
        } else if (error == 4) {
            func_8003700C("[ RETRY SET DISC ]\n");
        } else {
            func_8003700C("[ NOP ]\n");
        }
        func_8003700C(D_8006FC6C);
        for (step = 0; step < 9; step++) {
            if (step < state) {
                switch (step) {
                case 0:
                    func_8003700C("  1 : NORMAL SPEED\n");
                    break;
                case 1:
                    func_8003700C("  2 : CD STOPED\n");
                    break;
                case 2:
                    func_8003700C("  3 : CD OPENED\n");
                    break;
                case 3:
                    func_8003700C("  4 : CD CLOSED\n");
                    break;
                case 4:
                    func_8003700C("  5 : SPINDLE OK\n");
                    break;
                case 5:
                    func_8003700C("  6 : TOC OK\n");
                    break;
                case 6:
                    func_8003700C("  7 : SET LOCATION OK\n");
                    break;
                case 7:
                    func_8003700C("  8 : PLAY STATION DISC OK\n");
                    break;
                case 8:
                    func_8003700C("  9 : XENOGEARS %2d DISC OK\n", func_80028530());
                    break;
                }
            } else {
                func_8003700C(" %2d :\n", step + 1);
            }
        }
        if (done) {
            func_8003700C("\n MODE %1d : NO ERROR  COUNT %6d\n", state, frames);
        } else {
            func_8003700C("\n MODE %1d : %2d ERROR COUNT %6d\n", state, done, frames);
        }
        func_8003700C(" RESULT %02x %02x %02x %02x %02x %02x %02x %02x\n", D_80076F84[0],
                      D_80076F84[1], D_80076F84[2], D_80076F84[3], D_80076F84[4], D_80076F84[5],
                      D_80076F84[6], D_80076F84[7]);
        func_8003700C("\n\n PUSH START TO TEST.\n");
        func_8003700C(" PUSH CIRCLE BUTTON TO MENU.\n");
        if (state > 0) {
            state = func_80072A08(3 - func_80028530(), state, &error, &done);
        }
        if ((D_800773AC & 0x40) && !(D_800773B4 & 0x40) && state > 0 && state < 8) {
            state++;
        }
        if ((D_800773AC & 0x800) && !(D_800773B4 & 0x800) &&
            (state == 0 || state == 9 || error != 0)) {
            error = 0;
            state = 2;
            func_8007293C();
        }
        if (D_80077394 > 0) {
            D_80077394 = 0;
        }
        func_80037324(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 8, 20, 304, 192);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 7, 19, 306, 194);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
        if (button == 2) {
            break;
        }
        frames++;
    }
}

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

/* Read `size` bytes of the host file `name` into `buffer`. */
void func_800729A8(char *name, void *buffer, s32 size) {
    s32 fd;

    fd = PCopen(name, 0, 0);
    func_8004C398(fd, buffer, size);
    PCclose(fd);
}

/* One step of the disc change test for disc `disc`: stop the drive, wait for
 * the lid to open and close and the spindle, read the TOC, seek sector 0 and
 * check the disc label, then reload the directory tables. With the host PC
 * the tables are read from its files instead. `*error` gets 1 (seek error),
 * 2 (not a Xenogears disc) or 3 (wrong disc); `*done` the command result.
 * Returns the next state. */
s32 func_80072A08(s32 disc, s32 state, s32 *error, s32 *done) {
    u32 label[4] = {0, 0, 0, 0};
    CdlLOC loc;
    s32 result;

    CdIntToPos(0, &loc);
    result = 1;
    if (*error == 0) {
        if (func_8002C3D8() != 0 && state < 9) {
            if (disc == 1) {
                func_800729A8("c:\\work\\cdrom.mdg", D_8004FDF0, 0x8000);
                func_800729A8("c:\\work\\cdrom.fid", D_8004FDF4, 0x7A);
                func_800729A8("c:\\work\\cdrom.fnd", D_8004FE48, 0x40000);
            } else {
                func_800729A8("c:\\work\\cdrom2.mdg", D_8004FDF0, 0x8000);
                func_800729A8("c:\\work\\cdrom2.fid", D_8004FDF4, 0x7A);
                func_800729A8("c:\\work\\cdrom2.fnd", D_8004FE48, 0x40000);
            }
            state = 9;
        } else {
            switch (state) {
            case 1:
                result = CdControlB(CdlStop, NULL, D_80076F84);
                if (result != 0) {
                    state++;
                }
                break;
            case 2:
                CdControlB(CdlNop, NULL, D_80076F84);
                if (D_80076F84[0] & 0x10) {
                    state++;
                }
                break;
            case 3:
                CdControlB(CdlNop, NULL, D_80076F84);
                if (!(D_80076F84[0] & 0x10)) {
                    state++;
                }
                break;
            case 4:
                result = CdControlB(CdlNop, NULL, D_80076F84);
                if (D_80076F84[0] & 2) {
                    if (result != 0) {
                        state++;
                    }
                }
                break;
            case 5:
                result = CdControlB(CdlGetTN, NULL, D_80076F84);
                if (result != 0) {
                    state++;
                }
                break;
            case 6:
                result = CdControlB(CdlSetloc, (u8 *)&loc, D_80076F84);
                if (result != 0) {
                    state++;
                }
                break;
            case 7:
                result = CdControlB(CdlSeekL, NULL, D_80076F84);
                if ((D_80076F84[0] & 1) && (D_80076F84[1] & 0x40) && result == 0) {
                    *error = 1;
                } else if (result != 0) {
                    state++;
                }
                if (result == 0) {
                    state = 5;
                }
                break;
            case 8:
                func_8002A428(0xA0);
                func_80028A60(0);
                VSync(3);
                func_8002954C(0x17, label, 0x10, 0, 0);
                func_80028A60(0);
                if (label[1] == 0x4E45585F) { /* "_XEN" */
                    if (((u8 *)label)[3] == disc + '0') {
                        func_8002954C(0x18, D_8004FDF0, 0x8000, 0, 0);
                        state++;
                        func_80028A60(0);
                        func_8002954C(0x28, D_8004FDF4, 0x7A, 0, 0);
                        func_80028A60(0);
                        *error = 0;
                    } else {
                        *error = 3;
                    }
                } else {
                    *error = 2;
                }
                break;
            }
        }
        *done = result;
    }
    return state;
}

/* Set up the menu backdrop quads of both buffers at (x, y), w by h, with
 * random dark blue corner fades. */
void func_80072D84(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_80076F8C[i].r = (func_80074AF0() & 0xFF) / 32 + 8;
        D_80076F8C[i].g = 8;
        D_80076F8C[i].b = (func_80074AF0() & 0xFF) / 3 + 16;
        D_80076F9C[i].r = (func_80074AF0() & 0xFF) / 32 + 8;
        D_80076F9C[i].g = 8;
        D_80076F9C[i].b = (func_80074AF0() & 0xFF) / 3 + 16;
    }
    for (i = 0; i < 4; i++) {
        D_80076FAC[i] = 0;
        D_80076FBC[i] = (func_80074AF0() & 0xFF) + 32;
    }
    SetPolyG4(poly0);
    SetSemiTrans(poly0, 0);
    poly0->x0 = x;
    poly0->y0 = y;
    poly0->x1 = x + w;
    poly0->y1 = y;
    poly0->x2 = x;
    poly0->y2 = y + h;
    poly0->x3 = x + w;
    poly0->y3 = y + h;
    SetPolyG4(poly1);
    SetSemiTrans(poly1, 0);
    poly1->x0 = x;
    poly1->y0 = y;
    poly1->x1 = x + w;
    poly1->y1 = y;
    poly1->x2 = x;
    poly1->y2 = y + h;
    poly1->x3 = x + w;
    poly1->y3 = y + h;
}

#ifdef NON_MATCHING
/* Add the menu backdrop to `ot`: a gouraud quad at (x, y), w by h, whose
 * corner colors each fade toward a new random color. Same instructions,
 * different register allocation: the original keeps the corner index and its
 * word offset apart, spilling the offset and the from-green pointer. */
void func_80072F98(u32 *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h) {
    s32 i;
    s32 r;
    s32 g;
    s32 b;

    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    for (i = 0; i < 4; i++) {
        if (++D_80076FAC[i] > D_80076FBC[i]) {
            D_80076FAC[i] = 0;
            D_80076FBC[i] = (func_80074AF0() & 0xFF) + 32;
            D_80076F8C[i].r = D_80076F9C[i].r;
            D_80076F8C[i].g = D_80076F9C[i].g;
            D_80076F8C[i].b = D_80076F9C[i].b;
            D_80076F9C[i].r = (func_80074AF0() & 0xFF) / 32 + 8;
            D_80076F9C[i].g = 8;
            D_80076F9C[i].b = (func_80074AF0() & 0xFF) / 3 + 16;
        }
        r = D_80076F8C[i].r +
            (D_80076F9C[i].r - D_80076F8C[i].r) * D_80076FAC[i] / D_80076FBC[i];
        g = D_80076F8C[i].g +
            (D_80076F9C[i].g - D_80076F8C[i].g) * D_80076FAC[i] / D_80076FBC[i];
        b = D_80076F8C[i].b +
            (D_80076F9C[i].b - D_80076F8C[i].b) * D_80076FAC[i] / D_80076FBC[i];
        switch (i) {
        case 0:
            poly->r0 = r;
            poly->g0 = g;
            poly->b0 = b;
            break;
        case 1:
            poly->r1 = r;
            poly->g1 = g;
            poly->b1 = b;
            break;
        case 2:
            poly->r2 = r;
            poly->g2 = g;
            poly->b2 = b;
            break;
        case 3:
            poly->r3 = r;
            poly->g3 = g;
            poly->b3 = b;
            break;
        }
    }
    poly->tag = (poly->tag & 0xFF000000) | (*ot & 0xFFFFFF);
    *ot = (*ot & 0xFF000000) | ((u32)poly & 0xFFFFFF);
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80072F98);
#endif

/* Set up the menu frame quads of both buffers at (x, y), w by h, with random
 * pale yellow corner fades. */
void func_80073328(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_80076FCC[i].r = 0xFF;
        D_80076FCC[i].g = 0xFF;
        D_80076FCC[i].b = (func_80074AF0() & 0x3F) - 0x42;
        D_80076FDC[i].r = 0xFF;
        D_80076FDC[i].g = 0xFF;
        D_80076FDC[i].b = (func_80074AF0() & 0x3F) - 0x42;
    }
    for (i = 0; i < 4; i++) {
        D_80076FEC[i] = 0;
        D_80076FFC[i] = (func_80074AF0() & 0xFF) + 32;
    }
    SetPolyG4(poly0);
    poly0->x0 = x;
    poly0->y0 = y;
    poly0->x1 = x + w;
    poly0->y1 = y;
    poly0->x2 = x;
    poly0->y2 = y + h;
    poly0->x3 = x + w;
    poly0->y3 = y + h;
    SetPolyG4(poly1);
    poly1->x0 = x;
    poly1->y0 = y;
    poly1->x1 = x + w;
    poly1->y1 = y;
    poly1->x2 = x;
    poly1->y2 = y + h;
    poly1->x3 = x + w;
    poly1->y3 = y + h;
}

#ifdef NON_MATCHING
/* Add the menu frame to `ot`: a gouraud quad at (x, y), w by h, whose corner
 * colors each fade toward a new random pale yellow. Same instructions but
 * for register allocation and one difference: the original reloads each
 * from-color after its division instead of keeping it. */
void func_800734B8(u32 *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h) {
    s32 i;
    s32 r;
    s32 g;
    s32 b;

    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    for (i = 0; i < 4; i++) {
        if (++D_80076FEC[i] > D_80076FFC[i]) {
            D_80076FEC[i] = 0;
            D_80076FFC[i] = (func_80074AF0() & 0xFF) + 32;
            D_80076FCC[i].r = D_80076FDC[i].r;
            D_80076FCC[i].g = D_80076FDC[i].g;
            D_80076FCC[i].b = D_80076FDC[i].b;
            D_80076FDC[i].r = 0xFF;
            D_80076FDC[i].g = 0xFF;
            D_80076FDC[i].b = (func_80074AF0() & 0x3F) - 0x42;
        }
        r = D_80076FCC[i].r +
            (D_80076FDC[i].r - D_80076FCC[i].r) * D_80076FEC[i] / D_80076FFC[i];
        g = D_80076FCC[i].g +
            (D_80076FDC[i].g - D_80076FCC[i].g) * D_80076FEC[i] / D_80076FFC[i];
        b = D_80076FCC[i].b +
            (D_80076FDC[i].b - D_80076FCC[i].b) * D_80076FEC[i] / D_80076FFC[i];
        switch (i) {
        case 0:
            poly->r0 = r;
            poly->g0 = g;
            poly->b0 = b;
            break;
        case 1:
            poly->r1 = r;
            poly->g1 = g;
            poly->b1 = b;
            break;
        case 2:
            poly->r2 = r;
            poly->g2 = g;
            poly->b2 = b;
            break;
        case 3:
            poly->r3 = r;
            poly->g3 = g;
            poly->b3 = b;
            break;
        }
    }
    poly->tag = (poly->tag & 0xFF000000) | (*ot & 0xFFFFFF);
    *ot = (*ot & 0xFF000000) | ((u32)poly & 0xFFFFFF);
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_800734B8);
#endif

#ifdef NON_MATCHING
/* Mode 6 entry. Load the movie library below the heap top and open it at
 * 320x256; with a movie request (8004fe44..47) play it and select the next
 * mode. Otherwise run the development menu: movie type, number, start and
 * end frame (Circle seeks them), channel, colour depth, rows drawn, rewind,
 * then the movie test, CD-ROM monitor, CD-ROM check, FAT check, disc change
 * test and a return to the kernel. Square and Cross speed up the frame
 * settings. The menu cursor is kept across the screens it opens. The unused
 * name reproduces the original's frame. Draft: the register count and most
 * code match; the constant 1 of the setup stores gets another register
 * (scheduling differs), and the request byte 8004fe44 is reread in the
 * original where this source reuses the value. */
void func_800737EC(void) {
    char name[8] = "trouble";
    s32 button;
    s32 step;
    s32 dir;
    s32 line;
    s32 last;
    s32 cursor;
    void *top;
    void *library;
    MovieBuffer *buffer;
    u32 *ot;

    func_80032498(4, 0);
    func_80028470(0x18, 0);
    func_80038D18(0, 0);
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    top = func_80031BDC(4, 1);
    library = func_80031BDC(((u32)top & 0xFFFFFF) - 0x1D3008, 1);
    func_800320E8(top);
    func_800295D8(1, library, 0, 0);
    func_80028A60(0);
    func_801D3538(320, 256, 128, 16, 32, 0x800, 3);
    D_80077450 = func_8002C3D8();
    D_800773A0 = -1;
    D_8007739C = 0xC80;
    D_80077394 = 0;
    D_800773B0 = 0;
    D_80077454 = 1;
    D_80077448 = 1;
    D_8007711C = 0;
    D_800773A4 = 1;
    D_80077398 = 1;
    D_80077444 = 2;
    D_8007743C = 0;
    D_800773A8 = 0;
    D_80077438 = 0;
    SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 320, 240);
    SetDefDispEnv(&D_80077124[0].disp, 0, 240, 320, 240);
    SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&D_80077124[1].disp, 0, 0, 320, 240);
    D_80077124[0].draw.dtd = 1;
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.dtd = 1;
    D_80077124[1].draw.isbg = 1;
    D_80077124[0].draw.r0 = 0;
    D_80077124[0].draw.g0 = 0;
    D_80077124[0].draw.b0 = 0;
    D_80077124[0].disp.isinter = 0;
    D_80077124[1].draw.r0 = 0;
    D_80077124[1].draw.g0 = 0;
    D_80077124[1].draw.b0 = 0;
    D_80077124[1].disp.isinter = 0;
    D_80077124[0].disp.screen.x = 0;
    D_80077124[0].disp.screen.y = 10;
    D_80077124[0].disp.screen.w = 256;
    D_80077124[0].disp.screen.h = 216;
    D_80077124[1].disp.screen.x = 0;
    D_80077124[1].disp.screen.y = 10;
    D_80077124[1].disp.screen.w = 256;
    D_80077124[1].disp.screen.h = 216;
    D_80077120 = &D_80077124[0];
    D_8007744C = 0;
    PutDrawEnv(&D_80077124[0].draw);
    PutDispEnv(&D_80077120->disp);
    if (D_80077450 != 0) {
        VSync(2);
        D_800773AC = func_8003569C(0);
    } else {
        D_800773AC = 0;
    }
    if (D_8004FE44 != 0xFF && !(D_800773AC & 0x100)) {
        D_80077440 = 0;
        D_80077398 = 1;
        D_800773A4 = 1;
        D_80077448 = D_8004FE44 & 0x7F;
        D_8007711C = D_8004FE45;
        D_8007739C = (D_8004FE44 & 0x80) ? D_80062514 : 0xE9;
        func_800763BC(D_8004FE47);
        func_801D43B0();
        func_800320E8(library);
        func_8001996C(D_8004FE46);
        func_80019ACC(0);
    }
    func_80072D84((POLY_G4 *)D_80077124[0].box, (POLY_G4 *)D_80077124[1].box, 0, 0, 0, 0);
    func_80073328((POLY_G4 *)D_80077124[0].frame, (POLY_G4 *)D_80077124[1].frame, 0, 0, 0, 0);
    func_800374E8(16, 16, 640, 240, 0x400, 0, 640, 0, 640, 256, 0);
    D_80077440 = 1;
    SetDispMask(1);
    for (;;) {
        if (D_80077120 == &D_80077124[0]) {
            buffer = &D_80077124[1];
        } else {
            buffer = &D_80077124[0];
        }
        ot = buffer->ot;
        D_80077120 = buffer;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        if (D_80077450 == 0) {
            func_8003700C("  [ MOVIE CD-ROM MODE1 DISK %1d ]  \n\n", func_80028530());
        } else if (D_80077450 == -1) {
            func_8003700C("  [ MOVIE CD-ROM MODE2 DISK %1d ]  \n\n", func_80028530());
        } else {
            func_8003700C("  [ MOVIE PC HDD MODE  DISK %1d ]  \n\n", func_80028530());
        }
        step = 1;
        func_8003700C("    ERROR %2d Sect %2d:%2d FM%3d\n", D_8005A4DC, D_8005A4A8, D_8005A4B4,
                      D_8005A4B8);
        func_8003700C("    LesMem%2d NoMem%2d Skp%3d\n", D_8005A49C, D_8005A4A4, D_801E89D4,
                      D_80062514);
        dir = func_800747AC(0, 13, &button);
        if (D_800773AC & 0x10) {
            step = 32;
        }
        if (D_800773AC & 0x80) {
            step <<= 7;
        }
        if (D_80077118 == 0 && dir != 0) {
            D_80077448 += dir;
            if (D_80077448 < 0) {
                D_80077448 = 2;
            }
            if (D_80077448 >= 3) {
                D_80077448 = 0;
            }
            D_800773A4 = 1;
            D_800773A8 = 0;
            D_8007743C = 0;
            D_80077444 = 2;
        }
        if (D_80077118 == 1 && dir != 0) {
            D_8007711C += dir;
            if (D_8007711C < 0) {
                D_8007711C = 63;
            }
            if (D_8007711C >= 64) {
                D_8007711C = 0;
            }
            D_800773A4 = 1;
            D_800773A8 = 0;
            D_8007743C = 0;
            D_80077444 = 2;
        }
        if (D_80077118 == 2 && dir != 0) {
            D_800773A4 += dir * step;
            if (D_800773A4 <= 0) {
                D_800773A4 = 1;
            }
            if (D_800773A4 >= 0x2000) {
                D_800773A4 = 0x1FFF;
            }
            if (D_800773A4 > D_8007739C) {
                D_800773A4 = D_8007739C;
            }
            D_80077444 = 1;
        }
        if (D_80077118 == 2 && button == 2 && D_80077444 == 1) {
            D_800773A8 = func_80074BA4(D_800773A4);
            if (D_800773A4 >= D_8007739C) {
                D_8007739C = D_800773A4;
                D_8007743C = 0;
            }
            D_80077444 = 2;
        }
        if (D_80077118 == 3 && dir != 0) {
            D_8007739C += dir * step;
            if (D_8007739C <= 0) {
                D_8007739C = 1;
            }
            if (D_8007739C >= 0x2000) {
                D_8007739C = 0x1FFF;
            }
            if (D_8007739C < D_800773A4) {
                D_8007739C = D_800773A4;
            }
            D_8007743C = 0;
        }
        if (D_80077118 == 3 && button == 2 && D_8007743C == 0) {
            last = func_8007519C();
            if (last >= 0) {
                D_8007739C = last;
                D_8007743C = 1;
                if (D_800773A4 >= last) {
                    D_800773A4 = last;
                    D_80077444 = 1;
                }
            } else {
                D_8007743C = 2;
            }
        }
        if (D_80077118 == 4 && dir != 0) {
            D_80077398 += dir;
            if (D_80077398 < 0) {
                D_80077398 = 7;
            }
            if (D_80077398 >= 8) {
                D_80077398 = 0;
            }
        }
        if (D_80077118 == 5 && dir != 0) {
            D_80077454 = 1 - D_80077454;
        }
        if (D_80077118 == 6) {
            if (dir != 0) {
                D_800773A0 = (D_800773A0 + dir * step) & 0xFF;
            }
            if (button == 2) {
                D_800773A0 = -1;
            }
        }
        if (D_80077118 == 7 && dir != 0) {
            D_80077438 = 1 - D_80077438;
        }
        for (line = 0; line < 14; line++) {
            func_8003700C(D_80077118 == line ? "  >" : "   ");
            switch (line) {
            case 0:
                func_8003700C(" MOVIE TYPE   ");
                if (D_80077448 == 0) {
                    func_8003700C("PICTURE ONLY\n");
                } else if (D_80077448 == 1) {
                    func_8003700C("PICTURE+ADPCM\n");
                } else if (D_80077448 == 2) {
                    func_8003700C("ADPCM ONLY\n");
                }
                break;
            case 1:
                func_8003700C(" MOVIE NUMBER %4d\n\n", D_8007711C);
                break;
            case 2:
                func_8003700C(" START FRAME  %4d ", D_800773A4);
                if (D_80077444 == 1) {
                    func_8003700C("SET");
                }
                if (D_80077444 == 2) {
                    if (D_800773A8 < 0) {
                        func_8003700C("EOF");
                    } else {
                        func_8003700C("+%4dSECT", D_800773A8);
                    }
                }
                func_8003700C("\n");
                break;
            case 3:
                func_8003700C(" END   FRAME  %4d ", D_8007739C);
                if (D_8007743C == 0) {
                    func_8003700C("SET");
                }
                if (D_8007743C == 2) {
                    func_8003700C("???");
                }
                func_8003700C("\n");
                break;
            case 4:
                func_8003700C(" MOVIE CHANNEL %3d\n", D_80077398);
                break;
            case 5:
                func_8003700C(" SCREEN MODE  ");
                func_8003700C(D_80077454 ? "24 BIT COLOR" : "16 BIT COLOR");
                func_8003700C("\n");
                break;
            case 6:
                func_8003700C(" SCREEN DRAW  ");
                if (D_800773A0 < 0) {
                    func_8003700C("ALL");
                } else {
                    func_8003700C("%3d", D_800773A0);
                }
                func_8003700C("\n");
                break;
            case 7:
                func_8003700C(" REWIND       ");
                func_8003700C(D_80077438 ? "ON" : "OFF");
                func_8003700C("\n\n");
                break;
            case 8:
                func_8003700C(" MOVIE START.\n\n");
                break;
            case 9:
                func_8003700C(" CD-ROM MONITOR.\n\n");
                break;
            case 10:
                func_8003700C(" CD-ROM CHECK.\n");
                break;
            case 11:
                func_8003700C(" FAT CHECK.\n\n");
                break;
            case 12:
                func_8003700C(" [DISC CHANGE.]\n");
                break;
            case 13:
                func_8003700C(" [RETURN TO KERNEL.]\n");
                break;
            }
        }
        if (D_80077394 > 0) {
            func_8003278C(1, 0, 6, 0x808D);
        }
        func_80037324(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 20, 12, 284, 198);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 19, 11, 286, 200);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
        cursor = D_80077118;
        if ((cursor == 0 || cursor == 1 || cursor == 4 || cursor == 5 || cursor == 7 ||
             cursor == 8) &&
            button == 2) {
            D_8005A49C = 0;
            D_8005A4A4 = 0;
            D_8005A4A8 = 0;
            D_8005A4B4 = 0;
            func_8007625C();
            D_800773AC = -1;
        }
        if (D_80077118 == 9 && button == 2) {
            func_80075534();
        }
        if (D_80077118 == 10 && button == 2) {
            func_800704E8();
        }
        if (D_80077118 == 11 && button == 2) {
            func_80075D8C();
        }
        if (D_80077118 == 12 && button == 2) {
            func_80072480();
        }
        if (D_80077118 == 13 && button == 2) {
            func_800320E8(library);
            func_80019ACC(0);
        }
        D_80077118 = cursor;
        func_80019CA0();
    }
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_800737EC);
#endif

/* Menu input: read controller port 0 with a repeat after 16 frames held; Up
 * and Down move the cursor between `first` and `last`, wrapping; the four
 * face buttons report 1..4 in `button`; Select toggles the monitor and Start
 * the statistics. Returns 1 for Right, -1 for Left, else 0. */
s32 func_800747AC(s32 first, s32 last, s32 *button) {
    D_800773B4 = D_800773AC;
    D_800773AC = func_8003569C(0);
    if (D_800773B4 == D_800773AC && D_800773B4 != 0) {
        D_80076EEC++;
        if (D_80076EF0 < D_80076EEC) {
            D_800773B4 = 0;
            D_80076EF0 = 1;
            D_80076EEC = 0;
        }
    } else {
        D_80076EF0 = 16;
        D_80076EEC = 0;
    }
    if (!(D_800773B4 & 0x1000) && (D_800773AC & 0x1000)) {
        if (--D_80077118 < first) {
            D_80077118 = last;
        }
    }
    if (!(D_800773B4 & 0x4000) && (D_800773AC & 0x4000)) {
        if (++D_80077118 > last) {
            D_80077118 = first;
        }
    }
    *button = 0;
    if (!(D_800773B4 & 0x10) && (D_800773AC & 0x10)) {
        *button = 1;
    }
    if (!(D_800773B4 & 0x20) && (D_800773AC & 0x20)) {
        *button = 2;
    }
    if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40)) {
        *button = 3;
    }
    if (!(D_800773B4 & 0x80) && (D_800773AC & 0x80)) {
        *button = 4;
    }
    if (!(D_800773B4 & 0x100) && (D_800773AC & 0x100)) {
        D_800773B0 = 1 - D_800773B0;
    }
    if (!(D_800773B4 & 0x800) && (D_800773AC & 0x800)) {
        D_80077394 = 1 - D_80077394;
    }
    if (!(D_800773B4 & 0x2000) && (D_800773AC & 0x2000)) {
        return 1;
    }
    if (!(D_800773B4 & 0x8000) && (D_800773AC & 0x8000)) {
        return -1;
    }
    return 0;
}

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

#ifdef NON_MATCHING
/* The sector of the selected movie where `frame` starts: guess from the
 * first sector's sectors per frame, correct once, then step sector by
 * sector (frame by frame on headers) until a header names the frame.
 * Returns 0 for the first frame, -1 past the file; an index past the list
 * returns without a value, as the original does. Draft: the control flow
 * is right, the register allocation and header reloads are not (0x28 short). */
s32 func_80074BA4(s32 frame) {
    u8 buffer[0x1000];
    s32 file;
    char *name;
    s32 sector_size;
    s32 read_size;
    s32 header;
    s32 lower;
    s32 fd;
    s32 count;
    s32 per_frame;
    s32 pos;
    s32 next;
    s32 total;
    s32 guess;
    MovieSector *h;

    lower = 0;
    if (D_80077448 == 0) {
        func_80028470(0x18, 0);
        if (D_8007711C >= func_80028928(2)) {
            return;
        }
        file = D_8007711C + 3;
        name = func_80028998(file);
        sector_size = 0x800;
        read_size = 0x20;
        header = 0;
    } else if (D_80077448 == 1) {
        func_80028470(0x18, 1);
        if (D_8007711C >= func_80028928(1)) {
            return;
        }
        file = D_8007711C + 2;
        name = func_80028998(file);
        sector_size = 0x920;
        read_size = 0x28;
        header = 8;
    } else if (D_80077448 == 2) {
        return -1;
    }
    if (frame < 2) {
        return 0;
    }
    if (func_8002C3D8() != 0) {
        fd = PCopen(name, 0, 0);
        PClseek(fd, 0, 2);
        PClseek(fd, 0, 0);
        func_8004C398(fd, buffer, read_size);
        h = (MovieSector *)(buffer + header);
        per_frame = h->sectors;
        pos = (frame - 1) * per_frame - (frame - 1) / 4;
        PClseek(fd, pos * sector_size, 0);
        count = func_8004C398(fd, buffer, read_size);
        if (h->frame != frame || count == 0) {
            if (h->frame < frame && pos > 0 && count != 0) {
                lower = pos;
            }
            guess = (frame - 1) * per_frame - (frame - 1) / 4;
            pos = guess + guess / 7;
            PClseek(fd, pos * sector_size, 0);
            h = (MovieSector *)(buffer + header);
            count = func_8004C398(fd, buffer, read_size);
            if (h->frame == frame && count != 0) {
                goto found;
            }
            if (h->frame < frame && lower < pos && count != 0) {
                lower = pos;
            }
            next = (per_frame - 1) * (frame - 1);
            if (lower > 0) {
                next = lower;
            }
            goto scan;
        }
    found:
        if (h->sector != 0) {
            next = pos - h->sector - 2;
        scan:
            h = (MovieSector *)(buffer + header);
            do {
                pos = next;
                PClseek(fd, next * sector_size, 0);
                count = func_8004C398(fd, buffer, read_size);
                next = pos + 1;
                if (h->magic == 0x160) {
                    next = pos + (h->sectors - h->sector);
                }
            } while (h->frame != frame && count > 0);
            if (count == 0) {
                pos = -1;
            }
        }
        PCclose(fd);
        return pos;
    }
    h = (MovieSector *)buffer;
    func_8002954C(func_800289D0(file), buffer, 0x800, 0, 0);
    func_80028A60(0);
    total = (func_800288EC(file) + sector_size - 1) / sector_size;
    pos = (frame - 1) * h->sectors - (frame - 1) / 4;
    func_8002954C(func_800289D0(file) + pos, buffer, 0x800, 0, 0);
    func_80028A60(0);
    if (h->frame == frame && pos < total) {
        goto found_disc;
    }
    if (h->frame < frame && pos > 0 && pos < total) {
        lower = pos;
    }
    guess = (frame - 1) * h->sectors - (frame - 1) / 4;
    pos = guess + guess / 7;
    func_8002954C(func_800289D0(file) + pos, buffer, 0x800, 0, 0);
    func_80028A60(0);
    if (h->frame == frame && pos < total) {
    found_disc:
        if (h->sector != 0) {
            next = pos - h->sector - 2;
            goto scan_disc;
        }
    } else {
        if (h->frame < frame && lower < pos && pos < total) {
            lower = pos;
        }
        next = (h->sectors - 1) * (frame - 1);
        if (lower > 0) {
            next = lower;
        }
    scan_disc:
        do {
            pos = next;
            func_8002954C(func_800289D0(file) + pos, buffer, 0x800, 0, 0);
            func_80028A60(0);
            next = pos + 1;
            if (h->magic == 0x160) {
                next = pos + (h->sectors - h->sector);
            }
        } while (h->frame != frame && next < total);
    }
    if (next >= total) {
        pos = -1;
    }
    return pos;
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80074BA4);
#endif

#ifdef NON_MATCHING
/* The last frame number of the selected movie, read from the header of its
 * last sector (host PC file or disc), or -1. An index past the list returns
 * without a value, and a kind above 2 reads with unset parameters, as the
 * original does. Same instructions but for register allocation (frames and
 * the read size swap $s3/$s4; the sector count takes $s0). */
s32 func_8007519C(void) {
    u8 buffer[0x1000];
    s32 file;
    char *name;
    s32 sector_size;
    s32 read_size;
    s32 frames;
    s32 header;
    s32 fd;
    s32 size;
    s32 sectors;

    frames = -1;
    if (D_80077448 == 0) {
        func_80028470(0x18, 0);
        if (D_8007711C >= func_80028928(2)) {
            return;
        }
        file = D_8007711C + 3;
        name = func_80028998(file);
        sector_size = 0x800;
        read_size = 0x20;
        header = 0;
    } else if (D_80077448 == 1) {
        func_80028470(0x18, 1);
        if (D_8007711C >= func_80028928(1)) {
            return;
        }
        file = D_8007711C + 2;
        name = func_80028998(file);
        sector_size = 0x920;
        read_size = 0x28;
        header = 8;
    } else if (D_80077448 == 2) {
        return -1;
    }
    if (func_8002C3D8() != 0) {
        fd = PCopen(name, 0, 0);
        size = PClseek(fd, 0, 2);
        PClseek(fd, 0, 0);
        PClseek(fd, size - sector_size, 0);
        func_8004C398(fd, buffer, read_size);
        if (((MovieSector *)(buffer + header))->magic == 0x160) {
            frames = ((MovieSector *)(buffer + header))->frame;
        }
        PCclose(fd);
        return frames;
    }
    sectors = (func_800288EC(file) + sector_size - 1) / sector_size;
    func_8002954C(func_800289D0(file) + sectors - 1, buffer, 0x800, 0, 0);
    func_80028A60(0);
    if (((MovieSector *)buffer)->magic == 0x160) {
        frames = ((MovieSector *)buffer)->frame;
    }
    return frames;
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_8007519C);
#endif

/* Load the three sound effect banks from the host PC, waiting for each
 * transfer to the sound memory. */
void func_800753B8(void) {
    void *bank;

    bank = func_80028570("c:\\work\\cdrom\\sound\\wave\\main_se.wd", 0);
    func_80037FD8(bank, 0);
    while (func_8003BDFC(0) != 0) {
    }
    func_800320E8(bank);
    bank = func_80028570("c:\\work\\cdrom\\sound\\wave\\bat_se.wd", 0);
    func_80037FD8(bank, 0);
    while (func_8003BDFC(0) != 0) {
    }
    func_800320E8(bank);
    bank = func_80028570("c:\\work\\cdrom\\sound\\wave\\gear_se.wd", 0);
    func_80037FD8(bank, 0);
    while (func_8003BDFC(0) != 0) {
    }
    func_800320E8(bank);
}

#ifdef NON_MATCHING
/* Load the battle sound bank from the host PC, waiting for its transfer,
 * then the battle music sequence. The instructions match; the original
 * rodata has a non-zero padding byte (0x08) after "battle2.smd". */
void func_8007548C(void) {
    void *bank;

    bank = func_80028570("c:\\work\\cdrom\\sound\\wave\\battle2.wd", 0);
    func_80037FD8(bank, 0);
    while (func_8003BDFC(0) != 0) {
    }
    func_800320E8(bank);
    D_8007700C = func_80039850(func_80028570("c:\\work\\cdrom\\sound\\music\\battle2.smd", 0));
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_8007548C);
#endif

void func_80075508(void) {
    func_80039A80(D_8007700C, 0x7F, 0);
}

/* The menu's sector monitor: at 640x240, dump 192 bytes of the current
 * sector (Up/Down by a row, Triangle/Cross by twelve) with its position;
 * Left/Right step the sector by one, L1/R1 by 75 (a second) and L2/R2 by
 * 4500 (a minute), rereading it when it changes. Circle returns to the
 * 320-wide menu. The original reads the menu cursor before the exit test and
 * stores it back after. */
void func_80075534(void) {
    CdlLOC loc;
    s32 button;
    u8 *buffer;
    s32 sector;
    s32 row;
    s32 i;
    u8 *hex;
    u8 *text;
    u8 c;
    s32 cursor;
    MovieBuffer *draw;
    u32 *ot;

    func_80074B58();
    buffer = func_80031BDC(0x800, 0);
    if (buffer == NULL) {
        return;
    }
    SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 640, 240);
    SetDefDispEnv(&D_80077124[0].disp, 0, 240, 640, 240);
    SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 640, 240);
    SetDefDispEnv(&D_80077124[1].disp, 0, 0, 640, 240);
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.isbg = 1;
    D_80077124[0].disp.screen.x = 0;
    D_80077124[0].disp.screen.y = 10;
    D_80077124[0].disp.screen.w = 256;
    D_80077124[0].disp.screen.h = 216;
    D_80077124[1].disp.screen.x = 0;
    D_80077124[1].disp.screen.y = 10;
    D_80077124[1].disp.screen.w = 256;
    D_80077124[1].disp.screen.h = 216;
    func_8002954C(D_80076EFC, buffer, 0x800, 0, 0);
    func_80028A60(0);
    for (;;) {
        if (D_80077120 == &D_80077124[0]) {
            draw = &D_80077124[1];
        } else {
            draw = &D_80077124[0];
        }
        ot = draw->ot;
        D_80077120 = draw;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        func_8003700C("\n[ MONITOR ]\n");
        func_800747AC(0, 0, &button);
        sector = D_80076EFC;
        if (!(D_800773B4 & 0x1000) && (D_800773AC & 0x1000) && D_80076F00 > 0) {
            D_80076F00--;
        }
        if (!(D_800773B4 & 0x10) && (D_800773AC & 0x10)) {
            D_80076F00 -= 12;
            if (D_80076F00 < 0) {
                D_80076F00 = 0;
            }
        }
        if (!(D_800773B4 & 0x4000) && (D_800773AC & 0x4000) && D_80076F00 < 116) {
            D_80076F00++;
        }
        if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40)) {
            D_80076F00 += 12;
            if (D_80076F00 >= 116) {
                D_80076F00 = 116;
            }
        }
        if (!(D_800773B4 & 0x8000) && (D_800773AC & 0x8000) && D_80076EFC > 0) {
            D_80076EFC--;
        }
        if (!(D_800773B4 & 0x2000) && (D_800773AC & 0x2000)) {
            D_80076EFC++;
        }
        if (!(D_800773B4 & 4) && (D_800773AC & 4)) {
            D_80076EFC -= 75;
            if (D_80076EFC < 0) {
                D_80076EFC = 0;
            }
        }
        if (!(D_800773B4 & 8) && (D_800773AC & 8)) {
            D_80076EFC += 75;
        }
        if (!(D_800773B4 & 1) && (D_800773AC & 1)) {
            D_80076EFC -= 4500;
            if (D_80076EFC < 0) {
                D_80076EFC = 0;
            }
        }
        if (!(D_800773B4 & 2) && (D_800773AC & 2)) {
            D_80076EFC += 4500;
        }
        if (sector != D_80076EFC) {
            func_8002954C(D_80076EFC, buffer, 0x800, 0, 0);
            func_80028A60(0);
        }
        CdIntToPos(D_80076EFC, &loc);
        func_8003700C("ABSPOS %8d POS %04x\nMINUTE %02x SECOND %02x SECTOR %02x\n\n", D_80076EFC,
                      D_80076F00 * 16, loc.minute, loc.second, loc.sector);
        text = buffer + D_80076F00 * 16;
        hex = text;
        for (row = 0; row < 12; row++) {
            func_8003700C("%03x:", (row + D_80076F00) * 16);
            for (i = 0; i < 15; i++) {
                func_8003700C("%02x ", *hex);
                hex++;
            }
            func_8003700C("%02x", *hex);
            hex++;
            func_8003700C(":");
            for (i = 0; i < 16; i++) {
                c = *text;
                if (c >= 0x20 && c < 0x7E) {
                    func_8003700C("%c", c);
                } else {
                    func_8003700C(" ");
                }
                text++;
            }
            func_8003700C(D_8007042C);
        }
        func_8003700C(D_80070430);
        if (D_80077394 > 0) {
            func_8003278C(1, 0, 6, 0x808D);
        }
        func_80037324(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 8, 20, 624, 160);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 7, 19, 626, 162);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
        cursor = D_80077118;
        if (button == 2) {
            break;
        }
        D_80077118 = cursor;
    }
    func_800320E8(buffer);
    func_80074B58();
    SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 320, 240);
    SetDefDispEnv(&D_80077124[0].disp, 0, 240, 320, 240);
    SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&D_80077124[1].disp, 0, 0, 320, 240);
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.isbg = 1;
    D_80077124[0].disp.screen.x = 0;
    D_80077124[0].disp.screen.y = 10;
    D_80077124[0].disp.screen.w = 256;
    D_80077124[0].disp.screen.h = 216;
    D_80077124[1].disp.screen.x = 0;
    D_80077124[1].disp.screen.y = 10;
    D_80077124[1].disp.screen.w = 256;
    D_80077124[1].disp.screen.h = 216;
}

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_8007042C);

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_80070430);

/* The first sector of directory record `index` (bytes 3..6). */
u32 func_80075D4C(s32 index) {
    u8 *record;

    record = D_8004FDF0 + index * 7;
    return ((record[6] << 24) + (record[5] << 16) + (record[4] << 8)) | record[3];
}

#ifdef NON_MATCHING
/* The menu's FAT check: list twenty directory records from the cursor
 * (Up/Down by one, Triangle/Cross by twenty) with their first sector and
 * their size or, toggled by L1, their host file name; R1 switches between
 * decimal and hexadecimal. Circle returns to the menu. The instructions
 * match; the original rodata has two non-zero padding bytes (0x0894) after
 * "Size%9d\n" that a C literal cannot reproduce. */
void func_80075D8C(void) {
    s32 directory;
    s32 offset;
    s32 button;
    s32 top;
    s32 hex;
    s32 names;
    s32 index;
    s32 count;
    char *name;
    char *p;
    MovieBuffer *buffer;
    u32 *ot;

    func_80074B58();
    top = 0;
    func_800284B4(&directory, &offset);
    func_80028470(0, 0);
    hex = 0;
    names = 0;
    do {
        count = 0x1249;
        if (D_80077120 == &D_80077124[0]) {
            buffer = &D_80077124[1];
        } else {
            buffer = &D_80077124[0];
        }
        ot = buffer->ot;
        D_80077120 = buffer;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        func_8003700C("\n[ FAT CHECK MODE ");
        func_8003700C(hex ? "HEX ]\n" : "DEC ]\n");
        func_800747AC(0, 0, &button);
        if (!(D_800773B4 & 0x1000) && (D_800773AC & 0x1000) && top > 0) {
            top--;
        }
        if (!(D_800773B4 & 0x10) && (D_800773AC & 0x10)) {
            top -= 20;
            if (top < 0) {
                top = 0;
            }
        }
        if (!(D_800773B4 & 0x4000) && (D_800773AC & 0x4000) && top < count - 20) {
            top++;
        }
        if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40)) {
            top += 20;
            if (top > count - 20) {
                top = count - 20;
            }
        }
        if (!(D_800773B4 & 8) && (D_800773AC & 8)) {
            hex = 1 - hex;
        }
        if (!(D_800773B4 & 4) && (D_800773AC & 4)) {
            names = 1 - names;
        }
        index = top;
        do {
            if (func_80075D4C(index) == 0) {
                if (hex) {
                    func_8003700C("No %4x NullFile\n", index);
                    index++;
                } else {
                    func_8003700C("No %4d NullFile\n", index);
                    index++;
                }
            } else {
                if (hex) {
                    func_8003700C("No %4x Sect%6x ", index, func_800289D0(index + 1));
                } else {
                    func_8003700C("No %4d Sect%6d ", index, func_800289D0(index + 1));
                }
                if (names) {
                    if ((s32)func_80075D4C(index) < 0) {
                        func_8003700C("[P%3d]\n", -func_80075D4C(index));
                        index++;
                    } else {
                        name = func_80028998(index + 1);
                        if (name != NULL) {
                            if (*name != 0) {
                                p = name;
                                do {
                                    if (*p == '\\') {
                                        name = p + 1;
                                    }
                                    p++;
                                } while (*p != 0);
                            }
                            func_8003700C("%s\n", name);
                        } else {
                            func_8003700C(D_8007042C);
                        }
                        index++;
                    }
                } else if (hex) {
                    func_8003700C("Size%9x\n", func_80075D4C(index));
                    index++;
                } else {
                    func_8003700C("Size%9d\n", func_80075D4C(index));
                    index++;
                }
            }
        } while (index < top + 20);
        func_8003700C(D_80070430);
        if (D_80077394 > 0) {
            func_8003278C(1, 0, 6, 0x808D);
        }
        func_80037324(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 8, 20, 304, 192);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 7, 19, 306, 194);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
    } while (button != 2);
    func_80028470(directory, offset);
}
#else
INCLUDE_ASM(".local/decomp/movie/asm/nonmatchings/movie", func_80075D8C);
#endif

/* The menu's movie test: play the selected movie with the display blanked
 * until it starts, then clear the screen and restore the menu's buffers.
 * The unused array reproduces the original's frame. */
void func_8007625C(void) {
    u8 unused[0x18];
    RECT screen;

    screen = D_800704E0;
    D_80077028 = 0;
    D_800773AC = -1;
    if (D_80077448 == 0) {
        func_80028470(0x18, 0);
        if (D_8007711C >= func_80028928(2)) {
            return;
        }
        SetDispMask(0);
    } else if (D_80077448 == 1) {
        func_80028470(0x18, 1);
        if (D_8007711C >= func_80028928(1)) {
            return;
        }
        SetDispMask(0);
    } else if (D_80077448 == 2) {
        return;
    }
    D_80077124[0].draw.isbg = 0;
    D_80077124[1].draw.isbg = 0;
    if (D_80077454 != 0) {
        D_80077124[0].disp.isrgb24 = 1;
        D_80077124[1].disp.isrgb24 = 1;
    }
    func_80076488();
    VSync(0);
    ClearImage(&screen, 0, 0, 0);
    DrawSync(0);
    VSync(0);
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.isbg = 1;
    if (D_80077454 != 0) {
        D_80077124[0].disp.isrgb24 = 0;
        D_80077124[1].disp.isrgb24 = 0;
    }
}

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

/* Aim the camera from the eye at the target, rolled, and compose the world
 * to screen matrix. */
void func_80076CA4(void) {
    s32 dz;
    s32 dx;
    s32 dy;
    s32 dz2;
    s32 dx2;

    dx = D_8007703C.vx - D_8007702C.vx;
    dz = D_8007703C.vz - D_8007702C.vz;
    dz2 = dz * dz;
    dx2 = dx * dx;
    dy = D_8007703C.vy - D_8007702C.vy;
    D_800770B0.vx = ratan2(dy, SquareRoot0(dz2 + dx2));
    D_800770B0.vy = -ratan2(dx, dz);
    func_8003F738(&D_800770B0, &D_800770D8);
    RotMatrixZ(D_8007704C, &D_800770D8);
    MulMatrix2(&D_800770F8, &D_800770D8);
    D_800770D8.t[0] = 0;
    D_800770D8.t[1] = 0;
    D_800770D8.t[2] = SquareRoot0(dx2 + dy * dy + dz2);
    D_800770B8.t[0] = D_80076F2C.vx - D_8007703C.vx;
    D_800770B8.t[1] = D_80076F2C.vy - D_8007703C.vy;
    D_800770B8.t[2] = D_80076F2C.vz - D_8007703C.vz;
    CompMatrix(&D_800770D8, &D_800770B8, &D_80077050);
}

INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_800704E0);
