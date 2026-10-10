/* Battle unit from 800B3F04 to 800B7134: the sprite script command handler
 * (800B3F04, commands 1-107) and its commands, the sprite effects (trails,
 * approach watches, partner links, streaks) and the shattered screen's update
 * (Cygnus CDK GCC 2.7.2, like battle_tmd_screen_effects.c). 800B1F6C's odd-length table
 * ends at 0x80070850 and 800B3F04's follows unpadded at 0 mod 8, so a unit
 * starts between the two; the functions from 800B2AEC to 800B3E04 have no
 * rodata, and the boundary is placed at the first function that has (the
 * previous unit's own .bss is used up to 800B3E04). Its 107-entry table, all
 * its rodata, is followed directly by 800B7870's at 0x800709FC (4 mod 8), so
 * the unit ends before 800B7870; it ends before 800B7134, whose shatter draw
 * shares 800B7870's own variable battle_shatter_ot (a unit's .bss is read by its own
 * code only, docs/matching.md). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/model.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "battle/action_file.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/flow.h"
#include "battle/frame.h"
#include "battle/highlight.h"
#include "battle/objects.h"
#include "battle/scene.h"
#include "battle/screen.h"
#include "battle/setup.h"
#include "battle/sprite.h"
#include "battle/sprite_script.h"
#include "curve.h"
#include "files.h"
#include "overlays.h"
#include "own_declarations.h"
#include "resident_views.h"
#include "sprite_effect.h"

/* This unit's functions, declared before their first use. */
void battle_sprite_command_copy_part_texture(Sprite *sprite);
void battle_trail_update(Task *task);
void battle_trail_create(Sprite *sprite, u8 *colours);
void battle_link_update();
SpriteLink *battle_link_create();
void battle_orbit_start(Sprite *sprite);
void battle_streak_start();
void battle_parent_line_start();
void battle_sprite_command_load_object_set();
void battle_sprite_command_free_stage_object();
void battle_sprite_command_show_stage_object();
void battle_sprite_command_fade_lights();
void battle_sprite_command_upload_images();
void battle_sprite_command_draw_unlit_model();
void battle_sprite_command_shift_cluts();
void battle_sprite_command_slot_animation();
void battle_sprite_command_face_target_point();
void battle_sprite_command_steer_velocity();
void battle_sprite_command_aim_velocity();
void battle_sprite_command_set_position();
void battle_sprite_set_script_vector();
void battle_sprite_add_script_vector();
void battle_sprite_command_take_parent_velocity();
void battle_sprite_command_break_image();
void battle_sprite_command_spin();
void battle_sprite_command_shatter_screen();
void battle_sprite_command_roll_to_velocity();
void battle_sprite_command_pitch_to_velocity();
void battle_sprite_command_face_velocity();
void battle_sprite_command_clear_anchors();
void battle_sprite_command_turn_velocity();

/* The unit's own uninitialized variables (its .bss, after
 * battle_tmd_screen_effects.c's): ASPSX 2.56 keeps the halfwords two bytes apart and
 * starts the 3-byte colour at the next word (decomp/Makefile). */
static s16 battle_trail_last_corner_x2;     /* 800C3CA4: the last trail segment's far corners */
static s16 battle_trail_last_corner_y2;     /* 800C3CA6 */
static s16 battle_trail_last_corner_x3;     /* 800C3CA8 */
static s16 battle_trail_last_corner_y3;     /* 800C3CAA */
static u8 battle_saved_background_flag;     /* 800C3CAC: the saved background flag of the display buffers */
static u8 battle_saved_background_color[3]; /* 800C3CB0: the saved background colour */

u8 battle_unread_sprite_slot_mark = 0; /* 800C3564 */
Sprite *battle_sprite_for_debugger = NULL; /* 800C3568 */
u8 battle_trail_anchor_indices[5] = {1, 2, 3, 5, 6}; /* 800C356C */
MATRIX battle_screen_space_matrix = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}}; /* 800C3574 */
/* The shatter's triangles and launch velocity, which only the next unit's
 * code (800B7134-800B7424) reads; where that unit's data starts is not
 * known, so they stay here. */
SVECTOR battle_shatter_upper_left_triangle[3] = {{-160, -160, 0}, {352, -160, 0}, {-160, 352, 0}}; /* 800C3594 */
SVECTOR battle_shatter_lower_right_triangle[3] = {{160, -352, 0}, {160, 160, 0}, {-352, 160, 0}}; /* 800C35AC */
VECTOR battle_shatter_launch_velocity = {0, 0, -1536 << 16}; /* 800C35C4 */
/* The sound fade flag that 800B7870 and 800B8098 also use; whether it ends
 * this unit's data or the next unit's is not known. Stray bytes follow it,
 * so it stays original data. */
INCLUDE_ORIGINAL(".data", battle_music_lowered, 0x800C35D4, 4);

/* 800B3F04: Run battle sprite script command (1-107) on sprite with its argument
 * bytes: motion, velocity and gravity settings, render flags, camera and
 * display switches, sounds, target highlights and the helpers 800B4EDC-
 * 800B6E84 and the battle module's 801FC6FC/801FC7B0/801FC898. */
