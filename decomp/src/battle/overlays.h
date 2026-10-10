#ifndef BATTLE_OVERLAYS_H
#define BATTLE_OVERLAYS_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "ovl2615/load_modes.h"

/* The entries of the overlays the battle loads and calls, declared as the
 * battle calls them: some definitions take fewer arguments or other types
 * (801E879C, the 801FC000 modules' sprite script commands), so these stay
 * out of the overlays' own headers. The event script overlay's other entries
 * are in battle/event_script.h. */

/* The result screens (ovl2596, 801DE000). */
void battle_results_queue_screens(void); /* queue the result screens' primitives */
void battle_results_build_card_exp_totals(void); /* build each present member's first numbers */
void battle_results_build_card_exp_to_count(void); /* build their seven-digit numbers */
void battle_results_leave_battle(void); /* leave the battle */

/* The battle's start from the scene select (ovl2606, 801E0000). */
void battle_scene_select_main(void);

/* The battle loader (ovl2615, 801E4000): the enemy set file and the stage
 * (its set-up phases and load modes, which the battle calls as they are
 * defined, are in ovl2615/load_modes.h). */
void battle_setup_loader_start(s32 data); /* start loading the enemy set file */
u8 battle_setup_build_stage(u8 **scene, s32 unused, u8 *stage, u8 *origin, u8 *colours, u8 *tint); /* set up the stage */

/* The event script interpreter's pass (ovl3087, 801E5000), which takes no
 * argument. */
void battle_event_script_run(s32);

/* The battle modules at 0x801FC000, one loaded at a time: break a model into
 * pieces (ovl3384: the model bound at a model sprite's renderer +0x34, its
 * packets and matrix), start an effect circling a sprite (ovl3383), and the
 * sprite script commands of ovl3385, ovl3386 and ovl3387. */
void battle_module_debris_start(void *model, void *prims, MATRIX *matrix, s32 gravity, s32 speed, s32 speed_range, s32 spin_range, s32 life);
void battle_module_spin_start(Sprite *actor, s32 angle, s32 radius, s32 swing_growth, s32 row_angle_step, s32 angle_step_growth, s32 step);
void battle_module_scroll_start(Sprite *sprite, u8 *args);
void battle_module_hold_start(Sprite *sprite, u8 *args);
void battle_module_burst_play(void);

/* The debug pages (debug2611, 80280000). */
void battle_debug_print_state_page(void);
void battle_debug_run_tools_frame(void); /* the debugger's frame hook */

#endif
