#ifndef MENU_MODE_H
#define MENU_MODE_H

#include "common.h"
#include "psyq/types.h"
#include "resident/cd.h"

/* The menu mode (menu5 80084FD0-8008509C, 8008518C-80085E34, 800888B0-
 * 80088BFC; menu6): its entry and frame loop, the task that loads its
 * files and runs the title, options and scene screens, its music, and the
 * option settings and progress flags it keeps in the game data. */

/* The menu's image file (arena_mode_files[4], unpacked): pointers to its TIM
 * images, named by their readers. */
typedef struct MenuImageFile {
    u_long *unk0;
    u_long *sparkle0[12];  /* 0x04: sparkle kind 0's frames */
    u_long *sparkle1;      /* 0x34: kind 1 (and 3) */
    u_long *backdrop;      /* 0x38 (8007b388 also loads it) */
    u_long *font;          /* 0x3C */
    u_long *sparkle2;      /* 0x40: kind 2 */
    u_long *effect;        /* 0x44: the trail and line texture */
    u_long *icons[4];      /* 0x48 */
    u_long *name;          /* 0x58: the name plates */
    u_long *bar;           /* 0x5C: the gauge bars */
    u_long *sheet;         /* 0x60: the menu's sprite sheet */
    u_long *banner;        /* 0x64 */
    u_long *sparkle4;      /* 0x68: kind 4 */
    u_long *floor;         /* 0x6C */
    u_long *extra[9];      /* 0x70 */
} MenuImageFile;

/* Current option settings (0x80099d98). */
typedef struct Settings {
    u8 level;     /* 0x00: saved as option13 */
    u8 unk1;
    u8 rate;      /* 0x02: frame rate choice */
    u8 option4;   /* 0x03: port 1 vibration */
    u8 option5;   /* 0x04: port 2 vibration */
    u8 com1;      /* 0x05: side 1 played by the computer */
    u8 driven;    /* 0x06: side 2 played by the computer */
    u8 option6;   /* 0x07 */
    u8 unk8;
    u8 speed;     /* 0x09 */
    u8 command;   /* 0x0A: the opponent's current command */
    u8 unkB;
    s16 unkC;
} Settings;

extern void (*arena_mode_tasks[])(s32);             /* mode tasks, by mode_arena_task */
extern FileRequest arena_mode_files[6];             /* sequence, sound bank, messages, map, scene; zero file */
extern s32 arena_mode_own_seq;                      /* nonzero: the menu plays its own sequence */
extern char *arena_mode_heap_tag_names[];           /* names of the menu's heap block kinds */
extern s16 arena_mode_vblanks_per_frame;            /* vertical blanks per frame */
extern s32 arena_play_mode;                         /* menu mode */
extern s32 arena_mode_unread_disc_mode_kind;
extern u16 arena_debug_display_flags;               /* debug display switches */
extern u8 arena_menu_screen_flags;                  /* bit 0: the menu screen is shown */
extern void (*arena_debug_frame_hook)(void *block); /* debug hook (switch 0x10) */
extern struct SoundSeq *arena_mode_music_seq;       /* the music sequence */
extern Settings arena_settings;

void arena_mode_exit(void);
void arena_mode_task(s32 arg);       /* the menu task */
void arena_progress_set_flag(s32 flag);
s32 arena_progress_is_flag_set(s32 flag);
void arena_progress_add_unlocked_entry(void);
s32 arena_progress_check_complete(void);
void arena_settings_save_to_game_data(void);
void arena_settings_load_from_game_data(void);
void arena_progress_set_flag_and_check(s32 flag);
void arena_debug_draw_sync_callback(void);
void arena_debug_step_counter(void);
void arena_display_init_buffer_sprite(s32 index);

#endif
