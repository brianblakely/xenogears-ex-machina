/*
 * libgpu (PsyQ 4.x sys.c "1.129 1996/12/25", prim.c, tim.c) and libetc's
 * video mode over the host's GPU (xem/gpu.h). Every function keeps the SDK's
 * packet formats and memory effects: the GPU words it sends, the ordering
 * tables and packets it writes, the RECTs it clamps in place and its state at
 * the original addresses (GEnv, the command queue, the ClearImage packet and
 * the OT terminator), so DMA lists the SDK builds for itself are game memory
 * the device reads like the game's own.
 *
 * Transfers end when the game next waits (xem/gpu.h): until then DMA2 is busy
 * and libgpu queues further work, which the completion interrupt runs through
 * _exeque, the DMA callback the SDK installs; DrawSyncCallback's function runs
 * from that interrupt when the queue drains, as on the console. The SDK's spin
 * loops on the busy bit yield (XEM_YIELD_DRAWSYNC) instead.
 *
 * Not ported: libgpu's debug messages, which go through libgpu_printf, set
 * to mode_empty_debug_print (an empty function) in this program; the GPU
 * timeout (set_alarm/get_alarm), which cannot fire when every transfer ends
 * at the next wait; and the TMD reader (OpenTMD, ReadTMD, get_tmd_addr,
 * unpack_packet), which no image calls.
 */
#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libetc.h"
#include "xem/port.h"
#include "xem/gpu.h"

/* libetc (port/libetc.c). */
void *DMACallback(int channel, void (*func)(void));
long GetVideoMode(void);

/* libgpu's environment, GEnv: its first byte is libgpu_graph_type (splat
 * names the fields separately). ResetGraph clears all 0x80 bytes. */
typedef struct {
    u8 graph_type;   /* GPU type from _version: 0-4 */
    u8 queue_mode;   /* SetGraphQueue: 0 runs work at once */
    u8 debug_level;  /* SetGraphDebug */
    u8 reverse;      /* SetGraphReverse */
    s16 vram_width;  /* by graph type */
    s16 vram_height;
    s32 draw_sync_pending; /* work was submitted since the last callback */
    void (*draw_sync_callback)(void);
    DRAWENV draw; /* the last PutDrawEnv/DrawOTagEnv */
    DISPENV disp; /* the last PutDispEnv */
} XemGpuEnv;

/* One entry of the 64-entry command queue: a function from the SDK's table,
 * called as func(argument, parameter); `data` holds a copy of the argument
 * when the caller asked for one (RECTs, DR_ENVs, the MoveImage packet). */
typedef int (*XemGpuQueueFunction)(u_long *argument, u_long parameter);
typedef struct {
    XemGpuQueueFunction function;
    u_long *argument;
    u_long parameter;
    u_long data[21];
} XemGpuQueueEntry;

#if defined(__mips__)
LAYOUT_CHECK(xem_gpu_env_size, sizeof(XemGpuEnv) == 0x80);
LAYOUT_CHECK(xem_gpu_queue_entry_size, sizeof(XemGpuQueueEntry) == 0x60);
#endif

#define XEM_GPU_QUEUE_LENGTH 64

extern XemGpuEnv libgpu_graph_type;
extern u_long libgpu_current_function_table;
extern s32 libgpu_type_vram_widths[5];
extern s32 libgpu_type_vram_heights[5];
extern u_long libgpu_move_image_source;
extern u_long libgpu_move_image_destination;
extern u_long libgpu_move_image_size;
extern u_long libgpu_clear_otag_terminator[5];
extern u_long libgpu_clear_packet[13];
extern u8 libgpu_control_words[0x100];
extern XemGpuQueueFunction libgpu_last_call_function;
extern u_long *libgpu_last_call_argument;
extern u_long libgpu_last_call_parameter;
extern s32 libgpu_queue_write_index;
extern s32 libgpu_queue_read_index;
extern XemGpuQueueEntry libgpu_queue_entry_function[XEM_GPU_QUEUE_LENGTH];
extern u_long *libgpu_tim_cursor;
extern long libetc_video_mode;

#define genv libgpu_graph_type

/* GPUSTAT bit 26: ready for a command word; DMA2 CHCR bit 24: busy. */
#define XEM_GPU_READY 0x04000000
#define XEM_GPU_DMA_BUSY 0x01000000

void _exeque(void);
int _reset(int mode);
int _sync(int mode);
void SetDrawEnv2(DR_ENV *dr_env, DRAWENV *env);

static s32 xem_gpu_dma_busy(void) {
    return (xem_host_gpu_dma_control() & XEM_GPU_DMA_BUSY) != 0;
}

/* The SDK's spin loops on the GPU and DMA: the transfer ends at a wait. */
static void xem_gpu_wait_ready(void) {
    while (!(xem_host_gpu_status() & XEM_GPU_READY)) {
        xem_host_yield(XEM_YIELD_DRAWSYNC);
    }
}

static void xem_gpu_copy_words(u_long *dst, u_long *src, s32 count) {
    while (count-- > 0) {
        *dst++ = *src++;
    }
}

static s32 xem_gpu_queue_empty(void) {
    return libgpu_queue_write_index == libgpu_queue_read_index;
}

/* ---------------------------------------------------------------- video mode */

/* libetc: 0 NTSC, 1 PAL. */
long SetVideoMode(long mode) {
    long previous = libetc_video_mode;

    libetc_video_mode = mode;
    return previous;
}

long GetVideoMode(void) {
    return libetc_video_mode;
}

/* ---------------------------------------------------------------- low level */

void libgpu_fill_bytes(void *p, int value, int count) {
    u8 *bytes = p;

    while (count-- > 0) {
        *bytes++ = value;
    }
}

/* BIOS A(49h): send a GP0 command word. */
void GPU_cw(u_long word) {
    xem_host_gpu_gp0(word);
}

u_long _status(void) {
    return xem_host_gpu_status();
}

