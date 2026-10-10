#ifndef FIELD_MONITOR_H
#define FIELD_MONITOR_H

/* The field and its debug monitor (debug595, at 80280000): the monitor's
 * entries and debug-lines flag, which the field calls and sets while
 * field_monitor_absent is clear, and the field's state and calls that the monitor
 * shows, edits and makes. The field and the monitor both include this and
 * field/actors.h, the actor, descriptor and collision records both read; the
 * monitor's views of the field's other objects (its view, work block, emitters
 * and an actor's box) stay in debug595.h. */

#include "common.h"

/* The monitor: its debug-lines flag and entries (field_debug_monitor.c), and the
 * field's flag for its absence (field_common.c, set by 80077e88) and its
 * page toggle (field.c). */
extern s32 field_monitor_absent;         /* set when the debug monitor is absent */
extern s32 field_debug_lines_shown;         /* debug lines shown: the field sets it on a talk or a
                                * touch, the monitor clears it each frame (80281400) */
extern s32 field_monitor_page_toggle;         /* the monitor's page toggle (field.c) */

void field_debug_mark_cpu_time(char *name);      /* start a named timer ("EVENT CODE", "PARTICLE  ") */
void field_debug_reset_screen_cursor(void);
void field_debug_count_encounter(s32 formation); /* count the formation field_encounter_count_down drew */
void field_debug_clear_counters(void);
void field_debug_reset_lines(void);
void field_debug_finish_frame(void);
void field_debug_draw_lines(void);
void field_debug_update_line_matrices(void);
void field_debug_move_camera(void);              /* move the camera by the pad (L2 and the debug button) */

/* This frame's held and repeated buttons per port, which the monitor reads
 * (80074700 drains them; the field's field_pad.h has the newly pressed ones
 * and port 1's mask). */
extern u16 field_pad_port0_held;         /* port 1 held */
extern u16 field_pad_port0_repeated;     /* port 1 repeated (they move a window's choice) */
extern u16 field_pad_port1_held;         /* port 2 held */
extern u16 field_pad_port1_repeated;     /* port 2 repeated */
extern s16 field_player_stuck_frames;    /* frames stuck against terrain (player control) */

/* The frame: its times and the draw buffer. */
extern s32 field_frame_start_time;                                                    /* frame start time; the monitor's marks restart it */
extern s32 field_frame_cpu_time;                                                      /* frame draw (CPU) time */
extern s32 field_frame_gpu_time;                                                      /* GPU time (the VSync counter, 8007781c) */
extern s32 field_draw_buffer_index;                                                   /* the current draw buffer (0 or 1) */
void field_instance_refresh_bounds_modes(void);                                       /* refresh the instances' bounds and modes */
void field_fade_start(s32 channel, s32 steps, s32 red, s32 green, s32 blue, s32 abr); /* start a fade */

/* The camera. */
extern s32 field_ground_override_height;          /* the floor height actors take while it is set */
extern s32 field_ground_override_enabled;         /* set by the monitor's dolly (80284ea4); the field's
                                * motion tests it (8007b814, 80084158) */
s32 field_camera_get_octant(void);                  /* camera octant (0..7; the monitor's CamDIR) */
s32 field_actor_get_controlled_facing_octant(void); /* the controlled actor's facing octant (ChrDIR) */

/* The events: the actor count, the variable bank and its read (a reference
 * is a byte offset into the bank). Variable 0 is the scenario flag. */
extern s32 field_event_actor_count;           /* event actor count */
extern s16 field_event_variables[0x400];      /* event variable bank */
s32 field_event_read_variable(s32 reference); /* read a variable */
void field_encounter_draw_steps(void);        /* draw distinct random numbers into the work block */
void field_save_snapshot(void);               /* write the snapshot */

/* The particle effects: the template the events (and the monitor's editor)
 * set up, and the effect slots. */
extern s32 field_effect_edited_template;         /* the template the monitor edits (BANK); cleared with
                                * the templates */
extern s32 field_effect_template_actor;                  /* the selected template's +52 (80089004), 0xff none */
s32 field_effect_start(s32 owner);                       /* start an effect; -1 when no slot */
void field_effect_stop_by_owner(s32 owner, s32 release); /* stop an owner's effects */

#endif
