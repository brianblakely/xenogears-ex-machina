#ifndef BATTLE_SETUP_H
#define BATTLE_SETUP_H

#include "common.h"

/* The battle's set-up and end: the scene settings, the battle's outcome and
 * exit (80070E2C's unit 80076544), the result screen step (battle.c
 * 8008A9C0, declared where it is called), the party's battle masks and their
 * adjustments at the start (8008CCCC's 8009892C), the music, and the battle
 * heap and disc helpers (battle.c 8008AB4C-8008AC50). */

extern u8 battle_resume_event_script_at_end;
extern u8 battle_defeat_allowed_by_event_script;
extern s32 battle_enemy_set_file;
extern s32 battle_music_seq;                        /* the battle music's sequence */
extern u8 battle_frame_mode;                        /* the HUD and frame tick: 2 event script, 1 turns, 0 results */
extern u8 battle_area_outcome;                      /* battle outcome (the battle area's outcome) */
extern u8 battle_turn_hud_hidden;                   /* keep the battle's resources at its end (the battle area's +0xa7a) */
extern s32 battle_heap_mark_for_post_battle_module; /* 801de000 module blocks */
extern s32 battle_heap_reserve_for_post_battle_module;
extern u8 battle_skip_result_screens;
extern u8 battle_exit_requested;                    /* battle exit requested; the results then skip the rewards */
/* The battle overlay's entry, which the resident's battle mode (2) runs, and
 * the flag the event script (with a movie request, ovl3087 opcode 27), the
 * result screens and the loader overlays set for it: the battle then
 * continues in the movie mode (6). */
void battle_main(void);
extern u8 battle_continue_to_movie_mode;

/* The skills each party member knew at the battle's start (80070E2C's unit
 * copies them from the game data): the result screens (ovl2596) show those
 * it learnt since. */
typedef struct KnownSkills {
    u16 counterSkills;
    u16 levelSkills;
} KnownSkills;

extern KnownSkills battle_known_skills_at_start[3];

void battle_draw_hud(void);              /* end the battle by its outcome state */
void battle_adjust_party_at_start(void); /* the party's adjustments at battle start */

/* The battle heap and the disc. */
void battle_cd_select_event_script_directory(void);              /* heap mode 0x20/0 */
void battle_cd_select_music_directory(void);                     /* heap mode 0x20/2 */
void battle_cd_select_menu_directory(void);                      /* heap mode 0x20/3 */
void battle_cd_wait_for_reads(void);                             /* wait until the disc reads finish */

#endif
