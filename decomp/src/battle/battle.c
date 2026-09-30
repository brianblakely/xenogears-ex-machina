/* Battle code from 8008115C to 8009E53C (the earlier units have files of
 * their own). Its rodata starts at 80070010, where the jump tables return to
 * 0 mod 8 right after 80080160's (docs/matching.md); the text boundary lies
 * after 80080160. The range still spans original units: the tables change
 * phase again at 80070314 (8008B478) and 80070370 (80094EE4). */
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 2) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 2) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        D_800C3EA4->panels[member].unk1E1 = 2;
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 2) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 2) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 2) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 3) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 0) {
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
        } else if (D_800C3EAC->repeatArmed != 0 && D_800C3E29 == 2) {
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
    func_80076D58(&D_800D2DB4->unk5550[index * 2], odd, 3);
    width = func_80034EAC(func_80033784(D_800D2D24[member], id), *pixels, 0x1B, odd);
    rect.x = cell * 30 + 0x3C0;
    rect.y = 0x1A;
    rect.w = 0x1E;
    rect.h = 13;
    LoadImage(&rect, *pixels);
    func_80076C78(&D_800D2DB4->unk5550[index * 2 + D_800CCB04.buffer], (column + (offset + 1)) * 16 + 0x50 + index * 4,
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
                func_80076A10(id, &D_800D2DB4->unk5640[D_800D2DB4->counts[12] * 2], 0x50 + combo * 16, 0xD0 - index * 16);
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
                func_80076A10(7, &D_800D2DB4->unk5640[D_800D2DB4->counts[12] * 2], 0x50 + combo * 16, 0xD0 - index * 16);
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
                func_80076A10(8, &D_800D2DB4->unk5640[D_800D2DB4->counts[12] * 2], 0x54 + combo * 16, 0xD0 - index * 16);
            D_800D2DB4->counts[12] +=
                func_80076A10(7, &D_800D2DB4->unk5640[D_800D2DB4->counts[12] * 2], 0x64 + combo * 16, 0xD0 - index * 16);
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
                func_80076A10(9, &D_800D2DB4->unk5640[D_800D2DB4->counts[12] * 2], 0x58 + combo * 16, 0xD0 - index * 16);
            D_800D2DB4->counts[12] +=
                func_80076A10(7, &D_800D2DB4->unk5640[D_800D2DB4->counts[12] * 2], 0x68 + combo * 16, 0xD0 - index * 16);
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086C88);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80086F98);

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
        D_800D2DB4->unk5D70[4] +=
            func_80076A10(D_800C33B0[i], &D_800D2DB4->unk1720[D_800D2DB4->unk5D70[4] * 2], 0xA0, 0x64);
    }
    D_800D2DB4->counts[14] = D_800CCB04.buffer;
    D_800D2DB4->unk5D70[0] = func_80076A10(0xA8, &D_800D2DB4->unk0[D_800D2DB4->unk5D70[0] * 2], 0xA0, 0x64);
    D_800D2DB4->buffers[14] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->unk5D70[4]; i++) {
        func_80076B68(&D_800D2DB4->unk1720[i * 2 + D_800D2DB4->counts[14]]);
    }
    for (i = 0; i < D_800D2DB4->unk5D70[0]; i++) {
        func_80076B68(&D_800D2DB4->unk0[i * 2 + D_800D2DB4->buffers[14]]);
    }
    for (i = 2; i < 4; i++) {
        D_800D2DB4->counts[9] +=
            func_80076A10(D_800C33B0[i], &D_800D2DB4->unk2530[D_800D2DB4->counts[9] * 2], 0xA0, 0x64);
    }
    D_800D2DB4->buffers[9] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[9]; i++) {
        func_80076BF0(&D_800D2DB4->unk2530[i * 2 + D_800D2DB4->buffers[9]]);
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80088B80);

/* Build the glyphs of the flags set in 800d2c30 (up to five, 10 pixels
 * apart from y 0x6e) into the +0x4ce0 primitives. */
void func_80089038(void) {
    s32 i;
    s32 y; /* 16.16 */

    i = 0;
    y = 0x6E << 16;
    D_800D2DB4->unk5DA1 = 0;
    for (; i < 5; i++) {
        if (func_80089C6C(D_800D2C30, i)) {
            D_800D2DB4->unk5DA1 += func_80076A10(i + 0xC4, &D_800D2DB4->unk4CE0[D_800D2DB4->unk5DA1 * 2], 0xE0, y >> 16);
            y += 10 << 16;
        }
    }
    D_800D2DB4->unk5DA0 = D_800CCB04.buffer;
    D_800D2DB4->unk5DA2 = 0;
}

/* Build glyph 0xa0 (0xa1 with 800d2c38) into the +0x3ac0 primitives and
 * initialise the current buffer's quads. */
void func_80089110(void) {
    s32 id = 0xA0;
    s32 i;

    if (D_800D2C38 != 0) {
        id = 0xA1;
    }
    D_800D2DB4->counts[0] = func_80076A10(id, D_800D2DB4->unk3AC0, 0xA0, 0x64);
    D_800D2DB4->buffers[0] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[0]; i++) {
        func_80076B68(&D_800D2DB4->unk3AC0[i * 2 + D_800D2DB4->buffers[0]]);
    }
}

/* Build the two glyphs of the escape/limit page (800d2c34 - 0x5d and
 * - 0x25) into lists 2 and 10 and initialise their quads. */
#ifdef NON_MATCHING
void func_800891E4(void) {
    s32 i;
    u8 second = D_800D2C34 - 0x25;

    D_800D2DB4->counts[2] = func_80076A10((u8)(D_800D2C34 - 0x5D), D_800D2DB4->unk3E80, 0xA0, 0x64);
    D_800D2DB4->buffers[2] = D_800CCB04.buffer;
    D_800D2DB4->counts[10] = func_80076A10(second, D_800D2DB4->unk43D0, 0xA0, 0x64);
    D_800D2DB4->buffers[10] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[2]; i++) {
        func_80076B68(&D_800D2DB4->unk3E80[i * 2 + D_800D2DB4->buffers[2]]);
    }
    for (i = 0; i < D_800D2DB4->counts[10]; i++) {
        func_80076BF0(&D_800D2DB4->unk43D0[i * 2 + D_800D2DB4->buffers[10]]);
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
                func_80076A10(digit + 0x92, &D_800D2DB4->unk4E70[D_800D2DB4->counts[3] * 2], x >> 16, 0x46);
            x += 6 << 16;
        }
    }
    D_800D2DB4->buffers[3] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[3]; i++) {
        func_80076B68(&D_800D2DB4->unk4E70[i * 2 + D_800D2DB4->buffers[3]]);
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
                    func_80076A10(digit + 0x92, &D_800D2DB4->unk4FB0[D_800D2DB4->counts[4] * 2], x >> 16, 0x4E);
                digits++;
                x += 6 << 16;
            }
        }
        D_800D2DB4->counts[4] +=
            func_80076A10(0x9D, &D_800D2DB4->unk4FB0[D_800D2DB4->counts[4] * 2], digits * 6 + 0x11A, 0x4E);
    } else {
        D_800D2DB4->counts[4] = func_80076A10(0xA2, D_800D2DB4->unk4FB0, 0x11A, 0x4E);
    }
    D_800D2DB4->buffers[4] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[4]; i++) {
        func_80076B68(&D_800D2DB4->unk4FB0[i * 2 + D_800D2DB4->buffers[4]]);
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
                func_80076A10(digit + 0x92, &D_800D2DB4->unk50A0[D_800D2DB4->counts[5] * 2], x >> 16, 0x56);
            digits++;
            x += 6 << 16;
        }
    }
    D_800D2DB4->counts[5] +=
        func_80076A10(0x9D, &D_800D2DB4->unk50A0[D_800D2DB4->counts[5] * 2], digits * 6 + 0x11A, 0x56);
    D_800D2DB4->buffers[5] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[5]; i++) {
        func_80076B68(&D_800D2DB4->unk50A0[i * 2 + D_800D2DB4->buffers[5]]);
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
                func_80076A10(digit + 0x92, &D_800D2DB4->unk51E0[D_800D2DB4->counts[6] * 2], x >> 16, 0x5E);
            x += 6 << 16;
        }
    }
    D_800D2DB4->buffers[6] = D_800CCB04.buffer;
    for (i = 0; i < D_800D2DB4->counts[6]; i++) {
        func_80076B68(&D_800D2DB4->unk51E0[i * 2 + D_800D2DB4->buffers[6]]);
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
                func_80076A10(digit + 0x92, &D_800D2DB4->unk5280[D_800D2DB4->counts[7] * 2], x >> 16, 0xCC);
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
                func_80076A10(digit + 0x92, &D_800D2DB4->unk53C0[D_800D2DB4->counts[8] * 2], x >> 16, 0xCC);
            x += 8 << 16;
        }
    }
    D_800D2DB4->counts[8] += func_80076A10(0x9C, &D_800D2DB4->unk53C0[D_800D2DB4->counts[8] * 2], 0x41, 0xCC);
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80089CCC);

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

/* Run the member's technique menu: four windows, a two-column list of
 * twelve visible cells scrolled by rows (800d3288 in pixels, 800d39d4 the
 * scroll request), until a technique is committed (1, its index in the
 * turn state +0x2e6) or the menu is cancelled (0). (Nonmatching: GCC aligns
 * the jump table to 8 where the original's is at 0x80070314, and the cursor
 * x multiply is synthesised differently.) */
#ifdef NON_MATCHING
u8 func_8008B478(u8 member) {
    s32 frame;
    u8 ticks;
    u8 cell;
    u8 top;
    u8 shownCell;
    u8 shownTop;
    s32 shownScroll;
    u8 result;
    s32 target;

    cell = 0;
    top = 0;
    shownCell = 0xFF;
    shownTop = 0xFF;
    shownScroll = 0xFFFF;
    result = 2;
    frame = 4;
    ticks = 0;
    D_800D3288 = 0;
    D_800D39D4 = 0;
    D_800D2D28->unkCB = 0;
    func_8008F8F4(0, 0x1C, 0xA0, 0xAC, 0x38, 0, 1);
    func_8008F8F4(1, 0x20, 0x2C, 0x118, 0x70, 0, 1);
    func_8008F8F4(2, 0xD0, 0xA4, 0x50, 0x18, 0, 1);
    func_8008F8F4(3, 0xE6, 0xC0, 0x4C, 0x18, 0, 1);
    func_80091064(member);
    func_80077698();
    do {
        if (cell != shownCell || top != shownTop) {
            func_80091D38(member, cell, top);
            shownCell = cell;
            shownTop = top;
        }
        if (D_800D3288 != shownScroll) {
            func_80091604(D_800D3288);
            func_80091EC4(D_800D3288);
            shownScroll = D_800D3288;
        }
        func_80090B90((cell % 2) * 0x84 + 0x2A, (cell / 2) * 16 + 0x38, &frame, &ticks);
        func_800716D8();
        switch (D_800D3014) {
        case 5:
            result = 0;
            break;
        case 4:
            if (func_8008B224(member, cell, top)) {
                result = 1;
                D_800C3EAC->unk2E6 = cell + top * 2;
            }
            break;
        case 0:
            if (top * 2 + cell != 15) {
                if (cell + 1 == 12) {
                    D_800D39D4 = 1;
                } else {
                    cell++;
                }
            }
            break;
        case 2:
            if (top * 2 + cell > 0) {
                if (cell == 0 && top != 0) {
                    D_800D39D4 = 2;
                } else {
                    cell--;
                }
            }
            break;
        case 1:
            if (top * 2 + cell < 14) {
                if (cell + 2 >= 12) {
                    D_800D39D4 = 3;
                } else {
                    cell += 2;
                }
            }
            break;
        case 3:
            if (top * 2 + cell >= 2) {
                if (cell < 2 && top != 0) {
                    D_800D39D4 = 4;
                } else {
                    cell -= 2;
                }
            }
            break;
        }
        switch (D_800D39D4) {
        case 1:
            target = (top + 1) * 16;
            if (D_800D3288 >= target) {
                top++;
                cell--;
                D_800D3288 = target;
                D_800D39D4 = 0;
            }
            break;
        case 2:
            target = (top - 1) * 16;
            if (D_800D3288 < target) {
                top--;
                cell++;
                D_800D3288 = target;
                D_800D39D4 = 0;
            }
            break;
        case 3:
            target = (top + 1) * 16;
            if (D_800D3288 >= target) {
                top++;
                D_800D3288 = target;
                D_800D39D4 = 0;
            }
            break;
        case 4:
            target = (top - 1) * 16;
            if (D_800D3288 < target) {
                top--;
                D_800D3288 = target;
                D_800D39D4 = 0;
            }
            break;
        }
    } while (result == 2);
    func_8008B108(result);
    func_8007765C();
    func_80077980();
    func_8008FA60(0);
    func_8008FA60(1);
    func_8008FA60(2);
    func_8008FA60(3);
    return result;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008B478);
#endif

/* Execute the chosen item (turn state +0x2e6) for the member: reset the
 * events, show the member's use model, commit the item against its targets
 * (the target candidates, or the chosen target), use one up unless the
 * item keeps (0x8000), apply its results as event 0xf5, react the targeted
 * enemies, and wait for the presentation; targeted party members react. */
void func_8008B908(member)
u8 member;
{
    u16 targets;
    s32 i;

    D_800D366C = 0;
    func_800BCD98(0);
    for (i = 0; i < 32; i++) {
        D_800C3FE8[i].type = 0xFF;
    }
    func_800B89FC(1, member, D_800C3EAC->slots[member].defaultTarget, func_80080AE4(member));
    if (D_800D2200[D_800C3EAC->unk2E6].target & 7) {
        targets = D_800C3D64;
    } else {
        targets = func_80089C08(D_800C3E2C);
    }
    func_8008AC88(targets, member);
    func_80085388();
    func_80085C48(member, targets, D_800C3EAC->unk2E6);
    if (!(D_800D2C94.held & 0x8000)) {
        if (--D_800D2C94.itemCounts[D_800C3D00 * 2 + D_800D3670] == 0) {
            D_800D2C94.itemIds[D_800C3D00 * 2 + D_800D3670] = 0;
        }
    }
    D_800C2050 = 1;
    func_80085C88(D_800C3EAC->eventCount);
    D_800C2050 = 0;
    D_800C3FE8[D_800C3EAC->eventCount].type = 0xF5;
    D_800C3FE8[D_800C3EAC->eventCount].parameter = D_800D2C94.animation;
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
    for (i = 0; i < 3; i++) {
        if (func_80089C9C(targets, i)) {
            D_800D2D28->reaction[i] = 1;
        }
    }
}

/* Hide the command windows (two panels); without `keep` show the +0x641c
 * lists. */
void func_8008BC40(u8 keep) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 0;
    D_800D2D28->unkB7 = 0;
    if (keep == 0) {
        D_800D2D28->unkCB = 1;
    }
}

/* Show the command windows (two panels, page 2) and frame the camera on the
 * member and its default target. */
void func_8008BC98(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 1;
    D_800D2D28->unkB7 = 2;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

/* Use the list item at (column, row) for `member`: an item (from the
 * character list) starts its effect with target selection, 1 when started;
 * a gear part (from the gear list, inGear) is applied and its count taken,
 * 2 when applied. An empty entry buzzes (0x4f). */
#ifdef NON_MATCHING
u8 func_8008BD50(member, column, row, inGear)
u8 member;
u8 column;
u8 row;
u8 inGear;
{
    u8 *id;
    s32 index;
    u16 effect;
    u8 item;
    u8 result;

    id = D_800D2CE0;
    index = row * 2 + column;
    id += index;
    effect = D_800D2200[*id].target;
    result = 0;
    if (!inGear) {
        item = *id;
    } else {
        item = D_800C3D70[index];
    }
    if (item != 0) {
        if (!inGear) {
            func_8008BC40(1);
            D_800D2D28->unkC6 = 1;
            if (func_80085084(effect, member, 0)) {
                result = 1;
            } else {
                D_800D2D28->unkC6 = 0;
                func_8008BC98(member);
            }
        } else if (((s32 (*)())func_8009A7E4)(item - 50)) {
            /* Both calls are unprototyped in the original: the part index
             * is passed unnarrowed and 8009a854's entry argument is left
             * undefined (it then uses whatever the register holds). */
            ((void (*)())func_8009A854)(item);
            if (--D_800D3688[row * 2 + column] == 0) {
                D_800C3D70[row * 2 + column] = 0;
            }
            result = 2;
        }
    } else {
        func_8008AA74(0x4F);
    }
    return result;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BD50);
#endif

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008BED8);

/* Hide the command windows; with `close` also close windows 0 and 1 and
 * release the graphics block. */
void func_8008C360(u8 close) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->unkB7 = 0;
    if (close != 0) {
        D_800D2D28->unkC6 = 0;
        func_8008FA60(0);
        func_8008FA60(1);
        func_800716D8();
        func_8007765C();
        func_80077980();
    } else {
        D_800D2D28->windows[0] = D_800D2D28->windows[1] = 0;
    }
}

/* Show the command windows (two panels, page 3) and frame the camera on the
 * member and its default target. */
void func_8008C3F0(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->unkB7 = 3;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = 1;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

/* Run the member's combo: choose the steps (8008c81c); when cancelled
 * reopen the command windows and return 1. Otherwise enter the attack page
 * against the chosen target (an event 0xf3 first when it reacts while
 * down), commit each chosen step (action step + 8) as an event, move into
 * the target's group and wait for the presentation. Returns 0. */
u8 func_8008C4A8(member)
u8 member;
{
    u8 cancelled = 1;
    s32 i;

    if (func_8008C81C(member)) {
        func_800BAF40(member, 0x100);
        func_8008C360(1);
        D_800D2D28->unkCB = 1;
        cancelled = 0;
    } else {
        func_800BAF40(member, 0x80);
        D_800D2D28->unkAF = 0;
        func_8008C360(1);
        D_800D366C = 0;
        func_800BCD98(0);
        D_800C3EAC->slots[member].defaultTarget = D_800C3E2C;
        func_800877E0(member, D_800C3EAC->slots[member].defaultTarget);
        D_800C4928 = 1;
        func_80087A38(member);
        if (D_800CCCE8.records[D_800C3EAC->slots[member].defaultTarget].pilot.flags34 & 0x800) {
            D_800C3FE8[D_800C3EAC->eventCount].actor = member;
            D_800C3FE8[D_800C3EAC->eventCount].type = 0xF3;
            D_800C3FE8[D_800C3EAC->eventCount].parameter = func_80089C08(D_800C3EAC->slots[member].defaultTarget);
            D_800C3EAC->eventCount++;
        }
        for (i = 0; i < 7; i++) {
            if (D_800C3EAC->combo[i] != 0xFF) {
                D_800C3EAC->unk2DC = D_800C3EAC->combo[i] + 8;
                func_80085CCC(member, func_80089C08(D_800C3EAC->slots[member].defaultTarget), D_800C3EAC->unk2DC - 1);
                func_80085C88(D_800C3EAC->eventCount);
                D_800C3FE8[D_800C3EAC->eventCount].type = D_800C3EAC->unk2DC - 1;
                D_800C3FE8[D_800C3EAC->eventCount].actor = member;
                D_800C3FE8[D_800C3EAC->eventCount].targetMask = D_800D2C94.targets;
                D_800C3EAC->eventCount++;
            }
        }
        func_80080B64(member);
        func_80087EDC(member, D_800C3EAC->slots[member].defaultTarget);
        while (D_800C3EAC->eventsDone == 0) {
            func_800716D8();
        }
    }
    return cancelled;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008C81C);

/* Hide the command windows (three panels); without `keep` show the +0x641c
 * lists. */
void func_8008CCCC(u8 keep) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 0;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = 0;
    D_800D2D28->unkB7 = 0;
    if (keep == 0) {
        D_800D2D28->unkCB = 1;
    }
}

/* Show the command windows (three panels, page 4) and frame the camera on
 * the member and its default target. */
void func_8008CD28(u8 member) {
    D_800D2D28->unk9C = D_800D2D28->unk9D = D_800D2D28->unk9E = 1;
    D_800D2D28->windows[0] = D_800D2D28->windows[1] = D_800D2D28->windows[2] = 1;
    D_800D2D28->unkB7 = 4;
    func_800BC404(func_80089C08(member) | func_80089C08(D_800C3EAC->slots[member].defaultTarget));
    func_800BCD98(func_80089C08(D_800C3EAC->slots[member].defaultTarget));
}

/* Confirm the member's gear command `index` (0-3, the gear's commands from
 * 37): it needs the fuel it costs, the character's permission bit and no
 * seal status for it. Hides the command windows and commits the command,
 * paying the fuel; if the commit fails the windows come back. A refused
 * command plays the error sound. Returns 1 when committed. */
u8 func_8008CDE4(u8 member, u8 index) {
    u16 sealed[4];
    u8 committed;
    u16 cost;
    u16 command;
    u8 refused;

    sealed[0] = D_800C3234[13];
    sealed[1] = D_800C3234[14];
    sealed[2] = D_800C3234[15];
    sealed[3] = D_800C3234[3];
    committed = 0;
    cost = D_800CCCE8.gearCommands[member][index + 37].hudState;
    command = D_800CCCE8.gearCommands[member][index + 37].state;
    refused = 1;

    if (D_800CCCE8.records[member].gear.fuel >= cost &&
        func_80089C6C(D_8006ECF4[D_800D2D24[member]].flags1A, index) != 0 &&
        !(D_800CCCE8.records[member].pilot.status7A & sealed[index])) {
        func_8008CCCC(1);
        D_800D2D28->unkC6 = 1;
        if (func_80085084(command, member, 0)) {
            D_800CCCE8.records[member].gear.fuel -= cost;
            committed = 1;
        } else {
            D_800D2D28->unkC6 = 0;
            func_8008CD28(member);
        }
        refused = 0;
    }
    if (refused) {
        func_8008AA74(0x4F);
    }
    return committed;
}

/* Run the member's gear command menu: list the gear's commands 37-40 the
 * character may use and no status seals (id and fuel cost), open its three
 * windows and move the cursor until a command is committed (1, remembered
 * + 0x10 in the turn state) or the menu is cancelled (0). */
u8 func_8008CFB8(member)
u8 member;
{
    u8 ids[4];
    u16 costs[4];
    u16 seals[4];
    s32 frame;
    u8 ticks;
    s32 shown;
    s32 cursor;
    u8 result;
    s32 i;

    cursor = 0;
    shown = 0xFF;
    result = 2;
    frame = 4;
    ticks = 0;
    seals[0] = D_800C3234[13];
    seals[1] = D_800C3234[14];
    seals[2] = D_800C3234[15];
    seals[3] = D_800C3234[3];
    for (i = 0; i < 4; i++) {
        ids[i] = 0xFF;
        costs[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].flags1A, i) != 0 &&
            !(D_800CCCE8.records[member].pilot.status7A & seals[i])) {
            ids[i] = D_800CCCE8.records[member].pilot.gearId * 4 + i;
            costs[i] = D_800CCCE8.gearCommands[member][i + 37].hudState;
        }
    }
    D_800D2D28->unkCB = 0;
    func_8008F8F4(0, 0x74, 0xA0, 0xAC, 0x38, 0, 1);
    func_8008F8F4(1, 0x84, 0x4C, 0x9C, 0x50, 0, 1);
    func_8008F8F4(2, 0x1C, 0xA0, 0x50, 0x18, 0, 1);
    func_800930AC(member, ids, costs);
    func_80077698();
    while (result == 2) {
        if (cursor != shown) {
            func_800939CC(member, cursor);
            shown = cursor;
        }
        func_80090B90(0x8C, cursor * 16 + 0x58, &frame, &ticks);
        func_800716D8();
        switch (D_800D3014) {
        case 5:
            result = 0;
            break;
        case 4:
            if (func_8008CDE4(member, cursor)) {
                result = 1;
                D_800C3EAC->unk2E6 = cursor + 0x10;
            }
            break;
        case 1:
            if (++cursor >= 4) {
                cursor = 3;
            }
            break;
        case 3:
            if (--cursor < 0) {
                cursor = 0;
            }
            break;
        }
    }
    func_8008CCCC(result);
    func_8007765C();
    func_80077980();
    func_8008FA60(0);
    func_8008FA60(1);
    func_8008FA60(2);
    return result;
}

/* Fade the list quads of both draw buffers (every other one from 800d2d28
 * +0xa3): semi-transparent, raw texture, darker by the turn state's step
 * each frame. When all have faded out, clear the fading flag (+0xcb). */
void func_8008D328(void) {
    s32 buffer;
    s32 i;
    u8 fading = 0;
    u8 shade;

    for (buffer = 0; buffer < 2; buffer++) {
        if (D_800D2D28->unkD0[buffer] != 0) {
            for (i = 0; i < D_800D2D28->unkD0[buffer] * 2; i += 2) {
                SetSemiTrans(&D_800C3EA4->unk641C[buffer][i + D_800D2D28->unkA3], 1);
                SetShadeTex(&D_800C3EA4->unk641C[buffer][i + D_800D2D28->unkA3], 0);
                D_800C3EA4->unk641C[buffer][i + D_800D2D28->unkA3].tpage |= 0x20;
                shade = D_800C3EA4->unk641C[buffer][i + D_800D2D28->unkA3].r0;
                if (shade != 0) {
                    fading = 1;
                    (D_800C3EA4->unk641C[buffer] + (i + D_800D2D28->unkA3))->r0 = shade - D_800C3EAC->unk2E0 * 16;
                    (D_800C3EA4->unk641C[buffer] + (i + D_800D2D28->unkA3))->g0 = shade - D_800C3EAC->unk2E0 * 16;
                    (D_800C3EA4->unk641C[buffer] + (i + D_800D2D28->unkA3))->b0 = shade - D_800C3EAC->unk2E0 * 16;
                }
            }
        }
    }
    if (!fading) {
        D_800D2D28->unkCB = 0;
    }
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008D598);

