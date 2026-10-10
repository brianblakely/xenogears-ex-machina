/* The resident's commons from 80062648, after the PsyQ data-stream
 * library's, up to the PsyQ graphics library's at 8006be34
 * (commons_before_libspu.c). */
#include "common.h"

u8 mode_music_buffer[0x3200]; /* 80062648: battle, field, world map, ovl2606, ovl3087 */
s32 mode_field_pointer_state[5]; /* 80065848: field; debug595 reads [2]-[4] */
s32 commons_unused_27_words[27]; /* 8006585C: unreferenced */
u8 *mode_battle_scene_data; /* 800658C8: battle scene data */
void *menu_state_debug_heap_marker; /* 800658CC */
s32 commons_unused_3_words_b[3]; /* 800658D0: unreferenced */
struct EncounterSet formation_encounter_set; /* 800658DC: battle, field, world map, ovl2606 */
u8 formation_encounter_weights[16]; /* 80065ADC: field, debug595 */
s32 commons_unused_4_words_b[4]; /* 80065AEC: unreferenced */
void *mode_party_file_blocks[3]; /* 80065AFC: party character file blocks */
s32 mode_field_last_moved_actor; /* 80065B08: field */
u8 sound_memory_pool[0x6300]; /* 80065B0C: the sound driver's memory pool */
s32 commons_unused_word_e; /* 8006BE0C: unreferenced */
u8 sprite_shared_source[0x14]; /* 8006BE10: the shared sprite source (a SpriteSource record): sprites, battle */
void *menu_state_debug_heap_reservation; /* 8006BE24 */
s32 commons_unused_word_f; /* 8006BE28: unreferenced */
s16 mode_party_gear_refresh_flags[3]; /* 8006BE2C: field */

#include "resident/formation.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "../sound_driver.h"
