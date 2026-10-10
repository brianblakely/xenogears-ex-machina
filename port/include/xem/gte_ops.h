#ifndef XEM_GTE_OPS_H
#define XEM_GTE_OPS_H

/*
 * The handwritten routines' GTE instructions on the software GTE: the command
 * words of decomp/include/macro.inc and the transfers through game memory.
 * mfc2 and cfc2 are xem_gte_read_data and xem_gte_read_control; they have no
 * side effects, so a port routine reads a register when it needs the value.
 */
#include "xem/gte.h"
#include "xem/memory.h"

#define XEM_GTE_RTPS 0x0180001
#define XEM_GTE_RTPT 0x0280030
#define XEM_GTE_NCLIP 0x1400006
#define XEM_GTE_AVSZ3 0x158002D
#define XEM_GTE_AVSZ4 0x168002E
#define XEM_GTE_DPCS 0x0780010
#define XEM_GTE_DPCT 0x0F8002A
#define XEM_GTE_DPCL 0x0680029
#define XEM_GTE_INTPL 0x0980011
#define XEM_GTE_NCS 0x0C8041E
#define XEM_GTE_NCT 0x0D80420
#define XEM_GTE_NCDS 0x0E80413
#define XEM_GTE_NCDT 0x0F80416
#define XEM_GTE_NCCS 0x108041B
#define XEM_GTE_NCCT 0x118043F
#define XEM_GTE_CDP 0x1280414
#define XEM_GTE_CC 0x138041C
#define XEM_GTE_SQR(sf) (0x0A00428 | (sf) << 19)
#define XEM_GTE_OP(sf) (0x170000C | (sf) << 19)
#define XEM_GTE_GPF(sf) (0x190003D | (sf) << 19)
#define XEM_GTE_GPL(sf) (0x1A0003E | (sf) << 19)
#define XEM_GTE_MVMVA(sf, mx, v, cv, lm) (0x0400012 | (sf) << 19 | (mx) << 17 | (v) << 15 | (cv) << 13 | (lm) << 10)

/* Data registers by number. */
enum {
    XEM_GTE_VXY0, XEM_GTE_VZ0, XEM_GTE_VXY1, XEM_GTE_VZ1, XEM_GTE_VXY2, XEM_GTE_VZ2, XEM_GTE_RGBC, XEM_GTE_OTZ,
    XEM_GTE_IR0, XEM_GTE_IR1, XEM_GTE_IR2, XEM_GTE_IR3, XEM_GTE_SXY0, XEM_GTE_SXY1, XEM_GTE_SXY2, XEM_GTE_SXYP,
    XEM_GTE_SZ0, XEM_GTE_SZ1, XEM_GTE_SZ2, XEM_GTE_SZ3, XEM_GTE_RGB0, XEM_GTE_RGB1, XEM_GTE_RGB2, XEM_GTE_RES1,
    XEM_GTE_MAC0, XEM_GTE_MAC1, XEM_GTE_MAC2, XEM_GTE_MAC3, XEM_GTE_IRGB, XEM_GTE_ORGB, XEM_GTE_LZCS, XEM_GTE_LZCR
};

/* lwc2 and swc2: a data register from or to the word at a PS1 address. */
static inline void xem_lwc2(int reg, unsigned int address) {
    xem_gte_write_data(reg, XEM_U32(address));
}

static inline void xem_swc2(int reg, unsigned int address) {
    XEM_U32(address) = xem_gte_read_data(reg);
}

#endif