/* GP1 command; the SDK remembers each command's low byte. */
void _ctl(u_long word) {
    xem_host_gpu_gp1(word);
    libgpu_control_words[word >> 24] = word;
}

u_long _getctl(int command) {
    return libgpu_control_words[command];
}

/* GP1(10h): a GPU information word. */
u_long _param(int index) {
    xem_host_gpu_gp1(0x10000000 | index);
    return xem_host_gpu_read() & 0xFFFFFF;
}

/* Words to GP0 by the CPU. */
int _cwb(u_long *words, u_long count) {
    xem_host_gpu_gp1(0x04000000);
    while (count-- != 0) {
        xem_host_gpu_gp0(*words++);
    }
    return 0;
}

/* A DMA2 linked list (an ordering table or a packet chain) to GP0. */
int _cwc(u_long *list, u_long unused) {
    xem_host_gpu_gp1(0x04000002);
    xem_host_gpu_dma((u32)list, 0, 0x01000401);
    return 0;
}

/* DMA6 clears an ordering table backwards: each entry links to the one
 * before it and the first ends the list (0xFFFFFF). */
int _otc(u_long *ot, int count) {
    s32 i;

    for (i = count - 1; i > 0; i--) {
        ot[i] = (u32)&ot[i - 1] & 0xFFFFFF;
    }
    if (count > 0) {
        ot[0] = 0xFFFFFF;
    }
    return count;
}

static s16 xem_gpu_clamp(s16 value, s16 limit) {
    if (value < 0) {
        return 0;
    }
    return value > limit ? limit : value;
}

/* Fill a VRAM rectangle (ClearImage): GP0(02h) when it is 64-pixel aligned in
 * x and width, else a drawing-area-wide rectangle (GP0(60h)) between saved and
 * restored drawing settings. `color` is 0xBBGGRR, bit 31 the mask bit. */
int _clr(RECT *rect, u_long color) {
    u_long *packet = libgpu_clear_packet;
    u_long mode;

    rect->w = xem_gpu_clamp(rect->w, libgpu_graph_type.vram_width - 1);
    rect->h = xem_gpu_clamp(rect->h, libgpu_graph_type.vram_height - 1);
    mode = 0xE1000000 | (color >> 31) << 10 | (xem_host_gpu_status() & 0x7FF);
    if ((rect->x & 0x3F) || (rect->w & 0x3F)) {
        packet[0] = 0x08000000 | ((u32)&packet[9] & 0xFFFFFF);
        packet[1] = 0xE3000000;
        packet[2] = 0xE4FFFFFF;
        packet[3] = 0xE5000000;
        packet[4] = 0xE6000000;
        packet[5] = mode;
        packet[6] = 0x60000000 | (color & 0xFFFFFF);
        packet[7] = *(u_long *)&rect->x;
        packet[8] = *(u_long *)&rect->w;
        packet[9] = 0x03FFFFFF;
        packet[10] = _param(3) | 0xE3000000;
        packet[11] = _param(4) | 0xE4000000;
        packet[12] = _param(5) | 0xE5000000;
    } else {
        packet[0] = 0x05FFFFFF;
        packet[1] = 0xE6000000;
        packet[2] = mode;
        packet[3] = 0x02000000 | (color & 0xFFFFFF);
        packet[4] = *(u_long *)&rect->x;
        packet[5] = *(u_long *)&rect->w;
    }
    _cwc(packet, 0);
    return 0;
}

/* The RECT's transfer size, clamped in place to the VRAM: words in all and
 * the 16-word DMA blocks among them. */
static s32 xem_gpu_transfer_words(RECT *rect, s32 *blocks) {
    s32 doubled;

    rect->w = xem_gpu_clamp(rect->w, libgpu_graph_type.vram_width);
    rect->h = xem_gpu_clamp(rect->h, libgpu_graph_type.vram_height);
    doubled = rect->w * rect->h + 1;
    *blocks = doubled >> 5;
    return doubled / 2;
}

/* LoadImage: GP0(A0h); the words beyond whole 16-word blocks by the CPU,
 * then the blocks by DMA. */
int _dws(RECT *rect, u_long pixels) {
    u_long *p = (u_long *)pixels;
    s32 blocks;
    s32 words = xem_gpu_transfer_words(rect, &blocks);
    s32 rest;

    if (words <= 0) {
        return -1;
    }
    rest = words - blocks * 16;
    xem_gpu_wait_ready();
    xem_host_gpu_gp1(0x04000000);
    xem_host_gpu_gp0(0x01000000);
    xem_host_gpu_gp0(0xA0000000);
    xem_host_gpu_gp0(*(u_long *)&rect->x);
    xem_host_gpu_gp0(*(u_long *)&rect->w);
    while (rest-- > 0) {
        xem_host_gpu_gp0(*p++);
    }
    if (blocks != 0) {
        xem_host_gpu_gp1(0x04000002);
        xem_host_gpu_dma((u32)p, blocks << 16 | 0x10, 0x01000201);
    }
    return 0;
}

/* StoreImage: GP0(C0h); the words beyond whole blocks by the CPU, then the
 * blocks by DMA. */
int _drs(RECT *rect, u_long pixels) {
    u_long *p = (u_long *)pixels;
    s32 blocks;
    s32 words = xem_gpu_transfer_words(rect, &blocks);
    s32 rest;

    if (words <= 0) {
        return -1;
    }
    rest = words - blocks * 16;
    xem_gpu_wait_ready();
    xem_host_gpu_gp1(0x04000000);
    xem_host_gpu_gp0(0x01000000);
    xem_host_gpu_gp0(0xC0000000);
    xem_host_gpu_gp0(*(u_long *)&rect->x);
    xem_host_gpu_gp0(*(u_long *)&rect->w);
    while (!(xem_host_gpu_status() & 0x08000000)) {
        xem_host_yield(XEM_YIELD_DRAWSYNC);
    }
    while (rest-- > 0) {
        *p++ = xem_host_gpu_read();
    }
    if (blocks != 0) {
        xem_host_gpu_gp1(0x04000003);
        xem_host_gpu_dma((u32)p, blocks << 16 | 0x10, 0x01000200);
    }
    return 0;
}