/* Place a window's four corner glyphs (the alternate set while the battle
 * is ending) at its corners in the current draw buffer. */
void func_8008DC34(u8 window, u16 x, u16 y, u16 w, u16 h) {
    u8 glyphs[4];
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    if (D_800C3E4C != 0) {
        glyphs[0] = 0x4A;
        glyphs[1] = 0x4C;
        glyphs[2] = 0x4F;
        glyphs[3] = 0x51;
    } else {
        glyphs[0] = 0xF0;
        glyphs[1] = 0xF2;
        glyphs[2] = 0xF5;
        glyphs[3] = 0xF7;
    }
    block->cornerCount = 0;
    block->cornerCount += func_80076A10(glyphs[0], &block->corners[block->cornerCount * 2], x, y);
    block->cornerCount += func_80076A10(glyphs[1], &block->corners[block->cornerCount * 2], x + w - 8, y);
    block->cornerCount += func_80076A10(glyphs[2], &block->corners[block->cornerCount * 2], x, y + h - 8);
    block->cornerCount += func_80076A10(glyphs[3], &block->corners[block->cornerCount * 2], x + w - 8, y + h - 8);
    for (i = 0; i < 4; i++) {
        func_80076B00(&block->corners[i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window's top edge: two pieces of texture 1 across the top,
 * each half the inner width, in the current draw buffer. */
void func_8008DE04(u8 window, u16 x, u16 y, u16 w) {
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    setXY4(block->frame[0] + D_800CCB04.buffer, x + 8, y - 8, x + 8 + (w - 16) / 2, y - 8, x + 8, y + 8,
           x + 8 + (w - 16) / 2, y + 8);
    setXY4(block->frame[0] + D_800CCB04.buffer + 2, x + 8 + (w - 16) / 2, y - 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y - 8, x + 8 + (w - 16) / 2, y + 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + 8);
    setUV4(block->frame[0] + D_800CCB04.buffer, WINDOW_TEX_U(1), WINDOW_TEX_V(1), WINDOW_TEX_U(1) + 7,
           WINDOW_TEX_V(1), WINDOW_TEX_U(1), WINDOW_TEX_V(1) + 16, WINDOW_TEX_U(1) + 7, WINDOW_TEX_V(1) + 16);
    setUV4(block->frame[0] + D_800CCB04.buffer + 2, WINDOW_TEX_U(1), WINDOW_TEX_V(1), WINDOW_TEX_U(1) + 7,
           WINDOW_TEX_V(1), WINDOW_TEX_U(1), WINDOW_TEX_V(1) + 16, WINDOW_TEX_U(1) + 7, WINDOW_TEX_V(1) + 16);
    for (i = 0; i < 2; i++) {
        func_80076B00(&block->frame[0][i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window's bottom edge: two pieces of texture 2 across the bottom,
 * each half the inner width, in the current draw buffer. */
void func_8008E430(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    setXY4(block->frame[1] + D_800CCB04.buffer, x + 8, y + h - 8, x + 8 + (w - 16) / 2, y + h - 8, x + 8,
           y + h + 8, x + 8 + (w - 16) / 2, y + h + 8);
    setXY4(block->frame[1] + D_800CCB04.buffer + 2, x + 8 + (w - 16) / 2, y + h - 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + h - 8, x + 8 + (w - 16) / 2, y + h + 8,
           x + 8 + (w - 16) / 2 + (w - 16) / 2, y + h + 8);
    setUV4(block->frame[1] + D_800CCB04.buffer, WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 1,
           WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2) + 16, WINDOW_TEX_U(2) - 1, WINDOW_TEX_V(2) + 16);
    setUV4(block->frame[1] + D_800CCB04.buffer + 2, WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 1,
           WINDOW_TEX_V(2), WINDOW_TEX_U(2) - 8, WINDOW_TEX_V(2) + 16, WINDOW_TEX_U(2) - 1, WINDOW_TEX_V(2) + 16);
    for (i = 0; i < 2; i++) {
        func_80076B00(&block->frame[1][i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window's left edge: two pieces of texture 3 down the left side,
 * each half the inner height, in the current draw buffer. */
void func_8008EA70(u8 window, u16 x, u16 y, u16 h) {
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    setXY4(block->frame[2] + D_800CCB04.buffer, x - 8, y + 8, x + 8, y + 8, x - 8, y + 8 + (h - 16) / 2, x + 8,
           y + 8 + (h - 16) / 2);
    setXY4(block->frame[2] + D_800CCB04.buffer + 2, x - 8, y + 8 + (h - 16) / 2, x + 8, y + 8 + (h - 16) / 2,
           x - 8, y + 8 + (h - 16) / 2 + (h - 16) / 2, x + 8, y + 8 + (h - 16) / 2 + (h - 16) / 2);
    setUV4(block->frame[2] + D_800CCB04.buffer, WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 30,
           WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3) + 7, WINDOW_TEX_U(3) + 30, WINDOW_TEX_V(3) + 7);
    setUV4(block->frame[2] + D_800CCB04.buffer + 2, WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 30,
           WINDOW_TEX_V(3), WINDOW_TEX_U(3) + 14, WINDOW_TEX_V(3) + 7, WINDOW_TEX_U(3) + 30, WINDOW_TEX_V(3) + 7);
    for (i = 0; i < 2; i++) {
        func_80076B00(&block->frame[2][i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window's right edge: two pieces of texture 4 down the right side,
 * each half the inner height, in the current draw buffer. */
void func_8008F0A8(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = D_800D2E38[window];
    s32 i;

    setXY4(block->frame[3] + D_800CCB04.buffer, x + w - 8, y + 8, x + w + 8, y + 8, x + w - 8,
           y + 8 + (h - 16) / 2, x + w + 8, y + 8 + (h - 16) / 2);
    setXY4(block->frame[3] + D_800CCB04.buffer + 2, x + w - 8, y + 8 + (h - 16) / 2, x + w + 8,
           y + 8 + (h - 16) / 2, x + w - 8, y + 8 + (h - 16) / 2 + (h - 16) / 2, x + w + 8,
           y + 8 + (h - 16) / 2 + (h - 16) / 2);
    setUV4(block->frame[3] + D_800CCB04.buffer, WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 30,
           WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4) + 7, WINDOW_TEX_U(4) + 30, WINDOW_TEX_V(4) + 7);
    setUV4(block->frame[3] + D_800CCB04.buffer + 2, WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 30,
           WINDOW_TEX_V(4), WINDOW_TEX_U(4) + 14, WINDOW_TEX_V(4) + 7, WINDOW_TEX_U(4) + 30, WINDOW_TEX_V(4) + 7);
    for (i = 0; i < 2; i++) {
        func_80076B00(&block->frame[3][i * 2 + D_800CCB04.buffer]);
    }
}

/* Place a window at (x, y, w, h) in the current draw buffer: its
 * background, corners and edges. The window is hidden while it changes. */
void func_8008F6E4(u8 window, u16 x, u16 y, u16 w, u16 h) {
    WindowBlock *block = D_800D2E38[window];

    D_800D2D28->windows[window] = 0;
    setXY4(block->shade + D_800CCB04.buffer, x, y, x + w, y, x, y + h, x + w, y + h);
    func_8008DC34(window, x, y, w, h);
    func_8008DE04(window, x, y, w);
    func_8008E430(window, x, y, w, h);
    func_8008EA70(window, x, y, h);
    func_8008F0A8(window, x, y, w, h);
    block->buffer = D_800CCB04.buffer;
    D_800D2D28->windows[window] = 1;
}

/* Open window `window` at (x, y) of w x h: allocate its blocks when it is not
 * shown; `animate` grows it open, otherwise it is drawn at once (and a frame
 * waited with `wait`). */
void func_8008F8F4(u8 window, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 wait) {
    WindowRect *rect;

    if (D_800D2D28->windows[window] == 0) {
        D_800D2E38[window] = (void *)func_8008ABB8(0x5A8, 0);
        bzero(D_800D2E38[window], 0x5A8);
        D_800D2D90[window] = (WindowRect *)func_8008ABB8(0xE, 0);
        bzero(D_800D2D90[window], 0xE);
        func_80077454(window);
    }
    if (animate != 0) {
        rect = D_800D2D90[window];
        rect->style = window;
        rect->x = x;
        rect->y = y;
        rect->w = w;
        rect->h = h;
        rect->curW = 0;
        rect->curH = 0;
        D_800D2D28->unkBF[window] = 0;
        D_800D2D28->unkB8[window] = 1;
    } else {
        func_8008F6E4(window, x, y, w, h);
        if (wait != 0) {
            func_800716D8();
        }
    }
}

/* Close window `window` and release its two blocks after a frame. */
void func_8008FA60(u8 window) {
    D_800D2D28->windows[window] = 0;
    D_800D2D28->unkB8[window] = 0;
    func_800716D8();
    func_800320E8(D_800D2E38[window]);
    func_800320E8(D_800D2D90[window]);
}

/* Grow every opening window by 32 pixels per frame up to its size, centred,
 * and mark it open when both sides are complete. */
void func_8008FAD8(void) {
    s32 i;
    WindowRect *rect;
    u8 done;

    for (i = 0; i < 7; i++) {
        rect = D_800D2D90[i];
        if (D_800D2D28->unkB8[i] != 0 && D_800D2D28->unkBF[i] == 0) {
            done = 0;
            if (rect->curW + 32 >= rect->w) {
                rect->curW = rect->w;
                done = 1;
            } else {
                rect->curW += 32;
            }
            if (rect->curH + 32 >= rect->h) {
                rect->curH = rect->h;
                done++;
            } else {
                rect->curH += 32;
            }
            if (done == 2) {
                D_800D2D28->unkBF[i] = 1;
            }
            func_8008F6E4(rect->style, rect->x + (rect->w >> 1) - (rect->curW >> 1),
                          rect->y + (rect->h >> 1) - (rect->curH >> 1), rect->curW, rect->curH);
        }
    }
}

/* Build a message frame from glyphs into the +0x1e68 list: `rows` side
 * glyphs down from `top`, the corner at (x, y) and a stretched edge of
 * width `w`. Nonmatching: prologue scheduling (the original reads `rows`
 * after clearing the count). */
#ifdef NON_MATCHING
void func_8008FC1C(s16 x, s16 y, s16 w, s16 top, u8 rows) {
    s32 i;
    s32 row;

    D_800D2D28->unkFC = 0;
    for (i = 0, row = top; i < rows; i++) {
        D_800D2D28->unkFC += func_80076A10(0x65, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2], x, row);
        row += 8;
    }
    D_800D2D28->unkFC = func_80076A10(0x64, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2], x, y) + D_800D2D28->unkFC;
    D_800D2D28->unkFC += func_800263E4(D_800D2F5C, 0x64, &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2],
                                       D_800CCB04.buffer, x, w, 0x1000, 0, 1);
    D_800D2D28->unkA6 = D_800CCB04.buffer;
    D_800D2D28->unk9D = 1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8008FC1C);
#endif

/* Open the standard message window (0x20, 0x5c, 0xcc x 0x60, style 0xe). */
void func_8008FDE4(void) {
    func_8008FC1C(0x20, 0x5C, 0xCC, 0x60, 0xE);
}

/* Build the item list page: (with `open`) set up the graphics block and the
 * message frame, then render every list entry's two item names and their
 * two-digit counts into VRAM text images (names at 0x380, digits at 0x3c0,
 * one 13-line row per entry pair). */
void func_8008FE18(u8 column, u8 row, u8 open) {
    TextImage images[32];
    RECT nameRect;
    RECT tensRect;
    RECT onesRect;
    RECT tens2Rect;
    RECT ones2Rect;
    RECT rect;
    s32 unused[2]; /* an unreferenced 8-byte local in the frame */
    u8 counts[48];
    u8 ids[48];
    s32 i;
    u8 tens;
    u32 *digit;

    if (open != 0) {
        func_80077610();
        func_80076EA4();
        func_8008FC1C(0x20, 0x5C, 0xCC, 0x60, 0xE);
    }
    D_800D2DB0 = (u32 *)func_8008AC00(0x39);
    bzero(D_800D2DB0, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, D_800D2DB0);
    for (i = 0; i < 48; i++) {
        ids[i] = D_800D2CE0[i];
        counts[i] = D_800D2CB0[i];
    }
    for (i = 0; i < 32; i++) {
        images[i].pixels = (u32 *)func_8008AC00(0x1B);
        bzero(images[i].pixels, 0x30C);
        nameRect.x = (i % 2) * 30 + 0x380;
        nameRect.y = (i / 2) * 13 + 0x100;
        nameRect.w = 30;
        nameRect.h = 13;
        tensRect.x = (i % 2) * 16 + 0x3C0;
        tensRect.y = (i / 2) * 13 + 0x100;
        tensRect.w = 6;
        tensRect.h = 13;
        onesRect.x = (i % 2) * 16 + 0x3C2;
        onesRect.y = (i / 2) * 13 + 0x100;
        onesRect.w = 6;
        onesRect.h = 13;
        tens2Rect.x = (i % 2) * 16 + 0x3C4;
        tens2Rect.y = (i / 2) * 13 + 0x100;
        tens2Rect.w = 6;
        tens2Rect.h = 13;
        ones2Rect.x = (i % 2) * 16 + 0x3C6;
        ones2Rect.y = (i / 2) * 13 + 0x100;
        ones2Rect.w = 6;
        ones2Rect.h = 13;
        if (ids[i] != 0) {
            func_80034EAC(func_80033818(ids[i]), images[i].pixels, 0x1B, 0);
        }
        if (ids[i + 16] != 0) {
            func_80034EAC(func_80033818(ids[i + 16]), images[i].pixels, 0x1B, 1);
        }
        func_800769E8(&nameRect, images[i].pixels);
        tens = counts[i] / 10;
        if (tens != 0) {
            digit = D_800C3E5C[tens].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&tensRect, digit);
        if (ids[i] != 0) {
            digit = D_800C3E5C[(u8)(counts[i] % 10)].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&onesRect, digit);
        if ((u8)(D_800D2CC0[i] / 10) != 0) {
            digit = D_800C3E5C[counts[i + 16] / 10].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&tens2Rect, digit);
        if (ids[i + 16] != 0) {
            digit = D_800C3E5C[(u8)(counts[i + 16] % 10)].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&ones2Rect, digit);
    }
    for (i = 0; i < 32; i++) {
        func_800320E8(images[i].pixels);
    }
    func_800320E8(D_800D2DB0);
    func_800716D8();
    D_800C3EA4->unkA230->unk669 = 1;
    D_800D2D28->unkB7 = 2;
}

/* Build nine glyph rows (0x66) from y + 0x64 into the +0xba8 primitives. */
void func_8009023C(s32 y) {
    s32 i;
    s32 offset;

    i = 0;
    offset = 0x68;
    D_800D2D28->unkF8 = 0;
    for (; i < 9; i++) {
        D_800D2D28->unkF8 += func_80076A10(0x66, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x20, (offset - 4) + y);
        offset += 8;
    }
    D_800D2D28->unkA5 = D_800CCB04.buffer;
    D_800D2D28->unk9C = 1;
}

/* Point the page title quads at list entry (column, row): the entry's image
 * cell (two per image row, 13 lines each; entries past 16 on the second
 * image page with the alternate CLUT). */
void func_80090310(u8 column, u8 row) {
    s32 index;
    s32 page;

    index = row * 2 + column;
    page = 0;
    if (index > 16) {
        index -= 16;
        D_800C3EA4->unkA230->unk140[D_800CCB04.buffer].clut = D_80059414;
        page = 0x10;
    } else {
        D_800C3EA4->unkA230->unk140[D_800CCB04.buffer].clut = D_800595D4;
    }
    func_80076C78(&D_800C3EA4->unkA230->unk140[D_800CCB04.buffer], 0x18, 0x33, (index % 2) * 0x78, (index / 2) * 13,
                  0x60);
    func_80076C78(&D_800C3EA4->unkA230->unk190[D_800CCB04.buffer], 0x84, 0x33, (index % 2) << 6 | page,
                  (index / 2) * 13, 0x10);
}

/* Point the item page icon quads at the images for the item in list cell
 * (column, row): its state icon (9 bit 0x4000, 7 bit 0x1000, else 8) and its
 * level frame (12, 13 or 21 for levels 0-2), each with its CLUT. */
void func_800904A0(u8 column, u8 row) {
    u16 flags;
    s32 icon;
    u8 frame;

    flags = D_800D2200[D_800D2CE0[row * 2 + column]].target;
    if (flags & 0x4000) {
        icon = 9;
    } else {
        icon = 8;
        if (flags & 0x1000) {
            icon = 7;
        }
    }
    switch (flags & 0xF) {
    case 0:
        frame = 12;
        break;
    case 1:
        frame = 13;
        break;
    case 2:
        frame = 21;
        break;
    }
    func_80076C78(&D_800C3EA4->unkA230->unk320[D_800CCB04.buffer], 0xA8, 0x33, D_800D2F68[icon].u, D_800D2F68[icon].v,
                  D_800D2F68[icon].w);
    D_800C3EA4->unkA230->unk320[D_800CCB04.buffer].clut = D_800D2F68[icon].alternate ? D_80059414 : D_800595D4;
    func_80076C78(&D_800C3EA4->unkA230->unk370[D_800CCB04.buffer], 0xD0, 0x33, D_800D2F68[frame].u,
                  D_800D2F68[frame].v, D_800D2F68[frame].w);
    D_800C3EA4->unkA230->unk370[D_800CCB04.buffer].clut = D_800D2F68[frame].alternate ? D_80059414 : D_800595D4;
}

/* Render the name of the item in the list cell (column, row) into a text
 * image and place it on the graphics block's +0x280 quad. */
void func_8009070C(u8 column, u8 row) {
    RECT rect;
    u8 item = D_800D2CE0[row * 2 + column];
    u32 *pixels = (u32 *)func_8008AC00(0x39);
    s32 width;

    bzero(pixels, 0x618);
    width = func_80034EAC(func_80033728(D_800D329C, item), pixels, 0x39, 0);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, pixels);
    func_80076C78(&D_800C3EA4->unkA230->unk280[D_800CCB04.buffer], 0x30, 0x42, 0, 0, width);
    func_800320E8(pixels);
}

/* Show the item list cell (column, row) of the item list or (from the
 * 800c3d70 list) the other list: frame, name and count. */
void func_8009080C(u8 column, u8 row, u8 other) {
    u8 item;

    if (other == 0) {
        item = D_800D2CE0[row * 2 + column];
    } else {
        item = D_800C3D70[row * 2 + column];
    }
    D_800C3EA4->unkA230->unk66D = 0;
    func_80090310(column, row);
    if (item != 0 && other == 0) {
        func_8009070C(column, row);
        func_800904A0(column, row);
        D_800C3EA4->unkA230->unk66D = 1;
    }
    D_800C3EA4->unkA230->buffer = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk66B = 1;
}

/* Point the four list page quads at the list image from row offset `v`
 * (offsets from 0x68 on the second image page with the alternate CLUT). */
void func_8009093C(s32 v) {
    s32 page = 0;

    if (v >= 0x68) {
        v -= 0x68;
        D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_80059414;
        D_800C3EA4->unkA230->unk50[D_800CCB04.buffer].clut = D_80059414;
        page = 0x10;
    } else {
        D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
        D_800C3EA4->unkA230->unk50[D_800CCB04.buffer].clut = D_800595D4;
    }
    func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0x30, 0x60, 0, v, 0x60, 0x68);
    func_80076CE8(&D_800C3EA4->unkA230->unk50[D_800CCB04.buffer], 0xB4, 0x60, 0x78, v, 0x60, 0x68);
    func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0x98, 0x60, page, v, 0x10, 0x68);
    func_80076CE8(&D_800C3EA4->unkA230->unkF0[D_800CCB04.buffer], 0x11C, 0x60, page | 0x40, v, 0x10, 0x68);
    D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
}

/* Animate the five-frame cursor glyph at (x, y): advance `frame` every third
 * tick. */
void func_80090B90(s32 x, s32 y, s32 *frame, u8 *ticks) {
    if (++*ticks >= 3) {
        *frame -= 1;
        if (*frame < 0) {
            *frame = 4;
        }
        *ticks = 0;
    }
    D_800D2D28->unk100 = func_80076A10(*frame + 0xE0, D_800C3EA4->unk27C8, x, y);
    D_800D2D28->unkA7 = D_800CCB04.buffer;
    D_800D2D28->unk9E = 1;
}

/* Show the member's EP and maximum EP as two digit glyphs each (no leading
 * zero). */
void func_80090C44(u8 member) {
    u16 digit;

    digit = D_800CCCE8.records[member].pilot.ep / 10;
    if (digit != 0) {
        func_80076C78(&D_800C3EA4->unkA230->unk460[D_800CCB04.buffer], 0x104, 0xC6, digit * 8 + 0x78, 0, 8);
    }
    func_80076C78(&D_800C3EA4->unkA230->unk4B0[D_800CCB04.buffer], 0x10C, 0xC6,
                  (u16)(D_800CCCE8.records[member].pilot.ep % 10) * 8 + 0x78, 0, 8);
    digit = D_800CCCE8.records[member].pilot.maxEp / 10;
    if (digit != 0) {
        func_80076C78(&D_800C3EA4->unkA230->unk500[D_800CCB04.buffer], 0x11C, 0xC6, digit * 8 + 0x78, 0, 8);
    }
    func_80076C78(&D_800C3EA4->unkA230->unk550[D_800CCB04.buffer], 0x124, 0xC6,
                  (u16)(D_800CCCE8.records[member].pilot.maxEp % 10) * 8 + 0x78, 0, 8);
}

/* Show the member's EP panel: the two EP icons (cells 6 and 5), the EP
 * digits and the EP label glyphs. */
void func_80090E7C(u8 member) {
    func_80076C78(&D_800C3EA4->unkA230->unk3C0[D_800CCB04.buffer], 0xA8, 0xA6, D_800D2F68[6].u, D_800D2F68[6].v,
                  D_800D2F68[6].w);
    D_800C3EA4->unkA230->unk3C0[D_800CCB04.buffer].clut = D_800D2F68[6].alternate ? D_80059414 : D_800595D4;
    func_80076C78(&D_800C3EA4->unkA230->unk410[D_800CCB04.buffer], 0xEC, 0xC6, D_800D2F68[5].u, D_800D2F68[5].v,
                  D_800D2F68[5].w);
    D_800C3EA4->unkA230->unk410[D_800CCB04.buffer].clut = D_800D2F68[5].alternate ? D_80059414 : D_800595D4;
    func_80090C44(member);
    func_80076A10(0x71, D_800C3EA4->unkA230->unk5A0, 0x118, 0xD1);
    D_800C3EA4->unkA230->unk66C = D_800CCB04.buffer;
}

/* Build the member's art list page: set up the graphics block and the
 * message frame, then render the name and two-digit EP cost of every art its
 * character (or its gear) knows into VRAM text images, then the EP panel. */
void func_80091064(u8 member) {
    RECT nameRect;
    RECT tensRect;
    RECT onesRect;
    RECT rowRect;
    RECT rightRect;
    RECT rect;
    TextImage images[16];
    u8 known[16];
    u8 costs[16];
    s32 i;
    u8 tens;
    u32 *digit;

    func_80077610();
    func_80076EA4();
    func_8008FC1C(0x20, 0x30, 0x98, 0x38, 0xC);
    D_800D2DB0 = (u32 *)func_8008AC00(0x39);
    bzero(D_800D2DB0, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, D_800D2DB0);
    if (D_800D32A0[member].unk1 == 0) {
        for (i = 0; i < 16; i++) {
            if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask2, i)) {
                known[i] = 1;
                costs[i] = D_800CCCE8.partyCommands[member][i + 22].cost;
            } else {
                known[i] = 0;
                costs[i] = 0;
            }
        }
    } else {
        for (i = 0; i < 16; i++) {
            if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask6, i)) {
                known[i] = 1;
                costs[i] = D_800CCCE8.gearCommands[member][i + 21].cost;
            } else {
                known[i] = 0;
                costs[i] = 0;
            }
        }
    }
    for (i = 0; i < 16; i++) {
        images[i].pixels = (u32 *)func_8008AC00(0x1B);
        bzero(images[i].pixels, 0x30C);
        rowRect.x = (i % 2) * 30 + 0x380;
        rowRect.y = (i / 2) * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        func_800769E8(&rowRect, D_800D2DB0);
        if (known[i] != 0) {
            if (D_800D32A0[member].unk1 == 0) {
                func_80034EAC(func_80033908(D_800CCCE8.records[member].pilot.characterId * 16 + i), images[i].pixels, 0x1B,
                              0);
            } else {
                func_80034EAC(func_800339FC(D_800CCCE8.records[member].pilot.gearId * 16 + i), images[i].pixels, 0x1B, 0);
            }
            nameRect.x = (i % 2) * 30 + 0x380;
            nameRect.y = (i / 2) * 16 + 0x102;
            nameRect.w = 30;
            nameRect.h = 13;
            func_800769E8(&nameRect, images[i].pixels);
        }
        if (!(i & 1)) {
            rightRect.x = 0x3C0;
            rightRect.y = (i / 2) * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            func_800769E8(&rightRect, D_800D2DB0);
        }
        tensRect.x = (i % 2) * 16 + 0x3C0;
        tensRect.y = (i / 2) * 16 + 0x102;
        tensRect.w = 6;
        tensRect.h = 13;
        tens = costs[i] / 10;
        if (tens != 0) {
            digit = D_800C3E5C[tens].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&tensRect, digit);
        onesRect.x = (i % 2) * 16 + 0x3C2;
        onesRect.y = (i / 2) * 16 + 0x102;
        onesRect.w = 6;
        onesRect.h = 13;
        if (known[i] != 0) {
            digit = D_800C3E5C[(u8)(costs[i] % 10)].pixels;
        } else {
            digit = D_800D2DB0;
        }
        func_800769E8(&onesRect, digit);
    }
    func_80090E7C(member);
    for (i = 0; i < 16; i++) {
        func_800320E8(images[i].pixels);
    }
    func_800320E8(D_800D2DB0);
    func_800716D8();
    D_800C3EA4->unkA230->unk140[0].clut = D_800595D4;
    D_800C3EA4->unkA230->unk140[1].clut = D_800595D4;
    D_800C3EA4->unkA230->unk669 = 1;
    D_800D2D28->unkB7 = 1;
}

/* Build eight glyph rows (0x66) from y + 0x38 into the +0xba8 primitives. */
void func_80091604(s32 y) {
    s32 i;
    s32 offset;

    i = 0;
    offset = 0x38;
    D_800D2D28->unkF8 = 0;
    for (; i < 8; i++) {
        D_800D2D28->unkF8 += func_80076A10(0x66, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x20, offset + y);
        offset += 8;
    }
    D_800D2D28->unkA5 = D_800CCB04.buffer;
    D_800D2D28->unk9C = 1;
}

/* Point the page title quad at entry (column, row) of the technique image
 * and, for character 1 (Fei), show the entry's cost (8009a258) as up to three
 * digit glyphs. */
void func_800916D4(u8 column, u8 row, u8 member) {
    u32 index;
    u32 cellX;
    u32 cellY;
    s32 i;
    s32 x;
    u8 digit;

    index = row * 2 + column;
    cellY = index / 2;
    cellX = index - cellY * 2;
    func_80076CE8(&D_800C3EA4->unkA230->unk140[D_800CCB04.buffer], 0x20, 0xA4, cellX * 0x78, cellY * 16, 0x60, 0x10);
    D_800C3EA4->unkA230->unk66E = 0;
    if (D_800D2D24[member] == 1) {
        /* 8009a258 is called unprototyped: the command is passed and its
         * result returned unnarrowed. */
        func_8008AAA0(((s32 (*)())func_8009A258)(member, index + 0x16));
        for (i = 0, x = 0x8C; i < 3; i++) {
            digit = D_800C3CF4[i + 6];
            if (digit != 0xFF) {
                func_80076C78(&D_800C3EA4->unkA230->unk190[D_800C3EA4->unkA230->unk66E * 2 + D_800CCB04.buffer], x,
                              0xA6, digit * 8 + 0x78, 0, 8);
                D_800C3EA4->unkA230->unk66E++;
            }
            x += 8;
        }
    }
}

/* Point the art page icon quads at the images for art (column, row) of the
 * member (its gear's arts in a gear): its state icon (9 sealed, 7 flag
 * 0x1000, else 8) and its level frame (13 level 1, 21 level 2, else 12). */
void func_8009187C(u8 member, u8 column, u8 row) {
    u16 state;
    s32 icon;
    s32 frame;

    if (D_800D32A0[member].unk1 == 0) {
        state = D_800CCCE8.partyCommands[member][row * 2 + column + 22].state;
    } else {
        state = D_800CCCE8.gearCommands[member][row * 2 + column + 21].state;
    }
    if (state & 0x4000) {
        icon = 9;
    } else {
        icon = 8;
        if (state & 0x1000) {
            icon = 7;
        }
    }
    switch (state & 0xF) {
    case 0:
        frame = 12;
        break;
    case 1:
        frame = 13;
        break;
    case 2:
        frame = 21;
        break;
    default:
        frame = 12;
        break;
    }
    func_80076C78(&D_800C3EA4->unkA230->unk320[D_800CCB04.buffer], 0xDA, 0xAA, D_800D2F68[icon].u, D_800D2F68[icon].v,
                  D_800D2F68[icon].w);
    D_800C3EA4->unkA230->unk320[D_800CCB04.buffer].clut = D_800D2F68[icon].alternate ? D_80059414 : D_800595D4;
    func_80076C78(&D_800C3EA4->unkA230->unk370[D_800CCB04.buffer], 0xFE, 0xAA, D_800D2F68[frame].u,
                  D_800D2F68[frame].v, D_800D2F68[frame].w);
    D_800C3EA4->unkA230->unk370[D_800CCB04.buffer].clut = D_800D2F68[frame].alternate ? D_80059414 : D_800595D4;
}

/* Build the description of art (column, row) of the member (its gear's in
 * a gear): two text lines from the menu module block into VRAM, placed on
 * the page's two description quads. */
void func_80091B38(u8 member, u8 column, u8 row) {
    RECT rect;
    u32 *pixels;
    s32 text;
    s32 width0;
    s32 width1;

    if (D_800D32A0[member].unk1 == 0) {
        text = D_800D2D24[member] * 32 + (row * 2 + column) * 2;
    } else {
        text = D_8006D8A0.characters[D_800D2D24[member]].gearId * 32 + (row * 2 + column) * 2;
    }
    pixels = (u32 *)func_8008AC00(0x39);
    bzero(pixels, 0x618);
    width0 = func_80034EAC(func_80033728(D_800D367C, text & 0xFFFF), pixels, 0x39, 0);
    width1 = func_80034EAC(func_80033728(D_800D367C, (text & 0xFFFF) | 1), pixels, 0x39, 1);
    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x3C;
    rect.h = 13;
    func_800769E8(&rect, pixels);
    func_80076C78(&D_800C3EA4->unkA230->unk280[D_800CCB04.buffer], 0x30, 0xB6, 0, 0, width0);
    func_80076C78(&D_800C3EA4->unkA230->unk2D0[D_800CCB04.buffer], 0x30, 0xC6, 0, 0, width1);
    func_800320E8(pixels);
}

/* Open the combo/technique entry (column, row) of the member's page when its
 * character knows it (mask +2 on foot, +6 in a gear): build its graphics for
 * the current draw buffer; otherwise mark the page closed. */
void func_80091D38(member, column, row)
u8 member;
u8 column;
u8 row;
{
    u8 known = 0;

    if (D_800D32A0[member].unk1 == 0) {
        known = func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask2, column + row * 2) != 0;
    } else if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].mask6, column + row * 2)) {
        known = 1;
    }
    if (known) {
        func_800916D4(column, row, member);
        func_8009187C(member, column, row);
        func_80091B38(member, column, row);
        D_800C3EA4->unkA230->buffer = D_800CCB04.buffer;
        D_800C3EA4->unkA230->unk66B = 1;
    } else {
        D_800C3EA4->unkA230->unk66B = 0;
    }
}

