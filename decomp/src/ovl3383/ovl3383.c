/* ovl3383: a battle module at 0x801fc000. The battle overlay's 800beb04 loads
 * the current battle's module into 0x801fc000..0x80200000 (the slot above the
 * resident heap, which the boot code bounds at 0x801fc000): file
 * (D_800591B3 + 2) of the battle directory, D_800591B3 being the platform
 * bits (6..11) of the directory header of battle file 2, whenever they differ
 * from the loaded module's (D_800591B2). Battle script opcodes call fixed
 * entry addresses in the loaded module: this one provides 801fc53c, called by
 * the opcode handler 800b6b98, which starts an effect circling an actor.
 *
 * The module was built by a compiler that schedules %hi/%lo halves of
 * addresses separately (lui far from its lw/sw/addiu, even in delay slots)
 * and keeps positive li as addiu; the qualified GCC 2.6.3/2.7.2 + ASPSX 2.34
 * do neither, so functions addressing symbols stay NON_MATCHING. */
#include "spin.h"

/* Advance the effect's angle by its step. */
void func_801FC000(TaskNode *node) {
    SpinTask *spin = node->object;

    spin->angle += spin->step;
}

INCLUDE_ASM(".local/decomp/ovl3383/asm/nonmatchings/ovl3383", func_801FC020);

/* Opcode entry: start the effect circling `actor` from `angle`, advancing by
 * `step` each frame (operands from the battle script, see 800b6b98). */
#ifdef NON_MATCHING
void func_801FC53C(Actor *actor, s32 angle, s32 radius, s32 arg3, s32 arg4, s32 arg5, s32 step) {
    SpinTask *spin;

    spin = func_8001D1D8(sizeof(SpinTask), actor->task, func_801FC000, func_801FC020, NULL);
    spin->actor = actor;
    spin->radius = radius;
    spin->arg3 = arg3;
    spin->arg4 = arg4;
    spin->arg5 = arg5;
    spin->angle = angle;
    spin->step = step;
}
#else
INCLUDE_ASM(".local/decomp/ovl3383/asm/nonmatchings/ovl3383", func_801FC53C);
#endif