void battle_sprite_command_run(Sprite *sprite, s32 command, u8 *args) {
    s32 unused[10]; /* allocated in the original frame */
    VECTOR eye;
    VECTOR target;
    Sprite *other;
    Task *task;
    s32 distance;
    s32 scale;
    s32 kind;
    u8 isbg, r, g, b;
    u16 played;

    switch (command) {
    /* 6b: copy the three VRAM columns away (800b3e04). */
    case 0x6B:
        battle_save_vram_columns();
        break;
    /* 6a: flags 800c492a = 1, 800c35d4 = 0; fade sequence 800c3e54 to 0 over 120 frames. */
    case 0x6A:
        battle_turn_hud_hidden = 1;
        battle_music_lowered = 0;
        sound_set_seq_fade((SoundSeq *)battle_music_seq, 0, 0x78);
        break;
    /* 69: rebind the block idle bit 10 names: +4c when set, else +48 (bit 10 kept). */
    case 0x69:
        if ((sprite->b0.wordb0 >> 10) & 1) {
            sprite_bind_resource(sprite, (s32 *)sprite->resource);
            sprite->b0.wordb0 |= 0x400;
        } else {
            sprite_bind_resource(sprite, sprite->animations);
            sprite->b0.wordb0 &= ~0x400;
        }
        break;
    /* 68: bind +4c (idle bit 10 set) for a negative animation, else +48. */
    case 0x68:
        if ((s8)sprite->motion.bytes[3] < 0) {
            sprite_bind_resource(sprite, (s32 *)sprite->resource);
            sprite->b0.wordb0 |= 0x400;
        } else {
            sprite_bind_resource(sprite, sprite->animations);
            sprite->b0.wordb0 &= ~0x400;
        }
        break;
    /* 66: flag 800c3688 = 1. */
    case 0x66:
        battle_camera_skip_gear_heights = 1;
        break;
    /* 64: idle bit 9, then as 63. */
    case 0x64:
        sprite->b0.wordb0 |= 0x200;
    /* 63: when (s16)80059454 > 0x200: field3a = it, facing group 3, render bit 28. */
    case 0x63:
        if ((s16)mode_battle_camera_range > 0x200) {
            sprite->rate = mode_battle_camera_range;
            sprite->flags = (sprite->flags & ~0x1F00) | 0x300;
            sprite->render.word |= 0x10000000;
        }
        break;
    /* 62: show the current event's result on this slot (800bd1fc). */
    case 0x62:
        battle_show_slot_result(SPRITE_SLOT(sprite));
        break;
    /* 61 s8: y = ground - 1; aim the jump (800ba768) at the target point (+a0) = the
     * partner's x + s8 (scaled; the side by its home place), y 0 and z. */
    case 0x61:
        sprite->y = (sprite->ground - 1) << 16;
        other = sprite->partner;
        distance = (s8)args[0] * sprite->scale / 4096;
        if (FIXED_WHOLE(other->x) == (u16)BATTLE_AREA.slots[SPRITE_SLOT(other)].x) {
            if (!sprite->motion.bits.mirror) {
                distance = -distance;
            }
        } else if (FIXED_WHOLE(other->x) < (u16)BATTLE_AREA.slots[SPRITE_SLOT(other)].x) {
            distance = -distance;
        }
        sprite->target_x = (other->x >> 16) + distance;
        sprite->target_z = other->z >> 16;
        sprite->target_y = 0;
        battle_sprite_aim_jump_keep_rise(sprite);
        break;
    /* 5f: 800c3564 = 1 (nothing recovered reads it). */
    case 0x5F:
        battle_unread_sprite_slot_mark = 1;
        break;
    /* 60: 800c3564 = the sprite's slot + 2. */
    case 0x60:
        battle_unread_sprite_slot_mark = SPRITE_SLOT(sprite) + 2;
        break;
    /* 59: screen fades work again (800d3638 = 0). */
    case 0x59:
        battle_screen_fade_blocked = 0;
        break;
    /* 5a: screen fades are ignored (800d3638 = 1: 800b39c0 returns at once). */
    case 0x5A:
        battle_screen_fade_blocked = 1;
        break;
    /* 57: flag 800c3b74 = 1 (800aa788). */
    case 0x57:
        battle_set_object_drawing(1);
        break;
    /* 58: flag 800c3b74 = 0 (800aa788). */
    case 0x58:
        battle_set_object_drawing(0);
        break;
    /* 56: with 800c3622, play sound kind + 0x52 of the scripts' bank for the slot's kind
     * (8, 9, 10 as 15, 2, 6), once per slot (800c3626). */
    case 0x56: {
        s32 slot;
        s32 low_slot;
        if (battle_wave_bank_7_loaded) {
            low_slot = sprite->frame_bits.unknown30;
            slot = sprite->motion.bits.unknown0 << 2 | low_slot;
            kind = BATTLE_AREA.slots[slot].field2;
            if (kind == 9) {
                kind = 2;
            }
            if (kind == 10) {
                kind = 6;
            }
            if (kind == 8) {
                kind = 15;
            }
            played = battle_gear_sound_played_slots;
            if (!((played >> slot) & 1)) {
                battle_gear_sound_played_slots = played | (1 << slot);
                sound_play_effect((kind + 0x52) | (sprite_script_sound_bank->bank << 16));
            }
        }
        break;
    }
    /* 54: the loaded battle module's 801fc898 (ovl3387: its effect on an 8 KB stack). */
    case 0x54:
        battle_module_burst_play();
        break;
    /* 4f u8: play sound u8 of the sprite's bank on the last two effect voices (80039db8). */
    case 0x4F:
        if (sprite->word50 != 0) {
            sound_play_effect_on_last_channels(args[0] | (((SoundBank *)sprite->word50)->id << 16));
        }
        break;
    /* 52 u8 v: play sound u8 of the sprite's bank on effect voices (v & fe) ^ 8 (80039ec4). */
    case 0x52:
        if (sprite->word50 != 0) {
            sound_play_effect_on_channel(args[0] | (((SoundBank *)sprite->word50)->id << 16), args[1]);
        }
        break;
    /* 4d u8: stop effect u8 of the sprite's bank (8003a14c). */
    case 0x4D:
        if (sprite->word50 != 0) {
            sound_stop_effect(args[0] | (((SoundBank *)sprite->word50)->id << 16));
        }
        break;
    /* 4e u8: stop effect u8 of the scripts' bank (8003a14c). */
    case 0x4E:
        if (sprite_script_sound_bank != NULL) {
            sound_stop_effect(args[0] | (sprite_script_sound_bank->bank << 16));
        }
        break;
    /* 4c: flags bit 1 (a lit model). */
    case 0x4C:
        sprite->flags |= 2;
        break;
    /* 48: flags bit 0. */
    case 0x48:
        sprite->flags |= 1;
        break;
    /* 67: wait until the camera eye and look-at are within 4 of the marked points (800d3354,
     * 800d335c): back two bytes and a frame (c3 form). */
    case 0x67:
        sprite->countdown++;
        eye.vx = battle_camera.eye.vx - battle_camera_view_eye.vx;
        eye.vy = battle_camera.eye.vy - battle_camera_view_eye.vy;
        eye.vz = battle_camera.eye.vz - battle_camera_view_eye.vz;
        Square0(&eye, &eye);
        target.vx = battle_camera.target.vx - battle_camera_view_target.vx;
        target.vy = battle_camera.target.vy - battle_camera_view_target.vy;
        target.vz = battle_camera.target.vz - battle_camera_view_target.vz;
        Square0(&target, &target);
        if (SquareRoot0(eye.vx + eye.vy + eye.vz) < 4 && SquareRoot0(target.vx + target.vy + target.vz) < 4) {
            break;
        }
        sprite->script -= 2;
        break;
    /* 50: wait a frame; with a hit counted (800d36bc, 800bf998) take one and go on, else
     * retry: back two bytes (the c3 form). */
    case 0x50:
        sprite->countdown++;
        if (battle_pending_hit_count != 0) {
            battle_pending_hit_count--;
            break;
        }
        sprite->script -= 2;
        break;
    /* 46 u8: load the battle's sound bank set u8 (800b61f8, 800a96b4); both hit counts
     * (800d36bc, 800d2d4c) = 0. */
    case 0x46:
        battle_sprite_command_load_object_set(sprite, args);
        battle_effect_hit_count = 0;
        break;
    /* 51: free stage object 11 (800b626c). */
    case 0x51:
        battle_sprite_command_free_stage_object(sprite, args);
        break;
    /* 47 u8: show stage object 11 in mode u8 (800b62c8). */
    case 0x47:
        battle_sprite_command_show_stage_object(sprite, args);
        break;
    /* 44: the debug actor tool (debug2611) selects this sprite (800c3568), unless
     * 80010000 is -1. */
    case 0x44:
        if (*(s32 *)0x80010000 != -1) {
            battle_sprite_for_debugger = sprite;
        }
        break;
    /* 45: the debug actor tool selects none (800c3568), unless 80010000 is -1. */
    case 0x45:
        if (*(s32 *)0x80010000 != -1) {
            battle_sprite_for_debugger = NULL;
        }
        break;
    /* 43: render bit 31 (drawn about the screen centre). */
    case 0x43:
        sprite->render.word |= 0x80000000;
        break;
    /* 3e: partner = the next of the event's targets (800bf8cc). */
    case 0x3E:
        battle_sprite_next_target(sprite);
        break;
    /* 3d: copy the current part's texture block and CLUT row (800b4edc). */
    case 0x3D:
        battle_sprite_command_copy_part_texture(sprite);
        break;
    /* 3a s16: fade the lights with the six bytes at the arguments + s16 (800b639c). */
    case 0x3A:
        battle_sprite_command_fade_lights(sprite, args);
        break;
    /* 42: motion bit 5 off. */
    case 0x42:
        sprite->motion.word &= ~0x20;
        break;
    /* 38 s16: upload the image list at the arguments + s16 (800b63f0). */
    case 0x38:
        battle_sprite_command_upload_images(sprite, args);
        break;
    /* 37: request the upload of directory 2c file 1's images (800c3621; 800bf9ec does it
     * and clears the flag). */
    case 0x37:
        battle_image_upload_requested = 1;
        break;
    /* 34 s8: gravity = (s8 * 2 * (+82) / 4096 << 5) * (skip + 1)^2. */
    case 0x34:
        sprite->gravity = (((s8)args[0] << 1) * (s16)sprite->word82 / 4096) << 5;
        sprite->gravity *= (sprite_frame_skip + 1) * (sprite_frame_skip + 1);
        break;
    /* 33 u8: with an animation block (+48), play animation u8. */
    case 0x33:
        if (sprite->animations != 0) {
            sprite_start_animation(sprite, args[0]);
        }
        break;
    /* 31: geometry offset 160, 112. */
    case 0x31:
        SetGeomOffset(0xA0, 0x70);
        break;
    /* 32: geometry offset 160, 164. */
    case 0x32:
        SetGeomOffset(0xA0, 0xA4);
        break;
    /* 30 u8: one-sided views: offset (+3c, +3d) = anchor u8's x, y. */
    case 0x30:
        if (sprite->renderer != NULL && (sprite->render.word & 3) == 1 && sprite->renderer->pointer34 != NULL) {
            sprite->renderer->offset.y = sprite->renderer->pointer34[args[0]].byte1;
            sprite->renderer->offset.x = sprite->renderer->pointer34[args[0]].byte0;
        }
        break;
    /* 2f: 800c3628 = this sprite (800bf730). */
    case 0x2F:
        battle_request_single_action_start((s32)sprite);
        break;
    /* 2c: draw as an unlit model (draw task update 80025a88, 800b6438). */
    case 0x2C:
        battle_sprite_command_draw_unlit_model(sprite, args);
        break;
    /* 27 x y: move the parts' CLUTs by x, y (800b6464). */
    case 0x27:
        battle_sprite_command_shift_cluts(sprite, args);
        break;
    /* 26 a s: battle sprite s plays animation a (800b64d4). */
    case 0x26:
        battle_sprite_command_slot_animation(sprite, args);
        break;
    /* 28 s8: x velocity = s8 * 16 * (+82) / 4096 << 8. */
    case 0x28:
        sprite->speed_x = (((s8)args[0] << 4) * (s16)sprite->word82 / 4096) << 8;
        break;
    /* 29 s8: x velocity += s8 * 16 * (+82) / 4096 << 8. */
    case 0x29:
        sprite->speed_x += (((s8)args[0] << 4) * (s16)sprite->word82 / 4096) << 8;
        break;
    /* 21 s8: z velocity = s8 * 16 * (+82) / 4096 << 12. */
    case 0x21:
        sprite->speed_z = (((s8)args[0] << 4) * (s16)sprite->word82 / 4096) << 12;
        break;
    /* 22 s8: z velocity += s8 * 16 * (+82) / 4096 << 8. */
    case 0x22:
        sprite->speed_z += (((s8)args[0] << 4) * (s16)sprite->word82 / 4096) << 8;
        break;
    /* 2a s8: y velocity = s8 * 16 * (+82) / 4096 << 12. */
    case 0x2A:
        sprite->speed_y = (((s8)args[0] << 4) * (s16)sprite->word82 / 4096) << 12;
        break;
    /* 2b s8: y velocity += s8 * 16 * (+82) / 4096 << 8. */
    case 0x2B:
        sprite->speed_y += (((s8)args[0] << 4) * (s16)sprite->word82 / 4096) << 8;
        break;
    /* 1f: render bit 26 off; rest on the stage floor (800ba8f4). */
    case 0x1F:
        sprite->render.word &= ~0x04000000;
        battle_sprite_update_ground(sprite);
        break;
    /* 3f: render bit 26 on. */
    case 0x3F:
        sprite->render.word |= 0x04000000;
        break;
    /* 3b: render bit 29 on (drawn at the creator's depth). */
    case 0x3B:
        sprite->render.word |= 0x20000000;
        break;
    /* 3c: render bit 29 off. */
    case 0x3C:
        sprite->render.word &= ~0x20000000;
        break;
    /* 35: render bit 27 on. */
    case 0x35:
        sprite->render.word |= 0x08000000;
        break;
    /* 36: render bit 27 off. */
    case 0x36:
        sprite->render.word &= ~0x08000000;
        break;
    /* 23: face the target point (+a0) (800b6518). */
    case 0x23:
        battle_sprite_command_face_target_point(sprite, args);
        break;
    /* 40 u8: turn the velocity towards the target point (+a0) by at most u8 * 4 in yaw and
     * in pitch (800b65b0). */
    case 0x40:
        battle_sprite_command_steer_velocity(sprite, args);
        break;
    /* 1c: velocity = the walking speed towards the target point (+a0); face that way
     * (800b6808). */
    case 0x1C:
        battle_sprite_command_aim_velocity(sprite, args);
        break;
    /* 1b: take the parent's velocity (800b6a50). */
    case 0x1B:
        battle_sprite_command_take_parent_velocity(sprite, args);
        break;
    /* 1a: destroy the tasks the sprite's task created (8001ce74). */
    case 0x1A:
        task_destroy_owned_by(sprite->block);
        break;
    /* 2d s16: position = the three s16 at the arguments + s16 (800b6930). */
    case 0x2D:
        battle_sprite_command_set_position(sprite, args);
        break;
    /* 2e s16: target = the three s16 at the arguments + s16 (800b6990). */
    case 0x2E:
        battle_sprite_set_script_vector(&sprite->target_x, args);
        break;
    /* 5b s16: view vector +44 = the three s16 at the arguments + s16. */
    case 0x5B:
        battle_sprite_set_script_vector(&sprite->renderer->light_angles.vx, args);
        break;
    /* 5c s16: view vector +4c = the three s16 at the arguments + s16. */
    case 0x5C:
        battle_sprite_set_script_vector((s16 *)sprite->renderer->light_colour, args);
        break;
    /* 5d s16: view vector +44 += the three s16 at the arguments + s16 (800b69e4). */
    case 0x5D:
        battle_sprite_add_script_vector(&sprite->renderer->light_angles.vx, args);
        break;
    /* 5e s16: view vector +4c += the three s16 at the arguments + s16. */
    case 0x5E:
        battle_sprite_add_script_vector((s16 *)sprite->renderer->light_colour, args);
        break;
    /* 1d s16: break the image into pieces with the six bytes at the arguments + s16
     * (800b6a7c). */
    case 0x1D:
        battle_sprite_command_break_image(sprite, args);
        break;
    /* 19 s16: start a burst with the six bytes at the arguments + s16 (800b6b98). */
    case 0x19:
        battle_sprite_command_spin(sprite, args);
        break;
    /* 18: copy the screen and shatter it (800b6bfc). */
    case 0x18:
        battle_sprite_command_shatter_screen(sprite, args);
        break;
    /* 17 s8: velocity *= s8 * 4 / 256. */
    case 0x17:
        scale = (s8)args[0] << 2;
        sprite->speed_x = sprite->speed_x * scale / 256;
        sprite->speed_y = sprite->speed_y * scale / 256;
        sprite->speed_z = sprite->speed_z * scale / 256;
        break;
    /* 16: render bit 25 (drawn at the back). */
    case 0x16:
        sprite->render.word |= 0x02000000;
        break;
    /* 15: render bit 24 (its own placement). */
    case 0x15:
        sprite->render.word |= 0x01000000;
        break;
    /* 11: pause the battle sprites (800c3664 = 1): their task update 800bac50 and draw
     * 800bab0c skip them, and resident sprites with passive children are not drawn
     * (80025258). */
    case 0x11:
        battle_sprites_paused = 1;
        break;
    /* 12: 800c372c = 1; both draw buffers clear to black (the first's colour and flag
     * saved). */
    case 0x12:
        battle_stage_drawing_off = 1;
        isbg = BATTLE_AREA.buffers[0].drawEnv.isbg;
        r = BATTLE_AREA.buffers[0].drawEnv.r0;
        g = BATTLE_AREA.buffers[0].drawEnv.g0;
        b = BATTLE_AREA.buffers[0].drawEnv.b0;
        BATTLE_AREA.buffers[1].drawEnv.isbg = 1;
        BATTLE_AREA.buffers[0].drawEnv.isbg = 1;
        BATTLE_AREA.buffers[0].drawEnv.r0 = 0;
        BATTLE_AREA.buffers[1].drawEnv.r0 = 0;
        BATTLE_AREA.buffers[0].drawEnv.g0 = 0;
        BATTLE_AREA.buffers[1].drawEnv.g0 = 0;
        BATTLE_AREA.buffers[0].drawEnv.b0 = 0;
        BATTLE_AREA.buffers[1].drawEnv.b0 = 0;
        battle_saved_background_flag = isbg;
        battle_saved_background_color[0] = r;
        battle_saved_background_color[1] = g;
        battle_saved_background_color[2] = b;
        break;
    /* 13: resume them (800c3664 = 0). */
    case 0x13:
        battle_sprites_paused = 0;
        break;
    /* 14: 800c372c = 0; restore the saved clear colour and flag. */
    case 0x14:
        battle_stage_drawing_off = 0;
        BATTLE_AREA.buffers[1].drawEnv.isbg = battle_saved_background_flag;
        BATTLE_AREA.buffers[0].drawEnv.isbg = battle_saved_background_flag;
        BATTLE_AREA.buffers[0].drawEnv.r0 = battle_saved_background_color[0];
        BATTLE_AREA.buffers[1].drawEnv.r0 = battle_saved_background_color[0];
        BATTLE_AREA.buffers[0].drawEnv.g0 = battle_saved_background_color[1];
        BATTLE_AREA.buffers[1].drawEnv.g0 = battle_saved_background_color[1];
        BATTLE_AREA.buffers[0].drawEnv.b0 = battle_saved_background_color[2];
        BATTLE_AREA.buffers[1].drawEnv.b0 = battle_saved_background_color[2];
        break;
    /* 10: position = the camera look-at point (800d335c). */
    case 0x10:
        sprite->x = battle_camera_view_target.vx << 16;
        sprite->y = battle_camera_view_target.vy << 16;
        sprite->z = battle_camera_view_target.vz << 16;
        break;
    /* 53: mirror and flip bits off. */
    case 0x53:
        sprite->motion.word &= ~8;
        sprite->motion.word &= ~4;
        sprite->render.word &= ~8;
        sprite->render.word &= ~0x10;
        break;
    /* 0f: mirror and flip bits off, direction 0. */
    case 0xF:
        sprite->motion.word &= ~8;
        sprite->motion.word &= ~4;
        sprite->render.word &= ~8;
        sprite->render.word &= ~0x10;
        sprite_set_direction(sprite, 0);
        break;
    /* 0e s8: direction = s8 * 16. */
    case 0xE:
        sprite_set_direction(sprite, (s8)args[0] * 16);
        break;
    /* 0d s8: turn the velocity about z by s8 * 16 (800b6e84). */
    case 0xD:
        battle_sprite_command_turn_velocity(sprite, args);
        break;
    /* 0a: view z angle from the velocity's x-y direction (800b6c44). */
    case 0xA:
        battle_sprite_command_roll_to_velocity(sprite, args);
        break;
    /* 0b: view x angle from the velocity's x-z direction (800b6c98). */
    case 0xB:
        battle_sprite_command_pitch_to_velocity(sprite, args);
        break;
    /* 49 s8: view x angle += s8, render bit 28. */
    case 0x49:
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_x += (s8)args[0];
            sprite->render.word |= 0x10000000;
        }
        break;
    /* 4a s8: view y angle += s8, render bit 28. */
    case 0x4A:
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_y += (s8)args[0];
            sprite->render.word |= 0x10000000;
        }
        break;
    /* 4b s8: view z angle += s8, render bit 28. */
    case 0x4B:
        if (sprite->renderer != NULL) {
            sprite->renderer->angle_z += (s8)args[0];
            sprite->render.word |= 0x10000000;
        }
        break;
    /* 0c: view angles from the velocity's direction (800b6cec). */
    case 0xC:
        battle_sprite_command_face_velocity(sprite, args);
        break;
    /* 09: direction += 0x800 (half a turn) and the velocity negated. */
    case 0x9:
        sprite->direction += 0x800;
        sprite->speed_x = -sprite->speed_x;
        sprite->speed_y = -sprite->speed_y;
        sprite->speed_z = -sprite->speed_z;
        break;
    /* 01 var: a trail in the colours at the variable (a count first, 0: 4) (800b572c). */
    case 0x1:
        sprite_alloc_group_entries(sprite);
        battle_trail_create(sprite, sprite_vm_resolve_variable(sprite, args));
        break;
    /* 24: the loaded battle module's 801fc7b0 (ovl3385: hold the sprite in place under its
     * effect). */
    case 0x24:
        battle_module_hold_start(sprite, args);
        break;
    /* 25: the loaded battle module's 801fc6fc (ovl3386: hold the sprite where it is under
     * the scrolling effect). */
    case 0x25:
        battle_module_scroll_start(sprite, args);
        break;
    /* 02: end the trail (the child task running 800b5588). */
    case 0x2:
        task = task_find_owned_with_update(sprite->block, battle_trail_update);
        if (task != NULL) {
            task->destroy(task);
        }
        break;
    /* 03 u8: link the partner at anchors u8: low nibble its own, high the partner's
     * (800b5c18). */
    case 0x3:
        battle_link_create(sprite, args);
        break;
    /* 04: end every link (the tasks running 800b5b3c). */
    case 0x4:
        while ((task = task_find_by_update(battle_link_update)) != NULL) {
            task->destroy(task);
        }
        break;
    /* 1e: draw as a line in its colour to the parent (800b61b0: frame 1, primitive code
     * 0x40). */
    case 0x1E:
        battle_parent_line_start(sprite, args);
        break;
    /* 20: start an orbit (800b5dc4). */
    case 0x20:
        battle_orbit_start(sprite);
        break;
    /* 05: draw as a streak, a line in its colour back along the velocity (800b5fbc: frame
     * 1, primitive code 0x40). */
    case 0x5:
        battle_streak_start(sprite, args);
        break;
    /* 06: camera move to the acting sprite's slot (800bc404). */
    case 0x6:
        battle_camera_start_move(1 << SPRITE_SLOT(battle_acting_sprite));
        break;
    /* 55: camera move to the acting sprite's slot and the slots 800d3634. */
    case 0x55:
        battle_camera_start_move((1 << SPRITE_SLOT(battle_acting_sprite)) | battle_area_event_target_mask);
        break;
    /* 07: camera move to the partner's slot. */
    case 0x7:
        battle_camera_start_move(1 << SPRITE_SLOT(sprite->partner));
        break;
    /* 65: camera move to the slots 800d3634. */
    case 0x65:
        battle_camera_start_move(battle_area_event_target_mask);
        break;
    /* 41: camera move to the partner's and the acting sprite's slots. */
    case 0x41: {
        Sprite **active = &battle_acting_sprite;
        s32 a = SPRITE_SLOT(sprite->partner);
        battle_camera_start_move((1 << a) | (1 << SPRITE_SLOT(*active)));
        break;
    }
    /* 08: flags bit 19; clear the eight anchors (800b6dc0). */
    case 0x8:
        sprite->flags |= 0x80000;
        battle_sprite_command_clear_anchors(sprite, args);
        break;
    }
}

