/* arena_camera_and_scenes: text 800707A8-800732CC, rodata 8006FAF4-8006FBF8, data
 * 80090F38-800910F4, variables 800925D4-80092638 and 80092954-80092A24.
 * The camera's eye, look-at point and view modes; the scene script
 * interpreter and its scenes (the opening, the scene list, the bout-end
 * sequence, the winner screen); then the handwritten ground-map triangles
 * and GTE helpers (80072D18-800732CC, the .s files beside it). Its jump
 * tables lie at 4 mod 8 (8006FAF4-8006FB9C) and the next unit's at 0 mod 8;
 * its first function already reads its variables (the overlay number's
 * unit, menu.c, has no code), and it ends with the handwritten block. */
#include "common.h"
#include "psyq/inline_c.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/window.h"
#include "actor.h"
#include "bout.h"
#include "brain.h"
#include "camera.h"
#include "display.h"
#include "effects.h"
#include "glow.h"
#include "helpers.h"
#include "menus.h"
#include "mode.h"
#include "node.h"
#include "resident_views.h"
#include "script.h"
#include "select.h"
#include "sound.h"
#include "stage.h"
#include "task.h"
#include "text.h"

/* The unit's small uninitialized variables, zero in the file after every
 * unit's data, each in a slot of whole words (decomp/Makefile). */
static s32 arena_scene_message_number; /* 800925D4: message the scene shows */
static s32 arena_scene_shown_message_number; /* 800925D8 */
static s32 arena_scene_unread_word; /* 800925DC */
static s32 arena_scene_marker_x; /* 800925E0: marker sprite x, y */
static s32 arena_scene_marker_y; /* 800925E4 */
static s32 arena_scene_marker_target_x; /* 800925E8: marker sprite target x, y */
static s32 arena_scene_marker_target_y; /* 800925EC */
static u8 arena_scene_marker_shown; /* 800925F0: marker sprite shown */
static s32 arena_camera_view_lift; /* 800925F4: vertical camera lift of the current view */
static u8 *arena_scene_script_pc; /* 800925F8: running scene script */
static s32 arena_scene_wait_timer; /* 800925FC: scene script frame counter */
static s16 arena_scene_charge_hold; /* 80092600: scene script charge hold */
static s8 arena_scene_choice_cursor; /* 80092604: scene choice cursor */
static u8 arena_scene_choice_open; /* 80092608 */
static s16 arena_scene_knockdown_flash_level; /* 8009260C: knock-down flash level */
static Node *arena_winner_shadow_node; /* 80092610: the scene's root node */
static Actor *arena_winner_actor; /* 80092614 */
static s8 arena_winner_record_toggle; /* 80092618: odd: show the record text */
static s32 arena_winner_pitch; /* 8009261C */
static s32 arena_winner_turn_speed; /* 80092620 */
static s32 arena_winner_offset_y; /* 80092624 */
static s32 arena_winner_offset_z; /* 80092628 */
static s32 arena_winner_scale; /* 8009262C */
static SVECTOR arena_winner_angles; /* 80092630: model view angles */

/* Its larger ones, past the program's end (not in the file), each unit's
 * after every unit's small ones (menu.mk). */
static Window arena_scene_choice_window; /* 80092954: the opening text, then the scene list */
static DR_TPAGE arena_scene_marker_tpages[2]; /* 800929E4 */
static VECTOR arena_scene_bout_end_embers[3]; /* 800929F4: sparking embers; pad counts down to the next spark */

/* Scene scripts, user-supplied bytecode run by arena_scene_run_script (an asset in
 * menu.classification.txt; tools/analysis/overlay_scripts.py decodes them).
 * The opening's is empty; arena_scene_unreferenced_script has the same form, but nothing starts
 * it. */
extern u8 arena_scene_script_9[], arena_scene_script_1[], arena_scene_script_2[], arena_scene_script_0[], arena_scene_script_5[];
extern u8 arena_scene_script_3[], arena_scene_script_4[], arena_scene_script_6[], arena_scene_script_7[], arena_scene_script_8[];
INCLUDE_ASSET(".data", arena_scene_opening_script, 0x80090F38, 0x1);
INCLUDE_ASSET(".data", arena_scene_script_9, 0x80090F3C, 0x4);
INCLUDE_ASSET(".data", arena_scene_script_1, 0x80090F40, 0x4);
INCLUDE_ASSET(".data", arena_scene_script_2, 0x80090F44, 0x25);
INCLUDE_ASSET(".data", arena_scene_script_0, 0x80090F6C, 0x4);
INCLUDE_ASSET(".data", arena_scene_script_5, 0x80090F70, 0x39);
INCLUDE_ASSET(".data", arena_scene_script_3, 0x80090FAC, 0x31);
INCLUDE_ASSET(".data", arena_scene_script_4, 0x80090FE0, 0x2D);
INCLUDE_ASSET(".data", arena_scene_script_6, 0x80091010, 0x2A);
INCLUDE_ASSET(".data", arena_scene_script_7, 0x8009103C, 0x11);
INCLUDE_ASSET(".data", arena_scene_unreferenced_script, 0x80091050, 0x4);
INCLUDE_ASSET(".data", arena_scene_script_8, 0x80091054, 0x7);

/* Scene scripts by scene number. */
u8 *arena_scene_scripts[] = { /* 8009105C */
    arena_scene_script_0, arena_scene_script_1, arena_scene_script_2, arena_scene_script_3, arena_scene_script_4,
    arena_scene_script_5, arena_scene_script_6, arena_scene_script_7, arena_scene_script_8, arena_scene_script_9,
};

FloorStep arena_scene_floor_steps[8] = { /* 80091084 */
    { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 },
    { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 },
};

