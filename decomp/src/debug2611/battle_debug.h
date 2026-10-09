#ifndef DEBUG2611_BATTLE_DEBUG_H
#define DEBUG2611_BATTLE_DEBUG_H

/* The battle debug tools (debug2611.c): the battle overlay's objects come
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

/* Callers convert arguments/result differently from the resident definition
 * (u16 colours there): build the console font CLUTs from a foreground and a
 * background colour. */
void func_80036E4C(s32 foreground, s32 background);

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

void func_80280844(s32 buttons);
void func_80280960(s32 buttons);
void func_8028103C(void);
void func_802810C4(void);
void func_80281330(Task *task);
void func_802813F4(SVECTOR *v, u8 r, u8 g, u8 b);
void func_802814F8(u8 r, u8 g, u8 b);
void func_802815E8(s16 length, u8 r, u8 g, u8 b);
void func_802816AC(Task *task);
void func_8028191C(void);
s32 func_80281980(void);
void func_80281F98(void);

#endif
