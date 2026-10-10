/* Battle unit from 800B8098 to 800BD3AC: the battle's start and close and the
 * loads it waits for, the acting slot's turn run through the battle menu
 * (walks, sounds, the turn cancel), the slots' sprites and gear objects, the
 * battle camera, and the slot highlights and results. It is built like
 * battle_tmd_screen_effects.c by the Cygnus CDK GCC 2.7.2. Its jump tables sit at
 * 0 mod 8 (800B8098's at 0x80070A10) where the previous unit's sit at 4
 * (800B7870's at 0x800709FC), so a unit starts between the two; the functions
 * from 800B7C28 to 800B8068 have no rodata, and the boundary is placed at the
 * first function that has. Its last table (800B9F78's, 9 entries at
 * 0x80070AB8) is followed directly by 800BD3AC's at 0x80070ADC (4 mod 8), so
 * the unit ends before 800BD3AC. */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/action_file.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/command.h"
#include "battle/effect_script.h"
#include "battle/flow.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/highlight.h"
#include "battle/objects.h"
#include "battle/scene.h"
#include "battle/screen.h"
#include "battle/setup.h"
#include "battle/sprite.h"
#include "battle/stage.h"
#include "battle/turn.h"
#include "battle/windows.h"
#include "files.h"
#include "overlays.h"
#include "own_declarations.h"
#include "popup.h"
#include "resident_views.h"
#include "settle.h"
#include "sprite_effect.h"

/* Functions of other units declared as this unit calls them, which differs
 * from their definitions. */
void battle_set_background_color_ptrs(u8 *first, u8 *second);   /* the two buffers' background colours (two words there) */
void battle_set_object_current_animation(s32 index, s32 value); /* set stage object index's byte 0x2A (a u8 there) */
/* Defined without a return value: 800B89FC takes what it leaves in v0, the
 * new acting sprite. */
Sprite *battle_menu_set_acting_slot(s32 slot);

/* This unit's functions, declared before their first use. */
void battle_init_display_buffers(void);
void battle_reset_frame_state(void);
void battle_start_first_frame(void);
void battle_empty_frame_reset_step(void);
void battle_area_event_step(Sprite *sprite);
void battle_sprites_face_each_other(Sprite *sprite, Sprite *other);
void battle_slot_sprite_remove(s32 slot);
void battle_camera_reset(void);
void battle_camera_set_framing_pitch(s16 value);
void battle_camera_frame_slots(u32 mask);
void battle_highlight_clear(void);
void battle_slot_ring_update(Task *task);
void battle_slot_ring_create(SpriteTask *owner);

/* The unit's own uninitialized variables (its .bss, after
 * battle_action_files.c's; ASPSX 2.56 aligns each by its size up to a word:
 * decomp/Makefile). */
static u8 battle_gear_file_read_count;     /* 800C3CB8: gear file reads running */
static s32 battle_camera_ease_mode;        /* 800C3CBC */
static s32 battle_camera_mode;             /* 800C3CC0: camera mode */
static u8 battle_camera_sprite_count;      /* 800C3CC4: eye and look-at sprites running */
static s32 battle_unused_camera_word;      /* 800C3CC8: unreferenced */
static SVECTOR battle_camera_saved_eye;    /* 800C3CCC: the eye point saved while the camera sprites run */
static SVECTOR battle_camera_saved_target; /* 800C3CD4: the look-at point saved while they run */
static s32 battle_camera_framed_range;     /* 800C3CDC: the framed camera range */
static s32 battle_unused_camera_pair[2];   /* 800C3CE0: unreferenced */

s32 battle_gear_object_load_count = 0; /* 800C35D8 */
BattleSound battle_sound_table[] = { /* 800C35DC */
    {0x1F, 0x0F}, {0x26, 0x13}, {0x2F, 0x12}, {0x36, 0x0D}, {0x3E, 0x11}, {0x44, 0x1B},
    {0x4A, 0x16}, {0x50, 0x12}, {0x57, 0x01}, {0x59, 0x01}, {0x5B, 0x02},
};
u16 battle_slots_counting_while_down = 0; /* 800C3608 */
s32 battle_area_event_index = 0; /* 800C360C */
BattleMenu *battle_current_menu = NULL; /* 800C3610 */
s16 battle_area_event_delay_timer = 0; /* 800C3614 */
void *battle_command_file = NULL; /* 800C3618 */
s32 battle_command_file_slot = 0; /* 800C361C */
u8 battle_wave_bank_5_loaded = 0; /* 800C3620 */
u8 battle_image_upload_requested = 0; /* 800C3621 */
u8 battle_wave_bank_7_loaded = 0; /* 800C3622 */
u8 battle_single_action_keeps_pose_13 = 0; /* 800C3623 */
u8 battle_sprite_script_finished = 0; /* 800C3624 */
u16 battle_gear_sound_played_slots = 0; /* 800C3626 */
s32 battle_single_action_start_request = 0; /* 800C3628 */
u8 battle_gear_restart_mode = 0; /* 800C362C */
s16 battle_single_action_base_table[] = {0, 0x16, 0x2F, 0x4A, 0x5E, 0x71, 0x7C, 0x8F, 0x97, 0x2F, 0x7C}; /* 800C3630 */
s16 battle_single_action_split_table[] = {6, 0x1A, 0x37, 0x4E, 0x61, 0x74, 0x83, 0x8F, 0x97, 0x37, 0x83}; /* 800C3648 */
s32 battle_menu_update_running = 0; /* 800C3660 */
u8 battle_sprites_paused = 0; /* 800C3664 */
u16 battle_gear_image_places_taken = 0; /* 800C3666 */
ImagePlace battle_gear_image_places[3] = {{0x340, 0}, {0x2C0, 0x100}, {0x300, 0x100}}; /* 800C3668 */
s32 battle_camera_ease_fraction = 0x200; /* 800C3674 */
s32 battle_camera_framed_slots = -1; /* 800C3678 */
s32 battle_camera_resume_mode = 1; /* 800C367C */
SpriteTask *battle_camera_eye_task = NULL; /* 800C3680 */
SpriteTask *battle_camera_target_task = NULL; /* 800C3684 */
u8 battle_camera_skip_gear_heights = 0; /* 800C3688 */
/* A box's corners and its twelve edges; nothing reads them. */
SVECTOR battle_unused_box_corners[8] = { /* 800C368C */
    {-1500, -768, 0},    {1500, -768, 0},    {1500, 0, 0},    {-1500, 0, 0},
    {-1500, -768, 1536}, {1500, -768, 1536}, {1500, 0, 1536}, {-1500, 0, 1536},
};
SVECTOR *battle_unused_box_edges[12][2] = { /* 800C36CC */
    {&battle_unused_box_corners[3], &battle_unused_box_corners[7]}, {&battle_unused_box_corners[2], &battle_unused_box_corners[3]}, {&battle_unused_box_corners[2], &battle_unused_box_corners[6]},
    {&battle_unused_box_corners[0], &battle_unused_box_corners[1]}, {&battle_unused_box_corners[1], &battle_unused_box_corners[2]}, {&battle_unused_box_corners[0], &battle_unused_box_corners[3]},
    {&battle_unused_box_corners[6], &battle_unused_box_corners[7]}, {&battle_unused_box_corners[4], &battle_unused_box_corners[5]}, {&battle_unused_box_corners[5], &battle_unused_box_corners[6]},
    {&battle_unused_box_corners[4], &battle_unused_box_corners[7]}, {&battle_unused_box_corners[1], &battle_unused_box_corners[5]}, {&battle_unused_box_corners[0], &battle_unused_box_corners[4]},
};
u8 battle_stage_drawing_off = 0; /* 800C372C */
SVECTOR battle_camera_up_vector = {0, 0x1000, 0}; /* 800C3730 */
s32 battle_camera_circle_distance = 0x200; /* 800C3738 */
s16 battle_camera_circle_angle = 0xC0; /* 800C373C */
SVECTOR battle_camera_framing_angles = {0xC0, 0, 0}; /* 800C3740 */
SlotPulse *battle_current_slot_pulse = NULL; /* 800C3748 */

/* 800B8098: Start the battle in mode (1-4 the battle module's intros, 801E8588..;
 * others 800B7870): the display, the frame state and the formation's
 * background colour. */
void battle_start_intro(s32 mode) {
    battle_start_mode = mode;
    battle_init_display_buffers();
    mode_battle_load_files();
    switch (mode) {
    case 1:
        cd_sync_reads(0);
        battle_setup_run_shatter_load_mode();
        break;
    case 2:
        cd_sync_reads(0);
        battle_setup_run_burst_load_mode();
        break;
    case 3:
        cd_sync_reads(0);
        battle_setup_run_burst_variant1_load_mode();
        break;
    case 4:
        cd_sync_reads(0);
        battle_setup_run_shatter_in_place_load_mode();
        break;
    case 0:
    case 5:
    default:
        battle_run_intro_swirl();
        break;
    }
    cd_sync_reads(0);
    battle_reset_scene();
    BATTLE_AREA.buffers[0].drawEnv.isbg = battle_setup_build_stage(&mode_battle_scene_file, mode_battle_stage_unused_word, mode_battle_stage_file, battle_light_matrix,
                                                        battle_light_matrix + 0x20, &battle_area_buffer0_background_color);
    battle_set_background_color_ptrs(&battle_area_buffer0_background_color, &BATTLE_AREA.buffers[1].drawEnv.r0);
}

/* 800B81BC: Enter the battle: the first frame buffer (800B88C4), the frame state
 * (800B8840), the battle module (801E62E0) and the scene's camera. */
void battle_enter(s32 enemy_set) {
    battle_start_first_frame();
    battle_reset_frame_state();
    battle_setup_loader_start(enemy_set);
    sound_release_wave_bank(mode_wave_bank_5);
    sprite_set_svector(&battle_camera_wanted_points[0], SCENE_DATA->cameras[0].eye[0], SCENE_DATA->cameras[0].eye[1], SCENE_DATA->cameras[0].eye[2]);
    sprite_set_svector(&battle_camera_view_eye, SCENE_DATA->cameras[0].eye[0], SCENE_DATA->cameras[0].eye[1], SCENE_DATA->cameras[0].eye[2]);
    sprite_set_svector(&battle_camera_wanted_points[1], SCENE_DATA->cameras[0].lookAt[0], SCENE_DATA->cameras[0].lookAt[1], SCENE_DATA->cameras[0].lookAt[2]);
    sprite_set_svector(&battle_camera_view_target, SCENE_DATA->cameras[0].lookAt[0], SCENE_DATA->cameras[0].lookAt[1], SCENE_DATA->cameras[0].lookAt[2]);
    SetDispMask(1);
}

/* 800B8284: Set up the two display buffers: 320 x 224 at y 224 and 0 (drawn at 0
 * and 224), shown at (0, 10) as 256 x 216. */
void battle_init_display_buffers(void) {
    SetGeomOffset(160, 164);
    SetDefDispEnv(&BATTLE_AREA.buffers[0].dispEnv, 0, 224, 320, 224);
    SetDefDispEnv(&BATTLE_AREA.buffers[1].dispEnv, 0, 0, 320, 224);
    SetDefDrawEnv(&BATTLE_AREA.buffers[0].drawEnv, 0, 0, 320, 224);
    SetDefDrawEnv(&BATTLE_AREA.buffers[1].drawEnv, 0, 224, 320, 224);
    BATTLE_AREA.buffers[1].dispEnv.screen.y = 10;
    BATTLE_AREA.buffers[0].dispEnv.screen.y = 10;
    BATTLE_AREA.buffers[1].dispEnv.screen.w = 256;
    BATTLE_AREA.buffers[0].dispEnv.screen.w = 256;
    BATTLE_AREA.buffers[1].dispEnv.screen.x = 0;
    BATTLE_AREA.buffers[0].dispEnv.screen.x = 0;
    BATTLE_AREA.buffers[1].dispEnv.screen.h = 216;
    BATTLE_AREA.buffers[0].dispEnv.screen.h = 216;
}

