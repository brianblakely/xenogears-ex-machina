#ifndef XEM_GTE_H
#define XEM_GTE_H

/* The software Geometry Transformation Engine (coprocessor 2) of the port
 * build: the PS1's 32 data and 32 control registers and its commands, bit
 * exact (psx-spx "Geometry Transformation Engine (GTE)"). Every GTE access the
 * game makes, through the inline macros (psyq/inline_c.h), the target-local
 * macro headers, libgte (decomp/port/libgte.c) and the handwritten routines,
 * goes through these five operations, so the whole GTE state is the one
 * object xem_gte, which a snapshot saves and restores as plain bytes.
 *
 * Each register holds the value a read (mfc2/swc2, cfc2) returns: 16-bit
 * registers are kept sign or zero extended as the hardware reads them back,
 * SXYP (data 15) mirrors SXY2, IRGB and ORGB (data 28, 29) are both the
 * colour collected from IR1-IR3, and FLAG bit 31 is the error summary. GTE
 * instructions take effect at once: the port has no coprocessor pipeline, so
 * the load and command delays the original's nops cover do not exist. */

typedef struct {
    unsigned int data[32];    /* cop2r0-31: vectors, colours, FIFOs, MAC, LZC */
    unsigned int control[32]; /* cop2r32-63: matrices, vectors, offsets, FLAG */
} XemGte;

extern XemGte xem_gte;

/* The state's address and size, for snapshots. */
XemGte *xem_gte_state(void);
unsigned int xem_gte_state_size(void);

/* Power-on state: every register zero, so LZCR reads 32. */
void xem_gte_reset(void);

/* mtc2/lwc2 and mfc2/swc2 of data register reg (0-31). */
void xem_gte_write_data(int reg, unsigned int value);
unsigned int xem_gte_read_data(int reg);

/* ctc2 and cfc2 of control register reg (0-31, cop2r32-63). */
void xem_gte_write_control(int reg, unsigned int value);
unsigned int xem_gte_read_control(int reg);

/* A cop2 command: the instruction word's low 25 bits (opcode bits 0-5, lm
 * bit 10, translation 13-14, vector 15-16, matrix 17-18, sf bit 19); the
 * upper bits of an instruction word (0x4A000000) are ignored. */
void xem_gte_execute(unsigned int command);

#endif