/* Script of the one-time scene setup (arena_scene_start_bout_end), user-supplied like
 * the scene scripts. */
INCLUDE_ASSET(".data", arena_scene_bout_end_script, 0x800910C4, 0x2C);

LightRig *arena_winner_light_rig = NULL; /* 800910F0 */

/* 800707A8: Start the menu camera: mode 3 setup and its script block. */
void arena_scene_close_choice(void) {
    arena_mode_set_state(3);
    window_close(&arena_scene_choice_window);
}

/* 800707D8: One easing step from current toward target: the remaining distance
 * (rounded away from zero) divided by the number of steps. */
s32 arena_camera_ease_step(s32 target, s32 current, s32 steps) {
    s32 delta = target - current;

    if (delta < 0) {
        delta++;
        delta -= steps;
    } else {
        delta--;
        delta += steps;
    }
    return delta / steps;
}

/* 80070808: Ease the camera's focus (its look-at point) toward target over the given
 * number of steps; its height is compared including the current lift. */
void arena_camera_ease_focus(VECTOR *target, s32 steps) {
    arena_camera_focus.vx += arena_camera_ease_step(target->vx, arena_camera_focus.vx, steps);
    arena_camera_focus.vz += arena_camera_ease_step(target->vz, arena_camera_focus.vz, steps);
    arena_camera_focus.vy += arena_camera_ease_step(target->vy, arena_camera_focus.vy + arena_camera_view_lift, steps);
}

/* 800708C4: Ease the camera's position (its eye) toward target, limited by the
 * collision step check. */
void arena_camera_ease_position(VECTOR *target, s32 steps) {
    VECTOR step;

    step.vx = arena_camera_ease_step(target->vx, arena_camera_position.vx, steps);
    step.vy = arena_camera_ease_step(target->vy, arena_camera_position.vy, steps);
    step.vz = arena_camera_ease_step(target->vz, arena_camera_position.vz, steps);
    arena_stage_keep_step_inside(&arena_camera_position, &step, 0x3D00);
    arena_camera_position.vx += step.vx;
    arena_camera_position.vy += step.vy;
    arena_camera_position.vz += step.vz;
}

/* 8007099C: Place the menu camera for one of the view modes. */
void arena_camera_apply_view_mode(u32 mode) {
    VECTOR target;
    s32 top;

    switch (mode) {
    case 0:
        arena_camera_frame_actors_for_scene(&arena_first_actor, &arena_second_actor);
        break;
    case 1:
        target = arena_first_actor.pos;
        arena_camera_view_lift = 0;
        target.vy -= 0xA0;
        arena_camera_ease_focus(&target, 4);
        break;
    case 2:
        target = arena_second_actor.pos;
        arena_camera_view_lift = 0;
        target.vy -= 0xA0;
        arena_camera_ease_focus(&target, 4);
        break;
    case 3:
        arena_camera_view_lift = 0xA0;
        goto lifted;
    case 4:
        arena_camera_view_lift = 0x80;
    lifted:
        target = arena_first_actor.core;
        target.vy += arena_camera_view_lift;
        arena_camera_ease_focus(&target, 0x10);
        target.vx = arena_first_actor.pos.vx + ((gpu_get_sin(arena_first_actor.angle + 0xA80) * 0xD0) >> 12);
        target.vy = arena_first_actor.pos.vy - (arena_camera_view_lift + 0x20);
        target.vz = arena_first_actor.pos.vz + ((gpu_get_cos(arena_first_actor.angle + 0xA80) * 0xD0) >> 12);
        arena_camera_ease_position(&target, 0x46);
        top = arena_stage_get_ground_height(&arena_camera_position, 0) - (arena_camera_view_lift + 0x40);
        if (top < arena_camera_position.vy) {
            arena_camera_position.vy = top;
        }
        break;
    case 5:
        arena_camera_focus.vx = arena_second_actor.pos.vx;
        arena_camera_focus.vy = arena_second_actor.pos.vy - 0xC0;
        arena_camera_focus.vz = arena_second_actor.pos.vz;
        arena_camera_position.vx = arena_camera_focus.vx + ((gpu_get_sin(arena_second_actor.angle + 0x900) * 0xE0) >> 12);
        arena_camera_position.vy = arena_second_actor.pos.vy - 0xD0;
        arena_camera_position.vz = arena_camera_focus.vz + ((gpu_get_cos(arena_second_actor.angle + 0x900) * 0xE0) >> 12);
        break;
    }
}

/* 80070C7C: Re-centre the two actors and the camera's position on a fixed scene
 * spot: the midpoint of the actors moves to the layout's anchor, actors on
 * the floor and the camera at a fixed height. */
void arena_scene_apply_layout(s32 layout) {
    VECTOR first = arena_first_actor.pos;
    VECTOR second = arena_second_actor.pos;
    VECTOR eye = arena_camera_position;
    VECTOR centre = first;

    centre.vx += second.vx;
    centre.vy += second.vy;
    centre.vz += second.vz;
    centre.vx /= 2;
    centre.vy /= 2;
    centre.vz /= 2;
    first.vx -= centre.vx;
    first.vy -= centre.vy;
    first.vz -= centre.vz;
    second.vx -= centre.vx;
    second.vy -= centre.vy;
    second.vz -= centre.vz;
    eye.vx -= centre.vx;
    eye.vy -= centre.vy;
    eye.vz -= centre.vz;
    switch (layout) {
    case 0:
        centre.vx = 0x4000;
        centre.vy = 0;
        centre.vz = 0x4000;
        break;
    case 1:
        centre.vx = 0x6000;
        centre.vy = 0;
        centre.vz = 0x6000;
        break;
    case 2:
        centre.vx = 0x2000;
        centre.vy = 0;
        centre.vz = 0x2000;
        break;
    case 3:
        centre.vx = 0x4000;
        centre.vy = 0;
        centre.vz = 0x2400;
        break;
    }
    first.vx += centre.vx;
    first.vy += centre.vy;
    first.vz += centre.vz;
    second.vx += centre.vx;
    second.vy += centre.vy;
    second.vz += centre.vz;
    eye.vx += centre.vx;
    eye.vy += centre.vy;
    eye.vz += centre.vz;
    first.vy = 0;
    second.vy = 0;
    eye.vy = -0x300;
    arena_first_actor.pos = first;
    arena_second_actor.pos = second;
    arena_camera_position = eye;
    arena_effect_clear_ground_effects();
}

