/*
 * libetc: interrupt callbacks and vertical sync (PsyQ intr.c). The host's
 * clock raises interrupts through xem_interrupt while the game is suspended,
 * so callbacks run at the game's waits, never inside its code. The counters
 * keep their original addresses.
 */
#include "common.h"
#include "psyq/libetc.h"

extern s32 libetc_vsync_count;
extern s32 libetc_vsync_previous_count;
extern s32 libetc_vsync_previous_counter1;

void xem_pad_vblank(void);
void xem_event_deliver(u32 class, u32 spec);

static void (*xem_vsync_callback)(void);
static void (*xem_interrupt_callbacks[11])(void);
static void (*xem_dma_callbacks[7])(void);

/* Initialise the callbacks once: libetc_interrupt_enabled stays set (it is
 * data, not BSS, so a soft reset keeps it too), and later calls, such as
 * ResetGraph(0)'s, change nothing. */
extern u16 libetc_interrupt_enabled;

int ResetCallback(void) {
    s32 i;

    if (libetc_interrupt_enabled) {
        return 0;
    }
    libetc_interrupt_enabled = 1;
    xem_vsync_callback = NULL;
    for (i = 0; i < 11; i++) {
        xem_interrupt_callbacks[i] = NULL;
    }
    for (i = 0; i < 7; i++) {
        xem_dma_callbacks[i] = NULL;
    }
    libetc_vsync_count = 0;
    libetc_vsync_previous_count = 0;
    return 0;
}

int VSyncCallback(void (*func)()) {
    void (*previous)(void) = xem_vsync_callback;

    xem_vsync_callback = (void (*)(void))func;
    return (int)previous;
}

void *InterruptCallback(int irq, void (*func)(void)) {
    void (*previous)(void) = xem_interrupt_callbacks[irq];

    xem_interrupt_callbacks[irq] = func;
    return (void *)previous;
}

void *DMACallback(int channel, void (*func)(void)) {
    void (*previous)(void) = xem_dma_callbacks[channel];

    xem_dma_callbacks[channel] = func;
    return (void *)previous;
}

/* Root counter 1 counts horizontal blanks: VSync reports their number since
 * its previous call. */
static u32 xem_hblank_counter(void) {
    return xem_host_rcnt_read(1);
}

/* Wait until the vertical blank count reaches `target`. */
static void xem_vsync_wait(s32 target) {
    while (libetc_vsync_count < target) {
        xem_host_yield(XEM_YIELD_VSYNC);
    }
}

/* mode < 0: the vertical blank count; 1: the horizontal blanks since the
 * previous call; otherwise wait until max(mode, 1) - 1 blanks have passed
 * since the previous call, then for the next blank, and return that count. */
int VSync(int mode) {
    u32 elapsed = (xem_hblank_counter() - libetc_vsync_previous_counter1) & 0xFFFF;

    if (mode < 0) {
        return libetc_vsync_count;
    }
    if (mode == 1) {
        return elapsed;
    }
    xem_vsync_wait(mode > 0 ? libetc_vsync_previous_count + mode - 1 : libetc_vsync_previous_count);
    xem_vsync_wait(libetc_vsync_count + 1);
    libetc_vsync_previous_count = libetc_vsync_count;
    libetc_vsync_previous_counter1 = xem_hblank_counter();
    return elapsed;
}

/* The interrupt handler: the host raises `irq` (XEM_IRQ_*; `detail` is the DMA
 * channel for XEM_IRQ_DMA). */
void xem_interrupt(u32 irq, u32 detail) {
    switch (irq) {
    case XEM_IRQ_VBLANK:
        libetc_vsync_count++;
        xem_pad_vblank();
        if (xem_interrupt_callbacks[XEM_IRQ_VBLANK] != NULL) {
            xem_interrupt_callbacks[XEM_IRQ_VBLANK]();
        }
        if (xem_vsync_callback != NULL) {
            xem_vsync_callback();
        }
        break;
    case XEM_IRQ_DMA:
        if (detail < 7 && xem_dma_callbacks[detail] != NULL) {
            xem_dma_callbacks[detail]();
        }
        break;
    case XEM_IRQ_RCNT0:
    case XEM_IRQ_RCNT1:
    case XEM_IRQ_RCNT2:
        /* The BIOS delivers root counter interrupts as events. */
        xem_event_deliver(0xF2000000 | (irq - XEM_IRQ_RCNT0), 0x2);
        if (xem_interrupt_callbacks[irq] != NULL) {
            xem_interrupt_callbacks[irq]();
        }
        break;
    default:
        if (irq < 11 && xem_interrupt_callbacks[irq] != NULL) {
            xem_interrupt_callbacks[irq]();
        }
        break;
    }
}
