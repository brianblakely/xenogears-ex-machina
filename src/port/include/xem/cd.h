#ifndef XEM_CD_H
#define XEM_CD_H

/*
 * The CD-ROM controller as host imports (src/runtime/crates/xem-core/src/devices/cd.rs):
 * what libcd's register code does to the hardware, one call each. Pointers
 * are game addresses.
 */

/* CD_cw: send command `com` with `count` parameter bytes at `param`. */
void xem_host_cd_command(unsigned int com, void *param, unsigned int count);

/* getintr: take the pending interrupt. Returns its type (1-5; 0 when none)
 * | the number of response bytes << 8, and writes the 8 response bytes
 * (zero-padded) to `result`. Taking a data-ready interrupt loads its sector
 * into the sector buffer. */
unsigned int xem_host_cd_interrupt(void *result);

/* CD_getsector: copy the next `words` words of the sector buffer to `dest`. */
void xem_host_cd_get_sector(void *dest, unsigned int words);

/* CD_vol: the CD audio attenuation (a CdlATV at `atv`). */
void xem_host_cd_mix(void *atv);

/* CD_flush: acknowledge every pending interrupt. */
void xem_host_cd_flush(void);

/* Inside an interrupt handler a wait cannot yield: run the drive's next
 * event at once. Returns whether there was one. */
unsigned int xem_host_cd_poll_now(void);

#endif
