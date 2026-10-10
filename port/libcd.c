/*
 * libcd (PsyQ bios.c and sys.c, "$Id: bios.c,v 1.86 1997/03/28"): commands,
 * their interrupts and callbacks, sector transfers and positions, over the
 * host's CD controller (xem/cd.h, runtime/crates/xem-core/src/devices/cd.rs).
 *
 * The library's state keeps its original addresses, so its tables (which
 * commands take a Setloc, have a second response, reset the ready status or
 * update the status byte, and their parameter counts) are read from the
 * image. The interrupt handler `callback` runs from the port's interrupt
 * dispatch (InterruptCallback(XEM_IRQ_CDROM, callback)): data-ready
 * interrupts call the ready callback with the ready status and result bytes,
 * completions and errors the sync callback. Waits that the original spins
 * yield to the host's clock (XEM_YIELD_POLL); inside an interrupt handler,
 * where the original polls the controller itself (CheckCallback), the host
 * runs the drive's next event at once instead.
 *
 * Differences that cannot be observed by the game: the debug prints
 * (CdSetDebug(0) keeps them off; mode_empty_debug_print prints nothing), the
 * timeout's poll count (only the 960-vblank deadline is kept: a poll here is
 * a wait for the next interrupt) and DMA timing (sector transfers end at
 * once).
 */
#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "xem/port.h"
#include "xem/cd.h"

void *InterruptCallback(int irq, void (*func)(void));
void *DMACallback(int channel, void (*func)(void));
void DeliverEvent(unsigned long class, unsigned long spec);
void xem_interrupt(u32 irq, u32 detail);
void CD_flush(void);

/* Interrupt types and sync statuses (CdlDataReady...CdlDiskError). */
enum { CD_DATA_READY = 1, CD_COMPLETE = 2, CD_ACKNOWLEDGE = 3, CD_DATA_END = 4, CD_DISK_ERROR = 5 };

extern s32 libcd_command_needs_setloc_table[32];
extern s32 libcd_command_has_second_response_table[32];
extern s32 libcd_command_resets_ready_table[32];
extern s32 libcd_command_updates_status_table[32];
extern s32 libcd_command_parameter_counts[32];
extern char *libcd_command_name_table[32];
extern char *libcd_interrupt_name_table[8];
extern char libcd_command_and_interrupt_names[];
extern CdlCB libcd_sync_callback;
extern CdlCB libcd_ready_callback;
extern s32 libcd_debug_level;
extern s32 libcd_last_status;
extern s32 libcd_last_response_byte1;
extern s32 libcd_shell_open_count;
extern CdlLOC libcd_last_position;
extern u8 libcd_last_mode;
extern u8 libcd_last_command;
extern u8 libcd_sync_status;
extern u8 libcd_ready_status;
extern u8 libcd_data_end_status;
extern u8 libcd_sync_result[8];
extern u8 libcd_ready_result[8];
extern u8 libcd_data_end_result[8];
extern s32 libcd_timeout_deadline;
extern s32 libcd_timeout_poll_count;
extern char *libcd_timeout_name;
extern char libcd_cd_sync_name[];
extern char libcd_cd_ready_name[];
extern char libcd_cd_cw_name_and_version[];
extern char libcd_cd_datasync_name[];
extern CdlCB libcd_read_callback;
extern s32 libcd_read_transfer_mode;

/* Set while any interrupt handler runs (libetc.c): no wait can suspend. */
extern s32 xem_in_interrupt;

/* Nesting of the CD interrupt handler, and a sector transfer it made whose
 * DMA completion interrupt follows the handler. */
static s32 libcd_interrupt_depth;
static s32 libcd_dma_done;

/* The DMA3 completion interrupt, served now (the dispatcher's next one). */
static void libcd_dma_interrupt(void) {
    s32 in_interrupt = xem_in_interrupt;

    xem_interrupt(XEM_IRQ_DMA, 3);
    xem_in_interrupt = in_interrupt;
}

/* The controller's pending interrupt: its kind for the callbacks (bit 2:
 * ready callback, bit 1: sync callback), or 0 when there is none. */
