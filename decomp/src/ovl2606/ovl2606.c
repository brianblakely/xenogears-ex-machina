/* Debug battle-scene selector (slot 2606, loaded at 801e0000; the whole
 * image is this one unit: rodata 801E0000-801E0124, text 801E0124-801E0D40,
 * data 801E0D40-801E1DDC). The battle entry 80070f40 (battle overlay, at
 * 80071050) loads this module (directory 16 file 2) when the mode-2 byte
 * 800594f8 is nonzero and calls 801e0a34 before building the battle. A
 * debug-font screen ("SceneNo", "Party", "Robo" Off/Nml/Bar, "FileNo" or
 * Event1-3) is edited with the pad until Start; the choice becomes the enemy
 * set, the party and their gear, and the field file whose formation table
 * and event data are loaded. Resident calls: libgpu environments/ordering
 * tables, the debug text print 8003700c, disc file size/read/wait, the heap.
 * Its load address is fixed by the absolute jump table at 801e00e8, whose 15
 * targets are case labels of 801e0238. */
#include "scene_select.h"

/* The row labels and character names, each in a 256-byte text buffer, and
 * the tables of pointers to them that the screen prints. */
char D_801E0D40[4][0x100] = {"SceneNo ", "Party   ", "Robo    ", "FileNo  "};
char D_801E1140[12][0x100] = {
    "Fei", "Elly", "Shitan", "Baltho", "Billy", "Lico",
    "Emerada", "Chuchu", "Maria", "Shitan2", "Emerada2", "",
};
char *D_801E1D40[4] = {D_801E0D40[0], D_801E0D40[1], D_801E0D40[2], D_801E0D40[3]};
char *D_801E1D50[12] = {
    D_801E1140[0], D_801E1140[1], D_801E1140[2], D_801E1140[3],
    D_801E1140[4], D_801E1140[5], D_801E1140[6], D_801E1140[7],
    D_801E1140[8], D_801E1140[9], D_801E1140[10], D_801E1140[11],
};

/* Gear ids for the gear columns Nml and Bar, indexed by character (ff
 * none). */
