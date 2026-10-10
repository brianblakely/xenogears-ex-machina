#ifndef BATTLE_FRAME_H
#define BATTLE_FRAME_H

#include "common.h"
#include "psyq/libgte.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/sprite.h"

/* The battle's frame loop and battle menu (800BD3AC's unit, 800BE6EC-
 * 800BFE48): a frame of the battle (800BE790), the module loads, the battle
 * menu that runs the acting slot's walk and command file, command motions,
 * value watches and targets, requested loads, gear restarts and effect
 * sprites; and the distances and command file parts of 800BFE48's unit
 * (800C06E4-800C08CC, 800C0F70-800C11CC). The late units address the battle
 * area from battle_area as one aggregate, BattleArea (battle/area.h). */

/* The battle's sprite source (resident sprite_shared_source). */
#define SPRITE_SOURCE ((SpriteSource *)sprite_shared_source)

extern s32 battle_frame_nesting;    /* frame loop nesting */
extern u8 battle_showing_status_drains;     /* a slot's sprite commands run */
extern u8 battle_light_matrix[];
extern u16 battle_frame_start_time;    /* the frame time */
extern u8 battle_unread_single_action_loaded;
extern u8 battle_start_mode;     /* the battle's start mode */
extern u8 battle_area_buffer0_background_color;     /* BATTLE_AREA.buffers[0].drawEnv.r0, which 800B8098 addresses apart from the area */
extern s32 battle_gear_enemy_count;    /* gear enemies present */

/* The battle menu (battle_current_menu, 0x50 bytes). */
typedef struct BattleMenu {
    u8 pad0[4];
    struct Sprite *sprite;              /* 0x04: the acting slot's */
    void (*update)(struct BattleMenu *menu); /* 0x08 */
    u8 padC[0x1C - 0xC];
    s32 state;                              /* 0x1C */
    s32 turnSlot;                           /* 0x20: the slot whose turn it is */
    s32 slot;                               /* 0x24: the acting slot */
    s32 targetSlot;                         /* 0x28 */
    s32 field2C;                            /* 0x2C: the next path point */
    s32 field30;                            /* 0x30 */
    s32 field34;                            /* 0x34 */
    u8 pad38[0x40 - 0x38];
    s32 field40;                            /* 0x40 */
    s32 field44;                            /* 0x44 */
    u8 field48;                             /* 0x48 */
    u8 field49;                             /* 0x49 */
    u8 field4A;                             /* 0x4A */
    u8 pad4B;
    struct Sprite *target;              /* 0x4C */
} BattleMenu;

extern BattleMenu *battle_current_menu;
extern s32 battle_unread_menu_word;
extern s16 battle_knocked_down_mask;

/* The acting slot's walk and command file (800BEFF4-800BF4F0). */
extern void *battle_command_file;             /* the loaded command file */
extern s32 battle_command_file_slot;               /* its slot */
extern u8 battle_command_file_started;                /* the command file is started */
extern u16 battle_area_event_target_mask;               /* the current event's targets */
extern Sprite *battle_area_event_target_sprites[];     /* their sprites, NULL ended */
extern s16 battle_area_event_target_count;               /* their count */

/* Command motions, value watches and targets (800BF5E8-800BF998). */
typedef struct SlotWatch {
    u8 pad0[0xC];
    void (*destroy)(struct SlotWatch *watch);  /* 0x0C */
    u8 pad10[0x1C - 0x10];
    Sprite *sprite;                        /* 0x1C */
    s32 mode;                                  /* 0x20: the sprite's mode at the start */
    s32 value;                                 /* 0x24: its last value */
    s32 threshold;                             /* 0x28 */
    void (*callback)(Sprite *sprite);      /* 0x2C */
} SlotWatch;

extern s32 battle_single_action_start_request;
extern s16 battle_effect_hit_count;           /* effect hits */

