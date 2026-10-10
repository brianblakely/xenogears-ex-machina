#ifndef XEM_SPU_H
#define XEM_SPU_H

/*
 * The SPU service (runtime/crates/xem-core/src/devices/spu.rs). libspu and the
 * sound driver write the registers into the register window in game memory,
 * 0x1F801C00-0x1F801FFF, as the originals write the hardware's; the host
 * applies the window after every step and interrupt (the xem-spu crate's
 * register-window contract) and on xem_host_spu_sync. Transfers go through
 * the calls below, since the window sees values, not writes.
 */

/* The register window. */
#define XEM_SPU_REGISTERS 0x1F801C00

/* Apply the window to the SPU now and read the hardware values back into it:
 * libspu's register waits (_spu_Fw1ts and the status polls) and anything that
 * writes one register twice before the next step. */
void xem_host_spu_sync(void);

/* DMA channel 4 in block mode: `bytes` (whole 64-byte blocks) between game
 * memory at `ram` and SPU RAM at byte address `spu_address` (direction 0
 * writes SPU RAM, 1 reads it). The data moves at once; the host raises
 * XEM_IRQ_DMA with channel 4 when the modelled transfer time has passed. */
void xem_host_spu_transfer(unsigned int spu_address, void *ram, unsigned int bytes, unsigned int direction);

/* The manual-write path (the transfer FIFO, 0x1F801DA8): write `bytes` from
 * `data` at byte address `spu_address`; complete when it returns. */
void xem_host_spu_write_io(unsigned int spu_address, void *data, unsigned int bytes);

/* libspu's own completion entry (0x8004CB3C, the DMA-4 callback SpuStart
 * installs with DMACallback(4, ...)): the address has no symbol of its own,
 * so libetc's interrupt dispatch recognises it and calls
 * xem_spu_transfer_done. */
#define XEM_LIBSPU_TRANSFER_WAIT_ENTRY 0x8004CB3C
void xem_spu_transfer_done(void);

#endif