/* The GPU type: 3 for the retail GPU (GP1(10h) index 7 answers 2), 4 with
 * mode bit 3 (GP1(09h) on); otherwise 0-2 from whether GP0(E1h) bit 12 sticks. */
int _version(int mode) {
    xem_host_gpu_gp1(0x10000007);
    if ((xem_host_gpu_read() & 0xFFFFFF) == 2) {
        if (mode & 8) {
            xem_host_gpu_gp1(0x09000001);
            return 4;
        }
        return 3;
    }
    xem_host_gpu_gp0(0xE1001000 | (xem_host_gpu_status() & 0x3FFF));
    xem_host_gpu_read();
    if (!(xem_host_gpu_status() & 0x1000)) {
        return 0;
    }
    if (!(mode & 8)) {
        return 1;
    }
    xem_host_gpu_gp1(0x20000504);
    return 2;
}

/* Mode 0 and 5: stop DMA2, reset the GPU and clear the queue; 1 and 3: stop
 * DMA2, acknowledge the GPU interrupt and drop a partly sent command. Mode 0
 * returns the GPU type. */
int _reset(int mode) {
    libgpu_queue_read_index = 0;
    libgpu_queue_write_index = libgpu_queue_read_index;
    switch (mode & 7) {
    case 0:
    case 5:
        xem_host_gpu_dma(0, 0, 0x401);
        xem_host_gpu_gp1(0);
        libgpu_fill_bytes(libgpu_control_words, 0, sizeof libgpu_control_words);
        libgpu_fill_bytes(libgpu_queue_entry_function, 0, sizeof libgpu_queue_entry_function);
        break;
    case 1:
    case 3:
        xem_host_gpu_dma(0, 0, 0x401);
        xem_host_gpu_gp1(0x02000000);
        xem_host_gpu_gp1(0x01000000);
        break;
    }
    if ((mode & 7) != 0) {
        return 0;
    }
    return _version(mode);
}

/* ---------------------------------------------------------------- the queue */

/* Run queued work while DMA2 is free (the DMA2 interrupt callback); when the
 * queue has drained and the transfer ended, call DrawSyncCallback's function.
 * The original also returns the work still queued, which no caller reads; the
 * port's has no result, as DMACallback calls it. */
void _exeque(void) {
    XemGpuQueueEntry *entry;

    if (xem_gpu_dma_busy()) {
        return;
    }
    while (!xem_gpu_queue_empty() && !xem_gpu_dma_busy()) {
        if (((libgpu_queue_read_index + 1) & (XEM_GPU_QUEUE_LENGTH - 1)) == libgpu_queue_write_index &&
            libgpu_graph_type.draw_sync_callback == NULL) {
            DMACallback(2, NULL);
        }
        xem_gpu_wait_ready();
        entry = &libgpu_queue_entry_function[libgpu_queue_read_index];
        entry->function(entry->argument, entry->parameter);
        libgpu_last_call_function = entry->function;
        libgpu_last_call_argument = entry->argument;
        libgpu_last_call_parameter = entry->parameter;
        libgpu_queue_read_index = (libgpu_queue_read_index + 1) & (XEM_GPU_QUEUE_LENGTH - 1);
    }
    if (xem_gpu_queue_empty() && !xem_gpu_dma_busy() && libgpu_graph_type.draw_sync_pending &&
        libgpu_graph_type.draw_sync_callback != NULL) {
        libgpu_graph_type.draw_sync_pending = 0;
        libgpu_graph_type.draw_sync_callback();
    }
}

/* Run func(argument, parameter) now when nothing is queued or running and no
 * DrawSyncCallback waits (or queueing is off); else queue it, with a copy of
 * `size` bytes of the argument, and let the DMA2 interrupt run it. */
int _addque2(XemGpuQueueFunction func, u_long *argument, int size, u_long parameter) {
    XemGpuQueueEntry *entry;

    while (((libgpu_queue_write_index + 1) & (XEM_GPU_QUEUE_LENGTH - 1)) == libgpu_queue_read_index) {
        _exeque();
        if (((libgpu_queue_write_index + 1) & (XEM_GPU_QUEUE_LENGTH - 1)) == libgpu_queue_read_index) {
            xem_host_yield(XEM_YIELD_DRAWSYNC);
        }
    }
    libgpu_graph_type.draw_sync_pending = 1;
    if (libgpu_graph_type.queue_mode == 0 ||
        (xem_gpu_queue_empty() && !xem_gpu_dma_busy() && libgpu_graph_type.draw_sync_callback == NULL)) {
        xem_gpu_wait_ready();
        func(argument, parameter);
        libgpu_last_call_function = func;
        libgpu_last_call_argument = argument;
        libgpu_last_call_parameter = parameter;
        return 0;
    }
    DMACallback(2, _exeque);
    entry = &libgpu_queue_entry_function[libgpu_queue_write_index];
    if (size != 0) {
        xem_gpu_copy_words(entry->data, argument, size / 4);
        entry->argument = entry->data;
    } else {
        entry->argument = argument;
    }
    entry->parameter = parameter;
    entry->function = func;
    libgpu_queue_write_index = (libgpu_queue_write_index + 1) & (XEM_GPU_QUEUE_LENGTH - 1);
    _exeque();
    return (libgpu_queue_write_index - libgpu_queue_read_index) & (XEM_GPU_QUEUE_LENGTH - 1);
}

int _addque(XemGpuQueueFunction func, u_long *argument, u_long parameter) {
    return _addque2(func, argument, 0, parameter);
}

/* Mode 0: run the queue and wait for the GPU, returning 0. Otherwise the
 * work still queued (at least 1 while a transfer runs). */
