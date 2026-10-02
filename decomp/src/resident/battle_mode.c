/* Resident battle-mode entry. This unit owns the entry flag; setup flags
 * belong to the following menu-support unit and are addressed absolutely. */
#include "common.h"
#include "cd.h"
#include "console.h"
#include "mode.h"

u8 D_8005959C;
extern u8 D_8005947C;
extern u8 D_800594F8;
extern u8 D_800C48EA;
extern u8 D_800D3338;
extern u16 D_8006F94E;
extern u16 D_8006F950;
extern u16 D_8006F952;
extern u16 D_8006F954;
void func_8001B844(void);
void func_80070F40(void);
void func_8001AC94(void);

/* Run the battle and choose the next mode from its outcome and pending
 * scene state. Outcome 0x81 installs scene 0x1EA before mode 1. */
void func_8001B6C4(void) {
    u8 outcome;
    s32 mode;

    D_8005959C = 1;
    func_80028A60(0);
    func_80028470(12, 0);
    if (*D_8005917C != -1) {
        func_8003747C(0x80200000);
        func_800374E8(0x10, 0x10, 0x140, 0x100, 0x3E8, 0,
                     0x340, 0, 0x340, 0x20, NULL);
    }
    func_8001B844();
    func_80070F40();
    outcome = D_800C48EA;
    if (outcome == 1 || outcome == 0x40 || outcome == 0x21) {
        if (D_800D3338 != 0) {
            mode = 6;
        } else if (D_8005947C == 0) {
            if ((D_8006F94E & 0x7FF) < 0x400) {
                mode = 1;
            } else {
                mode = 3;
            }
        } else {
            mode = 2;
        }
        func_8001996C(mode);
    } else if (outcome == 0x81) {
        func_8001AC94();
        D_8006F94E = 0x1EA;
        D_8006F950 = 0;
        D_8006F952 = 0;
        D_8006F954 = 0;
        func_8001996C(1);
    }
    if (D_8005947C == 0) {
        D_800594F8 = 1;
    }
    func_80019ACC(0);
}