/* Point the four art page quads at the art list image from row `v`. */
void func_80091EC4(u8 v) {
    D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
    D_800C3EA4->unkA230->unk50[D_800CCB04.buffer].clut = D_800595D4;
    func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0x34, 0x34, 0, v, 0x60, 0x60);
    func_80076CE8(&D_800C3EA4->unkA230->unk50[D_800CCB04.buffer], 0xB8, 0x34, 0x78, v, 0x60, 0x60);
    func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0x9C, 0x34, 0, v, 0x10, 0x60);
    func_80076CE8(&D_800C3EA4->unkA230->unkF0[D_800CCB04.buffer], 0x120, 0x34, 0x40, v, 0x10, 0x60);
    D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
}

/* Show the gear page list image on the four page quads and mark the page
 * (command window page 3) open. */
void func_8009209C(void) {
    D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
    D_800C3EA4->unkA230->unk50[D_800CCB04.buffer].clut = D_800595D4;
    func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0x36, 0x62, 0, 0, 0x60, 0x40);
    func_80076CE8(&D_800C3EA4->unkA230->unk50[D_800CCB04.buffer], 0xC2, 0x62, 0x78, 0, 0x60, 0x40);
    func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0x96, 0x62, 0, 0, 0x10, 0x40);
    func_80076CE8(&D_800C3EA4->unkA230->unkF0[D_800CCB04.buffer], 0x122, 0x62, 0x40, 0, 0x10, 0x40);
    D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk669 = 1;
    D_800D2D28->unkB7 = 3;
}

/* Set up the gear page's shaded bar and black box primitives, then build its
 * glyphs (from the 800c33b4 table) for every entry of `shown` that is not
 * 0xff into the +0x1e68 list. */
void func_80092298(u8 member, u8 *shown) {
    s32 i;
    s32 glyph;

    for (i = 0; i < 2; i++) {
        SetPolyG4(&D_800C3EA4->unkA230->unk5F0[i]);
        (D_800C3EA4->unkA230->unk5F0 + i)->r0 = 0xFF;
        (D_800C3EA4->unkA230->unk5F0 + i)->g0 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->b0 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->r1 = 0xFF;
        (D_800C3EA4->unkA230->unk5F0 + i)->g1 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->b1 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->r2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->g2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->b2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->r3 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->g3 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->b3 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->x0 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->y0 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->x1 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->y1 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->x2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->y2 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->x3 = 0;
        (D_800C3EA4->unkA230->unk5F0 + i)->y3 = 0;
    }
    for (i = 0; i < 2; i++) {
        SetPolyF4(&D_800C3EA4->unkA230->unk638[i]);
        (D_800C3EA4->unkA230->unk638 + i)->r0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->g0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->b0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->x0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->y0 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->x1 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->y1 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->x2 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->y2 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->x3 = 0;
        (D_800C3EA4->unkA230->unk638 + i)->y3 = 0;
    }
    D_800D2D28->unkFC = 0;
    for (glyph = 0; glyph < 16; glyph++) {
        if (shown[glyph] != 0xFF) {
            D_800D2D28->unkFC += func_80076A10(D_800C33B4[glyph], &D_800C3EA4->unk1E68[D_800D2D28->unkFC * 2],
                                               D_800C33C4[glyph], D_800C3404[glyph]);
        }
    }
    D_800D2D28->unkA6 = D_800CCB04.buffer;
    D_800D2D28->unk9D = 1;
}

/* Build the member's gear page: set up the graphics block, render the name
 * and two-digit count of each of the seven gear parts in `ids` (0xff none)
 * and the fixed eighth entry (system text 10) into VRAM text images, then
 * the page glyphs and quads. Nonmatching: the original keeps the name
 * rectangle width 30 in $t0 on both paths. */
#ifdef NON_MATCHING
void func_80092784(u8 member, u8 *ids, u8 *counts) {
    RECT nameRect;
    RECT tensRect;
    RECT onesRect;
    RECT rowRect;
    RECT rightRect;
    RECT rect;
    TextImage images[8];
    s32 i;
    u8 tens;
    u32 *digit;

    func_80077610();
    func_80076EA4();
    D_800D2DB0 = (u32 *)func_8008AC00(0x39);
    bzero(D_800D2DB0, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, D_800D2DB0);
    for (i = 0; i < 8; i++) {
        images[i].pixels = (u32 *)func_8008AC00(0x1B);
        bzero(images[i].pixels, 0x30C);
        rowRect.x = (i % 2) * 30 + 0x380;
        rowRect.y = (i / 2) * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        func_800769E8(&rowRect, D_800D2DB0);
        if (i != 7) {
            if (ids[i] != 0xFF) {
                func_80034EAC(func_80033784(D_800D2D24[member], ids[i]), images[i].pixels, 0x1B, 0);
                nameRect.x = (i % 2) * 30 + 0x380;
                nameRect.y = (i / 2) * 16 + 0x102;
                nameRect.w = 30;
                nameRect.h = 13;
                func_800769E8(&nameRect, images[i].pixels);
            }
        } else {
            func_80034EAC(func_800338D8(10), images[7].pixels, 0x1B, 0);
            nameRect.x = 0x39E;
            nameRect.y = 0x132;
            nameRect.w = 30;
            nameRect.h = 13;
            func_800769E8(&nameRect, images[7].pixels);
        }
        if (!(i & 1)) {
            rightRect.x = 0x3C0;
            rightRect.y = (i / 2) * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            func_800769E8(&rightRect, D_800D2DB0);
        }
        tensRect.x = (i % 2) * 16 + 0x3C0;
        tensRect.y = (i / 2) * 16 + 0x102;
        tensRect.w = 6;
        tensRect.h = 13;
        if (i != 7) {
            tens = counts[i] / 10;
            if (tens != 0) {
                digit = D_800C3E5C[tens].pixels;
            } else {
                digit = D_800D2DB0;
            }
            func_800769E8(&tensRect, digit);
            onesRect.x = (i % 2) * 16 + 0x3C2;
            onesRect.y = (i / 2) * 16 + 0x102;
            onesRect.w = 6;
            onesRect.h = 13;
            if (ids[i] != 0xFF) {
                func_800769E8(&onesRect, D_800C3E5C[(u8)(counts[i] % 10)].pixels);
            } else {
                func_800769E8(&onesRect, D_800D2DB0);
            }
        } else {
            func_800769E8(&tensRect, D_800D2DB0);
            onesRect.x = 0x3D2;
            onesRect.y = 0x132;
            onesRect.w = 6;
            onesRect.h = 13;
            func_800769E8(&onesRect, D_800D2DB0);
        }
    }
    func_80092298(member, ids);
    for (i = 0; i < 8; i++) {
        func_800320E8(images[i].pixels);
    }
    func_800320E8(D_800D2DB0);
    func_8009209C();
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80092784);
#endif

/* Build the combo entry display: the AP count as one or two digit glyphs,
 * the button glyph of each entered step with separators, and the remaining
 * AP bar (4 pixels per point) on the shaded bar and black box. */
void func_80092B74(u8 member, u8 ap) {
    u8 digits[2];
    u8 tens;
    s32 i;

    digits[0] = tens = ap / 10;
    digits[1] = ap - tens * 10;
    D_800D2D28->unkF8 = 0;
    if (digits[0] != 0) {
        D_800D2D28->unkF8 += func_80076A10(digits[0] + 0x67, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x56, 0x38);
    }
    D_800D2D28->unkF8 += func_80076A10(digits[1] + 0x67, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], 0x5E, 0x38);
    for (i = 0; i < 7; i++) {
        if (D_800C3EAC->combo[i] == 0xFF) {
            break;
        }
        D_800D2D28->unkF8 += func_80076A10(D_800C3DE0[i] + 0x39, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2], i * 32 + 0x2A, 0x4A);
        if (i != 0) {
            D_800D2D28->unkF8 += func_80076A10(0xA, &D_800C3EA4->unkBA8[D_800D2D28->unkF8 * 2],
                                               (i - 1) * 32 + 0x3A, 0x4A);
        }
    }
    D_800D2D28->unkA5 = D_800CCB04.buffer;
    D_800D2D28->unk9C = 1;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->x0 = 0x80;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->y0 = 0x34;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->x1 = ap * 4 + 0x80;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->y1 = 0x34;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->x2 = 0x80;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->y2 = 0x3C;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->x3 = ap * 4 + 0x80;
    (D_800C3EA4->unkA230->unk5F0 + D_800CCB04.buffer)->y3 = 0x3C;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->x0 = ap * 4 + 0x80;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->y0 = 0x34;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->x1 = 0xF0;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->y1 = 0x34;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->x2 = ap * 4 + 0x80;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->y2 = 0x3C;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->x3 = 0xF0;
    (D_800C3EA4->unkA230->unk638 + D_800CCB04.buffer)->y3 = 0x3C;
    D_800C3EA4->unkA230->unk66C = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk66F = 1;
}

/* Build the member's gear value page: the four entries' names (gear text
 * gearId * 4 + entry) and up to four-digit values (no leading zeros) into
 * VRAM text images; each shown digit is taken off the value. */
void func_800930AC(u8 member, u8 *present, u16 *values) {
    RECT nameRect;
    RECT onesRect;
    RECT digitRect;
    RECT rowRect;
    RECT rightRect;
    RECT rect;
    TextImage images[4];
    u16 divisors[3];
    u8 shown;
    s32 i;
    s32 j;
    u8 digit;

    divisors[0] = 1000;
    divisors[1] = 100;
    divisors[2] = 10;
    func_80077610();
    func_80076EA4();
    D_800D2DB0 = (u32 *)func_8008AC00(0x39);
    bzero(D_800D2DB0, 0x618);
    rect.x = 0x3C0;
    rect.w = 0x3C;
    rect.y = 0;
    rect.h = 0xD;
    func_800769E8(&rect, D_800D2DB0);
    for (i = 0; i < 4; i++) {
        images[i].pixels = (u32 *)func_8008AC00(0x1B);
        bzero(images[i].pixels, 0x30C);
        rowRect.x = 0x380;
        rowRect.y = i * 16 + 0x100;
        rowRect.w = 0x1B;
        rowRect.h = 16;
        func_800769E8(&rowRect, D_800D2DB0);
        if (present[i] != 0xFF) {
            func_80034EAC(func_80033A8C(D_800CCCE8.records[member].pilot.gearId * 4 + i), images[i].pixels, 0x1B, 0);
            nameRect.x = 0x380;
            nameRect.y = i * 16 + 0x102;
            nameRect.w = 30;
            nameRect.h = 13;
            func_800769E8(&nameRect, images[i].pixels);
        }
        rightRect.x = 0x3C0;
        rightRect.y = i * 16 + 0x100;
        rightRect.w = 0x1B;
        rightRect.h = 16;
        func_800769E8(&rightRect, D_800D2DB0);
        shown = 0;
        for (j = 0; j < 3; j++) {
            digitRect.x = j * 2 + 0x3C0;
            digitRect.y = i * 16 + 0x102;
            digitRect.w = 6;
            digitRect.h = 13;
            digit = values[i] / divisors[j];
            if (digit != 0 || shown) {
                func_800769E8(&digitRect, D_800C3E5C[digit].pixels);
                shown = 1;
                values[i] -= digit * divisors[j];
            } else {
                func_800769E8(&digitRect, D_800D2DB0);
            }
        }
        onesRect.x = 0x3C6;
        onesRect.y = i * 16 + 0x102;
        onesRect.w = 6;
        onesRect.h = 13;
        if (present[i] != 0xFF) {
            func_800769E8(&onesRect, D_800C3E5C[(u16)(values[i] % 10)].pixels);
        } else {
            func_800769E8(&onesRect, D_800D2DB0);
        }
    }
    for (i = 0; i < 4; i++) {
        func_800320E8(images[i].pixels);
    }
    func_800320E8(D_800D2DB0);
    D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
    func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0x94, 0x54, 0, 0, 0x60, 0x40);
    func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0xFC, 0x54, 0, 0, 0x20, 0x40);
    D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
    D_800C3EA4->unkA230->unk669 = 1;
    D_800D2D28->unkB7 = 4;
}

/* Point the gear page `kind` quads at their images: the page title cell, the
 * command's state icon (9 sealed, 7 flag 0x1000, else 8) and its level frame
 * (13 for level 1, 21 for level 2, else 12), each with its CLUT. */
void func_80093578(u8 member, u8 kind) {
    u16 state;
    s32 icon;
    s32 frame;

    func_80076CE8(&D_800C3EA4->unkA230->unk140[D_800CCB04.buffer], 0x7C, 0xA4, 0, kind * 16, 0x60, 0x10);
    state = D_800CCCE8.gearCommands[member][kind + 37].state;
    if (state & 0x4000) {
        icon = 9;
    } else {
        icon = 8;
        if (state & 0x1000) {
            icon = 7;
        }
    }
    switch (state & 0xF) {
    case 0:
        frame = 12;
        break;
    case 1:
        frame = 13;
        break;
    case 2:
        frame = 21;
        break;
    default:
        frame = 12;
        break;
    }
    func_80076C78(&D_800C3EA4->unkA230->unk320[D_800CCB04.buffer], 0x24, 0xA6, D_800D2F68[icon].u, D_800D2F68[icon].v,
                  D_800D2F68[icon].w);
    D_800C3EA4->unkA230->unk320[D_800CCB04.buffer].clut = D_800D2F68[icon].alternate ? D_80059414 : D_800595D4;
    func_80076C78(&D_800C3EA4->unkA230->unk370[D_800CCB04.buffer], 0x48, 0xA6, D_800D2F68[frame].u,
                  D_800D2F68[frame].v, D_800D2F68[frame].w);
    D_800C3EA4->unkA230->unk370[D_800CCB04.buffer].clut = D_800D2F68[frame].alternate ? D_80059414 : D_800595D4;
}

/* Build the name of the member's gear page `kind` (two text lines from the
 * file 3 block) into VRAM and point the page's two title quads at them. */
void func_8009382C(u8 member, u8 kind) {
    RECT rect;
    u32 *pixels;
    s32 text;
    s32 width0;
    s32 width1;

    text = (D_800CCCE8.records[member].pilot.gearId * 4 + kind) * 2;
    pixels = (u32 *)func_8008AC00(0x39);
    bzero(pixels, 0x618);
    width0 = func_80034EAC(func_80033728(D_800C3DE8, text & 0xFFFF), pixels, 0x39, 0);
    width1 = func_80034EAC(func_80033728(D_800C3DE8, (text & 0xFFFF) | 1), pixels, 0x39, 1);
    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x3C;
    rect.h = 13;
    func_800769E8(&rect, pixels);
    func_80076C78(&D_800C3EA4->unkA230->unk280[D_800CCB04.buffer], 0x88, 0xB6, 0, 0, width0);
    func_80076C78(&D_800C3EA4->unkA230->unk2D0[D_800CCB04.buffer], 0x88, 0xC6, 0, 0, width1);
    func_800320E8(pixels);
}

/* Open the member's `kind` page (0-2 the special pages, 3 the item page)
 * when its character has it and no status seals it; the page's graphics
 * are built for the current draw buffer. Otherwise mark the page closed. */
void func_800939CC(u8 member, u8 kind) {
    u16 seals[4];

    seals[0] = D_800C3234[13];
    seals[1] = D_800C3234[14];
    seals[2] = D_800C3234[15];
    seals[3] = D_800C3234[3];
    if (func_80089C6C(D_8006ECF4[D_800D2D24[member]].flags1A, kind) &&
        !(D_800CCCE8.records[member].pilot.status7A & seals[kind])) {
        func_80093578(member, kind);
        func_8009382C(member, kind);
        D_800C3EA4->unkA230->buffer = D_800CCB04.buffer;
        D_800C3EA4->unkA230->unk66B = 1;
    } else {
        D_800C3EA4->unkA230->unk66B = 0;
    }
}

/* For party character 4 with no command page open: show the two equipped
 * items' (in a gear: parts') names and durability (two digits, no leading
 * zero) in window 0. */