int _sync(int mode) {
    s32 queued;

    if (mode == 0) {
        while (!xem_gpu_queue_empty()) {
            _exeque();
            if (!xem_gpu_queue_empty()) {
                xem_host_yield(XEM_YIELD_DRAWSYNC);
            }
        }
        while (xem_gpu_dma_busy() || !(xem_host_gpu_status() & XEM_GPU_READY)) {
            xem_host_yield(XEM_YIELD_DRAWSYNC);
        }
        return 0;
    }
    queued = (libgpu_queue_write_index - libgpu_queue_read_index) & (XEM_GPU_QUEUE_LENGTH - 1);
    if (queued != 0) {
        _exeque();
    }
    if (xem_gpu_dma_busy() || !(xem_host_gpu_status() & XEM_GPU_READY)) {
        return queued != 0 ? queued : 1;
    }
    return queued;
}

/* ---------------------------------------------------------------- sys.c */

/* Mode 0, 3 and 5 initialise libgpu (clearing GEnv, so the DrawSync callback
 * too) and the GPU and return its type; others only reset the queue and the
 * command buffer. */
int ResetGraph(int mode) {
    switch (mode & 7) {
    case 0:
    case 3:
    case 5:
        libgpu_fill_bytes(&libgpu_graph_type, 0, sizeof libgpu_graph_type);
        ResetCallback();
        GPU_cw(libgpu_current_function_table & 0xFFFFFF);
        libgpu_graph_type.graph_type = _reset(mode);
        libgpu_graph_type.queue_mode = 1;
        libgpu_graph_type.vram_width = libgpu_type_vram_widths[libgpu_graph_type.graph_type];
        libgpu_graph_type.vram_height = libgpu_type_vram_heights[libgpu_graph_type.graph_type];
        libgpu_fill_bytes(&libgpu_graph_type.draw, -1, sizeof libgpu_graph_type.draw);
        libgpu_fill_bytes(&libgpu_graph_type.disp, -1, sizeof libgpu_graph_type.disp);
        return libgpu_graph_type.graph_type;
    }
    return _reset(1);
}

int SetGraphReverse(int mode) {
    s32 previous = libgpu_graph_type.reverse;

    libgpu_graph_type.reverse = mode;
    _ctl(_getctl(8) | (libgpu_graph_type.reverse ? 0x08000080 : 0x08000000));
    if (libgpu_graph_type.graph_type == 2) {
        _ctl(libgpu_graph_type.reverse ? 0x20000501 : 0x20000504);
    }
    return previous;
}

int SetGraphDebug(int level) {
    s32 previous = libgpu_graph_type.debug_level;

    libgpu_graph_type.debug_level = level;
    return previous;
}

int SetGraphQueue(int mode) {
    s32 previous = libgpu_graph_type.queue_mode;

    if (mode != libgpu_graph_type.queue_mode) {
        _reset(1);
        libgpu_graph_type.queue_mode = mode;
        DMACallback(2, NULL);
    }
    return previous;
}

int GetGraphType(void) {
    return libgpu_graph_type.graph_type;
}

int GetGraphDebug(void) {
    return libgpu_graph_type.debug_level;
}

int DrawSyncCallback(void (*func)()) {
    void (*previous)(void) = libgpu_graph_type.draw_sync_callback;

    libgpu_graph_type.draw_sync_callback = (void (*)(void))func;
    return (int)previous;
}

void SetDispMask(int mask) {
    if (mask == 0) {
        libgpu_fill_bytes(&libgpu_graph_type.disp, -1, sizeof libgpu_graph_type.disp);
    }
    _ctl(mask ? 0x03000000 : 0x03000001);
}

int DrawSync(int mode) {
    return _sync(mode);
}

/* Debug-level RECT validation: only prints (through libgpu_printf). */
void checkRECT(char *name, RECT *rect) {
}

int ClearImage(RECT *rect, u_char r, u_char g, u_char b) {
    return _addque2((XemGpuQueueFunction)_clr, (u_long *)rect, 8, b << 16 | g << 8 | r);
}

/* ClearImage with the mask bit set (GP0(E1h) bit 10). */
int ClearImage2(RECT *rect, u_char r, u_char g, u_char b) {
    return _addque2((XemGpuQueueFunction)_clr, (u_long *)rect, 8, 0x80000000 | b << 16 | g << 8 | r);
}

int LoadImage(RECT *rect, u_long *p) {
    return _addque2((XemGpuQueueFunction)_dws, (u_long *)rect, 8, (u_long)p);
}

int StoreImage(RECT *rect, u_long *p) {
    return _addque2((XemGpuQueueFunction)_drs, (u_long *)rect, 8, (u_long)p);
}

/* GP0(80h) through the SDK's packet: a list node of four words (the tag
 * 0x04FFFFFF and 0x80000000 precede libgpu_move_image_source). */
int MoveImage(RECT *rect, int x, int y) {
    if (rect->w == 0 || rect->h == 0) {
        return -1;
    }
    libgpu_move_image_destination = y << 16 | (x & 0xFFFF);
    libgpu_move_image_source = *(u_long *)&rect->x;
    libgpu_move_image_size = *(u_long *)&rect->w;
    return _addque2(_cwc, (u_long *)((u32)&libgpu_move_image_source - 8), 0x14, 0);
}

/* Link each entry to the next; the last ends at the SDK's terminator. */
u_long *ClearOTag(u_long *ot, int n) {
    while (--n != 0) {
        ot[0] = (u32)&ot[1] & 0xFFFFFF;
        ot++;
    }
    *ot = (u32)libgpu_clear_otag_terminator & 0xFFFFFF;
    return ot;
}

/* Link each entry to the previous (DMA6); the first ends at the terminator. */
u_long *ClearOTagR(u_long *ot, int n) {
    _otc(ot, n);
    *ot = (u32)libgpu_clear_otag_terminator & 0xFFFFFF;
    return ot;
}

/* Send one primitive's words by the CPU once the GPU is idle. */
void DrawPrim(void *p) {
    s32 words = ((u8 *)p)[3];

    _sync(0);
    _cwb((u_long *)p + 1, words);
}

void DrawOTag(u_long *p) {
    _addque2(_cwc, p, 0, 0);
}

