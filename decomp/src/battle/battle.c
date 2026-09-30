/* Battle code from 8008115C to 8008B478 (the earlier units have files of
 * their own). Its rodata starts at 80070010, where the jump tables return to
 * 0 mod 8 right after 80080160's (docs/matching.md); the text boundary lies
 * after 80080160. The next unit's tables start at 80070314 at 4 mod 8. */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "menu_pages.h"
#include "resolver.h"
#include "action_resolve.h"
#include "hud_draw.h"
#include "battle_command.h"
#include "window_draw.h"
#include "formation_route.h"
#include "gear_menu.h"
#include "glyph_lists.h"
#include "item_command.h"
#include "result_input.h"

/* Confirm the selected entry of the member's on-foot window: entries 4, 6
 * and 7 open the attack page (5) when the member has a target; 1 and 0 open
 * pages 2 and 7 unless their item is unavailable (buzzer 0x4f); 2 opens page
 * 3; 3 opens page 4 when available, else on a second press of the repeat
 * entry (800c3e29 = 3) page 0xa. */
void func_8008115C(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        if (D_800C3EAC->slots[member].defaultTarget != 0xFF) {
            D_800D366C = 0;
            func_80087A38(member);
            func_80084A7C(member);
            func_80077698();
            D_800C3EAC->page = 5;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[11] == 0) {
            D_800C3EAC->page = 2;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        D_800C3EAC->page = 3;
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[10] == 0) {
            D_800C3EAC->page = 4;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[7] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0xA;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[5] == 0) {
            D_800C3EAC->page = 7;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's item command window: entries
 * 4, 6 and 7 open the item list when the member has one (else page 2); 0
 * opens page 1 when available, else on a repeat press (800c3e29 = 0) page 7;
 * 2 opens page 3; 3 opens page 4, else on a repeat press page 0xa; 1 opens
 * page 8 unless unavailable (buzzer 0x4f). */
void func_80081318(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        if (func_8008BED8(member)) {
            func_8008B908(member);
        } else {
            D_800C3EAC->page = 2;
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 1;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[5] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 7;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        D_800C3EAC->page = 3;
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[10] == 0) {
            D_800C3EAC->page = 4;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[7] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0xA;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[11] == 0) {
            D_800C3EAC->page = 8;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's charge command window:
 * entries 4, 6 and 7 charge and end the menu; 0 opens page 1 when available,
 * else on a repeat press (800c3e29 = 0) page 7; 1 and 2 open pages 2 and 9
 * unless unavailable (buzzer 0x4f); 3 opens page 4, else on a repeat press
 * page 0xa. */
void func_80081504(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        func_8009AA44(member);
        D_800C3EAC->unk2EA = 0;
        D_800C3EAC->menuDone = 1;
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 1;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[5] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 7;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[11] == 0) {
            D_800C3EAC->page = 2;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[10] == 0) {
            D_800C3EAC->page = 4;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[7] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0xA;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        if (D_800C3EAC->slots[member].items[8] == 0) {
            D_800C3EAC->page = 9;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's gear-list command window:
 * entries 4, 6 and 7 open the gear list when the member has one (else page
 * 4); 0 opens page 1 when available, else on a repeat press (800c3e29 = 0)
 * page 7; 1 and 3 open pages 2 and 0xa unless unavailable (buzzer 0x4f); 2
 * opens page 3. */
void func_800816F8(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        func_8007FD38(member);
        if (func_8008B478(member)) {
            func_8008ADD0(member);
        } else {
            D_800C3EAC->page = 4;
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 1;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[5] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 7;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[11] == 0) {
            D_800C3EAC->page = 2;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        D_800C3EAC->page = 3;
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[7] == 0) {
            D_800C3EAC->page = 0xA;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Frame the camera and target cursor on the attack page target, mark the
 * four directions that lead to another target and show its name. */
void func_8008189C(u8 member) {
    s32 direction;

    if (D_800C3EAC->unk2E9 == 0) {
        func_800BC404(func_80089C08(D_800C3EAC->unk2E8));
        func_800BCD98(func_80089C08(D_800C3EAC->unk2E8));
        for (direction = 0; direction < 4; direction++) {
            if (func_80084854(D_800C3EAC->unk2E8, direction) != D_800C3EAC->unk2E8) {
                D_800C3E24->arrows[direction] = 1;
            } else {
                D_800C3E24->arrows[direction] = 0;
            }
        }
        if (D_800D2C34 != 4) {
            func_80093B08(member);
        }
    }
}

/* Confirm the attack page's target (once): make it the member's default
 * target, close the page, highlight member and target and (except for
 * character 4) queue a move event toward it. */
void func_800819A4(member)
u8 member;
{
    if (D_800C3EAC->unk2E9 == 0) {
        D_800C3EAC->unk2E9 = 1;
        func_800BCD98(0);
        D_800C3EAC->slots[member].defaultTarget = D_800C3EAC->unk2E8;
        func_8007FCE8();
        func_8007FDEC();
        func_80077980();
        func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
        if (D_800D2D24[member] != 4) {
            D_800C3FE8[D_800C3EAC->eventCount].actor = member;
            D_800C3FE8[D_800C3EAC->eventCount].type = 0xFD;
            D_800C3FE8[D_800C3EAC->eventCount].parameter = 0;
            D_800C3FE8[D_800C3EAC->eventCount].targetMask = func_80089C08(D_800C3EAC->slots[member].defaultTarget);
            D_800C3EAC->eventCount++;
        }
        func_8009413C(member, 0);
    }
}

/* The attack page (5) of the member's turn. The turn state bytes 0x2d4/0x2d5
 * hold the AP left and the maximum, 0x2df the cost of the pressed attack,
 * 0x2e0-0x2e5 the execution, closed, shown page, attacked, combo armed and
 * reacted flags. Cancel (5) leaves the page while no AP were spent, else
 * closes the command; attacks whose item is blocked beep; directions retarget
 * until the combo starts. Attacks 6/7/4 cost 1/2/3 AP (4 also arms a known
 * combo): with enough AP the combo is recorded and executed and the timer
 * reload set from the AP left; the command closes when no AP remain or the
 * target reacted. It is an int function that returns no value (the return
 * register stays live at the exits). */
s32 func_80081B58(u8 member) {
    D_800D366C = 0;
    if (D_800C3EAC->unk2E1[0] != 0) {
        return;
    }
    D_800C3EAC->unk2DF = 0;
    func_8008189C(member);
    switch (D_800D3014) {
    case 5:
        if (D_800C3EAC->unk2D4[0] != D_800C3EAC->unk2D4[1]) {
            func_80080B64(member);
            D_800C3EAC->unk2E1[0] = 1;
        } else {
            func_800B8DA4();
            D_800D366C = 1;
            D_800D2D28->unk7B = 0;
            D_800D2D28->unkAF = 0;
            D_800C3EAC->page = 1;
            func_80077980();
            func_8009413C(member, 1);
        }
        break;
    case 4:
        if (D_800C3EAC->slots[member].items[2] != 0) {
            D_800D3014 = 5;
            func_8008AA40(0x4F);
        }
        break;
    case 7:
        if (D_800C3EAC->slots[member].items[1] != 0) {
            D_800D3014 = 5;
            func_8008AA40(0x4F);
        }
        break;
    case 6:
        if (D_800C3EAC->slots[member].items[0] != 0) {
            D_800D3014 = 5;
            func_8008AA40(0x4F);
        }
        break;
    }
    if (D_800C3EAC->unk2E9 == 0) {
        switch (D_800D3014) {
        case 0:
            D_800C3EAC->unk2E8 = func_80084854(D_800C3EAC->unk2E8, 0);
            func_800879A8(D_800C3EAC->actor, D_800C3EAC->unk2E8);
            func_8008AA40(0x4C);
            break;
        case 1:
            D_800C3EAC->unk2E8 = func_80084854(D_800C3EAC->unk2E8, 1);
            func_800879A8(D_800C3EAC->actor, D_800C3EAC->unk2E8);
            func_8008AA40(0x4C);
            break;
        case 2:
            D_800C3EAC->unk2E8 = func_80084854(D_800C3EAC->unk2E8, 2);
            func_800879A8(D_800C3EAC->actor, D_800C3EAC->unk2E8);
            func_8008AA40(0x4C);
            break;
        case 3:
            D_800C3EAC->unk2E8 = func_80084854(D_800C3EAC->unk2E8, 3);
            func_800879A8(D_800C3EAC->actor, D_800C3EAC->unk2E8);
            func_8008AA40(0x4C);
            break;
        }
        func_800877E0(member, D_800C3EAC->unk2E8);
    }
    D_800D2D28->unkAF = 1;
    switch (D_800D3014) {
    case 4:
        if (func_80085EB4(4, member) && D_800C3EAC->unk2D4[0] - 3 >= 0) {
            D_800C3EAC->unk2E1[3]++;
        }
        D_800C3EAC->unk2DF++;
    case 7:
        D_800C3EAC->unk2DF++;
    case 6:
        D_800C3EAC->unk2DF++;
        if (D_800C3EAC->unk2D4[0] - D_800C3EAC->unk2DF >= 0) {
            func_8008AA40(0x4D);
            func_800819A4(member);
            if (D_800D2D28->unkCB != 0) {
                D_800C3EAC->page = 0x64;
                D_800C3EAC->unk2E1[1] = 0xFF;
                D_800C3EAC->unk2E0 = 1;
            } else {
                D_800C3EAC->page = 5;
                D_800C3EAC->unk2E1[1] = 5;
            }
            D_800C3EAC->unk2D4[0] -= D_800C3EAC->unk2DF;
            func_80085D34();
            func_800861D0(D_800D3014, member);
            D_800C3EAC->unk2E1[4] = func_80087AF0(member, D_800C3EAC->unk2DF);
            D_800D2E06[member] = D_800C31D4[D_800C3EAC->unk2D4[1]][D_800C3EAC->unk2D4[0]] * 100 / 56;
            D_800C3EAC->unk2E1[2] = 1;
        } else {
            func_8008AA40(0x4F);
        }
        if (D_800C3EAC->unk2D4[0] == 0) {
            func_80080B64(member);
            D_800C3EAC->unk2E1[0] = 1;
        }
        if (D_800C3EAC->unk2E1[4] != 0) {
            func_80080B64(member);
            D_800C3EAC->unk2E1[0] = 1;
        }
        break;
    case 5:
        break;
    }
}

/* Confirm the selected entry of the member's special command window:
 * entries 4, 6 and 7 start the selection (800c3eac +0x2e1) when the member
 * has a target and a special is available, else open page 7; 1 and 0 open
 * pages 8 and 1 unless unavailable (buzzer 0x4f); 2 opens page 9, else on a
 * repeat press (800c3e29 = 2) page 3; 3 opens page 0xa, else on a repeat
 * press page 4. */
void func_800820A4(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        if (D_800C3EAC->slots[member].defaultTarget != 0xFF) {
            if (func_8008C4A8(member)) {
                D_800C3EAC->unk2E1[0] = 1;
            } else {
                D_800C3EAC->page = 7;
                D_800C3EAC->unk2E1[1] = 0xFF;
            }
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[11] == 0) {
            D_800C3EAC->page = 8;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        if (D_800C3EAC->slots[member].items[8] == 0) {
            D_800C3EAC->page = 9;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 2) {
            D_800C3EAC->page = 3;
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[7] == 0) {
            D_800C3EAC->page = 0xA;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[10] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 4;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 1;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's item window: entries 4, 6 and 7
 * open the item list when the member has one (else page 8); 0 opens page 7,
 * else on a repeat press (800c3e29 = 0) page 1; 2 opens page 9, else on a
 * repeat press (= 2) page 3; 3 opens page 0xa, else on a repeat press (= 3)
 * page 4; 1 opens page 2 unless unavailable (buzzer 0x4f). */
void func_800822C4(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        if (func_8008BED8(member)) {
            func_8008B908(member);
        } else {
            D_800C3EAC->page = 8;
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[5] == 0) {
            D_800C3EAC->page = 7;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[9] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 1;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        if (D_800C3EAC->slots[member].items[8] == 0) {
            D_800C3EAC->page = 9;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 2) {
            D_800C3EAC->page = 3;
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[7] == 0) {
            D_800C3EAC->page = 0xA;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[10] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 4;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[11] == 0) {
            D_800C3EAC->page = 2;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the escape command window: entries 4, 6 and
 * 7 try to flee (outcome 0x40 on success) and end the menu; 0 opens page 7
 * when available, else on a repeat press (800c3e29 = 0) page 1; 3 opens page
 * 0xa, else on a repeat press (800c3e29 = 3) page 4; 1 opens page 8 unless
 * unavailable (buzzer 0x4f); 2 opens page 3. */
void func_80082504(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        /* 8009a9d0 takes no arguments; the call passes the actor. */
        if (((s32 (*)())func_8009A9D0)(D_800C3EAC->actor)) {
            D_800C48EA = 0x40;
        }
        D_800C3EAC->menuDone = 1;
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[5] == 0) {
            D_800C3EAC->page = 7;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[9] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 1;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[11] == 0) {
            D_800C3EAC->page = 8;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[7] == 0) {
            D_800C3EAC->page = 0xA;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[10] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 4;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        D_800C3EAC->page = 3;
        break;
    }
}

/* The member boards its gear: it takes a formation group of its own, its
 * records and panel switch to the gear, and the game data notes that the
 * party member (8006f368) entered a gear unless 80059179 is set. */
#ifdef NON_MATCHING
void func_800826CC(u8 member) {
    s32 i;

    func_80088490(member);
    func_8009AEFC(member);
    func_800BAF48(member);
    D_800CCCE8.records[member].flags15A |= 0x80;
    func_800883AC(member);
    D_800D32A0[member].unk1 = 2;
    if (D_800D2D24[member] != 7) {
        D_800C3EA4->panels[member].state = 2;
    }
    D_800C3EB4[member].gear = 1;
    D_800C3EAC->reaction[member] = 1;
    for (i = 0; i < 3; i++) {
        if (D_800D2D24[member] == D_8006F368[i] && D_80059179 == 0) {
            D_8006F8E5[i] = 1;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800826CC);
#endif

/* Confirm the selected entry of the member's main command window: entries
 * 4, 6 and 7 board the gear and end the menu; 0 opens page 7 when available,
 * else on a second press of the repeat entry (800c3e29 = 0) page 1; 2 opens
 * page 9, else on a repeat press (800c3e29 = 2) page 3; 1 and 3 open pages 8
 * and 4 unless their item is unavailable (buzzer 0x4f). */
void func_80082820(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        func_800826CC(member);
        D_800C3EAC->menuDone = 1;
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[5] == 0) {
            D_800C3EAC->page = 7;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[9] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 1;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[11] == 0) {
            D_800C3EAC->page = 8;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        if (D_800C3EAC->slots[member].items[8] == 0) {
            D_800C3EAC->page = 9;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 2) {
            D_800C3EAC->page = 3;
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[10] == 0) {
            D_800C3EAC->page = 4;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's on-foot command window: entries
 * 4, 6 and 7 open the attack page (0x19) when the member has a target; 0 and
 * 1 open pages 0x15 and 0x11 unless their item is unavailable (buzzer 0x4f);
 * 2 opens page 0x12; 3 opens page 0x13 when available, else on a second
 * press of the repeat entry page 0x18. */
void func_800829F4(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        if (D_800C3EAC->slots[member].defaultTarget != 0xFF) {
            func_80087A38(member);
            func_80084A7C(member);
            D_800C3EAC->page = 0x19;
            func_80077698();
            D_800D366C = 0;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[12] == 0) {
            D_800C3EAC->page = 0x11;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        D_800C3EAC->page = 0x12;
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[6] == 0) {
            D_800C3EAC->page = 0x13;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[4] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x18;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[13] == 0) {
            D_800C3EAC->page = 0x15;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's gear command window: entries 4,
 * 6 and 7 open the part list when the gear has one (else page 0x11); 0 and 1
 * open pages 0x10 and 0x16 unless their item is unavailable (buzzer 0x4f);
 * 2 opens page 0x12; 3 opens page 0x13 when available, else on a second
 * press of the repeat entry (800c3e29 = 3) page 0x18. */
void func_80082BB0(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        if (func_8008BED8(member)) {
            func_8008B908(member);
        } else {
            D_800C3EAC->page = 0x11;
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 0x10;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        D_800C3EAC->page = 0x12;
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[6] == 0) {
            D_800C3EAC->page = 0x13;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[4] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x18;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[12] == 0) {
            D_800C3EAC->page = 0x16;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's gear window with charge:
 * entries 4, 6 and 7 charge the gear's fuel (by 800d2c32, capped) and end
 * the member's turn; 0, 1 and 2 open pages 0x10, 0x11 and 0x17 unless their
 * item is unavailable (buzzer 0x4f); 3 opens page 0x13 when available, else
 * on a second press of the repeat entry page 0x18. */
void func_80082D4C(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        func_8009AA44(member);
        D_800CCCE8.records[member].gear.fuel += D_800D2C32;
        if (D_800CCCE8.records[member].gear.fuel > D_800CCCE8.records[member].gear.maxFuel) {
            D_800CCCE8.records[member].gear.fuel = D_800CCCE8.records[member].gear.maxFuel;
        }
        D_800C3EAC->reaction[member] = 1;
        D_800C3EAC->unk2EA = 0;
        D_800C3EAC->menuDone = 1;
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 0x10;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[12] == 0) {
            D_800C3EAC->page = 0x11;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[6] == 0) {
            D_800C3EAC->page = 0x13;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[4] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x18;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        if (D_800C3EAC->slots[member].items[8] == 0) {
            D_800C3EAC->page = 0x17;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's command window (800d3014):
 * entries 4, 6 and 7 open the gear list when the member has one (else page
 * 0x13); 0, 1 and 3 open pages 0x10, 0x11 and 0x18 unless their item is
 * unavailable (buzzer 0x4f); 2 opens page 0x12. */
void func_80082F7C(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        func_8007FD38(member);
        if (func_8008B478(member)) {
            func_8008ADD0(member);
        } else {
            D_800C3EAC->page = 0x13;
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 0x10;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[12] == 0) {
            D_800C3EAC->page = 0x11;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        D_800C3EAC->page = 0x12;
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[4] == 0) {
            D_800C3EAC->page = 0x18;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's gear command window with the
 * guard toggle: entries 4, 6 and 7 toggle the guard (pilot and gear status
 * bit 0x8000; setting it clears gear bit 0x20 and pilot bit 0x1000) and end
 * the menu; 1 and 0 open pages 0x16 and 0x10 unless unavailable (buzzer
 * 0x4f); 2 opens page 0x17, else on a repeat press (800c3e29 = 2) page 0x12;
 * 3 opens page 0x18, else on a repeat press page 0x13. */
void func_800830A8(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        if (D_800CCCE8.records[member].gear.status80 & 0x8000) {
            D_800CCCE8.records[member].gear.status80 &= 0x7FFF;
            D_800CCCE8.records[member].pilot.status84.half.active &= 0x7FFF;
        } else {
            D_800CCCE8.records[member].gear.status7C &= ~0x20;
            D_800CCCE8.records[member].pilot.status7C &= ~0x1000;
            D_800CCCE8.records[member].gear.status80 |= 0x8000;
            D_800CCCE8.records[member].pilot.status84.half.active |= 0x8000;
        }
        D_800C3EAC->menuDone = 1;
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[12] == 0) {
            D_800C3EAC->page = 0x16;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        if (D_800C3EAC->slots[member].items[8] == 0) {
            D_800C3EAC->page = 0x17;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 2) {
            D_800C3EAC->page = 0x12;
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[4] == 0) {
            D_800C3EAC->page = 0x18;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[6] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x13;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[9] == 0) {
            D_800C3EAC->page = 0x10;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the member's item window (gear pages):
 * entries 4, 6 and 7 open the item list when the member has one (else page
 * 0x16); 0 opens page 0x15, else on a repeat press (800c3e29 = 0) page 0x10;
 * 2 opens page 0x17, else on a repeat press page 0x12; 3 opens page 0x18,
 * else on a repeat press page 0x13; 1 opens page 0x11 unless unavailable
 * (buzzer 0x4f). */
void func_80083340(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        if (func_8008BED8(member)) {
            func_8008B908(member);
        } else {
            D_800C3EAC->page = 0x16;
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[13] == 0) {
            D_800C3EAC->page = 0x15;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[9] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x10;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        if (D_800C3EAC->slots[member].items[8] == 0) {
            D_800C3EAC->page = 0x17;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 2) {
            D_800C3EAC->page = 0x12;
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[4] == 0) {
            D_800C3EAC->page = 0x18;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[6] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x13;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[12] == 0) {
            D_800C3EAC->page = 0x11;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* Confirm the selected entry of the escape window (gear pages): entries 4,
 * 6 and 7 try to flee (outcome 0x40 on success) and end the menu; 0 opens
 * page 0x15, else on a repeat press (800c3e29 = 0) page 0x10; 1 opens page
 * 0x16 unless unavailable (buzzer 0x4f); 3 opens page 0x18, else on a repeat
 * press page 0x13; 2 opens page 0x12. */
void func_80083580(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        /* 8009a9d0 takes no arguments; the call passes the actor. */
        if (((s32 (*)())func_8009A9D0)(D_800C3EAC->actor)) {
            D_800C48EA = 0x40;
        }
        D_800C3EAC->menuDone = 1;
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[13] == 0) {
            D_800C3EAC->page = 0x15;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[9] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x10;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[12] == 0) {
            D_800C3EAC->page = 0x16;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[4] == 0) {
            D_800C3EAC->page = 0x18;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 3) {
            if (D_800C3EAC->slots[member].items[6] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x13;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        D_800C3EAC->page = 0x12;
        break;
    }
}

/* Confirm the selected entry of the member's gear part window: entries 4, 6
 * and 7 load file 3 and open the part list when the gear has one (else page
 * 0x18); 0 opens page 0x15, else on a repeat press (800c3e29 = 0) page 0x10;
 * 1 and 3 open pages 0x16 and 0x13 unless unavailable (buzzer 0x4f); 2 opens
 * page 0x17, else on a repeat press page 0x12. */
void func_80083748(member)
u8 member;
{
    switch (D_800D3014) {
    case 4:
    case 6:
    case 7:
        /* 8007fe3c takes no arguments; the call passes the member. */
        ((void (*)())func_8007FE3C)(member);
        if (func_8008CFB8(member)) {
            func_8008ADD0(member);
        } else {
            D_800C3EAC->page = 0x18;
        }
        break;
    case 0:
        if (D_800C3EAC->slots[member].items[13] == 0) {
            D_800C3EAC->page = 0x15;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 0) {
            if (D_800C3EAC->slots[member].items[9] != 0) {
                func_8008AA74(0x4F);
            } else {
                D_800C3EAC->page = 0x10;
            }
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 1:
        if (D_800C3EAC->slots[member].items[12] == 0) {
            D_800C3EAC->page = 0x16;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    case 2:
        if (D_800C3EAC->slots[member].items[8] == 0) {
            D_800C3EAC->page = 0x17;
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E28[1] == 2) {
            D_800C3EAC->page = 0x12;
            D_800C3EAC->repeatArmed = 0;
        } else {
            D_800C3EAC->repeatArmed = 1;
            func_8008AA74(0x4F);
        }
        break;
    case 3:
        if (D_800C3EAC->slots[member].items[6] == 0) {
            D_800C3EAC->page = 0x13;
        } else {
            func_8008AA74(0x4F);
        }
        break;
    }
}

/* The gear attack page (0x19): like the attack page (80081b58) but paid in
 * fuel. The gear HUD level (4: free combos) and the chain (+0x2d6, history
 * +0x2cc) pick the step's fuel cost from the gear HUD table, indexed like the
 * combo flags (80086b88). Cancel (5) leaves the page unless steps were
 * taken; the command closes once the chain cannot continue, the target is
 * down or the attack ended the chain. Int function without a value. */
s32 func_80083948(u8 member) {
    s32 steps = 0;
    u16 fuel = D_800CCCE8.records[member].gear.fuel;
    u8 enough = 0;
    u8 leave;
    s32 index;
    s32 cost;

    D_800D366C = 0;
    if (D_800C3EAC->unk2E1[0] != 0) {
        return;
    }
    func_8008189C(member);
    switch (D_800D3014) {
    case 5:
        leave = 1;
        if (D_800C3EAC->unk2E7 != 0 && D_800CCCE8.gearHud.level != 4) {
            func_80080B64(member);
            D_800C3EAC->unk2E1[0] = leave;
            leave = 0;
        }
        if (leave) {
            D_800D2D28->unkA8 = 0;
            func_800B8DA4();
            D_800D366C = 1;
            D_800C3EAC->page = 0x10;
            func_80077980();
            if (D_800CCCE8.gearHud.level != 4) {
                func_8009413C(member, 1);
            } else {
                D_800C3EAC->unk2E7 = 0;
                D_800C3EAC->unk2D6 = 0;
            }
        }
        break;
    case 4:
        if (D_800C3EAC->slots[member].items[2] != 0) {
            D_800D3014 = 5;
            func_8008AA40(0x4F);
        }
        break;
    case 7:
        if (D_800C3EAC->slots[member].items[1] != 0) {
            D_800D3014 = 5;
            func_8008AA40(0x4F);
        }
        break;
    case 6:
        if (D_800C3EAC->slots[member].items[0] != 0) {
            D_800D3014 = 5;
            func_8008AA40(0x4F);
        }
        break;
    }
    if (D_800C3EAC->unk2E9 == 0) {
        switch (D_800D3014) {
        case 0:
            D_800C3EAC->unk2E8 = func_80084854(D_800C3EAC->unk2E8, 0);
            func_8008AA40(0x4C);
            break;
        case 1:
            D_800C3EAC->unk2E8 = func_80084854(D_800C3EAC->unk2E8, 1);
            func_8008AA40(0x4C);
            break;
        case 2:
            D_800C3EAC->unk2E8 = func_80084854(D_800C3EAC->unk2E8, 2);
            func_8008AA40(0x4C);
            break;
        case 3:
            D_800C3EAC->unk2E8 = func_80084854(D_800C3EAC->unk2E8, 3);
            func_8008AA40(0x4C);
            break;
        }
    }
    D_800D2D28->reaction[member] = 1;
    if (D_800C3EAC->unk2E7 != 0) {
        if (D_800CCCE8.gearHud.level != 4 &&
            (D_800CCCE8.gearHud.level == 0 || D_800CCCE8.gearHud.level - 1 < D_800C3EAC->unk2CC[0])) {
            func_80080B64(member);
            D_800C3EAC->unk2E1[0] = 1;
            return;
        }
        if (func_80089C9C(D_800C48E8, D_800C3EAC->slots[member].defaultTarget)) {
            func_80080B64(member);
            D_800C3EAC->unk2E1[0] = 1;
            return;
        }
    }
    switch (D_800D3014) {
    default:
        if (D_800CCCE8.gearHud.level == 4 && D_800C3EAC->unk2E7 == 0) {
            D_800C3EAC->unk2E7++;
            func_80086F98(0xFF, member);
        }
        break;
    case 5:
        break;
    case 4:
        steps++;
    case 7:
        steps++;
    case 6:
        steps++;
        if (D_800CCCE8.gearHud.level == 4 && D_800C3EAC->unk2E7 == 0) {
            D_800C3EAC->unk2E7++;
            func_80086F98(0xFF, member);
            break;
        }
        /* The call narrows like a u8 (u8, u8) prototype. */
        if ((u8)func_80086B88((u8)(steps - 1), member)) {
            if (D_800C3EAC->unk2D6 == 0) {
                cost = STEP_FUEL[steps];
            } else if (D_800CCCE8.gearHud.level == 4) {
                index = steps + 12;
                cost = STEP_FUEL[index];
            } else {
                cost = STEP_FUEL[steps + (D_800C3EAC->unk2CC[0] + 1) * 3];
            }
            if (fuel - cost >= 0) {
                enough = 1;
            }
            if (enough) {
                func_8008AA40(0x4D);
                D_800C4929 = 1;
                func_800819A4(member);
                if (D_800C3EAC->unk2D6 != 0) {
                    D_800C3EAC->unk2E1[3]++;
                }
                if (D_800D2D28->unkCB != 0) {
                    D_800C3EAC->page = 0x65;
                    D_800C3EAC->unk2E1[1] = 0xFF;
                    D_800C3EAC->unk2E0 = 1;
                } else {
                    D_800C3EAC->page = 0x19;
                    D_800C3EAC->unk2E1[1] = 0x19;
                }
                D_800C3EAC->unk2E1[3] = func_80086F98(steps - 1, member) == 0;
                func_80087AF0(member, steps);
                D_800CCCE8.records[member].gear.fuel -= STEP_FUEL[D_800C3EAC->unk2DC];
                func_800898F0(member);
                D_800C3EAC->unk2E7++;
            } else {
                D_800D366C = 1;
                func_8008AA74(0x4F);
                D_800D366C = 0;
            }
            if (D_800C3EAC->unk2E1[3] != 0) {
                func_80080B64(member);
                D_800C3EAC->unk2E1[0] = 1;
            }
        }
        break;
    }
}

/* Whether the member can attack `slot`: present and visible; an unflagged
 * slot must be adjacent (formation distance 0) and not down, a flagged slot
 * not down by +0x120. */
u8 func_80083FF4(u8 member, u8 slot) {
    u8 result = 0;

    if (D_800D2DCC.present[slot] != 0 && D_800C3EB4[slot].hidden == 0) {
        if (D_800D32A0[slot].unk1 == 0) {
            if (D_800D3364->links[D_800C3EB4[member].group][D_800C3EB4[slot].group].distance == 0) {
                result = (D_800CCCE8.records[slot].pilot.status7C & 0xC001) == 0;
            }
        } else if (!(D_800CCCE8.records[slot].gear.status7C & 0xC001)) {
            result = 1;
        }
    }
    return result;
}

/* Whether `slot` can be targeted by a party attack: present and visible;
 * unflagged slots also not down (+0x7c 0xc001) unless `any`, flagged slots
 * not down by +0x120 unless `any`. */
u8 func_80084108(u8 slot, u8 any) {
    u8 result = 0;

    if (D_800D2DCC.present[slot] != 0 && D_800C3EB4[slot].hidden == 0) {
        if (D_800D32A0[slot].unk1 == 0) {
            result = 1;
            if (any == 0) {
                result = (D_800CCCE8.records[slot].pilot.status7C & 0xC001) == 0;
            }
        } else if (any != 0 || !(D_800CCCE8.records[slot].gear.status7C & 0xC001)) {
            result = 1;
        }
    }
    return result;
}

/* Order the member's attack candidates: the reachable opposing slots, those
 * in its own formation group first, and the lowest-HP one (within the group
 * when there is one) moved to the front. Returns the default target. */
#ifdef NON_MATCHING
u8 func_800841E0(u8 member) {
    u8 slots[12];
    u8 grouped[12];
    s32 i;
    s32 count;
    s32 n;
    u8 swap;

    D_800D3274 = 0;
    for (i = 0; i < 12; i++) {
        slots[i] = 0xFF;
        D_800C3E90[i] = 0xFF;
        grouped[i] = 0;
    }
    if (member < 3) {
        i = 3;
        count = 0;
        for (; i < 11; i++) {
            if (func_80083FF4(member, i)) {
                slots[count++] = i;
                D_800D3274++;
            }
        }
    } else {
        i = 0;
        count = 0;
        for (; i < 3; i++) {
            if (func_80083FF4(member, i)) {
                slots[count++] = i;
                D_800D3274++;
            }
        }
    }
    n = 0;
    for (i = 0; i < count; i++) {
        if (D_800C3EB4[member].group == D_800C3EB4[slots[i]].group) {
            D_800C3E90[n] = slots[i];
            slots[i] = 0xFF;
            grouped[n] = 1;
            n++;
        }
    }
    for (i = 0; i < count; i++) {
        if (slots[i] != 0xFF) {
            D_800C3E90[n++] = slots[i];
        }
    }
    if (grouped[0] != 0) {
        for (i = 1; i < count; i++) {
            if (grouped[i] != 0 && D_800CCCE8.records[D_800C3E90[i]].pilot.hp < D_800CCCE8.records[D_800C3E90[0]].pilot.hp) {
                swap = D_800C3E90[0];
                D_800C3E90[0] = D_800C3E90[i];
                D_800C3E90[i] = swap;
            }
        }
    } else {
        for (i = 1; i < count; i++) {
            if (D_800CCCE8.records[D_800C3E90[i]].pilot.hp < D_800CCCE8.records[D_800C3E90[0]].pilot.hp) {
                swap = D_800C3E90[0];
                D_800C3E90[0] = D_800C3E90[i];
                D_800C3E90[i] = swap;
            }
        }
    }
    return D_800C3E90[0];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800841E0);
#endif

/* Collect the slots a party attack can target (80084108, `any` includes
 * downed ones) as candidates with their mask: side 0 the enemies, 1 the
 * party, 2 both (the party first unless `partyFirst` is clear). Returns the
 * first candidate. */
#ifdef NON_MATCHING
u8 func_80084548(u8 side, u8 any, u8 partyFirst) {
    s32 i;
    s32 count;
    s32 n1;
    s32 n2 = 0;
    u8 first1;
    u8 first2;
    s32 slot;

    for (i = 0; i < 12; i++) {
        D_800C3E90[i] = 0xFF;
    }
    switch (side) {
    case 0:
        first1 = 3;
        n1 = 8;
        break;
    case 1:
        first1 = 0;
        n1 = 3;
        break;
    case 2:
        if (partyFirst == 0) {
            first1 = 3;
            n1 = 8;
            first2 = 0;
            n2 = 3;
        } else {
            first1 = 0;
            n1 = 3;
            first2 = 3;
            n2 = 8;
        }
        break;
    }
    count = 0;
    D_800D3274 = 0;
    D_800C3D64 = 0;
    i = first1;
    while (--n1 >= 0) {
        slot = i++;
        if (func_80084108(slot, any)) {
            D_800C3E90[count++] = slot;
            D_800C3D64 |= func_80089C08(slot);
            D_800D3274++;
        }
    }
    i = first2;
    while (--n2 >= 0) {
        slot = i++;
        if (func_80084108(slot, any)) {
            D_800C3E90[count++] = slot;
            D_800C3D64 |= func_80089C08(slot);
            D_800D3274++;
        }
    }
    return D_800C3E90[0];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084548);
#endif

/* Collect the enemy slots the member can target (80083ff4) as candidates
 * and their mask; returns the first candidate. */
#ifdef NON_MATCHING
u8 func_80084750(u8 member) {
    s32 i;
    s32 n;
    s32 count;
    s32 slot;

    n = 8;
    for (i = 0; i < 12; i++) {
        D_800C3E90[i] = 0xFF;
    }
    count = 0;
    D_800D3274 = 0;
    D_800C3D64 = 0;
    i = 3;
    while (--n >= 0) {
        slot = i++;
        if (func_80083FF4(member, slot)) {
            D_800C3E90[count++] = slot;
            D_800C3D64 |= func_80089C08(slot);
            D_800D3274++;
        }
    }
    return D_800C3E90[0];
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084750);
#endif

/* The candidate nearest to `origin` that lies in screen direction
 * `direction` (0-3, each a quarter turn around it); `origin` when none. */
u8 func_80084854(u8 origin, u8 direction) {
    s32 best = 0xFFFFFF;
    s32 i;
    s32 angle;
    s32 inside;
    s32 distance;
    u8 nearest = origin;

    for (i = 0; i < 11; i++) {
        if (D_800C3E90[i] != 0xFF && D_800C3E90[i] != origin) {
            inside = 0;
            angle = ratan2(SLOT_Z(D_800C3E90[i]) - SLOT_Z(origin), SLOT_X(D_800C3E90[i]) - SLOT_X(origin));
            switch (direction) {
            case 0:
                if ((u16)(angle + 0x200) < 0x400) {
                    inside = 1;
                }
                break;
            case 1:
                if ((u16)(angle + 0x600) < 0x400) {
                    inside = 1;
                }
                break;
            case 2:
                if ((u16)(angle + 0x800) < 0x200) {
                    inside = 1;
                }
                if ((u16)(angle - 0x600) <= 0x200) {
                    inside = 1;
                }
                break;
            case 3:
                if ((u16)(angle - 0x200) < 0x400) {
                    inside = 1;
                }
                break;
            }
            if (inside) {
                distance = SQUARE(SLOT_Z(D_800C3E90[i]) - SLOT_Z(origin));
                distance += SQUARE(SLOT_X(D_800C3E90[i]) - SLOT_X(origin));
                if (distance < best) {
                    best = distance;
                    nearest = D_800C3E90[i];
                }
            }
        }
    }
    return nearest;
}

/* Keep the member's default target as the attack page target when it is
 * still a candidate, else take the first candidate. */
void func_80084A7C(u8 member) {
    s32 found = 0;
    s32 i;

    D_800C3EAC->unk2E8 = D_800C3EAC->slots[member].defaultTarget;
    func_800841E0(member);
    for (i = 0; i < D_800D3274; i++) {
        if (D_800C3E90[i] == D_800C3EAC->slots[member].defaultTarget) {
            found++;
            break;
        }
    }
    if (found == 0) {
        D_800C3EAC->unk2E8 = D_800C3E90[0];
    }
}

/* Pick a target with the direction keys, starting from the attack page
 * target: highlight member and target each frame; 0-3 move to the nearest
 * candidate that way, 4/6/7 confirm it as the member's default target (1)
 * and 5 cancels back to the default target (0). */
u8 func_80084B40(u8 member) {
    u8 state = 2;
    u8 target = D_800C3EAC->unk2E8;

    D_800D3014 = 8;
    do {
        while (D_800D3014 == 8) {
            func_800BC404(func_80089C08(member) | func_80089C08(target));
            func_800BCD98(func_80089C08(target));
            func_800716D8();
        }
        switch (D_800D3014) {
        case 4:
        case 6:
        case 7:
            state = 1;
            D_800C3EAC->slots[member].defaultTarget = target;
            break;
        case 5:
            func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
            state = 0;
            func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
            break;
        case 0:
            target = func_80084854(target, 0);
            break;
        case 1:
            target = func_80084854(target, 1);
            break;
        case 2:
            target = func_80084854(target, 2);
            break;
        case 3:
            target = func_80084854(target, 3);
            break;
        }
        D_800D3014 = 8;
    } while (state == 2);
    return state;
}

/* The mask of slots in 800c3d64 in the formation group of slot 800c3e2c. */
u16 func_80084D28(void) {
    u16 mask = 0;
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        if (func_80089C9C(D_800C3D64, slot) && D_800C3EB4[D_800C3E2C].group == D_800C3EB4[slot].group) {
            mask |= func_80089C08(slot);
        }
    }
    return mask;
}

/* Set up the target candidates for a target selection word (mode 0: the
 * fallback's, 1: 0x3000, 2: 0x2001). Bit 0x1000 selects the enemies, else
 * the party; 0x2000 both; 0x8000 keeps only downed slots (+0x7c bit
 * 0x8000); 0x4000 the member alone. `own` takes the member's reachable
 * enemies (80084750). Sets the current target 800c3e2c and returns the
 * selection. */
#ifdef NON_MATCHING
u16 func_80084DE4(u16 selection, u16 fallback, u8 member, u8 mode, u8 own) {
    u8 downed = 0;
    u8 side;
    u8 partyFirst;
    u16 mask;
    s32 count;
    s32 i;
    u8 slot;

    switch (mode) {
    case 0:
        selection = fallback;
        break;
    case 1:
        selection = 0x3000;
        break;
    case 2:
        selection = 0x2001;
        break;
    }
    if (selection & 0x8000) {
        downed = 1;
    }
    partyFirst = 0;
    if (selection & 0x1000) {
        side = 0;
        mask = 0xFFF8;
    } else {
        partyFirst = 1;
        side = 1;
        mask = 7;
    }
    if (selection & 0x2000) {
        side = 2;
        mask = 0xFFFF;
    }
    if (own) {
        D_800C3E2C = func_80084750(member);
    } else {
        D_800C3E2C = func_80084548(side, downed, partyFirst);
    }
    D_800C3D64 &= mask;
    if (selection & 0x8000) {
        count = 0;
        D_800C3E2C = 0xFF;
        for (i = 0; i < 11; i++) {
            slot = i;
            if (func_80089C9C(D_800C3D64, slot)) {
                if (D_800D32A0[i].unk1 == 0 ? D_800CCCE8.records[i].pilot.status7C & 0x8000
                                            : D_800CCCE8.records[i].gear.status7C & 0x8000) {
                    D_800C3E2C = slot;
                    D_800C3E90[count++] = slot;
                } else {
                    D_800C3D64 &= func_80089C48(slot);
                }
            }
        }
        for (; count < 11; count++) {
            D_800C3E90[count] = 0xFF;
        }
    } else if (selection & 0x4000) {
        D_800C3E2C = member;
        D_800C3D64 = func_80089C08(member);
        for (i = 0; i < 11; i++) {
            D_800C3E90[i] = 0xFF;
        }
    }
    return selection;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80084DE4);
#endif

/* Select a target for selection word `target` (80084de4): each frame
 * highlight the current target (mode 0), all candidates (1) or those in the
 * current target's formation group (2); keys 0-3 move to the nearest
 * candidate that way, 4 confirms (1) and 5 cancels (0). The direction
 * arrows are refreshed whenever the key changes. Returns 0 when there is no
 * candidate. */
u8 func_80085084(u16 target, u8 member, s32 mode) {
    u8 lastKey = 0xFE;
    u8 state;
    u16 group;
    u8 next;
    s32 direction;

    func_80084DE4(target, target, member, 0, mode);
    group = D_800C3D64;
    state = D_800C3E2C == 0xFF;
    while (state == 0) {
        func_800716D8();
        switch (target & 0xF) {
        case 0:
            func_800BCD98(func_80089C08(D_800C3E2C));
            func_800BC404(func_80089C08(D_800C3E2C));
            break;
        case 1:
            func_800BCD98(D_800C3D64);
            func_800BC404(D_800C3D64);
            break;
        case 2:
            group = func_80084D28();
            func_800BCD98(group);
            func_800BC404(group);
            break;
        }
        switch (D_800D3014) {
        case 5:
            state = 1;
            break;
        case 4:
            D_800C3D64 = group;
            state = 2;
            break;
        case 0:
            next = func_80084854(D_800C3E2C, 0);
            if (func_80089C9C(D_800C3D64, next)) {
                D_800C3E2C = next;
            }
            break;
        case 1:
            next = func_80084854(D_800C3E2C, 1);
            if (func_80089C9C(D_800C3D64, next)) {
                D_800C3E2C = next;
            }
            break;
        case 2:
            next = func_80084854(D_800C3E2C, 2);
            if (func_80089C9C(D_800C3D64, next)) {
                D_800C3E2C = next;
            }
            break;
        case 3:
            next = func_80084854(D_800C3E2C, 3);
            if (func_80089C9C(D_800C3D64, next)) {
                D_800C3E2C = next;
            }
            break;
        case 6:
        case 7:
            break;
        }
        if (D_800D3014 != lastKey) {
            lastKey = D_800D3014;
            for (direction = 0; direction < 4; direction++) {
                if (func_80084854(D_800C3E2C, direction) != D_800C3E2C) {
                    D_800C3E24->arrows[direction] = 1;
                } else {
                    D_800C3E24->arrows[direction] = 0;
                }
            }
        }
    }
    return state - 1;
}

/* Whether slot b's slot-info +0xa is below slot a's. */
s32 func_80085310(u8 a, u8 b) {
    return (u16)D_800C3EB4[a].x > (u16)D_800C3EB4[b].x;
}

/* Reset the running result accumulation of every slot. */
void func_80085350(void) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        D_800D2D5C[slot] = 0xFF;
        D_800D2D70[slot] = 0;
    }
}

/* Clear the current event's per-slot results (the event index is re-read for
 * every store). */
void func_80085388(void) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        D_800C3FE8[D_800C3EAC->eventCount].amounts[slot] = 0;
        D_800C3FE8[D_800C3EAC->eventCount].codes[slot] = 0xFF;
        D_800C3FE8[D_800C3EAC->eventCount].accumulated[slot] = 0;
        D_800C3FE8[D_800C3EAC->eventCount].accumulatedCodes[slot] = 0xFF;
    }
}

/* Copy the resolver's damage and result codes into event `queue` and
 * accumulate them into the running amount and code of each slot: damage
 * (codes 0 and 5) and healing (code 2) add up or cancel out, any other code
 * replaces the running result. */
void func_80085454(u8 queue) {
    s32 slot;

    for (slot = 0; slot < 11; slot++) {
        D_800C3FE8[queue].amounts[slot] = D_800D2C54[slot];
        D_800C3FE8[queue].codes[slot] = D_800D2C88[slot];
        switch (D_800D2C88[slot]) {
        case 0:
        case 5:
            if (D_800D2D5C[slot] == 0 || D_800D2D5C[slot] == 5) {
                D_800D2D70[slot] += D_800D2C54[slot];
            } else if (D_800D2D5C[slot] == 2) {
                if ((s16)D_800D2C54[slot] - D_800D2D70[slot] < 0) {
                    D_800D2D70[slot] = D_800D2D70[slot] - (s16)D_800D2C54[slot];
                } else {
                    D_800D2D70[slot] = (s16)D_800D2C54[slot] - D_800D2D70[slot];
                    D_800D2D5C[slot] = 0;
                }
            } else {
                D_800D2D70[slot] = D_800D2C54[slot];
                D_800D2D5C[slot] = D_800D2C88[slot];
            }
            break;
        case 2:
            if (D_800D2D5C[slot] == 2) {
                D_800D2D70[slot] += D_800D2C54[slot];
            } else if (D_800D2D5C[slot] == 0 || D_800D2D5C[slot] == 5) {
                if ((s16)D_800D2C54[slot] - D_800D2D70[slot] < 0) {
                    D_800D2D70[slot] = D_800D2D70[slot] - (s16)D_800D2C54[slot];
                } else {
                    D_800D2D70[slot] = (s16)D_800D2C54[slot] - D_800D2D70[slot];
                    D_800D2D5C[slot] = 2;
                }
            } else {
                D_800D2D70[slot] = D_800D2C54[slot];
                D_800D2D5C[slot] = D_800D2C88[slot];
            }
            break;
        }
        D_800C3FE8[queue].accumulated[slot] = D_800D2D70[slot];
        D_800C3FE8[queue].accumulatedCodes[slot] = D_800D2D5C[slot];
    }
}

/* Apply the results of event `queue` to every present slot: damage (codes
 * 0, 5, 7, 8) to HP or, in a gear, gear HP (knocking the slot out at 0),
 * healing (2) up to the maximum, EP loss (1, 9) and gain (3), fuel loss
 * (10) and gain (11). A knocked-out slot only joins the 800c48e8 mask.
 * Slots whose values changed get their refresh flag (+0x2eb). */
#ifdef NON_MATCHING
void func_80085618(u8 queue) {
    s32 slot;
    s32 left;

    for (slot = 0; slot < 11; slot++) {
        if (D_800D2DCC.present[slot] == 0) {
            continue;
        }
        if (D_800CCCE8.records[slot].pilot.status7C & 0x8000) {
            D_800C48E8 |= func_80089C08(slot);
            continue;
        }
        switch (D_800C3FE8[queue].codes[slot]) {
        case 0:
        case 5:
        case 7:
        case 8:
            if (D_800C3EB4[slot].gear == 0) {
                left = D_800CCCE8.records[slot].pilot.hp - (s16)D_800C3FE8[queue].amounts[slot];
                if (left > 0) {
                    D_800CCCE8.records[slot].pilot.hp = left;
                    break;
                }
                D_800CCCE8.records[slot].pilot.hp = 0;
                D_800C48E8 |= func_80089C08(slot);
                D_800CCCE8.records[slot].pilot.status7C |= 0x8000;
            } else {
                left = D_800CCCE8.records[slot].gear.hp - D_800C3FE8[queue].amounts[slot];
                if (left > 0) {
                    D_800CCCE8.records[slot].gear.hp = left;
                    break;
                }
                D_800CCCE8.records[slot].gear.hp = 0;
                D_800C48E8 |= func_80089C08(slot);
                D_800CCCE8.records[slot].gear.status7C |= 0x8000;
                D_800CCCE8.records[slot].pilot.status7C |= 0x8000;
            }
            if (slot >= 3) {
                func_800883AC(slot);
            }
            break;
        case 2:
            if (D_800C3EB4[slot].gear != 0 && D_800C2050 == 0) {
                D_800CCCE8.records[slot].gear.hp += D_800C3FE8[queue].amounts[slot];
                if (D_800CCCE8.records[slot].gear.maxHp < D_800CCCE8.records[slot].gear.hp) {
                    D_800CCCE8.records[slot].gear.hp = D_800CCCE8.records[slot].gear.maxHp;
                }
            } else {
                D_800CCCE8.records[slot].pilot.hp += D_800C3FE8[queue].amounts[slot];
                if (D_800CCCE8.records[slot].pilot.maxHp < D_800CCCE8.records[slot].pilot.hp) {
                    D_800CCCE8.records[slot].pilot.hp = D_800CCCE8.records[slot].pilot.maxHp;
                }
            }
            break;
        case 1:
        case 9:
            left = (s16)D_800CCCE8.records[slot].pilot.ep - (s16)D_800C3FE8[queue].amounts[slot];
            if (left > 0) {
                D_800CCCE8.records[slot].pilot.ep = left;
            } else {
                D_800CCCE8.records[slot].pilot.ep = 0;
            }
            continue;
        case 3:
            if (D_800C3EB4[slot].gear != 0 && D_800C2050 == 0) {
                continue;
            }
            D_800CCCE8.records[slot].pilot.ep += D_800C3FE8[queue].amounts[slot];
            if (D_800CCCE8.records[slot].pilot.maxEp < D_800CCCE8.records[slot].pilot.ep) {
                D_800CCCE8.records[slot].pilot.ep = D_800CCCE8.records[slot].pilot.maxEp;
            }
            continue;
        case 10:
            if (D_800CCCE8.records[slot].gear.fuel - D_800C3FE8[queue].amounts[slot] > 0) {
                D_800CCCE8.records[slot].gear.fuel -= D_800C3FE8[queue].amounts[slot];
            } else {
                D_800CCCE8.records[slot].gear.fuel = 0;
            }
            break;
        case 11:
            D_800CCCE8.records[slot].gear.fuel += D_800C3FE8[queue].amounts[slot];
            if (D_800CCCE8.records[slot].gear.maxFuel < D_800CCCE8.records[slot].gear.fuel) {
                D_800CCCE8.records[slot].gear.fuel = D_800CCCE8.records[slot].gear.maxFuel;
            }
            break;
        default:
            continue;
        }
        D_800C3EAC->reaction[slot] = 1;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085618);
#endif

/* Revive slot at full HP and clear its timed statuses (the active halves of
 * the status words 0x7c-0x80 and 0x84-0x8c). */
#ifdef NON_MATCHING
void func_80085AC4(slot)
u8 slot;
{
    s32 i;
    u16 *status;

    status = &D_800CCCE8.records[slot].pilot.status7C;
    D_800CCCE8.records[slot].pilot.hp = D_800CCCE8.records[slot].pilot.maxHp;
    for (i = 2; i >= 0; i -= 2) {
        status[i] = 0;
    }
    status = &D_800CCCE8.records[slot].pilot.status84.half.active;
    for (i = 4; i >= 0; i -= 2) {
        status[i] = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80085AC4);
#endif

/* Apply up to three recovery amounts from 8009ada0 to `slot` as separate
 * events (codes 8..10) and show them. */
void func_80085B58(u8 slot) {
    s32 amounts[3];
    s32 i;

    amounts[2] = 0;
    amounts[1] = 0;
    amounts[0] = 0;
    if (func_8009ADA0(slot, amounts) != 0) {
        for (i = 0; i < 3; i++) {
            if (amounts[i] != 0) {
                D_800C3EAC->eventCount = 0;
                func_80085388();
                D_800C3FE8[0].codes[slot] = i + 8;
                D_800C3FE8[0].amounts[slot] = amounts[i];
                func_80085618(D_800C3EAC->eventCount);
            }
        }
        func_800BE538(slot, amounts[0], amounts[1], amounts[2]);
    }
}

/* Commit the targets for an item/effect and run 80098c6c with `param`. */
void func_80085C48(actor, targets, param)
u8 actor;
s16 targets;
u16 param;
{
    D_800C48E8 = 0;
    D_800D2C94.targets = targets;
    D_800D2C94.alive = D_800D39DC;
    func_80098C6C(param);
}

/* Accumulate and apply event `queue`'s results. */
void func_80085C88(u8 queue) {
    func_80085454(queue);
    func_80085618(queue);
    D_800D2D28->unkAD = 0;
}

/* Commit an action (attacker, target mask, animation) and resolve it. */
void func_80085CCC(u8 actor, u16 targets, u16 animation) {
    u8 action; /* 1-based */

    D_800C48E8 = 0;
    D_800D2C94.actor = actor;
    action = D_800C3EAC->unk2DC;
    D_800D2C94.targets = targets;
    D_800D2C94.animation = animation;
    D_800D2C94.alive = D_800D39DC;
    D_800D2C94.action = action - 1;
    func_800941A4();
}

/* Build the attack page's AP text (current AP, '/', maximum) as glyph
 * primitives and remember the draw buffer. */
void func_80085D34(void) {
    D_800D2D28->unk7B = 0;
    D_800D2D28->unk7B +=
        func_80076A10(D_800C3EAC->unk2D4[0] + 0xF, D_800C3EA4->unk9C8[D_800D2D28->unk7B], 0x2A, 0xD0);
    D_800D2D28->unk7B += func_80076A10(0x19, D_800C3EA4->unk9C8[D_800D2D28->unk7B], 0x32, 0xD0);
    D_800D2D28->unk7B +=
        func_80076A10(D_800C3EAC->unk2D4[1] + 0xF, D_800C3EA4->unk9C8[D_800D2D28->unk7B], 0x3A, 0xD0);
    D_800D2D28->unkA4 = D_800CCB04.buffer;
}

/* Reset the turn state's seven +0x2cc bytes to 0xff and clear +0x2d6. */
void func_80085E78(void) {
    s32 i;

    for (i = 0; i < 7; i++) {
        D_800C3EAC->unk2CC[i] = 0xFF;
    }
    D_800C3EAC->unk2D6 = 0;
}

/* Mode 4: whether the combo input history (+0x2cc) matches one of the 13
 * combo patterns whose deathblow the member's character knows. */
u8 func_80085EB4(u8 mode, u8 member) {
    u8 result = 0;
    s32 match;
    s32 combo;
    s32 i;

    if (D_800C3EAC->unk2D6 != 0 && mode == 4) {
        for (combo = 0; combo < 13; combo++) {
            for (i = 0; i < 7; i++) {
                if (D_800C3EAC->unk2CC[i] == D_800C3160[combo][i]) {
                    match = 1;
                } else {
                    match = 0;
                    break;
                }
            }
            i = 0;
            if (match) {
                break;
            }
        }
        /* The deathblow index counts down from the last combo (the switch
         * reuses the match flag's variable). */
        match = combo;
        switch (match) {
        case 0:
            i++;
        case 1:
            i++;
        case 2:
            i++;
        case 3:
            i++;
        case 4:
            i++;
        case 5:
            i++;
        case 6:
            i++;
        case 7:
            i++;
        case 8:
            i++;
        case 9:
            i++;
        case 10:
            i++;
        case 11:
            i++;
        case 12:
            if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask0,
                              D_800C31AC[D_800CCCE8.records[member].pilot.characterId][12 - i])) {
                result = 1;
            }
            break;
        }
    }
    return result;
}

/* Add entry `index` to list 11 (the combo chain display): render the
 * member's text `id` into the shared image (two entries per image cell),
 * upload it and place its quad after `column` + `offset` + 1 steps.
 * Returns the next index. */
#ifdef NON_MATCHING
s32 func_80086028(member, index, column, id, pixels, offset)
u8 member;
s32 index;
s32 column;
u8 id;
u32 **pixels;
u8 offset;
{
    s32 cell;
    s32 odd;
    RECT rect;
    s32 width;

    cell = index / 2;
    odd = index % 2;
    func_80076D58(&D_800D2DB4->list11[index * 2], odd, 3);
    width = func_80034EAC(func_80033784(D_800D2D24[member], id), *pixels, 0x1B, odd);
    rect.x = cell * 30 + 0x3C0;
    rect.y = 0x1A;
    rect.w = 0x1E;
    rect.h = 13;
    LoadImage(&rect, *pixels);
    func_80076C78(&D_800D2DB4->list11[index * 2 + D_800CCB04.buffer], (column + (offset + 1)) * 16 + 0x50 + index * 4,
                  0xC8 - index * 16, cell * 0x78, 0x1A, width);
    D_800D2DB4->counts[11]++;
    return index + 1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086028);
#endif

/* Record attack input `code` in the combo history (+0x2cc, length +0x2d6)
 * and show it (list 12) with the deathblows it completes or leads into:
 * the combo it spells when its character knows it and the attack is
 * available, else (unless the chain is armed) the longer combos it
 * continues. With an armed chain a completed deathblow becomes the combo step
 * (+0x2dc). The three text image blocks live for one frame. */
#ifdef NON_MATCHING
void func_800861D0(u8 code, u8 member) {
    s32 index = 0;
    s32 shown = 0;
    s32 block;
    s32 combo;
    s32 match;
    s32 i;
    u8 id;

    for (block = 0; block < 3; block++) {
        D_800C3A70[block] = (u32 *)func_8008AC00(0x1E);
    }
    D_800C3EAC->unk2CC[D_800C3EAC->unk2D6] = code - 4;
    D_800C3EAC->unk2D6++;
    if (D_800C3EAC->unk2E1[3] != 0) {
        D_800C3EAC->unk2CC[D_800C3EAC->unk2D6 - 1] = 0xFF;
    }
    for (combo = 0; combo < 13; combo++) {
        for (i = 0; i < 7; i++) {
            if (D_800C3EAC->unk2CC[i] == D_800C3160[combo][i]) {
                match = 1;
            } else {
                match = 0;
                break;
            }
        }
        if (match) {
            break;
        }
    }
    /* From here the match flag's variable holds the combo. */
    match = combo;
    if (D_800C3EAC->unk2E1[3] != 0) {
        D_800C3EAC->unk2CC[D_800C3EAC->unk2D6 - 1] = code - 4;
    }
    D_800D2DB4->counts[12] = 0;
    D_800D2DB4->counts[11] = 0;
    for (combo = 0; combo < D_800C3EAC->unk2D6; combo++) {
        switch (D_800C3EAC->unk2CC[combo]) {
        case 0:
            id = 0x5D;
            break;
        case 2:
            id = 0x5E;
            break;
        case 3:
            id = 0x5F;
            break;
        }
        D_800D2DB4->counts[12] +=
                func_80076A10(id, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x50 + combo * 16, 0xD0 - index * 16);
    }
    /* The deathblow index counts down from the last combo. */
    i = 0;
    switch (match) {
    case 0:
        i++;
    case 1:
        i++;
    case 2:
        i++;
    case 3:
        i++;
    case 4:
        i++;
    case 5:
        i++;
    case 6:
        i++;
    case 7:
        i++;
    case 8:
        i++;
    case 9:
        i++;
    case 10:
        i++;
    case 11:
        i++;
    case 12:
        if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask0, D_800C31AC[D_800CCCE8.records[member].pilot.characterId][12 - i]) &&
            D_800C3EAC->slots[member].items[2] == 0) {
            if (D_800C3EAC->unk2E1[3] == 0) {
                D_800D2DB4->counts[12] +=
                func_80076A10(7, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x50 + combo * 16, 0xD0 - index * 16);
            } else {
                combo--;
            }
            shown = 1;
            index = func_80086028(member, index, combo, D_800C31AC[D_800CCCE8.records[member].pilot.characterId][12 - i], &D_800C3A70[index / 2], 0);
        }
        break;
    }
    if (D_800C3EAC->unk2E1[3] != 0 && shown) {
        D_800D2DB4->buffers[12] = D_800CCB04.buffer;
        D_800C3EAC->unk2DC = D_800C31AC[D_800CCCE8.records[member].pilot.characterId][12 - i] + 8;
        D_800D2DB4->buffers[11] = D_800CCB04.buffer;
        D_800D2D28->unkA8 = 1;
        func_800716D8();
        for (block = 0; block < 3; block++) {
            func_800320E8(D_800C3A70[block]);
        }
        return;
    }
    /* The second, longer deathblow (index 13-18, or 19 after combo 8). */
    i = 0;
    switch (match) {
    case 0:
        i++;
    case 1:
        i++;
    case 2:
        i++;
    case 3:
        i++;
    case 4:
        i++;
    case 5:
        i++;
    case 8:
        if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask0, D_800C31AC[D_800CCCE8.records[member].pilot.characterId][19 - i]) &&
            D_800C3EAC->slots[member].items[0] == 0 && D_800C3EAC->slots[member].items[2] == 0) {
            D_800D2DB4->counts[12] +=
                func_80076A10(8, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x54 + combo * 16, 0xD0 - index * 16);
            D_800D2DB4->counts[12] +=
                func_80076A10(7, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x64 + combo * 16, 0xD0 - index * 16);
            index = func_80086028(member, index, combo, D_800C31AC[D_800CCCE8.records[member].pilot.characterId][19 - i], &D_800C3A70[index / 2], 1);
        }
        break;
    case 6:
    case 7:
        break;
    }
    /* The third deathblow (index 20-22) after combos 0, 1 and 3. */
    i = 0;
    switch (match) {
    case 0:
        i++;
    case 1:
        i++;
    case 3:
        if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask0, D_800C31AC[D_800CCCE8.records[member].pilot.characterId][22 - i]) &&
            D_800C3EAC->slots[member].items[1] == 0 && D_800C3EAC->slots[member].items[2] == 0) {
            D_800D2DB4->counts[12] +=
                func_80076A10(9, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x58 + combo * 16, 0xD0 - index * 16);
            D_800D2DB4->counts[12] +=
                func_80076A10(7, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x68 + combo * 16, 0xD0 - index * 16);
            func_80086028(member, index, combo, D_800C31AC[D_800CCCE8.records[member].pilot.characterId][22 - i], &D_800C3A70[index / 2], 1);
        }
        break;
    }
    D_800D2DB4->buffers[12] = D_800CCB04.buffer;
    D_800D2DB4->buffers[11] = D_800CCB04.buffer;
    D_800D2D28->unkA8 = 1;
    func_800716D8();
    for (block = 0; block < 3; block++) {
        func_800320E8(D_800C3A70[block]);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800861D0);
#endif

/* Whether the member can use combo step `step` now: without a combo chain
 * (+0x2d6) always; otherwise its character must know the combo flag
 * (800c34cc) and the chain must allow another step. 800d2c34 is the gear
 * HUD's level byte of the battle work area (the original addresses it through
 * 800ccce8). */
s32 func_80086B88(s32 step, u8 member) {
    u8 index = step + (D_800C3EAC->unk2CC[0] + 1) * 3;
    s32 result = 1;

    if (D_800C3EAC->unk2D6 != 0) {
        if (D_800CCCE8.gearHud.level == 4) {
            index = step + 12;
        }
        if (!func_80089C6C(D_8006ECF8[D_800D2D24[member]].combos, D_800C34CC[index])) {
            result = 0;
        } else if (D_800C3EAC->unk2CC[0] != 0xFF && D_800CCCE8.gearHud.level != 4 &&
                   D_800CCCE8.gearHud.level < D_800C3EAC->unk2CC[0] + 1) {
            result = 0;
        }
    }
    return result;
}

/* Add entry `index` to lists 11 and 13 (the gear's combo chain display):
 * render the gear's text for combo step `step` into the shared image (two
 * entries per image cell) and place its quad after `column` + 1 steps, then
 * upload the step's fuel cost digits and place their quad. Returns the next
 * index. Nonmatching: the original reads the fuel cost through the draw
 * buffer index's address (800ccb34 + 0x60d8, the gear HUD of the battle
 * work area), so the draw state and the work area form one aggregate there;
 * the digit loop's registers differ too. */
#ifdef NON_MATCHING
s32 func_80086C88(u8 member, s32 index, s32 column, u8 step, u32 **pixels) {
    RECT rect;
    RECT digits[4];
    s32 cell;
    s32 odd;
    s32 width;
    s32 count;
    s32 i;

    count = 0;
    cell = index / 2;
    odd = index % 2;
    func_80076D58(&D_800D2DB4->list11[index * 2], odd, 3);
    width = func_80034EAC(func_800339C8(D_800CCCE8.records[member].pilot.gearId, D_800C34CC[step]), *pixels, 0x1B, odd);
    rect.x = cell * 30 + 0x3C0;
    rect.y = 0x1A;
    rect.w = 0x1E;
    rect.h = 0xD;
    LoadImage(&rect, *pixels);
    func_80076C78(&D_800D2DB4->list11[index * 2 + D_800CCB04.buffer], index * 4 + (column + 1) * 16 + 0x86,
                  0xC8 - index * 16, cell * 0x78, 0x1A, width);
    func_80076D58(&D_800D2DB4->list13[index * 2], 0, 3);
    func_8008AAA0(D_800CCCE8.gearHud.commands[step]);
    for (i = 0; i < 4; i++) {
        if (D_800C3CF4[i + 5] != 0xFF) {
            digits[count].x = index * 8 + i * 2 + 0x3DE;
            digits[count].y = 0;
            digits[count].w = 6;
            digits[count].h = 0xD;
            func_800769E8(&digits[count], D_800C3E5C[D_800C3CF4[i + 5]].pixels);
            count++;
        }
    }
    func_80076C78(&D_800D2DB4->list13[index * 2 + D_800CCB04.buffer], index * 4 + (column + 1) * 16 + 0xEA,
                  0xC8 - index * 16, index * 32 + 0x78, 0, count * 8);
    D_800D2DB4->counts[11]++;
    D_800D2DB4->counts[13]++;
    return index + 1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086C88);
#endif

/* Record gear combo step `step` for the member: unless the target fights in
 * a gear (record +0x15a bit 0x80) or the attack level is 4, the step's flag
 * becomes the combo step (+0x2dc) at once. Otherwise, when the attack level
 * allows the step (or +0x2e7 is set), add it to the combo history (+0x2cc,
 * length +0x2d6), show the history (list 12) and the deathblows it leads to
 * (80086c88, lists 11 and 13) and, with an armed chain (+0x2e4), take a
 * completed deathblow as the combo step. Returns whether a deathblow was
 * shown (0 with an armed chain), else 1. Nonmatching: the text block loops
 * address 800c3a70 through a pointer instead of an offset, and registers. */
#ifdef NON_MATCHING
u8 func_80086F98(u8 step, u8 member) {
    s32 index;
    s32 i;
    s32 block;
    s32 shown;
    u8 flag;
    u8 id;
    u8 next;

    flag = (step + 1) * 3;
    index = 0;
    shown = 0;
    if (!(D_800CCCE8.records[D_800C3EAC->slots[member].defaultTarget].flags15A & 0x80) && D_800CCCE8.gearHud.level != 4) {
        D_800C3EAC->unk2DC = D_800C34CC[step];
        return 1;
    }
    if ((D_800CCCE8.gearHud.level != 0 && D_800CCCE8.gearHud.level - 1 >= step) || D_800C3EAC->unk2E7 != 0) {
        for (block = 0; block < 3; block++) {
            D_800C3A70[block] = (u32 *)func_8008AC00(0x1E);
        }
        D_800C3EAC->unk2CC[D_800C3EAC->unk2D6] = step;
        D_800C3EAC->unk2D6++;
        if (D_800C3EAC->unk2E1[3] != 0) {
            D_800C3EAC->unk2CC[D_800C3EAC->unk2D6 - 1] = 0xFF;
            D_800C3EAC->unk2CC[D_800C3EAC->unk2D6 - 1] = step;
            flag = step + (D_800C3EAC->unk2CC[0] + 1) * 3;
        }
        D_800D2DB4->counts[12] = 0;
        D_800D2DB4->counts[11] = 0;
        D_800D2DB4->counts[13] = 0;
        for (i = 0; i < D_800C3EAC->unk2D6; i++) {
            if (D_800C3EAC->unk2CC[i] != 0xFF) {
                switch (D_800C3EAC->unk2CC[i]) {
                case 0:
                    id = 0x5E;
                    break;
                case 1:
                    id = 0x5F;
                    break;
                case 2:
                    id = 0x5D;
                    break;
                }
                D_800D2DB4->counts[12] +=
                    func_80076A10(id, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x80 + i * 16, 0xD0 - index * 16);
            }
        }
        if (D_800CCCE8.gearHud.level == 4) {
            flag = step + 12;
            if (step == 0xFF) {
                flag = 12;
            }
        }
        if (func_80089C6C(D_8006ECF8[D_800D2D24[member]].combos, D_800C34CC[flag])) {
            if (D_800C3EAC->unk2E1[3] == 0) {
                shown = 1;
                if (D_800C3EAC->slots[member].items[0] == 0) {
                    D_800D2DB4->counts[12] +=
                        func_80076A10(8, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x80 + i * 16, 0xD0 - index * 16);
                    index = func_80086C88(member, index, i - 1, flag, &D_800C3A70[index / 2]);
                }
            } else {
                shown = 1;
                index = func_80086C88(member, index, i - 1, flag, &D_800C3A70[index / 2]);
            }
        }
        if (D_800C3EAC->unk2E1[3] != 0 && shown) {
            if (D_800C3EAC->unk2CC[0] == 0xFF) {
                flag = step + 12;
            }
            D_800D2DB4->buffers[12] = D_800CCB04.buffer;
            D_800C3EAC->unk2DC = D_800C34CC[flag];
            D_800D2DB4->buffers[11] = D_800CCB04.buffer;
            D_800D2DB4->buffers[13] = D_800CCB04.buffer;
            D_800D2D28->unkA8 = 1;
            func_800716D8();
            for (block = 0; block < 3; block++) {
                func_800320E8(D_800C3A70[block]);
            }
            return 0;
        }
        D_800C3EAC->unk2DC = step;
        if (D_800CCCE8.gearHud.level != 4) {
            next = (step + 1) * 3 + 1;
        } else {
            next = 13;
        }
        if (func_80089C6C(D_8006ECF8[D_800D2D24[member]].combos, D_800C34CC[next]) &&
            D_800C3EAC->slots[member].items[1] == 0) {
            shown = 1;
            D_800D2DB4->counts[12] +=
                func_80076A10(9, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x84 + i * 16, 0xD0 - index * 16);
            index = func_80086C88(member, index, i, next, &D_800C3A70[index / 2]);
        }
        if (D_800CCCE8.gearHud.level != 4) {
            next = (step + 1) * 3 + 2;
        } else {
            next = 14;
        }
        if (func_80089C6C(D_8006ECF8[D_800D2D24[member]].combos, D_800C34CC[next]) &&
            D_800C3EAC->slots[member].items[2] == 0) {
            shown = 1;
            D_800D2DB4->counts[12] +=
                func_80076A10(7, &D_800D2DB4->list12[D_800D2DB4->counts[12] * 2], 0x88 + i * 16, 0xD0 - index * 16);
            func_80086C88(member, index, i, next, &D_800C3A70[index / 2]);
        }
        D_800D2DB4->buffers[12] = D_800CCB04.buffer;
        D_800D2DB4->buffers[11] = D_800CCB04.buffer;
        D_800D2DB4->buffers[13] = D_800CCB04.buffer;
        D_800D2D28->unkA8 = 1;
        func_800716D8();
        for (block = 0; block < 3; block++) {
            func_800320E8(D_800C3A70[block]);
        }
        return shown;
    }
    D_800C3EAC->unk2DC = step;
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086F98);
#endif

/* Plan the approach route from `actor` to `target`: the actor's position,
 * then the route points the formation lists between their groups. Returns
 * 1 when the actor's character is 4. (Nonmatching: the link address is
 * formed as formation + from * 0x40 first.) */
#ifdef NON_MATCHING
s32 func_800877E0(u8 actor, u8 target) {
    s32 result = 0;
    s32 i;

    for (i = 0; i < 9; i++) {
        D_800C48EC[i].x = 0xFFFF;
        D_800C48EC[i].z = 0xFFFF;
    }
    D_800C48EC[0].x = D_800C3EB4[actor].x;
    D_800C48EC[0].z = D_800C3EB4[actor].z;
    D_800C48EC[0].flag = 0;
    for (i = 1; i < 8; i++) {
        if (D_800D3364->links[D_800C3EB4[actor].group][D_800C3EB4[target].group].points[i - 1] == 0xFF) {
            break;
        }
        D_800C48EC[i].x =
            D_800D3364->areas[D_800D3364->links[D_800C3EB4[actor].group][D_800C3EB4[target].group].points[i - 1] & 7]
                .centre.x;
        D_800C48EC[i].z =
            D_800D3364->areas[D_800D3364->links[D_800C3EB4[actor].group][D_800C3EB4[target].group].points[i - 1] & 7]
                .centre.z;
        D_800C48EC[i].flag =
            D_800D3364->links[D_800C3EB4[actor].group][D_800C3EB4[target].group].points[i - 1] & 0x80;
    }
    if (D_800D2D24[actor] == 4) {
        result = 1;
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800877E0);
#endif

/* Reset every event to type 0xff for `actor` against `target`'s bit. */
void func_800879A8(u8 actor, u8 target) {
    s32 i;

    for (i = 0; i < 32; i++) {
        D_800C3FE8[i].type = 0xFF;
        D_800C3FE8[i].actor = actor;
        D_800C3FE8[i].targetMask = func_80089C08(target);
    }
}

/* Enter the attack page: AP text, events against the default target, and
 * the member's attack model. */
void func_80087A38(u8 member) {
    s32 route;

    func_80085D34();
    func_800879A8(member, D_800C3EAC->slots[member].defaultTarget);
    D_800D366C = 0;
    route = func_800877E0(member, D_800C3EAC->slots[member].defaultTarget);
    func_800B89FC(route, member, D_800C3EAC->slots[member].defaultTarget, func_80080AE4(member));
    D_800D366C = 1;
    D_800C3E18 = 0;
}

/* Execute the member's attack step (paying `cost` AP, 1-3): once per
 * turn move into the target's group (unless the character is 4; flagged
 * slots use 800881b8), reset the events, advance the combo step through the
 * step table (a step from 8 is a deathblow the character must know, else
 * step 7), queue an event 0xf3 when the target reacts while down, commit
 * the step against the target, queue its event, let the enemy remember the
 * attack and run its reaction script, and apply the results. Returns
 * whether the combo completed a known deathblow or the reaction ran action
 * 0x62. */
#ifdef NON_MATCHING
u8 func_80087AF0(u8 member, u8 cost) {
    u8 queue;
    u8 reacted = 0;

    if (D_800C3E18 == 0) {
        if (D_800D32A0[member].unk1 == 0) {
            if (D_800D2D24[member] != 4) {
                func_80087EDC(member, D_800C3EAC->slots[member].defaultTarget);
            }
        } else {
            func_800881B8(member, D_800C3EAC->slots[member].defaultTarget);
        }
        D_800C3E18 = 1;
    }
    func_80085388();
    if (D_800D32A0[member].unk1 == 0) {
        if (D_800C3EAC->unk2DC < 8) {
            D_800C3EAC->unk2DC = D_800C34B3[D_800C3EAC->unk2DC][cost];
        } else if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask0, D_800C3EAC->unk2DC - 8)) {
            reacted = 1;
        } else {
            D_800C3EAC->unk2DC = 7;
        }
    } else {
        D_800C3EAC->unk2DC++;
    }
    if (D_800CCCE8.records[D_800C3EAC->slots[member].defaultTarget].pilot.flags34 & 0x800) {
        D_800C3FE8[D_800C3EAC->eventCount].actor = member;
        D_800C3FE8[D_800C3EAC->eventCount].type = 0xF3;
        D_800C3FE8[D_800C3EAC->eventCount].parameter = func_80089C08(D_800C3EAC->slots[member].defaultTarget);
        D_800C3EAC->eventCount++;
    }
    func_80085CCC(member, func_80089C08(D_800C3EAC->slots[member].defaultTarget), D_800C3EAC->unk2DC - 1);
    queue = D_800C3EAC->eventCount;
    D_800C3FE8[D_800C3EAC->eventCount].actor = member;
    D_800C3FE8[D_800C3EAC->eventCount].targetMask = D_800D2C94.targets;
    D_800C3FE8[D_800C3EAC->eventCount].type = D_800D2C94.animation;
    D_800C3EAC->eventCount++;
    func_80079840(member, D_800C3EAC->slots[member].defaultTarget);
    if (D_800C3EAC->slots[member].defaultTarget >= 3 && (u8)func_80079AB0(D_800C3EAC->slots[member].defaultTarget)) {
        reacted = 1;
    }
    func_80085C88(queue);
    return reacted;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80087AF0);
#endif

/* Move `actor` into `target`'s formation group when it is another group
 * with room (under four members): leave the old group, take the first free
 * member place and stand at that place of the group's area. (Nonmatching:
 * the target's record offset and register allocation.) */
#ifdef NON_MATCHING
void func_80087EDC(u8 actor, u8 target) {
    u8 base;
    s8 member;

    if (D_800C3EB4[actor].group != D_800C3EB4[target].group) {
        base = target < 3 ? (actor >= 3) * 8 : 0;
        if (D_800D301C[D_800C3EB4[target].group + base].count < 4) {
            func_800883AC(actor);
            D_800D301C[D_800C3EB4[target].group + base].count++;
            for (member = 0; member < 4; member++) {
                if (func_80089C9C(D_800D301C[D_800C3EB4[target].group + base].members, member) == 0) {
                    break;
                }
            }
            D_800C3EB4[actor].member = member;
            D_800C3EB4[actor].group = D_800C3EB4[target].group;
            D_800D301C[D_800C3EB4[actor].group + base].members |= func_80089C08(D_800C3EB4[actor].member);
            if (actor < 3) {
                D_800C3EB4[actor].x = D_800D3364->areas[D_800C3EB4[actor].group].party[D_800C3EB4[actor].member].x;
                D_800C3EB4[actor].z = D_800D3364->areas[D_800C3EB4[actor].group].party[D_800C3EB4[actor].member].z;
            } else {
                D_800C3EB4[actor].x = D_800D3364->areas[D_800C3EB4[actor].group].enemies[D_800C3EB4[actor].member].x;
                D_800C3EB4[actor].z = D_800D3364->areas[D_800C3EB4[actor].group].enemies[D_800C3EB4[actor].member].z;
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80087EDC);
#endif

/* Move `actor` alone into `target`'s formation group when that group is
 * another one and empty (entries from 0x10, or 0x18 for an enemy joining a
 * party member): it becomes the only member, at the group's position.
 * (Nonmatching: the base selection branches and store/load order.) */
#ifdef NON_MATCHING
void func_800881B8(u8 actor, u8 target) {
    u8 base;

    if (D_800C3EB4[actor].group != D_800C3EB4[target].group) {
        if (target < 3 && actor >= 3) {
            base = 0x18;
        } else {
            base = 0x10;
        }
        if (D_800D301C[D_800C3EB4[target].group + base].count == 0) {
            func_800883AC(actor);
            D_800C3EB4[actor].member = 0;
            D_800C3EB4[actor].group = D_800C3EB4[target].group;
            D_800D301C[D_800C3EB4[actor].group + base].count = 1;
            D_800D301C[D_800C3EB4[actor].group + base].members = 1;
            if (actor < 3) {
                D_800C3EB4[actor].x = D_800D3364->positions[D_800C3EB4[actor].group].x;
                D_800C3EB4[actor].z = D_800D3364->positions[D_800C3EB4[actor].group].z;
            } else {
                D_800C3EB4[actor].x = D_800D3364->positions[D_800C3EB4[actor].group].enemyX;
                D_800C3EB4[actor].z = D_800D3364->positions[D_800C3EB4[actor].group].enemyZ;
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800881B8);
#endif

/* Drop a slot from its formation group (enemy entries from 8, flagged slots
 * add 0x10). */
void func_800883AC(u8 slot) {
    u8 base = (slot >= 3) * 8;

    if (D_800D32A0[slot].unk1 != 0) {
        base |= 0x10;
    }
    D_800D301C[D_800C3EB4[slot].group + base].count--;
    D_800D301C[D_800C3EB4[slot].group + base].members &= func_80089C48(D_800C3EB4[slot].member);
}

/* Give slot a formation group of its own: keep its group when that is empty,
 * else take the first empty one; it becomes the group's only member and is
 * placed at the group's position. */
void func_80088490(slot)
u8 slot;
{
    u8 group;
    s32 i;

    if (D_800D301C[D_800C3EB4[slot].group + 16].count == 0) {
        group = D_800C3EB4[slot].group;
    } else {
        for (i = 0; i < 8; i++) {
            if (D_800D301C[16 + i].count == 0) {
                group = i;
                break;
            }
        }
    }
    D_800C3EB4[slot].group = group;
    D_800C3EB4[slot].member = 0;
    D_800D301C[D_800C3EB4[slot].group + 16].members = 1;
    D_800D301C[D_800C3EB4[slot].group + 16].count = 1;
    D_800C3EB4[slot].x = D_800D3364->positions[D_800C3EB4[slot].group].x;
    D_800C3EB4[slot].z = D_800D3364->positions[D_800C3EB4[slot].group].z;
}

/* The member count of the slot's group among the flagged enemy groups. */
u8 func_800885D0(u8 slot) {
    return D_800D301C[D_800C3EB4[slot].group + 0x18].count;
}

/* Build the glyph lists at +0x1720 (glyphs 800c33b0[0..1]), +0 (glyph 0xa8)
 * and +0x2530 (800c33b0[2..3]) at (0xa0, 0x64) and initialise the current
 * buffer's quads. The first two keep their count and buffer at +0x5d74 /
 * +0x5d83 and +0x5d70 / +0x5d92. */
void func_8008860C(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        D_800D2DB4->extraCounts[4] +=
            func_80076A10(D_800C33B0[i], &D_800D2DB4->extra4[D_800D2DB4->extraCounts[4] * 2], 0xA0, 0x64);
    }
    D_800D2DB4->extraBuffer4 = D_800CCB04.buffer;
    D_800D2DB4->extraCounts[0] = func_80076A10(0xA8, &D_800D2DB4->extra0[D_800D2DB4->extraCounts[0] * 2], 0xA0, 0x64);
    D_800D2DB4->extraBuffers[0] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->extraCounts[4]; i++) {
        func_80076B68(&D_800D2DB4->extra4[i * 2 + D_800D2DB4->extraBuffer4]);
    }
    for (i = 0; i < D_800D2DB4->extraCounts[0]; i++) {
        func_80076B68(&D_800D2DB4->extra0[i * 2 + D_800D2DB4->extraBuffers[0]]);
    }
    for (i = 2; i < 4; i++) {
        D_800D2DB4->counts[9] +=
            func_80076A10(D_800C33B0[i], &D_800D2DB4->list9[D_800D2DB4->counts[9] * 2], 0xA0, 0x64);
    }
    D_800D2DB4->buffers[9] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[9]; i++) {
        func_80076BF0(&D_800D2DB4->list9[i * 2 + D_800D2DB4->buffers[9]]);
    }
}

/* Set up a stepped line from (x0, y0) to (x1, y1): directions, the 8.8 steps
 * of the minor axis and a random speed 1..8. */
void func_8008887C(s32 x0, s32 y0, s32 x1, s32 y1) {
    s32 dx;
    s32 dy;

    D_800C3A7C = x0;
    D_800C3A80 = y0;
    D_800C3A84 = x1;
    D_800C3A88 = y1;
    if (x1 != x0 && y1 != y0) {
        if (x1 < x0) {
            D_800C3A94 = 1;
            dx = x0 - x1;
        } else {
            dx = x1 - x0;
            D_800C3A94 = 0;
        }
        if (y1 < y0) {
            D_800C3A98 = 1;
            dy = y0 - y1;
        } else {
            dy = y1 - y0;
            D_800C3A98 = 0;
        }
        if (dx >= dy) {
            D_800C3A8C = 0x100;
            D_800C3A90 = (dy << 8) / dx;
        } else {
            D_800C3A90 = 0x100;
            D_800C3A8C = (dx << 8) / dy;
        }
        D_800C2080 = 0;
        D_800C2084 = 0;
        D_800C3A9C = func_8001BD40(1, 8);
        D_800C207C = 0;
    }
}

/* Advance the stepped line by its speed and flag its end (800c207c) once
 * the major axis passes the end point. */
void func_80088990(void) {
    s32 i;

    for (i = 0; i < D_800C3A9C; i++) {
        if (D_800C3A94) {
            D_800C2080 -= D_800C3A8C;
        } else {
            D_800C2080 += D_800C3A8C;
        }
        if (D_800C3A98) {
            D_800C2084 -= D_800C3A90;
        } else {
            D_800C2084 += D_800C3A90;
        }
    }
    if (D_800C3A8C == 0x100) {
        if (D_800C3A94) {
            if (D_800C2080 / 256 + D_800C3A7C < D_800C3A84) {
                D_800C207C = 1;
            }
        } else if (D_800C2080 / 256 + D_800C3A7C > D_800C3A84) {
            D_800C207C = 1;
        }
    } else if (D_800C3A98) {
        if (D_800C2084 / 256 + D_800C3A80 < D_800C3A88) {
            D_800C207C = 1;
        }
    } else if (D_800C2084 / 256 + D_800C3A80 > D_800C3A88) {
        D_800C207C = 1;
    }
}

/* Draw the stepped line effect while UI +0xad is set: once a line ends,
 * start another towards a random end point (a 4 in 100 chance per frame);
 * otherwise advance it. Place its glyphs at the current point (lists
 * extra1-extra3 and +0x32a0) and, while the line runs, twenty random
 * glyphs (+0x46a0) in two rows. */
void func_80088B80(void) {
    s32 i;
    s32 j;
    s32 n;

    if (D_800D2D28->unkAD != 0) {
        if (D_800C207C != 0 && func_8001BD40(0, 99) >= 0x60) {
            func_8008887C(D_800D2DB4->lineX, D_800D2DB4->lineY, D_800C2054[0][func_8001BD40(0, 4)],
                          D_800C2054[1][func_8001BD40(0, 4)]);
        }
        if (D_800C207C == 0) {
            func_80088990();
            D_800D2DB4->lineX = D_800C3A7C + D_800C2080 / 256;
            D_800D2DB4->lineY = D_800C3A80 + D_800C2084 / 256;
        }
        D_800D2DB4->extraCounts[2] = func_80076A10(0xB9, D_800D2DB4->extra2, D_800D2DB4->lineX, D_800D2DB4->lineY);
        D_800D2DB4->extraBuffers[2] = D_800CCB04.buffer;
        D_800D2DB4->extraCounts[1] = func_80076A10((D_800D2DB4->lineX & 0xF) + 0xA9, D_800D2DB4->extra1, 0xA0, 0x64);
        D_800D2DB4->extraBuffers[1] = D_800CCB04.buffer;
        D_800D2DB4->extraCounts[3] = func_80076A10(0x82, D_800D2DB4->extra3, D_800D2DB4->lineX, D_800D2DB4->lineY);
        D_800D2DB4->extraBuffers[3] = D_800CCB04.buffer;
        for (i = 0; i < D_800D2DB4->extraCounts[2]; i++) {
            func_80076B68(&D_800D2DB4->extra2[i * 2 + D_800D2DB4->extraBuffers[2]]);
        }
        for (i = 0; i < D_800D2DB4->extraCounts[3]; i++) {
            func_80076B68(&D_800D2DB4->extra3[i * 2 + D_800D2DB4->extraBuffers[3]]);
        }
        for (i = 0; i < D_800D2DB4->extraCounts[1]; i++) {
            func_80076B68(&D_800D2DB4->extra1[i * 2 + D_800D2DB4->extraBuffers[1]]);
        }
        D_800D2DB4->count32A0 = func_80076A10((D_800D2DB4->lineY & 0xF) + 0xC9, D_800D2DB4->unk32A0,
                                              D_800D2DB4->lineX, D_800D2DB4->lineY);
        D_800D2DB4->buffer32A0 = D_800CCB04.buffer;
        for (i = 0; i < D_800D2DB4->count32A0; i++) {
            func_80076BF0(&D_800D2DB4->unk32A0[i * 2 + D_800D2DB4->buffer32A0]);
        }
        if (D_800C207C == 0) {
            for (i = 0; i < 2; i++) {
                for (j = 0; j < 10; j++) {
                    n = i * 10 + j;
                    func_80076A10(func_8001BD40(0, 9) + 0xBA, &D_800D2DB4->unk46A0[n * 2], 0x82 + j * 6, 0xA + i * 0xBD);
                    func_80076B68(&D_800D2DB4->unk46A0[n * 2 + D_800CCB04.buffer]);
                }
            }
            D_800D2DB4->buffer46A0 = D_800CCB04.buffer;
        }
    }
}

/* Build the glyphs of the flags set in 800d2c30 (up to five, 10 pixels
 * apart from y 0x6e) into the +0x4ce0 primitives. */
void func_80089038(void) {
    s32 i;
    s32 y; /* 16.16 */

    i = 0;
    y = 0x6E << 16;
    D_800D2DB4->count4CE0 = 0;
    for (; i < 5; i++) {
        if (func_80089C6C(D_800D2C30, i)) {
            D_800D2DB4->count4CE0 += func_80076A10(i + 0xC4, &D_800D2DB4->unk4CE0[D_800D2DB4->count4CE0 * 2], 0xE0, y >> 16);
            y += 10 << 16;
        }
    }
    D_800D2DB4->buffer4CE0 = D_800CCB04.buffer;
    D_800D2DB4->blink = 0;
}

/* Build glyph 0xa0 (0xa1 with 800d2c38) into the +0x3ac0 primitives and
 * initialise the current buffer's quads. */
void func_80089110(void) {
    s32 id = 0xA0;
    s32 i;

    if (D_800D2C38 != 0) {
        id = 0xA1;
    }
    D_800D2DB4->counts[0] = func_80076A10(id, D_800D2DB4->list0, 0xA0, 0x64);
    D_800D2DB4->buffers[0] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[0]; i++) {
        func_80076B68(&D_800D2DB4->list0[i * 2 + D_800D2DB4->buffers[0]]);
    }
}

/* Build the two glyphs of the escape/limit page (800d2c34 - 0x5d and
 * - 0x25) into lists 2 and 10 and initialise their quads. */
#ifdef NON_MATCHING
void func_800891E4(void) {
    s32 i;
    u8 second = D_800D2C34 - 0x25;

    D_800D2DB4->counts[2] = func_80076A10((u8)(D_800D2C34 - 0x5D), D_800D2DB4->list2, 0xA0, 0x64);
    D_800D2DB4->buffers[2] = D_800CCB04.buffer;
    D_800D2DB4->counts[10] = func_80076A10(second, D_800D2DB4->list10, 0xA0, 0x64);
    D_800D2DB4->buffers[10] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[2]; i++) {
        func_80076B68(&D_800D2DB4->list2[i * 2 + D_800D2DB4->buffers[2]]);
    }
    for (i = 0; i < D_800D2DB4->counts[10]; i++) {
        func_80076BF0(&D_800D2DB4->list10[i * 2 + D_800D2DB4->buffers[10]]);
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800891E4);
#endif

/* Build the five-digit value 800d2c2a as glyphs into list 3 at (0x11a, 0x46)
 * and initialise its quads. */
void func_80089348(void) {
    s32 i;
    s32 x; /* 16.16 */
    u8 digit;

    i = 0;
    x = 0x11A << 16;
    func_8008AAA0(D_800D2C2A);
    for (; i < 5; i++) {
        digit = D_800C3CF4[i + 4];
        if (digit != 0xFF) {
            D_800D2DB4->counts[3] +=
                func_80076A10(digit + 0x92, &D_800D2DB4->list3[D_800D2DB4->counts[3] * 2], x >> 16, 0x46);
            x += 6 << 16;
        }
    }
    D_800D2DB4->buffers[3] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[3]; i++) {
        func_80076B68(&D_800D2DB4->list3[i * 2 + D_800D2DB4->buffers[3]]);
    }
}

/* Build the two-digit value 800d2c3a and a '%' glyph into list 4 at
 * (0x11a, 0x4e), or glyph 0xa2 from page 4 on, and initialise its quads. */
void func_8008946C(void) {
    s32 i;
    s32 x; /* 16.16 */
    s32 digits;
    u8 digit;

    if (D_800D2C34 < 4) {
        func_8008AAA0(D_800D2C3A);
        i = 0;
        digits = 0;
        x = 0x11A << 16;
        for (; i < 2; i++) {
            digit = D_800C3CF4[i + 7];
            if (digit != 0xFF) {
                    D_800D2DB4->counts[4] +=
                    func_80076A10(digit + 0x92, &D_800D2DB4->list4[D_800D2DB4->counts[4] * 2], x >> 16, 0x4E);
                digits++;
                x += 6 << 16;
            }
        }
        D_800D2DB4->counts[4] +=
            func_80076A10(0x9D, &D_800D2DB4->list4[D_800D2DB4->counts[4] * 2], digits * 6 + 0x11A, 0x4E);
    } else {
        D_800D2DB4->counts[4] = func_80076A10(0xA2, D_800D2DB4->list4, 0x11A, 0x4E);
    }
    D_800D2DB4->buffers[4] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[4]; i++) {
        func_80076B68(&D_800D2DB4->list4[i * 2 + D_800D2DB4->buffers[4]]);
    }
}

/* Build the three-digit value 800d2c36 and a '%' glyph into list 5 at
 * (0x11a, 0x56) and initialise its quads. */
void func_8008963C(void) {
    s32 i;
    s32 x; /* 16.16 */
    s32 digits;
    u8 digit;

    i = 0;
    digits = 0;
    x = 0x11A << 16;
    func_8008AAA0(D_800D2C36);
    for (; i < 3; i++) {
        digit = D_800C3CF4[i + 6];
        if (digit != 0xFF) {
            D_800D2DB4->counts[5] +=
                func_80076A10(digit + 0x92, &D_800D2DB4->list5[D_800D2DB4->counts[5] * 2], x >> 16, 0x56);
            digits++;
            x += 6 << 16;
        }
    }
    D_800D2DB4->counts[5] +=
        func_80076A10(0x9D, &D_800D2DB4->list5[D_800D2DB4->counts[5] * 2], digits * 6 + 0x11A, 0x56);
    D_800D2DB4->buffers[5] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[5]; i++) {
        func_80076B68(&D_800D2DB4->list5[i * 2 + D_800D2DB4->buffers[5]]);
    }
}

/* Build the two-digit value 800d2c35 as glyphs into list 6 at (0x11a,
 * 0x5e) and initialise its quads. */
void func_800897CC(void) {
    s32 i;
    s32 x; /* 16.16 */
    u8 digit;

    i = 0;
    x = 0x11A << 16;
    func_8008AAA0(D_800D2C35);
    for (; i < 2; i++) {
        digit = D_800C3CF4[i + 7];
        if (digit != 0xFF) {
            D_800D2DB4->counts[6] +=
                func_80076A10(digit + 0x92, &D_800D2DB4->list6[D_800D2DB4->counts[6] * 2], x >> 16, 0x5E);
            x += 6 << 16;
        }
    }
    D_800D2DB4->buffers[6] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[6]; i++) {
        func_80076B68(&D_800D2DB4->list6[i * 2 + D_800D2DB4->buffers[6]]);
    }
}

/* Build the member's gear fuel as glyphs at y 0xcc: the fuel's last four
 * digits into list 7 (from x 0x20, blanks keep their place) and the maximum
 * fuel's into list 8 (from x 0x48, packed), then the separator glyph 0x9c
 * at x 0x41. */
void func_800898F0(u8 member) {
    s32 i;
    s32 x; /* 16.16 */
    u8 digit;

    i = 0;
    D_800D2DB4->counts[7] = 0;
    x = 0x20 << 16;
    func_8008AAA0(D_800CCCE8.records[member].gear.fuel);
    for (; i < 4; i++) {
        digit = D_800C3CF4[i + 5];
        if (digit != 0xFF) {
            D_800D2DB4->counts[7] +=
                func_80076A10(digit + 0x92, &D_800D2DB4->list7[D_800D2DB4->counts[7] * 2], x >> 16, 0xCC);
        }
        x += 8 << 16;
    }
    i = 0;
    D_800D2DB4->buffers[7] = D_800CCB04.buffer;
    D_800D2DB4->counts[8] = 0;
    x = 0x48 << 16;
    func_8008AAA0(D_800CCCE8.records[member].gear.maxFuel);
    for (; i < 4; i++) {
        digit = D_800C3CF4[i + 5];
        if (digit != 0xFF) {
            D_800D2DB4->counts[8] +=
                func_80076A10(digit + 0x92, &D_800D2DB4->list8[D_800D2DB4->counts[8] * 2], x >> 16, 0xCC);
            x += 8 << 16;
        }
    }
    D_800D2DB4->counts[8] += func_80076A10(0x9C, &D_800D2DB4->list8[D_800D2DB4->counts[8] * 2], 0x41, 0xCC);
    D_800D2DB4->buffers[8] = D_800CCB04.buffer;
}

/* Run the eight 8008860c..800897cc steps (the member is unused). */
void func_80089AF8(u8 member) {
    func_8008860C();
    func_80089038();
    func_80089110();
    func_800891E4();
    func_80089348();
    func_8008946C();
    func_8008963C();
    func_800897CC();
}

/* A random value in low..high (0xffff for low 0xffff, 0 for high 0). */
#ifdef NON_MATCHING
u16 func_80089B50(u16 low, u16 high) {
    s32 span;

    if (low == 0xFFFF) {
        return 0xFFFF;
    }
    if (high == 0) {
        return 0;
    }
    span = high - low;
    if (low == high) {
        return low;
    }
    if (span >= 0xFFFF) {
        return rand();
    }
    return low + (u16)rand() % (span + 1);
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089B50);
#endif

/* The mask bit `bit`. */
u16 func_80089BEC(u8 bit) {
    return D_800C3468[bit];
}

/* The mask bit of `slot`. */
u16 func_80089C08(u8 slot) {
    return D_800C3448[slot];
}

/* Every mask bit but `bit`. */
u16 func_80089C24(u8 bit) {
    return ~D_800C3468[bit];
}

/* Every slot bit but `slot`'s. */
u16 func_80089C48(u8 slot) {
    return ~D_800C3448[slot];
}

/* Bit `bit` of `mask`; 0 for bits past 15. */
u16 func_80089C6C(u16 mask, u8 bit) {
    u16 result;

    if (bit < 16) {
        result = D_800C3468[bit] & mask;
    } else {
        result = 0;
    }
    return result;
}

/* The bit of `slot` in `mask`; 0 for slots past 15. */
u16 func_80089C9C(u16 mask, u8 slot) {
    u16 result;

    if (slot < 16) {
        result = D_800C3448[slot] & mask;
    } else {
        result = 0;
    }
    return result;
}

/* Read the battle input into 800d3014: wait for a controller as 8008a3ec
 * does, then dequeue pad entries until one matters. Directions (0-3,
 * remembered in 800c3e28) and the face buttons (4-7) play their sounds;
 * with the debug flag, select refills the party's AP and button 2 opens the
 * debug console; start (0x800, while 800ccc58) pauses or resumes, and while
 * paused holding both 4 and 8 on a debug build ends the battle. A finished
 * battle or event returns 0xff. Loops while paused. Nonmatching: the
 * original keeps 800c48ea's address in a register. */
#ifdef NON_MATCHING
void func_80089CCC(s32 mode) {
    u8 code = 8;
    u8 waiting = 1;
    u8 paused = 0;
    s32 vsyncs;

    do {
        if (func_80035734(0) == 0) {
            if (paused == 0) {
                func_8001FAB4(0x88, 0x64);
                func_8001FAB4(0x88, 0x144);
                paused++;
                func_80037EE4();
                vsyncs = D_80059488;
            }
        } else {
            waiting = 0;
            if (paused) {
                func_80037E8C();
                D_80059488 = vsyncs;
            }
        }
    } while (waiting);
    do {
        if (func_80036410()) {
            func_80035DB0();
        } else {
            while (func_80035CDC()) {
                if (D_800C48EA != 0 || D_800C3EAC->eventsDone != 0) {
                    code = 0xFF;
                    break;
                }
                if (D_800C3444 != 0) {
                    if (*D_8005917C != -1 && (D_800594A4 & 4) && (D_800594A4 & 8)) {
                        D_800C48EA = 1;
                        goto resume;
                    }
                } else if (D_800594A4 & 0x2000) {
                    func_8008AA74(0x4C);
                    code = 0;
                    D_800C3E28[0] = D_800C3E28[1];
                    D_800C3E28[1] = code;
                    break;
                } else if (D_800594A4 & 0x4000) {
                    func_8008AA74(0x4C);
                    code = 1;
                    D_800C3E28[0] = D_800C3E28[1];
                    D_800C3E28[1] = code;
                    break;
                } else if (D_800594A4 & 0x8000) {
                    func_8008AA74(0x4C);
                    code = 2;
                    D_800C3E28[0] = D_800C3E28[1];
                    D_800C3E28[1] = code;
                    break;
                } else if (D_800594A4 & 0x1000) {
                    func_8008AA74(0x4C);
                    code = 3;
                    D_800C3E28[0] = D_800C3E28[1];
                    D_800C3E28[1] = code;
                    break;
                } else if (D_8005948C & 0x20) {
                    code = 4;
                    func_8008AA74(0x4D);
                    break;
                } else if (D_8005948C & 0x40) {
                    code = 5;
                    func_8008AA74(0x4E);
                    break;
                } else if (D_8005948C & 0x80) {
                    code = 6;
                    func_8008AA74(0x4D);
                    break;
                } else if (D_8005948C & 0x10) {
                    code = 7;
                    func_8008AA74(0x4D);
                    break;
                } else if (D_8005948C & 1) {
                    code = 0xC;
                    if (*D_8005917C != -1) {
                        D_800D32A0[0].unk0 = 28;
                        D_800D32A0[1].unk0 = 28;
                        D_800D32A0[2].unk0 = 28;
                        D_800CCCE8.records[0].field148 = 4;
                        D_800CCCE8.records[1].field148 = 4;
                        D_800CCCE8.records[2].field148 = 4;
                        D_800CCCE8.records[0].statusTimers[6] = 0xFF;
                        D_800CCCE8.records[1].statusTimers[6] = 0xFF;
                        D_800CCCE8.records[2].statusTimers[6] = 0xFF;
                    }
                    break;
                } else if (D_8005948C & 2) {
                    if (*D_8005917C != -1) {
                        code = 0xB;
                        if (++D_8005959C >= 5) {
                            D_8005959C = 0;
                        }
                        if (D_800C3AA0 == 0) {
                            func_8003747C(0x80200000);
                            func_800374E8(0x10, 0x10, 0x140, 0x100, 0x3E8, 0, 0x340, 0, 0x340, 0x20, 0);
                            D_800C3AA0++;
                        }
                    }
                    break;
                } else if (D_8005948C & 0x100) {
                    code = 0xD;
                    break;
                }
                if (D_8005948C & 0x800) {
                    code = 0xE;
                    if (D_800CCC58 != 0) {
                        if (D_800C3444 == 0) {
                            func_8001FAB4(0x88, 0x64);
                            func_8001FAB4(0x88, 0x144);
                            func_80037EE4();
                            vsyncs = D_80059488;
                            D_800C3444 = 1;
                        } else {
                        resume:
                            func_80037E8C();
                            D_80059488 = vsyncs;
                            D_800C3444 = 0;
                        }
                    }
                    break;
                }
            }
        }
    } while (D_800C3444 != 0);
    D_800D3014 = code;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089CCC);
#endif

/* Every other frame upload the next step of the four cycling CLUT strips. */
void func_8008A144(void) {
    D_800D2D28->unkAA++;
    if (D_800D2D28->unkAA & 1) {
        LoadImage(&D_800C3EA4->unk8950[0], &D_800C3EA4->unk8970[0][D_800D2D28->unk30]);
        LoadImage(&D_800C3EA4->unk8950[1], &D_800C3EA4->unk8970[1][D_800D2D28->unk30]);
        LoadImage(&D_800C3EA4->unk8950[2], &D_800C3EA4->unk8970[2][D_800D2D28->unk30]);
        LoadImage(&D_800C3EA4->unk8950[3], &D_800C3EA4->unk8970[3][D_800D2D28->unk30]);
        D_800D2D28->unk30 += 4;
        if (D_800D2D28->unk30 >= 0xC7) {
            D_800D2D28->unk30 = 0;
        }
    }
}

/* Result-screen tick of end state 1: the ATB while 800ccc58, input for the
 * member, counters, the pulsing shade, the scroll by 800d39d4 and the CLUT
 * cycle. */
void func_8008A274(u8 member) {
    if (D_800CCC58 != 0) {
        func_8007171C();
    }
    if (member == 0) {
        func_80089CCC(0);
    }
    D_800D2D28->unkA9 += 6;
    D_800D2D28->unkAB++;
    if (D_800C3EA4->unk6415 != 0) {
        if (D_800C3EA4->unk6416 == 0) {
            D_800C3EA4->unk6410 += 4;
            if (D_800C3EA4->unk6410 > 0x80) {
                D_800C3EA4->unk6410 = 0x7C;
                D_800C3EA4->unk6416 = 1;
            }
        } else {
            D_800C3EA4->unk6410 -= 4;
            if (D_800C3EA4->unk6410 < 0) {
                D_800C3EA4->unk6410 = 4;
                D_800C3EA4->unk6416 = 0;
            }
        }
    }
    switch (D_800D39D4) {
    case 1:
    case 3:
        D_800D3288++;
        break;
    case 2:
    case 4:
        D_800D3288--;
        break;
    }
    func_8008A144();
}

/* Wait for a controller (pausing the sound and the vsync count while there
 * is none), run a result-screen tick, count down the active 800d3278 entry
 * timers, then read the input: an overflowed queue is reset; otherwise
 * entries are dequeued until one matters. Confirm (0x20) sets input code 4;
 * start (0x800, while 800ccc58) pauses or resumes the battle, and while
 * paused holding both 4 and 8 with a disc ends it (outcome 1). Loops while
 * paused. */
void func_8008A3EC(u8 member) {
    u8 waiting = 1;
    u8 paused = 0;
    s32 vsyncs;
    s32 i;

    do {
        if (func_80035734(0) == 0) {
            if (paused == 0) {
                func_8001FAB4(0x88, 0x64);
                func_8001FAB4(0x88, 0x144);
                paused++;
                func_80037EE4();
                vsyncs = D_80059488;
            }
        } else {
            waiting = 0;
            if (paused) {
                func_80037E8C();
                D_80059488 = vsyncs;
            }
        }
    } while (waiting);
    func_8008A144();
    if (D_800D2D28->unkCA != 0) {
        for (i = 0; i < 16; i++) {
            if (D_800D3278->entries[i].unk28 != 0) {
                if (--D_800D3278->entries[i].unk26 < 0) {
                    D_800D3278->entries[i].unk26 = 0;
                }
            }
        }
    }
    do {
        if (D_800D2D28->unkCC[3] == 0) {
            D_800D3014 = 0xFF;
        }
        if (func_80036410()) {
            func_80035DB0();
        } else {
            while (func_80035CDC()) {
                if (D_800C3444 != 0) {
                    if (*D_8005917C != -1 && (D_800594A4 & 4) && (D_800594A4 & 8)) {
                        D_800C48EA = 1;
                        goto resume;
                    }
                } else if (D_8005948C & 0x20) {
                    D_800D3014 = 4;
                    break;
                }
                if (D_8005948C & 0x800) {
                    if (D_800CCC58 != 0) {
                        if (D_800C3444 == 0) {
                            func_80037EE4();
                            func_8001FAB4(0x88, 0x64);
                            func_8001FAB4(0x88, 0x144);
                            vsyncs = D_80059488;
                            D_800C3444 = 1;
                        } else {
                        resume:
                            func_80037E8C();
                            D_80059488 = vsyncs;
                            D_800C3444 = 0;
                        }
                    }
                    break;
                }
            }
        }
    } while (D_800C3444 != 0);
}

/* Result screen input: wait for a controller as 8008a3ec does, read the
 * input (confirm sets input code 4, start pauses or resumes) while paused,
 * then count each member's two result values one step; while any is still
 * counting redraw the panels (801df270, 801df4c0), else stop counting.
 * (Nonmatching: the original addresses the work area's counters from an
 * aggregate that starts at 0x800c3eb0, so the stores' base differs.) */
#ifdef NON_MATCHING
void func_8008A684(u8 member) {
    u8 done = 1;
    u8 waiting = 1;
    u8 paused = 0;
    s32 vsyncs;
    s32 i;

    do {
        if (func_80035734(0) == 0) {
            if (paused == 0) {
                func_8001FAB4(0x88, 0x64);
                func_8001FAB4(0x88, 0x144);
                paused++;
                func_80037EE4();
                vsyncs = D_80059488;
            }
        } else {
            waiting = 0;
            if (paused) {
                func_80037E8C();
                D_80059488 = vsyncs;
            }
        }
    } while (waiting);
    if (D_800D2D28->unkCC[3] == 0) {
        D_800D3014 = 0xFF;
    }
    do {
        if (func_80036410()) {
            func_80035DB0();
        } else {
            while (func_80035CDC()) {
                if (D_8005948C & 0x20) {
                    D_800D3014 = 4;
                    break;
                }
                if (D_8005948C & 0x800) {
                    if (D_800C3444 == 0) {
                        func_80037EE4();
                        func_8001FAB4(0x88, 0x64);
                        func_8001FAB4(0x88, 0x144);
                        vsyncs = D_80059488;
                        D_800C3444 = 1;
                    } else {
                        func_80037E8C();
                        D_80059488 = vsyncs;
                        D_800C3444 = 0;
                    }
                    break;
                }
            }
        }
    } while (D_800C3444 != 0);
    if (D_800D2D28->unkA0 != 0 && D_800D32F8[0]->counting != 0) {
        for (i = 0; i < 3; i++) {
            if (D_800D32F8[i]->done[0] == 0) {
                if (D_800CCCE8.toCount[i][0] == 0) {
                    D_800D32F8[i]->done[0] = 1;
                } else {
                    D_800CCCE8.toCount[i][0]--;
                    D_800CCCE8.expTotals[i][0]++;
                }
            }
            if (D_800D32F8[i]->done[1] == 0) {
                if (D_800CCCE8.toCount[i][1] == 0) {
                    D_800D32F8[i]->done[1] = 1;
                } else {
                    D_800CCCE8.toCount[i][1]--;
                    D_800CCCE8.expTotals[i][1]++;
                }
            }
            done &= D_800D32F8[i]->done[0];
            if (D_800D32F8[0]->unk15F8 != 0) {
                done &= D_800D32F8[i]->done[1];
            }
        }
        if (!done) {
            func_801DF270();
            func_801DF4C0();
        } else {
            D_800D32F8[0]->counting = 0;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008A684);
#endif

/* Result-screen step by the battle end state 800c3e4c. */
void func_8008A9C0(u8 member) {
    switch (D_800C3E4C) {
    case 0:
        func_8008A684(member);
        break;
    case 1:
        func_8008A274(member);
        break;
    case 2:
        func_8008A3EC(member);
        break;
    }
}

/* Play menu sound effect `id` of the system effect bank. */
void func_8008AA40(u8 id) {
    func_80039DB8((D_8005919C->bank << 16) | id);
}

/* Play menu sound effect `id` while menu effects are enabled. */
void func_8008AA74(u8 id) {
    if (D_800D366C != 0) {
        func_8008AA40(id);
    }
}

/* Split `value` into nine decimal digits at 800c3cf4, leading zeros 0xff. */
void func_8008AAA0(u32 value) {
    u32 divisor = 100000000;
    s32 i;

    for (i = 0; i < 9; i++) {
        D_800C3CF4[i] = value / divisor;
        value %= divisor;
        divisor /= 10;
    }
    for (i = 1; i < 9; i++) {
        if (D_800C3CF4[i] != 0) {
            if (D_800C3CF4[i - 1] == 0) {
                D_800C3CF4[i - 1] = 0xFF;
            }
            break;
        }
        D_800C3CF4[i - 1] = 0xFF;
    }
}

/* Heap mode 0x20/0. */
void func_8008AB4C(void) {
    func_80028470(0x20, 0);
}

/* Heap mode 0x20/2. */
void func_8008AB70(void) {
    func_80028470(0x20, 2);
}

/* Heap mode 0x20/3. */
void func_8008AB94(void) {
    func_80028470(0x20, 3);
}

/* Allocate a battle heap block (owner tag 2). */
s32 func_8008ABB8(s32 size, s32 mode) {
    func_80032498(2, 0);
    return (s32)func_80031BDC(size, mode);
}

/* Allocate a text image block for `count` characters. */
s32 func_8008AC00(s32 count) {
    func_80032498(2, 0);
    return (s32)func_80031BDC((count + 3) * 26, 0);
}

/* Wait frames until the disc reads finish. */
void func_8008AC50(void) {
    while (func_800286CC() != 0) {
        func_800716D8();
    }
}

/* Queue event 0xf3 for `actor` with the enemies of `mask` whose +0x34 bit
 * 0x800 is set (low three bits dropped). */
void func_8008AC88(u16 mask, u8 actor) {
    u16 targets = mask & 0xFFF8;
    s32 i;

    for (i = 0; i < 8; i++) {
        if (func_80089C9C(targets, i + 3) && !(D_800CCCE8.records[i + 3].pilot.flags34 & 0x800)) {
            targets &= func_80089C48(i + 3);
        }
    }
    if (targets) {
        D_800C3FE8[D_800C3EAC->eventCount].actor = actor;
        D_800C3FE8[D_800C3EAC->eventCount].type = 0xF3;
        D_800C3FE8[D_800C3EAC->eventCount].parameter = targets;
        D_800C3EAC->eventCount++;
    }
}

/* Execute the chosen technique (turn state +0x2e6) for the member: reset
 * the events, commit the command (from 23, or the gear's from 22 in a gear;
 * command bits 0-2 target the candidates, else the chosen target), apply
 * its results, face the camera, show the member's model, queue its event
 * with the targeted enemies' reactions and wait for the presentation. */
void func_8008ADD0(member)
u8 member;
{
    u16 targets;
    u16 command;
    s32 i;

    D_800D366C = 0;
    func_800BCD98(0);
    for (i = 0; i < 32; i++) {
        D_800C3FE8[i].type = 0xFF;
    }
    if (D_800D32A0[member].unk1 == 0) {
        D_800C3EAC->unk2DC = D_800C3EAC->unk2E6 + 23;
        command = D_800CCCE8.partyCommands[member][D_800C3EAC->unk2E6 + 22].state;
    } else {
        D_800C3EAC->unk2DC = D_800C3EAC->unk2E6 + 22;
        command = D_800CCCE8.gearCommands[member][D_800C3EAC->unk2E6 + 21].state;
    }
    if (command & 7) {
        targets = D_800C3D64;
    } else {
        targets = func_80089C08(D_800C3E2C);
    }
    func_8008AC88(targets, member);
    func_80085388();
    func_80085CCC(member, targets, D_800C3EAC->unk2DC - 1);
    func_80085C88(D_800C3EAC->eventCount);
    func_800BC404(D_800D2C94.targets | func_80089C08(member));
    func_800B89FC(1, member, D_800C3EAC->slots[member].defaultTarget, func_80080AE4(member));
    D_800C3FE8[D_800C3EAC->eventCount].type = D_800D2C94.animation;
    D_800C3FE8[D_800C3EAC->eventCount].actor = member;
    D_800C3FE8[D_800C3EAC->eventCount].targetMask = D_800D2C94.targets;
    for (i = 3; i < 11; i++) {
        if (func_80089C9C(D_800D2C94.targets, i)) {
            func_80079840(member, i);
        }
    }
    D_800C3EAC->eventCount++;
    func_80080B64(member);
    while (D_800C3EAC->eventsDone == 0) {
        func_800716D8();
    }
}

/* Hide the command windows (four panels); without `keep` show the
 * +0x641c lists. */
void func_8008B108(u8 keep) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = D_800D2D28->windows[3] = 0;
    D_800D2D28->unkB7 = 0;
    if (keep == 0) {
        D_800D2D28->unkCB = 1;
    }
}

/* Show the command windows (four panels, page 1) and frame the camera on
 * the member and its default target. */
void func_8008B168(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = D_800D2D28->windows[3] = 1;
    D_800D2D28->unkB7 = 1;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

/* Confirm the technique in list cell (column, row) for the member: its
 * command (from 22, or the gear's from 21 when the member is in a gear) needs
 * the character's permission bit and the EP it costs. Hides the command
 * windows and commits it, paying the EP; if the commit fails the windows
 * come back. A refused technique plays the error sound. Returns 1 when
 * committed. */
u8 func_8008B224(u8 member, u8 column, u8 row) {
    u8 committed = 0;
    u8 refused = 1;
    u8 allowed = 0;
    u16 command;
    u8 cost;

    if (D_800D32A0[member].unk1 == 0) {
        command = D_800CCCE8.partyCommands[member][row * 2 + column + 22].state;
        cost = D_800CCCE8.partyCommands[member][row * 2 + column + 22].cost;
        allowed = func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask2, column + row * 2) != 0;
    } else {
        command = D_800CCCE8.gearCommands[member][row * 2 + column + 21].state;
        cost = D_800CCCE8.gearCommands[member][row * 2 + column + 21].cost;
        if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask6, column + row * 2) != 0) {
            allowed = 1;
        }
    }
    if (D_800CCCE8.records[member].pilot.ep >= cost && allowed) {
        func_8008B108(1);
        D_800D2D28->unkC6 = 1;
        if (func_80085084(command, member, 0)) {
            D_800CCCE8.records[member].pilot.ep -= cost;
            committed = 1;
        } else {
            D_800D2D28->unkC6 = 0;
            func_8008B168(member);
        }
        refused = 0;
    }
    if (refused) {
        func_8008AA74(0x4F);
    }
    return committed;
}