/* 800B4EDC: Copy the sprite's current part's 8 x 8 texture block and its 16-colour
 * CLUT row to VRAM (0x3F0, 0x1F0) and (0x3F0, 0x1EE). */
void battle_sprite_command_copy_part_texture(Sprite *sprite) {
    SpritePart *part = sprite->renderer->parts[1];
    RECT rect;
    u16 tpage;
    u16 clut;

    tpage = part->tpage;
    rect.x = (part->u >> 2) + ((tpage & 0xF) << 6);
    rect.y = part->v + ((tpage << 4) & 0x100);
    rect.w = 8;
    rect.h = 8;
    MoveImage(&rect, 0x3F0, 0x1F0);
    clut = part->clut;
    rect.w = 16;
    rect.h = 1;
    rect.x = clut & 0x3F;
    rect.y = (clut >> 6) & 0x1FF;
    MoveImage(&rect, 0x3F0, 0x1EE);
}

/* 800B4F88: The matrix of the sprite's anchor index: its angles, at the anchor's
 * offset (mirrored with the sprite, scaled) from the sprite's position, in
 * the sprite's screen matrix. */
void battle_sprite_get_anchor_matrix(Sprite *sprite, s32 index, MATRIX *m) {
    SpriteRendererEntry *anchor;
    s32 x;
    s32 y;
    SVECTOR angles;

    if (sprite->renderer != NULL) {
        anchor = (SpriteRendererEntry *)(index * sizeof(SpriteRendererEntry) + (s32)sprite->renderer->pointer34);
        y = anchor->byte1;
        x = anchor->byte0;
        if ((sprite->motion.word >> 2) & 1) {
            x = -x;
        }
        y = y * sprite->scale / 4096;
        x = x * sprite->scale / 4096;
        angles.vx = anchor->half2;
        angles.vy = sprite->renderer->pointer34[index].half4;
        angles.vz = sprite->renderer->pointer34[index].half6;
        gpu_build_rotation_matrix(&angles, m);
        m->t[0] = sprite->renderer->matrix.t[0] + x;
        m->t[1] = sprite->renderer->matrix.t[1] + y;
        m->t[2] = sprite->renderer->matrix.t[2];
        SetMulMatrix(m, &sprite->renderer->matrix);
    }
}