/* 800B8354: Run frames while the disc is busy. */
void battle_wait_for_disc(void) {
    while (cd_get_pending_read_count() != 0) {
        battle_run_frame();
    }
}

/* 800B838C: Play battle sound index to its end: load its sound bank and its wave bank
 * (the next bank, plus variant except for sound 8), start sound (plus
 * variant) and run frames while it plays, then free both banks. */
void battle_play_sound_to_end(s32 index, s32 variant) {
    FileRequest banks[4]; /* the sound bank, the wave bank, the end, one unused */
    SoundBank *system;
    void *waves;
    SoundSequence *waveBank;
    s32 sound;

    battle_release_wave_bank();
    battle_wait_for_disc();
    cd_select_directory(0x2C, 1);
    banks[0].file = battle_sound_table[index].bank;
    system = heap_alloc(cd_get_aligned_file_size(banks[0].file), 0);
    banks[0].destination = system;
    if (index == 8) {
        banks[1].file = battle_sound_table[8].bank + 1;
    } else {
        banks[1].file = battle_sound_table[index].bank + 1;
        banks[1].file += variant;
    }
    waves = heap_alloc(cd_get_aligned_file_size(banks[1].file), 0);
    banks[1].destination = waves;
    banks[2].destination = NULL;
    banks[2].file = 0;
    cd_read_file_list(banks, 0, 0);
    battle_wait_for_disc();
    sound_add_effect_bank(system);
    waveBank = sound_load_wave_bank(waves, 0);
    while (sound_sync_transfer(0) != 0) {
        battle_run_frame();
    }
    sound = battle_sound_table[index].sound + variant + (system->id << 16);
    sound_play_effect(sound);
    while (sound_get_active_effect_mask(sound) != 0) {
        battle_run_frame();
    }
    sound_remove_effect_bank(system);
    sound_release_wave_bank(waveBank);
    heap_free(waves);
    heap_free(system);
}

/* 800B853C: Close the battle: for mode 0 the party (on foot) turns to its pose 0x18,
 * for mode 2 a white fade and pose 5 over 40 frames; then remove the enemy
 * slots, load wave bank 5 as the battle's (mode_wave_bank_5) and free the enemy
 * set data. */
void battle_close(s32 mode) {
    s32 i;
    s32 slot;
    Sprite *sprite;
    Sprite *member;
    void *waves;

    switch (mode) {
    case 0:
        battle_camera_start_move(-1);
        for (i = 0; i != 3; i++) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL && (s8)sprite->motion.bytes[3] != 0x15) {
                battle_find_or_destroy_effect_sprites(sprite, 0, 2);
                sprite_start_animation(sprite, 0x18);
            }
        }
        break;
    case 2:
        battle_camera_start_move(0);
        battle_screen_fade_start(0x28, 2, 0xFF, 0xFF, 0xFF);
        for (slot = 0; slot != 3; slot++) {
            member = BATTLE_AREA.sprites[slot];
            if (member != NULL && (s8)member->motion.bytes[3] != 0x15) {
                sprite_start_animation(member, 5);
            }
        }
        slot = 0x28;
        do {
            battle_run_frame();
            slot--;
        } while (slot > 0);
    case 1:
        break;
    }
    for (slot = 3; slot != 11; slot++) {
        battle_free_object(slot);
        battle_slot_sprite_remove(slot);
    }
    battle_release_wave_bank();
    battle_wait_for_disc();
    cd_select_directory(0x2C, 0);
    waves = heap_alloc(cd_get_aligned_file_size(5), 1);
    cd_read_file(5, waves, 0, 0x80);
    battle_wait_for_disc();
    mode_wave_bank_5 = sound_load_wave_bank(waves, 0);
    while (sound_sync_transfer(0) != 0) {
        battle_run_frame();
    }
    heap_free(waves);
    heap_free(battle_enemy_set_copy);
    BATTLE_AREA.field8DA8 = 0;
}

/* 800B8774: Leave the battle: finish drawing, remove the slots' sprites, the
 * resident sprites and tasks, the sound bank and the scene. */
void battle_leave(void) {
    s32 slot;

    if (BATTLE_AREA.buffer == 0) {
        battle_run_frame();
    }
    DrawSync(0);
    for (slot = 0; slot != 11; slot++) {
        battle_slot_sprite_remove(slot);
    }
    sprite_free_queues();
    task_destroy_all();
    sound_remove_effect_bank((SoundBank *)sprite_script_sound_bank);
    heap_free(sprite_script_sound_bank);
    sprite_in_battle = 0;
    DrawSync(0);
    battle_free_objects();
    battle_free_scene();
    heap_free(*(void **)battle_file2_block_and_max_hp_digits); /* a heap block here */
}

/* 800B8840: Reset the battle's frame state, sprites, camera and effects. */
void battle_reset_frame_state(void) {
    sprite_in_battle = 1;
    task_active_main_count = 0;
    task_new_tasks_active = 0;
    sprite_default_scale = 0x2000;
    battle_menu_clear();
    battle_total_popup_reset();
    task_clear_lists();
    battle_camera_reset();
    sprite_alloc_queues(0x5000, 0);
    battle_highlight_clear();
    battle_single_action_clear_loaded();
    battle_empty_frame_reset_step();
    model_box_test_mode = 0;
}

/* 800B88C4: Start the first frame: the frame skip from the gear enemies present,
 * draw into the second buffer with the first one's background colour. */
void battle_start_first_frame(void) {
    s32 i = 3;
    s32 skip;
    FrameBuffer *buffer;

    battle_gear_enemy_count = 0;
    for (; i != 11; i++) {
        if (BATTLE_AREA.slots[i].field2 < 0x11 && BATTLE_AREA.slots[i].gear != 0) {
            battle_gear_enemy_count++;
        }
    }
    sprite_frame_skip = battle_gear_enemy_count / 2 - 1;
    if (sprite_frame_skip < 0) {
        sprite_frame_skip = 0;
    }
    skip = sprite_frame_skip;
    BATTLE_AREA.frameTicks = skip;
    sprite_frame_skip = 0;
    model_ot_depth_shift = 2;
    buffer = &BATTLE_AREA.buffers[0];
    if (BATTLE_AREA.current == buffer) {
        buffer = &BATTLE_AREA.buffers[1];
    }
    BATTLE_AREA.current = buffer;
    BATTLE_AREA.ot = buffer->ot;
    ClearOTagR((u_long *)buffer->ot, 0x1000);
    BATTLE_AREA.buffer = 1;
    BATTLE_AREA.current = &BATTLE_AREA.buffers[1];
    BATTLE_AREA.field8DA8 = 0;
    BATTLE_AREA.buffers[1].drawEnv.isbg = BATTLE_AREA.buffers[0].drawEnv.isbg;
    BATTLE_AREA.buffers[1].drawEnv.r0 = BATTLE_AREA.buffers[0].drawEnv.r0;
    BATTLE_AREA.buffers[1].drawEnv.g0 = BATTLE_AREA.buffers[0].drawEnv.g0;
    BATTLE_AREA.buffers[1].drawEnv.b0 = BATTLE_AREA.buffers[0].drawEnv.b0;
}

/* 800B89F4: Empty; the frame state's reset (800B8840) calls it last. */
void battle_empty_frame_reset_step(void) {
}

/* 800B89FC: Open the battle menu for slot's turn: make it the acting slot facing its
 * event's first target. On foot, load wave bank 7 once for a gear frame or
 * return the sprite to its state; a gear turns to targets (800AA320 0x1A).
 * Mode 0 then walks the sprite (a gear turns to the event's targets),
 * otherwise the menu goes to state 4 (9 for a gear). */
void battle_menu_open_turn(s32 mode, s32 slot, s32 targets, s32 next_slot) {
    BattleMenu *menu;
    Sprite *sprite;
    void *waves;

    battle_camera_set_resume_mode(0);
    if (battle_current_menu != NULL) {
        for (;;) {
            __asm__ volatile(".word 0x0001000D"); /* break 1 */
        }
    }
    battle_run_frame();
    battle_run_frame();
    battle_sprite_script_finished = 0;
    battle_slots_counting_while_down &= ~(1 << slot);
    menu = battle_menu_open();
    battle_current_menu = menu;
    menu->field40 = next_slot;
    menu->turnSlot = slot;
    battle_face_first_target(battle_menu_set_acting_slot(slot));
    battle_acting_sprite_command_motion = 0;
    sprite = battle_current_menu->sprite;
    if (!BATTLE_AREA.slots[slot].gear) {
        battle_release_wave_bank();
        battle_wave_bank_5_loaded = 0;
        battle_camera_set_framing_pitch(0xC0);
        if (!sprite_is_cell_directory(*(u8 **)sprite->image)) {
            battle_wave_bank_7_loaded = 0;
            if (battle_command_file == NULL || SPRITE_SLOT(sprite) != battle_command_file_slot) {
                battle_load_slot_command_file(sprite);
            }
        } else {
            if (!battle_wave_bank_7_loaded) {
                battle_wait_for_disc();
                cd_select_directory(0x2C, 0);
                waves = heap_alloc(cd_get_aligned_file_size(7), 0);
                cd_read_file(7, waves, 0, 0x80);
                battle_wait_for_disc();
                battle_release_wave_bank();
                battle_transferred_wave_bank = sound_load_wave_bank(waves, 0);
                while (sound_sync_transfer(0) != 0) {
                    battle_run_frame();
                }
                heap_free(waves);
            }
            battle_wave_bank_7_loaded = 1;
        }
    } else {
        battle_finish_loads();
        battle_load_wave_bank_5();
        battle_menu_set_state(8);
        battle_start_object_script_on_own_stack(slot, targets, 0x1A);
        battle_camera_set_framing_pitch(0xC0);
    }
    if (mode == 0) {
        if (BATTLE_AREA.slots[slot].gear) {
            battle_start_object_script_on_own_stack(slot, BATTLE_AREA.events[battle_area_event_index].targetMask, 2);
        } else {
            sprite_set_completion_callback(sprite, battle_menu_mark_sprite_done);
            battle_walk_next_path_point(sprite);
        }
    } else {
        battle_menu_set_state(BATTLE_AREA.slots[slot].gear ? 9 : 4);
    }
}

/* 800B8D04: Finish the battle's loads: wait for the disc (800B8354), start the
 * requested loads (800BF9EC), run frames until task_active_main_count is reached,
 * then free the command file. */
void battle_finish_loads(void) {
    battle_wait_for_disc();
    battle_upload_requested_images();
    while (task_active_main_count != battle_is_total_popup_shown()) {
        battle_run_frame();
    }
    battle_stop_command_file();
    if (battle_command_file != NULL) {
        heap_free(battle_command_file);
        battle_command_file = NULL;
    }
}

/* 800B8D7C: Stop 8002A498 and finish the loads (800B8D04). */
void battle_stop_reads_finish_loads(void) {
    cd_stop_read(0);
    battle_finish_loads();
}

/* 800B8DA4: Cancel the turn: the turn's slot acts again; a gear turns back (800AA320
 * 0x1F), a party member returns to its place, ground and idle motion; close
 * the battle menu. */
