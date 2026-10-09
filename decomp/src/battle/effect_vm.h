#ifndef BATTLE_EFFECT_VM_H
#define BATTLE_EFFECT_VM_H

/* The battle objects' effect script VM (800AAD54): the object fields it uses
 * beyond BattleObject's, and the services it calls. */

/* The commands (800aad54's cases): a signed 16-bit word with the opcode in
 * its low byte and the argument (arg) in its high byte, then the listed u16
 * parameter words (a word named a_b holds a in its high byte, b in its low
 * byte). Offsets are signed bytes from the command's start; other opcodes
 * stop the script on themselves. tools/analysis/battle_effect_vm.py decodes
 * them (docs/scripts/battle-effect-vm.md).
 *   00 end: stop; the script stays on this command
 *   01 wait(frames): wait `frames` frames (counter at 0x40); a just-started
 *         script waits a frame first
 *   02 popup_wait_or_menu_step: acting for an event script (byte 0x35,
 *         800aa384): wait while a number popup shows (800bf6f8), then clear
 *         80059464, 800591ac and 0x35 and report to the event thread
 *         (80080c6c); otherwise count a battle menu step (800b9258) after
 *         the run
 *   03 popup_wait: as 02 without the menu step
 *   04 load_extra(file): without an extra file: read file arg +
 *         800c3530[file] of directory 0x28/2 as it
 *   05 setup_extra: once the extra file is read: relocate it, upload its
 *         images and sound bank and take its animations (ids from 0x40),
 *         waiting a run; byte 0x23 set: wait for the disc, clear it
 *   06 free_extra: free the extra file and its sound bank (800b0060)
 *   07 stop_disc_read: request a disc read stop (8002a498(0))
 *   08 stop_effects: release the parts' non-persistent effects (800a2acc),
 *         stop the object animation
 *   09 release_kind: release every part's effects of kind arg (800a2bb8)
 *   0a release_part: release part arg's effects 0-2 (800a216c mask 7)
 *   0b reset_parts: release the parts' non-persistent effects, zero the
 *         rotation and translation of every part below the root
 *   0c stop_motion: clear spin, spin acceleration, drift and drift
 *         acceleration
 *   0d release_part_rotation: release part arg's rotation effect (attachment
 *         0)
 *   0e release_part_translation: release part arg's translation effect
 *         (attachment 1)
 *   0f camera_end: camera mode 1 (800bcad0), release the camera channels
 *         (800b00f4), effects stop (800c3df8 = 0)
 *   10 pose: apply animation arg (800af518) as a frame at once (800a1b50)
 *   11 animate(mode_tag): unless the id resolves flagged (800af518): start
 *         animation arg on the parts (800a2434; mode high byte, tag low
 *         byte), take its span into 0x8e and start its events (800ae1bc,
 *         looping when mode is set)
 *   12 animate_from_start(mode_tag): as 11 through 800a2704 (the parts first
 *         take the frame's start values), no events
 *   13 tween_to_frame(tag_index, duration_smooth): tween the parts to
 *         animation (low byte) as a frame (800a1cf4: mode arg, tag high
 *         byte; duration, smoothing) and stop the object animation
 *   14 call(script_source): start script (high byte) of the object of target
 *         code (low byte; 0xFF: the target's own) on the object of target
 *         code arg (800aa934, queued while its queue is on); arg 0xFD (this
 *         object) switches without queueing and returns
 *   15 clone(part): copy the object into a free slot 13-1e with its own
 *         hierarchy; part `part` and its descendants are drawn on the copy
 *         only (800af180); start script arg on it (0xFF none)
 *   16 merge_back: draw the parts on the original object again (800af270),
 *         free this copy (800a9ff0) and start script arg on the original
 *         (0xFF none); returns
 *   17 free_self: free this object (800a9ff0); returns
 *   18 object_animation(loop): start the events of animation arg (800ae1bc),
 *         looping when `loop` is set
 *   19 stop_animation: stop the object animation (800aeeec)
 *   1a move_image(x, y, dst_x, dst_y, w, h): MoveImage the rectangle (width
 *         rounded up to even) to (dst_x, dst_y); arg bit 0: relative to the
 *         object's image placement (none: skipped)
 *   1b image_animation(mode_source, step_flags, x, y, z, x2, y2, z2, x3, y3,
 *         p0, p1, p2, p3, p4): start image animation arg (800a3640) from a
 *         source animation (0xFF none), mode (bit 7: at the image
 *         placement), step handler (800aa820), flags, rectangles and
 *         parameters; the 15 words are read even when arg is out of range
 *   1c stop_image_animation: stop image animation arg (800a429c)
 *   1d tween_part(mode_flags, field1_kind, start_x, start_y, start_z, end_x,
 *         end_y, end_z, duration): tween part arg's rotation, translation or
 *         scale (flags & 7) from start to end over `duration` (800af678:
 *         flags 0x20/0x40 relative start/end, 0x80 its children too)
 *   1e scaled_hierarchy: byte 0x37 = arg: compose with per-part scales
 *         (8009f1c4)
 *   1f select_object: act on the object of target code arg (800af438) from
 *         here when it exists; it takes the script position at the end of
 *         the run
 *   20 wait_tweens: wait while a tween runs (step flag 0x100); a just-
 *         started script waits a frame first
 *   21 wait_tag: tag (0x3c) = arg; wait while a tween with the tag runs
 *         (step flag 1)
 *   22 wait_loops(count): wait for `count` tween loops (step flag 0x400; arg
 *         != 0xFF: tag = arg, flag 4; counter at 0x42)
 *   23 show_part(part): draw part `part` or not by arg bit 0 (bit 7: with
 *         its descendants, 800afa98)
 *   24 show: active (0x34) = arg bit 0
 *   25 attach(part_code, x, y, z): attach the object of the target code (low
 *         byte) to part (high byte) at the offset, or where it is (arg bit
 *         0); arg bit 1 turns it with the part
 *   26 detach: detach the object of target code arg
 *   27 compose: recompose the hierarchy (8009f1c4 or 8009ef3c by byte 0x37)
 *   28 wait_effect_near(time_part): wait until part (low byte)'s rotation or
 *         translation effect is at step (high byte) and distance / 0x8e <
 *         arg
 *   29 wait_effect_far(time_part): as 28 with distance / 0x8e >= arg
 *   2a wait_near: wait while the distance to the position is >= 0x8e
 *   2b wait_far: wait while the distance to the position is <= 0x8e
 *   2c wait_point_near(x, y, z): wait while the distance to the point is >=
 *         0x8e
 *   2d wait_point_far(x, y, z): wait while the distance to the point is <=
 *         0x8e
 *   2e on_near(offset): jump in a later run once within 0x8e of the position
 *         (kept at 0x48); arg 0 cancels
 *   2f wait_popup: wait while a number popup shows (800bf6f8)
 *   30 loop_start(counter): loop counter (the operand word itself) = 0; arg
 *         is the loop count
 *   31 loop(offset): count the 30 command at start + offset; back past it
 *         while below its count
 *   32 jump(offset): jump
 *   33 jump_if_start_mode(offset): jump when the battle start mode
 *         (800d36b8) is nonzero
 *   34 jump_if_byte22_clear(offset): jump when byte 0x22 is 0
 *   35 jump_random(offset): jump when rand() >= 0x4000
 *   36 on_timer(frames, offset): jump in a later run after `frames` frames
 *         (timer at 0x44); arg 0 cancels
 *   37 on_ground(offset): jump in a later run once the root reaches the
 *         ground; arg 0 cancels
 *   38 track_position(rate_gain): turn the root toward the position every
 *         frame by at most arg + (distance + t) * gain / (first distance +
 *         1), t growing by rate a frame (800ae098 kind 7, pitch and yaw),
 *         until released
 *   39 track_position_yaw(rate_gain): as 38 for the heading only (kind 8)
 *   3a wait_popup_when_idle: wait while a number popup shows when the
 *         presentation events from index 2 are at their end (800aa7dc), byte
 *         0x22 is 0 or 0x35 is set
 *   3b jump_if_slot_flag(offset): jump when the object's slot bit of
 *         800c48e8 is set
 *   3c fade_sound(frames_sound): fade sound (low byte) of bank source arg
 *         (800ae220) to silence over (high byte) frames (8003a3b8)
 *   3d replay_queue_if: when arg is among the queued scripts: start the
 *         queue (5b 2), return
 *   3e jump_if_all_hit(offset): jump when every selected target's event code
 *         class (0: codes 0-1, 5: 2, 3, 5, 4: others) is arg; arg 8: when
 *         each target object has flag 2 (0x4a)
 *   3f menu_update: update the battle menu when open (800bf6cc)
 *   40 turn_to(x, y, z): turn the root to the angles over arg frames
 *         (800adf1c)
 *   41 turn_by(x, y, z): turn the root by the angles over arg frames
 *   42 face_position: turn the root toward the position over arg frames
 *   43 face_position_yaw: as 42, heading only
 *   44 set_spin(x, y, z): spin = (x, y, z)
 *   45 add_spin(x, y, z): spin += (x, y, z)
 *   46 set_spin_acceleration(x, y, z): spin acceleration = (x, y, z)
 *   47 add_spin_acceleration(x, y, z): spin acceleration += (x, y, z)
 *   48 keep_height: byte 0x36 = arg: nonzero keeps the root off the ground
 *         (800aff9c)
 *   49 place(x, y, z): put the root at (x, y, z)
 *   4a place_at: arg 0xFB: put the root at distance 0x8e from the position;
 *         else at x, z of target code arg
 *   4b set_drift(x, y, z): drift = (x, y, z)
 *   4c add_drift(x, y, z): drift += (x, y, z)
 *   4d set_drift_acceleration(x, y, z): drift acceleration = (x, y, z)
 *   4e add_drift_acceleration(x, y, z): drift acceleration += (x, y, z)
 *   4f drift_to_position: drift (z) that covers the distance to the position
 *         in arg frames, negated when the position is within a quarter turn
 *         of the heading
 *   50 set_position(x, y, z): position = (x, y, z); stop following
 *   51 position_at_slot: position = the battle position of target code arg
 *   52 follow(part, x, y, z): follow target code arg at part `part` and
 *         offset (800aef68)
 *   53 position_on_ground: put the position on the scene ground
 *   54 set_distance(distance): 0x8e = distance scaled by the object's scale
 *   55 add_distance(distance): 0x8e += distance scaled by the object's scale
 *   56 add_distance_raw(distance): 0x8e += distance
 *   57 add_slot_size: 0x8e += the scaled size of the object of target code
 *         arg (800aa650)
 *   58 follow_target: follow target code arg at its root
 *   59 position_at_camera_eye: position = camera preset arg's eye
 *   5a position_at_camera_target: position = camera preset arg's look-at
 *         point
 *   5b queue_mode: queue state (0x2b) = arg (1 not while scripts are
 *         queued); 2 starts the queued scripts (800aa934) and returns when
 *         there were any
 *   5c jump_if_at_position(offset): jump when the root is at the position
 *   5d set_draw_mode(part): part `part`'s draw mode (0x52) = arg (1 upright
 *         billboard, 2 facing the view, 4-7 model modes 2-5; 800a48ec)
 *   5e set_scale(scale): object scale (0x1c) = scale
 *   5f set_flags(flags): object flags (0x4a) = flags
 *   60 position_at_area: position = the centre of the slot's formation area
 *   61 jump_if_group(offset): jump when the slot's group has members among
 *         flagged groups (800885d0)
 *   62 set_part_transform(part, x, y, z): set or add part `part`'s rotation,
 *         translation or scale (800afd98, mode arg)
 *   63 object_events(offset): the object animation runs the arg animation
 *         events after the operand (800ae2a4, from the next frame); jump
 *         past them
 *   64 set_word3e(value): halfword 0x3e = value
 *   65 camera_target_to(code_kind, duration_preset, height): run camera
 *         channel 7 (the look-at point, 800b0164) from the look-at point to
 *         the position (code f6) or camera preset (low byte of word 2)'s
 *         look-at (f5) or eye (f4) raised by height, else after the object
 *         of the code (height as a fraction of its size); mode arg, kind
 *         (low byte), duration (high byte of word 2)
 *   66 camera_eye_to(code_kind, duration_preset, height): as 65 for channel
 *         8 (the camera position) from the camera position
 *   67 camera_turn(channel_kind, duration_flags, start, end): run camera
 *         channel (high byte: 0 orbit yaw, 1 look-at yaw, 2 orbit pitch, 3
 *         orbit distance, 4 look-at distance, 5 look-at height, 6 orbit
 *         height) from start to end (800b0164); yaws add the root's heading
 *         unless arg has 0x20; flag 0x20 starts at its value + start, 0x40
 *         ends at end past the start (with 0x20 the old value); yaws and
 *         pitch take the shorter way
 *   68 camera_start: release the camera channels, camera mode 4 (800bcaa4),
 *         effects run (800c3df8), no channel reported, the orbit faces the
 *         view with zero pitch, distances and heights
 *   69 camera_wait: report camera channel kind arg (800c3b84), then wait
 *         while it runs (800c3b88 bit 0)
 *   6a camera_snap: camera channels 7 and 8 start at their targets (800c3b8c
 *         = 1)
 *   6b rotation_order(part): part `part` composes its rotation in YXZ order
 *         when arg is set
 *   6c wait_disc: wait while the disc is busy (800286cc)
 *   6d set_busy: byte 0x38 = arg bit 0 (battle: 800b136c waits until none is
 *         set)
 *   6e wait_busy(value): wait while the object of target code arg has byte
 *         0x38 == value bit 0
 *   6f mark_targets: halfword 0x3a = the selected targets (800c3e30), or -1
 *         for arg 0
 *   70 jump_if_same_targets(offset): jump and end the run when 0x3a equals
 *         the selected targets
 *   71 set_action(action): arg 0: the single action to request (800d39e4) =
 *         action
 *   72 wait_action: end the run; stay here until the requested single action
 *         is done (800591b1)
 *   73 request_action: request single action 800d39e4 (800b8054; the frame
 *         loop starts it, 800b8068), end the run
 *   74 count_hit: count an effect hit (800bf998; the second frees object
 *         11); returns
 *   75 jump_if_facing(offset): jump when the root's yaw is the heading 43
 *         turns to */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "model.h"
