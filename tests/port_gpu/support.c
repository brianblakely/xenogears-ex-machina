/* A recording stand-in for the host's GPU device and the run loop, for the
 * port's libgpu built natively (tests/test_port_gpu.py). libgpu's data lives
 * at fixed addresses below 16 MB (the test maps that arena and links the
 * symbols there), so the 24-bit addresses libgpu writes into packets and
 * passes to DMA are the objects' own addresses, as in game memory.
 *
 * Each GPU access is logged as (kind, value): GP0 words the CPU sends, GP1
 * words, DMA2 starts (MADR, BCR, CHCR as three entries) and the GP0 words a
 * linked-list DMA delivers. A started transfer is busy until libgpu yields;
 * the yield ends it and calls the DMA2 callback, as the runtime delivers the
 * completion interrupt at the game's next wait. */
enum { LOG_GP0, LOG_GP1, LOG_DMA, LOG_LIST_GP0 };

#define LOG_LENGTH 4096

unsigned int test_log_kind[LOG_LENGTH];
unsigned int test_log_value[LOG_LENGTH];
int test_log_length;
unsigned int test_status = 0x14000000; /* ready for commands and DMA */
unsigned int test_param[8];            /* GP1(10h) answers */
unsigned int test_read_words[64];      /* GPUREAD after GP0(C0h) */
int test_read_index;
unsigned int test_dma_control;
int test_yields;
int test_draw_sync_callbacks;
int test_reset_callbacks;
void (*test_dma_callbacks[7])(void);
static unsigned int test_gpuread;

static void test_log(unsigned int kind, unsigned int value) {
    if (test_log_length < LOG_LENGTH) {
        test_log_kind[test_log_length] = kind;
        test_log_value[test_log_length] = value;
        test_log_length++;
    }
}

void xem_host_gpu_gp0(unsigned int word) {
    test_log(LOG_GP0, word);
}

void xem_host_gpu_gp1(unsigned int word) {
    test_log(LOG_GP1, word);
    if ((word >> 24) == 0x10) {
        test_gpuread = test_param[word & 7];
    }
}

unsigned int xem_host_gpu_read(void) {
    if (test_read_index < 64 && (test_status & 0x08000000)) {
        return test_read_words[test_read_index++];
    }
    return test_gpuread;
}

unsigned int xem_host_gpu_status(void) {
    return test_status;
}

void xem_host_gpu_dma(unsigned int madr, unsigned int bcr, unsigned int chcr) {
    test_log(LOG_DMA, madr);
    test_log(LOG_DMA, bcr);
    test_log(LOG_DMA, chcr);
    test_dma_control = chcr;
    if (chcr == 0x01000401) {
        unsigned int node = madr & 0xFFFFFF;
        int nodes = 0;

        while (!(node & 0x800000) && nodes++ < 1000) {
            unsigned int *words = (unsigned int *)(unsigned long)(node & 0xFFFFFC);
            unsigned int count = words[0] >> 24;
            unsigned int i;

            for (i = 1; i <= count; i++) {
                test_log(LOG_LIST_GP0, words[i]);
            }
            node = words[0] & 0xFFFFFF;
        }
    } else if (chcr == 0x01000200) {
        unsigned int *words = (unsigned int *)(unsigned long)madr;
        unsigned int i;

        for (i = 0; i < (bcr >> 16) * (bcr & 0xFFFF); i++) {
            words[i] = xem_host_gpu_read();
        }
    }
}

unsigned int xem_host_gpu_dma_control(void) {
    return test_dma_control;
}

void xem_host_yield(int reason) {
    test_yields++;
    if (test_dma_control & 0x01000000) {
        test_dma_control &= ~0x01000000;
        if (test_dma_callbacks[2] != 0) {
            test_dma_callbacks[2]();
        }
    }
}

int ResetCallback(void) {
    test_reset_callbacks++;
    return 0;
}

void *DMACallback(int channel, void (*func)(void)) {
    void (*previous)(void) = test_dma_callbacks[channel];

    test_dma_callbacks[channel] = func;
    return (void *)previous;
}

void test_draw_sync_callback(void) {
    test_draw_sync_callbacks++;
}

void test_reset_log(void) {
    test_log_length = 0;
}