s32 getintr(void) {
    u8 result[8];
    u32 taken = xem_host_cd_interrupt(result);
    s32 intr = taken & 0xFF;
    s32 count = taken >> 8;
    s32 error = 0;
    s32 i;

    if (intr == 0) {
        return 0;
    }
    if (intr != CD_ACKNOWLEDGE || libcd_command_updates_status_table[libcd_last_command]) {
        if (!(libcd_last_status & 0x10) && (result[0] & 0x10)) {
            libcd_shell_open_count++;
        }
        libcd_last_status = result[0];
        libcd_last_response_byte1 = result[1];
        error = result[0] & 0x1D;
    }
    switch (intr) {
    case CD_ACKNOWLEDGE:
        if (error) {
            libcd_sync_status = CD_DISK_ERROR;
        } else if (libcd_command_has_second_response_table[libcd_last_command]) {
            libcd_sync_status = CD_ACKNOWLEDGE;
            for (i = 0; i < 8; i++) {
                libcd_sync_result[i] = result[i];
            }
            return 1;
        } else {
            libcd_sync_status = CD_COMPLETE;
        }
        for (i = 0; i < 8; i++) {
            libcd_sync_result[i] = result[i];
        }
        return 2;
    case CD_COMPLETE:
        libcd_sync_status = error ? CD_DISK_ERROR : CD_COMPLETE;
        for (i = 0; i < 8; i++) {
            libcd_sync_result[i] = result[i];
        }
        return 2;
    case CD_DATA_READY:
        /* A status-only response does not report its error bits. */
        if (error && count == 1) {
            error = 0;
        }
        libcd_ready_status = error ? CD_DISK_ERROR : CD_DATA_READY;
        for (i = 0; i < 8; i++) {
            libcd_ready_result[i] = result[i];
        }
        return 4;
    case CD_DATA_END:
        libcd_data_end_status = CD_DATA_END;
        libcd_ready_status = CD_DATA_END;
        for (i = 0; i < 8; i++) {
            libcd_data_end_result[i] = result[i];
            libcd_ready_result[i] = result[i];
        }
        return 4;
    case CD_DISK_ERROR:
        libcd_ready_status = CD_DISK_ERROR;
        libcd_sync_status = CD_DISK_ERROR;
        for (i = 0; i < 8; i++) {
            libcd_sync_result[i] = result[i];
            libcd_ready_result[i] = result[i];
        }
        return 6;
    }
    return 0;
}

/* Take every pending interrupt and run its callbacks. */
static void libcd_dispatch(void) {
    s32 kind;

    while ((kind = getintr()) != 0) {
        if ((kind & 4) && libcd_ready_callback != NULL) {
            libcd_ready_callback(libcd_ready_status, libcd_ready_result);
        }
        if ((kind & 2) && libcd_sync_callback != NULL) {
            libcd_sync_callback(libcd_sync_status, libcd_sync_result);
        }
    }
}

/* The CD interrupt handler. A sector transfer its callbacks made completes
 * a DMA interrupt, which the dispatcher serves next: the data callback. */
void callback(void) {
    libcd_interrupt_depth++;
    libcd_dispatch();
    libcd_interrupt_depth--;
    if (libcd_interrupt_depth == 0 && libcd_dma_done) {
        libcd_dma_done = 0;
        libcd_dma_interrupt();
    }
}

/* One turn of a libcd wait: let time pass to the next interrupt. Inside
 * the interrupt handler, where the original polls the controller itself
 * (CheckCallback) and the clock cannot move, the drive's next event happens
 * at once; returns 0 when the drive has none, so nothing can come. */
static s32 libcd_wait(void) {
    if (xem_in_interrupt) {
        if (!xem_host_cd_poll_now()) {
            return 0;
        }
        libcd_dispatch();
        return 1;
    }
    xem_host_yield(XEM_YIELD_POLL);
    return 1;
}

/* Whether a wait ran past its 960 vblanks: then the controller is flushed. */
static s32 libcd_timed_out(void) {
    if (VSync(-1) > libcd_timeout_deadline) {
        CD_flush();
        return 1;
    }
    return 0;
}

static void libcd_start_timeout(char *name) {
    libcd_timeout_deadline = VSync(-1) + 960;
    libcd_timeout_poll_count = 0;
    libcd_timeout_name = name;
}