/* 800B50D4: The offsets of the sprite's five trail anchors (battle_trail_anchor_indices), mirrored with
 * the sprite and scaled, as points (x, y, 0) of out, when it is drawn one
 * sided. */
void battle_trail_get_anchors(Sprite *sprite, SVECTOR *out) {
    s32 i;
    s32 x;
    s32 y;

    if ((sprite->render.word & 3) == 1 && sprite->renderer != NULL && sprite->renderer->pointer34 != NULL) {
        for (i = 0; i != 5; i++, out++) {
            x = sprite->renderer->pointer34[battle_trail_anchor_indices[i]].byte0;
            y = sprite->renderer->pointer34[battle_trail_anchor_indices[i]].byte1;
            if ((sprite->motion.word >> 2) & 1) {
                x = -x;
            }
            x = x * sprite->scale / 8192;
            y = y * sprite->scale / 8192;
            out->vx = x;
            out->vy = y;
            out->vz = 0;
        }
    }
}

/* 800B51B0: Draw one segment of a sprite trail (800C08CC) from point a to point b: a
 * light line, and when on screen a quad as wide as the trail (battle_trail_color_count)
 * textured from the sprite's copied 8 x 8 block (800B4EDC), joined to the
 * previous segment's far corners; counts the segments in battle_curve_segments_drawn. */
