#ifndef XEM_MDEC_H
#define XEM_MDEC_H

/*
 * The MDEC and its two DMA channels as host imports
 * (runtime/crates/xem-core/src/devices/mdec.rs). Pointers are game addresses.
 */

/* Write the control register (1F801824h). */
void xem_host_mdec_control(unsigned int value);

/* Read the status register (1F801824h). */
unsigned int xem_host_mdec_status(void);

/* MDEC_in: the command word at `addr`, then (size / 32) blocks of 32 words
 * from addr + 4 by DMA0. */
void xem_host_mdec_in(void *addr, unsigned int size);

/* MDEC_out: (size / 32) blocks of 32 words to `addr` by DMA1; the device
 * ends the transfer as the MDEC produces them and raises the DMA1
 * interrupt. */
void xem_host_mdec_out(void *addr, unsigned int size);

/* Whether DMA1 is still transferring. */
unsigned int xem_host_mdec_out_busy(void);

/* Stop both DMA channels (their control registers cleared). */
void xem_host_mdec_stop_dma(void);

#endif
