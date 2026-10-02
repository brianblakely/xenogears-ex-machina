/* Battle unit from 800C11CC to the end of the overlay text (Cygnus CDK
 * GCC 2.7.2). 800C11CC's tables start at 0x80070C14 (4 mod 8) directly
 * after 800C0564's odd-length one at 0 mod 8; the functions from 800C06E4
 * to 800C1140 have no rodata, and the boundary is placed at the first
 * function that has. */
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

INCLUDE_ASM(".local/decomp/battle/asm/nonmatchings/battle_800C11CC", func_800C11CC);