void battle_trail_draw_segment(VECTOR *a, VECTOR *b) {
    SVECTOR from;
    SVECTOR to;
    long scratch;
    long flag;
    LINE_F2 *line;
    POLY_FT4 *quad;
    s32 abr;
    s32 depth;
    s32 width;
    s32 angle;
    s32 dx;
    s32 dy;

    if (a->vx == b->vx && a->vy == b->vy && a->vz == b->vz) {
        return;
    }
    from.vx = a->vx;
    from.vy = a->vy;
    from.vz = a->vz;
    to.vx = b->vx;
    to.vy = b->vy;
    to.vz = b->vz;
    if ((u8 *)sprite_queue_next_free + sizeof(LINE_F2) >= sprite_queue_block_end) {
        return;
    }
    line = (LINE_F2 *)sprite_queue_next_free;
    sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(LINE_F2));
    SetLineF2(line);
    abr = battle_trail_blend;
    if (abr != 0) {
        abr--;
        SetSemiTrans(line, 1);
    } else {
        SetSemiTrans(line, 0);
    }
    line->r0 = 0xF0;
    line->g0 = 0xF0;
    line->b0 = 0xF0;
    if (battle_trail_colors[battle_curve_segments_drawn / 8] != 0) {
        depth = 2;
    } else {
        depth = -2;
    }
    depth += battle_trail_depth;
    RotTransPers3(&from, &to, &from, (long *)&line->x0, (long *)&line->x1, &scratch, &scratch, &flag);
    if (depth <= 0 || depth >= 0x1000) {
        return;
    }
    dx = line->x1 - line->x0;
    dy = line->y1 - line->y0;
    width = battle_trail_color_count;
    angle = ratan2(dy, dx) + 0x400; /* across the line */
    dx = gpu_get_cos(angle) * width / 8192;
    dy = gpu_get_sin(angle) * width / 8192;
    if ((u8 *)sprite_queue_next_free + sizeof(POLY_FT4) >= sprite_queue_block_end) {
        return;
    }
    quad = (POLY_FT4 *)sprite_queue_next_free;
    sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(POLY_FT4));
    SetPolyFT4(quad);
    SetShadeTex(quad, 1);
    quad->u0 = 0xC0 + (battle_curve_segments_drawn & 7);
    quad->v0 = 0xF0;
    quad->u1 = 0xC1 + (battle_curve_segments_drawn & 7);
    quad->v1 = 0xF0;
    quad->u2 = 0xC0 + (battle_curve_segments_drawn & 7);
    quad->v2 = 0xF7;
    quad->u3 = 0xC1 + (battle_curve_segments_drawn & 7);
    quad->v3 = 0xF7;
    quad->tpage = GetTPage(0, abr, 0x3F0, 0x1F0);
    quad->clut = GetClut(0x3F0, 0x1EE);
    quad->r0 = 0xF0;
    quad->g0 = 0xF0;
    quad->b0 = 0xF0;
    if (battle_curve_segments_drawn == 0) {
        quad->x0 = line->x0 - dx;
        quad->y0 = line->y0 - dy;
        quad->x1 = line->x0 + dx;
        quad->y1 = line->y0 + dy;
    } else {
        quad->x0 = battle_trail_last_corner_x2;
        quad->y0 = battle_trail_last_corner_y2;
        quad->x1 = battle_trail_last_corner_x3;
        quad->y1 = battle_trail_last_corner_y3;
    }
    battle_trail_last_corner_x2 = quad->x2 = line->x1 - dx;
    battle_trail_last_corner_y2 = quad->y2 = line->y1 - dy;
    battle_trail_last_corner_x3 = quad->x3 = line->x1 + dx;
    battle_trail_last_corner_y3 = quad->y3 = line->y1 + dy;
    AddPrim((u32 *)sprite_ot + depth, quad);
    battle_curve_segments_drawn++;
}

/* 800B5588: Trail update: publish its colours and blend for drawing, refresh the
 * anchors when the sprite's frame changed and ease the trail towards them;
 * end once the sprite's motion changes or its phase is 0 or 1. */
void battle_trail_update(Task *task) {
    SpriteTrail *trail = task->data;
    Sprite *sprite;
    Sprite *current;
    s32 phase;
    s32 i;

    battle_trail_colors = trail->colours;
    sprite = trail->sprite;
    battle_trail_color_count = trail->count;
    battle_trail_blend = trail->blend;
    battle_trail_depth = sprite->depth;
    if (sprite->frame != trail->frame) {
        trail->frame = sprite->frame;
        battle_trail_get_anchors(sprite, trail->anchors);
    }
    for (i = 1; i != 5; i++) {
        trail->trail[i].vx += (trail->anchors[i].vx - trail->trail[i].vx) / 2;
        trail->trail[i].vy += (trail->anchors[i].vy - trail->trail[i].vy) / 2;
        trail->trail[i].vz += (trail->anchors[i].vz - trail->trail[i].vz) / 2;
    }
    trail->trail[0].vx = trail->anchors[0].vx;
    trail->trail[0].vy = trail->anchors[0].vy;
    trail->trail[0].vz = trail->anchors[0].vz;
    current = trail->sprite;
    if (trail->motion != (s8)current->motion.bytes[3] || (phase = (SPRITE_FRAME_WORD(current) >> 28) & 3) == 0 || phase == 1) {
        trail->task.destroy(&trail->task);
    }
}

/* 800B56E4: Trail draw: the sprite, then the trail (800C08CC). */
void battle_trail_draw(Task *draw) {
    SpriteTrail *trail = draw->data;

    sprite_set_draw_matrix(trail->sprite);
    battle_curve_draw(5, trail->trail, battle_trail_draw_segment);
}

/* 800B572C: Give sprite a trail in colours (the first byte the colour count, 0 for
 * 4). */
void battle_trail_create(Sprite *sprite, u8 *colours) {
    SpriteTrail *trail = (SpriteTrail *)task_alloc_two_node_task(0xB8, sprite->block, battle_trail_update, battle_trail_draw, NULL);

    trail->sprite = sprite;
    trail->frame = sprite->frame;
    trail->motion = (s8)sprite->motion.bytes[3];
    trail->colours = colours;
    trail->blend = ((u8 *)&sprite->render)[0] >> 5;
    if (colours[0] == 0) {
        trail->count = 4;
    } else {
        trail->count = colours[0];
    }
    battle_trail_get_anchors(sprite, trail->trail);
    battle_trail_get_anchors(sprite, trail->anchors);
}

/* 800B57E4: The distance from the sprite to its target. */
s32 battle_sprite_get_target_distance(Sprite *sprite) {
    VECTOR delta;
    VECTOR squares;

    delta.vx = sprite->target_x - FIXED_WHOLE(sprite->x);
    delta.vy = sprite->target_y - FIXED_WHOLE(sprite->y);
    delta.vz = sprite->target_z - FIXED_WHOLE(sprite->z);
    Square0(&delta, &squares);
    return SquareRoot0(squares.vx + squares.vz + squares.vy);
}

/* 800B5854: Approach watch update: resume the sprite at the given script on its next
 * tick once it passes its target or comes near it; end once redirected,
 * its countdown reaches zero, or its motion changes. */
void battle_approach_watch_update(Task *task) {
    SpriteApproach *approach = (SpriteApproach *)task;
    u8 done = 0;
    Sprite *sprite = approach->sprite;
    s32 last = approach->distance;
    s32 distance = battle_sprite_get_target_distance(sprite);
    u8 *resume;

    approach->distance = distance;
    if (last < distance || distance < approach->near) {
        done = 1;
        resume = approach->resume;
        sprite->countdown = 1;
        sprite->script = resume;
    }
    if (sprite->countdown == 0) {
        done = 1;
    }
    if ((s8)sprite->motion.bytes[3] != approach->motion) {
        done = 1;
    }
    if (done) {
        approach->task.destroy(&approach->task);
    }
}