DRAWENV *PutDrawEnv(DRAWENV *env) {
    SetDrawEnv2(&env->dr_env, env);
    env->dr_env.tag |= 0xFFFFFF;
    _addque2(_cwc, (u_long *)&env->dr_env, sizeof env->dr_env, 0);
    xem_gpu_copy_words((u_long *)&libgpu_graph_type.draw, (u_long *)env, sizeof *env / 4);
    return env;
}

/* The environment's packet followed by the ordering table. */
void DrawOTagEnv(u_long *p, DRAWENV *env) {
    SetDrawEnv2(&env->dr_env, env);
    env->dr_env.tag = (env->dr_env.tag & 0xFF000000) | ((u32)p & 0xFFFFFF);
    _addque2(_cwc, (u_long *)&env->dr_env, sizeof env->dr_env, 0);
    xem_gpu_copy_words((u_long *)&libgpu_graph_type.draw, (u_long *)env, sizeof *env / 4);
}

DRAWENV *GetDrawEnv(DRAWENV *env) {
    xem_gpu_copy_words((u_long *)env, (u_long *)&libgpu_graph_type.draw, sizeof *env / 4);
    return env;
}

/* The display area's horizontal start for GPU types 1 and 2 (whose reverse
 * mode mirrors it; type 2 counts in halves). */
int get_dx(DISPENV *env) {
    switch (libgpu_graph_type.graph_type) {
    case 1:
        if (libgpu_graph_type.reverse) {
            return 0x400 - env->disp.w - env->disp.x;
        }
        break;
    case 2:
        if (libgpu_graph_type.reverse) {
            return 0x400 - env->disp.w / 2 - env->disp.x;
        }
        return env->disp.x / 2;
    }
    return env->disp.x;
}

/* GP1(05h) the display start; GP1(06h)/(07h) the screen ranges when the
 * screen changed; GP1(08h) the mode when the mode or area changed. */
DISPENV *PutDispEnv(DISPENV *env) {
    DISPENV *current = &libgpu_graph_type.disp;
    u_long mode = 0x08000000;
    s32 h_start;
    s32 h_end;
    s32 v_start;
    s32 v_end;

    if (libgpu_graph_type.graph_type == 1 || libgpu_graph_type.graph_type == 2) {
        _ctl(0x05000000 | (env->disp.y & 0xFFF) << 12 | (get_dx(env) & 0xFFF));
    } else {
        _ctl(0x05000000 | (env->disp.y & 0x3FF) << 10 | (env->disp.x & 0x3FF));
    }
    if (current->screen.x != env->screen.x || current->screen.y != env->screen.y ||
        current->screen.w != env->screen.w || current->screen.h != env->screen.h) {
        env->pad0 = GetVideoMode();
        h_start = env->screen.x * 10 + 0x260;
        v_start = env->screen.y + (env->pad0 ? 0x13 : 0x10);
        h_end = env->screen.w ? h_start + env->screen.w * 10 : h_start + 0xA00;
        v_end = v_start + (env->screen.h ? env->screen.h : 0xF0);
        if (h_start < 0x1F4) {
            h_start = 0x1F4;
        } else if (h_start > 0xCDA) {
            h_start = 0xCDA;
        }
        if (h_end < h_start + 0x50) {
            h_end = h_start + 0x50;
        } else if (h_end > 0xCDA) {
            h_end = 0xCDA;
        }
        if (v_start < 0x10) {
            v_start = 0x10;
        } else if (v_start > (env->pad0 ? 0x136 : 0x100)) {
            v_start = env->pad0 ? 0x136 : 0x100;
        }
        if (v_end < v_start + 2) {
            v_end = v_start + 2;
        } else if (v_end > (env->pad0 ? 0x138 : 0x102)) {
            v_end = env->pad0 ? 0x138 : 0x102;
        }
        _ctl(0x06000000 | (h_end & 0xFFF) << 12 | (h_start & 0xFFF));
        _ctl(0x07000000 | (v_end & 0x3FF) << 10 | (v_start & 0x3FF));
    }
    if (*(u_long *)&current->isinter != *(u_long *)&env->isinter || current->disp.x != env->disp.x ||
        current->disp.y != env->disp.y || current->disp.w != env->disp.w || current->disp.h != env->disp.h) {
        env->pad0 = GetVideoMode();
        if (env->pad0 == 1) {
            mode |= 0x08;
        }
        if (env->isrgb24) {
            mode |= 0x10;
        }
        if (env->isinter) {
            mode |= 0x20;
        }
        if (libgpu_graph_type.reverse) {
            mode |= 0x80;
        }
        if (env->disp.w > 0x118) {
            if (env->disp.w <= 0x160) {
                mode |= 0x01;
            } else if (env->disp.w <= 0x190) {
                mode |= 0x40;
            } else if (env->disp.w <= 0x230) {
                mode |= 0x02;
            } else {
                mode |= 0x03;
            }
        }
        if (env->disp.h > (env->pad0 ? 0x120 : 0x100)) {
            mode |= 0x24;
        }
        _ctl(mode);
    }
    xem_gpu_copy_words((u_long *)current, (u_long *)env, sizeof *env / 4);
    return env;
}

DISPENV *GetDispEnv(DISPENV *env) {
    xem_gpu_copy_words((u_long *)env, (u_long *)&libgpu_graph_type.disp, sizeof *env / 4);
    return env;
}

/* The field being displayed (GPUSTAT bit 31). */
int GetODE(void) {
    return _status() >> 31;
}

/* ---------------------------------------------------------------- packets */

/* GP0(E1h): texture page with dithering (dtd) and drawing to the displayed
 * area (dfe); GPU types 1 and 2 place the bits higher. */
