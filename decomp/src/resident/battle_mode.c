/* Resident battle-mode entry (8001b6c4-8001b844, mode 2): runs the battle
 * overlay and chooses the next mode from its outcome. This unit owns the
 * entry flag D_8005959C, a small common it addresses through $gp, while the
 * setup flags belong to the following menu-support unit and are addressed
 * absolutely; that ownership makes it a unit of its own, whose end may lie
 * later than 8001b844 (no own $gp reference or rodata follows; see the
 * target yaml). GCC 2.6.3 and 2.7.2 build the same object. */
#include "common.h"
#include "battle/area.h"
#include "battle/setup.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/mode.h"

u8 D_8005959C;

/* Run the battle and choose the next mode from its outcome and the next
 * battle's formation D_8005947C: another battle (mode 2) while it is set.
 * Outcome 0x81 installs scene 0x1EA before mode 1. */
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
    outcome = D_800C3EB0.outcome;
    if (outcome == 1 || outcome == 0x40 || outcome == 0x21) {
        if (D_800D3338 != 0) {
            mode = 6;
        } else if (D_8005947C == 0) {
            if ((D_8006D634.map & 0x7FF) < 0x400) {
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
        D_8006D634.map = 0x1EA;
        D_8006D634.entry[0] = 0;
        D_8006D634.entry[1] = 0;
        D_8006D634.entry[2] = 0;
        func_8001996C(1);
    }
    if (D_8005947C == 0) {
        D_800594F8 = 1;
    }
    func_80019ACC(0);
}