void func_80093B08(u8 member) {
    RECT nameRect;
    RECT onesRect;
    RECT digitRect;
    RECT rowRect;
    RECT rightRect;
    RECT rect; /* unused */
    TextImage images[2];
    u16 divisors[2];
    u8 slots[2];
    u16 values[2];
    u8 shown;
    s32 i;
    s32 j;
    u8 digit;

    if (D_800D2D24[member] == 4 && D_800D2D28->unkB7 == 0) {
        divisors[0] = 100;
        divisors[1] = 10;
        if (D_800D32A0[member].unk1 == 0) {
            slots[0] = D_8006D8A0.characters[D_800D2D24[member]].entryItems[0];
            slots[1] = D_8006D8A0.characters[D_800D2D24[member]].entryItems[3];
            values[0] = D_8006F8BA[slots[0]];
            values[1] = D_8006F8BA[slots[1]];
        } else {
            slots[0] = D_8006D8A0.gears[D_8006D8A0.characters[D_800D2D24[member]].gearId].partItems[0];
            slots[1] = D_8006D8A0.gears[D_8006D8A0.characters[D_800D2D24[member]].gearId].partItems[3];
            values[0] = D_8006F8EA[slots[0]];
            values[1] = D_8006F8EA[slots[1]];
        }
        func_80077610();
        func_80076EA4();
        D_800D2DB0 = (u32 *)func_8008AC00(0x39);
        bzero(D_800D2DB0, 0x618);
        for (i = 0; i < 2; i++) {
            images[i].pixels = (u32 *)func_8008AC00(0x1B);
            bzero(images[i].pixels, 0x30C);
            rowRect.x = 0x380;
            rowRect.y = i * 16 + 0x100;
            rowRect.w = 0x1B;
            rowRect.h = 16;
            func_800769E8(&rowRect, D_800D2DB0);
            if (slots[i] != 0) {
                func_80034EAC(func_80033848(slots[i]), images[i].pixels, 0x1B, 0);
                nameRect.x = 0x380;
                nameRect.y = i * 16 + 0x102;
                nameRect.w = 30;
                nameRect.h = 13;
                func_800769E8(&nameRect, images[i].pixels);
            }
            rightRect.x = 0x3C0;
            rightRect.y = i * 16 + 0x100;
            rightRect.w = 0x1B;
            rightRect.h = 16;
            func_800769E8(&rightRect, D_800D2DB0);
            shown = 0;
            for (j = 0; j < 2; j++) {
                digitRect.x = j * 2 + 0x3C0;
                digitRect.y = i * 16 + 0x102;
                digitRect.w = 6;
                digitRect.h = 13;
                digit = values[i] / divisors[j];
                if (digit != 0 || shown) {
                    func_800769E8(&digitRect, D_800C3E5C[digit].pixels);
                    shown = 1;
                    values[i] -= digit * divisors[j];
                } else {
                    func_800769E8(&digitRect, D_800D2DB0);
                }
            }
            onesRect.x = 0x3C4;
            onesRect.y = i * 16 + 0x102;
            onesRect.w = 6;
            onesRect.h = 13;
            if (slots[i] != 0) {
                func_800769E8(&onesRect, D_800C3E5C[(u16)(values[i] % 10)].pixels);
            } else {
                func_800769E8(&onesRect, D_800D2DB0);
            }
        }
        for (i = 0; i < 2; i++) {
            func_800320E8(images[i].pixels);
        }
        func_800320E8(D_800D2DB0);
        D_800C3EA4->unkA230->unk0[D_800CCB04.buffer].clut = D_800595D4;
        func_80076CE8(&D_800C3EA4->unkA230->unk0[D_800CCB04.buffer], 0xA0, 0xAC, 0, 0, 0x60, 0x20);
        func_80076CE8(&D_800C3EA4->unkA230->unkA0[D_800CCB04.buffer], 0x104, 0xAC, 0, 0, 0x18, 0x20);
        D_800C3EA4->unkA230->unk668 = D_800CCB04.buffer;
        D_800C3EA4->unkA230->unk669 = 1;
        func_8008F8F4(0, 0x98, 0xA8, 0x8C, 0x28, 0, 1);
        D_800D2D28->unkB7 = 5;
        D_800D2D28->unk8E = 1;
    }
}

/* For party character 4: close its command window and, with `release`,
 * its extra resources. */
void func_8009413C(u8 member, u8 release) {
    if (D_800D2D24[member] == 4) {
        D_800D2D28->unkB7 = 0;
        D_800D2D28->windows[0] = 0;
        if (release != 0) {
            func_8007FB70(member);
        }
    }
}

/* Resolve the committed action: select the attacker and its command
 * descriptor (a gear's goes to 8009c198), then for every target in the
 * target mask run the descriptor's formula, the post-adjustment and the
 * status step its flags ask for; finally publish the command, hold the
 * attacker's commands and raise the used command's use count. Returns the
 * resolve status, except on the two gear paths, which return without a
 * value. */
u8 func_800941A4(void) {
    u16 bit;
    u8 member;

    func_80097D08();
    D_800D2DB8 = 0;
    D_800C34AE = 0;
    D_800C3E04 = D_800C34B0->attackerIndex;
    D_800C3E00 = &D_800C34B0->records[D_800C3E04];
    D_800D2D6C = &D_800C34B0->records[D_800C3E04].gear;
    if (D_800C34B0->records[D_800C34B0->attackerIndex].flags15A & 0x80) {
        func_8009C198();
        func_8009AB38(D_800C3E04);
        return;
    }
    if (D_800C3E04 < 3) {
        D_800C3DFC = &D_800C34B0->partyCommands[D_800C3E04][D_800C34B0->commandIndex];
    } else {
        D_800C3DFC = &D_800C34B0->enemyCommands[D_800C34B0->commandIndex];
    }
    func_8009AC48(D_800C3E04, 1);
    if (D_800C3DFC->flagsA & 0x10) {
        func_8009C198();
        func_8009AB38(D_800C3E04);
        func_80094C78();
        return;
    }
    D_800D2DC4 = 0;
    if ((D_800C3DFC->flagsA & 0x100) && D_800C3E00->pilot.characterId == 1) {
        func_80096824();
    }
    if (D_800C3E00->pilot.characterId == 4 && (D_800C34B0->commandIndex == 4 || D_800C34B0->commandIndex == 5)) {
        D_800C3DFC->attributes[2] = D_800C3E00->pilot.entries[D_800C34B0->commandIndex - 3].value4;
    }
    if ((D_800C3E00->pilot.status80 & 0x20) && (D_800D2C94.targets & 7) && D_800D2C94.targets < 7) {
        for (member = 0; member < 3; member++) {
            if (D_800CCCE8.records[member].pilot.characterId == 3) {
                D_800D2C94.targets = 1 << member;
            }
        }
    }
    bit = 1;
    for (D_800C3E50 = 0; D_800C3E50 < 11; D_800C3E50++, bit <<= 1) {
        if (bit & D_800C34B0->targetMask) {
            D_800C3E34 = &D_800C34B0->records[D_800C3E50];
            D_800D2DC8 = &D_800C34B0->records[D_800C3E50].gear;
            D_800C3D60 = &D_800C34B0->records[D_800C3E50].field148;
            if (D_800C3E50 < 3) {
                func_800968C0();
            }
            D_800C348C[D_800C3DFC->formula]();
            func_800946F4();
            if (D_800C34B0->resultCode[D_800C3E50] == 0) {
                if (D_800C3DFC->flagsA & 0x800) {
                    func_80095B44();
                } else if (D_800C3DFC->flagsA & 0x4000) {
                    func_80095A78();
                } else if (D_800C3DFC->flagsA & 4) {
                    func_800958D8();
                }
            }
            if (D_800C3DFC->flagsA & 1) {
                D_800C34B0->shownCommand = D_800C3DFC->name;
            } else {
                D_800C34B0->shownCommand = D_800C34B0->commandIndex;
            }
        }
    }
    func_80099FB0();
    func_8009AB38(D_800C3E04);
    if (D_800C3E00->pilot.characterId == 4 && D_800C34AE == 0) {
        func_8009AFD8();
    }
    if (D_800C3E04 < 3 && D_800C34B0->commandIndex < 7) {
        if (D_800C3E00->pilot.useCounts[D_800C34B0->commandIndex] <= 0xFDE7) {
            D_800C3E00->pilot.useCounts[D_800C34B0->commandIndex] +=
                D_800C3E00->pilot.pad55 + D_800C3E00->pilot.padA1[0];
        }
    }
    func_80094C78();
    return D_800D2DB8;
}

/* Per-target follow-up of a party or enemy action. On a hit (result 0): a
 * party attacker's counter +0x3a rises when the damage equals the target's
 * HP; the target loses status80 bit 0x1000 and, on 70%, 0x2000; an ether
 * command (flags 0x100) gets result 2 against flags36 0x4000 (or 0x2000
 * without attribute 2 bits); flags32 0x80 halves (60%, 80% for character
 * 0) or raises damage by half; flags32 0x20 turns it on the attacker; a
 * party attack clears the target's status7c bit 2 (restoring status7a); a
 * blow at least character 3's HP clears every enemy's status80 bit 0x20.
 * Then flags36 0x8000 swaps results 0/5 and 2, status84 0x80 doubles
 * result-2 damage and status88 0x200 nullifies result-1 damage. */
void func_800946F4(void) {
    s32 chance;
    u8 slot;

    if (D_800C34B0->resultCode[D_800C3E50] == 0) {
        if (D_800C34B0->damage[D_800C3E50] == D_800C3E34->pilot.hp && D_800C3E04 < 3) {
            if (++D_800C3E00->pilot.field3A > 0xFDE8) {
                D_800C3E00->pilot.field3A--;
            }
        }
        D_800C3E34->pilot.status80 &= ~0x1000;
        if ((D_800C3E34->pilot.status80 & 0x2000) && rand() % 100 < 70) {
            D_800C3E34->pilot.status80 &= ~0x2000;
        }
        if (D_800C3DFC->flagsA & 0x100) {
            if (D_800C3E34->pilot.flags36 & 0x4000) {
                D_800C34B0->resultCode[D_800C3E50] = 2;
            }
            if ((D_800C3E34->pilot.flags36 & 0x2000) && !(D_800C3DFC->attributes[2] & 0xF)) {
                D_800C34B0->resultCode[D_800C3E50] = 2;
            }
        }
        if (D_800C3E34->pilot.flags32 & 0x80) {
            chance = 60;
            if (D_800C3E34->pilot.characterId == 0) {
                chance = 80;
            }
            if (rand() % 100 < chance) {
                D_800C34B0->damage[D_800C3E50] >>= 1;
            } else {
                D_800C34B0->damage[D_800C3E50] += D_800C34B0->damage[D_800C3E50] >> 1;
            }
        }
        if (D_800C3E34->pilot.flags32 & 0x20) {
            D_800C34B0->resultCode[D_800C3E04] = 0;
            D_800C34B0->damage[D_800C3E04] = D_800C34B0->damage[D_800C3E50];
        }
        if (D_800C3E04 < 3 && D_800C3E04 != D_800C3E50) {
            if (D_800C34B0->records[D_800C3E50].pilot.status7C & 2) {
                D_800C34B0->records[D_800C3E50].pilot.status7C &= ~2;
                D_800C34B0->records[D_800C3E50].pilot.status7A = D_800C3AA4[D_800C3E50];
            }
        }
        if (D_800C3E34->pilot.characterId == 3 && (u32)D_800D2C54[D_800C3E50] >= D_800C3E34->pilot.hp) {
            for (slot = 3; slot < 11; slot++) {
                D_800CCCE8.records[slot].pilot.status80 &= ~0x20;
            }
        }
    }
    if (D_800C3E34->pilot.flags36 & 0x8000) {
        switch (D_800C34B0->resultCode[D_800C3E50]) {
        case 0:
        case 5:
            D_800C34B0->resultCode[D_800C3E50] = 2;
            break;
        case 2:
            D_800C34B0->resultCode[D_800C3E50] = 0;
            break;
        }
    }
    if ((D_800C3E34->pilot.status84.half.permanent & 0x80) && D_800C34B0->resultCode[D_800C3E50] == 2) {
        D_800C34B0->damage[D_800C3E50] *= 2;
    }
    if ((D_800C3E34->pilot.status88.half.permanent & 0x200) && D_800C34B0->resultCode[D_800C3E50] == 1) {
        D_800C34B0->damage[D_800C3E50] = 0;
    }
}

/* Add the damage dealt to the target to each hit party member's running
 * total (+0x5f60 with record flag 0x80 at +0x15a, else +0x5f54). */
void func_80094C78(void) {
    u8 member;

    for (member = 0; member < 3; member++) {
        if (D_800C34B0->resultCode[member] == 0) {
            if (D_800C34B0->records[member].flags15A & 0x80) {
                D_800C34B0->field5F60[member] += D_800C34B0->damage[D_800C3E50];
            } else {
                D_800C34B0->field5F54[member] += D_800C34B0->damage[D_800C3E50];
            }
        }
    }
}

/* With one party member out of action (status7c 0xc000) and two carrying
 * status7c bit 2, or two and one, clear bit 2 on the party and restore their
 * status7a. */
void func_80094D24(void) {
    u8 member;
    u8 down;
    u8 marked;

    down = 0;
    for (member = 0; member < 3; member++) {
        if (D_800C34B0->records[member].pilot.status7C & 0xC000) {
            down++;
        }
    }
    marked = 0;
    for (member = 0; member < 3; member++) {
        if (D_800C34B0->records[member].pilot.status7C & 2) {
            marked++;
        }
    }
    if (down == 1 && marked == 2) {
        for (member = 0; member < 3; member++) {
            if (D_800C34B0->records[member].pilot.status7C & 2) {
                D_800C34B0->records[member].pilot.status7C &= ~2;
                D_800C34B0->records[member].pilot.status7A = D_800C3AA4[member];
            }
        }
    }
    if (down == 2 && marked == 1) {
        for (member = 0; member < 3; member++) {
            if (D_800C34B0->records[member].pilot.status7C & 2) {
                D_800C34B0->records[member].pilot.status7C &= ~2;
                D_800C34B0->records[member].pilot.status7A = D_800C3AA4[member];
            }
        }
    }
}

/* Formula type 0 (physical and ether damage): none against an immune target
 * (flags34 0x8000, 0x4000 for ether); otherwise attack and defense adjusted
 * by both sides' statuses and the command's attributes, scaled 4:3 (5:4 for
 * ether), by the power / 20 for kinds 0-1, randomised, then shaped by the hit
 * outcome and clamped to 0-9999. */
void func_80094EE4(void) {
    u16 attack;
    u16 defense;
    s8 hit;
    u8 power;
    s32 attackScale;
    s32 defenseScale;
    s32 amount;
    s32 immune;
    s32 kind;

    power = D_800C3DFC->power;
    if (D_800C3DFC->flagsA & 0x100) {
        immune = D_800C3E34->pilot.flags34 & 0x4000;
    } else {
        immune = D_800C3E34->pilot.flags34 & 0x8000;
    }
    if (immune) {
        D_800C34B0->resultCode[D_800C3E50] = 0;
        return;
    }
    hit = func_80096AB8();
    attack = func_80096FBC();
    defense = func_80097610();
    if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 8) {
        attack += attack / 5;
    }
    if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 2) {
        attack += attack / 10;
    }
    if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 4) {
        attack -= attack / 5;
    }
    if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 1) {
        attack -= attack / 10;
    }
    if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 4) {
        defense += defense / 5;
    }
    if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 1) {
        defense += defense / 10;
    }
    if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 8) {
        defense -= defense / 5;
    }
    if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 2) {
        defense -= defense / 10;
    }
    if (D_800C3DFC->attributes[2] & 0x10) {
        if (!(D_800C3E34->pilot.status82 & 0x40)) {
            D_800C3E34->pilot.status80 |= 0x40;
        }
        if ((D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) & 0x4000) {
            D_800D2C88[D_800C3E04] = 3;
            D_800D2C54[D_800C3E04] = (u16)(D_800C3E00->pilot.maxEp / 10) * 2;
        }
        if ((D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) & 0x1000) {
            D_800D2C88[D_800C3E04] = 2;
            D_800D2C54[D_800C3E04] = (u16)(D_800C3E00->pilot.maxHp / 10) * 2;
        }
    }
    if ((D_800C3DFC->attributes[2] & 0x20) && !(D_800C3E34->pilot.status82 & 0x80)) {
        D_800C3E34->pilot.status80 |= 0x80;
    }
    if (D_800C3E34->pilot.status80 & 0x40) {
        defense -= defense >> 2;
        D_800C3E34->pilot.status80 &= ~0x40;
    }
    if (D_800C3E00->pilot.status80 & 0x80) {
        attack -= attack >> 2;
        D_800C3E00->pilot.status80 &= ~0x80;
    }
    if (D_800C3DFC->flagsA & 0x400) {
        power = 20;
    }
    func_80096494(&attack, &defense, &hit);
    attackScale = 5;
    if (D_800C3DFC->flagsA & 0x100) {
        defenseScale = 4;
    } else {
        attackScale = 4;
        defenseScale = 3;
    }
    if (defense != 0) {
        amount = attackScale * attack - defenseScale * defense;
    } else {
        amount = attackScale * attack;
    }
    kind = D_800C3DFC->amountKind;
    if (kind >= 0) {
        if (kind < 2) {
            amount = power * amount / 20;
        }
    }
    if (amount <= 0) {
        amount = 0;
    } else if (amount < 10) {
        amount += rand() % 2;
    } else {
        amount += rand() % (amount / 10 + 2);
    }
    switch (hit) {
    case 1:
        if (amount > 0) {
            D_800C34B0->resultCode[D_800C3E50] = 0;
        } else {
            amount = 1;
            D_800C34B0->resultCode[D_800C3E50] = 0;
        }
        break;
    case 2:
        amount /= 2;
        D_800C34B0->resultCode[D_800C3E50] = 5;
        break;
    case 3:
        amount = 0;
        D_800C34B0->resultCode[D_800C3E50] = 4;
        break;
    case 4:
        D_800C34B0->resultCode[D_800C3E50] = 2;
        break;
    case 5:
        D_800C34B0->resultCode[D_800C3E50] = 7;
        break;
    }
    if (D_800D2DC4 != 0 && (D_800C3DFC->flagsA & 0x100) && amount != 0) {
        amount /= 3;
    }
    if (amount >= 10000) {
        amount = 9999;
    }
    if (amount < 0) {
        amount = 0;
    }
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* Formula: amount = attacker +0x5b times the descriptor's +0x11 (doubled
 * with attacker +0x8a bit 0x2000), scaled 0.7 / 1.3 by the target's
 * +0x8c|+0x8e bits 0x100 / 0x200, none for a +0x15a 0x80 target; code 2. */
void func_80095690(void) {
    s16 amount = D_800C3E00->pilot.accuracy * D_800C3DFC->power;
    u16 status;

    if (D_800C3E00->pilot.status88.half.permanent & 0x2000) {
        amount *= 2;
    }
    status = D_800C3E34->pilot.status8C.half.active | D_800C3E34->pilot.status8C.half.permanent;
    if (status & 0x100) {
        amount = amount * 7 / 10;
    }
    if (status & 0x200) {
        amount = amount * 13 / 10;
    }
    if (D_800CCCE8.records[D_800C3E50].flags15A & 0x80) {
        amount = 0;
    }
    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* The target defends: a +0x56 state 2 target first leaves it (8009ac48),
 * its status words clear, result code 2 and a tenth of its +0x4e times the
 * descriptor's +0x11 as the amount; its timer is held. Nonmatching: the
 * original addresses the held mask (800d2c9e) with its own %hi/%lo on the
 * load and the store instead of one address register. */
#ifdef NON_MATCHING
void func_800957D8(void) {
    if (D_800CCCE8.records[D_800C3E50].pilot.characterId == 2) {
        func_8009AC48(D_800C3E50, 1);
    }
    D_800C3E34->pilot.status7C = 0;
    D_800C3E34->pilot.status80 = 0;
    D_800C3E34->pilot.status84.half.active = 0;
    D_800C3E34->pilot.status88.half.active = 0;
    D_800C3E34->pilot.status8C.half.active = 0;
    D_800D2C88[D_800C3E50] = 2;
    D_800D2C54[D_800C3E50] = (D_800C3E34->pilot.maxHp * D_800C3DFC->power) / 10;
    D_800D2C94.held |= 1 << D_800C3E50;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_800957D8);
#endif

/* With the command's chance (+0x1c in percent) and kind 0x6e, clear the
 * target's status words named by the command's bits 0x8000-0x400 (message
 * 0x3a). */
void func_800958D8(void) {
    if (rand() % 100 <= D_800C3DFC->field1C && D_800C3DFC->field1D == 0x6E) {
        if (D_800C3DFC->field1E & 0x8000) {
            D_800C3E34->pilot.status84.half.active = 0;
        }
        if (D_800C3DFC->field1E & 0x4000) {
            D_800C3E34->pilot.status84.half.permanent = 0;
        }
        if (D_800C3DFC->field1E & 0x2000) {
            D_800C3E34->pilot.status88.half.active = 0;
        }
        if (D_800C3DFC->field1E & 0x1000) {
            D_800C3E34->pilot.status88.half.permanent = 0;
        }
        if (D_800C3DFC->field1E & 0x800) {
            D_800C3E34->pilot.status8C.half.active = 0;
        }
        if (D_800C3DFC->field1E & 0x400) {
            D_800C3E34->pilot.status8C.half.permanent = 0;
        }
        D_800C34B0->message = 0x3A;
    }
}

/* Status effect of the descriptor on the target (mode +0x11, or 5 with
 * +0xa bit 0x4000); a refused status marks the target's result 6. */
void func_80095A78(void) {
    s8 accepted = func_80097964(D_800C3DFC->field1C, D_800C3DFC->field1D, D_800C3DFC->field1E);

    if (D_800C3DFC->flagsA & 0x4000) {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, 5);
    } else {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, D_800C3DFC->power);
        if (accepted != 1) {
            D_800C34B0->resultCode[D_800C3E50] = 6;
        }
    }
}

/* When 80097964 accepts the attacker's +2/+3/+0 values, run 800995a0 on the
 * target with the descriptor's +0x1d/+0x1e and mode 5. */
void func_80095B44(void) {
    if (func_80097964(D_800C3E00->pilot.entries[0].value2, D_800C3E00->pilot.entries[0].value3, D_800C3E00->pilot.entries[0].field0) == 1) {
        func_800995A0(D_800C3E50, D_800C3DFC->field1D, D_800C3DFC->field1E, 5);
    }
}

/* With the command's chance (+0x1c in percent) clear the target's statuses
 * named by the command's bits (+0x1d: 0x80 the state word except KO/down,
 * 0x40 the timer holds and 0x20 of 7a, 0x20-0x08 the active status words);
 * otherwise it misses (result 6). */
void func_80095BAC(void) {
    if (D_800C3DFC->field1C < rand() % 100) {
        D_800C34B0->resultCode[D_800C3E50] = 6;
        return;
    }
    if (D_800C3DFC->field1D & 0x80) {
        D_800C3E34->pilot.status7C &= 0xC000;
    }
    if (D_800C3DFC->field1D & 0x40) {
        D_800C3E34->pilot.status80 = 0;
        D_800C3E34->pilot.status7A &= ~0x20;
    }
    if (D_800C3DFC->field1D & 0x20) {
        D_800C3E34->pilot.status84.half.active = 0;
    }
    if (D_800C3DFC->field1D & 0x10) {
        D_800C3E34->pilot.status88.half.active = 0;
    }
    if (D_800C3DFC->field1D & 8) {
        D_800C3E34->pilot.status8C.half.active = 0;
    }
}

/* Formula type 3: on a chance roll (the attacker's +0x60 or the command's
 * +0x1c), transfer power / 20 of a maximum (attacker's or target's HP for
 * kinds 0-1, EP for 2-3; kind 5 the target's HP less one) between attacker
 * and target: both slots get the amount, HP kinds with results 2 / 0, EP
 * kinds 3 / 1 unless the target nullifies (status88 0x200). A failed roll
 * or nullified transfer is result 6. */
void func_80095D4C(void) {
    u8 chance;
    u16 base;
    u16 amount;

    switch (D_800C3DFC->chanceSource) {
    case 0:
        chance = D_800C3E00->pilot.field60;
        break;
    case 1:
        chance = D_800C3DFC->field1C;
        break;
    }
    if (chance < rand() % 100) {
        goto missed;
    }
    switch (D_800C3DFC->amountKind) {
    case 0:
        base = D_800C3E00->pilot.maxHp;
        break;
    case 1:
        base = D_800C3E34->pilot.maxHp;
        break;
    case 2:
        base = D_800C3E00->pilot.maxEp;
        break;
    case 3:
        base = D_800C3E34->pilot.maxEp;
        break;
    }
    amount = base * D_800C3DFC->power / 20;
    if (D_800C3DFC->amountKind == 5) {
        amount = D_800C3E34->pilot.hp - 1;
    }
    switch (D_800C3DFC->amountKind) {
    case 0:
    case 1:
    case 5:
        D_800C34B0->resultCode[D_800C3E04] = 2;
        D_800C34B0->resultCode[D_800C3E50] = 0;
        D_800C34B0->damage[D_800C3E04] = amount;
        D_800C34B0->damage[D_800C3E50] = amount;
        break;
    case 2:
    case 3:
        if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 0x200) {
        missed:
            D_800C34B0->resultCode[D_800C3E50] = 6;
        } else {
            D_800C34B0->resultCode[D_800C3E04] = 3;
            D_800C34B0->resultCode[D_800C3E50] = 1;
            D_800C34B0->damage[D_800C3E04] = amount;
            D_800C34B0->damage[D_800C3E50] = amount;
        }
        break;
    }
}

/* Chance roll (attacker +0x60 or the descriptor's +0x1c, by +0x18), then the
 * amount by the descriptor's kind +0x1a: target HP / power, HP - 1, the
 * attacker's missing HP, EP * 10, 1, HP, the maximum HP (capped at 9999)
 * or the target's down state (gears refuse it). */