u_long get_mode(int dfe, int dtd, int tpage) {
    u_long mode;

    if (libgpu_graph_type.graph_type == 1 || libgpu_graph_type.graph_type == 2) {
        mode = dtd ? 0xE1000800 : 0xE1000000;
        return mode | (dfe ? (tpage & 0x27FF) | 0x1000 : tpage & 0x27FF);
    }
    mode = dtd ? 0xE1000200 : 0xE1000000;
    return mode | (dfe ? (tpage & 0x9FF) | 0x400 : tpage & 0x9FF);
}

/* A drawing-area corner, clamped to the VRAM. */
static u_long xem_gpu_corner(u_long command, s32 x, s32 y) {
    s16 cx = x;
    s16 cy = y;

    if (cx < 0) {
        cx = 0;
    } else if (cx > libgpu_graph_type.vram_width - 1) {
        cx = libgpu_graph_type.vram_width - 1;
    }
    if (cy < 0) {
        cy = 0;
    } else if (cy > libgpu_graph_type.vram_height - 1) {
        cy = libgpu_graph_type.vram_height - 1;
    }
    if (libgpu_graph_type.graph_type == 1 || libgpu_graph_type.graph_type == 2) {
        return command | (cy & 0xFFF) << 12 | (cx & 0xFFF);
    }
    return command | (cy & 0x3FF) << 10 | (cx & 0x3FF);
}

/* GP0(E3h): the drawing area's top left. */
u_long get_cs(s16 x, s16 y) {
    return xem_gpu_corner(0xE3000000, x, y);
}

/* GP0(E4h): the drawing area's bottom right. */
u_long get_ce(s16 x, s16 y) {
    return xem_gpu_corner(0xE4000000, x, y);
}

/* GP0(E5h): the drawing offset. */
u_long get_ofs(s16 x, s16 y) {
    if (libgpu_graph_type.graph_type == 1 || libgpu_graph_type.graph_type == 2) {
        return 0xE5000000 | (y & 0xFFF) << 12 | (x & 0xFFF);
    }
    return 0xE5000000 | (y & 0x7FF) << 11 | (x & 0x7FF);
}

/* GP0(E2h): the texture window (none for NULL); x and y count in 8 pixels,
 * the masks are the negated sizes. */
u_long get_tw(RECT *tw) {
    if (tw == NULL) {
        return 0;
    }
    return 0xE2000000 | ((u8)tw->y >> 3) << 15 | ((u8)tw->x >> 3) << 10 | ((-tw->h & 0xFF) >> 3) << 5 |
           ((-tw->w & 0xFF) >> 3);
}

/* The drawing environment's packet: area, offset, mode, texture window and
 * mask settings; with isbg, a rectangle that clears the area. */
static void xem_gpu_draw_env(DR_ENV *dr_env, DRAWENV *env, s32 fill_aligned) {
    RECT area;
    s32 words = 7;

    dr_env->code[0] = get_cs(env->clip.x, env->clip.y);
    dr_env->code[1] = get_ce(env->clip.x + env->clip.w - 1, env->clip.y + env->clip.h - 1);
    dr_env->code[2] = get_ofs(env->ofs[0], env->ofs[1]);
    dr_env->code[3] = get_mode(env->dfe, env->dtd, env->tpage);
    dr_env->code[4] = get_tw(&env->tw);
    dr_env->code[5] = 0xE6000000;
    if (env->isbg) {
        area = env->clip;
        area.w = xem_gpu_clamp(area.w, libgpu_graph_type.vram_width - 1);
        area.h = xem_gpu_clamp(area.h, libgpu_graph_type.vram_height - 1);
        if (fill_aligned && !(area.x & 0x3F) && !(area.w & 0x3F)) {
            dr_env->code[6] = 0x02000000 | env->b0 << 16 | env->g0 << 8 | env->r0;
        } else {
            area.x -= env->ofs[0];
            area.y -= env->ofs[1];
            dr_env->code[6] = 0x60000000 | env->b0 << 16 | env->g0 << 8 | env->r0;
        }
        dr_env->code[7] = *(u_long *)&area.x;
        dr_env->code[8] = *(u_long *)&area.w;
        words += 3;
    }
    ((P_TAG *)dr_env)->len = words - 1;
}

void SetDrawEnv(DR_ENV *dr_env, DRAWENV *env) {
    xem_gpu_draw_env(dr_env, env, 0);
}

/* SetDrawEnv, but a 64-pixel aligned background is filled with GP0(02h). */
void SetDrawEnv2(DR_ENV *dr_env, DRAWENV *env) {
    xem_gpu_draw_env(dr_env, env, 1);
}

DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h) {
    env->clip.x = x;
    env->clip.y = y;
    env->clip.w = w;
    env->tw.x = 0;
    env->tw.y = 0;
    env->tw.w = 0;
    env->tw.h = 0;
    env->r0 = 0;
    env->g0 = 0;
    env->b0 = 0;
    env->dtd = 1;
    env->clip.h = h;
    env->dfe = GetVideoMode() ? h < 0x121 : h < 0x101;
    env->ofs[0] = x;
    env->ofs[1] = y;
    env->tpage = 10;
    env->isbg = 0;
    return env;
}

DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h) {
    env->disp.x = x;
    env->disp.y = y;
    env->disp.w = w;
    env->screen.x = 0;
    env->screen.y = 0;
    env->screen.w = 0;
    env->screen.h = 0;
    env->isrgb24 = 0;
    env->isinter = 0;
    env->pad1 = 0;
    env->pad0 = 0;
    env->disp.h = h;
    return env;
}

void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw) {
    setlen(p, 2);
    p->code[0] = get_mode(dfe, dtd, tpage & 0xFFFF);
    p->code[1] = get_tw(tw);
}

void SetTexWindow(DR_TWIN *p, RECT *tw) {
    setlen(p, 2);
    p->code[0] = get_tw(tw);
    p->code[1] = 0;
}

void SetDrawArea(DR_AREA *p, RECT *r) {
    setlen(p, 2);
    p->code[0] = get_cs(r->x, r->y);
    p->code[1] = get_ce(r->x + r->w - 1, r->y + r->h - 1);
}

