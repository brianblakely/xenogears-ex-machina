#include "common.h"
#include "battle_core.h"

/* Start the 801e5000 module: reserve its heap span and load it. */
void func_80070E2C(void) {
    s32 block;

    if (D_800C3D48 != 0) {
        func_8008AB4C();
        block = func_8008ABB8(4, 1);
        D_800D3284 = block;
        D_800D328C = func_8008ABB8(block - 0x801E5000, 1);
        func_800295D8(1, 0x801E5000, 0, 0x80);
        func_8008AC50();
        func_801E5160();
    }
}

/* Forward a byte argument to the 801e5000 module when it is loaded. */
void func_80070EB0(s32 value) {
    if (D_800C3D48 != 0) {
        func_801E879C(value & 0xFF);
    }
}

/* Fade the battle music out once the escape outcome is set, unless the
 * 801e5000 module handled it. */
void func_80070EDC(void) {
    s32 handled = 0;

    if (D_800C3D48 != 0) {
        handled = func_801E563C();
    }
    if ((handled & 0xFF) == 0 && D_800C48EA == 0x81) {
        func_8003A89C(D_800C3E54, 0, 0xF0);
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80070F40);

/* One battle frame: the 80280000 module's hook when present, then the task
 * runner. */
#ifdef NON_MATCHING
s32 func_800716D8(void) {
    if (*D_8005917C != -1) {
        func_8028022C();
    }
    func_800BE790();
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800716D8);
#endif

/* One ATB tick for every present slot that is not yet ready. */
void func_8007171C(void) {
    s32 slot;
    s32 step;
    u8 *ready;
    u16 flags;
    u8 delay;
    u16 *toggle;
    s16 *timer;

    if (D_800D3298 != 0) {
        slot = 0;
        ready = D_800D2DE4;
        for (; slot < 11; ready++, slot++) {
            if (D_800D2DCC[slot] == 0 || *ready != 0) {
                continue;
            }
            step = 1;
            if ((D_800CCCE8[slot].status84 | D_800CCCE8[slot].status86) & 0x8000) {
                step = 2;
            }
            if (D_800CCCE8[slot].flags7C & 0x1000) {
                toggle = &D_800D2E1C[slot];
                if ((*toggle ^= 1) != 0) {
                    continue;
                }
            }
            flags = D_800CCCE8[slot].flags7C;
            if (flags & 0x2000) {
                delay = D_800CCCE8[slot].delay15C -= step;
                if (delay == 0) {
                    D_800CCCE8[slot].delay15C = 0;
                    D_800CCCE8[slot].flags7C &= 0xDFFF;
                }
                continue;
            }
            if ((flags & 0x80) || (D_800CCCE8[slot].flags80 & 0x1000)) {
                continue;
            }
            timer = &D_800D2DF0[1][slot];
            if ((*timer -= step) <= 0) {
                *ready = 1;
                *timer = 0;
            }
        }
    }
}

/* Reload the acting slot's turn timer and clear its ready flag. */
#ifdef NON_MATCHING
void func_800718BC(void) {
    u8 actor = D_800C3EAC->actor;

    if (D_800D2DE4[actor] != 0xFF) {
        D_800D2DE4[actor] = 0;
    }
    D_800D2DF0[1][D_800C3EAC->actor] = func_80098AF8(D_800C3EAC->actor, 0);
    D_800D2DF0[0][D_800C3EAC->actor] = D_800D2DF0[1][D_800C3EAC->actor];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800718BC);
#endif

/* Render the pending battle message into its image, upload it and hold it
 * for three frames. */
void func_80071964(void) {
    s32 frames;

    if (D_800D2CAF != 0 && (D_800D2C94 & D_800C48E8) == 0) {
        frames = 3;
        D_800D39B8.width = func_80034EAC(func_80033728(D_800D39F0, D_800D2CAF),
                                         D_800D39B8.pixels, 0x39, 1);
        func_80044894(&D_800D39B8.rect, D_800D39B8.pixels);
        do {
            frames--;
            func_800716D8();
        } while (frames != 0);
    }
}

/* Clear the party's reaction flags. */
void func_80071A08(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800C3EAC->reaction[i] = 0;
    }
}

/* Publish the party's reaction flags to the UI, then wait a frame. */
void func_80071A38(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800D2D28->reaction[i] = D_800C3EAC->reaction[i];
    }
    func_800716D8();
}

/* Reset the eight 0x60-byte entries at 800d3720 and two UI bytes, then wait a
 * frame. */
#ifdef NON_MATCHING
void func_80071A8C(void) {
    s32 i;

    for (i = 7; i >= 0; i--) {
        D_800D3720[i].unk5 = 0;
    }
    D_800D2D28->unkB5 = 0;
    D_800D2D28->unkB4 = 0;
    func_800716D8();
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80071A8C);
#endif

/* Show the pending battle message (window 7) until a button is pressed or
 * 59 frames pass. */
