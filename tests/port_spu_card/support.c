/* What port/libspu.c, port/libcard.c and port/bios.c take from the game and
 * the host, as test-owned objects that tests/test_port_spu_card.py drives:
 * libspu's state at its original addresses, a register window standing in for
 * 0x1F801C00, and host imports and libapi/libetc calls that record what they
 * were asked. The "SPU" behind the window is the minimum libspu's waits
 * read back: a sync clears the write-triggered KON and KOFF, after recording
 * the window as the hardware saw it. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef short s16;

/* The register window and libspu's state. */
u16 test_window[0x200];
u16 *libspu_spu_registers = test_window;
s32 libspu_transfer_event;
s32 libspu_voice_key_on_mask;
s32 libspu_transfer_mode;
s32 libspu_reverb_on;
s32 libspu_reverb_work_area_reserved;
s32 libspu_reverb_work_area_start;
s32 libspu_reverb_mode_type;
s16 libspu_reverb_depth_left;
s16 libspu_reverb_depth_right;
s32 libspu_reverb_delay;
s32 libspu_reverb_feedback;
s32 libspu_init_word;
/* libspu_voice_attributes (a word) is followed by 24 halfwords ending at
 * libspu_voice_attribute_end; the test keeps them in one array. */
u16 test_voice_attributes[2 + 24];
s32 libspu_voice_attributes;
u8 libspu_reverb_clear_buffer[0x400];
s32 libspu_init_word_b;
s32 libspu_started;
u16 libspu_transfer_start_address;
s32 libspu_transfer_by_io;
s32 libspu_init_word_c;
s32 libspu_memory_shift_unit;
s32 libspu_memory_shift;
s32 libspu_memory_unit;
s32 libspu_memory_unit_mask;
s32 libspu_transfer_idle;
void (*libspu_transfer_callback)(void);
void (*libspu_irq_callback)(void);
u8 libspu_voice_init_bytes[0x10];
s32 libspu_transfer_is_read;
u32 libspu_transfer_ram_address;
u32 libspu_transfer_block_count;
s32 libspu_malloc_table;
s32 libspu_malloc_count;
u32 *libspu_malloc_reserved_top;
s32 libspu_reverb_work_area_starts[10];
u16 libspu_commons[10];
struct {
    u32 mask;
    u16 regs[32];
} libspu_reverb_mode_parameters[10];
void *console_current;

/* libspu_voice_attribute_end is the last of the halfwords. */
__asm__(".globl libspu_voice_attribute_end\n.set libspu_voice_attribute_end, test_voice_attributes + 50");

/* Recorded host calls. */
u16 test_syncs[64][0x200];
int test_sync_count;
u32 test_transfers[16][4];
int test_transfer_count;
u32 test_write_io[16][3];
int test_write_io_count;
int test_yields;
int test_card_busy_polls;
u32 test_debug_breaks[16];
int test_debug_break_count;
u32 test_events[16][2];
int test_event_count;
long test_open_event[4];
void *test_dma_callback[8];
void *test_interrupt_callback[16];
int test_callback_calls;
int test_test_event_ready;

void xem_host_spu_sync(void) {
    int i;

    if (test_sync_count < 64) {
        for (i = 0; i < 0x200; i++) {
            test_syncs[test_sync_count][i] = test_window[i];
        }
    }
    test_sync_count++;
    test_window[0x188 / 2] = test_window[0x18A / 2] = 0;
    test_window[0x18C / 2] = test_window[0x18E / 2] = 0;
}

void xem_host_spu_transfer(unsigned int spu_address, void *ram, unsigned int bytes, unsigned int direction) {
    u32 *t = test_transfers[test_transfer_count++ & 15];

    t[0] = spu_address;
    t[1] = (u32)((u8 *)ram - (u8 *)0);
    t[2] = bytes;
    t[3] = direction;
}

void xem_host_spu_write_io(unsigned int spu_address, void *data, unsigned int bytes) {
    u32 *t = test_write_io[test_write_io_count++ & 15];

    t[0] = spu_address;
    t[1] = data == libspu_voice_init_bytes;
    t[2] = bytes;
}

void test_callback(void) { test_callback_calls++; }

/* libetc and libapi. */
int ResetCallback(void) { return 0; }
void *DMACallback(int channel, void (*func)(void)) {
    void *previous = test_dma_callback[channel];
    test_dma_callback[channel] = (void *)func;
    return previous;
}
void *InterruptCallback(int irq, void (*func)(void)) {
    void *previous = test_interrupt_callback[irq];
    test_interrupt_callback[irq] = (void *)func;
    return previous;
}
long EnterCriticalSection(void) { return 1; }
void ExitCriticalSection(void) {}
long OpenEvent(unsigned int desc, int spec, int mode, int (*func)()) {
    test_open_event[0] = desc;
    test_open_event[1] = spec;
    test_open_event[2] = mode;
    return 0xF1000003;
}
long EnableEvent(int event) { return 1; }
long DisableEvent(int event) { return 1; }
long CloseEvent(int event) { return 1; }
long TestEvent(int event) {
    if (test_test_event_ready > 0) {
        test_test_event_ready--;
        return 1;
    }
    return 0;
}
void DeliverEvent(unsigned int class, unsigned int spec) {
    u32 *e = test_events[test_event_count++ & 15];
    e[0] = class;
    e[1] = spec;
}
void ChangeClearPAD(int val) {}
int console_vprintf(int target, const char *format, void *args) { return 0; }

void xem_host_yield(int reason) {
    test_yields++;
    test_test_event_ready = 1;
}
void xem_host_debug_break(unsigned int code) { test_debug_breaks[test_debug_break_count++ & 15] = code; }

/* The card service: a fixed answer per call, busy for two polls after each. */
int test_card_busy;
int test_card_result = 7;
void xem_host_card_init(int pad_enable) {}
void xem_host_card_start(void) {}
void xem_host_card_stop(void) {}
void xem_host_card_bu_init(void) {}
int xem_host_card_info(int channel) { return channel == 0; }
static int busy_result(void) {
    test_card_busy = 2;
    return test_card_result;
}
int xem_host_card_open(char *name, unsigned int flags) { return busy_result(); }
int xem_host_card_read(int fd, void *buffer, int length) { return busy_result(); }
int xem_host_card_write(int fd, void *buffer, int length) { return busy_result(); }
int xem_host_card_close(int fd) { return busy_result(); }
int xem_host_card_firstfile(char *pattern, void *entry) { return busy_result(); }
int xem_host_card_nextfile(void *entry) { return busy_result(); }
int xem_host_card_rename(char *from, char *to) { return busy_result(); }
int xem_host_card_delete(char *name) { return busy_result(); }
int xem_host_card_format(char *device) { return busy_result(); }
int xem_host_card_busy(void) {
    test_card_busy_polls++;
    if (test_card_busy > 0) {
        test_card_busy--;
        return 1;
    }
    return 0;
}