/* 800B5924: Watch sprite approach its target (800B5854), resuming at the given script. */
SpriteApproach *battle_approach_watch_start(Sprite *sprite, s32 near, u8 *resume) {
    SpriteApproach *approach = (SpriteApproach *)task_alloc_main_task(sprite->block, sizeof(SpriteApproach) - sizeof(Task));

    task_set_update_callback(&approach->task, battle_approach_watch_update);
    approach->sprite = sprite;
    approach->distance = battle_sprite_get_target_distance(sprite);
    approach->near = near;
    approach->resume = resume;
    approach->motion = (s8)sprite->motion.bytes[3];
    sprite->motion.word |= 0x20;
    return approach;
}

/* 800B59BC: The offset of the sprite's anchor index (mirrored with the sprite,
 * scaled), when it is drawn one sided. */
DVECTOR battle_sprite_get_anchor_offset(Sprite *sprite, s32 index) {
    DVECTOR offset;

    if (sprite->renderer != NULL && (sprite->render.word & 3) == 1 && sprite->renderer->pointer34 != NULL) {
        offset.vy = sprite->renderer->pointer34[index].byte1;
        offset.vx = sprite->renderer->pointer34[index].byte0;
        if ((sprite->motion.word >> 2) & 1) {
            offset.vx = -offset.vx;
        }
        offset.vy = offset.vy * sprite->scale / 4096;
        offset.vx = offset.vx * sprite->scale / 4096;
        return offset;
    }
}

/* 800B5AC4: The screen position of the sprite's anchor index. */
DVECTOR battle_sprite_get_anchor_point(Sprite *sprite, s32 index) {
    DVECTOR point = battle_sprite_get_anchor_offset(sprite, index);

    point.vx += sprite->x >> 16;
    point.vy += sprite->y >> 16;
    return point;
}

/* 800B5B3C: Link update: move the partner so that its anchor meets the sprite's;
 * end once the sprite's motion changes or its phase is 0 or 1. */
void battle_link_update(SpriteLink *link) {
    Sprite *partner = link->sprite->partner;
    DVECTOR a = battle_sprite_get_anchor_point(link->sprite, link->anchor);
    DVECTOR b = battle_sprite_get_anchor_point(partner, link->partnerAnchor);
    DVECTOR delta;
    Sprite *sprite;
    s32 phase;

    delta.vx = a.vx - b.vx;
    delta.vy = a.vy - b.vy;
    partner->x += delta.vx << 16;
    partner->y += delta.vy << 16;
    sprite = link->sprite;
    if (link->motion != (s8)sprite->motion.bytes[3] || (phase = (SPRITE_FRAME_WORD(sprite) >> 28) & 3) == 0 || phase == 1) {
        link->task.destroy(&link->task);
    }
}

/* 800B5C18: Link sprite's partner to it at anchors (low nibble the sprite's, high
 * nibble the partner's). */
SpriteLink *battle_link_create(Sprite *sprite, u8 *anchors) {
    SpriteLink *link = (SpriteLink *)task_alloc_main_task(sprite->block, sizeof(SpriteLink) - sizeof(Task));

    task_set_update_callback(&link->task, (void (*)(Task *))battle_link_update);
    link->sprite = sprite;
    link->partner = sprite->partner;
    link->frame = sprite->frame;
    link->motion = (s8)sprite->motion.bytes[3];
    link->anchor = *anchors & 0xF;
    link->partnerAnchor = *anchors >> 4;
    battle_link_update(link);
    return link;
}

/* 800B5CC0: Sprite orbit update: place the sprite around its target by its speeds
 * (radius and angles); end with its frames. */
void battle_orbit_update(Task *task) {
    Sprite *sprite = task->data;
    MATRIX m;
    SVECTOR offset;
    SVECTOR angles;
    VECTOR position;

    sprite_vm_tick(sprite);
    angles.vy = sprite->speed_y >> 13;
    angles.vz = sprite->speed_z >> 13;
    angles.vx = 0;
    offset.vx = sprite_scale_by_rate(sprite, sprite->speed_x >> 13);
    offset.vy = 0;
    offset.vz = 0;
    gpu_build_rotation_matrix(&angles, &m);
    ApplyMatrix(&m, &offset, &position);
    position.vx += sprite->target_x;
    position.vy += sprite->target_y;
    position.vz += sprite->target_z;
    sprite->x = position.vx << 16;
    sprite->y = position.vy << 16;
    sprite->z = position.vz << 16;
    if (sprite->script == 0) {
        task->destroy(task);
    }
}

/* 800B5DC4: Start sprite's orbit (800B5CC0). */
void battle_orbit_start(Sprite *sprite) {
    sprite->frame = 1;
    task_set_update_callback(sprite->block, battle_orbit_update);
}

/* 800B5DF4: Draw the sprite as a streak: a line in its colour from its position back
 * along its velocity (scaled down by its size), blended by its render mode;
 * sets its depth. */
void battle_streak_draw(Task *draw) {
    Sprite *sprite = draw->data;
    u8 *cursor;
    LINE_F2 *line;
    DR_TPAGE *tpage;
    SVECTOR position;
    long p;
    s32 depth;
    s32 shift;
    u32 blend;

    if (sprite->frame != 0) {
        return;
    }
    cursor = (u8 *)sprite_queue_next_free;
    if (cursor + sizeof(LINE_F2) >= sprite_queue_block_end) {
        return;
    }
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    sprite_queue_next_free = (SpriteQueueEntry *)(cursor + sizeof(LINE_F2));
    position.vz = sprite->z >> 16;
    line = (LINE_F2 *)cursor;
    if (((u8 *)&sprite->render)[3] & 1) {
        SetRotMatrix(&battle_screen_space_matrix);
        SetTransMatrix(&battle_screen_space_matrix);
    } else {
        SetRotMatrix(&sprite_view_matrix);
        SetTransMatrix(&sprite_view_matrix);
    }
    depth = RotTransPers(&position, (long *)&line->x0, &p, &p) >> model_ot_depth_shift;
    shift = sprite->height + 8;
    sprite->depth = depth;
    position.vx -= sprite->speed_x >> shift;
    position.vy -= sprite->speed_y >> shift;
    position.vz -= sprite->speed_z >> shift;
    RotTransPers(&position, (long *)&line->x1, &p, &p);
    setlen(line, 3);
    *(u32 *)&line->r0 = *(u32 *)&sprite->red;
    AddPrim((u32 *)sprite_ot + depth, line);
    tpage = (DR_TPAGE *)sprite_queue_next_free;
    if ((u8 *)sprite_queue_next_free + sizeof(DR_TPAGE) < sprite_queue_block_end) {
        blend = ((u8 *)&sprite->render)[0] >> 5;
        if (blend != 0) {
            sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(DR_TPAGE));
            setlen(tpage, 1);
            tpage->code[0] = 0xE1000000 | (((blend - 1) & 3) << 5);
            AddPrim((u32 *)sprite_ot + depth, tpage);
        }
    }
}

/* 800B5FBC: Draw sprite with 800B5DF4: frame 1, its colour word's primitive code 0x40
 * (LINE_F2). */
void battle_streak_start(Sprite *sprite) {
    sprite->frame = 1;
    task_set_draw_callback(&((SpriteTask *)sprite->block)->auxiliary, battle_streak_draw);
    sprite->colour_flags = 0x40;
}

/* 800B6004: Draw the sprite as a line in its colour from it to its parent, blended by
 * its render mode; sets its depth. */
void battle_parent_line_draw(Task *draw) {
    Sprite *sprite = draw->data;
    Sprite *parent;
    LINE_F2 *line;
    DR_TPAGE *tpage;
    SVECTOR position;
    long p;
    s32 depth;
    u32 blend;

    if (sprite->frame != 0) {
        return;
    }
    line = (LINE_F2 *)sprite_queue_next_free;
    parent = sprite->parent;
    if ((u8 *)(line + 1) >= sprite_queue_block_end) {
        return;
    }
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    position.vz = sprite->z >> 16;
    sprite_queue_next_free = (SpriteQueueEntry *)(line + 1);
    SetRotMatrix(&sprite_view_matrix);
    SetTransMatrix(&sprite_view_matrix);
    depth = RotTransPers(&position, (long *)&line->x0, &p, &p) >> model_ot_depth_shift;
    sprite->depth = depth;
    position.vx = sprite->x >> 16;
    position.vy = sprite->y >> 16;
    position.vz = sprite->z >> 16;
    position.vx = parent->x >> 16;
    position.vy = parent->y >> 16;
    position.vz = parent->z >> 16;
    RotTransPers(&position, (long *)&line->x1, &p, &p);
    setlen(line, 3);
    *(u32 *)&line->r0 = *(u32 *)&sprite->red;
    AddPrim((u32 *)sprite_ot + depth, line);
    tpage = (DR_TPAGE *)sprite_queue_next_free;
    if ((u8 *)sprite_queue_next_free + sizeof(DR_TPAGE) < sprite_queue_block_end) {
        blend = ((u8 *)&sprite->render)[0] >> 5;
        if (blend != 0) {
            sprite_queue_next_free = (SpriteQueueEntry *)((u8 *)sprite_queue_next_free + sizeof(DR_TPAGE));
            setlen(tpage, 1);
            tpage->code[0] = 0xE1000000 | (((blend - 1) & 3) << 5);
            AddPrim((u32 *)sprite_ot + depth, tpage);
        }
    }
}

