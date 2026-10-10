#ifndef DEBUG2611_BATTLE_DEBUG_H
#define DEBUG2611_BATTLE_DEBUG_H

/* The battle debug tools (battle_debug_tools.c): the battle overlay's objects come
 * from the shared battle headers (the area, the camera and the slots'
 * sprites in battle/actor.h, the camera's points in battle/objects.h, the
 * followed sprite in battle/sprite_script.h); the tools' load meter and
 * calls are here. */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/objects.h"
#include "battle/sprite_script.h"
#include "resident/console.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/sound.h"
#include "resident/sprite.h"

/* The bound object of a kind 15 sprite's model renderer: its polygon count. */
typedef struct {
    u8 unk0[0x14];
    s32 polys;         /* +14 */
} DebugShape;

/* The CPU/GPU load meter task: two needles over a dial, eased averages and
 * peaks held for 80 frames. */
typedef struct {
    Task task;      /* +00 */
    Task draw;      /* +1c */
    s32 cpu;        /* +38: needle angle */
    s32 gpu;        /* +3c */
    s32 cpu_avg;    /* +40: eased, x16 */
    s32 gpu_avg;    /* +44 */
    s32 cpu_peak;   /* +48 */
    s32 gpu_peak;   /* +4c */
    s32 cpu_hold;   /* +50: frames the peak is held */
    s32 gpu_hold;   /* +54 */
} LoadMeter;        /* 0x58 */

void battle_debug_move_camera_position(s32 buttons);
void battle_debug_move_look_at_point(s32 buttons);
void battle_debug_open_text_window(void);
void battle_debug_run_heap_monitor(void);
void battle_debug_load_meter_update(Task *task);
void battle_debug_draw_flat_triangle(SVECTOR *v, u8 r, u8 g, u8 b);
void battle_debug_load_meter_draw_tick(u8 r, u8 g, u8 b);
void battle_debug_load_meter_draw_peak(s16 length, u8 r, u8 g, u8 b);
void battle_debug_load_meter_draw(Task *task);
void battle_debug_print_wave_banks(void);
s32 battle_debug_run_actor_tool(void);
void battle_debug_write_heap_report_file(void);

#endif