/* 80070F80: Start a scene script; reset both actors' states and clear their 0x8000
 * flag. */
void arena_scene_start_script(u8 *script) {
    arena_scene_script_pc = script;
    arena_first_actor.state = 0;
    arena_second_actor.state = 0;
    arena_scene_wait_timer = 0;
    arena_first_actor.flags &= ~0x8000;
    arena_second_actor.flags &= ~0x8000;
}

/* 80070FD8: Walk an actor at stick speed 0xFF toward one of two fixed directions,
 * chosen by which side of the scene centre it stands. */
s32 arena_scene_walk_out_of_far_quadrant(Actor *actor) {
    VECTOR pos = actor->pos;

    pos.vx -= 0x3F80;
    pos.vz -= 0x3F80;
    if ((ratan2(pos.vx, pos.vz) & 0xFFF) > 0x200) {
        actor->target_angle = 0x800 - arena_actors_heading;
    } else {
        actor->target_angle = 0xC00 - arena_actors_heading;
    }
    actor->state = 0xFF;
    actor->unkCE = 0;
    return 0;
}

/* 8007107C: Run the scene script (arena_scene_script_pc) until a command waits. Each run first
 * releases the driven actor's stick (state 0) and applies command 32's
 * charge hold. A command is a byte and up to two byte operands: it selects
 * the driven actor, queues its inputs, walks it, waits frames or for the
 * message window, or sets scene values (marker sprite, message, camera
 * view, layout, bout-end step, hit points, charge, guard). Headings are
 * relative to the actor's facing toward its opponent. tools/analysis/
 * overlay_scripts.py disassembles the scripts. Declared int with no value
 * returned, which keeps $v0 live at its exits as in the original. */
