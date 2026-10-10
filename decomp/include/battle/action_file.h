#ifndef BATTLE_ACTION_FILE_H
#define BATTLE_ACTION_FILE_H

#include "common.h"
#include "resident/sprite.h"

/* Single actions and their command files (800B7134's unit, 800B7870-
 * 800B8068): the battle's intro swirl, a requested single action's command
 * file (0x22 + 2 * index, read into a heap block; its images upload and its
 * sound bank starts) and stream (0x23 + 2 * index); also the party's sprites'
 * end (800BB080). The event script overlay starts command files too. */

extern u8 battle_unread_command_file_loaded;    /* a command file is loaded */
extern u8 battle_music_lowered;                 /* the command file's sound bank is started */
extern Sprite *battle_acting_sprite;            /* the acting sprite (battle_single_action_set_actor sets it) */
extern s16 battle_acting_sprite_command_motion; /* the acting sprite's command motion */

void battle_run_intro_swirl(void);                   /* the battle's intro swirl */
void battle_single_action_clear_loaded(void);
void battle_single_action_load(s32 index);           /* load a single action's command file and stream */
void battle_single_action_set_actor(Sprite *sprite); /* set the acting sprite of a single action */
void battle_single_action_request(s32 action);       /* request single action `action` (800b8068 runs it) */
void battle_single_action_run(s32 action);           /* run a requested single action */
void battle_end_party_sprites(s32 keep);             /* end the party's sprites other than keep's */

#endif
