/* The resident's commons from 8006d634, after the PsyQ graphics library's,
 * up to the end of the BSS, the word below the overlay area at 8006faf0
 * that the entry point clears last (commons_before_libspu.c). */
#include "common.h"
#include "psyq/libgte.h"

struct GameData game_data; /* 8006D634: the saved game */
s32 commons_unused_word_g; /* 8006F98C: unreferenced */
s32 mode_party_stand_in_actors[3]; /* 8006F990 */
VECTOR sprite_camera_eye; /* 8006F99C: positions (16.16) of two field points */
VECTOR sprite_camera_look_at; /* 8006F9AC */
struct FileRequest mode_battle_file_list[4]; /* 8006F9BC: the mode's sound files */
struct BattleFormation formation_active; /* 8006F9DC: the battle's formation */
struct SpuMemBlock sound_spu_memory_map[12]; /* 8006F9FC: the SPU memory map */
s32 mode_party_file_ids[3]; /* 8006FABC: party members of the loaded field files */
u8 sound_spu_malloc_table[0x28]; /* 8006FAC8: the SPU memory management table (SpuInitMalloc, 4 blocks) */

#include "resident/cd.h"
#include "resident/formation.h"
#include "resident/gamedata.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "../sound_driver.h"