s32 arena_scene_run_script(void) {
    Actor *actor = arena_scene_driven_actor;

    actor->state = 0;
    if (arena_scene_charge_hold != 0) {
        if (arena_scene_charge_hold < actor->charge) {
            arena_scene_charge_hold = actor->charge;
        }
        actor->charge = arena_scene_charge_hold;
    }
    for (;;) {
        switch (*arena_scene_script_pc) {
        /* 1 n: wait n frames (the first run loads the counter and yields). */
        case 1:
            if (arena_scene_wait_timer == 0) {
                arena_scene_wait_timer = arena_scene_script_pc[1];
                return;
            }
            if (--arena_scene_wait_timer != 0) {
                return;
            }
            arena_scene_script_pc += 2;
            break;
        /* 2: drive the first actor. */
        case 2:
            actor = arena_scene_driven_actor = &arena_first_actor;
            arena_scene_script_pc++;
            break;
        /* 3: drive the second actor. */
        case 3:
            actor = arena_scene_driven_actor = &arena_second_actor;
            arena_scene_script_pc++;
            break;
        /* 4: empty the driven actor's input queue. */
        case 4:
            arena_actor_clear_inputs(actor);
            arena_scene_script_pc++;
            break;
        /* 5: queue input 1 (combo button A, as pad button 0x10 does). */
        case 5:
            arena_actor_queue_input(actor, 1);
            arena_scene_script_pc++;
            break;
        /* 6: queue input 2 (combo button B, as pad button 0x20 does). */
        case 6:
            arena_actor_queue_input(actor, 2);
            arena_scene_script_pc++;
            break;
        /* 8: queue input 3 (charged shot, as pad button 8 does). */
        case 8:
            arena_actor_queue_input(actor, 3);
            arena_scene_script_pc++;
            break;
        /* 7: queue input 4 (jump, as pad button 0x80 does). */
        case 7:
            arena_actor_queue_input(actor, 4);
            arena_scene_script_pc++;
            break;
        /* 10: queue input 5 (animation 0xF with effect 0xE). */
        case 10:
            arena_actor_queue_input(actor, 5);
            arena_scene_script_pc++;
            break;
        /* 9: the same as 10. */
        case 9:
            arena_actor_queue_input(actor, 5);
            arena_scene_script_pc++;
            break;
        /* 11: while 0x100 or more apart, walk at the opponent (stick speed 0xF0,
         * heading 0) and yield; then advance. */
        case 11:
            if (arena_actors_flat_distance >= 0x100) {
                actor->state = 0xF0;
                actor->target_angle = 0;
                actor->unkCE = 0;
                return;
            }
            arena_scene_script_pc++;
            break;
        /* 12: while at most 0x400 apart, walk away (heading 0x800) and yield. */
        case 12:
            if (arena_actors_flat_distance <= 0x400) {
                actor->state = 0xF0;
                actor->target_angle = 0x800;
                actor->unkCE = 0;
                return;
            }
            arena_scene_script_pc++;
            break;
        /* 13: while in the far quadrant or on floor kind 1, walk out of it
         * (arena_scene_walk_out_of_far_quadrant); then while at most 0x800 apart, walk away; each
         * yields. */
        case 13:
            if (arena_brain_is_in_far_quadrant(actor)) {
                arena_scene_walk_out_of_far_quadrant(actor);
                return;
            }
            if (arena_actors_flat_distance <= 0x800) {
                actor->state = 0xF0;
                actor->target_angle = 0x800;
                actor->unkCE = 0;
                return;
            }
            arena_scene_script_pc++;
            break;
        /* 15 n: walk sideways (heading 0x400) for n frames. */
        case 15:
            if (arena_scene_wait_timer == 0) {
                arena_scene_wait_timer = arena_scene_script_pc[1];
                return;
            }
            if (--arena_scene_wait_timer != 0) {
                actor->state = 0xF0;
                actor->target_angle = 0x400;
                actor->unkCE = 0;
                return;
            }
            arena_scene_script_pc += 2;
            break;
        /* 14 n: walk sideways (heading -0x400) for n frames. */
        case 14:
            if (arena_scene_wait_timer == 0) {
                arena_scene_wait_timer = arena_scene_script_pc[1];
                return;
            }
            if (--arena_scene_wait_timer != 0) {
                actor->state = 0xF0;
                actor->target_angle = -0x400;
                actor->unkCE = 0;
                return;
            }
            arena_scene_script_pc += 2;
            break;
        /* 20 n: as 14, also setting the actor's 0x8000 flag. */
        case 20:
            if (arena_scene_wait_timer != 0) {
                if (--arena_scene_wait_timer != 0) {
                    actor->state = 0xF0;
                    actor->target_angle = -0x400;
                    actor->unkCE = 0;
                    actor->flags |= 0x8000;
                    return;
                }
                arena_scene_script_pc += 2;
                break;
            }
            arena_scene_wait_timer = arena_scene_script_pc[1];
            return;
        /* 16, 17: never advance; this loop runs them forever. */
        case 16:
        case 17:
            break;
        /* 18 m: show message m (arena_scene_message_number) in the message window. */
        case 18:
            arena_scene_script_pc++;
            arena_scene_message_number = *arena_scene_script_pc;
            arena_scene_script_pc++;
            break;
        /* 19: resume the message window (window_end_wait). */
        case 19:
            window_end_wait(&arena_scene_message_window);
            arena_scene_script_pc++;
            break;
        /* 27: hide the marker sprite and put it back at 0xA0, 0x6D. */
        case 27:
            arena_scene_marker_x = arena_scene_marker_target_x = 0xA0;
            arena_scene_marker_shown = 0;
            arena_scene_marker_y = arena_scene_marker_target_y = 0x6D;
            arena_scene_script_pc++;
            break;
        /* 21 x y: show the marker sprite, easing to x * 2, y. */
        case 21:
            arena_scene_marker_target_x = arena_scene_script_pc[1] * 2;
            arena_scene_marker_target_y = arena_scene_script_pc[2];
            arena_scene_marker_shown = 1;
            arena_scene_script_pc += 3;
            break;
        /* 22 c: scene callback c (arena_scene_run_callback). */
        case 22:
            arena_scene_run_callback(arena_scene_script_pc[1]);
            arena_scene_script_pc += 2;
            break;
        /* 23 v: camera view v (arena_camera_view_mode, arena_camera_apply_view_mode). */
        case 23:
            arena_camera_view_mode = arena_scene_script_pc[1];
            arena_scene_script_pc += 2;
            break;
        /* 24 s: bout-end sequence step s (arena_scene_bout_end_step, arena_scene_update_bout_end). */
        case 24:
            arena_scene_bout_end_step = arena_scene_script_pc[1];
            arena_scene_script_pc += 2;
            break;
        /* 25: wait until the message window stops with code 1 while pad button
         * 0x20 is newly pressed, then resume it and advance; a stop with
         * another code is resumed in place. */
        case 25:
            if (window_get_wait_state(&arena_scene_message_window) == 0 || !(pad_port0_pressed & 0x20)) {
                return;
            }
            if (window_get_wait_state(&arena_scene_message_window) == 1) {
                arena_scene_script_pc++;
            }
            window_end_wait(&arena_scene_message_window);
            break;
        /* 26: wait for a stop with code 2 or 3 while pad button 0x20 is newly
         * pressed, then resume the window and advance; yields on every run. */
        case 26:
            if (window_get_wait_state(&arena_scene_message_window) != 0 && (pad_port0_pressed & 0x20)) {
                s32 answer = window_get_wait_state(&arena_scene_message_window);

                if (answer < 4) {
                    if (answer >= 2) {
                        window_end_wait(&arena_scene_message_window);
                        arena_scene_script_pc++;
                    }
                }
            }
            return;
        /* 28 o: full HP for the driven actor (o = 0) or its opponent. */
        case 28:
            if (arena_scene_script_pc[1]) {
                actor->opponent->hp = actor->opponent->max_hp;
            } else {
                actor->hp = actor->max_hp;
            }
            arena_scene_script_pc += 2;
            break;
        /* 29 o: 1 HP for the driven actor (o = 0) or its opponent. */
        case 29:
            if (arena_scene_script_pc[1]) {
                actor->opponent->hp = 1;
            } else {
                actor->hp = 1;
            }
            arena_scene_script_pc += 2;
            break;
        /* 30: scene mode 3 and close the choice window (arena_scene_close_choice). */
        case 30:
            arena_scene_close_choice();
            arena_scene_script_pc++;
            break;
        /* 31 c: set the driven actor's charge to c * 16 (0x1000 is full). */
        case 31:
            actor->charge = arena_scene_script_pc[1] * 16;
            arena_scene_script_pc += 2;
            break;
        /* 32 c: hold the driven actor's charge at no less than c * 16 and its
         * highest value since (0 releases). */
        case 32:
            arena_scene_charge_hold = arena_scene_script_pc[1] * 16;
            arena_scene_script_pc += 2;
            break;
        /* 33 l: re-centre the scene on layout l (arena_scene_apply_layout). */
        case 33:
            arena_scene_apply_layout(arena_scene_script_pc[1]);
            arena_scene_script_pc += 2;
            break;
        /* 34 g: guard (flag 2) when g is nonzero; otherwise clear the guard and
         * its count (flags 0x38). */
        case 34:
            if (arena_scene_script_pc[1]) {
                actor->flags |= 2;
            } else {
                actor->flags &= ~2;
                actor->flags &= ~0x38;
            }
            arena_scene_script_pc += 2;
            break;
        /* 0 and bytes without a case: stop without advancing (arena_scene_update
         * restarts scene 0 once the script stands on 0). */
        case 0:
        default:
            return;
        }
    }
}