void func_80071AE0(void) {
    s32 frames;

    if (D_800D2CAF != 0 && (D_800D2C94 & D_800C48E8) == 0) {
        func_80079E18(7);
        frames = 0x3B;
        D_800C3EAC->eventsDone = 0;
        do {
            func_800716D8();
        } while (D_800D3014 == 8 && --frames != 0);
        D_800C3EAC->eventsDone = 1;
        func_80079E4C(7);
        func_800716D8();
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80071B94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072270);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072324);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800723E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007252C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800728B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072938);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072A9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072DA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80072F38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073380);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073538);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073A58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073B64);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073E88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073F08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80073FB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800742A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800743A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800744BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074554);

INCLUDE_RODATA(".local/decomp/battle/asm/nonmatchings/battle", D_8006FAF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800745EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074AB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074D4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074EEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80074F70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007500C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80075168);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80075938);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076418);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800764B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800764EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076544);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800765C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076710);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800769E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076A10);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076A6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076AC8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076B00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076B68);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076BAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076BF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076C34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076CE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076D58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80076EA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077074);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077364);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077610);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007765C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077698);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077980);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80077990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800780A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007819C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078310);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078508);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800785D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078658);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800787E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007887C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007893C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078998);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078B34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078C9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078CEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078D48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078D6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80078E24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079054);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079114);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007916C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800791FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079270);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800792F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800793F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079674);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079778);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079840);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079934);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079948);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800799C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079AB0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079C24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079E18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079E4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079E7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80079ED8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A280);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A628);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A6C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A744);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A7BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A828);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A874);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A8B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A900);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A92C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A968);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A9A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007A9D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AA1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AA60);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AAB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AAF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AB30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AB68);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ABA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ABD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AC30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AC80);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ACDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AD24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AD6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ADB0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ADF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AE38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AE98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AEF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AF5C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AFAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007AFFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B040);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B084);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B0C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B134);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B198);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B208);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B264);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B2C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B310);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B360);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B3B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B3E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B424);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B4B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B578);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B608);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B6C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B7B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B8D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B914);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B958);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B98C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007B9C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BA04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BA44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BA88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BAB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BAE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BB2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BB70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BBD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BC40);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BC84);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BCE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BD5C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007BEA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C040);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C1A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C33C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C4A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C580);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C678);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C75C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C840);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007C9D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007CB20);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007CC50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007CD10);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007CDD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007CEA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007CFB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D0CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D148);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D1A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D1DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D30C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D344);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D478);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D5B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D610);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D6A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D7B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007D8C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007DA1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007DB78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007DCF8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007DE78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007DFD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E154);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E1D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E234);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E334);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E438);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E554);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E674);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E6A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E6F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E740);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E780);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E7C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E7E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E8AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E8E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E934);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E954);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E98C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007E9D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EA08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EA4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EA84);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EAC8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EB08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EB50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EB90);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EBD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EC10);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EC54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EC94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ECDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ED14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ED58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007ED98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EDE0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EE28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EE70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EEA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EED0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EEE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EF44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007EF6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007F8C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FB70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FBE0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FCE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FD38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FDEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FE3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FEC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8007FF14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800800E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080160);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080AE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080B64);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080BD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080C6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80080C94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008115C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80081318);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80081504);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800816F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008189C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800819A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80081B58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800820A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800822C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082504);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800826CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082820);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800829F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082BB0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082D4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80082F7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800830A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083340);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083580);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083748);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083948);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80083FF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800841E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084548);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084750);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084A7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084B40);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084D28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084DE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085084);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085310);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085350);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085388);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085618);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085AC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085B58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085C48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085C88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085CCC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085D34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085E78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085EB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086028);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800861D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086B88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086C88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086F98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800877E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800879A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80087A38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80087AF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80087EDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800881B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800883AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088490);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800885D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008860C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008887C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088B80);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089038);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089110);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800891E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089348);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008946C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008963C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800897CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800898F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089AF8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089B50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089BEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089C08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089C24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089C48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089C6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089C9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089CCC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A144);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A274);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A3EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A684);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A9C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AA40);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AA74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AAA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AB4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AB70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AB94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008ABB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AC00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AC50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008AC88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008ADD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B168);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B224);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B478);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B908);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BC40);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BC98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BD50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BED8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C360);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C3F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C4A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C81C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008CCCC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008CD28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008CDE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008CFB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008D328);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008D598);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008DC34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008DE04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008E430);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008EA70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008F0A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008F6E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008F8F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FA60);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FAD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FC1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FDE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FE18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009023C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090310);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800904A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009070C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009080C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009093C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090B90);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090C44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80090E7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091064);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091604);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800916D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009187C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091B38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091D38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80091EC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009209C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80092298);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80092784);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80092B74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800930AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80093578);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009382C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800939CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80093B08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009413C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800941A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800946F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80094C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80094D24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80094EE4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095690);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800957D8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800958D8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095A78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095B44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095BAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80095D4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096018);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096494);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096824);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800968C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096AB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096FBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097610);

