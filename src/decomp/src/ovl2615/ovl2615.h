#ifndef OVL2615_OVL2615_H
#define OVL2615_OVL2615_H

/* The battle setup module (overlay slot 2615, five units): the declarations
 * more than one of its units needs. */

#include "common.h"

/* Combatant slots: 0-2 party members, 3-10 enemies. */
#define SLOT_COUNT 11
#define NO_COMBATANT 0x7F

/* Callers convert arguments differently from the resident definition (s16
 * modes and u16 positions there): upload an image list (battle_setup_phases.c,
 * battle_loader.c). */
void model_load_image_list(void *images, s32 mode, s32 x, s32 y, s32 mode2, s32 x2, s32 y2);

/* The battle overlay's model setup (stage.c places the stage model with it,
 * battle_loader.c the enemy models). */
void battle_create_object(s32 index, s32 flags, void *script_file, void *model_file, s32 x, s32 y, s32 z, s32 w, s32 position);

#endif