void battle_cancel_turn(void) {
    Sprite *sprite;

    if (battle_current_menu != NULL) {
        battle_menu_set_acting_slot(battle_current_menu->turnSlot);
        sprite = battle_current_menu->sprite;
        if (BATTLE_AREA.slots[SPRITE_SLOT(sprite)].gear) {
            battle_start_object_script_on_own_stack(SPRITE_SLOT(sprite), 0, 0x1F);
        } else {
            sprite->x = (u16)BATTLE_AREA.slots[battle_current_menu->slot].x << 16;
            sprite->z = (u16)BATTLE_AREA.slots[battle_current_menu->slot].z << 16;
            battle_sprite_update_ground(sprite);
            sprite->y = sprite->ground << 16;
            sprite_start_animation(sprite, (s8)sprite->b0.byteb0);
            sprite_set_completion_callback(sprite, NULL);
        }
        battle_menu_close();
    }
}

/* 800B8EBC: End the turn: wait for the command's loads and sprites, restore the view
 * and the camera, close the battle menu, end the turn's presentation
 * (800BA4E0), fade a pending sound and clear every sprite's bit 6. */
void battle_end_turn(void) {
    Sprite *sprite = battle_current_menu->sprite;
    s32 slot;
    s32 i;

    if (battle_current_menu != NULL) {
        battle_sprite_script_finished = 0;
        battle_acting_sprite_command_motion = 0;
        battle_wave_bank_7_loaded = 0;
        battle_mark_actor_events_done();
        DrawSync(0);
        slot = battle_current_menu->field40;
        sprite_set_completion_callback(sprite, NULL);
        battle_knock_down_slots();
        while (task_active_main_count != battle_is_total_popup_shown()) {
            battle_run_frame();
        }
        battle_wait_sprites_settled();
        battle_stop_reads_finish_loads();
        battle_camera_set_resume_mode(1);
        battle_total_popup_hide();
        battle_upload_requested_images();
        battle_restart_party_gears();
        battle_camera_skip_gear_heights = 0;
        battle_camera_set_framing_pitch(0xC0);
        battle_menu_close();
        battle_end_turn_and_preload_slot(slot);
        if (battle_music_lowered) {
            sound_set_seq_fade((SoundSeq *)battle_music_seq, 0x7F, 0x50);
        }
        battle_music_lowered = 0;
        for (i = 0; i != 11; i++) {
            if (BATTLE_AREA.sprites[i] != NULL) {
                BATTLE_AREA.sprites[i]->motion.word &= ~0x40;
            }
        }
    }
}

/* 800B9020: Return the sprite to its idle motion. */
void battle_sprite_return_to_idle(Sprite *sprite) {
    sprite_start_animation(sprite, (s8)sprite->b0.byteb0);
    sprite_set_completion_callback(sprite, NULL);
}

/* 800B905C: Start the current event: its actor acts, facing its first target. For
 * a partner action both sprites get bit 6 and face each other, the camera
 * frames both, and the actor steps 0x50 beside its partner (to the side it
 * came from, or away from the partner's place when it stands there); then
 * start the loads and walk (800B9508). */
void battle_area_event_start(void) {
    Sprite *sprite;
    Sprite *partner;
    u16 mask;
    s16 x;

    battle_menu_set_acting_slot(BATTLE_AREA.events[battle_area_event_index].actor);
    sprite = battle_current_menu->sprite;
    battle_face_first_target(sprite);
    if (AREA_PARTNER_ACTION) {
        partner = sprite->partner;
        partner->motion.word |= 0x40;
        sprite->motion.word |= 0x40;
        battle_sprites_face_each_other(sprite, partner);
        partner = sprite->partner;
        mask = (1 << SPRITE_SLOT(sprite)) | (1 << SPRITE_SLOT(partner));
        battle_camera_frame_slots(mask);
        x = partner->x >> 16;
        if (x != (u16)BATTLE_AREA.slots[SPRITE_SLOT(partner)].x
            || (partner->z >> 16) != (u16)BATTLE_AREA.slots[SPRITE_SLOT(partner)].z) {
            if (x < (u16)BATTLE_AREA.slots[SPRITE_SLOT(partner)].x) {
                sprite->target_x = x - 0x50;
            } else {
                sprite->target_x = x + 0x50;
            }
        } else if ((sprite->x >> 16) < x) {
            sprite->target_x = x - 0x50;
        } else {
            sprite->target_x = x + 0x50;
        }
        sprite->target_z = partner->z >> 16;
        sprite->target_y = 0;
        battle_camera_frame_slots(mask);
        copyVector(&battle_camera_view_eye, &battle_camera_wanted_points[0]);
        copyVector(&battle_camera_view_target, &battle_camera_wanted_points[1]);
        battle_sprites_face_each_other(sprite, partner);
    }
    battle_upload_requested_images();
    battle_area_event_step(sprite);
}

/* 800B9258: Count a step of the battle menu (field34), when there is one. */
void battle_menu_add_step(void) {
    if (battle_current_menu != NULL) {
        battle_current_menu->field34++;
    }
}

/* 800B9284: Run a control event (type 0xF3-0xFA) of the current event for sprite:
 * the event's parameter shows or hides a message window, delays the next
 * event, runs a command (with motion 0x11, for 0xF5 also pose 0x13 and
 * battle_single_action_keeps_pose_13), frames its targets, or adds slots that count while down. */
void battle_area_event_run_control(Sprite *sprite, s32 type) {
    s32 parameter;

    switch (type) {
    case 0xFA:
        battle_current_menu->field48 = 1;
        sprite_set_completion_callback(sprite, NULL);
        battle_show_message_window(BATTLE_AREA.events[battle_area_event_index].parameter);
        break;
    case 0xF8:
        battle_current_menu->field48 = 1;
        sprite_set_completion_callback(sprite, NULL);
        battle_hide_message_window(BATTLE_AREA.events[battle_area_event_index].parameter);
        break;
    case 0xF7:
        battle_current_menu->field48 = 1;
        sprite_set_completion_callback(sprite, NULL);
        battle_area_event_delay_timer = BATTLE_AREA.events[battle_area_event_index].parameter;
        break;
    case 0xF5:
        battle_single_action_set_actor(sprite);
        parameter = BATTLE_AREA.events[battle_area_event_index].parameter;
        battle_acting_sprite_command_motion = (parameter >> 9) & 0x3F;
        sprite->motion.bytes[3] = 0x11;
        battle_single_action_load_during_motion(parameter & 0x1FF, sprite);
        battle_request_single_action_start((s32)sprite);
        sprite_start_animation(sprite, 0x13);
        battle_single_action_keeps_pose_13 = 1;
        break;
    case 0xF4:
        battle_single_action_set_actor(sprite);
        parameter = BATTLE_AREA.events[battle_area_event_index].parameter;
        battle_acting_sprite_command_motion = (parameter >> 9) & 0x3F;
        sprite->motion.bytes[3] = 0x11;
        battle_single_action_load_during_motion(parameter & 0x1FF, sprite);
        break;
    case 0xF6:
        battle_camera_start_move(BATTLE_AREA.events[battle_area_event_index].targetMask);
        break;
    case 0xF3:
        battle_current_menu->field48 = 1;
        battle_slots_counting_while_down |= BATTLE_AREA.events[battle_area_event_index].parameter;
        break;
    }
}

/* 800B9508: Step the current event for the acting sprite (after any delay): control
 * events 0xF3-0xFA (800B9284; 0xF9 ends with pose 5), 0xFB takes another
 * slot's sprite images, 0xFC sets the idle mode, 0xFD walks on, 0xFE (and
 * 0xFF once 800C0314 is done) returns the turn's sprite to its place;
 * other types are commands: a motion on foot (from 0x10 a command of the
 * sprite's slot's field2), or a gear's pose framing it and its partner. */
void battle_area_event_step(Sprite *sprite) {
    s32 motion;
    Sprite *other;
    Sprite *partner;
    s32 type;
    s32 command;

    if (battle_area_event_delay_timer != 0) {
        battle_area_event_delay_timer--;
        return;
    }
    type = BATTLE_AREA.events[battle_area_event_index].type;
    switch (type) {
    case 0xF3:
    case 0xF4:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xFA:
        battle_area_event_run_control(sprite, type);
        break;
    case 0xF9:
        sprite_start_animation(sprite, 5);
        battle_camera_start_move(0);
        return;
    case 0xFF:
        sprite_set_completion_callback(sprite, NULL);
        battle_current_menu->field48 = 1;
        if (battle_knock_down_slots() == 0) {
            if (sprite->frame_bits.field28 == 0 && !BATTLE_AREA.slots[SPRITE_SLOT(sprite)].hidden) {
                sprite_start_animation(sprite, (s8)sprite->b0.byteb0);
            }
            return;
        }
    case 0xFE:
        battle_current_menu->field48 = 0;
        sprite = battle_menu_set_acting_slot(battle_current_menu->turnSlot);
        battle_face_first_target(sprite);
        sprite_set_completion_callback(sprite, battle_menu_mark_sprite_done);
        battle_menu_set_state(5);
        if (FIXED_WHOLE(sprite->x) == (u16)BATTLE_AREA.slots[battle_current_menu->slot].x
            && FIXED_WHOLE(sprite->z) == (u16)BATTLE_AREA.slots[battle_current_menu->slot].z) {
            battle_menu_mark_sprite_done(sprite);
        } else {
            sprite->target_x = BATTLE_AREA.slots[battle_current_menu->slot].x;
            sprite->target_z = BATTLE_AREA.slots[battle_current_menu->slot].z;
            sprite->target_y = 0;
            sprite_start_animation(sprite, 4);
        }
        return;
    case 0xFD:
        battle_current_menu->field48 = 1;
        if (BATTLE_AREA.events[battle_area_event_index].parameter == 0) {
            battle_current_menu->field48 = 0;
            battle_walk_next_path_point(sprite);
            sprite_set_completion_callback(sprite, battle_menu_mark_sprite_done);
        }
        battle_area_event_index++;
        return;
    case 0xFC:
        battle_current_menu->field48 = 1;
        sprite_set_completion_callback(sprite, NULL);
        sprite_set_idle_animation(sprite, BATTLE_AREA.events[battle_area_event_index].parameter);
        battle_area_event_index++;
        return;
    case 0xFB:
        battle_current_menu->field48 = 1;
        sprite_set_completion_callback(sprite, NULL);
        other = BATTLE_AREA.sprites[BATTLE_AREA.events[battle_area_event_index].parameter];
        sprite->image = other->image;
        ((SpriteSequencer *)sprite->sequencer)->size = ((SpriteSequencer *)other->sequencer)->size;
        sprite->render.word |= 0x40000000;
        heap_free(sprite->renderer->parts[0]);
        sprite->renderer->parts[1] = sprite->renderer->parts[0] = heap_alloc(heap_get_block_size((u8 *)other->renderer->parts[0]), 0);
        battle_area_event_index++;
        return;
    default:
        partner = sprite->partner;
        battle_gear_sound_played_slots = 0;
        battle_single_action_set_actor(sprite);
        sprite_set_completion_callback(sprite, battle_menu_mark_sprite_done);
        ((SpriteSequencer *)sprite->sequencer)->word8 = BATTLE_AREA.events[battle_area_event_index].codes[SPRITE_SLOT(partner)];
        if (!sprite_is_cell_directory(*(u8 **)sprite->image)) {
            if (type >= 0x10) {
                command = type - 0x10;
                command += battle_single_action_base_table[BATTLE_AREA.slots[SPRITE_SLOT(sprite)].field2];
                if (command < battle_single_action_split_table[BATTLE_AREA.slots[SPRITE_SLOT(sprite)].field2]) {
                    sprite->motion.bytes[3] = 0x1C;
                } else {
                    sprite->motion.bytes[3] = 0x11;
                }
                battle_single_action_load_during_motion(command, sprite);
                battle_request_single_action_start((s32)sprite);
            } else {
                motion = type;
                if (!battle_command_file_started && battle_command_file != NULL) {
                    battle_wait_for_disc();
                    sprite->word50 = battle_start_command_file_once();
                    sprite_set_alternate_resource(sprite, (s32)battle_command_file);
                }
                if (battle_current_menu->turnSlot == SPRITE_SLOT(sprite)) {
                    motion = ~motion;
                    sprite_start_animation(sprite, motion);
                } else {
                    sprite_start_animation(sprite, motion);
                }
            }
        } else {
            sprite_start_animation(sprite, type);
            battle_camera_frame_slots((1 << SPRITE_SLOT(sprite)) | (1 << SPRITE_SLOT(sprite->partner)));
        }
        battle_current_menu->field48 = 0;
        battle_area_event_index++;
        return;
    }
    battle_area_event_index++;
}