void func_80096018(void) {
    u8 chance;

    switch (D_800C3DFC->chanceSource) {
    case 0:
        chance = D_800C3E00->pilot.field60;
        break;
    case 1:
        chance = D_800C3DFC->field1C;
        break;
    }
    if (chance < rand() % 100) {
        D_800C34B0->resultCode[D_800C3E50] = 6;
        return;
    }
    D_800C34B0->resultCode[D_800C3E50] = 0;
    switch (D_800C3DFC->amountKind) {
    case 0:
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            D_800C34B0->damage[D_800C3E50] = D_800D2DC8->hp / D_800C3DFC->power;
        } else {
            D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.hp / D_800C3DFC->power;
        }
        break;
    case 1:
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            D_800C34B0->damage[D_800C3E50] = D_800D2DC8->hp - 1;
        } else {
            D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.hp - 1;
        }
        break;
    case 2:
        if (D_800C34B0->records[D_800C3E04].flags15A & 0x80) {
            D_800C34B0->damage[D_800C3E50] = D_800D2D6C->maxHp - D_800D2D6C->hp;
        } else {
            D_800C34B0->damage[D_800C3E50] = D_800C3E00->pilot.maxHp - D_800C3E00->pilot.hp;
        }
        break;
    case 3:
        D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.ep * 10;
        break;
    case 4:
        D_800C34B0->damage[D_800C3E50] = 1;
        break;
    case 5:
        D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.hp;
        break;
    case 6:
        D_800C34B0->resultCode[D_800C3E50] = 0;
        if (D_800CCCE8.records[D_800C3E50].flags15A & 0x80) {
            D_800C34B0->damage[D_800C3E50] = D_800D2DC8->maxHp;
            if (D_800D2DC8->maxHp >= 10000) {
                D_800D2DC8->maxHp = 9999;
            }
        } else {
            D_800C34B0->damage[D_800C3E50] = D_800C3E34->pilot.maxHp;
            if (D_800C3E34->pilot.maxHp >= 10000) {
                D_800C3E34->pilot.maxHp = 9999;
            }
        }
        break;
    case 7:
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            D_800C34B0->resultCode[D_800C3E50] = 6;
            return;
        }
        D_800C3E34->pilot.status7C |= 1;
        D_800C3E34->pilot.status80 |= 1;
        break;
    }
}

#ifdef NON_MATCHING
/* Element adjustment of an attack/defense pair: the command's element (or
 * the attacker's own element status) against the target's weakness, its
 * resistance statuses (which may also force hit result 4) and its single
 * element guards, then a 20% boost for either side's +0x32 bit 0x10. */
void func_80096494(u16 *attack, u16 *defense, s8 *hit) {
    s32 ether = 0;
    u8 flag = 0;
    u8 element;
    u8 bits;
    u8 targetGear;
    u16 status;
    s8 scale;
    s32 defenseScale;

    targetGear = D_800C34B0->records[D_800C3E50].flags15A >> 7;
    element = D_800C3DFC->attributes[2] & 0x3F;
    bits = D_800C3E34->pilot.weakness & 0x3F;
    if (!(D_800C34B0->records[D_800C3E04].flags15A >> 7)) {
        status = D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent;
    } else {
        status = D_800D2D6C->status84.half.active | D_800D2D6C->status84.half.permanent;
    }
    status >>= 12;
    if (D_800C3DFC->flagsA & 0x100) {
        ether = 1;
    }
    if (element == 0 && status != 0) {
        element = status;
    }
    if (element & status) {
        flag = 1;
    }
    scale = 10;
    defenseScale = 10;
    if (element & bits) {
        scale = 15;
        if (D_800C3E34->pilot.weakness & 0x40) {
            scale = 18;
        }
        if (flag) {
            scale += 2;
        }
    }
    flag = 0;
    if (!targetGear) {
        status = D_800C3E34->pilot.status8C.half.active | D_800C3E34->pilot.status8C.half.permanent;
        bits = (status & 0xF00) >> 8;
        flag = (status >> 1) & 1;
    } else {
        status = D_800D2DC8->status84.half.active | D_800D2DC8->status84.half.permanent;
        bits = (status & 0xF00) >> 8;
        if (status & 2) {
            flag = 1;
        }
    }
    if ((element & bits) && ether) {
        scale -= 3;
        if (status & 4) {
            scale -= 3;
        }
        if (status & 8) {
            *hit = 4;
        }
    }
    if ((element & bits) && flag) {
        scale -= 3;
        if (status & 4) {
            scale -= 3;
        }
        if (status & 8) {
            *hit = 4;
        }
    }
    switch (element) {
    case 1:
        if ((bits << 8) & 0x200) {
            scale += 3;
        }
        break;
    case 2:
        if ((bits << 8) & 0x100) {
            scale += 3;
        }
        break;
    case 4:
        if ((bits << 8) & 0x800) {
            scale += 3;
        }
        break;
    case 8:
        if ((bits << 8) & 0x400) {
            scale += 3;
        }
        break;
    }
    if (scale <= 0) {
        scale = 1;
    }
    if (defenseScale == 0) {
        defenseScale = 1;
    }
    *attack = *attack * scale / 10;
    *defense = defenseScale * *defense / 10;
    if (D_800C3E00->pilot.flags32 & 0x10) {
        *attack += *attack / 5U;
    }
    if (D_800C3E34->pilot.flags32 & 0x10) {
        *defense += *defense / 5U;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80096494);
#endif

/* Ether check: unless rand % 100 falls below the attacker's +0x5b plus the
 * descriptor's +0x14, the action fails (code 0x38 at +0x5fc7). */
void func_80096824(void) {
    s32 chance = D_800C3E00->pilot.accuracy + D_800C3DFC->accuracy;

    if (rand() % 100 >= chance) {
        D_800C34B0->message = 0x38;
        D_800D2DC4 = 1;
    }
}

/* Counterattack: a target able to act (not down, turn timer free, an enemy
 * or one holding status 0x2000, with the counter status 0x1000 of +0x88)
 * answers a non-ether, non-gear attack with a 60% (character 0) or 50%
 * chance: attacker and target swap and the target's command 20 runs with
 * result 7 (message 0x34). */
void func_800968C0(void) {
    s32 chance;
    u8 slot;
    Combatant *record;

    if (D_800C3E34->pilot.status7C & 0xA000) {
        return;
    }
    if (D_800C3E34->pilot.status80 & 0x1000) {
        return;
    }
    if (D_800C3E04 < 3 && !(D_800C3E34->pilot.status80 & 0x2000)) {
        return;
    }
    if (!(D_800C3E34->pilot.status88.half.active & 0x1000)) {
        return;
    }
    if (D_800C3DFC->flagsA & 0x2000) {
        return;
    }
    if (D_800C3DFC->flagsA & 0x100) {
        return;
    }
    if (D_800C34B0->records[D_800C3E04].flags15A & 0x80) {
        return;
    }
    chance = 50;
    if (D_800C3E34->pilot.characterId == 0) {
        chance = 60;
    }
    if (rand() % 100 <= chance && D_800C3DFC->formula != 2) {
        slot = D_800C3E04;
        D_800C3E04 = D_800C3E50;
        record = D_800C3E00;
        D_800C3E00 = D_800C3E34;
        D_800C3E50 = slot;
        D_800C3E34 = record;
        D_800C3DFC = &D_800C34B0->partyCommands[D_800C3E04][20];
        D_800C34B0->resultCode[D_800C3E04] = 7;
        D_800C34B0->damage[D_800C3E04] = 0;
        D_800C34B0->message = 0x34;
    }
}

/* Hit outcome of the current command on the target: 1 hit, 2 half, 3 miss,
 * 5 forced. Gear-only and weapon checks, sure-hit and never-hit statuses
 * come first; then the attacker's +0x5e plus the command's +0x15 against
 * the target's +0x5f sets a margin for the percent rolls. */
s32 func_80096AB8(void) {
    s16 bonus = 0;
    s16 guard = 0;
    u8 accuracy = D_800C3E00->pilot.field5E;
    u8 evasion = D_800C3E34->pilot.field5F;
    u16 flags;
    u16 status;
    s16 margin;
    s16 roll;

    if ((D_800C3DFC->flagsA & 0x40) && (D_800CCCE8.records[D_800C3E50].flags15A & 0x80)) {
        return 3;
    }
    if (D_800C3E00->pilot.characterId == 4) {
        if ((D_800C3DFC->itemKinds & 0x80) && D_8006F8BA[D_800C3E00->pilot.entryItems[0]] == 0) {
            D_800C34AE = 1;
            return 3;
        }
        if ((D_800C3DFC->itemKinds & 0x10) && D_8006F8BA[D_800C3E00->pilot.entryItems[3]] == 0) {
            D_800C34AE = 1;
            return 3;
        }
    }
    if (D_800C3DFC->flagsA & 0x200) {
        if (D_800C3E34->pilot.flags34 & 8) {
            return 3;
        }
        if (D_800CCCE8.records[D_800C3E50].flags15A & 0x80) {
            return 3;
        }
    }
    flags = D_800C3DFC->flagsA;
    if (flags & 0x1000) {
        return 3;
    }
    status = D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent;
    if (status & 0x100) {
        return 3;
    }
    if (D_800C3E34->pilot.status7C & 0x2000) {
        return 1;
    }
    if (D_800C3E34->pilot.status80 & 0x1000) {
        return 1;
    }
    if (flags & 0x8000) {
        return 1;
    }
    if (flags & 2) {
        return 5;
    }
    if (D_800C3E00->pilot.status7C & 0x400) {
        bonus -= 50;
    }
    if ((D_800C3E00->pilot.status84.half.active | D_800C3E00->pilot.status84.half.permanent) & 0x1000) {
        bonus += 30;
    }
    if (status & 0x800) {
        guard += 50;
    }
    margin = D_800C3DFC->hitBonus + accuracy - evasion;
    if (D_800C34B0->records[D_800C3E50].flags15A & 1) {
        if (rand() % 100 < 95) {
            return 2;
        }
        return 1;
    }
    if (status & 0x20) {
        roll = rand() % 100 - margin;
        if (roll >= 50) {
            return 3;
        }
        return 1;
    }
    if ((D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent) & 0x40) {
        roll = rand() % 100 - margin;
        if (roll >= 50) {
            return 2;
        }
        return 1;
    }
    roll = rand() % 100 - margin;
    if (roll >= (s16)(bonus - (s16)(guard - 90))) {
        return 3;
    }
    roll = rand() % 100 - margin;
    if (roll >= (s16)(bonus - (s16)(guard - 85))) {
        return 2;
    }
    return 1;
}

/* Attack value of the current command: the item/part values its +0x10 bits
 * select plus the attack base (gear attack times scale, or the character's
 * +0x58, both scaled by statuses), or the ether value; kind 2 scales either
 * by the power over 20. Party attackers then get their character bonuses
 * and the target's weakness bonuses. */
s16 func_80096FBC(void) {
    u8 parts[5];
    u16 value;
    u8 i;
    u16 base;
    u16 sum;
    u16 ether;
    u8 kinds;
    u8 scale;
    u16 status;
    u16 flags;

    if (D_800C34B0->records[D_800C3E04].flags15A & 0x80) {
        for (i = 0; i < 3; i++) {
            parts[i] = D_800D2D6C->entries[i].valueE;
        }
        base = D_800D2D6C->attack * D_800D2D6C->attackScale;
        if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 0x1000) {
            base += D_800D2D6C->attack * 2;
        }
    } else {
        for (i = 0; i < 4; i++) {
            parts[i] = D_800C3E00->pilot.entries[i].value4;
        }
        base = D_800C3E00->pilot.attack;
    }
    kinds = D_800C3DFC->itemKinds;
    ether = D_800C3E00->pilot.accuracy;
    sum = 0;
    if (kinds & 0x80) {
        sum = parts[0];
    }
    if (kinds & 0x40) {
        sum += parts[1];
    }
    if (kinds & 0x20) {
        sum += parts[2];
    }
    if (kinds & 0x10) {
        sum += parts[3];
    }
    if (kinds & 0x08) {
        sum += parts[4];
    }
    if ((D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) & 1) {
        sum += sum >> 1;
    }
    if (D_800C3DFC->flagsA & 0x100) {
        scale = 4;
        if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 0x8000) {
            scale = 5;
        }
        if (D_800C3E00->pilot.status80 & 0x400) {
            scale--;
        }
        ether = ether * scale / 4;
        if ((D_800C3E00->pilot.status88.half.active | D_800C3E00->pilot.status88.half.permanent) & 0x2000) {
            ether *= 2;
        }
    } else if (!(D_800C34B0->records[D_800C3E04].flags15A & 0x80)) {
        status = D_800C3E00->pilot.status84.half.active | D_800C3E00->pilot.status84.half.permanent;
        scale = 4;
        if (status & 0x2000) {
            scale = 5;
        }
        if (D_800C3E00->pilot.status7C & 0x200) {
            scale--;
        }
        if (status & 0x400) {
            scale += 10 - D_800C3E00->pilot.hp / (u16)(D_800C3E00->pilot.maxHp / 10);
        }
        base = base * scale / 4;
    }
    flags = D_800C3DFC->flagsA;
    if (flags & 0x20) {
        base = 0;
    }
    switch (D_800C3DFC->amountKind) {
    case 0:
        value = sum + base;
        break;
    case 1:
        value = ether;
        break;
    case 2:
        if (D_800C3E00->pilot.characterId == 4) {
            sum = sum * 6 / 10;
        }
        if (flags & 0x100) {
            value = ether * D_800C3DFC->power / 20;
        } else {
            value = (sum + base) * D_800C3DFC->power / 20;
        }
        break;
    }
    if (D_800C3E04 < 3) {
        if (D_800C3E00->pilot.characterId == 7) {
            func_8009B46C(&value);
        }
        if (D_800C3E00->pilot.characterId == 4 || (D_800C3DFC->attributes[2] & 0x20)) {
            if (D_800C3E34->pilot.weakness & 0x20) {
                value += value >> 2;
            }
            if ((*(u32 *)&D_800C3E34->pilot.weakness & 0x60) == 0x60) {
                value += value >> 2;
            }
        }
        if (D_800C3E00->pilot.characterId == 8 && !(D_800CCCE8.records[D_800C3E04].flags15A & 0x80) &&
            (D_800C3DFC->flagsA & 0x100)) {
            value = value * D_800CCCE8.records[D_800C3E04].gear.frameFactor / 4;
        }
        if (D_800C3E00->pilot.characterId == 10) {
            value += value / 5;
        }
        if ((D_800C3DFC->attributes[2] & 0x10) && (D_800C3E34->pilot.weakness & 0x10)) {
            value += value >> 2;
            if (D_800C3E34->pilot.weakness & 0x10) {
                value += value >> 2;
            }
        }
    }
    return value;
}

/* Defense value of the target against the current command: body defense
 * plus armor (1.5x with status 0x100), the ether defense (1.5x with status
 * 0x4000 of +0x88) or the body alone, by the command's +0x1b. A character
 * target's +0x32 bits then scale it by the party members down. */
s16 func_80097610(void) {
    u16 armor;
    u16 body;
    u16 ether;
    u16 value;
    u8 downed;
    u8 i;

    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        body = D_800D2DC8->bodyDefense;
    } else {
        armor = D_800C3E34->pilot.defense;
        body = D_800C3E34->pilot.bodyDefense;
    }
    ether = D_800C3E34->pilot.etherDefense;
    if (D_800C3DFC->flagsA & 0x100) {
        if ((D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent) & 0x4000) {
            ether = ether * 3 / 2;
        }
    } else if (!(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        if ((D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent) & 0x100) {
            armor = armor * 3 / 2;
        }
    }
    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        armor = 0;
    }
    switch (D_800C3DFC->defenseKind) {
    case 0:
        value = body + armor;
        break;
    case 1:
        value = ether;
        break;
    case 2:
        value = body;
        break;
    }
    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        return value;
    }
    downed = 0;
    for (i = 0; i < 3; i++) {
        if (D_800C34B0->records[i].pilot.status7C & 0x8000) {
            downed++;
        }
    }
    if ((D_800C3E34->pilot.flags32 & 4) && downed != 0 && !(D_800C3DFC->flagsA & 0x100)) {
        value = value * (4 - downed) / 4;
    }
    if ((D_800C3E34->pilot.flags32 & 2) && downed != 0 && !(D_800C3DFC->flagsA & 0x100)) {
        value = value * (downed + 2) / 2;
    }
    if ((D_800C3E34->pilot.flags32 & 1) && downed != 0) {
        value = value * (downed + 2) / 2;
    }
    return value;
}

void func_8009795C(void) {
}

#ifdef NON_MATCHING
/* Roll a status onto the target (never a gear): with the chance in percent,
 * check the kind's immunities and clear the statuses it overrides, then set
 * the flag bits in the kind's status word and show its message. Returns 1
 * when the status took (or cancelled its opposite). */
s8 func_80097964(u8 chance, u8 kind, u16 flags) {
    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        return 0;
    }
    if (chance < rand() % 100) {
        return 0;
    }
    switch (kind) {
    case 0:
        if (D_800C3E34->pilot.status7E & (flags & 0xFFFD)) {
            return 0;
        }
        if (flags & 0x1000) {
            D_800C3E34->pilot.status84.half.active &= 0x7FFF;
        }
        break;
    case 2:
        if (flags & D_800C3E34->pilot.status82) {
            return 0;
        }
        break;
    case 5:
        if ((D_800C3E34->pilot.status7C | D_800C3E34->pilot.status7E) & 1) {
            return 0;
        }
        if (flags & 0x8000) {
            D_800C3E34->pilot.status7C &= 0xEFFF;
        }
        break;
    case 7:
        if ((D_800C3E34->pilot.status80 | D_800C3E34->pilot.status82) & 1) {
            return 0;
        }
        if (flags & 0xA) {
            if (!(D_800C3E34->pilot.status88.half.active & 5)) {
                break;
            }
            D_800C3E34->pilot.status88.half.active &= 0xFFFA;
            return 1;
        }
        if (flags & 5) {
            if (!(D_800C3E34->pilot.status88.half.active & 0xA)) {
                break;
            }
            D_800C3E34->pilot.status88.half.active &= 0xFFF5;
            return 1;
        }
    case 9:
        if (flags & 0xF000) {
            if (D_800C3E34->pilot.status8C.half.permanent & 0xF000) {
                D_800C34B0->message = 0x39;
                return 0;
            }
            D_800C3E34->pilot.status8C.half.active &= 0xFFF;
        }
        if (flags & 0xF00) {
            if (D_800C3E34->pilot.status8C.half.permanent & 0xF00) {
                D_800C34B0->message = 0x39;
                return 0;
            }
            D_800C3E34->pilot.status8C.half.active &= 0xF0FF;
        }
        break;
    }
    switch (kind) {
    case 0:
        D_800C3E34->pilot.status7C = (flags | D_800C3E34->pilot.status7C) & 0xFFFD;
        if (flags & 2) {
            if ((u8)func_80099498()) {
                D_800C3E34->pilot.status7C |= flags;
                D_800C34B0->message = 0x31;
                D_800C3E34->pilot.status7A = 0xFFEF;
            } else {
                D_800C34B0->message = 0x32;
            }
        }
        break;
    case 2:
        D_800C3E34->pilot.status80 |= flags;
        if (flags & 0x800) {
            D_800C3E34->pilot.status7A |= 0x20;
        }
        break;
    case 5:
    case 7:
    case 9:
        (&D_800C3E34->pilot.status7A)[kind] |= flags;
        break;
    }
    func_8009B684(kind, flags);
    return 1;
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80097964);
#endif

/* Clear the per-slot damage and result codes (12 entries). */
void func_80097D08(void) {
    s16 slot = 11;

    do {
        D_800C34B0->resultCode[slot] = 0xFF;
        D_800C34B0->damage[slot] = 0;
    } while (--slot != -1);
}

/* Derive each present party member's battle stats: keep the base values,
 * add the equipment bonuses and percentage HP/EP bonuses (capped at 999 and
 * 99), total the gear's equipment, set its fuel cost (command 37's shown
 * value), the attack level limit and scale, the status-driven command rate
 * changes and the character-specific adjustments; then count the members. */
void func_80097D5C(void) {
    u8 member;
    u8 i;
    u8 *level;
    u8 rate;
    u16 immunities;
    CharacterRecord *pilot;
    Combatant *record;
    GearRecord *gear;

    for (member = 0; member < 3; member++) {
        if (D_800D2D24[member] == 0x7F) {
            continue;
        }
        D_800C3E00 = &D_800C34B0->records[member];
        D_800D2D6C = &D_800C34B0->records[member].gear;
        level = &D_800C34B0->records[member].field148;
        if (D_800D2D24[member] == 9) {
            D_8006D8A0.characters[9].characterId = 9;
            D_800C3E00->pilot.characterId = 9;
        }
        if (D_800D2D24[member] == 10) {
            D_8006D8A0.characters[10].characterId = 10;
            D_800C3E00->pilot.characterId = 10;
        }
        if (D_800C3E00->pilot.characterId == 4) {
            D_800C34B0->savedStats[member][0] = D_800C3E00->pilot.entries[0].value4 + D_800C3E00->pilot.entries[3].value4;
        } else {
            D_800C34B0->savedStats[member][0] = D_800C3E00->pilot.attack + D_800C3E00->pilot.entries[0].value4;
        }
        D_800C34B0->savedStats[member][1] = D_800C3E00->pilot.field5E;
        D_800C34B0->savedStats[member][2] = D_800C3E00->pilot.defense + D_800C3E00->pilot.bodyDefense;
        D_800C34B0->savedStats[member][3] = D_800C3E00->pilot.field5F;
        D_800C34B0->savedStats[member][4] = D_800C3E00->pilot.accuracy;
        D_800C34B0->savedStats[member][5] = D_800C3E00->pilot.etherDefense;
        D_800C34B0->savedStats[member][6] = D_800C3E00->pilot.speed;
        pilot = &D_800C3E00->pilot;
        D_800C34B0->expTotals[member][0] = pilot->expTotalA;
        D_800C34B0->expTotals[member][1] = pilot->expTotalB;
        D_800C34B0->savedMax[member][0] = pilot->maxHp;
        D_800C34B0->savedMax[member][1] = pilot->maxEp;
        pilot->attack += pilot->equipAttack;
        pilot->flags34 = 0;
        D_800C3E00->pilot.defense += D_800C3E00->pilot.equipDefense;
        D_800C3E00->pilot.speed += D_800C3E00->pilot.equipSpeed;
        D_800C3E00->pilot.accuracy += D_800C3E00->pilot.equipAccuracy;
        D_800C3E00->pilot.etherDefense += D_800C3E00->pilot.equipEtherDefense;
        D_800C3E00->pilot.field5E += D_800C3E00->pilot.equip5E;
        D_800C3E00->pilot.field5F += D_800C3E00->pilot.equip5F;
        if (D_800C3E00->pilot.speed > 16) {
            D_800C3E00->pilot.speed = 16;
        }
        if (D_800C3E00->pilot.flags32 & 0x100) {
            D_800C3E00->pilot.field5E += D_800C3E00->pilot.field5E >> 2;
            D_800C3E00->pilot.field5F += D_800C3E00->pilot.field5F >> 2;
        }
        D_800C3E00->pilot.hp += D_800C3E00->pilot.maxHp * D_800C3E00->pilot.hpBonus / 20;
        D_800C3E00->pilot.ep += D_800C3E00->pilot.maxEp * D_800C3E00->pilot.epBonus / 20;
        D_800C3E00->pilot.maxHp += D_800C3E00->pilot.maxHp * D_800C3E00->pilot.hpBonus / 20;
        D_800C3E00->pilot.maxEp += D_800C3E00->pilot.maxEp * D_800C3E00->pilot.epBonus / 20;
        if (D_800C3E00->pilot.hp >= 1000) {
            D_800C3E00->pilot.hp = 999;
        }
        if (D_800C3E00->pilot.ep >= 100) {
            D_800C3E00->pilot.ep = 99;
        }
        if (D_800C3E00->pilot.maxHp >= 1000) {
            D_800C3E00->pilot.maxHp = 999;
        }
        if (D_800C3E00->pilot.maxEp >= 100) {
            D_800C3E00->pilot.maxEp = 99;
        }
        record = &D_800C34B0->records[member];
        record->expWeightA = 5;
        record->expWeightB = 5;
        level[0] = 0;
        gear = D_800D2D6C;
        gear->bodyDefense += gear->equipBodyDefense;
        gear->armor += gear->equipArmor;
        gear->field68 += gear->equip68a + gear->equip68b;
        gear->guard += gear->equipGuard;
        D_800D2D6C->hitBonus += D_800D2D6C->equipHitBonus;
        D_800D2D6C->speed += D_800D2D6C->equipSpeed - D_800D2D6C->speedPenalty;
        D_800D2D6C->frameFactor += D_800D2D6C->equipFrameFactor;
        rate = D_800D2D6C->field4F;
        if (rate != 0 && D_800C3E00->pilot.gearId != 0x12) {
            D_800C34B0->gearCommands[member][37].hudState = D_800D2D6C->maxHp / 10 * rate * 2 / 9;
            D_800C34B0->gearCommands[member][37].hudState /= 10;
            D_800C34B0->gearCommands[member][37].hudState *= 10;
        }
        level[1] = 0;
        if (D_8006ECF4[D_800C3E00->pilot.characterId].mask4 & 0x1C00) {
            level[1] = 1;
        }
        if (D_8006ECF4[D_800C3E00->pilot.characterId].mask4 & 0x380) {
            level[1] = 2;
        }
        if (D_8006ECF4[D_800C3E00->pilot.characterId].mask4 & 0x70) {
            level[1] = 3;
        }
        D_800D2D6C->attackScale = D_800D2D6C->field74;
        D_800D2D6C->field3E = D_800D2D6C->field74;
        D_800D2D6C->attackScale += D_800D2D6C->equipAttackScale;
        D_800D2D6C->field3E += D_800D2D6C->equipAttackScale;
        if (D_800C3E00->pilot.status88.half.permanent & 0x2000) {
            for (i = 0; i < 38; i++) {
                D_800C34B0->partyCommands[member][i].cost *= 2;
            }
            for (i = 0; i < 42; i++) {
                D_800C34B0->gearCommands[member][i].cost *= 2;
            }
        }
        if (D_800C3E00->pilot.flags32 & 0x4000) {
            for (i = 0; i < 38; i++) {
                D_800C34B0->partyCommands[member][i].cost = (D_800C34B0->partyCommands[member][i].cost + 1) >> 1;
            }
            for (i = 0; i < 42; i++) {
                D_800C34B0->gearCommands[member][i].cost = (D_800C34B0->gearCommands[member][i].cost + 1) >> 1;
            }
        }
        if (D_800C3E00->pilot.field62 >= 50) {
            D_8006ECF4[D_800C3E00->pilot.characterId].mask4 |= 8;
        }
        if (D_800C3E00->pilot.characterId == 7) {
            D_800D2D6C->hp = D_800C3E00->pilot.hp * 50;
            D_800D2D6C->maxHp = D_800C3E00->pilot.maxHp * 50;
            D_800D2D6C->attack = D_800C3E00->pilot.attack;
            D_800D2D6C->bodyDefense = D_800C3E00->pilot.defense * 12;
            D_800D2D6C->armor = D_800C3E00->pilot.etherDefense * 6;
            D_800D2D6C->speed = D_800C3E00->pilot.speed;
            immunities = D_800D2D6C->field7E;
            D_800D2D6C->field7E = immunities | 0x3C4;
            if (D_800C3E00->pilot.status82 & 0x2000) {
                D_800D2D6C->field7E = immunities | 0x13C4;
            }
        }
        if (D_800C3E00->pilot.gearId == 0xF) {
            D_8006ECF4[D_800C3E00->pilot.characterId].flags1A &= 0x8FFF;
        }
        if (D_800C3E00->pilot.gearId == 0x12) {
            D_800C3E00->pilot.status7A = 0x238;
            D_800D2D6C->fuel = 0x26AC;
            D_800D2D6C->maxFuel = 0x26AC;
            level[1] = 0;
            D_8006ECF4[3].flags1A = 0x8000;
        }
        if (D_8006D634.value1930 >= 231 && !(D_8006D634.flags2355 & 0x80)) {
            D_8006D634.characters[9].field6A = 0x27;
            D_8006D634.characters[9].entries[0].value4 = 0x1E;
            D_8006D634.flags2355 |= 0x80;
        }
    }
    D_800C34AD = 3;
    for (member = 0; member < 3; member++) {
        if (D_800D2D24[member] == 0x7F) {
            D_800C34AD--;
        }
    }
    for (member = 0; member < 3; member++) {
        D_800C34B0->field5F54[member] = 0;
        D_800C34B0->field5F60[member] = 0;
    }
}