/* 80071724: Link the marker sprite (16x16, at arena_scene_marker_x, arena_scene_marker_y) and this
 * frame's texture page packet. */
void arena_scene_draw_marker(u32 *ot) {
    DisplayBuffer *frame = arena_current_draw_buffer;

    /* x0 and y0 of the sprite, stored as one word */
    *(u32 *)&frame->sprite.x0 = arena_scene_marker_x | (arena_scene_marker_y << 16);
    AddPrim(ot, &frame->sprite);
    AddPrim(ot, &arena_scene_marker_tpages[arena_draw_buffer_index]);
}

/* 80071794: Upload the menu's sprite sheet TIM (its first CLUT colour made
 * transparent), build both texture page packets and the sprite template. */
void arena_scene_load_sprite_sheet(MenuImageFile *files) {
    TIM_IMAGE image;
    RECT unused; /* the original frame reserves 8 more bytes */
    s16 *clut;

    OpenTIM(files->sheet);
    ReadTIM(&image);
    clut = (s16 *)image.caddr;
    clut[2] = -0x8000;
    clut[0] = 0;
    clut[3] = -1;
    LoadImage(image.crect, image.caddr);
    LoadImage(image.prect, image.paddr);
    SetDrawTPage(&arena_scene_marker_tpages[0], 0, 0, GetTPage(0, 1, image.prect->x, image.prect->y));
    arena_scene_marker_tpages[1] = arena_scene_marker_tpages[0];
    arena_display_buffers[0].sprite.u0 = (image.prect->x & 0x3F) * 4;
    arena_display_buffers[0].sprite.v0 = image.prect->y;
    arena_display_buffers[0].sprite.clut = GetClut(image.crect->x, image.crect->y);
    arena_display_buffers[1].sprite = arena_display_buffers[0].sprite;
}

/* 800718C0: Open the menu message window. */
void arena_scene_open_message_window(void) {
    arena_scene_shown_message_number = -1;
    arena_scene_message_number = 0;
    window_open(&arena_scene_message_window, 0x140, 0x30, 0x1C, 0x9A, 0x40, 4);
}

/* 8007191C: Enter a menu scene: the first scene also starts sound 0x37 and uses a
 * taller window; centres the marker sprite, releases the charge hold and
 * restarts both actors at full HP. */
void arena_scene_enter(s32 scene) {
    arena_scene_choice_open = scene == 0;
    if (scene == 0) {
        arena_sound_play_effect(0x37);
        arena_scene_message_window.lines = 2;
        arena_scene_message_window.unk6 = 0xB4;
    } else {
        arena_scene_message_window.lines = 4;
        arena_scene_message_window.unk6 = 0x9A;
    }
    arena_scene_start_script(arena_scene_scripts[scene]);
    arena_scene_marker_target_x = 0xA0;
    arena_scene_marker_x = 0xA0;
    arena_scene_marker_target_y = 0x6D;
    arena_scene_marker_y = 0x6D;
    arena_scene_charge_hold = 0;
    arena_first_actor.hp = arena_first_actor.max_hp;
    arena_second_actor.hp = arena_second_actor.max_hp;
}

/* 800719F0: Start the menu's opening: text window with message 0x42, the intro
 * script, then scene 9. */
void arena_scene_start_tutorial(void) {
    arena_settings.com1 = 0;
    arena_settings.driven = 0;
    arena_mode_set_state(7);
    arena_play_mode = 5;
    arena_rubber_band_enabled = 0;
    window_open(&arena_scene_choice_window, 0x140, 0x70, 0xA2, 0x2A, 0x1C, 8);
    window_queue_message(&arena_scene_choice_window, text_get_resource_entry(arena_text_message_table, 0x42));
    arena_scene_choice_window.unk68 = 0x1E;
    arena_scene_unread_word = 0;
    arena_scene_start_script(arena_scene_opening_script);
    arena_scene_choice_cursor = 0;
    arena_camera_view_mode = 0;
    arena_scene_bout_end_step = 0;
    arena_scene_enter(9);
}

/* 80071AD0: Per-frame menu scene update: scene choice input, the scene script, the
 * marker sprite (shown while arena_scene_marker_shown is set, blinking every 4 frames)
 * and its easing, the message window and the camera. */