/* 800B9B30: Mark the battle menu (field48) with its state. */
void battle_menu_mark_sprite_done(void) {
    battle_current_menu->field48 = 1;
    battle_current_menu->field49 = battle_current_menu->state;
}

/* 800B9B54: Turn two sprites to face each other (the second not while its motion
 * mode is 0x15). */
void battle_sprites_face_each_other(Sprite *sprite, Sprite *other) {
    if (sprite != other) {
        sprite_set_facing(sprite, battle_get_sprite_direction(sprite, other));
        sprite_set_direction(sprite, battle_get_sprite_direction(sprite, other));
        if ((s8)other->motion.bytes[3] != 0x15) {
            sprite_set_facing(other, battle_get_sprite_direction(other, sprite));
            sprite_set_direction(other, battle_get_sprite_direction(other, sprite));
        }
    }
}

/* 800B9C00: Put the sprite at its target, idle, facing other. Defined without a
 * prototype: 800BF0C4 calls it with the sprite alone. */
void battle_sprite_arrive_at_target(sprite, other)
    Sprite *sprite;
    Sprite *other;
{
    sprite->x = sprite->target_x << 16;
    sprite->z = sprite->target_z << 16;
    battle_current_menu->field48 = 1;
    battle_menu_set_state(4);
    sprite_start_animation(sprite, (s8)sprite->b0.byteb0);
    battle_sprites_face_each_other(sprite, other);
}

/* 800B9C78: Step the current event of a gear's turn (after any delay): control events
 * (800B9284), 0xF4 sets the command sound and step, 0xFB swaps stage
 * objects, 0xFC sets a stage object's byte, 0xFD/0xF9 and commands run
 * 800AA320 on the event's targets, 0xFE turns the turn's slot to them and
 * ends (state 10), 0xFF ends (state 9). */
void battle_area_event_step_gear(void) {
    s32 slot;
    Sprite *sprite;
    s32 type;
    s32 parameter;
    s32 event;

    if (battle_area_event_delay_timer != 0) {
        battle_area_event_delay_timer--;
        return;
    }
    battle_upload_requested_images();
    slot = BATTLE_AREA.events[battle_area_event_index].actor;
    battle_menu_set_acting_slot(slot);
    battle_face_first_target(battle_current_menu->sprite);
    slot = battle_current_menu->slot;
    sprite = BATTLE_AREA.sprites[slot];
    type = BATTLE_AREA.events[battle_area_event_index].type;
    battle_menu_set_state(8);
    if (battle_current_menu->field4A) {
        AREA_BYTE_A73 = 0;
    }
    battle_current_menu->field4A = 0;
    switch (type) {
    case 0xFF:
        battle_menu_set_state(9);
        return;
    case 0xFE:
        battle_wait_for_disc();
        battle_start_object_script_on_own_stack(battle_current_menu->turnSlot, BATTLE_AREA.events[battle_area_event_index].targetMask, 4);
        battle_menu_set_state(10);
        return;
    case 0xFC:
        battle_set_object_current_animation(slot, BATTLE_AREA.events[battle_area_event_index].parameter);
        battle_menu_set_state(9);
        battle_area_event_index++;
        return;
    case 0xFD:
        battle_area_event_index++;
        battle_start_object_script_on_own_stack(slot, BATTLE_AREA.events[battle_area_event_index - 1].targetMask, 2);
        return;
    case 0xF3:
    case 0xF5:
    case 0xF6:
    case 0xF7:
    case 0xF8:
    case 0xFA:
        battle_area_event_run_control(sprite, type);
        battle_menu_set_state(9);
        battle_area_event_index++;
        return;
    case 0xFB:
        battle_menu_set_state(9);
        battle_swap_objects(SPRITE_SLOT(sprite), BATTLE_AREA.events[battle_area_event_index].parameter);
        battle_area_event_index++;
        return;
    case 0xF4:
        battle_menu_set_state(9);
        event = battle_area_event_index;
        battle_area_event_index = event + 1;
        parameter = BATTLE_AREA.events[event].parameter;
        battle_acting_sprite_command_motion = (parameter >> 9) & 0x3F;
        battle_requested_single_action = parameter & 0x1FF;
        return;
    default:
        battle_current_menu->field4A = 1;
        battle_single_action_set_actor(sprite);
        battle_area_event_index++;
        battle_start_object_script_on_own_stack(slot, BATTLE_AREA.events[battle_area_event_index - 1].targetMask, type);
        return;
    }
}

/* 800B9F78: The battle menu's update (not reentered): finish a requested sound
 * command, step a gear's events, run the menu's pending action (field49:
 * 2 put the sprite at its target, 4 a party target's hit pose 0x1B,
 * 5 return to idle and end the slot's turn, 6 walk to the partner), then
 * run its state: 2/6 walk towards the target until the distance grows,
 * 4 start the event, 7/8/10 wait for the popups, 9 step a gear's event. */
void battle_menu_update(BattleMenu *menu) {
    Sprite *sprite;
    Sprite *target;
    Sprite *first;
    GroundPoint from;
    GroundPoint to;
    GroundPoint toPartner;
    s32 distance;

    if (battle_menu_update_running != 0) {
        return;
    }
    battle_menu_update_running = 1;
    battle_current_menu = menu;
    sprite = menu->sprite;
    target = menu->target;
    if (battle_single_action_start_request != 0) {
        battle_single_action_start_request = 0;
        if (battle_single_action_start()) {
            sprite_set_completion_callback(sprite, battle_menu_mark_sprite_done);
        } else {
            if (SPRITE_SLOT(sprite) < 3) {
                if (battle_single_action_keeps_pose_13) {
                    sprite_start_animation(sprite, 0x13);
                } else {
                    sprite_start_animation(sprite, 0x12);
                }
                battle_single_action_keeps_pose_13 = 0;
            }
            if (sprite->animations != 0) {
                battle_menu_set_state(7);
            }
        }
    }
    if (battle_current_menu->field34 != 0) {
        battle_area_event_step_gear();
        battle_current_menu->field34--;
    }
    if (battle_current_menu->field49 != 0) {
        switch (battle_current_menu->field49) {
        case 4:
            first = battle_area_event_target_sprites[0];
            if (SPRITE_SLOT(first) < 3 && !BATTLE_AREA.slots[SPRITE_SLOT(first)].gear
                && BATTLE_AREA.events[battle_area_event_index - 1].codes[SPRITE_SLOT(first)] == 7) {
                battle_single_action_set_actor(first);
                sprite_start_animation(first, 0x1B);
                while ((s8)first->motion.bytes[3] == 0x1B) {
                    battle_run_frame();
                }
            }
            break;
        case 6:
            battle_walk_beside_target(sprite, sprite->partner);
            break;
        case 2:
            battle_sprite_arrive_at_target(sprite, sprite->partner);
            break;
        case 5:
            if (!BATTLE_AREA.slots[SPRITE_SLOT(sprite)].hidden) {
                sprite_start_animation(sprite, (s8)sprite->b0.byteb0);
            }
            battle_slot_sprite_face_side(battle_current_menu->slot);
            battle_menu_set_state(10);
            break;
        }
        battle_current_menu->field49 = 0;
    }
    switch (battle_current_menu->state) {
    case 7:
        if (task_active_main_count == battle_is_total_popup_shown()) {
            battle_menu_set_state(4);
            battle_current_menu->field48 = 1;
            battle_upload_requested_images();
        }
        break;
    case 10:
        if (task_active_main_count == battle_is_total_popup_shown()) {
            battle_end_turn();
        }
        break;
    case 9:
        battle_area_event_step_gear();
        break;
    case 8:
        if (battle_gear_restart_mode && task_active_main_count == battle_is_total_popup_shown()) {
            if (!battle_sprite_script_finished) {
                break;
            }
            if (battle_gear_restart_mode == 1) {
                battle_current_menu->field34++;
            }
            battle_restart_party_gears();
        }
        break;
    case 2:
        from.x = sprite->x >> 16;
        from.z = sprite->z >> 16;
        to.x = sprite->target_x;
        to.z = sprite->target_z;
        distance = battle_get_ground_distance(from, to);
        if (battle_current_menu->field44 < distance) {
            battle_sprite_arrive_at_target(sprite, target);
        } else {
            battle_current_menu->field44 = distance;
        }
        break;
    case 6:
        from.x = sprite->x >> 16;
        from.z = sprite->z >> 16;
        toPartner.x = sprite->target_x;
        toPartner.z = sprite->target_z;
        distance = battle_get_ground_distance(from, toPartner);
        if (battle_current_menu->field44 < distance) {
            battle_walk_beside_target(sprite, target);
        } else {
            battle_current_menu->field44 = distance;
        }
        break;
    case 4:
        if (battle_current_menu->field48) {
            battle_area_event_start();
        }
        break;
    }
    battle_menu_update_running = 0;
}

/* 800BA4E0: End slot's turn presentation: wait for the stage objects (800B136C), then
 * restore the view (800B8D7C) and, for a gear, 800BFBA0; for a party member
 * on foot its sprite's state (800BF2B8). */
void battle_end_turn_and_preload_slot(s32 slot) {
    task_active_main_count = 0;
    task_new_tasks_active = 0;
    battle_wait_objects_idle();
    if (BATTLE_AREA.slots[slot].gear) {
        battle_stop_reads_finish_loads();
        battle_load_wave_bank_5();
    } else if (slot < 3) {
        if (BATTLE_AREA.sprites[slot] != NULL) {
            battle_load_slot_command_file(BATTLE_AREA.sprites[slot]);
        }
    } else {
        battle_stop_reads_finish_loads();
    }
    battle_camera_set_framing_pitch(0xC0);
}

/* 800BA59C: Turn sprite to direction, its horizontal speed a quarter of its speed
 * along it. */
void battle_sprite_set_heading(Sprite *sprite, s16 direction) {
    s32 speed;

    sprite->direction = direction;
    speed = sprite->speed >> 3;
    sprite->speed_x = (gpu_get_cos(direction) >> 1) * speed >> 8;
    sprite->speed_z = -((gpu_get_sin(sprite->direction) >> 1) * speed) >> 8;
}

/* 800BA614: Aim sprite's jump at its target: turn it towards the target and set the
 * rising speed that lands it on the ground there (or the target's height
 * when that is higher). */
void battle_sprite_aim_jump(Sprite *sprite) {
    VECTOR delta;
    SVECTOR point;
    VECTOR out;
    s32 triangle;
    s32 height;
    s16 angle;
    s32 distance;

    sprite_set_svector(&point, sprite->target_x, sprite->target_y, sprite->target_z);
    triangle = battle_find_scene_triangle_near(&point, sprite->word78, 4);
    if (triangle < 0) {
        triangle = battle_find_scene_triangle(&point);
    }
    battle_put_point_on_scene_triangle(&point, triangle, &out);
    if (point.vy > sprite->target_y) {
        point.vy = sprite->target_y;
    }
    height = ((point.vy << 16) - sprite->y) >> 16;
    delta.vx = sprite->target_x - (sprite->x >> 16);
    delta.vz = sprite->target_z - (sprite->z >> 16);
    angle = -ratan2(delta.vz, delta.vx);
    Square0(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz);
    sprite->speed_y = -sprite->gravity * distance * 16 / (sprite->speed >> 11) + sprite->speed * height / distance;
    battle_sprite_set_heading(sprite, angle);
}

