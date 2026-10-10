/*
 * libapi: the BIOS calls the game makes for events, root counters, critical
 * sections and the pad driver. The BIOS is not part of the images; this is
 * its documented behaviour over the host's clock and pads.
 */
#include "common.h"
#include "psyq/libapi.h"

/* BIOS events: class (e.g. 0xF2000002, root counter 2), spec (0x2 interrupt),
 * mode (0x1000 calls the handler, 0x2000 marks the event ready). */
typedef struct {
    u32 class;
    u32 spec;
    u32 mode;
    long (*handler)();
    s32 open;
    s32 enabled;
    s32 ready;
} XemEvent;

#define XEM_EVENT_COUNT 16
#define XEM_EVENT_ID(index) (0xF1000000 | (index))

static XemEvent xem_events[XEM_EVENT_COUNT];
static s32 xem_critical;

long OpenEvent(unsigned long desc, long spec, long mode, long (*func)()) {
    s32 i;

    for (i = 0; i < XEM_EVENT_COUNT; i++) {
        if (!xem_events[i].open) {
            xem_events[i].class = desc;
            xem_events[i].spec = spec;
            xem_events[i].mode = mode;
            xem_events[i].handler = func;
            xem_events[i].open = 1;
            xem_events[i].enabled = 0;
            xem_events[i].ready = 0;
            return XEM_EVENT_ID(i);
        }
    }
    return -1;
}

static XemEvent *xem_event(long event) {
    u32 index = (u32)event & 0xFFFF;

    if (((u32)event & 0xFFFF0000) != 0xF1000000 || index >= XEM_EVENT_COUNT || !xem_events[index].open) {
        return NULL;
    }
    return &xem_events[index];
}

long CloseEvent(long event) {
    XemEvent *e = xem_event(event);

    if (e == NULL) {
        return 0;
    }
    e->open = 0;
    return 1;
}

long EnableEvent(long event) {
    XemEvent *e = xem_event(event);

    if (e == NULL) {
        return 0;
    }
    e->enabled = 1;
    return 1;
}

long DisableEvent(long event) {
    XemEvent *e = xem_event(event);

    if (e == NULL) {
        return 0;
    }
    e->enabled = 0;
    return 1;
}

/* Whether the event happened since the last test (ready mode). */
long TestEvent(long event) {
    XemEvent *e = xem_event(event);

    if (e != NULL && e->ready) {
        e->ready = 0;
        return 1;
    }
    return 0;
}

/* Deliver (class, spec) to every enabled event waiting for it. */
void xem_event_deliver(u32 class, u32 spec) {
    s32 i;

    for (i = 0; i < XEM_EVENT_COUNT; i++) {
        XemEvent *e = &xem_events[i];

        if (!e->open || !e->enabled || e->class != class || !(e->spec & spec)) {
            continue;
        }
        if (e->mode == 0x2000) {
            e->ready = 1;
        } else if (e->handler != NULL) {
            e->handler();
        }
    }
}

void DeliverEvent(unsigned long class, unsigned long spec) {
    xem_event_deliver(class, spec);
}

void UnDeliverEvent(unsigned long class, unsigned long spec) {
    s32 i;

    for (i = 0; i < XEM_EVENT_COUNT; i++) {
        if (xem_events[i].open && xem_events[i].class == class && (xem_events[i].spec & spec)) {
            xem_events[i].ready = 0;
        }
    }
}

/* Interrupts reach the game only at its waits, so a critical section only
 * reports whether one was open. */
s32 xem_in_critical_section(void) {
    return xem_critical;
}

long EnterCriticalSection(void) {
    s32 was_open = !xem_critical;

    xem_critical = 1;
    return was_open;
}

void ExitCriticalSection(void) {
    xem_critical = 0;
}

void SwEnterCriticalSection(void) {
    xem_critical = 1;
}

void SwExitCriticalSection(void) {
    xem_critical = 0;
}

/* The port's code is not in game memory; patched instructions are read from
 * game memory where it matters (model_set_envmap_mapping). */
void FlushCache(void) {
}

/* Root counters: `spec` is 0xF2000000 | counter. */
long SetRCnt(unsigned long spec, unsigned short target, long mode) {
    xem_host_rcnt_set(spec & 3, target, mode);
    return 1;
}

long GetRCnt(unsigned long spec) {
    return xem_host_rcnt_read(spec & 3);
}

long StartRCnt(unsigned long spec) {
    xem_host_rcnt_start(spec & 3);
    return 1;
}

long StopRCnt(unsigned long spec) {
    xem_host_rcnt_stop(spec & 3);
    return 1;
}

long ResetRCnt(unsigned long spec) {
    xem_host_rcnt_set(spec & 3, 0xFFFF, 0);
    return 1;
}

/* The BIOS pad driver: after InitPAD and StartPAD it fills both receive
 * buffers at every vertical blank, before the game's callbacks run. */
static char *xem_pad_buffers[2];
static long xem_pad_lengths[2];
static s32 xem_pad_started;

void InitPAD(char *bufA, long lenA, char *bufB, long lenB) {
    xem_pad_buffers[0] = bufA;
    xem_pad_lengths[0] = lenA;
    xem_pad_buffers[1] = bufB;
    xem_pad_lengths[1] = lenB;
}

int StartPAD(void) {
    xem_pad_started = 1;
    return 1;
}

void StopPAD(void) {
    xem_pad_started = 0;
}

/* Whether the BIOS acknowledges the pad interrupt itself; nothing to do. */
void ChangeClearPAD(long val) {
}

/* The send (actuator) buffers the BIOS transmits with each poll. */
static unsigned char *xem_pad_send_buffers[2];

void libapi_register_pad_send_buffers(unsigned char *data0, long size0, unsigned char *data1, long size1) {
    xem_pad_send_buffers[0] = data0;
    xem_pad_send_buffers[1] = data1;
}

void xem_pad_vblank(void) {
    s32 port;

    if (!xem_pad_started) {
        return;
    }
    for (port = 0; port < 2; port++) {
        if (xem_pad_buffers[port] != NULL) {
            xem_host_pad_read(port, xem_pad_buffers[port], xem_pad_lengths[port]);
        }
    }
}