#include "scene.h"
#include "effect.h"
#include "objects.h"

/* A script's pending jumps: to atScript once the object comes within
 * atDistance of its position (2E), to groundScript once it reaches the
 * ground (37), to timerScript after timerLimit frames (36). */
#define OBJECT_TIMER(object) (((u16 *)(object)->pad44)[0])       /* 0x44 */
#define OBJECT_TIMER_LIMIT(object) (((u16 *)(object)->pad44)[1]) /* 0x46 */
#define OBJECT_AT_DISTANCE(object) (((s16 *)(object)->pad44)[2]) /* 0x48 */
#define OBJECT_AT_SCRIPT(object) (*(u16 **)&(object)->field4C)
#define OBJECT_TIMER_SCRIPT(object) (*(u16 **)&(object)->field50)
#define OBJECT_GROUND_SCRIPT(object) (*(u16 **)&(object)->field54)
#define OBJECT_FIELD3E(object) (*(u16 *)(object)->pad3E)

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

/* The travel of an animation (its s16 at 0x10), in model units. */
#define ANIMATION_SPAN(animation) (((s16 *)(animation))[8])

extern u8 D_800C3530[]; /* extra file bases */
extern u16 D_800D39E4;  /* the single action to request (71, 73) */
extern u8 D_800D36B8; /* the battle's start mode */