void func_8009795C(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097964);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097D08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097D5C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009892C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098AF8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098C6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098D2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80099498);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800995A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80099890);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80099CF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80099FB0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A074);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A0DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A1AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A258);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A2D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A7B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A7E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009A9D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AA44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AB00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AB38);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AC48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009ADA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AEFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009AFD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B104);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B1E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B46C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B684);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009BAC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009BD94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009BE0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C050);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C0E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C134);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C198);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C4B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009C9C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009CA90);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009CB68);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009CBC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009D354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009D3A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009D948);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009DA04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009DB54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009DBFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E268);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E278);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E2EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E364);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E3C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E410);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E48C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E508);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E53C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E5C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E788);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009E868);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009EBA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009EC4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009EF3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F1C4);

void func_8009F5B0(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F5B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F708);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F794);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009F844);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A0838);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A1B50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A1CF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A216C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2234);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A22A8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A22E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A23E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2434);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2704);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2ACC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2BB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2CA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2D1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2D5C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2E88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2F94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A2FD8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A32D8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3484);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3490);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3514);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3578);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A35C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3640);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A3E98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A429C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4348);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A43F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A44C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4654);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4820);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A48EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4B3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4CF8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A4DB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A577C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A578C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A579C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5870);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5914);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5A48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5BE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5D54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5E9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A5EB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6444);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A64E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6884);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6AE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A6F98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A7064);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A7948);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8A88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8B0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A8BF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9540);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A96B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A979C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9A50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9F94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800A9FF0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA320);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA384);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA514);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA564);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA600);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA650);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA6E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA760);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA788);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA79C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA7DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA820);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA898);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AA934);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AAA20);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AAB34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AAD54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800ADF1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE1BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE220);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AE2A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AEEEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AEEF8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AEF68);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF180);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF270);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF2C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF400);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF438);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF518);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AF678);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFA98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFB4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFC68);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFD98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800AFF9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0060);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B00D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B00F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0164);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B026C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0AB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0B14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0D70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B0FF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B10EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B12D0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B136C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B14B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B14CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B15D8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B168C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B16A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B16F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B1720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B1EA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B1F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B1F6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B2AEC);

void func_800B3348(void) {
}

void func_800B3350(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3358);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B35C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3658);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B36BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B383C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3878);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B397C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B39C0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3B6C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3B94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3C2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3C74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3CD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3E04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B3F04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B4EDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B4F88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B50D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B51B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5588);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B56E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B572C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B57E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5854);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5924);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B59BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5AC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5B3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5C18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5CC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5DC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5DF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B5FBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6004);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B61B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B61F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B626C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B62C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B639C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B63F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6438);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6464);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B64D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6518);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B65B0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6808);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6930);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6990);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B69E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6A50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6A7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6B98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6BFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6C44);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6C98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6CEC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6DC0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6E84);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B6F0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7134);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7160);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7364);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B73A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B73EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7424);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7870);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7C28);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7C34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B7E94);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8048);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8054);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8068);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B81BC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B838C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B853C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8774);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8840);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B88C4);

void func_800B89F4(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B89FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8D04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8D7C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8DA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B8EBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9020);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B905C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9258);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9284);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9508);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9B30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9B54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9C00);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9C78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800B9F78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA4E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA59C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA614);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA768);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA8F4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BA984);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BAB0C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BABDC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BAC50);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BACBC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BADD4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BAEB8);

void func_800BAF40(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BAF48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB080);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB13C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB248);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB350);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB540);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB620);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB690);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB6E0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB760);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB7F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB844);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BB9D4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BBAB8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BBEE0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC018);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC158);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC2F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC3F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC404);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC454);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BC460);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCAA4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCAD0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCAFC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCB54);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCBB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCC60);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCD8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCD98);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCEAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BCFAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD024);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD098);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD1FC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD2E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD3AC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD7A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD810);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BD974);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDA1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDB08);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDB74);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDC14);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDC78);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDCF8);

void func_800BDD34(void) {
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDD3C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDE58);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BDF1C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE0DC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE108);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE11C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE1C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE330);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE538);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE6A0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE6E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BE790);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEB04);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEBC4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEC18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BED30);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BED4C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEDE8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEE2C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEEB4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEF24);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEF8C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BEFF4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF0B4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF0C4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF1EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF2B8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF354);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF3A4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF3E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF4F0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF5E8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF600);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF6CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF6F8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF720);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF730);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF73C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF7C8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF85C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF8CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF954);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF998);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BF9EC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFA9C);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFBA0);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFC80);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFD88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFDA8);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800BFE48);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0314);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0564);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C06E4);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0758);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C07CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0828);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C08CC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0D18);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0F70);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C0FAC);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C1140);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800C11CC);
