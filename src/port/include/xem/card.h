#ifndef XEM_CARD_H
#define XEM_CARD_H

/*
 * The memory card service (src/runtime/crates/xem-core/src/devices/card.rs): two
 * cards as raw 128 KB images and the BIOS's card file system over them. A
 * device name is "buXY:" (X the port, Y the multitap slot, 0); names are NUL
 * terminated strings in game memory.
 */

/* The card driver: B(4Ah) InitCard, B(4Bh) StartCard, B(4Ch) StopCard,
 * A(70h) _bu_init. */
void xem_host_card_init(int pad_enable);
void xem_host_card_start(void);
void xem_host_card_stop(void);
void xem_host_card_bu_init(void);

/* A(ABh) _card_info: start a check of `channel` (port << 4 | slot). 1 when
 * started; the host then raises XEM_IRQ_PAD with the result event's spec
 * (XEM_CARD_EVENT_*) when the check ends. */
int xem_host_card_info(int channel);

/* The BIOS file calls on a card device, answered at once; the card stays busy
 * for the modelled access time (xem_host_card_busy), which the port waits
 * out as the BIOS blocks. */
int xem_host_card_open(char *name, unsigned int flags);
int xem_host_card_read(int fd, void *buffer, int length);
int xem_host_card_write(int fd, void *buffer, int length);
int xem_host_card_close(int fd);
/* Fills `entry` (struct DIRENTRY) and returns 1, or 0 when no file matches. */
int xem_host_card_firstfile(char *pattern, void *entry);
int xem_host_card_nextfile(void *entry);
int xem_host_card_rename(char *from, char *to);
int xem_host_card_delete(char *name);
int xem_host_card_format(char *device);
int xem_host_card_busy(void);

/* Whether a file call names a card device ("bu"). */
#define XEM_CARD_DEVICE(name) ((name)[0] == 'b' && (name)[1] == 'u')

/* The card events: class SwCARD 0xF0000011 and HwCARD 0xF4000001, spec done,
 * error, timeout (no card) or new card (the card answers without a file
 * system). */
#define XEM_CARD_SW_CLASS 0xF0000011
#define XEM_CARD_HW_CLASS 0xF4000001
#define XEM_CARD_EVENT_DONE 0x0004
#define XEM_CARD_EVENT_ERROR 0x8000
#define XEM_CARD_EVENT_TIMEOUT 0x0100
#define XEM_CARD_EVENT_NEW 0x2000

/* The card's interrupt (XEM_IRQ_PAD) with the event spec it ends with. */
void xem_card_interrupt(unsigned int spec);

#endif