void arena_scene_update(void) {
    Window *message;

    if (arena_scene_marker_shown != 0 && (arena_frame_count & 4)) {
        arena_scene_draw_marker(arena_current_ot);
    }
    pad_merge_queued_states();
    message = &arena_scene_message_window;
    if (arena_scene_choice_open != 0) {
        if (pad_port0_repeated & 0x1000) {
            arena_sound_play_effect(0x1E);
            arena_scene_choice_cursor--;
        }
        if (pad_port0_repeated & 0x4000) {
            arena_sound_play_effect(0x1E);
            arena_scene_choice_cursor++;
        }
        if (arena_scene_choice_cursor >= 8) {
            arena_scene_choice_cursor = 0;
        }
        if (arena_scene_choice_cursor < 0) {
            arena_scene_choice_cursor = 7;
        }
        window_set_color(&arena_scene_choice_window, (arena_frame_count * 7) & 0x3F, 0xC0, 0x10);
        window_highlight_line(&arena_scene_choice_window, arena_scene_choice_cursor);
        if (pad_port0_pressed & 0x20) {
            arena_sound_play_effect(0x21);
            arena_scene_enter(arena_scene_choice_cursor + 1);
            window_reset(message);
        }
    }
    if (*arena_scene_script_pc == 0) {
        arena_scene_enter(0);
    }
    arena_scene_run_script();
    arena_scene_marker_x += arena_camera_ease_step(arena_scene_marker_target_x, arena_scene_marker_x, 4);
    arena_scene_marker_y += arena_camera_ease_step(arena_scene_marker_target_y, arena_scene_marker_y, 4);
    if (arena_scene_choice_open != 0) {
        window_draw_frame(&arena_scene_choice_window, (u_long *)arena_current_ot, arena_draw_buffer_index);
    }
    if (arena_scene_message_number != arena_scene_shown_message_number) {
        message->unk68 = 3;
        window_reset(message);
        window_queue_message(message, text_get_resource_entry(arena_text_message_table, arena_scene_message_number));
        arena_scene_shown_message_number = arena_scene_message_number;
    }
    arena_bout_update(&arena_first_actor, &arena_second_actor);
    arena_camera_apply_view_mode(arena_camera_view_mode);
}

/* 80071DA4: Settle an actor on the floor: while a probe 0xC0 away in one of eight
 * directions finds the floor more than 0x40 higher, step away from it (at
 * most 20 times); then record the floor height and its attribute bits.
 * The direction table holds interleaved x/z words; each cursor follows
 * one column at the FloorStep stride. */
void arena_scene_settle_actor_on_floor(Actor *actor) {
    VECTOR *pos = &actor->pos;
    s32 tries = 0;
    u8 *steps_x = (u8 *)arena_scene_floor_steps;
    s32 best;
    s32 highest;
    s32 dir;
    s32 floor;
    VECTOR probe;

    do {
        u8 *steps_z;

        pos->vy = arena_stage_get_ground_height(pos, 1);
        highest = pos->vy;
        steps_z = (u8 *)arena_scene_floor_steps + sizeof(s32);
        for (dir = 0; dir < 8; dir++) {
            probe = *pos;
            probe.vx += *(s32 *)(steps_x + dir * sizeof(FloorStep)) * 0xC0;
            probe.vz += *(s32 *)(steps_z + dir * sizeof(FloorStep)) * 0xC0;
            floor = arena_stage_get_ground_height(&probe, 1);
            if (floor < highest - 0x40) {
                best = dir;
                highest = floor;
            }
        }
        if (highest >= pos->vy - 0x40) {
            pos->vy = arena_stage_get_ground_height(pos, 1);
            break;
        }
        {
            u8 *steps_z = (u8 *)arena_scene_floor_steps + sizeof(s32);

            pos->vx -= *(s32 *)(steps_x + best * sizeof(FloorStep)) * 0xC0;
            pos->vz -= *(s32 *)(steps_z + best * sizeof(FloorStep)) * 0xC0;
        }
        tries++;
    } while (tries < 20);
    actor->floor_y = arena_stage_get_ground_height(&actor->pos, 1);
    actor->flags = (actor->flags & 0x9FFFFFFF) | ((((u32)arena_stage_get_ground_square(&actor->pos) >> 24) & 3) << 29);
}

/* 80071F8C: Scene script callback: 0 plays the stored sound, 1/2 act on one actor
 * (1 also picks the message for whichever actor has more HP left), 3 sets
 * the camera's height from the actors' distance, capped at -0x600. */
s32 arena_scene_run_callback(s32 command) {
    switch (command) {
    case 0:
        sound_stop_seq(arena_mode_music_seq);
        sound_stop_all_effects();
        break;
    case 1:
        arena_scene_settle_actor_on_floor(&arena_first_actor);
        if ((arena_first_actor.hp << 8) / arena_first_actor.max_hp > (arena_second_actor.hp << 8) / arena_second_actor.max_hp) {
            arena_scene_message_number = 0x43;
        } else {
            arena_scene_message_number = 0x44;
        }
        break;
    case 2:
        arena_scene_settle_actor_on_floor(&arena_second_actor);
        break;
    case 3:
        arena_camera_position.vy = -arena_actors_flat_distance;
        if (arena_camera_position.vy < -0x600) {
            arena_camera_position.vy = -0x600;
        }
        break;
    }
}

/* 800720C4: Allow the next menu scene setup. */
void arena_scene_allow_bout_end(void) {
    arena_scene_bout_end_active = 0;
}

/* 800720D4: One-time scene setup: start the scene script and clear the actors'
 * counters and the message state. */
void arena_scene_start_bout_end(void) {
    if (arena_scene_bout_end_active == 0) {
        arena_scene_start_script(arena_scene_bout_end_script);
        arena_scene_bout_end_active = 1;
        arena_bout_fight_active = 0;
        arena_settings.com1 = 0;
        arena_settings.driven = 0;
        arena_scene_bout_end_step = 0;
        arena_scene_message_number = 0;
        arena_scene_shown_message_number = 0;
        arena_scene_bout_end_embers[0].pad = 1;
        arena_scene_bout_end_embers[1].pad = 1;
        arena_scene_bout_end_embers[2].pad = 1;
        arena_second_actor.unkE8 = 0;
        arena_first_actor.unkE8 = 0;
    }
}
/* 80072170: Per-frame scene effects of the bout-end sequence (step arena_scene_bout_end_step):
 * three sparking embers on the first actor's body, or the first actor
 * knocked down while a flash fades out and back in (which ends the bout);
 * then the caption when its text changed and the camera view. */
