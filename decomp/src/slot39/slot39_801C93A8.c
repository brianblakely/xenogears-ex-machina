/* The menu overlay's card refresh, a translation unit of its own between the
 * card listing helpers (slot39.c) and the file screens (slot39_801C9BCC.c).
 * It was built by GCC 2.6.0 with -fno-rerun-cse-after-loop (slot39.mk): no
 * other qualified compiler or flag set reproduces it, and the neighbouring
 * units do not compile under that setting. The statement macro's loop note
 * ends the first cse pass's block, so with the second pass off `marked` stays
 * a register copy of noCard (`move s6,s4`); it is still 0 when the cursor
 * marker code tests it, so that store is never reached. */
#include "menu.h"

/* Statement macro around the result's initial assignment. */
#define SET_RESULT(var, value) \
    do {                       \
        (var) = (value);       \
    } while (0)

/* Refresh the cards before a save or load. With no card in either port: show
 * message 23 and, still none, wait a second and return 1; otherwise reset
 * the card events and listings. With cards: relist each port whose file
 * count changed (erase the temporary file, read each file's header and icon)
 * while the cards stay as they were, mark this game's files and place the
 * cursor marker. */
u8 func_801C93A8(void) {
    char path[64];
    u8 present[2];
    u8 unused[48]; /* unused in the original; reserves 48 bytes */
    MenuState *state;
    s32 port;
    s32 i;
    s32 wait;
    u8 noCard;
    u8 marked;
    u8 stop;

    D_801EA6F8 = 0;
    D_800625A0->card->mode = 2;
    func_801C7BF4();
    SET_RESULT(noCard, 0);
    marked = noCard;
    if (*(u16 *)D_800625A0->card->present == 0) {
        func_801D32B4();
        D_800625A0->party->messageShown = 0;
        D_800625A0->party->unkB = 0;
        func_801C7BF4();
        func_801D2F4C(0x23);
        D_800625A0->loadState = 1;
        D_800625A0->party->unk2F = 0;
        D_800625A0->card->unk4F80 = 0xff;
        D_800625A0->card->cursor = 0;
        if (*(u16 *)D_800625A0->card->present == 0) {
            D_800625A0->card->unk4F8C[0] = D_800625A0->card->unk4F8C[1] = 0xff;
            func_801C7BF4();
            wait = 59;
            do {
                VSync(0);
            } while (--wait != 0);
            noCard = 1;
        }
        if (!noCard) {
            func_801D9B08();
            D_800625A0->card->presentShown[0] = D_800625A0->card->presentShown[1] = 0xff;
            D_800625A0->card->scanned[0] = 0;
            D_800625A0->card->scanned[1] = 0;
            D_800625A0->cardPollTimer = 0x3c;
            func_801C7BF4();
            D_801EA6F8 = 1;
        }
        func_801D32B4();
    }
    D_801E9779 = 1;
    present[0] = D_800625A0->card->present[0];
    present[1] = D_800625A0->card->present[1];
    if (!noCard) {
        D_800625A0->sounds = 0;
        for (port = 0; port < 2; port++) {
            stop = 0;
            if (D_800625A0->card->unk4F8A[port] != D_800625A0->card->unk4F8C[port] &&
                D_800625A0->card->unk4F8A[port] != 0) {
                func_801D9B08();
                for (i = 0; i < 16; i++) {
                    D_800625A0->card->fileSlots[port * 16 + i] = 0xff;
                }
                if (port == 0) {
                    __builtin_memcpy(path, D_801C50A8, 6);
                } else {
                    __builtin_memcpy(path, D_801C50B0, 6);
                }
                strcat(path, D_801C50B8);
                func_800405B4(path);
                D_800625A0->card->fileCount = 0;
                D_801EA900[port] = 0;
                for (i = 0; i < D_800625A0->card->unk4F8A[port]; i++) {
                    if (D_800625A0->card->present[port] != 0 && present[0] == D_800625A0->card->present[0] &&
                        present[1] == D_800625A0->card->present[1]) {
                        func_801C90B0(port, i);
                        func_801E78C8(port * 16 + i);
                        D_800625A0->card->files[port * 16 + i].state = 1;
                        func_801C7BF4();
                        if (D_800625A0->card->scanned[port] != 0) {
                            continue;
                        }
                        stop = port + 1;
                    } else {
                        stop = 2;
                    }
                    port = 2;
                    noCard = 0;
                    break;
                }
                if (!stop) {
                    for (; i < 16; i++) {
                        D_800625A0->card->files[port * 16 + i].state = 0;
                    }
                    D_800625A0->card->unk4F8C[port] = D_800625A0->card->unk4F8A[port];
                }
            }
            if (!stop && D_800625A0->card->unk4F8A[port] == 0) {
                for (i = 0; i < 16; i++) {
                    D_800625A0->card->files[port * 16 + i].state = 0;
                }
                D_800625A0->card->unk4F8C[port] = 0xff;
            }
        }
        if (!stop) {
            func_801C9270(0);
            func_801C9270(1);
        }
    }
    if (!stop) {
        if (marked) {
            D_800625A0->party->unkB = 1;
        }
        state = D_800625A0;
        if (state->party->unk2F != 0 && state->markers->unk144[0] != 0) {
            setXY4(&state->markers->polys[state->markers->current[0]],
                   D_801E9894[D_801E981C[state->card->cursor]][0] + 8, D_801E9914[D_801E981C[state->card->cursor]][0] - 6,
                   D_801E9894[D_801E981C[state->card->cursor]][0] + 0x18, D_801E9914[D_801E981C[state->card->cursor]][0] - 6,
                   D_801E9894[D_801E981C[state->card->cursor]][0] + 8, D_801E9914[D_801E981C[state->card->cursor]][0] + 0xa,
                   D_801E9894[D_801E981C[state->card->cursor]][0] + 0x18, D_801E9914[D_801E981C[state->card->cursor]][0] + 0xa);
        }
    }
    D_800625A0->sounds = 1;
    D_801E9779 = 0x1e;
    return noCard;
}