void func_80022224(); /* upload an image (resource, image, at, clut, mode; the points by value) */

/* Services of this unit defined after the VM. */
u16 func_800A1CF4(EffectPool *pool, ModelPart *part, s16 *data, s32 duration, s32 mode, s32 smooth, s32 tag);
void func_800A216C(EffectPool *pool, ModelPart *part, s32 index, s32 mask);
s32 func_800A2434(EffectPool *pool, ModelPart *part, u16 *data, s32 mode, s32 tag);
s32 func_800A2704(EffectPool *pool, ModelPart *part, u16 *data, s32 mode, s32 tag);
u8 func_800AA7DC(s32 index);
void func_800ADF1C(EffectPool *pool, ModelPart *part, s32 duration, s32 x, s32 y, s32 z);
void func_800AE098(EffectPool *pool, ModelPart *part, s32 type, s32 param1, s32 param2, s32 field12, s32 x, s32 y,
                   s32 z);
void func_800AE1BC(BattleObject *object, Animation *animation, s32 loop);
s32 func_800AE220(BattleObject *object, s32 source);
void func_800AEEEC(BattleObject *object);
s32 func_800AEEF8(BattleObject *object);
void func_800AEF68(BattleObject *object);
void func_800AF270(ModelPart *from, ModelPart *to);
s16 func_800AF2C4(VECTOR *direction, VECTOR *a, VECTOR *b, s32 scale);
u8 *func_800AF518(BattleObject *object, u8 index, s32 *flag);
void func_800AF678(BattleObject *object, EffectPool *pool, ModelPart *part, u8 flags, u8 mode, u8 kind, u8 field1,
                   s16 startX, s16 startY, s16 startZ, s16 endX, s16 endY, s16 endZ, s16 duration);
void func_800AFD98(BattleObject *object, ModelPart *part, u8 mode, s16 x, s16 y, s16 z);
void func_800B00F4(EffectPool *pool);
/* Called unprototyped by the VM (its halfwords passed sign-extended). */
void func_800B0164(EffectPool *pool, s32 index, u8 field2, u8 kind, u16 p0, u16 p1, u16 p2, u16 p3, u16 p4,
                   u16 p5, u16 field12);

/* Services of other units. */
void func_80080C6C(u8 index);
u8 func_800885D0(u8 slot);
void func_800B8054(s32 action); /* request single action `action` (800b8068 runs it) */
void func_800B9258(void);
void func_800BCAA4(void);
void func_800BCAD0(void);
void func_800BF998(void);
u8 func_800AF438(BattleObject *object, u8 slot, u16 *mask); /* the slot of a target code */

#endif