/* 800BA768: Aim sprite's jump at its target keeping its rising speed: snap it to
 * whole units, turn it towards the target and set the speed that covers the
 * distance (and the height difference) in the jump's frames. */
void battle_sprite_aim_jump_keep_rise(Sprite *sprite) {
    VECTOR delta;
    SVECTOR point;
    VECTOR out;
    s32 frames;
    s32 triangle;
    s32 angle;
    s32 distance;
    s32 height;

    frames = -(sprite->speed_y * 2 / sprite->gravity);
    sprite->x &= 0xFFFF0000;
    sprite->y &= 0xFFFF0000;
    sprite->z &= 0xFFFF0000;
    delta.vx = sprite->target_x - (sprite->x >> 16);
    delta.vz = sprite->target_z - (sprite->z >> 16);
    delta.vy = 0;
    angle = -ratan2(delta.vz, delta.vx);
    Square0(&delta, &delta);
    distance = SquareRoot0(delta.vx + delta.vz) << 16;
    if (frames != 0) {
        sprite->speed = distance / frames;
    } else {
        sprite->speed = 0;
    }
    sprite_set_svector(&point, sprite->target_x, sprite->target_y, sprite->target_z);
    triangle = battle_find_scene_triangle_near(&point, sprite->word78, 4);
    if (triangle < 0) {
        triangle = battle_find_scene_triangle(&point);
    }
    battle_put_point_on_scene_triangle(&point, triangle, &out);
    if (point.vy > sprite->target_y) {
        point.vy = sprite->target_y;
    }
    height = (point.vy << 16) - sprite->y;
    if (frames != 0) {
        sprite->speed_y += height / frames;
    }
    battle_sprite_set_heading(sprite, angle);
    sprite_move_vertically(sprite);
}

/* 800BA8F4: Put sprite on the scene's ground: its triangle and ground height. */
void battle_sprite_update_ground(Sprite *sprite) {
    SVECTOR point;
    VECTOR out;
    s32 triangle;

    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
    triangle = battle_find_scene_triangle_near(&point, sprite->word78, 4);
    if (triangle < 0) {
        triangle = battle_find_scene_triangle(&point);
    }
    battle_put_point_on_scene_triangle(&point, triangle, &out);
    sprite->ground = point.vy;
    sprite->word78 = triangle;
}

/* 800BA984: Create a sprite task (updated by 800BAC50, drawn by 800BAB0C) at x, y, z
 * facing direction, running animation. */
SpriteTask *battle_sprite_task_create(s32 resource, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s16 unused5, s16 x,
                          s16 y, s16 z, s16 animation, s16 direction, s32 unused11, s32 unused12,
                          s32 palette_bank) {
    SpriteTask *task;
    Sprite *sprite;

    task = (SpriteTask *)task_alloc_two_node_task(0x19C, NULL, battle_sprite_task_update, battle_sprite_task_draw, battle_sprite_task_destroy);
    sprite = &task->sprite;
    task->task.data = sprite;
    task->auxiliary.data = sprite;
    task->auxiliary.owner = NULL;
    sprite_construct_with_palette_bank(sprite, resource, clut_x, clut_y, texture_x, texture_y, unused5, palette_bank);
    sprite->block = task;
    sprite->x = x << 16;
    sprite->y = y << 16;
    sprite->z = z << 16;
    sprite->b0.byteb0 = animation;
    sprite->render.word |= 4;
    sprite->direction = direction;
    sprite_set_scale(sprite, 0x2000);
    sprite->word82 = 0x2000;
    sprite->word78 = 0;
    sprite_start_animation(sprite, animation);
    return task;
}

/* 800BAB0C: Draw a sprite task: its depth in the view, and its parts when visible. */
void battle_sprite_task_draw(Task *task) {
    SVECTOR point;
    long result[2]; /* screen position, then the GTE flags */
    VECTOR unused;
    Sprite *sprite;
    s32 depth;

    if (battle_sprites_paused == 0) {
        sprite = task->data;
        point.vx = sprite->x >> 16;
        point.vy = sprite->y >> 16;
        point.vz = sprite->z >> 16;
        SetRotMatrix(&battle_camera_view_matrix);
        SetTransMatrix(&battle_camera_view_matrix);
        depth = (RotTransPers(&point, &result[0], &result[0], &result[1]) >> model_ot_depth_shift) + sprite->half30;
        if (result[1] & 0x8000) {
            depth = 0;
        }
        sprite->depth = depth;
        if ((u32)(depth - 1) < 0xFFF) {
            sprite_draw(sprite, (u_long *)sprite_ot + depth);
        }
    }
}

/* 800BABDC: Destroy a sprite task: its part block, children, sprite and node. */
void battle_sprite_task_destroy(Task *node) {
    SpriteTask *task = (SpriteTask *)node;
    Sprite *sprite = &task->sprite;
    void *parts = sprite->renderer->parts[0];

    if (parts != NULL) {
        heap_free(parts);
    }
    task_destroy_owned_by(&task->task);
    sprite_remove_pending(sprite);
    task_unlink_draw_node(&task->auxiliary);
    task_unlink_main_node(&task->task);
    heap_free(task);
}

/* 800BAC50: Update a sprite task (twice with double steps) unless paused. */
void battle_sprite_task_update(Task *task) {
    Sprite *sprite = task->data;

    if (battle_sprites_paused == 0) {
        sprite_vm_tick(sprite);
        sprite_move(sprite);
        if (sprite->motion.bits.double_step) {
            sprite_vm_tick(sprite);
            sprite_move(sprite);
        }
    }
}

/* 800BACBC: Party slot's sprite on screen: its position, depth and a box around it. */
void battle_get_slot_screen_box(s32 slot, s16 *x, s16 *y, s16 *depth, s16 *left, s16 *width, s16 *centre) {
    SVECTOR point;
    s16 sxy[2];
    long p;
    Sprite *sprite = BATTLE_AREA.sprites[slot];

    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
    PushMatrix();
    SetRotMatrix(&battle_camera_view_matrix);
    SetTransMatrix(&battle_camera_view_matrix);
    *depth = RotTransPers(&point, (long *)sxy, &p, &p) >> 4;
    *x = sxy[0];
    *y = sxy[1];
    *left = sxy[0] - 0x30;
    *width = 0x30;
    *centre = sxy[0] - 0x18;
    PopMatrix();
}

/* 800BADD4: Remove party slot's sprite task: stop its effects (800BFC80), free its
 * sprite source, destroy the task and clear the slot's sprite. */
void battle_slot_sprite_remove(s32 slot) {
    SpriteTask *task = BATTLE_AREA.tasks[slot];

    if (task != NULL) {
        battle_find_or_destroy_effect_sprites((Sprite *)task, 0, 2); /* the slot's task where a sprite is taken */
        if (slot < 3) {
            if (BATTLE_AREA.sources[slot].data != NULL) {
                heap_free(BATTLE_AREA.sources[slot].data);
            }
            BATTLE_AREA.sources[slot].data = NULL;
        }
        task->task.destroy(&task->task);
        task_destroy_owned_by(&task->task);
        BATTLE_AREA.sprites[slot] = NULL;
        BATTLE_AREA.tasks[slot] = NULL;
    }
}

/* 800BAEB8: Face slot's sprite along its side (turned for a nonzero target code),
 * unless it runs animation 0x15. */
void battle_slot_sprite_face_side(s32 slot) {
    Sprite *sprite = BATTLE_AREA.sprites[slot];
    s32 direction;

    if ((s8)sprite->motion.bytes[3] != 0x15) {
        direction = (BATTLE_AREA.slots[slot].targetCode != 0) << 11;
        sprite_set_facing(sprite, direction);
        sprite_set_direction(sprite, direction);
    }
}

/* 800BAF40: Empty; its callers (the combo and technique commits) pass a slot and a
 * mode it ignores. */
void battle_empty_commit_step(void) {
}

/* 800BAF48: Send party slot's sprite off: select it (800BC404), run its exit
 * animation 0x16 and wait for it and its tasks, then remove the sprite and
 * load the slot's gear object in its place (800BB760), waiting for it. */
void battle_slot_swap_sprite_for_gear(s32 slot) {
    Sprite *sprite;
    s32 tasks;

    battle_camera_start_move(1 << slot);
    battle_camera_start_move(0);
    tasks = task_main_count;
    sprite = BATTLE_AREA.sprites[slot];
    battle_stop_reads_finish_loads();
    sprite_start_animation(sprite, 0x16);
    while (sprite->countdown != 0 && (s8)sprite->motion.bytes[3] == 0x16) {
        battle_run_frame();
    }
    while (task_main_count != tasks) {
        battle_run_frame();
    }
    battle_slot_sprite_remove(slot);
    battle_run_frame();
    battle_run_frame();
    battle_gear_load_start(slot);
    while (battle_gear_object_load_count != 0) {
        battle_run_frame();
    }
}

/* 800BB080: Destroy the party members' sprites other than keep's that are not in
 * use, then end their stage objects (800B14CC). */
void battle_end_party_sprites(s32 keep) {
    s32 i;
    Sprite *sprite;

    for (i = 0; i != 3; i++) {
        if (i != keep) {
            sprite = BATTLE_AREA.sprites[i];
            if (sprite != NULL && sprite->animations == 0) {
                ((Task *)sprite->block)->destroy(sprite->block);
                BATTLE_AREA.sprites[i] = NULL;
                BATTLE_AREA.tasks[i] = NULL;
            }
        }
    }
    battle_end_party_objects(keep);
}

/* 800BB13C: Update of a sprite following its slot's stage object: step its animation
 * while it runs, then put it at the object's position. */
void battle_object_follower_update(Task *task) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    Sprite *sprite = task->data;
    u32 low = sprite->frame_bits.unknown30;
    BattleObject *object = battle_objects[sprite->motion.bits.unknown0 << 2 | low];

    if (object != NULL) {
        if (sprite->countdown == 0) {
            sprite->script = 0;
        }
        if (sprite->script != 0) {
            sprite_vm_tick(sprite);
            sprite_move(sprite);
            if (sprite->motion.bits.double_step) {
                sprite_vm_tick(sprite);
                sprite_move(sprite);
            }
        }
        sprite->x = object->hierarchy->translation[0] << 16;
        sprite->y = object->hierarchy->translation[1] << 16;
        sprite->z = object->hierarchy->translation[2] << 16;
    }
}

/* 800BB248: Draw of a slot-following sprite: its size from the slot's object and its
 * depth in the view. */
void battle_object_follower_draw(Task *task) {
    SVECTOR point;
    long result[2]; /* screen position, then the GTE flags */
    Sprite *sprite = task->data;
    s32 depth;
    u32 low;

    low = sprite->frame_bits.unknown30;
    sprite->height = battle_get_object_height(sprite->motion.bits.unknown0 << 2 | low);
    sprite->extent_depth = sprite->height / 2;
    point.vx = sprite->x >> 16;
    point.vy = sprite->y >> 16;
    point.vz = sprite->z >> 16;
    SetRotMatrix(&battle_camera_view_matrix);
    SetTransMatrix(&battle_camera_view_matrix);
    depth = (RotTransPers(&point, &result[0], &result[0], &result[1]) >> model_ot_depth_shift) + sprite->half30;
    if (result[1] & 0x8000) {
        depth = 0;
    }
    sprite->depth = depth;
}