/* 800B61B0: Draw sprite with 800B6004: frame 1, its colour word's primitive code 0x40
 * (LINE_F2). */
void battle_parent_line_start(Sprite *sprite) {
    sprite->frame = 1;
    task_set_draw_callback(&((SpriteTask *)sprite->block)->auxiliary, battle_parent_line_draw);
    sprite->colour_flags = 0x40;
}

/* 800B61F8: Script command: reset battle_pending_hit_count and load sound bank set args[0]
 * (800A96B4, on a stack in a heap block). */
void battle_sprite_command_load_object_set(Sprite *sprite, u8 *args) {
    u8 *stack = heap_alloc(0x4000, 1);

    STACK_ENTER(stack + 0x3E00);
    battle_pending_hit_count = 0;
    battle_read_object_set_files(args[0]);
    STACK_LEAVE();
    heap_free(stack);
}

/* 800B626C: Script command: free stage object 11 (800A9FF0, on a stack in a heap
 * block). */
void battle_sprite_command_free_stage_object(void) {
    u8 *stack = heap_alloc(0x4000, 1);

    STACK_ENTER(stack + 0x3E00);
    battle_free_object(0xB);
    STACK_LEAVE();
    heap_free(stack);
}

/* 800B62C8: Script command: show stage object 11 in mode args[0] (800BEE2C); mode 2
 * first places it and shows it to the side of battle_acting_sprite only. */
void battle_sprite_command_show_stage_object(Sprite *sprite, u8 *args) {
    u8 *stack = heap_alloc(0x4000, 1);
    u32 frame_bits;

    STACK_ENTER(stack + 0x3E00);
    if (args[0] == 2) {
        battle_create_object_from_files(0xB, 0x300, 0x100, 0, 0x1DB);
        frame_bits = SPRITE_FRAME_WORD(battle_acting_sprite) >> 30;
        battle_start_object_script_on_own_stack(0xB, 1 << (((battle_acting_sprite->motion.word & 3) << 2) | frame_bits), args[0]);
    } else {
        battle_start_object_script_on_own_stack(0xB, battle_area_event_target_mask, args[0]);
    }
    STACK_LEAVE();
    heap_free(stack);
}

/* 800B639C: Script command: fade the lights (800B3CD4) with the parameters at the
 * relative offset in args. */
void battle_sprite_command_fade_lights(Sprite *sprite, u8 *args) {
    s8 *fade = (s8 *)SCRIPT_DATA(args);

    battle_light_fade_start((u8)fade[5], (u8)fade[3], (u8)fade[4], fade[0], fade[1], fade[2]);
}

/* 800B63F0: Script command: upload the images at the relative offset in args. */
void battle_sprite_command_upload_images(Sprite *sprite, u8 *args) {
    model_load_image_list(SCRIPT_DATA(args), 0, 0, 0, 0, 0, 0);
}

/* 800B6438: Script command: draw sprite with the resident sprite drawer (80025A88). */
void battle_sprite_command_draw_unlit_model(Sprite *sprite) {
    task_set_draw_callback(&((SpriteTask *)sprite->block)->auxiliary, sprite_task_draw_unlit_tmd);
}

/* 800B6464: Script command: move the CLUTs of the sprite's parts by (args[0],
 * args[1]). */
void battle_sprite_command_shift_cluts(Sprite *sprite, u8 *args) {
    SpritePart *part = sprite->renderer->parts[1];
    s16 count = ((SpriteFlagBits *)&sprite->flags)->part_bytes >> 2;
    s16 i = 0;
    s32 x;
    s32 y;

    if (count != 0) {
        do {
            i++;
            x = part->clut & 0x3F;
            y = (part->clut >> 6) & 0x1FF;
            x += args[0];
            y += args[1];
            part->clut = x | (y << 6);
            part++;
        } while (i != count);
    }
}

/* 800B64D4: Script command: set battle sprite args[1]'s value (800245D8) to args[0]. */
void battle_sprite_command_slot_animation(Sprite *sprite, u8 *args) {
    sprite_start_animation(BATTLE_AREA.sprites[args[1]], args[0]);
}

/* 800B6518: Script command: turn the sprite towards its target (on the x-z plane). */
void battle_sprite_command_face_target_point(Sprite *sprite) {
    GroundPoint position;
    GroundPoint target;
    s16 direction;

    position.x = sprite->x >> 16;
    position.z = sprite->z >> 16;
    target.x = sprite->target_x;
    target.z = sprite->target_z;
    direction = sprite_get_ground_direction(target, position);
    sprite_set_direction(sprite, direction);
    sprite_set_facing(sprite, direction);
}

/* 800B65B0: Script command: turn the sprite's speed towards its target by at most
 * args[0] * 4 (of 4096) in each angle, keeping its length. */