/* Party adjustments at battle start: keep each present member's status
 * word 7A, replace the listed part speeds of its gear by the parts' own
 * (at most 16), set character 8's speed and battle flag, and before game
 * data word 0x1930 reaches 0xbb set the early gears' values. */
void func_8009892C(void) {
    u8 member;
    u8 i;
    Combatant *record;

    for (member = 0; member < 3; member++) {
        if (D_800D2D24[member] == 0x7F) {
            continue;
        }
        record = &D_800C34B0->records[member];
        D_800C3E00 = record;
        D_800D2D6C = &record->gear;
        D_800C3AA4[member] = record->pilot.status7A;
        for (i = 0; i < 4; i++) {
            if (D_800D2D10[i] != 0) {
                D_800D2D6C->speed -= D_800D2D10[i];
                D_800D2D6C->speed += D_800D2D6C->speedBonus[i];
            }
        }
        if (D_800D2D6C->speed > 16) {
            D_800D2D6C->speed = 16;
        }
    }
    D_8006D8A0.characters[8].speed = 7;
    if (D_8006ECF4[8].flags1A & 0x2000) {
        D_8006ECF4[8].mask2 |= 0x800;
    }
    if (D_8006D634.value1930 < 0xBB) {
        D_8006D8A0.gears[0].field74 = 10;
        D_8006D8A0.gears[1].field74 = 10;
        D_8006D8A0.gears[11].field74 = 9;
        D_8006D8A0.gears[12].field74 = 9;
        D_8006D8A0.gears[13].field74 = 8;
        D_8006D8A0.gears[14].field74 = 12;
        D_8006D8A0.gears[15].field74 = 12;
        D_8006D8A0.gears[13].field2 = 0x58;
        D_8006D8A0.gears[7].field3 = 0;
        D_8006D8A0.gears[15].field8 = 0x28;
    }
}

/* A slot's turn timer from its speed (party: less the current command's
 * weight, gear speed for a slot in gear), capped, with a random -3..4
 * spread. Sets the attacker globals and, for the party, the command
 * descriptor. Defined old-style: callers pass the slot unnarrowed. */
s32 func_80098AF8(slot, mode)
u8 slot;
s32 mode;
{
    Combatant *record = &D_800C34B0->records[slot];
    u16 speed;

    D_800D2D6C = &record->gear;
    D_800C3E00 = record;
    if (slot < 3) {
        if (!(record->flags15A & 0x80)) {
            D_800C3DFC = &D_800C34B0->partyCommands[slot][D_800C34B0->commandIndex];
            if (record->pilot.speed > D_800C3DFC->weight) {
                speed = (record->pilot.speed - D_800C3DFC->weight) * 9;
            } else {
                speed = 9;
            }
        } else {
            D_800C3DFC = &D_800C34B0->gearCommands[slot][D_800C34B0->commandIndex];
            if (record->gear.speed > D_800C3DFC->weight) {
                speed = (record->gear.speed - D_800C3DFC->weight) * 9;
            } else {
                speed = 9;
            }
        }
    } else {
        speed = record->pilot.speed * 9;
    }
    if (speed >= 0xA6) {
        speed = 0xA0;
    }
    speed = 0xA5 - speed;
    speed -= rand() % 8 - 4;
    func_80094D24();
    return (u8)speed;
}

/* Resolve an item/effect `param` on every slot in the +0x5fac mask, then
 * set its animation from the effect table. */
void func_80098C6C(u16 param) {
    s32 slot;
    s32 bit;

    func_80097D08();
    bit = 1;
    for (slot = 0; (u8)slot < 11; slot++) {
        if (bit & D_800C34B0->targetMask) {
            func_80098D2C(slot, param);
        }
        bit <<= 1;
    }
    D_800C34B0->shownCommand = D_800D2200[param & 0xFF].animation;
    if (D_800D2C94.held & 0x8000) {
        D_800D2C94.animation = 0xC2;
    }
}

#ifdef NON_MATCHING
/* Apply item effect `param` to `slot`: HP (amount x 50, doubled with status
 * 0x80 of +0x86) and EP (amount x 10) restoration, and unless either ran,
 * the status cures, revival (amount tenths of the maximum HP), status
 * grants, immunities and the special effects of flags value 1. The item
 * effect table lies inside the battle work area (+0x5518, D_800D2200). */
void func_80098D2C(u8 slot, u8 param) {
    u8 restored;
    Combatant *record;
    ItemEffect *effect;
    GearRecord *gear;
    u8 i;
    s32 hpUnit;
    s32 epUnit;

    restored = 0;
    record = &D_800CCCE8.records[slot];
    effect = &((ItemEffect *)&D_800CCCE8.lists)[param + 2];
    gear = &D_800CCCE8.records[slot].gear;
    epUnit = 10;

    if (effect->flags & 0x8000) {
        hpUnit = 50;
        D_800CCCE8.damage[slot] = effect->amount * hpUnit;
        D_800D2C88[slot] = 2;
        if (record->pilot.status84.half.permanent & 0x80) {
            D_800C34B0->damage[slot] *= 2;
        }
        if (record->pilot.flags36 & 0x8000) {
            D_800D2C88[slot] = 0;
        }
        restored = 1;
    }
    if (effect->flags & 0x4000) {
        D_800D2C54[slot] = epUnit * effect->amount;
        D_800D2C88[slot] = 3;
        restored++;
    }
    if (restored) {
        return;
    }
    if ((D_800CCCE8.records[slot].flags15A & 0x80) && effect->flags != 1) {
        D_800D2C94.message = 0x30;
        D_800D2C94.held |= 0x8000;
        return;
    }
    if (effect->flags & 0x2000) {
        record->pilot.status7C &= 0x8000;
    }
    if (effect->flags & 0x1000) {
        record->pilot.status80 = 0;
        record->pilot.status7A &= 0xFFDF;
    }
    if (effect->flags & 0x100) {
        if (D_800CCCE8.records[slot].pilot.characterId == 2) {
            func_8009AC48(slot, 0);
        }
        record->pilot.status7C = 0;
        record->pilot.status80 = 0;
        record->pilot.status84.half.active = 0;
        record->pilot.status88.half.active = 0;
        record->pilot.status8C.half.active = 0;
        record->pilot.hp = record->pilot.maxHp * effect->amount / 10;
        D_800C34B0->revived |= 1 << slot;
    }
    if (effect->flags & 0x800) {
        record->pilot.status84.half.active |= effect->status;
        func_800995A0(slot, 5, effect->status, 5);
        func_8009B684(5, effect->status);
    }
    if (effect->flags & 0x400) {
        record->pilot.status88.half.active |= effect->status;
        func_800995A0(slot, 7, effect->status, 5);
        func_8009B684(7, effect->status);
    }
    if (effect->flags & 0x200) {
        record->pilot.status8C.half.active |= effect->status;
        if ((effect->status & 0xF000) && !(record->pilot.status8C.half.permanent & 0xF000)) {
            record->pilot.status8C.half.active &= 0xFFF;
            record->pilot.status8C.half.active |= effect->status;
            func_800995A0(slot, 9, effect->status, 5);
            func_8009B684(9, effect->status);
        }
        if ((effect->status & 0xF00) && !(record->pilot.status8C.half.permanent & 0xF00)) {
            record->pilot.status8C.half.active &= 0xF0FF;
            record->pilot.status8C.half.active |= effect->status;
            func_800995A0(slot, 9, effect->status, 5);
            func_8009B684(9, effect->status);
        }
    }
    if (effect->flags & 0x20) {
        record->pilot.status7E |= 0x3F7C;
    }
    if (effect->flags & 0x10) {
        record->pilot.status7E |= 0xFFFE;
    }
    if (effect->flags & 8) {
        (&record->pilot.status84.half.active)[effect->amount] = 0;
        switch (effect->amount) {
        case 0:
            D_800C34B0->message = 0x35;
            break;
        case 2:
            D_800C34B0->message = 0x36;
            break;
        case 4:
            D_800C34B0->message = 0x37;
            break;
        }
    }
    if ((effect->flags & 0x80) && (effect->status & 2)) {
        if (slot >= 3) {
            D_800C34B0->message = 0x33;
            return;
        }
        if (effect->duration == 0) {
            if ((u8)func_80099498()) {
                record->pilot.status7C |= effect->status;
                D_800C34B0->message = 0x31;
                record->pilot.status7A = 0xFFEF;
            } else {
                D_800C34B0->message = 0x32;
            }
        } else {
            record->pilot.status7C &= ~effect->status;
            record->pilot.status7A = D_800C3AA4[slot];
        }
    }
    if (effect->flags == 1) {
        switch (effect->amount) {
        case 10:
            record->pilot.weakness = 0;
            record->pilot.weakness = effect->status;
            break;
        case 11:
            if (!(record->pilot.status7E & effect->status)) {
                record->pilot.status7C |= effect->status;
                func_800995A0(slot, 0, effect->status, effect->duration);
                func_8009B684(0, effect->status);
            }
            break;
        case 12:
            if (!(record->pilot.status82 & effect->status)) {
                record->pilot.status80 |= effect->status;
                func_800995A0(slot, 2, effect->status, effect->duration);
                func_8009B684(2, effect->status);
            }
            break;
        case 13:
            gear->defense += effect->status;
            D_800C34B0->message = 0x28;
            break;
        case 14:
            D_8006D8A0.characters[record->pilot.characterId].expNextA = 1;
            D_8006D8A0.characters[record->pilot.characterId].expNextB = 1;
            break;
        case 15:
            for (i = 0; i < 7; i++) {
                record->pilot.useCounts[i] += 10;
            }
            break;
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_80098D2C);
#endif

/* Party gate on the formation mode: 1 when mode 2 has no member with status
 * bits 0xC002, or mode 3 does not have exactly two; otherwise 0. */
s32 func_80099498(void) {
    s32 result = 0;
    u8 i;
    u8 count;

    switch (D_800C34AD) {
    case 2:
        count = 0;
        for (i = 0; i < 3; i++) {
            if (D_800C34B0->records[i].pilot.status7C & 0xC002) {
                count++;
            }
        }
        if (count == 0) {
            result = 1;
        }
        break;
    case 3:
        count = 0;
        for (i = 0; i < 3; i++) {
            if (D_800C34B0->records[i].pilot.status7C & 0xC002) {
                count++;
            }
        }
        if (count != 2) {
            result = 1;
        }
        break;
    case 1:
        break;
    }
    return result;
}

/* Store a timed status's duration in slot's timer table. The status is named by
 * its kind and flag bit; the attacker's flag 0x2000 doubles the amount, and for
 * kinds 5, 7 and 9 the target's status bits 0x5 double and 0xA halve it, while
 * the slot's flag 0x40 doubles it again. Unknown statuses are ignored. */
void func_800995A0(u8 slot, u8 kind, u16 flag, u8 amount) {
    Combatant *record;
    u8 index = 0xF;
    u16 state;

    if (D_800CCCE8.records[D_800C3E04].pilot.status88.half.permanent & 0x2000) {
        amount *= 2;
    }
    record = &D_800CCCE8.records[slot];
    if (kind == 0) {
        index = (flag == 0x1000) ? 1 : (flag == 0x2000) ? 0 : 0xF;
    }
    if (kind == 2) {
        switch (flag) {
        case 0x1000:
            index = 2;
            break;
        case 0x800:
            index = 3;
            break;
        }
    }
    if (kind == 5) {
        switch (flag) {
        case 0x8000:
            index = 4;
            break;
        case 0x4000:
            index = 5;
            break;
        }
    }
    if (kind == 7) {
        switch (flag) {
        case 0x8000:
            index = 9;
            break;
        case 0x4000:
            index = 0xA;
            break;
        case 0x1000:
            index = 0xB;
            break;
        }
    }
    if (kind == 9) {
        switch (flag) {
        case 0x1000:
        case 0x2000:
        case 0x4000:
        case 0x8000:
            index = 0xC;
            break;
        case 0x100:
        case 0x200:
        case 0x400:
        case 0x800:
            index = 0xD;
            break;
        }
    }
    if (index == 0xF) {
        return;
    }
    switch (kind) {
    case 5:
    case 7:
    case 9:
        state = D_800C3E34->pilot.status88.half.active | D_800C3E34->pilot.status88.half.permanent;
        if (state & 5) {
            amount *= 2;
        }
        if (state & 0xA) {
            amount /= 2;
        }
        break;
    }
    if (record->pilot.flags32 & 0x40) {
        switch (kind) {
        case 5:
        case 7:
        case 9:
            amount *= 2;
            break;
        }
    }
    D_800CCCE8.records[slot].statusTimers[index] = amount;
}

/* Count down slot's timed statuses at the start of its turn, clearing each one
 * whose timer runs out; a slot in a gear counts down its gear's statuses
 * instead (80099CF0). Returns a mask of the statuses that ended. */
u16 func_80099890(u8 slot) {
    Combatant *record = &D_800CCCE8.records[slot];
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    volatile u8 *timers = D_800CCCE8.records[slot].statusTimers;
    u16 ended;

    if (D_800CCCE8.records[slot].flags15A & 0x80) {
        func_80099CF0(gear, record, timers);
        return 0;
    }
    ended = 0;
    if (record->pilot.status7C & 0x1000) {
        timers[1] += -1;
        if (timers[1] == 0) {
            ended = 0x4000;
            record->pilot.status7C &= ~0x1000;
        }
    }
    if (record->pilot.status80 & 0x1000) {
        timers[2] += -1;
        if (timers[2] == 0) {
            ended |= 0x2000;
            record->pilot.status80 &= ~0x1000;
        }
    }
    if (record->pilot.status80 & 0x800) {
        timers[3] += -1;
        if (timers[3] == 0) {
            ended |= 0x1000;
            record->pilot.status80 &= ~0x800;
            record->pilot.status7A &= ~0x20;
        }
    }
    if ((record->pilot.status84.word & 0x80008000) == 0x8000) {
        timers[4] += -1;
        if (timers[4] == 0) {
            ended |= 0x800;
            record->pilot.status84.half.active &= ~0x8000;
        }
    }
    if ((record->pilot.status84.word & 0x40004000) == 0x4000) {
        timers[5] += -1;
        if (timers[5] == 0) {
            ended |= 0x400;
            record->pilot.status84.half.active &= ~0x4000;
        }
    }
    if ((record->pilot.status84.word & 0x20002000) == 0x2000) {
        timers[6] += -1;
        if (timers[6] == 0) {
            ended |= 0x200;
            record->pilot.status84.half.active &= ~0x2000;
        }
    }
    if ((record->pilot.status84.word & 0x10001000) == 0x1000) {
        timers[7] += -1;
        if (timers[7] == 0) {
            ended |= 0x100;
            record->pilot.status84.half.active &= ~0x1000;
        }
    }
    if ((record->pilot.status84.word & 0x08000800) == 0x800) {
        timers[8] += -1;
        if (timers[8] == 0) {
            ended |= 0x80;
            record->pilot.status84.half.active &= ~0x800;
        }
    }
    if ((record->pilot.status88.word & 0x80008000) == 0x8000) {
        timers[9] += -1;
        if (timers[9] == 0) {
            ended |= 0x40;
            record->pilot.status88.half.active &= ~0x8000;
        }
    }
    if ((record->pilot.status88.word & 0x40004000) == 0x4000) {
        timers[10] += -1;
        if (timers[10] == 0) {
            ended |= 0x20;
            record->pilot.status88.half.active &= ~0x4000;
        }
    }
    if ((record->pilot.status88.word & 0x10001000) == 0x1000) {
        timers[11] += -1;
        if (timers[11] == 0) {
            ended |= 0x10;
            record->pilot.status88.half.active &= ~0x1000;
        }
    }
    if ((record->pilot.status8C.half.active & 0xF000) && !(record->pilot.status8C.half.permanent & 0xF000)) {
        timers[12] += -1;
        if (timers[12] == 0) {
            ended |= 8;
            record->pilot.status8C.half.active &= ~0xF000;
        }
    }
    if ((record->pilot.status8C.half.active & 0xF00) && !(record->pilot.status8C.half.permanent & 0xF00)) {
        timers[13] += -1;
        if (timers[13] == 0) {
            ended |= 4;
            record->pilot.status8C.half.active &= ~0xF00;
        }
    }
    return ended;
}

/* Count down the timed statuses of a gear, clearing each one whose timer runs
 * out; the end of gear status 0x20 also ends the pilot's status 0x1000. */
void func_80099CF0(GearRecord *gear, Combatant *record, volatile u8 *timers) {
    if (gear->status7C & 0x200) {
        timers[1] += -1;
        if (timers[1] == 0) {
            gear->status7C &= ~0x200;
        }
    }
    if (gear->status7C & 0x100) {
        timers[2] += -1;
        if (timers[2] == 0) {
            gear->status7C &= ~0x100;
        }
    }
    if (gear->status7C & 0x80) {
        timers[3] += -1;
        if (timers[3] == 0) {
            gear->status7C &= ~0x80;
        }
    }
    if (gear->status7C & 0x20) {
        timers[4] += -1;
        if (timers[4] == 0) {
            gear->status7C &= ~0x20;
            record->pilot.status7C &= ~0x1000;
        }
    }
    if (gear->status7C & 0x10) {
        timers[5] += -1;
        if (timers[5] == 0) {
            gear->status7C &= ~0x10;
        }
    }
    if (gear->status7C & 0xF000) {
        timers[7] += -1;
        if (timers[7] == 0) {
            gear->status7C &= ~0xF000;
        }
    }
    if (gear->status7C & 0xF00) {
        timers[8] += -1;
        if (timers[8] == 0) {
            gear->status7C &= ~0xF00;
        }
    }
    if (gear->status80 & 0x1000) {
        timers[10] += -1;
        if (timers[10] == 0) {
            gear->status80 &= ~0x1000;
        }
    }
    if (gear->status80 & 0x40) {
        timers[11] += -1;
        if (timers[11] == 0) {
            gear->status80 &= ~0x40;
        }
    }
    if (gear->status80 & 0x20) {
        timers[12] += -1;
        if (timers[12] == 0) {
            gear->status80 &= ~0x20;
        }
    }
}

/* Make the current command descriptor the battle's current command: copy its
 * attribute bytes and index, and give a command without an element the
 * attacker's element statuses. */
void func_80099FB0(void) {
    u16 elements = (D_800C3E00->pilot.status8C.half.active | D_800C3E00->pilot.status8C.half.permanent) >> 12;

    D_800C34B0->commandAttributes[0] = D_800C3DFC->attributes[0];
    D_800C34B0->commandAttributes[1] = D_800C3DFC->attributes[1];
    D_800C34B0->commandAttributes[2] = D_800C3DFC->attributes[2];
    D_800C34B0->commandAttributes[3] = D_800C3DFC->attributes[3];
    D_800C34B0->commandIndexCopy = D_800C34B0->commandIndex;
    if ((D_800C34B0->commandAttributes[2] & 0x3F) == 0) {
        D_800C34B0->commandAttributes[2] |= elements;
    }
}

/* Restrict the target mask to the allowed targets of its side: the low three
 * bits (party) and bits 3-10 (enemies). */
void func_8009A074(void) {
    if (D_800C34B0->targetMask & 7) {
        D_800C34B0->targetMask = D_800C34B0->targetMask2 & 7;
    }
    if (D_800C34B0->targetMask & 0x7F8) {
        D_800C34B0->targetMask = D_800C34B0->targetMask2 & 0x7F8;
    }
}

/* The condition shown for slot, by priority: 8, 1, 2 for status bits 0x4000,
 * 0x8000, 0x2000; 3 and 4 for the second word's 0x1000 and 0x2000; 5, 6, 3 for
 * 0x800, 0x1000, 2; 7 for status pair 0x84 bit 0x8000; 5 below an eighth of
 * the maximum HP; otherwise 0. */
s32 func_8009A0DC(u8 slot) {
    Combatant *record = &D_800C34B0->records[slot];

    if (record->pilot.status7C & 0x4000) {
        return 8;
    }
    if (record->pilot.status7C & 0x8000) {
        return 1;
    }
    if (record->pilot.status7C & 0x2000) {
        return 2;
    }
    if (record->pilot.status80 & 0x1000) {
        return 3;
    }
    if (record->pilot.status80 & 0x2000) {
        return 4;
    }
    if (record->pilot.status7C & 0x800) {
        return 5;
    }
    if (record->pilot.status7C & 0x1000) {
        return 6;
    }
    if (record->pilot.status7C & 2) {
        return 3;
    }
    if ((record->pilot.status84.half.active | record->pilot.status84.half.permanent) & 0x8000) {
        return 7;
    }
    if (record->pilot.hp < record->pilot.maxHp >> 3) {
        return 5;
    }
    return 0;
}

/* Mask of slot's timed conditions for display (0 once status bit 0x8000 is
 * set): 0x8000, 0x4000 and 0x2000 for three statuses, plus the element
 * statuses (pair 0x8C bits 8-15) shifted down by three. */
u16 func_8009A1AC(u8 slot) {
    Combatant *record = &D_800C34B0->records[slot];
    u16 mask = 0;
    u16 elements;

    if (record->pilot.status7C & 0x8000) {
        return 0;
    }
    if (record->pilot.status80 & 0x1000) {
        mask = 0x8000;
    }
    if (record->pilot.status80 & 0x2000) {
        mask |= 0x4000;
    }
    if (record->pilot.status7C & 0x800) {
        mask |= 0x2000;
    }
    elements = record->pilot.status8C.half.active | record->pilot.status8C.half.permanent;
    if (elements & 0xF00) {
        mask |= (elements & 0xF00) >> 3;
    }
    if (elements & 0xF000) {
        mask |= (elements & 0xF000) >> 3;
    }
    return mask;
}

/* Accuracy of member's command: the descriptor's accuracy plus the member's
 * bonus, capped at 100. */
u8 func_8009A258(u8 member, u8 command) {
    CommandDescriptor *descriptor = &D_800C34B0->partyCommands[member][command];
    u8 accuracy = descriptor->accuracy + (D_800C34B0->records + member)->pilot.accuracy;

    if (accuracy > 100) {
        accuracy = 100;
    }
    return accuracy;
}

/* Fill the gear HUD for member: its first commands' states, charge rate,
 * attack, defense, the chance of a boost (from the gear's damage, when the
 * boost flag of D_8006F8EA is on), warning bits and overheat; count down an
 * active boost (level 4) or, at level 3, try to start one. */
void func_8009A2D4(u8 member) {
    Combatant *record = &D_800CCCE8.records[member];
    GearRecord *gear = &D_800CCCE8.records[member].gear;
    u8 *level = &D_800CCCE8.records[member].field148;
    CommandDescriptor *commands = D_800CCCE8.gearCommands[member];
    GearHud *hud = &D_800CCCE8.gearHud;
    u8 i;
    u8 chance;

    for (i = 0; i < 15; i++) {
        hud->commands[i] = commands->hudState;
        commands++;
    }
    if (gear->chargeRate != 0) {
        hud->charge = gear->chargeRate * hud->commands[0];
    } else {
        hud->charge = 30;
    }
    switch (*level) {
    case 1:
    case 2:
    case 3:
        hud->charge += *level * 20;
        break;
    case 4:
        hud->charge *= 10;
        break;
    }
    hud->attack = gear->attack * gear->attackScale;
    hud->attack = gear->entries[0].valueE + hud->attack;
    if (record->pilot.characterId == 4) {
        hud->attack = gear->entries[2].valueE + hud->attack;
    }
    hud->field29 = gear->speed;
    hud->defense = gear->defense;
    if (gear->maxHp != gear->hp) {
        chance = (gear->maxHp - gear->hp) / (gear->maxHp / 10);
        if (chance == 0) {
            chance++;
        }
    } else {
        chance++; /* uninitialised in the original */
    }
    chance *= record->pilot.field54 + 5;
    if (!(*(u16 *)D_8006F8EA & 0x4000)) {
        chance = 0;
    }
    if (record->pilot.field62 < 50) {
        chance = 0;
    }
    if (record->pilot.gearId == 3) {
        chance = 0;
    }
    if (record->pilot.gearId == 15) {
        chance = 99;
    }
    if (chance >= 100) {
        chance = 99;
    }
    hud->boostChance = chance;
    hud->status = 0;
    if (gear->status7C & 0x100) {
        hud->status = 0x8000;
    }
    if (gear->status7C & 0x200) {
        hud->status |= 0x4000;
    }
    if (gear->status7C & 0x80) {
        hud->status |= 0x2000;
    }
    if (gear->status7C & 0x10) {
        hud->status |= 0x1000;
    }
    if (gear->fuel < gear->maxFuel >> 3) {
        hud->status |= 0x800;
    } else {
        hud->status &= ~0x800;
    }
    if (gear->fuel == 0) {
        gear->status80 &= ~0x8000;
        record->pilot.status84.half.active &= ~0x8000;
    }
    if (gear->status80 & 0x8000) {
        hud->overheat = 1;
    } else {
        hud->overheat = 0;
    }
    if (*level == 4) {
        hud->level = 4;
        if (--D_800CCCE8.records[member].statusTimers[6] == 0) {
            gear->status80 &= ~0x4000;
            *level = 0;
            record->pilot.field54 = 0;
        }
    } else {
        hud->level = *level;
        if (*level == 3 && (*(u16 *)D_8006F8EA & 0x4000) && rand() % 100 < chance) {
            gear->status80 |= 0x4000;
            D_800CCCE8.records[member].statusTimers[6] = 3;
            if (D_800CCCE8.records[member].pilot.flags32 & 0x40) {
                D_800CCCE8.records[member].statusTimers[6] = 6;
            }
            (*level)++;
        }
    }
}

/* Byte 5 of game-data unit record id. */
u8 func_8009A7B8(u8 id) {
    return D_8006D8A0.characters[id].entries[0].pad5;
}

/* Whether gear item index is one of character 4's four entries. */
s32 func_8009A7E4(u8 index) {
    BattleItem *item = &D_800C34B0->lists.items.members[index];

    if (D_8006D8A0.characters[4].entries[0].id == item->id
        || D_8006D8A0.characters[4].entries[1].id == item->id
        || D_8006D8A0.characters[4].entries[2].id == item->id) {
        return 1;
    }
    return D_8006D8A0.characters[4].entries[3].id == item->id;
}

/* Put battle item index into character 4's entry holding its id (entry k when
 * none does): copy its values, record the item slot and durability, and update
 * the battle copies of character 4. */
void func_8009A854(u8 index, u8 k) {
    BattleItem *item = &D_800C34B0->lists.items.list[index];
    u8 i;

    if (D_8006D8A0.characters[4].entries[0].id == item->id) {
        k = 0;
    }
    if (D_8006D8A0.characters[4].entries[1].id == item->id) {
        k = 1;
    }
    if (D_8006D8A0.characters[4].entries[2].id == item->id) {
        k = 2;
    }
    if (D_8006D8A0.characters[4].entries[3].id == item->id) {
        k = 3;
    }
    D_8006D8A0.characters[4].entries[k].value4 = item->valueC;
    D_8006D8A0.characters[4].entries[k].value3 = item->valueB;
    D_8006D8A0.characters[4].entries[k].value2 = item->valueA;
    D_8006D8A0.characters[4].entries[k].value3 = item->valueB;
    D_8006D8A0.characters[4].entryItems[k] = index;
    D_8006F8BA[index] = item->durability;
    D_8006D8A0.characters[4].entryItems[k] = index;
    for (i = 0; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];

        if (record->pilot.characterId == 4) {
            record->pilot.entries[k].value4 = item->valueC;
            record->pilot.entries[k].value3 = item->valueB;
            record->pilot.entries[k].value2 = item->valueA;
            record->pilot.entries[k].value3 = item->valueB;
            record->pilot.entryItems[k] = index;
        }
    }
}

/* The Escape command: succeeds on half of the rolls, writing the party back
 * to the game data (8009BE0C). */
s32 func_8009A9D0(void) {
    D_800C34B0->commandIndex = 0;
    if (rand() % 100 < 50) {
        func_8009BE0C();
        return 1;
    }
    return 0;
}

/* The Defense command for member: mark it defending; a gear with status 0x10
 * in its second word drops its gear statuses 0x1B0 and the pilot's 0x1000. */
void func_8009AA44(u8 member) {
    D_800C34B0->commandIndex = 0;
    D_800C34B0->records[member].flags15A |= 1;
    if ((D_800C34B0->records[member].flags15A & 0x80) && (D_800C34B0->records[member].gear.status82 & 0x10)) {
        D_800C34B0->records[member].gear.status7C &= 0xFE4F;
        D_800C34B0->records[member].pilot.status7C &= ~0x1000;
    }
    if (D_800D2C34 == 4) {
        D_800C34B0->message = 0x3D;
    }
}

/* End member's defending. */
void func_8009AB00(u8 member) {
    D_800C34B0->records[member].flags15A &= ~1;
}

/* Enable member's Deathblow commands (descriptors 22 and 24-32; in a gear, gear
 * descriptors 21 and 23-28) when its command flag is set. */
void func_8009AB38(u8 member) {
    s32 flag;

    if (D_800C34B0->records[member].flags15A & 0x80) {
        flag = D_800C34B0->records[member].gear.status80 & 0x2000;
    } else {
        flag = D_800C34B0->records[member].pilot.status88.half.active & 0x400;
    }
    if (flag) {
        if (D_800C34B0->records[member].flags15A & 0x80) {
            CommandDescriptor *commands = D_800C34B0->gearCommands[member];

            commands[21].state = 1;
            commands[23].state = 1;
            commands[24].state = 1;
            commands[25].state = 1;
            commands[26].state = 1;
            commands[27].state = 1;
            commands[28].state = 1;
        } else {
            CommandDescriptor *commands = D_800C34B0->partyCommands[member];

            commands[22].state = 1;
            commands[24].state = 1;
            commands[25].state = 1;
            commands[26].state = 1;
            commands[27].state = 1;
            commands[28].state = 1;
            commands[29].state = 1;
            commands[30].state = 1;
            commands[31].state = 1;
            commands[32].state = 1;
        }
    }
}

/* Seal the same commands again and clear member's command flag; unless
 * checked is 0, only for a current command with flag 0x100. */
void func_8009AC48(u8 member, u8 checked) {
    s32 flag;

    if (checked == 0 || (D_800C3DFC->flagsA & 0x100)) {
        if (D_800C34B0->records[member].flags15A & 0x80) {
            flag = D_800C34B0->records[member].gear.status80 & 0x2000;
        } else {
            flag = D_800C34B0->records[member].pilot.status88.half.active & 0x400;
        }
        if (flag) {
            if (D_800C34B0->records[member].flags15A & 0x80) {
                CommandDescriptor *commands = D_800C34B0->gearCommands[member];

                commands[21].state = 0x2000;
                commands[23].state = 0x2000;
                commands[24].state = 0x2000;
                commands[25].state = 0x2000;
                commands[26].state = 0x2000;
                commands[27].state = 0x2000;
                commands[28].state = 0x2000;
                D_800C34B0->records[member].gear.status80 &= ~0x2000;
            } else {
                CommandDescriptor *commands = D_800C34B0->partyCommands[member];

                commands[22].state = 0x2000;
                commands[24].state = 0x2000;
                commands[25].state = 0x2000;
                commands[26].state = 0x2000;
                commands[27].state = 0x2000;
                commands[28].state = 0x2000;
                commands[29].state = 0x2000;
                commands[30].state = 0x2000;
                commands[31].state = 0x2000;
                commands[32].state = 0x2000;
                D_800C34B0->records[member].pilot.status88.half.active &= ~0x400;
            }
        }
    }
}

/* Regeneration amounts of slot for its turn: HP (maxHp / 20), EP (maxEp / 20,
 * from the pilot's or the gear's status) and fuel (maxFuel / 50 for each of
 * two gear statuses). Returns whether any applies; nothing once KO'd. */
s32 func_8009ADA0(u8 slot, s32 *amounts) {
    Combatant *record = &D_800C34B0->records[slot];
    s32 any = 0;

    if (record->pilot.status7C & 0x8000) {
        return 0;
    }
    if (record->pilot.status7C & 0x800) {
        any = 1;
        *amounts = (u16)(record->pilot.maxHp / 20);
    }
    amounts++;
    if (record->pilot.status80 & 0x200) {
        any = 1;
        *amounts = (u16)(record->pilot.maxEp / 20);
    }
    if (record->gear.status7C & 0x200) {
        any = 1;
        *amounts = (u16)(record->pilot.maxEp / 20);
    }
    amounts++;
    *amounts = 0;
    if (record->gear.status7C & 0x80) {
        any = 1;
        *amounts = (u16)(record->gear.maxFuel / 50);
    }
    if (record->gear.status80 & 0x8000) {
        any = 1;
        *amounts += (u16)(record->gear.maxFuel / 50);
    }
    return any;
}

/* Revive slot's record: clear its statuses (keeping status 0x2000 of the
 * second word), size Chu-Chu's gear HP (8009B104), and when character 3 is
 * revived clear status 0x20 of every enemy. */
void func_8009AEFC(u8 slot) {
    Combatant *record;
    u8 i;

    D_800C34B0->commandIndex = 0;
    record = &D_800C34B0->records[slot];
    record->pilot.status7C = 0;
    record->pilot.status84.half.active = 0;
    record->pilot.status88.half.active = 0;
    record->pilot.status8C.half.active = 0;
    record->pilot.status80 &= 0x2000;
    if (record->pilot.characterId == 7) {
        func_8009B104(slot, record);
    }
    if (record->pilot.characterId == 3) {
        for (i = 3; i < 11; i++) {
            D_800CCCE8.records[i].pilot.status80 &= ~0x20;
        }
    }
}

/* Wear the attacker's weapon items for the current command: commands 0-3 the
 * first entry's, 6 the fourth's, 7-19 both. */
void func_8009AFD8(void) {
    switch (D_800C34B0->commandIndex) {
    case 0:
    case 1:
    case 2:
    case 3:
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[0]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[0]] += -1;
        }
        break;
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[0]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[0]] += -1;
        }
        /* fallthrough */
    case 6:
        if (D_8006F8BA[D_800C3E00->pilot.entryItems[3]] != 0) {
            D_8006F8BA[D_800C3E00->pilot.entryItems[3]] += -1;
        }
        break;
    }
}