void arena_scene_update_bout_end(void) {
    Actor *actor;
    Window *window;
    VECTOR pos;
    Node *part;
    s32 i;
    s32 count;

    if (arena_scene_bout_end_active != 0) {
        actor = &arena_first_actor;
        window = &arena_scene_message_window;
        switch (arena_scene_bout_end_step) {
        case 11:
            arena_sound_play_effect(0x2A);
            arena_sound_play_effect(0x2B);
            arena_sound_play_effect(0x2C);
            arena_scene_bout_end_step = 1;
        case 1:
            for (i = 0; i < 3; i++) {
                if (--arena_scene_bout_end_embers[i].pad == -1) {
                    part = ((ModelSet *)arena_first_actor.node->data)->nodes[rand() % ((ModelSet *)arena_first_actor.node->data)->nodeCount];
                    arena_scene_bout_end_embers[i].vx = part->unk4C.t[0] + arena_first_actor.pos.vx;
                    arena_scene_bout_end_embers[i].vy = part->unk4C.t[1] + arena_first_actor.pos.vy;
                    arena_scene_bout_end_embers[i].vz = part->unk4C.t[2] + arena_first_actor.pos.vz;
                    arena_scene_bout_end_embers[i].pad = rand() % 16 + 8;
                }
                if (arena_scene_bout_end_embers[i].pad & 1) {
                    arena_effect_spawn_sparkle(&arena_scene_bout_end_embers[i], 9);
                }
            }
            break;
        case 2:
            actor->hp = 1;
            arena_scene_knockdown_flash_level = 0xFF;
            arena_scene_bout_end_step++;
        case 3:
            arena_glow_draw_shade_tile(arena_current_ot, arena_scene_knockdown_flash_level, 0);
            if (arena_scene_knockdown_flash_level < 0) {
                arena_scene_knockdown_flash_level = 0;
            }
            arena_scene_knockdown_flash_level -= 8;
        knocked:
            actor->unk4F = 0x10;
            actor->anim = 0xA;
            actor->unk52 = 0;
            actor->charge = 0x1000;
            actor->flags |= 0x400;
            if (rand() & 1) {
                count = ((ModelSet *)arena_first_actor.node->data)->nodeCount;
                part = ((ModelSet *)arena_first_actor.node->data)->nodes[rand() % count];
                pos.vx = part->unk4C.t[0] + arena_first_actor.pos.vx;
                pos.vy = part->unk4C.t[1] + arena_first_actor.pos.vy;
                pos.vz = part->unk4C.t[2] + arena_first_actor.pos.vz;
                arena_effect_spawn_sparkle(&pos, 0xB);
                arena_effect_spawn_sparkle(&pos, 8);
            }
            break;
        case 4:
            arena_scene_knockdown_flash_level += 3;
            if (arena_scene_knockdown_flash_level >= 0x100) {
                arena_scene_knockdown_flash_level = 0xFF;
                mode_arena_bout_outcome = 0x7F;
                arena_display_clear_buffers(0);
                arena_mode_exit();
            }
            arena_glow_draw_shade_tile(arena_current_ot, arena_scene_knockdown_flash_level, 1);
            goto knocked;
        }
        arena_scene_run_script();
        if (arena_scene_message_number != arena_scene_shown_message_number) {
            window->unk68 = 1;
            window_release_queue_if_idle(window);
            window_queue_message(window, text_get_resource_entry(arena_text_message_table, arena_scene_message_number));
            window->lines = 2;
            window->unk6 = 0xB4;
            arena_scene_shown_message_number = arena_scene_message_number;
        }
        arena_camera_apply_view_mode(arena_camera_view_mode);
        arena_first_actor.state = 0;
    }
}

/* 800725A8: Unreferenced, and empty. */
void arena_scene_empty_unreferenced(void) {
}

/* 800725B0: Set up the scene around an actor: graphics state, lights, its model
 * copy, the scene origin and the values from its move header. */
void arena_winner_open_screen(Actor *scene) {
    SceneHeader *header;

    model_set_envmap_mapping(5, 4, 0x40, 0x40);
    arena_winner_light_rig = arena_node_alloc_light_rig(arena_display_alloc_layer(0x10));
    arena_winner_shadow_node = arena_node_copy_root_as_instances(scene->node);
    arena_display_set_resolution(0x280, 0xDA);
    SetGeomScreen(0x400);
    arena_debug_display_flags = 0;
    arena_winner_actor = scene;
    arena_actor_reset_for_round(scene);
    arena_view_origin.vz = 0;
    arena_view_origin.vy = 0;
    arena_view_origin.vx = 0;
    header = scene->header;
    arena_winner_record_toggle = 1;
    arena_winner_pitch = header->unk14;
    arena_winner_turn_speed = header->unk16;
    arena_winner_offset_y = header->unk18;
    arena_winner_offset_z = header->unk1A;
    arena_winner_scale = header->unk1C;
    arena_winner_angles.vy = header->unk1E;
}

/* 800726B4: Tear down the scene set up by arena_winner_open_screen. */
void arena_winner_close_screen(void) {
    arena_task_yield();
    arena_menu_hide_captions();
    model_set_envmap_mapping(1, 1, 0x40, 0x40);
    arena_node_free_light_rig(arena_winner_light_rig);
    arena_node_free_tree(arena_winner_shadow_node);
    arena_display_set_resolution(0x140, 0xDA);
    SetGeomScreen(0xC0);
    arena_mode_set_state(3);
    arena_text_set_width_scale(0x100);
    arena_text_drop_quads();
}