void battle_sprite_command_steer_velocity(Sprite *sprite, u8 *args) {
    SVECTOR want;
    SVECTOR angles;
    VECTOR delta;
    VECTOR squares;
    VECTOR velocity;
    SVECTOR length;
    MATRIX m;
    VECTOR speed;
    s16 distance;
    s16 speedLength;
    s32 step;
    s16 turn;
    s16 difference;
    SVECTOR *length_vector;

    delta.vx = sprite->target_x - FIXED_WHOLE(sprite->x);
    delta.vy = sprite->target_y - FIXED_WHOLE(sprite->y);
    delta.vz = sprite->target_z - FIXED_WHOLE(sprite->z);
    Square0(&delta, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    want.vy = -ratan2(delta.vz, delta.vx);
    want.vz = ratan2(delta.vy, distance);
    want.vx = 0;
    velocity.vx = sprite->speed_x >> 7;
    velocity.vy = sprite->speed_y >> 7;
    velocity.vz = sprite->speed_z >> 7;
    Square0(&velocity, &squares);
    speedLength = SquareRoot0(squares.vy + squares.vx + squares.vz);
    distance = SquareRoot0(squares.vx + squares.vz);
    angles.vy = -ratan2(velocity.vz, velocity.vx);
    angles.vz = ratan2(velocity.vy, distance);
    angles.vx = 0;
    step = args[0] * 4;
    difference = ((s32)((u16)want.vy - (u16)angles.vy) << 20) >> 20;
    turn = difference;
    if (step < abs(difference)) {
        turn = step;
        if (difference < 0) {
            turn = -step;
        }
    }
    angles.vy += turn;
    difference = ((s32)((u16)want.vz - (u16)angles.vz) << 20) >> 20;
    turn = difference;
    length_vector = &length;
    if (step < abs(difference)) {
        turn = step;
        if (difference < 0) {
            turn = -step;
        }
    }
    angles.vz += turn;
    sprite_set_svector(length_vector, speedLength, 0, 0);
    gpu_build_rotation_matrix(&angles, &m);
    ApplyMatrix(&m, length_vector, &speed);
    sprite->speed_x = speed.vx << 7;
    sprite->speed_y = speed.vy << 7;
    sprite->speed_z = speed.vz << 7;
}

/* 800B6808: Script command: aim the sprite's speed (its speed setting, field18) at
 * its target and face it that way. */
void battle_sprite_command_aim_velocity(Sprite *sprite) {
    SVECTOR angles;
    VECTOR delta;
    VECTOR squares;
    SVECTOR length;
    MATRIX m;
    VECTOR speed;
    s32 distance;

    delta.vx = sprite->target_x - FIXED_WHOLE(sprite->x);
    delta.vy = sprite->target_y - FIXED_WHOLE(sprite->y);
    delta.vz = sprite->target_z - FIXED_WHOLE(sprite->z);
    Square0(&delta, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    angles.vy = -ratan2(delta.vz, delta.vx);
    angles.vz = ratan2(delta.vy, distance);
    angles.vx = 0;
    sprite->direction = angles.vy;
    sprite_set_svector(&length, (sprite->speed << 9) >> 16, 0, 0);
    gpu_build_rotation_matrix(&angles, &m);
    ApplyMatrix(&m, &length, &speed);
    sprite->speed_x = speed.vx << 7;
    sprite->speed_y = speed.vy << 7;
    sprite->speed_z = speed.vz << 7;
}

/* 800B6930: Script command: set a 16.16 point to the script's three s16s. */
void battle_sprite_command_set_position(s32 *point, u8 *args) {
    u8 *data = SCRIPT_DATA(args);

    point[0] = SCRIPT_S16(data, 0) << 16;
    point[1] = SCRIPT_S16(data, 2) << 16;
    point[2] = SCRIPT_S16(data, 4) << 16;
}

/* 800B6990: Script command: set a vector to the script's three s16s. */
void battle_sprite_set_script_vector(s16 *vector, u8 *args) {
    u8 *data = SCRIPT_DATA(args);
    s32 value;

    value = SCRIPT_S16(data, 0);
    vector[0] = value;
    value = SCRIPT_S16(data, 2);
    vector[1] = value;
    value = SCRIPT_S16(data, 4);
    vector[2] = value;
}

/* 800B69E4: Script command: add the script's three s16s to a vector. */
void battle_sprite_add_script_vector(s16 *vector, u8 *args) {
    u8 *data = SCRIPT_DATA(args);
    s32 value;

    value = SCRIPT_S16(data, 0);
    vector[0] += value;
    value = SCRIPT_S16(data, 2);
    vector[1] += value;
    value = SCRIPT_S16(data, 4);
    vector[2] += value;
}

/* 800B6A50: Script command: take the speed of the sprite's parent. */
void battle_sprite_command_take_parent_velocity(Sprite *sprite) {
    Sprite *parent = sprite->parent;

    sprite->speed_x = parent->speed_x;
    sprite->speed_y = parent->speed_y;
    sprite->speed_z = parent->speed_z;
}

/* 800B6A7C: Script command: break the sprite's image into pieces (801FC4C4) with the
 * script's parameters (scaled with the sprite when field3A is set), leaving
 * it without an image. */
void battle_sprite_command_break_image(Sprite *sprite, u8 *args) {
    u8 *data = SCRIPT_DATA(args);
    s32 a;
    s32 b;
    s32 c;
    SpriteRenderer *view;

    if (sprite->rate != 0) {
        a = sprite_scale_by_rate(sprite, (((s8 *)data)[1] << 3) | data[0]) << 8;
        b = sprite_scale_by_rate(sprite, data[2]) << 16;
        c = sprite_scale_by_rate(sprite, data[3]) << 16;
    } else {
        a = ((((s8 *)data)[1] << 3) | data[0]) << 5;
        b = data[2] << 13;
        c = data[3] << 13;
    }
    view = sprite->renderer;
    battle_module_debris_start(view->pointer34, view->parts[0], &view->matrix, a, b, c, data[4] * 16, data[5] * 4);
    sprite->renderer->pointer34 = NULL;
    sprite->renderer->parts[0] = NULL;
    sprite->renderer->parts[1] = NULL;
}

/* 800B6B98: Script command: start a burst from the sprite (801FC53C) with the
 * script's parameters. */
void battle_sprite_command_spin(Sprite *sprite, u8 *args) {
    u8 *data = SCRIPT_DATA(args);

    battle_module_spin_start(sprite, data[0] * 16, data[1], ((s8 *)data)[2] * 8, data[3] * 8, ((s8 *)data)[4] * 8, data[5]);
}

/* 800B6BFC: Script command: copy the screen to VRAM (0x2C0, 0x100) and shatter it
 * (800B73A0). */
void battle_sprite_command_shatter_screen(void) {
    RECT rect;

    rect.w = 320;
    rect.x = 0;
    rect.y = 0;
    rect.h = 224;
    MoveImage(&rect, 0x2C0, 0x100);
    battle_shatter_start();
}

/* 800B6C44: Script command: turn the sprite about z to its speed's direction in x-y. */
void battle_sprite_command_roll_to_velocity(Sprite *sprite) {
    sprite->renderer->angle_z = ratan2(sprite->speed_y >> 8, sprite->speed_x >> 8);
    sprite->render.word |= 0x10000000;
}

/* 800B6C98: Script command: turn the sprite about x to its speed's direction in x-z. */
void battle_sprite_command_pitch_to_velocity(Sprite *sprite) {
    sprite->renderer->angle_x = ratan2(sprite->speed_z >> 8, sprite->speed_x >> 8);
    sprite->render.word |= 0x10000000;
}

/* 800B6CEC: Script command: turn the sprite to its speed's direction. */
void battle_sprite_command_face_velocity(Sprite *sprite) {
    VECTOR speed;
    VECTOR squares;
    s32 distance;

    speed.vx = sprite->speed_x >> 8;
    speed.vy = sprite->speed_y >> 8;
    speed.vz = sprite->speed_z >> 8;
    if (speed.vz == 0) {
        speed.vz = 4;
    }
    Square0(&speed, &squares);
    distance = SquareRoot0(squares.vx + squares.vz);
    sprite->renderer->angle_y = -ratan2(speed.vz, speed.vx);
    sprite->renderer->angle_z = ratan2(speed.vy, distance);
    sprite->renderer->angle_x = 0;
    sprite->render.word |= 0x10000000;
}

/* 800B6DC0: Script command: clear the sprite's eight anchors. */
void battle_sprite_command_clear_anchors(Sprite *sprite) {
    s32 i;

    if (sprite->renderer != NULL && sprite->renderer->pointer34 != NULL) {
        for (i = 0; i != 8; i++) {
            sprite->renderer->pointer34[i].byte0 = 0;
            sprite->renderer->pointer34[i].byte1 = 0;
            sprite->renderer->pointer34[i].half2 = 0;
            sprite->renderer->pointer34[i].half4 = 0;
            sprite->renderer->pointer34[i].half6 = 0;
        }
        sprite->renderer->offset.x = 0;
        sprite->renderer->offset.y = 0;
    }
}

/* 800B6E84: Script command: turn the sprite's speed about z by args[0] * 16. */
void battle_sprite_command_turn_velocity(Sprite *sprite, s8 *args) {
    SVECTOR angles;
    MATRIX m;
    VECTOR velocity;

    sprite_set_svector(&angles, 0, 0, args[0] * 16);
    gpu_build_rotation_matrix(&angles, &m);
    ApplyMatrixLV(&m, (VECTOR *)&sprite->speed_x, &velocity);
    sprite->speed_x = velocity.vx;
    sprite->speed_y = velocity.vy;
    sprite->speed_z = velocity.vz;
}

/* 800B6F0C: Shatter update: after its delay each shard fades, moves, turns and
 * falls, its velocity easing out. */
void battle_shatter_update(Task *task) {
    ScreenShatter *shatter = task->data;
    s32 layer;
    s32 row;
    s32 column;
    ScreenShard *shard;
    POLY_FT3 *poly;
    SVECTOR step;

    shatter->frame++;
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                if (shard->delay != 0) {
                    shard->delay--;
                } else {
                    poly = &shard->poly[BATTLE_AREA.buffer];
                    poly->r0 = sprite_add_clamp_byte(poly->r0, -6);
                    poly->g0 = sprite_add_clamp_byte(poly->g0, -6);
                    poly->b0 = sprite_add_clamp_byte(poly->b0, -6);
                    step.vx = shard->velocity.vx >> 16;
                    step.vy = shard->velocity.vy >> 16;
                    step.vz = shard->velocity.vz >> 16;
                    shard->position.vx += step.vx;
                    shard->position.vy += step.vy;
                    shard->position.vz += step.vz;
                    shard->angles.vx += shard->spin.vx;
                    shard->angles.vy += shard->spin.vy;
                    shard->angles.vz += shard->spin.vz;
                    shard->velocity.vy -= shard->velocity.vy / 16;
                    shard->velocity.vx -= shard->velocity.vx / 16;
                    shard->velocity.vz -= shard->velocity.vz / 16;
                    shard->velocity.vy += shard->fall;
                }
            }
        }
    }
}