void SetDrawOffset(DR_OFFSET *p, u_short *ofs) {
    setlen(p, 2);
    p->code[0] = get_ofs(ofs[0], ofs[1]);
    p->code[1] = 0;
}

/* GP0(E6h): set the mask bit (pbc), skip masked pixels (pbw). */
void SetPriority(DR_MODE *p, int pbc, int pbw) {
    setlen(p, 2);
    p->code[0] = 0xE6000000 | (pbc ? 2 : 0) | (pbw ? 1 : 0);
    p->code[1] = 0;
}

void SetDrawTPage(DR_TPAGE *p, int dfe, int dtd, int tpage) {
    setlen(p, 1);
    p->code[0] = (dtd ? 0xE1000200 : 0xE1000000) | (dfe ? (tpage & 0x9FF) | 0x400 : tpage & 0x9FF);
}

/* GP0(80h) in a packet; empty (length 0) for a zero-sized RECT. */
void SetDrawMove(DR_MOVE *p, RECT *rect, int x, int y) {
    setlen(p, (rect->w != 0 && rect->h != 0) ? 5 : 0);
    p->code[0] = 0x01000000;
    p->code[1] = 0x80000000;
    p->code[2] = *(u_long *)&rect->x;
    p->code[3] = y << 16 | (x & 0xFFFF);
    p->code[4] = *(u_long *)&rect->w;
}

/* GP0(A0h) in a packet for up to 12 words of pixels, which the caller places
 * after it; then GP0(01h). Larger images get length 0. */
void SetDrawLoad(u_long *p, RECT *rect) {
    u32 words = (rect->w * rect->h + 1) / 2;
    u32 length = words < 13 ? words + 4 : 0;

    setlen(p, length);
    p[1] = 0xA0000000;
    p[2] = *(u_long *)&rect->x;
    p[3] = *(u_long *)&rect->w;
    p[length] = 0x01000000;
}

/* Join p1 to p0 as one packet (at most 16 words after the tag). */
int MargePrim(void *p0, void *p1) {
    s32 words = ((P_TAG *)p0)->len + ((P_TAG *)p1)->len + 1;

    if (words > 16) {
        return -1;
    }
    setlen(p0, words);
    *(u_long *)p1 = 0;
    return 0;
}

u_short GetTPage(int tp, int abr, int x, int y) {
    return getTPage(tp, abr, x, y);
}

u_short GetClut(int x, int y) {
    return getClut(x, y);
}

/* Load a texture page image of colour depth tp (0: 4-bit, 1: 8-bit,
 * 2: 16-bit) and return its tpage. */
u_short LoadTPage(u_long *pix, int tp, int abr, int x, int y, int w, int h) {
    RECT rect;

    rect.x = x;
    rect.y = y;
    rect.w = tp == 0 ? w / 4 : tp == 1 ? w / 2 : w;
    rect.h = h;
    LoadImage(&rect, pix);
    return GetTPage(tp, abr, x, y);
}

u_short LoadClut(u_long *clut, int x, int y) {
    RECT rect;

    rect.x = x;
    rect.y = y;
    rect.w = 256;
    rect.h = 1;
    LoadImage(&rect, clut);
    return GetClut(x, y);
}

u_short LoadClut2(u_long *clut, int x, int y) {
    RECT rect;

    rect.x = x;
    rect.y = y;
    rect.w = 16;
    rect.h = 1;
    LoadImage(&rect, clut);
    return GetClut(x, y);
}

/* The dumps only print (through libgpu_printf). */
void libgpu_dump_tpage(u_short tpage) {
}

void DumpClut(u_short clut) {
}

void libgpu_dump_draw_env(DRAWENV *env) {
}

void libgpu_dump_disp_env(DISPENV *env) {
}

/* The next packet of a chain, at its KSEG0 address. */
void *NextPrim(void *p) {
    return (void *)((*(u_long *)p & 0xFFFFFF) | 0x80000000);
}

int IsEndPrim(void *p) {
    return (*(u_long *)p & 0xFFFFFF) == 0xFFFFFF;
}

/* Insert p at the head of OT entry ot (both keep their length bytes). */
void AddPrim(void *ot, void *p) {
    *(u_long *)p = (*(u_long *)p & 0xFF000000) | (*(u_long *)ot & 0xFFFFFF);
    *(u_long *)ot = (*(u_long *)ot & 0xFF000000) | ((u32)p & 0xFFFFFF);
}

/* Insert the chain p0..p1 at the head of OT entry ot. */
void AddPrims(void *ot, void *p0, void *p1) {
    *(u_long *)p1 = (*(u_long *)p1 & 0xFF000000) | (*(u_long *)ot & 0xFFFFFF);
    *(u_long *)ot = (*(u_long *)ot & 0xFF000000) | ((u32)p0 & 0xFFFFFF);
}

void CatPrim(void *p0, void *p1) {
    *(u_long *)p0 = (*(u_long *)p0 & 0xFF000000) | ((u32)p1 & 0xFFFFFF);
}

void TermPrim(void *p) {
    *(u_long *)p |= 0xFFFFFF;
}

void SetSemiTrans(void *p, int abe) {
    if (abe) {
        ((P_TAG *)p)->code |= 0x02;
    } else {
        ((P_TAG *)p)->code &= ~0x02;
    }
}

void SetShadeTex(void *p, int tge) {
    if (tge) {
        ((P_TAG *)p)->code |= 0x01;
    } else {
        ((P_TAG *)p)->code &= ~0x01;
    }
}

/* Primitive headers: the word count after the tag and the GP0 command. */
#define XEM_GPU_SET_PRIM(name, type, words, command) \
    void name(type *p) {                              \
        setlen(p, words);                             \
        setcode(p, command);                          \
    }

/* Lines of three or more points end with the polyline terminator. */
#define XEM_GPU_SET_POLYLINE(name, type, words, command, end) \
    void name(type *p) {                                      \
        setlen(p, words);                                     \
        setcode(p, command);                                  \
        ((u_long *)p)[end] = 0x55555555;                      \
    }

