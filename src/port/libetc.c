/*
 * libetc: interrupt callbacks and vertical sync (PsyQ intr.c). The host's
 * clock raises interrupts through xem_interrupt while the game is suspended,
 * so callbacks run at the game's waits, never inside its code. The counters
 * keep their original addresses.
 */
#include "common.h"
#include "psyq/libetc.h"
#include "xem/card.h"
#include "xem/spu.h"

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
        xem_wait(XEM_YIELD_VSYNC);
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
/* Set while an interrupt handler runs: it cannot suspend (xem_loop_poll). */
s32 xem_in_interrupt;

/* The interrupt mask (I_MASK): a masked interrupt stays pending, as I_STAT
 * holds it, and runs when SetIntrMask enables it again (libgpu masks every
 * interrupt while it runs its queue). */
static u32 xem_interrupt_mask = 0xFFFFFFFF;
static u32 xem_pending_interrupts;
static u32 xem_pending_dma_channels;

static void xem_dispatch(u32 irq, u32 detail);

static void xem_run_pending(void) {
    u32 irq, channel;

    for (irq = 0; irq < 11; irq++) {
        if (!(xem_pending_interrupts & xem_interrupt_mask & (1 << irq))) {
            continue;
        }
        xem_pending_interrupts &= ~(1 << irq);
        if (irq == XEM_IRQ_DMA) {
            for (channel = 0; channel < 7; channel++) {
                if (xem_pending_dma_channels & (1 << channel)) {
                    xem_pending_dma_channels &= ~(1 << channel);
                    xem_dispatch(irq, channel);
                }
            }
        } else {
            xem_dispatch(irq, 0);
        }
    }
}

int SetIntrMask(int mask) {
    u32 previous = xem_interrupt_mask;

    xem_interrupt_mask = mask;
    if (xem_pending_interrupts & xem_interrupt_mask) {
        xem_run_pending();
    }
    return previous;
}

void xem_interrupt(u32 irq, u32 detail) {
    if (!(xem_interrupt_mask & (1 << irq))) {
        xem_pending_interrupts |= 1 << irq;
        if (irq == XEM_IRQ_DMA) {
            xem_pending_dma_channels |= 1 << detail;
        }
        return;
    }
    xem_dispatch(irq, detail);
}

static void xem_dispatch(u32 irq, u32 detail) {
    s32 nested = xem_in_interrupt;

    xem_in_interrupt = 1;
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
        if (detail == 4 && xem_dma_callbacks[4] == (void (*)(void))XEM_LIBSPU_TRANSFER_WAIT_ENTRY) {
            /* libspu's own entry, which the port implements. */
            xem_spu_transfer_done();
        } else if (detail < 7 && xem_dma_callbacks[detail] != NULL) {
            xem_dma_callbacks[detail]();
        }
        break;
    case XEM_IRQ_PAD:
        /* The memory card shares the controller port's interrupt. */
        xem_card_interrupt(detail);
        if (xem_interrupt_callbacks[irq] != NULL) {
            xem_interrupt_callbacks[irq]();
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
    xem_in_interrupt = nested;
}