/* 800BB314: Destroy a task node. */
void battle_object_follower_destroy(Task *task) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */

    task_unlink_draw_node(task + 1);
    task_unlink_main_node(task);
    heap_free(task);
}

/* 800BB350: Create slot's sprite following its stage object (800BB13C, 800BB248),
 * unless it has one. */
void battle_object_follower_create(u32 slot) {
    SpriteTask *task;
    Sprite *sprite;
    u8 saved;

    if (BATTLE_AREA.sprites[slot] == NULL) {
        saved = task_new_tasks_active;
        task_new_tasks_active = 0;
        task = (SpriteTask *)task_alloc_two_node_task(0x19C, NULL, battle_object_follower_update, battle_object_follower_draw, battle_object_follower_destroy);
        sprite = &task->sprite;
        sprite->block = task;
        task->task.data = sprite;
        task->auxiliary.data = sprite;
        sprite_reset_defaults(sprite);
        sprite_attach_inline_storage(sprite);
        ((SpriteFlagBits *)&sprite->flags)->type = 4;
        sprite->block = task;
        sprite->render.word &= ~3;
        sprite->frame_bits.sequencer_owned = 0;
        ((SpriteSequencer *)sprite->sequencer)->word8 = 0;
        ((SpriteSequencer *)sprite->sequencer)->halfc = 0;
        sprite->word82 = sprite_default_scale;
        sprite->x = (u16)BATTLE_AREA.slots[slot].x << 16;
        sprite->z = (u16)BATTLE_AREA.slots[slot].z << 16;
        sprite->y = 0;
        sprite->height = battle_get_object_height(slot);
        sprite->word82 = 0x2000;
        sprite->extent_depth = sprite->height >> 1;
        sprite_set_scale(sprite, 0x2000);
        sprite->image = sprite_shared_source;
        BATTLE_AREA.sprites[slot] = sprite;
        BATTLE_AREA.tasks[slot] = task;
        sprite->resource = 0;
        sprite->animations = 0;
        task_new_tasks_active = saved;
        sprite->frame_bits.unknown30 = slot;
        sprite->motion.bits.unknown0 = slot >> 2;
    }
}

/* 800BB540: Task step: take a free stage place (of three), create the gear object of
 * the task's slot there, its sprite (800BB350), and end the task; the last
 * one sets battle_gear_objects_loaded. */
void battle_gear_load_place_object(SlotTask *task) {
    s32 i;
    s32 bit;

    for (i = 0, bit = 1; i != 3; i++, bit <<= 1) {
        if (!(battle_gear_image_places_taken & bit)) {
            battle_gear_image_places_taken |= bit;
            break;
        }
    }
    battle_create_object_from_files(task->slot, battle_gear_image_places[i].x, battle_gear_image_places[i].y, 0, task->slot + 0x1C0);
    battle_gear_file_read_count--;
    battle_object_follower_create(task->slot);
    task->task.destroy(&task->task);
    if (--battle_gear_object_load_count == 0) {
        battle_gear_objects_loaded = 1;
    }
}

/* 800BB620: Task step: once the disc is idle, run 800BB540 on a separate stack. */
void battle_gear_load_wait_disc(SlotTask *task) {
    u8 *stack;

    if (cd_get_pending_read_count() == 0) {
        stack = heap_alloc(0x1000, 1);
        STACK_ENTER(stack + 0xF00);
        battle_gear_load_place_object(task);
        STACK_LEAVE();
        heap_free(stack);
    }
}

/* 800BB690: Task step: read the gear files of the task's slot (800A9540), then
 * continue with 800BB620. */
void battle_gear_load_read_files(SlotTask *task) {
    battle_read_gear_files(task->slot);
    battle_gear_file_read_count++;
    task_set_update_callback(&task->task, (void (*)(Task *))battle_gear_load_wait_disc);
}

/* 800BB6E0: Task step: once the disc and the file reads are idle, run 800BB690 on a
 * separate stack. */
void battle_gear_load_wait_reads(SlotTask *task) {
    u8 *stack;

    if (cd_get_pending_read_count() == 0 && battle_gear_file_read_count == 0) {
        stack = heap_alloc(0x1000, 1);
        STACK_ENTER(stack + 0xF00);
        battle_gear_load_read_files(task);
        STACK_LEAVE();
        heap_free(stack);
    }
}

/* 800BB760: Start a task loading slot's gear object (800BB6E0). */
void battle_gear_load_start(s32 slot) {
    u8 saved = task_new_tasks_active;
    SlotTask *task;

    task_new_tasks_active = 0;
    task_alloc_mode = 1;
    task = (SlotTask *)task_alloc_main_task(NULL, 4);
    task_set_update_callback(&task->task, (void (*)(Task *))battle_gear_load_wait_reads);
    task->slot = slot;
    task_alloc_mode = 0;
    battle_gear_object_load_count++;
    task_new_tasks_active = saved;
}

/* 800BB7F8: Reset the camera modes. */
void battle_camera_reset(void) {
    battle_camera_ease_fraction = 0x200;
    battle_camera_framed_slots = -1;
    battle_camera_sprite_count = 0;
    battle_camera_ease_mode = 1;
    battle_camera_set_mode(0);
}

/* 800BB844: Build view matrix m looking from eye at target with up vector up. */
void battle_camera_look_at(MATRIX *m, SVECTOR *eye, SVECTOR *target, SVECTOR *up) {
    VECTOR v;
    VECTOR forward;
    VECTOR right;
    VECTOR upward;

    sprite_set_vector(&v, target->vx - eye->vx, target->vy - eye->vy, target->vz - eye->vz);
    upward.vx = up->vx;
    upward.vy = up->vy;
    upward.vz = up->vz;
    VectorNormal(&v, &forward);
    OuterProduct12(&upward, &forward, &v);
    VectorNormal(&v, &right);
    OuterProduct12(&forward, &right, &v);
    VectorNormal(&v, &upward);
    m->m[0][0] = right.vx;
    m->m[0][1] = right.vy;
    m->m[0][2] = right.vz;
    m->m[1][0] = upward.vx;
    m->m[1][1] = upward.vy;
    m->m[1][2] = upward.vz;
    m->m[2][0] = forward.vx;
    m->m[2][1] = forward.vy;
    m->m[2][2] = forward.vz;
    PushMatrix();
    ApplyMatrix(m, eye, &v);
    m->t[0] = -v.vx;
    m->t[1] = -v.vy;
    m->t[2] = -v.vz;
    PopMatrix();
}

/* 800BB9D4: Set the battle view from the camera points, shaken by 800c354c, and draw
 * the stage unless that is off. */
void battle_set_view_and_draw_stage(void) {
    battle_camera_look_at(&battle_camera.matrix, &battle_camera_view_eye, &battle_camera_view_target, &battle_camera_up_vector);
    battle_camera.matrix.t[0] += battle_quake_view_offset.vx;
    battle_camera.matrix.t[1] += battle_quake_view_offset.vy;
    battle_camera.matrix.t[2] += battle_quake_view_offset.vz;
    if (battle_stage_drawing_off == 0) {
        battle_draw_stage(&battle_camera.matrix, NULL, 0, BATTLE_AREA.ot, BATTLE_AREA.buffer, &battle_camera_view_eye, &battle_camera_view_target,
                      0x1000);
    }
}

/* 800BBAB8: Step the battle camera: take its wanted points from the camera mode, move
 * the eye and look-at points a fraction (800c3674) of the way there, and
 * derive its angles and range. */
void battle_camera_step(void) {
    SVECTOR *point;
    VECTOR step;
    VECTOR unused[2]; /* unused in the original; reserves 32 bytes */
    VECTOR delta;
    VECTOR unused2; /* unused in the original; reserves 16 bytes */
    VECTOR square;
    s32 horizontal;

    switch (battle_camera_mode) {
    case 0:
        break;
    case 1:
        battle_camera_frame_slots(battle_camera_framed_slots);
        break;
    case 2:
        battle_camera_wanted_points[0].vx = sprite_camera_eye.vx >> 16;
        battle_camera_wanted_points[0].vy = sprite_camera_eye.vy >> 16;
        battle_camera_wanted_points[0].vz = sprite_camera_eye.vz >> 16;
        point = &battle_camera_wanted_points[1];
        point->vx = sprite_camera_look_at.vx >> 16;
        point->vy = sprite_camera_look_at.vy >> 16;
        point->vz = sprite_camera_look_at.vz >> 16;
        break;
    case 3:
        /* step holds the wanted look-at, then eye point */
        ((SVECTOR *)&step)[1].vx = ((SVECTOR *)&step)[0].vx = battle_camera_circled_sprite->x >> 16;
        ((SVECTOR *)&step)[0].vy = battle_camera_circled_sprite->y >> 16;
        ((SVECTOR *)&step)[0].vz = battle_camera_circled_sprite->z >> 16;
        ((SVECTOR *)&step)[1].vz = ((SVECTOR *)&step)[0].vz - gpu_get_cos(battle_camera_circle_angle) * battle_camera_circle_distance / 4096;
        ((SVECTOR *)&step)[1].vy = ((SVECTOR *)&step)[0].vy - gpu_get_sin(battle_camera_circle_angle) * battle_camera_circle_distance / 4096;
        battle_camera.eye = ((SVECTOR *)&step)[1];
        battle_camera.target = ((SVECTOR *)&step)[0];
        break;
    }
    if (battle_camera_ease_mode == 1) {
        gte_lddp(battle_camera_ease_fraction);
        step.vx = battle_camera.eye.vx - battle_camera_view_eye.vx;
        step.vy = battle_camera.eye.vy - battle_camera_view_eye.vy;
        step.vz = battle_camera.eye.vz - battle_camera_view_eye.vz;
        gte_ldlvl(&step);
        gte_gpf12();
        gte_stlvl(&step);
        if (step.vx | step.vz | step.vy) {
            battle_camera_view_eye.vx += step.vx;
            battle_camera_view_eye.vy += step.vy;
            battle_camera_view_eye.vz += step.vz;
        } else {
            SVECTOR *wanted = &battle_camera.eye;

            battle_camera_view_eye.vx = wanted->vx;
            battle_camera_view_eye.vy = wanted->vy;
            battle_camera_view_eye.vz = wanted->vz;
        }
        step.vx = battle_camera.target.vx - battle_camera_view_target.vx;
        step.vy = battle_camera.target.vy - battle_camera_view_target.vy;
        step.vz = battle_camera.target.vz - battle_camera_view_target.vz;
        gte_ldlvl(&step);
        gte_gpf12();
        gte_stlvl(&step);
        if (step.vx | step.vz | step.vy) {
            battle_camera_view_target.vx += step.vx;
            battle_camera_view_target.vy += step.vy;
            battle_camera_view_target.vz += step.vz;
        } else {
            SVECTOR *wanted = &battle_camera.target;

            battle_camera_view_target.vx = wanted->vx;
            battle_camera_view_target.vy = wanted->vy;
            battle_camera_view_target.vz = wanted->vz;
        }
    }
    delta.vx = battle_camera_view_target.vx - battle_camera_view_eye.vx;
    delta.vy = battle_camera_view_target.vy - battle_camera_view_eye.vy;
    delta.vz = battle_camera_view_target.vz - battle_camera_view_eye.vz;
    Square0(&delta, &square);
    horizontal = SquareRoot0(square.vx + square.vz);
    battle_camera.range = SquareRoot0(square.vx + square.vy + square.vz);
    battle_camera.rot.vy = -ratan2(delta.vz, delta.vx);
    battle_camera.rot.vx = -ratan2(delta.vy, horizontal);
    battle_camera.rot.vz = 0;
}

