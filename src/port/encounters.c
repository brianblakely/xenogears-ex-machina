/*
 * A host option that changes the game (docs/runtime.md, Random battles in
 * platforming areas): player control runs the field's random draw
 * (field_encounter_count_down, 80079288) every field frame a direction is
 * held, and a map whose scripts armed it (event f7) starts a battle when
 * one of its drawn steps comes down. Where the host turns random battles
 * off for the current map (its xem_host_random_battles is 0), the draw does
 * not run at all: no step counts down, rand() is not called and no battle
 * is requested, as on a map whose scripts never armed it. Scripted
 * battles, the scripts' f7 arming and inhibitions, map changes and the
 * world map's own encounters are the game's. The host records the maps it
 * turns off with the game's inputs; without any (every map 1) the draw
 * runs as the original's. The port keeps no data of its own.
 */
#include "common.h"
#include "resident/mode.h"
#include "../decomp/src/field/field_mode.h"

/* 0 when the host turns the random battles of field map `map` (the low 12
 * bits of mode_field_map_id) off, else 1. */
u32 xem_host_random_battles(u32 map);

s32 xem_original_field_encounter_count_down(void);

s32 field_encounter_count_down(void) {
    if (xem_host_random_battles((u32)mode_field_map_id & 0xFFF) == 0) {
        return 0;
    }
    return xem_original_field_encounter_count_down();
}
