/* Battle unit from 800B7870 to 800B8098 (Cygnus CDK GCC 2.7.2).
 * 800B7870's jump table at 0x800709FC sits at 4 mod 8 directly after
 * 800B3F04's odd-length table at 0 mod 8, so a unit starts between the two
 * (placed at the first function with rodata); its own 5-entry table is
 * followed directly by 800B8098's at 0x80070A10 (0 mod 8). */
#include "common.h"
#include "battle_core.h"
#include "combatant.h"
#include "model.h"
#include "scene.h"
#include "gte.h"
#include "effect.h"
#include "objects.h"
#include "screen.h"
#include "sprite.h"
#include "actor.h"
#include "popup.h"
#include "frame.h"
#include "stage.h"

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B7870", func_800B7870);

/* Clear D_800D2FDC. */
void func_800B7C28(void) {
    D_800D2FDC = 0;
}

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B7870", func_800B7C34);

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800B7870", func_800B7E94);

/* Set the acting sprite of a single action. */
void func_800B8048(BattleSprite *sprite) {
    D_800C3E1C = sprite;
}

/* Request sound (run by the frame loop, 800B8068). */
void func_800B8054(s32 sound) {
    D_800591B4 = sound;
    D_800591B1 = 0;
}

/* Run a requested sound command (800B7C34, 800B7E94) and mark it done. */
void func_800B8068(s32 sound) {
    func_800B7C34(sound);
    func_800B7E94();
    D_800591B1 = 1;
}