/* 800BBEE0: Destroy of a camera sprite task: release its camera role (restoring the
 * saved point unless effects are off), free it, and when the last one ends
 * return to camera mode 800c367c. */
void battle_camera_sprite_destroy(Task *node) {
    SpriteTask *task = (SpriteTask *)node;
    SVECTOR *point;
    Sprite *sprite = task->task.data;

    if (((SpriteFlagBits *)&sprite->flags)->type == 0xA) {
        if (battle_camera_eye_task == task) {
            battle_camera_eye_task = NULL;
            if (battle_effects_disabled == 0) {
                battle_camera_wanted_points[0].vx = battle_camera_saved_eye.vx;
                battle_camera_wanted_points[0].vy = battle_camera_saved_eye.vy;
                battle_camera_wanted_points[0].vz = battle_camera_saved_eye.vz;
            }
        }
    } else if (battle_camera_target_task == task) {
        battle_camera_target_task = NULL;
        if (battle_effects_disabled == 0) {
            point = &battle_camera_wanted_points[1];
            point->vx = battle_camera_saved_target.vx;
            point->vy = battle_camera_saved_target.vy;
            point->vz = battle_camera_saved_target.vz;
        }
    }
    if (sprite->motion.bits.owns_children) {
        task_destroy_owned_by(&task->task);
    }
    task_unlink_main_node(&task->task);
    task_unlink_draw_node(&task->auxiliary);
    heap_free(task);
    if (--battle_camera_sprite_count == 0) {
        battle_camera_set_mode(battle_camera_resume_mode);
    }
}

/* 800BC018: Update of a camera sprite task: step its animation (twice when double
 * stepping), make its position the camera eye (group 0xA) or look-at point,
 * and destroy it when its animation ends. */
void battle_camera_sprite_update(Task *task) {
    Sprite *sprite = task->data;

    sprite_vm_tick(sprite);
    sprite_move(sprite);
    if (((SpriteFlagBits *)&sprite->flags)->type == 0xA) {
        sprite_camera_eye.vx = sprite->x;
        sprite_camera_eye.vy = sprite->y;
        sprite_camera_eye.vz = sprite->z;
    } else {
        sprite_camera_look_at.vx = sprite->x;
        sprite_camera_look_at.vy = sprite->y;
        sprite_camera_look_at.vz = sprite->z;
    }
    if (sprite->script != 0) {
        if (sprite->motion.bits.double_step) {
            sprite_vm_tick(sprite);
            sprite_move(sprite);
            if (((SpriteFlagBits *)&sprite->flags)->type == 0xA) {
                sprite_camera_eye.vx = sprite->x;
                sprite_camera_eye.vy = sprite->y;
                sprite_camera_eye.vz = sprite->z;
            } else {
                sprite_camera_look_at.vx = sprite->x;
                sprite_camera_look_at.vy = sprite->y;
                sprite_camera_look_at.vz = sprite->z;
            }
            if (sprite->script == 0) {
                task->destroy(task);
            }
        }
    } else {
        task->destroy(task);
    }
}

/* 800BC158: Make sprite task a camera sprite: the eye (group 0xA) or look-at sprite,
 * saving the camera point or taking over (field34 1) from a running one,
 * else stopping the new one; then camera mode 2 follows the sprites. */
void battle_camera_register_sprite(SpriteTask *task) {
    SVECTOR *point;
    Sprite *sprite = &task->sprite;

    if (((SpriteFlagBits *)&sprite->flags)->type == 0xA) {
        if (battle_camera_eye_task != NULL) {
            if (battle_camera_eye_task->sprite.frame != 1 && sprite->frame == 1) {
                battle_camera_eye_task->task.destroy(&battle_camera_eye_task->task);
                battle_camera_eye_task = task;
            } else {
                sprite->countdown = 0;
                sprite->script = 0;
            }
        } else {
            battle_camera_eye_task = task;
            battle_camera_saved_eye.vx = battle_camera_wanted_points[0].vx;
            battle_camera_saved_eye.vy = battle_camera_wanted_points[0].vy;
            battle_camera_saved_eye.vz = battle_camera_wanted_points[0].vz;
        }
    } else if (battle_camera_target_task != NULL) {
        if (battle_camera_target_task->sprite.frame != 1 && sprite->frame == 1) {
            battle_camera_target_task->task.destroy(&battle_camera_target_task->task);
            battle_camera_target_task = task;
        } else {
            sprite->countdown = 0;
            sprite->script = 0;
        }
    } else {
        battle_camera_target_task = task;
        point = &battle_camera_wanted_points[1];
        battle_camera_saved_target.vx = point->vx;
        battle_camera_saved_target.vy = point->vy;
        battle_camera_saved_target.vz = point->vz;
    }
    battle_camera_sprite_count++;
    if (sprite->motion.bits.mirror) {
        sprite->direction = 0x800;
    } else {
        sprite->direction = 0;
    }
    task_set_destroy_callback(&task->task, battle_camera_sprite_destroy);
    task_set_update_callback(&task->task, battle_camera_sprite_update);
    battle_camera_set_mode(2);
}

/* 800BC2F0: Set the camera mode: 2 puts the eye and look-at sprites at the saved
 * points, 4 sets battle_camera_ease_mode to 5, others release them. */
void battle_camera_set_mode(s32 mode) {
    SVECTOR *point;

    battle_camera_mode = mode;
    battle_camera_ease_mode = 1;
    switch (mode) {
    case 4:
        battle_camera_ease_mode = 5;
        break;
    case 2:
        sprite_camera_eye.vx = battle_camera_wanted_points[0].vx << 16;
        sprite_camera_eye.vy = battle_camera_wanted_points[0].vy << 16;
        sprite_camera_eye.vz = battle_camera_wanted_points[0].vz << 16;
        point = &battle_camera_wanted_points[1];
        sprite_camera_look_at.vx = point->vx << 16;
        sprite_camera_look_at.vy = point->vy << 16;
        sprite_camera_look_at.vz = point->vz << 16;
        break;
    default:
        if (battle_camera_eye_task != NULL) {
            battle_camera_eye_task->task.destroy(&battle_camera_eye_task->task);
            battle_camera_eye_task = NULL;
        }
        if (battle_camera_target_task != NULL) {
            battle_camera_target_task->task.destroy(&battle_camera_target_task->task);
            battle_camera_target_task = NULL;
        }
        break;
    }
}

/* 800BC3F8: Set battle_camera_resume_mode. */
void battle_camera_set_resume_mode(s32 value) {
    battle_camera_resume_mode = value;
}

/* 800BC404: Start camera move (800BC460) unless effects are off; restore mode_battle_camera_range. */
void battle_camera_start_move(s32 mask) {
    if (battle_effects_disabled == 0) {
        battle_camera_set_mode(1);
        battle_camera_frame_slots(mask);
    }
    mode_battle_camera_range = battle_camera_framed_range;
}

/* 800BC454: Set the camera framing pitch. */
void battle_camera_set_framing_pitch(s16 value) {
    battle_camera_framing_angles.vx = value;
}

/* 800BC460: Frame the camera on the party slots in mask: look at the middle of their
 * sprites from the framing angles, at a range that keeps the farthest sprite
 * (and its gear top) on screen; the points go to the camera's wanted eye and
 * look-at points. */
void battle_camera_frame_slots(u32 mask) {
    VECTOR center;
    SVECTOR eye;
    SVECTOR target;
    MATRIX m;
    VECTOR offset;
    SVECTOR point;
    long screen[2];
    SVECTOR v;
    long result[2];
    MATRIX m2;
    VECTOR out;
    SVECTOR v2;
    MATRIX m3;
    VECTOR unused;
    SVECTOR v3;
    Sprite *sprite;
    s32 i;
    s32 count;
    s32 farthest;
    s32 minX, maxX, minY, maxY, minZ, maxZ;
    s32 distance;
    s32 range;
    u32 bits;

    memset(&center, 0, sizeof(center));
    farthest = 0;
    battle_camera_framed_slots = mask;
    i = 0;
    count = 0;
    for (bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
        if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
            count++;
            center.vx += sprite->x >> 1;
            center.vy += sprite->y >> 1;
            center.vz += sprite->z >> 1;
        }
    }
    if (count != 0) {
        center.vx = center.vx / count * 2;
        center.vy = center.vy / count * 2;
        center.vz = center.vz / count * 2;
        maxX = minX = center.vx;
        maxZ = minZ = center.vz;
        maxY = minY = center.vy;
        for (i = 0, bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
            if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
                if (maxX < sprite->x) {
                    maxX = sprite->x;
                }
                if (sprite->x < minX) {
                    minX = sprite->x;
                }
                if (maxZ < sprite->z) {
                    maxZ = sprite->z;
                }
                if (sprite->z < minZ) {
                    minZ = sprite->z;
                }
                if (maxY < sprite->y) {
                    maxY = sprite->y;
                }
                if (sprite->y < minY) {
                    minY = sprite->y;
                }
            }
        }
        center.vx = (minX + maxX) / 2;
        center.vy = (maxY + minY) / 2;
        center.vz = (minZ + maxZ) / 2;
        center.vx >>= 16;
        center.vy >>= 16;
        center.vz >>= 16;
        RotMatrix(&battle_camera_framing_angles, &m);
        v.vx = 0;
        v.vy = 0;
        v.vz = ReadGeomScreen() * 8;
        ApplyMatrix(&m, &v, &offset);
        eye.vx = center.vx;
        eye.vy = center.vy;
        eye.vz = center.vz;
        target.vx = center.vx;
        target.vy = center.vy;
        target.vz = center.vz;
        eye.vx -= offset.vx;
        eye.vy += offset.vy;
        eye.vz -= offset.vz;
        battle_camera_look_at(&m, &eye, &target, &battle_camera_up_vector);
        SetRotMatrix(&m);
        SetTransMatrix(&m);
        for (i = 0, bits = mask; i != 11; i++, bits = (bits & 0xFFFF) >> 1) {
            if ((bits & 1) && !BATTLE_AREA.slots[i].hidden && (sprite = BATTLE_AREA.sprites[i]) != NULL) {
                point.vx = sprite->x >> 16;
                point.vy = sprite->y >> 16;
                point.vz = sprite->z >> 16;
                RotTransPers(&point, screen, &result[0], &result[1]);
                ((s16 *)screen)[0] -= 160;
                ((s16 *)screen)[1] -= 164;
                ((s16 *)screen)[0] <<= 2;
                ((s16 *)screen)[1] <<= 2;
                distance = ((s16 *)screen)[0] * ((s16 *)screen)[0];
                distance += ((s16 *)screen)[1] * ((s16 *)screen)[1];
                if (farthest < distance) {
                    farthest = distance;
                }
                if (BATTLE_AREA.slots[i].gear && battle_camera_skip_gear_heights == 0) {
                    point.vy -= sprite->height;
                    RotTransPers(&point, screen, &result[0], &result[1]);
                    ((s16 *)screen)[0] -= 160;
                    ((s16 *)screen)[1] -= 164;
                    ((s16 *)screen)[0] <<= 2;
                    ((s16 *)screen)[1] <<= 2;
                    distance = ((s16 *)screen)[0] * ((s16 *)screen)[0];
                    distance += ((s16 *)screen)[1] * ((s16 *)screen)[1];
                    if (farthest < distance) {
                        farthest = distance;
                    }
                }
            }
        }
        farthest = SquareRoot0(farthest);
        if (farthest < 120) {
            RotMatrix(&battle_camera_framing_angles, &m2);
            v2.vx = 0;
            v2.vy = 0;
            v2.vz = ReadGeomScreen() * 2;
            battle_camera_framed_range = ReadGeomScreen() * 2;
            ApplyMatrix(&m2, &v2, (VECTOR *)&point);
            eye.vx = center.vx;
            eye.vy = center.vy;
            eye.vz = center.vz;
            target.vx = center.vx;
            target.vy = center.vy;
            target.vz = center.vz;
            eye.vx -= (*(VECTOR *)&point).vx;
            eye.vy += (*(VECTOR *)&point).vy;
            eye.vz -= (*(VECTOR *)&point).vz;
            battle_camera_wanted_points[0].vx = eye.vx;
            battle_camera_wanted_points[0].vy = eye.vy;
            battle_camera_wanted_points[0].vz = eye.vz;
            {
                SVECTOR *p = &battle_camera_wanted_points[1];

                p->vx = target.vx;
                p->vy = target.vy;
                p->vz = target.vz;
            }
        } else {
            range = (farthest << 14) / 120;
            range = (range << 1) * ReadGeomScreen();
            range >>= 14;
            battle_camera_framed_range = range;
            RotMatrix(&battle_camera_framing_angles, &m3);
            v3.vx = 0;
            v3.vy = 0;
            v3.vz = range;
            ApplyMatrix(&m3, &v3, &out);
            eye.vx = center.vx;
            eye.vy = center.vy;
            eye.vz = center.vz;
            target.vx = center.vx;
            target.vy = center.vy;
            target.vz = center.vz;
            eye.vx -= out.vx;
            eye.vy += out.vy;
            eye.vz -= out.vz;
            battle_camera_wanted_points[0].vx = eye.vx;
            battle_camera_wanted_points[0].vy = eye.vy;
            battle_camera_wanted_points[0].vz = eye.vz;
            {
                SVECTOR *p = &battle_camera_wanted_points[1];

                p->vx = target.vx;
                p->vy = target.vy;
                p->vz = target.vz;
            }
        }
    }
}