/* Debug party: members 1 and 2 get 100/100 HP and fixed stats. */
void func_8009B098(void) {
    u8 i;

    for (i = 1; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];

        record->pilot.hp = 100;
        record->pilot.maxHp = 100;
        record->pilot.field5E = 20;
        record->pilot.field5F = 15;
        record->pilot.status7A = 0x1FBF;
        record->field149 = 0;
    }
}

/* Chu-Chu's gear HP: fifty times her HP and maximum HP, capped at 99999. */
void func_8009B104(u8 slot, Combatant *chuchu) {
    Combatant *record = &D_800C34B0->records[slot];
    GearRecord *gear = &record->gear;

    if (chuchu->pilot.maxHp * 50 > 99999) {
        if (chuchu->pilot.hp * 50 > 99999) {
            record->gear.hp = 99999;
        } else {
            record->gear.hp = chuchu->pilot.hp * 50;
        }
        gear->maxHp = 99999;
    } else {
        record->gear.hp = chuchu->pilot.hp * 50;
        record->gear.maxHp = chuchu->pilot.maxHp * 50;
    }
}

/* Debug setup: 99 of every item 1-47, and fixed skill masks for all eleven
 * characters. */
void func_8009B1E4(void) {
    u8 i;

    for (i = 1; i < 48; i++) {
        D_8006F65A[i] = i;
        D_8006F5C4[i] = 99;
    }
    D_8006ECF4[0].mask0 = 0xFFF8;
    D_8006ECF4[0].mask2 = 0xFF00;
    D_8006ECF4[0].mask4 = 0xFFFF;
    D_8006ECF4[0].mask6 = 0xFE00;
    D_8006ECF4[0].flags1A = 0xF000;
    D_8006ECF4[0].field17 = 7;
    D_8006ECF4[1].mask0 = 0xFFE0;
    D_8006ECF4[1].mask2 = 0xFFF0;
    D_8006ECF4[1].mask4 = 0xFFFF;
    D_8006ECF4[1].mask6 = 0xFFF0;
    D_8006ECF4[1].flags1A = 0xC000;
    D_8006ECF4[1].field17 = 7;
    D_8006ECF4[2].mask0 = 0xFFE0;
    D_8006ECF4[2].mask2 = 0xFFE0;
    D_8006ECF4[2].mask4 = 0xFFFF;
    D_8006ECF4[2].mask6 = 0xFF00;
    D_8006ECF4[2].flags1A = 0x8000;
    D_8006ECF4[2].field17 = 7;
    D_8006ECF4[3].mask0 = 0xFFE0;
    D_8006ECF4[3].mask2 = 0xFFC0;
    D_8006ECF4[3].mask4 = 0xFFFF;
    D_8006ECF4[3].mask6 = 0xFF00;
    D_8006ECF4[3].flags1A = 0xF000;
    D_8006ECF4[3].field17 = 7;
    D_8006ECF4[4].mask0 = 0xFFC0;
    D_8006ECF4[4].mask2 = 0xFFC0;
    D_8006ECF4[4].mask4 = 0xFFFF;
    D_8006ECF4[4].mask6 = 0xFC00;
    D_8006ECF4[4].flags1A = 0xE000;
    D_8006ECF4[4].field17 = 7;
    D_8006ECF4[5].mask0 = 0xFFC0;
    D_8006ECF4[5].mask2 = 0xF000;
    D_8006ECF4[5].mask4 = 0xFFFF;
    D_8006ECF4[5].mask6 = 0xF000;
    D_8006ECF4[5].flags1A = 0x8000;
    D_8006ECF4[5].field17 = 7;
    D_8006ECF4[6].mask0 = 0xFFC0;
    D_8006ECF4[6].mask2 = 0xFF00;
    D_8006ECF4[6].mask4 = 0xFFFF;
    D_8006ECF4[6].mask6 = 0xFF00;
    D_8006ECF4[6].flags1A = 0x8000;
    D_8006ECF4[6].field17 = 7;
    D_8006ECF4[7].mask0 = 0;
    D_8006ECF4[7].mask2 = 0xFF00;
    D_8006ECF4[7].mask4 = 0;
    D_8006ECF4[7].mask6 = 0xFF00;
    D_8006ECF4[7].flags1A = 0;
    D_8006ECF4[7].field17 = 7;
    D_8006ECF4[8].mask0 = 0;
    D_8006ECF4[8].mask2 = 0xF800;
    D_8006ECF4[8].mask4 = 0xFFFF;
    D_8006ECF4[8].mask6 = 0;
    D_8006ECF4[8].flags1A = 0xE000;
    D_8006ECF4[8].field17 = 7;
    D_8006ECF4[9].mask0 = 0xFFE0;
    D_8006ECF4[9].mask2 = 0xFFE0;
    D_8006ECF4[9].mask4 = 0xFFFF;
    D_8006ECF4[9].mask6 = 0xFFE0;
    D_8006ECF4[9].flags1A = 0x8000;
    D_8006ECF4[9].field17 = 7;
    D_8006ECF4[10].mask0 = 0xFFC0;
    D_8006ECF4[10].mask2 = 0xFF00;
    D_8006ECF4[10].mask4 = 0xFFFF;
    D_8006ECF4[10].mask6 = 0xFF00;
    D_8006ECF4[10].flags1A = 0xC000;
    D_8006ECF4[10].field17 = 7;
}

#ifdef NON_MATCHING
/* Raise an attack's damage: by half for each of characters 0 and 3 in the
 * party below half HP (gear HP in a gear) and again below a quarter; then a
 * critical chance (10%, 60% with attacker flag 0x200) multiplies it by 1.5
 * (2 with attacker flag 0x400). */
void func_8009B46C(u16 *damage) {
    u8 count = 0;
    u8 i;
    u32 hp;
    u32 maxHp;
    s32 chance;
    s16 scale;

    for (i = 0; i < 3; i++) {
        Combatant *record = &D_800C34B0->records[i];
        GearRecord *gear = &record->gear;

        if (record->pilot.characterId == 0) {
            if (D_800CCCE8.records[i].flags15A & 0x80) {
                maxHp = record->gear.maxHp;
                hp = record->gear.hp;
            } else {
                maxHp = record->pilot.maxHp;
                hp = record->pilot.hp;
            }
            if (hp < maxHp >> 1) {
                count++;
            }
            if (hp < maxHp >> 2) {
                count++;
            }
        }
        if (record->pilot.characterId == 3) {
            if (D_800CCCE8.records[i].flags15A & 0x80) {
                maxHp = gear->maxHp;
                hp = gear->hp;
            } else {
                maxHp = record->pilot.maxHp;
                hp = record->pilot.hp;
            }
            if (hp < maxHp >> 1) {
                count++;
            }
            if (hp < maxHp >> 2) {
                count++;
            }
        }
    }
    chance = 10;
    if (count) {
        *damage += count * (*damage >> 1);
    }
    scale = 3;
    if (D_800C3E00->pilot.flags32 & 0x400) {
        scale = 4;
    }
    if (D_800C3E00->pilot.flags32 & 0x200) {
        chance = 60;
    }
    if (rand() % 100 < chance) {
        *damage = scale * *damage >> 1;
    }
}
#else
INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle", func_8009B46C);
#endif

/* Show the message for an applied status, named by its kind and flag bit. */
void func_8009B684(u8 kind, u16 flag) {
    switch (kind) {
    case 0:
        switch (flag) {
        case 0x2000:
            D_800C34B0->message = 0x1;
            break;
        case 0x1000:
            D_800C34B0->message = 0x2;
            break;
        case 0x800:
            D_800C34B0->message = 0x3;
            break;
        case 0x400:
            D_800C34B0->message = 0x4;
            break;
        case 0x200:
            D_800C34B0->message = 0x5;
            break;
        case 0x1:
            D_800C34B0->message = 0x8;
            break;
        }
        break;
    case 2:
        switch (flag) {
        case 0x2000:
            D_800C34B0->message = 0x9;
            break;
        case 0x1000:
            D_800C34B0->message = 0xA;
            break;
        case 0x800:
            D_800C34B0->message = 0xB;
            break;
        case 0x400:
            D_800C34B0->message = 0xC;
            break;
        case 0x1:
            D_800C34B0->message = 0xD;
            break;
        case 0x20:
            D_800C34B0->message = 0x7;
            break;
        }
        break;
    case 5:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0xE;
            break;
        case 0x4000:
            D_800C34B0->message = 0xF;
            break;
        case 0x2000:
            D_800C34B0->message = 0x10;
            break;
        case 0x1000:
            D_800C34B0->message = 0x11;
            break;
        case 0x800:
            D_800C34B0->message = 0x12;
            break;
        case 0x1800:
            D_800C34B0->message = 0x13;
            break;
        }
        break;
    case 7:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0x15;
            break;
        case 0x4000:
            D_800C34B0->message = 0x16;
            break;
        case 0x1000:
            D_800C34B0->message = 0x18;
            break;
        case 0x2:
        case 0x8:
            D_800C34B0->message = 0x19;
            break;
        case 0x1:
        case 0x4:
            D_800C34B0->message = 0x1A;
            break;
        }
        break;
    case 9:
        switch (flag) {
        case 0x8000:
            D_800C34B0->message = 0x1B;
            break;
        case 0x4000:
            D_800C34B0->message = 0x1C;
            break;
        case 0x2000:
            D_800C34B0->message = 0x1D;
            break;
        case 0x1000:
            D_800C34B0->message = 0x1E;
            break;
        case 0x400:
            D_800C34B0->message = 0x1F;
            break;
        case 0x800:
            D_800C34B0->message = 0x20;
            break;
        case 0x100:
            D_800C34B0->message = 0x21;
            break;
        case 0x200:
            D_800C34B0->message = 0x22;
            break;
        }
        break;
    }
}

/* Choose an automatic action for slot (confusion or auto-battle): choice[0]
 * is the kind (4 defend, 2 a skill with choice[1] its index among the
 * character's usable skills, 0/1 an attack with choice[1] its strength). Out of
 * a gear: defend on 10% unless flagged, a known skill on 25%, then Chu-Chu's
 * basic attack; otherwise attack weak 48%, medium 32%, strong 20%. */
void func_8009BAC4(u8 slot, u8 *choice, s16 *busy) {
    Combatant *record = &D_800CCCE8.records[slot];
    u16 skills;
    u8 count;
    u8 skill;

    if (!(D_800CCCE8.records[slot].flags15A & 0x80)) {
        if (rand() % 100 < 10 && !(record->pilot.status7A & 0x100) && *busy == 0) {
            choice[0] = 4;
            return;
        }
        switch (record->pilot.characterId) {
        case 3:
            skills = 0xC3C0;
            count = 10;
            break;
        case 4:
            skills = 0xDF80;
            count = 10;
            break;
        case 5:
            skills = 0x1000;
            count = 4;
            break;
        case 7:
            skills = 0xE000;
            count = 3;
            break;
        case 0:
        case 8:
            skills = 0xC000;
            count = 2;
            break;
        case 2:
        case 9:
            skills = 0xBFE0;
            count = 11;
            break;
        case 1:
        case 6:
        case 10:
            skills = 0xF000;
            count = 4;
            break;
        }
        if (rand() % 100 < 25 && !(record->pilot.status7A & 0x20)) {
            choice[0] = 2;
            skill = rand() % count;
            if (D_8006ECF4[record->pilot.characterId].mask2 & ((0x8000 >> skill) & skills)) {
                choice[1] = skill;
                return;
            }
        }
        if (record->pilot.characterId == 8) {
            choice[0] = 0;
            choice[1] = 0;
            return;
        }
    }
    choice[0] = 1;
    if (rand() % 100 < 80) {
        if (rand() % 100 >= 60) {
            choice[1] = 1;
        } else {
            choice[1] = 0;
        }
    } else {
        choice[1] = 2;
    }
}

/* Damage the target by the command's power in twentieths of its gear's
 * maximum HP. */
void func_8009BD94(void) {
    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = D_800C3DFC->power * D_800D2DC8->maxHp / 20;
}

/* Write the party back to the game data: each present member's HP (Chu-Chu in
 * a gear takes hers from the gear's HP), EP, use counts and field 0x3A, with
 * HP 1 when KO'd, and the HP and fuel of its gear (ids 0-6 and 8-16), a tenth
 * of the maximum when the gear is wrecked. */
void func_8009BE0C(void) {
    u8 i;
    u8 j;
    s32 unused[2]; /* never used; it gives the original its 8-byte frame */

    for (i = 0; i < 3; i++) {
        Combatant *record;
        CharacterRecord *character;
        GearRecord *gearRecord;
        GearRecord *gear;

        if (D_800D2D24[i] == 0x7F) {
            continue;
        }
        record = &D_800CCCE8.records[i];
        character = &D_8006D8A0.characters[record->pilot.characterId];
        gear = &D_800CCCE8.records[i].gear;
        gearRecord = &D_8006D8A0.gears[record->pilot.gearId];
        if (record->pilot.characterId == 7 && (D_800CCCE8.records[i].flags15A & 0x80)) {
            record->pilot.hp = (gear->hp + 1) / 50;
            if (record->pilot.hp == 0) {
                record->pilot.hp = 1;
            }
        }
        character->hp = record->pilot.hp;
        character->ep = record->pilot.ep;
        if (character->hp > character->maxHp) {
            character->hp = character->maxHp;
        }
        if (character->ep > character->maxEp) {
            character->ep = character->maxEp;
        }
        for (j = 0; j < 7; j++) {
            character->useCounts[j] = record->pilot.useCounts[j];
        }
        character->field3A = record->pilot.field3A;
        if (record->pilot.status7C & 0xC000) {
            character->hp = 1;
        }
        switch (record->pilot.gearId) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            gearRecord->hp = gear->hp;
            gearRecord->fuel = gear->fuel;
            if (gearRecord->hp > gearRecord->maxHp) {
                gearRecord->hp = gearRecord->maxHp;
            }
            if (gearRecord->fuel > gearRecord->maxFuel) {
                gearRecord->fuel = gearRecord->maxFuel;
            }
            if (gear->status7C & 0x8000) {
                gearRecord->hp = gearRecord->maxHp / 10;
            }
            break;
        }
    }
}

