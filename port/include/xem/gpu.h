#ifndef XEM_GPU_H
#define XEM_GPU_H

/*
 * The GPU and its DMA channel 2 as host services (xem.gpu_*), answered by the
 * runtime's GPU device over xem-gpu. libgpu (port/libgpu.c) writes the same
 * register words the original SDK writes to the hardware registers:
 *
 *   GP0 (0x1F801810 write)   xem_host_gpu_gp0
 *   GP1 (0x1F801814 write)   xem_host_gpu_gp1
 *   GPUREAD (0x1F801810)     xem_host_gpu_read
 *   GPUSTAT (0x1F801814)     xem_host_gpu_status
 *   DMA2 MADR/BCR/CHCR       xem_host_gpu_dma, xem_host_gpu_dma_control
 *
 * Addresses given to the DMA are game addresses; the device takes their low
 * 24 bits as the hardware does. A transfer moves its words at once but stays
 * busy (CHCR bit 24) until the game next waits, when the device ends it and
 * raises XEM_IRQ_DMA with channel 2, as the hardware's completion interrupt.
 */

void xem_host_gpu_gp0(unsigned int word);
void xem_host_gpu_gp1(unsigned int word);
unsigned int xem_host_gpu_read(void);
unsigned int xem_host_gpu_status(void);

/* Write DMA2's MADR, BCR and then CHCR: 0x01000401 walks a linked list from
 * `madr`, 0x01000201 sends BCR's blocks from memory to GP0, 0x01000200 stores
 * GPUREAD words to memory; a CHCR without bit 24 stops the channel. */
void xem_host_gpu_dma(unsigned int madr, unsigned int bcr, unsigned int chcr);

/* DMA2's CHCR: bit 24 is set while a transfer is in progress. */
unsigned int xem_host_gpu_dma_control(void);

#endif