/* 800BCAA4: Camera mode 4 (800BC2F0), unless effects are disabled. */
void battle_camera_hold(void) {
    if (battle_effects_disabled == 0) {
        battle_camera_set_mode(4);
    }
}

/* 800BCAD0: Camera mode 1 (800BC2F0), unless effects are disabled. */
void battle_camera_track_slots(void) {
    if (battle_effects_disabled == 0) {
        battle_camera_set_mode(1);
    }
}

/* 800BCAFC: Shade the sprite by the cosine of angle (0x80 plus half, at most 0xFF)
 * and update it (8001F6B0). */
void battle_shade_sprite_by_angle(Sprite *sprite, s32 angle) {
    s32 level = gpu_get_sin(angle << 6) + 0x1000;

    level >>= 6;
    level += 0x80;
    if (level >= 0x100) {
        level = 0xFF;
    }
    sprite->red = level;
    sprite->green = level;
    sprite->blue = level;
    sprite_recolor_parts(sprite);
}

/* 800BCB54: End the acting slot's pulse: restore its sprite's colour. */
void battle_slot_pulse_destroy(Task *task) {
    SlotPulse *pulse = battle_current_slot_pulse;

    if (pulse != NULL) {
        pulse->sprite->colour_flags |= 1;
        task_unlink_main_node(&pulse->task);
        heap_free(pulse);
        battle_current_slot_pulse = NULL;
    }
}

/* 800BCBB4: Pulse update: the sprite's red and green-blue follow two phases of the
 * tick (0x80 plus half, at most 0xFF). */
void battle_slot_pulse_update(Task *task) {
    SlotPulse *pulse = (SlotPulse *)task;
    s32 angle = pulse->tick << 6;
    Sprite *sprite = pulse->sprite;
    s32 level;

    level = gpu_get_sin(angle) + 0x1000;
    level >>= 6;
    level += 0x80;
    if (level >= 0x100) {
        level = 0xFF;
    }
    sprite->red = level;
    level = gpu_get_cos(angle) + 0x1000;
    level >>= 6;
    level += 0x80;
    if (level >= 0x100) {
        level = 0xFF;
    }
    sprite->green = level;
    sprite->blue = level;
    sprite_recolor_parts(sprite);
    pulse->tick++;
}

/* 800BCC60: Start the acting slot's pulse (unless it already runs for that slot). */
void battle_slot_pulse_start(void) {
    SlotPulse *pulse;
    Sprite *sprite;

    if (battle_current_slot_pulse != NULL) {
        if (battle_current_slot_pulse->slot == battle_acting_slot) {
            return;
        }
        battle_slot_pulse_destroy(NULL);
    }
    if (BATTLE_AREA.tasks[AREA_ACTING_SLOT] == NULL) {
        return;
    }
    pulse = (SlotPulse *)task_alloc_main_task(&BATTLE_AREA.tasks[AREA_ACTING_SLOT]->task, sizeof(SlotPulse) - sizeof(Task));
    battle_current_slot_pulse = pulse;
    task_set_update_callback(&pulse->task, battle_slot_pulse_update);
    task_set_destroy_callback(&pulse->task, battle_slot_pulse_destroy);
    if (task_new_tasks_active) {
        task_active_main_count--;
    }
    pulse->task.link.word &= 0x7FFFFFFF;
    pulse->sprite = sprite = BATTLE_AREA.sprites[AREA_ACTING_SLOT];
    pulse->slot = AREA_ACTING_SLOT;
    sprite->colour_flags &= ~1;
}

/* 800BCD8C: Clear the highlighted slots. */
void battle_highlight_clear(void) {
    battle_highlight_slot_mask = 0;
}

/* 800BCD98: Highlight the slots of mask (bit per slot): pulse the acting slot while
 * any is set, and give each highlighted slot's sprite a ring (ending the
 * others'). */
void battle_highlight_slots(u16 mask) {
    s32 slot;
    SpriteTask *task;
    Task *ring;

    battle_highlight_slot_mask = mask;
    if (mask) {
        battle_slot_pulse_start();
    } else {
        battle_slot_pulse_destroy(NULL);
    }
    for (slot = 0; slot != 11; slot++, mask >>= 1) {
        if (mask & 1) {
            task = BATTLE_AREA.tasks[slot];
            if (task != NULL && task_find_owned_with_update(&task->task, battle_slot_ring_update) == NULL) {
                battle_slot_ring_create(task);
            }
        } else {
            task = BATTLE_AREA.tasks[slot];
            if (task != NULL) {
                ring = task_find_owned_with_update(&task->task, battle_slot_ring_update);
                if (ring != NULL) {
                    ring->destroy(ring);
                }
            }
        }
    }
}

/* 800BCEAC: Ring draw: place, spin and scale it (to a constant screen size) and draw
 * its script into this buffer's vertices. */
void battle_slot_ring_draw(Task *draw) {
    SlotRing *ring = draw->data;
    MATRIX m;
    VECTOR scale;
    s32 size;

    TransMatrix(&m, &ring->pos);
    gpu_build_rotation_matrix(&ring->angle, &m);
    CompMatrix(&sprite_view_matrix, &m, &m);
    size = ReadGeomScreen() << 12;
    if (m.t[2] != 0) {
        size /= m.t[2];
    }
    size = 0x1000000 / size / 2;
    sprite_set_vector(&scale, size, size, size);
    ScaleMatrixL(&m, &scale);
    SetRotMatrix(&m);
    SetTransMatrix(&m);
    battle_tmd_draw_object(ring->script, ring->vertices[BATTLE_AREA.buffer], (u32 *)sprite_ot, 0, 0, 0);
}

/* 800BCFAC: Ring update: follow the sprite (0x20 above its top), shade it and spin. */
void battle_slot_ring_update(Task *task) {
    SlotRing *ring = task->data;
    Sprite *sprite = ring->sprite;

    ring->pos.vx = FIXED_WHOLE(sprite->x);
    ring->pos.vy = FIXED_WHOLE(sprite->y);
    ring->pos.vz = FIXED_WHOLE(sprite->z);
    ring->pos.vy = ring->pos.vy - ring->height - 0x20;
    battle_shade_sprite_by_angle(sprite, ++ring->tick);
    ring->angle.vy += 0x10;
}

/* 800BD024: Ring destroy: restore the sprite's colour and free the ring. */
void battle_slot_ring_destroy(Task *task) {
    SlotRing *ring = task->data;
    Sprite *sprite = ring->sprite;

    sprite->colour_flags |= 1;
    sprite_recolor_parts(sprite);
    sprite_queue_free_later((u32)ring->vertices[0]);
    task_destroy_owned_by(task);
    task_unlink_main_node(task);
    heap_free(ring);
}

/* 800BD098: Give an actor task's sprite a ring. */
void battle_slot_ring_create(SpriteTask *owner) {
    Sprite *sprite = owner->task.data;
    SlotRing *ring = (SlotRing *)task_alloc_two_node_task(sizeof(SlotRing), &owner->task, battle_slot_ring_update, battle_slot_ring_draw, battle_slot_ring_destroy);
    s32 size;
    u8 *vertices;

    ring->sprite = sprite;
    ring->height = sprite->height;
    if (task_new_tasks_active) {
        task_active_main_count--;
    }
    ring->task.link.word &= 0x7FFFFFFF;
    sprite_set_svector(&ring->angle, 0, 0, 0);
    size = battle_tmd_get_packet_size((ScriptEntry *)battle_tmd_get_object(model_slot_ring_tmd, 0));
    vertices = heap_alloc(size * 2, 0);
    battle_tmd_build_packets(battle_tmd_get_object(model_slot_ring_tmd, 0), vertices, 0, 1);
    memcpy(vertices + size, vertices, size);
    ring->vertices[0] = vertices;
    ring->vertices[1] = vertices + size;
    ring->script = battle_tmd_get_object(model_slot_ring_tmd, 0);
    sprite->colour_flags &= ~1;
    sprite_recolor_parts(sprite);
    battle_slot_ring_update(&ring->task);
}

/* 800BD1FC: Show the current event's result on slot's sprite (800BD3AC), with the
 * running total and colour kind, once. */
void battle_show_slot_result(s32 slot) {
    Sprite *sprite;
    s32 code;
    s32 total;
    s32 colour;
    s32 amount;

    if (BATTLE_AREA.events[battle_area_event_index - 1].codes[slot] != 0xFF && BATTLE_AREA.events[battle_area_event_index - 1].amounts[slot] != 0xFFFF) {
        sprite = BATTLE_AREA.sprites[slot];
        if (sprite != NULL) {
            code = BATTLE_AREA.events[battle_area_event_index - 1].codes[slot];
            total = BATTLE_AREA.events[battle_area_event_index - 1].accumulated[slot];
            colour = BATTLE_AREA.events[battle_area_event_index - 1].accumulatedCodes[slot];
            amount = BATTLE_AREA.events[battle_area_event_index - 1].amounts[slot];
            battle_running_total = total;
            battle_popup_color_kind = colour;
            battle_damage_popup_show(sprite, amount, code);
            BATTLE_AREA.events[battle_area_event_index - 1].amounts[slot] = 0xFFFF;
        }
    }
}

/* 800BD2E4: Show the current event's results on every slot (800BD1FC); once a slot
 * has code 7, not on the acting sprite. */
void battle_show_results(void) {
    u8 skipActor = 0;
    s32 slot;
    Sprite *sprite;

    for (slot = 0; slot != 11; slot++) {
        if (BATTLE_AREA.events[battle_area_event_index - 1].codes[slot] == 7) {
            skipActor = 1;
        }
        sprite = BATTLE_AREA.sprites[slot];
        if (sprite != NULL && (battle_acting_sprite != sprite || !skipActor)) {
            battle_show_slot_result(slot);
        }
    }
}
