#ifndef OVL3087_OVL3087_H
#define OVL3087_OVL3087_H

#include "common.h"
#include "resident/formation.h"
#include "resident/window.h"
#include "battle/event_script.h"

/* Battle event script interpreter (loaded at 801e5000 by the battle
 * overlay when the formation sets 800c3d48). Scripts run as up to 16
 * threads; each thread has eight priority levels with their own program
 * counter. Opcode handlers take the thread index and the instruction bytes
 * and return the instruction length (0 when the thread yields). The
 * interpreter's state and script file are battle/event_script.h's. */

/* A variable addressed by its byte offset in the variable area. */
#define SCRIPT_VAR(state, offset) \
    (*(u16 *)((u8 *)(state)->vars + ((offset) & 0xFFFE)))
/* The variable named by the instruction's first operand. */
#define INSN_VAR(insn) SCRIPT_VAR(battle_state_of_event_script, ((insn)[2] << 8) | (insn)[1])

/* Script file 2: per script set, the compressed script and its data. */
typedef struct {
    s32 count;
    struct {
        void *script;
        void *data;
    } sets[1];
} ScriptArchive;

/* Set n of the script archive as the original addresses it: from
 * archive + n * 8, past the count. */
typedef struct {
    s32 count;
    void *script;
    void *data;
} ScriptSet;

/* Script file 3: the model archive, entry offsets from +4. */
typedef struct {
    s32 count;
    void *entries[1];
} ModelArchive;

/* This overlay's data. */
extern u8 D_801E9B5C[];   /* portrait file per actor (normal, mirrored) */
extern u16 D_801E9C10[5]; /* default message window layout */
extern s32 D_801E9C1C;    /* the cursor glyph's frame, cycled 4..0 */
extern u8 D_801E9C20[16]; /* actor action started by the script */
extern s32 D_801E9C30;    /* text origin */
extern s32 D_801E9C34;
extern ModelArchive *D_801E9C38;

/* Resident data no shared header declares: the movie's last frame, which
 * the script sets for the movie it starts. */
extern u16 cd_movie_request_last_frame;

/* Resident functions whose callers convert arguments/result differently
 * from the resident definition (decomp/src/resident/own_declarations.h). */
void window_open(Window *window, s32 vramX, s32 vramY, s32 x, s32 y, s32 columns, s32 lines);
void *text_get_resource_entry(void *messages, u16 message);
s32 sound_create_and_play_seq(u8 *sequence, u8 volume, s32 frames);
void sound_play_effect_volume_pan(s32 sound, s16 volume, s16 pan);
void sound_set_effect_volume(s32 sound, u16 volume);
void sound_sync_transfer(s32 arg);

/* Battle functions the shared battle headers leave out: those whose callers
 * convert arguments/result differently from the battle's definition
 * (decomp/src/battle/own_declarations.h; 8007FF14, 800AA320, 800AA384,
 * 800B838C and 8009C0E0 are declared in their units), and 8008AB70. */
void battle_wait_frame(void);
void battle_gear_hud_show(s32 arg);
void battle_leave_member_menu(s32 arg);
void battle_leave_formation_group(s32 arg);
u16 battle_get_slot_bit(u8 id);
void *battle_heap_alloc(s32 size, s32 top);
void battle_window_open(s32 id, u16 x, u16 y, u16 width, u16 height, s32 style, s32 wait);
void battle_window_close(s32 id);
void battle_set_slot_attack_level4(s32 arg);
void battle_start_object_script(u16 member, u16 target, u16 arg);
void battle_make_object_act(u16 member, u16 target, u16 arg);
void battle_quake_start(u16 *position, u16 arg);
void battle_screen_fade_start(u16 actor, s32 mode, s32 r, s32 g, s32 b);
void battle_play_sound_to_end(u16 index, u16 variant);
void battle_highlight_slots(s32 arg);

/* This module. */
u16 func_801E5768(ScriptThread *thread);
u16 func_801E57C4(ScriptThread *thread);
void func_801E57F8(u8 *insn, u8 count, u8 immediateMask, u8 signedForm);
u8 func_801E58EC(s16 a, s16 b, u8 op);
s32 func_801E5DCC(s32 thread, u8 *insn);
void func_801E5B00(); /* K&R (s16 x, s16 y); callers pass ints unconverted */
void func_801E6750(u8 actor, s32 flags, s32 x, s32 y, s32 width);
u8 func_801E6CE8(u16 message, u8 actor, u16 flags);
void func_801E7A5C(s32 thread, u8 *insn);
s32 func_801E84A4(s32 thread, u8 *insn);

#endif
