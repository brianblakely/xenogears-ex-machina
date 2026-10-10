#ifndef OVL2615_OVL2615_H
#define OVL2615_OVL2615_H

/* The battle setup module (overlay slot 2615, five units): the declarations
 * more than one of its units needs. */

#include "common.h"

/* Combatant slots: 0-2 party members, 3-10 enemies. */
#define SLOT_COUNT 11
#define NO_COMBATANT 0x7F

/* Run one setup phase (ovl2615.c); the load modes (load_modes.c,
 * burst_modes.c) run them between their frames. The prototype keeps the
 * callers' u8 conversion. */
void func_801E5840(u8 phase);

/* Callers convert arguments differently from the resident definition (s16
 * modes and u16 positions there): upload an image list (ovl2615.c,
 * battle_loader.c). */
void model_load_image_list(void *images, s32 mode, s32 x, s32 y, s32 mode2, s32 x2, s32 y2);

/* The battle overlay's model setup (stage.c places the stage model with it,
 * battle_loader.c the enemy models). */
void func_800A8BF0(s32 a0, s32 a1, void *a2, void *a3, s32 a4, s32 a5, s32 a6, s32 a7, s32 a8);

#endif