u8 D_801E1D80[16] = {0, 2, 3, 4, 5, 6, 9, 7, 8, 16, 15, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
u8 D_801E1D90[16] = {1, 2, 11, 12, 13, 14, 9, 7, 8, 11, 18, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* The selector's values, set by func_801E0124: row 0 the scene (enemy set,
 * 0-15), row 1 the three party characters (0-11, ff none), row 2 their gear
 * mode (Off, Nml, Bar), row 3 the file number (253-255 the three event
 * files); and the columns shown on each row. */
s32 D_801E1DA0[4][3] = {0};
u8 D_801E1DD0[4][3] = {0};

/* Put the first frame's environments, enable the display and reset the
 * selector: party order 0/1/2, default values and every digit visible. */
void func_801E0124(void) {
    s32 i;
    s32 j;

    PutDrawEnv(&D_800C3EB0.buffers[0].drawEnv);
    PutDrawEnv(&D_800C3EB0.buffers[1].drawEnv);
    PutDispEnv(&D_800C3EB0.buffers[0].dispEnv);
    PutDispEnv(&D_800C3EB0.buffers[1].dispEnv);
    SetDispMask(1);
    D_8006D634.party[1] = 1;
    D_8006D634.party[2] = 2;
    D_801E1DA0[0][0] = 3;
    D_801E1DA0[1][1] = 1;
    D_8006D634.party[0] = 0;
    D_801E1DA0[1][0] = 0;
    D_801E1DA0[1][2] = 2;
    D_801E1DA0[2][2] = 0;
    D_801E1DA0[2][1] = 0;
    D_801E1DA0[2][0] = 0;
    D_801E1DA0[3][0] = 0;
    for (i = 0; i < 4; i++) {
        for (j = 2; j >= 0; j--) {
            D_801E1DD0[i][j] = 1;
        }
    }
    D_801E1DD0[3][2] = 0;
    D_801E1DD0[3][1] = 0;
    D_801E1DD0[0][2] = 0;
    D_801E1DD0[0][1] = 0;
}

/* The selector screen, one frame per pass until Start: the directions move
 * the cursor over the shown columns, Circle/Cross step the value (wrapping
 * per row), Triangle adds ten to the file number; then the rows are printed
 * with the resident debug text and the frame is shown. */
void func_801E0238(void) {
    u8 running;
    s32 row;
    s32 col;
    s32 i;
    s32 j;
    s32 value;
    char *name;
    FrameBuffer *frame;

    running = 1;
    row = 0;
    col = 0;
    do {
        func_80089CCC(0);
        frame = D_800C3EB0.buffers;
        if (D_800C3EB0.current == frame) {
            frame++;
        }
        D_800C3EB0.current = frame;
        ClearOTagR((u_long *)frame->ot, 0x1000);
        switch (D_800D3014) {
        case 14: /* Start */
            running = 0;
            break;
        case 1: /* down */
        case 13:
            if (++row >= 4) {
                row = 0;
            }
            if (!D_801E1DD0[row][col]) {
                col = 0;
            }
            break;
        case 3: /* up */
            if (--row < 0) {
                row = 3;
            }
            if (!D_801E1DD0[row][col]) {
                col = 0;
            }
            break;
        case 0: /* right */
            if (++col >= 3) {
                col = 0;
            }
            if (!D_801E1DD0[row][col]) {
                col = 0;
            }
            break;
        case 2: /* left */
            if (--col < 0) {
                col = 0;
            }
            if (!D_801E1DD0[row][col]) {
                col = 0;
            }
            break;
        case 5: /* Cross: decrement, wrapping to the row's top value */
            if (--D_801E1DA0[row][col] < 0) {
                switch (row) {
                case 0:
                    D_801E1DA0[row][col] = 15;
                    break;
                case 1:
                case 3:
                    D_801E1DA0[row][col] = 0xFF;
                    break;
                case 2:
                    D_801E1DA0[row][col] = 2;
                    break;
                }
            }
            break;
        case 4: /* Circle: increment, wrapping to zero */
            D_801E1DA0[row][col]++;
            switch (row) {
            case 0:
                if (D_801E1DA0[row][col] >= 16) {
                    D_801E1DA0[row][col] = 0;
                }
                break;
            case 1:
                if (D_801E1DA0[row][col] >= 12) {
                    D_801E1DA0[row][col] = 0;
                }
                break;
            case 2:
                if (D_801E1DA0[row][col] >= 3) {
                    D_801E1DA0[row][col] = 0;
                }
                break;
            case 3:
                if (D_801E1DA0[row][col] >= 0x100) {
                    D_801E1DA0[row][col] = 0;
                }
                break;
            }
            break;
        case 7: /* Triangle: file number + 10 */
            if (row == 3) {
                D_801E1DA0[row][col] += 10;
                if (D_801E1DA0[row][col] >= 0x100) {
                    D_801E1DA0[row][col] = 0;
                }
            }
            break;
        }
        func_8003700C("\n\n");
        for (i = 0; i < 4; i++) {
            func_8003700C("\n\n%s", D_801E1D40[i]);
            for (j = 0; j < 3; j++) {
                if (D_801E1DD0[i][j]) {
                    switch (i) {
                    case 0:
                        if (row == i && j == col) {
                            func_8003700C("[%d] ", D_801E1DA0[row][col]);
                        } else {
                            func_8003700C("%d ", D_801E1DA0[i][j]);
                        }
                        break;
                    case 2:
                        if (row == i && j == col) {
                            switch (D_801E1DA0[row][col]) {
                            case 0:
                                func_8003700C("[Off] ");
                                break;
                            case 1:
                                func_8003700C("[Nml] ");
                                break;
                            case 2:
                                func_8003700C("[Bar] ");
                                break;
                            }
                        } else {
                            switch (D_801E1DA0[i][j]) {
                            case 0:
                                func_8003700C("Off ");
                                break;
                            case 1:
                                func_8003700C("Nml ");
                                break;
                            case 2:
                                func_8003700C("Bar ");
                                break;
                            }
                        }
                        break;
                    case 3:
                        if (row == i && j == col) {
                            value = D_801E1DA0[row][col];
                            if (value < 253) {
                                func_8003700C("[%d] ", value);
                            } else {
                                switch (value - 253) {
                                case 0:
                                    func_8003700C("[Event3] ", value);
                                    break;
                                case 1:
                                    func_8003700C("[Event2] ", value);
                                    break;
                                case 2:
                                    func_8003700C("[Event1] ", value);
                                    break;
                                }
                            }
                        } else {
                            value = D_801E1DA0[i][j];
                            if (value < 253) {
                                func_8003700C("%d ", value);
                            } else {
                                switch (value - 253) {
                                case 0:
                                    func_8003700C("Event3 ", value);
                                    break;
                                case 1:
                                    func_8003700C("Event2 ", value);
                                    break;
                                case 2:
                                    func_8003700C("Event1 ", value);
                                    break;
                                }
                            }
                        }
                        break;
                    case 1:
                        if (row == i && j == col) {
                            if (D_801E1DA0[row][col] >= 12) {
                                func_8003700C("[%s] ", D_801E1D50[11]);
                            } else {
                                func_8003700C("[%s] ", D_801E1D50[D_801E1DA0[row][col]]);
                            }
                        } else {
                            name = D_801E1DA0[i][j] >= 12 ? D_801E1D50[11] : D_801E1D50[D_801E1DA0[i][j]];
                            func_8003700C("%s ", name);
                        }
                        break;
                    }
                }
            }
        }
        func_8003700C("\n\n     LU       Start  to Battle");
        func_8003700C("\n   LL  LR     Maru   +");
        func_8003700C("\n     LD       Batsu  -");
        func_80037324((u_long *)D_800C3EB0.current->ot);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_800C3EB0.current->drawEnv);
        PutDispEnv(&D_800C3EB0.current->dispEnv);
        DrawOTag((u_long *)&D_800C3EB0.current->ot[0xFFF]);
    } while (running);
}

