/* ovl3387: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file
 * (D_800591B3 + 2) of the battle directory, D_800591B3 being the platform
 * bits (6..11) of the directory header of battle file 2, whenever they differ
 * from the loaded module's (D_800591B2). Battle script opcodes call fixed
 * entry addresses in the loaded module: this one provides 801fc898, called by
 * the opcode handler 800b3f04, which plays a full-screen effect in its own
 * frame loop on a private stack.
 *
 * The module was built by a compiler that schedules %hi/%lo halves of
 * addresses separately (lui far from its lw/sw/addiu, even in delay slots)
 * and keeps positive li as addiu; the qualified GCC 2.6.3/2.7.2 + ASPSX 2.34
 * do neither, so functions addressing symbols stay NON_MATCHING. */
#include "burst.h"

/* Advance the effect one frame (two variants), fading it out after 100 or 24
 * frames. The empty loops over a 2x14x20 grid are left from removed work. */
#ifdef NON_MATCHING
void func_801FC000(TaskNode *node) {
    Burst *burst = node->object;
    s32 i, j, k;

    if (D_801FCE14 != 0) {
        burst->speed++;
        burst->frame++;
        burst->angle += 0x80;
        burst->twist += 0x40;
        burst->height -= 0x1E;
        burst->angle += burst->speed >> 2;
        if (burst->frame > 100) {
            burst->brightness -= 4;
        }
    } else {
        burst->speed += 10;
        burst->twist += 0x600;
        burst->frame++;
        burst->size += 10;
        burst->height -= burst->speed;
        if (burst->frame > 24) {
            burst->brightness -= 0x14;
        }
    }
    for (k = 0; k != 2; k++) {
        for (j = 0; j != 14; j++) {
            for (i = 0; i != 20; i++) {
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl3387/asm/nonmatchings/ovl3387", func_801FC000);
#endif

INCLUDE_ASM(".local/decomp/ovl3387/asm/nonmatchings/ovl3387", func_801FC11C);

/* Wait for drawing to finish and release the effect. */
void func_801FC400(Burst *burst) {
    DrawSync(0);
    func_800320E8(burst);
}

/* Unlink a task-registered effect and release it after the frame. */
void func_801FC434(TaskNode *node) {
    func_8001CB48(node + 1);
    func_8001CD94(node);
    func_80025180(node);
}

/* Allocate and set up the effect's state. */
Burst *func_801FC470(void) {
    Burst *burst = func_80031BDC(sizeof(Burst), 1);

    burst->task.object = burst;
    burst->draw.object = burst;
    return func_801FC4A8(burst);
}

INCLUDE_ASM(".local/decomp/ovl3387/asm/nonmatchings/ovl3387", func_801FC4A8);

/* Opcode entry: run the effect on a private 8 KB stack (its frame loop needs
 * more than the battle's). */
void func_801FC898(void) {
    u8 *stack = func_80031BDC(0x2000, 0);

    /* Push the caller's sp at the new stack top and switch to it. */
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                     :
                     : "r"(stack + 0x1F00)
                     : "$8", "memory");
    func_801FC8F4();
    __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
    func_800320E8(stack);
}

INCLUDE_ASM(".local/decomp/ovl3387/asm/nonmatchings/ovl3387", func_801FC8F4);