/* Requested loads, gear restarts and effect sprites (800BF9EC-800BFDA8). */
extern u8 battle_wave_bank_5_loaded;            /* the sound bank of file 5 is loaded */
extern u8 battle_image_upload_requested;            /* upload the images of file 1 */
extern u8 battle_wave_bank_7_loaded;            /* wave bank 7 is loaded (a gear frame's turn) */
extern u8 battle_gear_restart_mode;            /* restart the party's gears (2: all but the acting) */
extern SoundSequence *battle_transferred_wave_bank; /* the transferred wave bank of a command file */

/* Distances, blends and command file parts (800C06E4-800C1140). */
typedef struct {
    s16 x;
    s16 y;
} VramPoint;

extern s32 (*battle_curve_cell_weights)[4]; /* four weights per cell, 8 cells a row */

/* The frame loop and the battle menu. */
void battle_show_status_drain_amounts(s32 slot, s32 a, s32 b, s32 c); /* run commands on a slot's sprite and wait */
void battle_run_frame(void);        /* run one battle frame */
void battle_load_module(void);        /* load the requested battle module */
void battle_menu_clear(void);        /* clear the battle menu state */
BattleMenu *battle_menu_open(void); /* open the battle menu */
void battle_menu_close(void);        /* close the battle menu */
void battle_start_object_effect_on_stack(s32 index, s32 mask, s32 mode); /* 800AA320 on a stack of its own */
s32 battle_list_slot_sprites(u32 mask, Sprite **list, Sprite *target); /* list the sprites of the slots in mask */
s16 battle_get_sprite_direction(Sprite *from, Sprite *to); /* the direction between two sprites */
s16 battle_get_target_direction(Sprite *sprite); /* the direction to a sprite's target point */
void battle_menu_set_state(s32 state);    /* set the battle menu's state */
void battle_walk_next_path_point(Sprite *sprite); /* walk a sprite along the path */
void battle_load_slot_command_file(Sprite *sprite); /* load the file of a sprite's command */
s32 battle_start_command_file_once(void);         /* start the loaded command file once */
void battle_stop_command_file(void);        /* stop the started command file */
void battle_face_first_target(Sprite *sprite); /* face a sprite and the event's first target */
void battle_walk_beside_target(Sprite *sprite, Sprite *target);
void battle_single_action_load_during_motion(s32 command, Sprite *sprite); /* run a command and wait for its motion */
void battle_show_results_if_menu_open(void);        /* update the battle menu when it is open */
s32 battle_count_active_tasks(void);
s32 battle_is_total_popup_shown(void);         /* whether a number popup shows */
void battle_request_single_action_start(s32 value);
void battle_distance_watch_start(Sprite *sprite, s32 threshold, void (*callback)(Sprite *sprite)); /* watch its value against threshold */
void battle_set_single_target(s32 index, s32 slot);
void battle_sprite_next_target(Sprite *sprite); /* move a sprite's target to the next target */
s32 battle_get_target_index(Sprite *sprite); /* a sprite's index among the targets */
void battle_count_effect_hit(void);        /* count an effect hit */
void battle_upload_requested_images(void);        /* upload the requested images */
void battle_restart_party_gears(void);        /* restart the party's gears once requested */
void battle_load_wave_bank_5(void);        /* load the sound bank of file 5 once */
Sprite *battle_find_or_destroy_effect_sprites(Sprite *sprite, s32 mode, s32 action); /* find or destroy a sprite's effect sprites */
void battle_destroy_effect_sprites(Sprite *sprite, s32 mode);
void battle_add_effect_sprite(Sprite *sprite, s32 mode);

/* Distances and command file parts (800BFE48's unit). */
s32 battle_get_ground_distance(GroundPoint from, GroundPoint to); /* the distance between two points */
void battle_get_direction_angles(SVECTOR *from, SVECTOR *to, SVECTOR *angles); /* the direction angles between points */
void battle_release_wave_bank(void);        /* release the transferred sound bank */
SoundBank *battle_install_command_file_parts(s32 *file); /* set up a command file's parts */
void battle_free_command_file_sound_bank(s32 *file);   /* free a command file's sound bank */

#endif
