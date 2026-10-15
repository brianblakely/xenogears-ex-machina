/*
 * A host option that changes the game (docs/runtime.md, Random battles in
 * platforming areas): player control runs the field's random draw
 * (field_encounter_count_down, 80079288) every field frame a direction is
 * held, and a map whose scripts armed it (event f7) starts a battle when
 * one of its drawn steps comes down. Where the host turns random battles
 * off for the current map (its xem_host_random_battles is 0), the draw does
 * not run at all: no step counts down, rand() is not called and no battle
 * is requested, as on a map whose scripts never armed it. Where the host
 * halves them (2), the draw runs every other field frame (the dialogue's
 * tick, field_dialogue_tick_count, counts the field's frames), so its steps
 * come down, and its battles come, half as often for the same walk.
 * Scripted battles, the scripts' f7 arming and inhibitions, map changes and
 * the world map's own encounters are the game's. The host records the maps
 * it turns off and the halving with the game's inputs; without either
 * (every map 1) the draw runs as the original's. The port keeps no data of
 * its own.
 */
#include "common.h"
#include "resident/mode.h"
#include "../decomp/src/field/field_mode.h"
#include "../decomp/src/field/field_dialogue.h"

/* 0 when the host turns the random battles of field map `map` (the low 12
 * bits of mode_field_map_id) off, 2 when it halves them, else 1. */
u32 xem_host_random_battles(u32 map);

s32 xem_original_field_encounter_count_down(void);

s32 field_encounter_count_down(void) {
    u32 pace = xem_host_random_battles((u32)mode_field_map_id & 0xFFF);

    if (pace == 0 || (pace == 2 && (field_dialogue_tick_count & 1) != 0)) {
        return 0;
    }
    return xem_original_field_encounter_count_down();
}