typedef struct {
    u_long tag;
    u_long words[7];
} XemLineG3;

typedef struct {
    u_long tag;
    u_long words[9];
} XemLineG4;

XEM_GPU_SET_PRIM(SetPolyF3, POLY_F3, 4, 0x20)
XEM_GPU_SET_PRIM(SetPolyFT3, POLY_FT3, 7, 0x24)
XEM_GPU_SET_PRIM(SetPolyG3, POLY_G3, 6, 0x30)
XEM_GPU_SET_PRIM(SetPolyGT3, POLY_GT3, 9, 0x34)
XEM_GPU_SET_PRIM(SetPolyF4, POLY_F4, 5, 0x28)
XEM_GPU_SET_PRIM(SetPolyFT4, POLY_FT4, 9, 0x2C)
XEM_GPU_SET_PRIM(SetPolyG4, POLY_G4, 8, 0x38)
XEM_GPU_SET_PRIM(SetPolyGT4, POLY_GT4, 12, 0x3C)
XEM_GPU_SET_PRIM(SetSprt8, SPRT_8, 3, 0x74)
XEM_GPU_SET_PRIM(SetSprt16, SPRT_16, 3, 0x7C)
XEM_GPU_SET_PRIM(SetSprt, SPRT, 4, 0x64)
XEM_GPU_SET_PRIM(SetTile1, TILE_1, 2, 0x68)
XEM_GPU_SET_PRIM(SetTile8, TILE_1, 2, 0x70)
XEM_GPU_SET_PRIM(SetTile16, TILE_1, 2, 0x78)
XEM_GPU_SET_PRIM(SetTile, TILE, 3, 0x60)
XEM_GPU_SET_PRIM(SetLineF2, LINE_F2, 3, 0x40)
XEM_GPU_SET_PRIM(SetLineG2, LINE_G2, 4, 0x50)
XEM_GPU_SET_POLYLINE(SetLineF3, LINE_F3, 5, 0x48, 5)
XEM_GPU_SET_POLYLINE(libgpu_set_line_g3, XemLineG3, 7, 0x58, 7)
XEM_GPU_SET_POLYLINE(SetLineF4, LINE_F4, 6, 0x4C, 6)
XEM_GPU_SET_POLYLINE(libgpu_set_line_g4, XemLineG4, 9, 0x5C, 9)

/* ---------------------------------------------------------------- TIM */

int OpenTIM(u_long *addr) {
    libgpu_tim_cursor = addr;
    return 0;
}

/* Parse a TIM at `tim` into `image`: its mode, the CLUT block when mode bit 3
 * says there is one, and the pixel block. Returns the TIM's length in words,
 * or -1 when it does not start with the TIM id 0x10. */
int get_tim_addr(u_long *tim, TIM_IMAGE *image) {
    s32 clut_words = 0;

    if (*tim++ != 0x10) {
        return -1;
    }
    image->mode = *tim++;
    if (image->mode & 8) {
        image->crect = (RECT *)(tim + 1);
        image->caddr = tim + 3;
        clut_words = *tim >> 2;
        tim += clut_words;
    } else {
        image->crect = NULL;
        image->caddr = NULL;
    }
    image->prect = (RECT *)(tim + 1);
    image->paddr = tim + 3;
    return clut_words + (*tim >> 2) + 2;
}

TIM_IMAGE *ReadTIM(TIM_IMAGE *image) {
    s32 words = get_tim_addr(libgpu_tim_cursor, image);

    if (words == -1) {
        return NULL;
    }
    libgpu_tim_cursor += words;
    return image;
}

/* ---------------------------------------------------------------- OT links */

/* The resident's ordering-table helpers (gpu_ot_link_*.s, ot_link.s): the
 * entry receives the packet's 24-bit address and the packet's tag the entry's
 * previous contents ORed with its word count << 24 (the previous contents are
 * not masked). */
static void xem_gpu_ot_link(u_long *ot, void *packet, u32 words) {
    u_long previous = *ot;

    *ot = (u32)packet & 0xFFFFFF;
    *(u_long *)packet = previous | words << 24;
}

#define XEM_GPU_OT_LINK(name, words)       \
    void name(u_long *ot, void *packet) {  \
        xem_gpu_ot_link(ot, packet, words); \
    }

XEM_GPU_OT_LINK(gpu_ot_link_tile_1, 2)
XEM_GPU_OT_LINK(gpu_ot_link_tile_8, 2)
XEM_GPU_OT_LINK(gpu_ot_link_tile_16, 2)
XEM_GPU_OT_LINK(gpu_ot_link_tile, 3)
XEM_GPU_OT_LINK(gpu_ot_link_sprt_8, 3)
XEM_GPU_OT_LINK(gpu_ot_link_sprt_16, 3)
XEM_GPU_OT_LINK(gpu_ot_link_sprt, 4)
XEM_GPU_OT_LINK(gpu_ot_link_line_f2, 3)
XEM_GPU_OT_LINK(gpu_ot_link_line_g2, 4)
XEM_GPU_OT_LINK(gpu_ot_link_line_f3, 5)
XEM_GPU_OT_LINK(gpu_ot_link_line_g3, 7)
XEM_GPU_OT_LINK(gpu_ot_link_line_f4, 6)
XEM_GPU_OT_LINK(gpu_ot_link_line_g4, 9)
XEM_GPU_OT_LINK(gpu_ot_link_poly_f3, 4)
XEM_GPU_OT_LINK(gpu_ot_link_poly_ft3, 7)
XEM_GPU_OT_LINK(gpu_ot_link_poly_g3, 6)
XEM_GPU_OT_LINK(gpu_ot_link_poly_gt3, 9)
XEM_GPU_OT_LINK(gpu_ot_link_poly_f4, 5)
XEM_GPU_OT_LINK(gpu_ot_link_poly_ft4, 9)
XEM_GPU_OT_LINK(gpu_ot_link_poly_g4, 8)
XEM_GPU_OT_LINK(gpu_ot_link_11_words, 11)