/* Gear warning flags of slot: 1 gear status 0x400, 2 gear HP below an eighth
 * (unless the pilot's flag 1 at +0x36), 4 when field 0x148 is 4. */
s32 func_8009C050(u8 slot) {
    Combatant *record = &D_800CCCE8.records[slot];
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    u8 *state = &D_800CCCE8.records[slot].field148;
    u8 flags = 0;

    if (gear->status7C & 0x400) {
        flags = 1;
    }

    if (gear->hp < gear->maxHp >> 3 && !(record->pilot.flags36 & 1)) {
        flags |= 2;
    }
    if (*state == 4) {
        flags |= 4;
    }
    return flags;
}

/* Put slot into state 4 with timer 6 at three turns, and set flag 0x4000 of
 * character 0. */
void func_8009C0E0(u8 slot) {
    D_800CCCE8.records[slot].field148 = 4;
    D_800CCCE8.records[slot].statusTimers[6] = 3;
    D_8006ECF4[0].flags1A |= 0x4000;
}

/* Target the party member that is character 3. */
void func_8009C134(void) {
    u8 i;

    for (i = 0; i < 3; i++) {
        if (D_800CCCE8.records[i].pilot.characterId == 3) {
            D_800CCCE8.targetMask = 1 << i;
        }
    }
}

/* Resolve a gear's action against every target in the target mask: select the
 * current command descriptor, run its gear formula (table D_800C34DC) per
 * target with the status step it asks for (8009DBFC), then the per-target
 * follow-ups and the command's wear and element. */
void func_8009C198(void) {
    s32 bit;

    if (D_800C3E04 < 3) {
        D_800C3DFC = &D_800C34B0->gearCommands[D_800C34B0->attackerIndex][D_800C34B0->commandIndex];
    } else {
        D_800C3DFC = &D_800C34B0->enemyCommands[D_800C34B0->commandIndex];
    }
    /* Called without its second argument (an unprototyped call). */
    ((void (*)())func_8009AC48)(D_800C3E04);
    D_800C3D3C = &D_800C34B0->records[D_800C34B0->attackerIndex].field148;
    D_800D2DC4 = 0;
    if ((D_800C3DFC->flagsA & 0x100) && D_800C3E00->pilot.characterId == 1) {
        func_80096824();
    }
    if ((D_800C3E00->pilot.gearId == 5 || D_800C3E00->pilot.gearId == 13) && D_800C34B0->commandIndex == 1) {
        D_800C3DFC->attributes[2] = D_800D2D6C->entries[1].valueE;
    }
    bit = 1;
    for (D_800C3E50 = 0; D_800C3E50 < 11; D_800C3E50++, bit <<= 1) {
        if (bit & D_800C34B0->targetMask) {
            D_800C3E34 = &D_800C34B0->records[D_800C3E50];
            D_800D2DC8 = &D_800C34B0->records[D_800C3E50].gear;
            D_800C3D60 = &D_800C34B0->records[D_800C3E50].field148;
            func_8009CA90();
            D_800C34DC[D_800C3DFC->formula]();
            if (D_800C34B0->resultCode[D_800C3E50] == 0) {
                if (D_800C3DFC->flagsA & 0x800) {
                    func_8009DBFC(1);
                } else if (D_800C3DFC->flagsA & 0x4000) {
                    func_8009DBFC(0);
                }
            }
            D_800C34B0->shownCommand = D_800C3DFC->name;
            func_8009C4B4();
            func_8009CB68(D_800C3E50);
        }
    }
    if (D_800C3E00->pilot.characterId == 4) {
        func_8009E788();
    }
    func_8009C9C4();
}

/* Per-target follow-up of a gear action: a party gear's attack level (up by
 * one with commands 0-2, set below D_800D2C34 by the level-3/6/9 command
 * groups, which also raise the pilot's field 0x54), then the target's
 * reactions: breaking status 0x2000 on 80%, fuel-drain immunity, halving or
 * raising damage by its flag 0x80, reflecting it with flag 0x20, and nullifying
 * with status 0x200. */
void func_8009C4B4(void) {
    s32 chance;

    if (D_800C3E04 < 3) {
        if (D_800C34B0->commandIndex < 3) {
            if (++D_800C3D3C[0] > D_800C3D3C[1]) {
                D_800C3D3C[0]--;
            }
        }
        if ((u32)(D_800C34B0->commandIndex - 3) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 1;
        }
        if ((u32)(D_800C34B0->commandIndex - 6) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 2;
        }
        if ((u32)(D_800C34B0->commandIndex - 9) < 3) {
            D_800C3D3C[0] = D_800D2C34 - 3;
        }
        if (D_800C34B0->commandIndex >= 3 && D_800C34B0->commandIndex < 12) {
            D_800C3E00->pilot.field54 += D_800C34B0->commandIndex / 3;
        }
        if (D_800C34B0->commandIndex == 5) {
            D_800C3E00->pilot.field54 += 1;
        }
        if (D_800C34B0->commandIndex == 8) {
            D_800C3E00->pilot.field54 += 2;
        }
        if (D_800C34B0->commandIndex == 11) {
            D_800C3E00->pilot.field54 += 3;
        }
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 0 && (D_800C3E34->pilot.status80 & 0x2000)
        && rand() % 100 < 80) {
        D_800C3E34->pilot.status80 &= ~0x2000;
        D_800D2DC8->status7C &= ~0x1000;
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 10 && (D_800D2DC8->field7E & 0x80)) {
        D_800C34B0->damage[D_800C3E50] = 0;
    }
    if (D_800C34B0->resultCode[D_800C3E50] == 0) {
        if (D_800C3E34->pilot.flags32 & 0x80) {
            chance = 60;
            if (D_800C3E34->pilot.characterId == 0) {
                chance = 80;
            }
            if (rand() % 100 < chance) {
                D_800C34B0->damage[D_800C3E50] >>= 1;
            } else {
                D_800C34B0->damage[D_800C3E50] += D_800C34B0->damage[D_800C3E50] >> 1;
            }
        }
        if (D_800C3E34->pilot.flags32 & 0x20) {
            D_800C34B0->resultCode[D_800C3E04] = 0;
            D_800C34B0->damage[D_800C3E04] = D_800C34B0->damage[D_800C3E50];
        }
    }
    if ((D_800C3E34->pilot.status88.half.permanent & 0x200) && D_800C34B0->resultCode[D_800C3E50] == 1) {
        D_800C34B0->damage[D_800C3E50] = 0;
    }
}

/* The gear version of 80099FB0: make the current command descriptor the
 * battle's current command, a command without an element taking the gear's
 * element statuses. */
void func_8009C9C4(void) {
    u16 elements = (D_800D2D6C->status84.half.active | D_800C3E00->pilot.status88.half.permanent) >> 12;

    D_800C34B0->commandAttributes[0] = D_800C3DFC->attributes[0];
    D_800C34B0->commandAttributes[1] = D_800C3DFC->attributes[1];
    D_800C34B0->commandAttributes[2] = D_800C3DFC->attributes[2];
    D_800C34B0->commandAttributes[3] = D_800C3DFC->attributes[3];
    D_800C34B0->commandIndexCopy = D_800C34B0->commandIndex;
    if ((D_800C34B0->commandAttributes[2] & 0x3F) == 0) {
        D_800C34B0->commandAttributes[2] |= elements;
    }
}

/* Against a target on foot, switch the current descriptor to its on-foot
 * variant: commands 12-14 three descriptors on, commands 0-2 fifteen. */
void func_8009CA90(void) {
    if ((u32)(D_800C34B0->commandIndex - 12) < 3 && !(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        D_800C3DFC += 3;
    } else if (D_800C34B0->commandIndex < 3 && !(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        D_800C3DFC += 15;
    }
}

/* Mirror the gear's status 0x200 (second word) as the pilot's status 0x20. */
void func_8009CB68(u8 slot) {
    GearRecord *gear = &D_800CCCE8.records[slot].gear;
    Combatant *record = &D_800CCCE8.records[slot];

    if (gear->status80 & 0x200) {
        record->pilot.status84.half.active |= 0x20;
    } else {
        record->pilot.status84.half.active &= ~0x20;
    }
}

/* Gear attack damage: the gear hit outcome, attack and defense values with
 * the element adjustment and both gears' boost/break statuses, the command's
 * drain effects, then (5a - 4d for ether, else 4a - 3d) times the power over
 * 20, a random spread, the element resistance and the hit outcome's result
 * code; at most 9999. */
void func_8009CBC4(void) {
    u16 attack;
    u16 defense;
    s8 hit;
    u8 power;
    s32 damage;
    s32 attackScale;
    s32 defenseScale;
    u16 flags;
    u8 guard;

    power = D_800C3DFC->power;
    hit = func_8009D3A0();
    attack = func_8009D948();
    defense = func_8009DA04();
    func_80096494(&attack, &defense, &hit);
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 8) {
        attack += attack / 5;
    }
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 2) {
        attack += attack / 10;
    }
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 4) {
        attack -= attack / 5;
    }
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 1) {
        attack -= attack / 10;
    }
    if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 4) {
        defense += defense / 5;
    }
    if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 1) {
        defense += defense / 10;
    }
    if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 8) {
        defense -= defense / 5;
    }
    if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 2) {
        defense -= defense / 10;
    }
    if (D_800C3DFC->attributes[2] & 0x10) {
        if (!(D_800C3E34->pilot.status82 & 0x40)) {
            D_800C3E34->pilot.status80 |= 0x40;
        }
        if ((D_800D2D6C->status84.half.active | D_800D2D6C->status84.half.permanent) & 0x4000) {
            D_800D2C88[D_800C3E04] = 3;
            D_800D2C54[D_800C3E04] = (u16)(D_800C3E00->pilot.maxEp / 10) * 2;
        }
        if ((D_800D2D6C->status84.half.active | D_800D2D6C->status84.half.permanent) & 0x1000) {
            D_800D2C88[D_800C3E04] = 2;
            D_800D2C54[D_800C3E04] = D_800D2D6C->maxHp / 10 * 2;
        }
    }
    if ((D_800C3DFC->attributes[2] & 0x20) && !(D_800C3E34->pilot.status82 & 0x80)) {
        D_800C3E34->pilot.status80 |= 0x80;
    }
    if (D_800C3E34->pilot.status80 & 0x40) {
        defense -= defense >> 2;
        D_800C3E34->pilot.status80 &= 0xFFBF;
    }
    if (D_800C3E00->pilot.status80 & 0x80) {
        attack -= attack >> 2;
        D_800C3E00->pilot.status80 &= 0xFF7F;
    }
    flags = D_800C3DFC->flagsA;
    if (flags & 0x400) {
        power = 20;
    }
    if (flags & 0x100) {
        attackScale = 5;
        defenseScale = 4;
    } else {
        attackScale = 4;
        defenseScale = 3;
    }
    if (defense != 0) {
        damage = attackScale * attack - defenseScale * defense;
    } else {
        damage = attackScale * attack;
    }
    switch (D_800C3DFC->amountKind) {
    case 0:
    case 1:
        damage = power * damage / 20;
        break;
    case 2:
        break;
    }
    if (damage <= 0) {
        damage = 0;
    } else if (damage < 15) {
        damage += rand() % 3;
    } else {
        damage += rand() % (damage / 15 + 2);
    }
    if (D_800C3DFC->elements != 0) {
        damage = func_8009DB54(damage);
    }
    switch (hit) {
    case 1:
        D_800C34B0->resultCode[D_800C3E50] = 0;
        break;
    case 2:
        D_800C34B0->resultCode[D_800C3E50] = 5;
        guard = D_800D2DC8->guard;
        if (guard >= 10) {
            guard = 9;
        }
        if (damage != 0) {
            damage = damage * (10 - guard) / 20;
        }
        break;
    case 3:
        damage = 0;
        D_800C34B0->resultCode[D_800C3E50] = 4;
        break;
    case 4:
        D_800C34B0->resultCode[D_800C3E50] = 2;
        break;
    }
    if (D_800D2DC4 && (D_800C3DFC->flagsA & 0x100) && damage != 0) {
        damage /= 3;
    }
    if (damage >= 10000) {
        damage = 9999;
    }
    if (damage < 0) {
        damage = 0;
    }
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Mark the target missed (result 6) when 8009DBFC finds no hit. */
void func_8009D354(void) {
    if (func_8009DBFC(0) == 0) {
        D_800C34B0->resultCode[D_800C3E50] = 6;
    }
}

/* Gear hit outcome of the current command on the target: 1 hit, 2 half,
 * 3 miss. Like 80096ab8 with gear accuracy (+0x9f with a broken weapon,
 * 1.5x with status 0x800), the target's evasion (half its gear's +0x9f,
 * 1.5x with status 0x400) and the gears' blind/evade status 0x10. */
s8 func_8009D3A0(void) {
    s16 penalty = 0;
    s16 bonus = 0;
    s16 evasion;
    s16 accuracy;
    u8 durability;
    s16 margin;
    s16 roll;
    u16 status;

    if ((D_800C3DFC->flagsA & 0x200) && (D_800C3E34->pilot.flags34 & 8)) {
        return 3;
    }
    if (D_800C3DFC->flagsA & 0x1000) {
        return 3;
    }
    if ((D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent) & 0x100) {
        return 3;
    }
    if (D_800C3E34->pilot.status7C & 0x2000) {
        return 1;
    }
    if (D_800C3E34->pilot.status80 & 0x1000) {
        return 1;
    }
    if (D_800C3DFC->flagsA & 0x8000) {
        return 1;
    }
    evasion = D_800C3E34->pilot.field5F;
    durability = D_8006F8EA[D_800D2D6C->partItems[0]];
    accuracy = D_800C3E00->pilot.field5E;
    if (durability == 0) {
        accuracy += D_800D2D6C->hitBonus;
    }
    if (D_800D2DC8->hitBonus != 0) {
        evasion += D_800D2DC8->hitBonus / 2;
    }
    if (D_800C3E00->pilot.characterId == 4) {
        if ((D_800C3DFC->itemKinds & 0x80) && durability == 0) {
            return 3;
        }
        if ((D_800C3DFC->itemKinds & 0x20) && D_8006F8EA[D_800D2D6C->partItems[3]] == 0) {
            return 3;
        }
    }
    if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 0x800) {
        accuracy += accuracy / 2;
    }
    if (D_800D2D6C->status7C & 0x10) {
        penalty = 60;
    }
    if (D_800C3DFC->flagsA & 0x1000) {
        return 3;
    }
    if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
        if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 0x400) {
            evasion += evasion / 2;
        }
        if (D_800D2DC8->status7C & 0x10) {
            bonus = 30;
        }
        if (D_800D2DC8->status7C & 0xC00) {
            return 1;
        }
    } else {
        accuracy /= 2;
        if (D_800C3E34->pilot.status7C & 0x2000) {
            return 1;
        }
        if (D_800C3E34->pilot.status80 & 0x1000) {
            return 1;
        }
        if ((D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent) & 0x800) {
            evasion += evasion / 2;
        }
    }
    if (D_800C34B0->records[D_800C3E50].flags15A & 1) {
        if (rand() % 100 < 95) {
            return 2;
        }
        return 1;
    }
    margin = accuracy + D_800C3DFC->hitBonus - evasion;
    status = D_800C3E34->pilot.status84.half.active | D_800C3E34->pilot.status84.half.permanent;
    if (status & 0x20) {
        roll = rand() % 100 - margin;
        if (roll >= 50) {
            return 3;
        }
        return 1;
    }
    if (status & 0x40) {
        roll = rand() % 100 - margin;
        if (roll >= 50) {
            return 2;
        }
        return 1;
    }
    roll = rand() % 100 - margin;
    bonus += 85;
    bonus -= penalty;
    if (roll >= bonus) {
        return 3;
    }
    roll = rand() % 100 - margin;
    if (roll >= bonus) {
        return 2;
    }
    return 1;
}

/* Damage of a gear attack (80096FBC); a command with flag 0x100 scales it by
 * the gear's frame factor in quarters (4 when unset or with status 0x100), and
 * status 0x40 adds half again. */
u16 func_8009D948(void) {
    u16 damage = func_80096FBC();
    u8 factor;

    if (D_800C3DFC->flagsA & 0x100) {
        factor = D_800D2D6C->frameFactor;
        if ((D_800D2D6C->status7C & 0x100) || factor == 0) {
            factor = 4;
        }
        damage = factor * damage / 4;
        if ((D_800D2D6C->status80 | D_800D2D6C->status82) & 0x40) {
            damage += damage >> 1;
        }
    }
    return damage;
}

/* Damage of an attack on a gear (80097610): a command with flag 0x100 adds
 * the target gear's armor (reduced by its defense percentage) when the target
 * is in a gear, and half again with status 0x20; otherwise the damage itself
 * is reduced by the defense percentage. */
u16 func_8009DA04(void) {
    u16 damage = func_80097610();
    u16 armor;
    u8 defense;

    if (D_800C3DFC->flagsA & 0x100) {
        defense = D_800D2DC8->defense;
        armor = D_800D2DC8->armor;
        if (defense != 0 && armor != 0) {
            armor = armor * (100 - defense) / 100;
        }
        if (D_800C34B0->records[D_800C3E50].flags15A & 0x80) {
            damage += armor;
        }
        if ((D_800D2DC8->status80 | D_800D2DC8->status82) & 0x20) {
            damage += damage >> 1;
        }
    } else if (D_800D2DC8->defense != 0) {
        damage = damage * (100 - D_800D2DC8->defense) / 100;
    }
    return damage;
}

/* Scale damage by the target gear's resistance (in twentieths, 20 or more
 * nullifies) to the command's first element. */
s32 func_8009DB54(s32 damage) {
    u8 i;
    u8 element;
    u8 resistance;

    for (i = 0; i < 16; i++) {
        if (D_800C3DFC->elements & (0x8000 >> i)) {
            element = i;
            break;
        }
    }
    resistance = D_800D2DC8->resistances[element];
    if (resistance != 0) {
        if (resistance < 20) {
            damage = damage * (20 - resistance) / 20;
        } else {
            damage = 0;
        }
    }
    return damage;
}

/* Roll a status onto the target's gear, from the attacking gear's first
 * part (`fromGear`: chance +0x13, kind +0x14, flag +0x10, 5 turns) or from
 * the command (+0x1c/+0x1d/+0x1e, turns +0x11): cancel opposite statuses,
 * check immunities, set the kind's status word and turn timer, or apply the
 * special kinds 10-16. Returns 1 when it took. */
s8 func_8009DBFC(u8 fromGear) {
    Combatant *record = &D_800C34B0->records[D_800C3E50];
    CharacterRecord *pilot = &record->pilot;
    s8 chance;
    u8 kind;
    u16 flags;
    u8 turns;

    if (fromGear) {
        chance = D_800D2D6C->entries[0].value10;
        kind = D_800D2D6C->entries[0].value11;
        flags = D_800D2D6C->entries[0].field0;
        turns = 5;
    } else {
        chance = D_800C3DFC->field1C;
        kind = D_800C3DFC->field1D;
        flags = D_800C3DFC->field1E;
        turns = D_800C3DFC->power;
    }
    if (!(D_800C34B0->records[D_800C3E50].flags15A & 0x80)) {
        return 0;
    }
    if (chance < rand() % 100) {
        return 0;
    }
    switch (kind) {
    case 0:
        if ((flags & 0x20) && (D_800D2DC8->status80 & 0x8000)) {
            D_800D2DC8->status80 &= 0x7FFF;
            D_800C3E34->pilot.status84.half.active &= 0x7FFF;
            return 1;
        }
        break;
    case 1:
        if (flags & 0xA) {
            if (D_800D2DC8->status80 & 5) {
                D_800D2DC8->status80 &= 0xFFFA;
                return 1;
            }
        } else if (flags & 5) {
            if (D_800D2DC8->status80 & 0xA) {
                D_800D2DC8->status80 &= 0xFFF5;
                return 1;
            }
        }
        break;
    }
    if (kind == 0) {
        if (flags & D_800D2DC8->field7E) {
            return 0;
        }
        switch (flags) {
        case 0x400:
            record->statusTimers[0] = turns;
            D_800D2DC8->status7C &= 0xFBFF;
            pilot->status7C |= 0x2000;
            break;
        case 0x1000:
            D_800D2DC8->status7C &= 0xEFFF;
            pilot->status80 |= 0x2000;
            break;
        case 0x200:
            record->statusTimers[1] = turns;
            break;
        case 0x100:
            record->statusTimers[2] = turns;
            break;
        case 0x80:
            record->statusTimers[3] = turns;
            break;
        case 0x20:
            record->statusTimers[4] = turns;
            pilot->status7C |= 0x1000;
            break;
        case 0x10:
            record->statusTimers[5] = turns;
            break;
        }
        D_800D2DC8->status7C |= flags;
    }
    if (kind == 1) {
        D_800D2DC8->status80 |= flags;
        switch (flags) {
        case 0x1000:
            record->statusTimers[10] = turns;
            break;
        case 0x40:
            record->statusTimers[11] = turns;
            break;
        case 0x20:
            record->statusTimers[12] = turns;
            break;
        }
    }
    if (kind == 3) {
        if (flags & 0xF000) {
            if (D_800D2DC8->status84.half.permanent & 0xF000) {
                return 0;
            }
            D_800D2DC8->status84.half.active = flags | (D_800D2DC8->status84.half.active & 0xFFF);
            record->statusTimers[7] = turns;
        }
        if (flags & 0xF00) {
            if (D_800D2DC8->status84.half.permanent & 0xF00) {
                return 0;
            }
            D_800D2DC8->status84.half.active = flags | (D_800D2DC8->status84.half.active & 0xF0FF);
            record->statusTimers[8] = turns;
        }
    }
    if (kind == 10) {
        D_800D2DC8->status84.half.active &= ~flags;
    }
    if (kind == 11 && !(D_800D2DC8->field7E & 0x40)) {
        D_800D2DC8->defense += flags;
        kind = 0;
        if (D_800D2DC8->defense >= 100) {
            D_800D2DC8->defense = 99;
        }
        flags = 0x40;
    }
    if (kind == 12) {
        D_800C34B0->resultCode[D_800C3E50] = 0;
        D_800C34B0->damage[D_800C3E50] = D_800D2DC8->hp / flags;
    }
    if (kind == 13) {
        D_800C34B0->resultCode[D_800C3E50] = 0;
        D_800C34B0->damage[D_800C3E50] = D_800D2DC8->hp - 1;
    }
    if (kind == 14) {
        D_800D2DC8->status80 = 0;
        D_800D2DC8->status84.half.active = 0;
        if (flags == 1) {
            D_800D2DC8->status82 = 0;
            D_800D2DC8->status84.half.permanent = 0;
        }
    }
    if (kind == 16) {
        D_800D2DC8->status7C |= 1;
        D_800C3E34->pilot.status7C |= 0x80;
    }
    if (kind == 15) {
        D_800D2DC8->status7C &= 0xFFFE;
        D_800C3E34->pilot.status7C &= 0xFF7F;
    }
    func_8009E868(kind, flags);
    return 1;
}

/* Clear the target gear's defense percentage. */
void func_8009E268(void) {
    D_800D2DC8->defense = 0;
}

/* Damage the target gear by field 0x4F tenths of its maximum HP. */
void func_8009E278(void) {
    u32 damage = D_800D2DC8->field4F * D_800D2DC8->maxHp / 10;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Damage the target gear by the command's power in twentieths of its maximum
 * HP. */
void func_8009E2EC(void) {
    u32 damage = D_800C3DFC->power * D_800D2DC8->maxHp / 20;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Damage the target by the attacker's accuracy times the command's power. */
void func_8009E364(void) {
    u32 damage = D_800C3E00->pilot.accuracy * D_800C3DFC->power;

    D_800C34B0->resultCode[D_800C3E50] = 2;
    D_800C34B0->damage[D_800C3E50] = damage;
}

/* Put the target into state 4 with timer 6 at three turns. */
void func_8009E3C8(void) {
    *D_800C3D60 = 4;
    D_800CCCE8.records[D_800C3E50].statusTimers[6] = 3;
}

/* Drain the target gear's fuel (result 10) by the command's power in
 * twentieths of its maximum fuel. */
void func_8009E410(void) {
    s32 amount = D_800D2DC8->maxFuel * D_800C3DFC->power / 20;

    D_800C34B0->resultCode[D_800C3E50] = 10;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* Restore the target gear's fuel (result 11) by the command's power in
 * twentieths of its maximum fuel. */
void func_8009E48C(void) {
    s32 amount = D_800D2DC8->maxFuel * D_800C3DFC->power / 20;

    D_800C34B0->resultCode[D_800C3E50] = 11;
    D_800C34B0->damage[D_800C3E50] = amount;
}

/* Clear the target gear's statuses 0x7F4 and the target's status 0x20. */
void func_8009E508(void) {
    D_800D2DC8->status7C &= 0xF80B;
    D_800C3E34->pilot.status7A &= ~0x20;
}