/* 8007273C: Copy a model's matrix to out, rotated by the base matrix, with its
 * translation set to the model position relative to the scene origin. */
void arena_winner_build_shadow_matrix(Node *model, MATRIX *matrix, MATRIX *out) {
    MATRIX local;

    *out = *matrix;
    local = arena_identity_matrix;
    local.t[0] = model->position.vx - arena_view_origin.vx;
    local.t[1] = -arena_view_origin.vy;
    local.t[2] = model->position.vz - arena_view_origin.vz;
    CompMatrix(matrix, &local, &local);
    out->t[0] = local.t[0];
    out->t[1] = local.t[1];
    out->t[2] = local.t[2];
}

/* 80072858: One frame of the winner screen: turn the winner's model with the
 * shoulder buttons, toggle its record text with the first button, leave
 * with 0x20; draw the record (name, level, matches, time) and the model
 * turning in front of the scene's lights. */
void arena_winner_update_screen(LightRig *rig) {
    MATRIX unused1; /* the original frame has 32 unused bytes on */
    MATRIX m;
    MATRIX unused2; /* either side of the matrix */
    char text[64];
    Actor *winner = arena_winner_actor;
    u8 y;
    ModelSet *set;

    pad_merge_queued_states();
    if (winner->model_id != 7) {
        if (pad_port0_held & 0x2000) {
            arena_winner_turn_speed--;
        }
        if (pad_port0_held & 0x8000) {
            arena_winner_turn_speed++;
        }
    }
    if (arena_winner_turn_speed < -0x80) {
        arena_winner_turn_speed = -0x80;
    }
    if (arena_winner_turn_speed > 0x80) {
        arena_winner_turn_speed = 0x80;
    }
    if (arena_menu_driving_pad_port == 1) {
        if (pad_port1_pressed & 0x20) {
            arena_winner_close_screen();
            return;
        }
        if (pad_port1_pressed & 1) {
            arena_winner_record_toggle++;
        }
    } else {
        if (pad_port0_pressed & 0x20) {
            arena_winner_close_screen();
            return;
        }
        if (pad_port0_pressed & 1) {
            arena_winner_record_toggle++;
        }
    }
    if (arena_winner_record_toggle & 1) {
        y = 0x86;
        if (arena_play_mode == 2 || arena_play_mode == 3) {
            y = 0x9A;
        }
        arena_text_set_width_scale(0x1C0);
        arena_text_move_cursor(0x18, y);
        arena_text_draw_line("WINNER");
        if (arena_play_mode != 2 && arena_play_mode != 3) {
            arena_text_draw_line("LEVEL");
        }
        arena_text_draw_line("MATCHES");
        arena_text_draw_line("TIME");
        arena_text_move_cursor(0xA8, y);
        arena_text_draw_line(arena_select_gears[winner->model_id].name);
        if (arena_play_mode != 2 && arena_play_mode != 3) {
            sprintf(text, "%s", arena_menu_get_level_name());
            arena_text_draw_line(text);
        }
        sprintf(text, "%d/%d VS %s", winner->unkF2, arena_bout_round_number, arena_select_gears[winner->opponent->model_id].name);
        arena_text_draw_line(text);
        arena_bout_draw_elapsed_time();
    }
    arena_menu_draw_overlay(arena_current_ot);
    arena_winner_angles.vx = arena_winner_pitch;
    arena_winner_angles.vy += arena_winner_turn_speed;
    arena_winner_angles.vz = 0;
    gpu_build_rotation_matrix(&arena_winner_angles, &m);
    MulMatrix2(&arena_display_screen_scale, &m);
    m.t[0] = 0;
    m.t[1] = arena_winner_offset_y;
    m.t[2] = arena_winner_offset_z;
    arena_display_start_layer(rig->layer);
    arena_actor_play_animation(winner, 0);
    winner->node->position.vx = winner->node->position.vy = winner->node->position.vz = 0;
    winner->node->angles.vy = 0;
    set = winner->node->data;
    set->scale[0] = set->scale[1] = set->scale[2] = arena_winner_scale;
    ((Node *)winner->object)->view = m;
    arena_node_draw_tree(winner->node);
    arena_display_link_layer(rig->layer);
    arena_display_start_layer(arena_winner_light_rig->layer);
    arena_winner_build_shadow_matrix(winner->node, &m, &arena_winner_shadow_node->view);
    arena_node_draw_tree(arena_winner_shadow_node);
    {
        Node *camera = arena_winner_shadow_node;

        gte_SetRotMatrix(&camera->view);
        gte_SetTransMatrix(&camera->view);
        arena_node_draw_instances(camera);
    }
    arena_display_link_layer(arena_winner_light_rig->layer);
}

/* 80072D18 */
INCLUDE_ASM("decomp/src/menu", arena_stage_draw_ground_cells);

/* 80073064 */
INCLUDE_ASM("decomp/src/menu", arena_gte_scale_svector);

/* 800730AC */
INCLUDE_ASM("decomp/src/menu", arena_gte_multiply_svector);

/* 800730F4 */
INCLUDE_ASM("decomp/src/menu", arena_gte_scale_vector_low_halves);

/* 8007313C */
INCLUDE_ASM("decomp/src/menu", arena_box_filter_rgb555);

/* 800731F8 */
INCLUDE_ASM("decomp/src/menu", arena_gte_scale_matrix_columns);

/* 800732AC */
INCLUDE_ASM("decomp/src/menu", arena_copy_words);