/* Entry: run the selector, then set up the chosen battle: the party and
 * whether each member starts in a gear, the enemy set, the chosen file's
 * formation table and event data from disc, full HP/EP for every character
 * and every member joined. */
void func_801E0A34(void) {
    s32 i;
    s32 member;
    s32 size;
    void *data;

    func_801E0124();
    func_801E0238();
    member = 0;
    for (i = 0; i < 3; i++) {
        D_8006D634.inGear[i] = 0;
        D_8006D634.party[i] = 0xFF;
        if (D_801E1DA0[1][i] < 11) {
            D_8006D634.party[member] = D_801E1DA0[1][i];
            switch (D_801E1DA0[2][i]) {
            case 0:
                D_8006D634.inGear[member] = 0;
                break;
            case 2:
                D_8006D634.characters[D_8006D634.party[member]].gearId = D_801E1D90[D_8006D634.party[member]];
                D_8006D634.inGear[member] = 1;
                break;
            case 1:
                D_8006D634.characters[D_8006D634.party[member]].gearId = D_801E1D80[D_8006D634.party[member]];
                D_8006D634.inGear[member] = 1;
                break;
            }
            member++;
        }
    }
    func_80028470(0x20, 3);
    if (D_801E1DA0[3][0] < 0xFD) {
        i = D_801E1DA0[3][0] + 7;
    } else {
        i = D_801E1DA0[3][0] - 0xF9;
    }
    D_80059508 = D_801E1DA0[0][0];
    if (D_80059508 == 0xF && i == 7) {
        D_80059508 = 3;
        D_8005947C = 3;
    }
    size = func_800288EC(i);
    func_80032498(2, 0);
    data = func_8008ABB8(size, 1);
    func_800295D8(i, data, 0, 0x80);
    func_80028A60(0);
    memmove(&D_800658DC, data, sizeof(EncounterSet));
    func_800320E8(data);
    func_8001B66C();
    func_8008AB70();
    size = func_800288EC(4);
    D_800D39D8 = func_8008ABB8(size, 1);
    func_800295D8(4, D_800D39D8, 0, 0x80);
    func_80028A60(0);
    memmove(D_80062648, D_800D39D8, size);
    func_800320E8(D_800D39D8);
    func_8009B1E4();
    for (i = 0; i < 11; i++) {
        D_8006D634.characters[i].hp = 999;
        D_8006D634.characters[i].maxHp = 999;
        D_8006D634.characters[i].ep = 99;
        D_8006D634.characters[i].maxEp = 99;
    }
    D_8006D634.joined = 0xFFFF;
    func_8003748C();
    D_8005954C = D_80059508 & 3;
}
