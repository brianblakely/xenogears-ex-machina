#ifndef MENU_GTE_H
#define MENU_GTE_H

/* Two screen-depth stores of the menu's own, which the SDK's inline_c.h does
 * not have: one depth of the last RTPT, SZ1 or SZ2 (gte_stsz stores SZ3). The
 * menu's other GTE macros are psyq/inline_c.h's. */
#define gte_stsz1(r0) __asm__ volatile("swc2 $17, 0(%0)" : : "r"(r0) : "memory")
#define gte_stsz2(r0) __asm__ volatile("swc2 $18, 0(%0)" : : "r"(r0) : "memory")

#endif