static void libcd_copy_result(u8 *to, u8 *from) {
    s32 i;

    if (to != NULL) {
        for (i = 0; i < 8; i++) {
            to[i] = from[i];
        }
    }
}

/* The command's completion (mode 0 waits): CdlComplete or CdlDiskError,
 * after which the status reads CdlComplete again; 0 while it is pending
 * (mode 1); -1 on a timeout. */
s32 CD_sync(s32 mode, u8 *result) {
    s32 status;

    libcd_start_timeout(libcd_cd_sync_name);
    for (;;) {
        if (libcd_timed_out()) {
            return -1;
        }
        if (xem_in_interrupt) {
            libcd_dispatch();
        }
        status = libcd_sync_status;
        if (status == CD_COMPLETE || status == CD_DISK_ERROR) {
            libcd_sync_status = CD_COMPLETE;
            libcd_copy_result(result, libcd_sync_result);
            return status;
        }
        if (mode != 0) {
            return 0;
        }
        if (!libcd_wait()) {
            CD_flush();
            return -1;
        }
    }
}

/* The next data-ready (or data-end) report, consumed: its status, 0 when
 * none is waiting (mode 1), -1 on a timeout. */
s32 CD_ready(s32 mode, u8 *result) {
    s32 status;

    libcd_start_timeout(libcd_cd_ready_name);
    for (;;) {
        if (libcd_timed_out()) {
            return -1;
        }
        if (xem_in_interrupt) {
            libcd_dispatch();
        }
        status = libcd_data_end_status;
        if (status != 0) {
            libcd_data_end_status = 0;
            libcd_copy_result(result, libcd_data_end_result);
            return status;
        }
        status = libcd_ready_status;
        if (status != 0) {
            libcd_ready_status = 0;
            libcd_copy_result(result, libcd_ready_result);
            return status;
        }
        if (mode != 0) {
            return 0;
        }
        if (!libcd_wait()) {
            CD_flush();
            return -1;
        }
    }
}

/* Send a command: after the previous one completed, with its parameters;
 * `async` returns at once, otherwise wait for the first response. Returns
 * 0, -1 for an error response or a timeout, -2 for missing parameters. */
s32 CD_cw(u8 com, u8 *param, u8 *result, s32 async) {
    s32 i;

    if (libcd_command_parameter_counts[com] != 0 && param == NULL) {
        return -2;
    }
    CD_sync(0, NULL);
    if (com == CdlSetloc) {
        for (i = 0; i < 4; i++) {
            ((u8 *)&libcd_last_position)[i] = param[i];
        }
    }
    if (com == 0x0E) {
        libcd_last_mode = param[0];
    }
    libcd_sync_status = 0;
    if (libcd_command_resets_ready_table[com]) {
        libcd_ready_status = 0;
    }
    libcd_last_command = com;
    xem_host_cd_command(com, param, libcd_command_parameter_counts[com]);
    if (async) {
        return 0;
    }
    libcd_start_timeout(libcd_cd_cw_name_and_version);
    while (libcd_sync_status == 0) {
        if (libcd_timed_out()) {
            return -1;
        }
        if (!libcd_wait()) {
            CD_flush();
            return -1;
        }
    }
    libcd_copy_result(result, libcd_sync_result);
    return -(libcd_sync_status == CD_DISK_ERROR);
}

/* Acknowledge everything pending and reset the statuses. */
void CD_flush(void) {
    xem_host_cd_flush();
    libcd_data_end_status = 0;
    libcd_ready_status = 0;
    libcd_sync_status = CD_COMPLETE;
}

/* The handler alone (CdReset(2)). */
s32 CD_initintr(void) {
    libcd_ready_callback = NULL;
    libcd_sync_callback = NULL;
    libcd_last_response_byte1 = 0;
    libcd_last_status = 0;
    ResetCallback();
    InterruptCallback(XEM_IRQ_CDROM, callback);
    return 0;
}

/* Reset the library and the drive: Nop (twice with the lid open), Init,
 * Demute. Returns 0, or -1. */
s32 CD_init(void) {
    libcd_last_command = 0;
    libcd_last_mode = 0;
    libcd_ready_callback = NULL;
    libcd_sync_callback = NULL;
    libcd_last_response_byte1 = 0;
    libcd_last_status = 0;
    ResetCallback();
    InterruptCallback(XEM_IRQ_CDROM, callback);
    CD_flush();
    CD_cw(CdlNop, NULL, NULL, 0);
    if (libcd_last_status & 0x10) {
        CD_cw(CdlNop, NULL, NULL, 0);
    }
    if (CD_cw(0x0A, NULL, NULL, 0) != 0) {
        return -1;
    }
    if (CD_cw(0x0C, NULL, NULL, 0) != 0) {
        return -1;
    }
    return -(CD_sync(0, NULL) != CD_COMPLETE);
}

/* The CD audio attenuation. */
s32 CD_vol(CdlATV *vol) {
    xem_host_cd_mix(vol);
    return 0;
}

/* The SPU's CD input and the attenuation at full volume: the main volume
 * when it is still zero, the CD volume, SPU and CD audio enabled
 * (SPUCNT 0xC001), left to left and right to right. */
s32 CD_initvol(void) {
    volatile u16 *spu = (volatile u16 *)0x1F801C00;
    CdlATV vol;

    if (spu[0x1B8 / 2] == 0 && spu[0x1BA / 2] == 0) {
        spu[0x180 / 2] = 0x3FFF;
        spu[0x182 / 2] = 0x3FFF;
    }
    spu[0x1B0 / 2] = 0x3FFF;
    spu[0x1B2 / 2] = 0x3FFF;
    spu[0x1AA / 2] = 0xC001;
    vol.val0 = 0x80;
    vol.val1 = 0;
    vol.val2 = 0x80;
    vol.val3 = 0;
    xem_host_cd_mix(&vol);
    return 0;
}

/* DMA3 not busy: transfers end at once. */
s32 CD_datasync(s32 mode) {
    libcd_start_timeout(libcd_cd_datasync_name);
    return 0;
}

/* A transfer from the sector buffer (DMA3) and, when `interrupt`, its
 * completion interrupt: after the CD handler when the handler made it,
 * else at once. libcd's own transfers interrupt (DMACallback(3) enables
 * it); the stream library's sets it per transfer (libcd_stream.c). */
void xem_cd_transfer(void *madr, s32 words, s32 interrupt) {
    xem_host_cd_get_sector(madr, words);
    if (!interrupt) {
        return;
    }
    if (libcd_interrupt_depth > 0) {
        libcd_dma_done = 1;
    } else {
        libcd_dma_interrupt();
    }
}

s32 CD_getsector(void *madr, s32 size) {
    xem_cd_transfer(madr, size, 1);
    return 0;
}

s32 CD_getsector2(void *madr, s32 size) {
    xem_cd_transfer(madr, size, 1);
    return 0;
}

/* The default callbacks: deliver the CD events (HwCdRom). */
void def_cbsync(u8 intr, u8 *result) {
    DeliverEvent(0xF0000003, 0x20);
}

void def_cbready(u8 intr, u8 *result) {
    DeliverEvent(0xF0000003, 0x40);
}

void def_cbread(u8 intr, u8 *result) {
    DeliverEvent(0xF0000003, 0x40);
}

/* Reset: 2 the handler only; else the drive, and for 1 the volumes too.
 * Returns 1, or 0 when the drive did not answer. */
s32 CdReset(s32 mode) {
    if (mode == 2) {
        CD_initintr();
        return 1;
    }
    if (CD_init() != 0) {
        return 0;
    }
    if (mode == 1 && CD_initvol() != 0) {
        return 0;
    }
    return 1;
}

/* Up to four resets; then the default callbacks and read mode. */
int CdInit(void) {
    s32 i;

    for (i = 4; i > 0; i--) {
        if (CdReset(1) == 1) {
            CdSyncCallback(def_cbsync);
            CdReadyCallback(def_cbready);
            CdReadCallback(def_cbread);
            CdReadMode(0);
            return 1;
        }
    }
    return 0;
}

int CdStatus(void) {
    return (u8)libcd_last_status;
}

int CdMode(void) {
    return libcd_last_mode;
}

int CdLastCom(void) {
    return libcd_last_command;
}

CdlLOC *CdLastPos(void) {
    return &libcd_last_position;
}

void CdFlush(void) {
    CD_flush();
}

int CdSetDebug(int level) {
    s32 previous = libcd_debug_level;

    libcd_debug_level = level;
    return previous;
}

char *CdComstr(u8 com) {
    if (com < 0x1C) {
        return libcd_command_name_table[com];
    }
    return libcd_command_and_interrupt_names;
}

char *CdIntstr(u8 intr) {
    if (intr < 7) {
        return libcd_interrupt_name_table[intr];
    }
    return libcd_command_and_interrupt_names;
}

int CdSync(int mode, u_char *result) {
    return CD_sync(mode, result);
}

int CdReady(int mode, u_char *result) {
    return CD_ready(mode, result);
}

CdlCB CdSyncCallback(CdlCB func) {
    CdlCB previous = libcd_sync_callback;

    libcd_sync_callback = func;
    return previous;
}

CdlCB CdReadyCallback(CdlCB func) {
    CdlCB previous = libcd_ready_callback;

    libcd_ready_callback = func;
    return previous;
}

CdlCB CdReadCallback(CdlCB func) {
    CdlCB previous = libcd_read_callback;

    libcd_read_callback = func;
    return previous;
}

int CdReadMode(int mode) {
    s32 previous = libcd_read_transfer_mode;

    libcd_read_transfer_mode = mode;
    return previous;
}

/* Up to four tries: a Nop first when the lid was open, a Setloc first for a
 * command that takes a position, then the command, waited for (`async` 0)
 * or not. The sync callback is off during the extra commands. Returns
 * whether the command was accepted. */
static s32 libcd_control(u8 com, u8 *param, u8 *result, s32 async) {
    CdlCB saved = libcd_sync_callback;
    s32 tries;

    for (tries = 4; tries > 0; tries--) {
        libcd_sync_callback = NULL;
        if (com != CdlNop && (libcd_last_status & 0x10)) {
            CD_cw(CdlNop, NULL, NULL, 0);
        }
        if (param == NULL || libcd_command_needs_setloc_table[com] == 0 ||
            CD_cw(CdlSetloc, param, async ? NULL : result, 0) == 0) {
            libcd_sync_callback = saved;
            if (CD_cw(com, param, async ? NULL : result, async) == 0) {
                return 1;
            }
        }
    }
    libcd_sync_callback = saved;
    return 0;
}

int CdControl(u_char com, u_char *param, u_char *result) {
    return libcd_control(com, param, result, 0);
}

int CdControlF(u_char com, u_char *param) {
    return libcd_control(com, param, NULL, 1);
}

/* CdControl, then wait for the command's completion. */
int CdControlB(u_char com, u_char *param, u_char *result) {
    if (!libcd_control(com, param, result, 0)) {
        return 0;
    }
    return CD_sync(0, result) == CD_COMPLETE;
}

int CdMix(CdlATV *vol) {
    CD_vol(vol);
    return 1;
}

int CdGetSector(void *madr, int size) {
    return CD_getsector(madr, size) == 0;
}

int CdGetSector2(void *madr, int size) {
    return CD_getsector2(madr, size) == 0;
}

void (*CdDataCallback(void (*func)()))() {
    return DMACallback(3, (void (*)(void))func);
}

int CdDataSync(int mode) {
    return CD_datasync(mode);
}

/* Sector number to BCD minute, second and sector (two seconds of lead-in). */
CdlLOC *CdIntToPos(int i, CdlLOC *p) {
    s32 absolute = i + 150;
    s32 seconds = absolute / 75;
    s32 sector = absolute - seconds * 75;
    s32 minutes = seconds / 60;

    seconds -= minutes * 60;
    p->sector = (sector / 10) * 16 + sector % 10;
    p->second = (seconds / 10) * 16 + seconds % 10;
    p->minute = (minutes / 10) * 16 + minutes % 10;
    return p;
}

int CdPosToInt(CdlLOC *p) {
    return ((((p->minute >> 4) * 10 + (p->minute & 0xF)) * 60 + (p->second >> 4) * 10 + (p->second & 0xF)) * 75 +
            (p->sector >> 4) * 10 + (p->sector & 0xF)) -
           150;
}
