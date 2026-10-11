#ifndef XEM_GTE_H
#define XEM_GTE_H

/* The software Geometry Transformation Engine (coprocessor 2) of the port
 * build: the PS1's 32 data and 32 control registers and its commands, bit
 * exact (psx-spx "Geometry Transformation Engine (GTE)"). Every GTE access the
 * game makes, through the inline macros (psyq/inline_c.h), the target-local
 * macro headers, libgte (src/port/libgte.c) and the handwritten routines,
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

/* Presentation (docs/runtime.md, Presentation): every RTPS/RTPT vertex is
 * also appended to a ring of the last XEM_GTE_PROJECTIONS projections, which
 * libgpu hands the GPU device before each transfer so a renderer can give
 * the packets' screen points their depth and sub-pixel position. Nothing
 * reads it back into the game. In the game module the ring lives at a fixed
 * address outside the port's data, stack and every snapshot region, so it
 * changes neither the port's memory layout nor any saved state. */
#define XEM_GTE_PROJECTIONS 8192
#define XEM_GTE_RING_ADDRESS 0x70000000u

typedef struct {
    unsigned int sxy;    /* SXY2 as the command left it */
    unsigned int sz;     /* bits 0-15 SZ3, bits 16-23 the projection class */
    int x, y;            /* the screen point in 16.16, before saturation */
    unsigned int offset; /* OFX, OFY in whole pixels (16 bits each) */
    unsigned int h;      /* H, the projection distance */
    unsigned int owner;  /* the sprite or model being drawn (present.c), else 0 */
    unsigned int unused;
} XemProjection;

/* What draws the projections (present.c sets it around the wrapped seams). */
enum {
    XEM_PROJECTION_OTHER = 0,
    XEM_PROJECTION_MODEL = 1,
    XEM_PROJECTION_SPRITE = 2,
};

typedef struct {
    unsigned int count;      /* projections ever written */
    unsigned int class_;     /* XEM_PROJECTION_* for the next ones */
    unsigned int owner;      /* the sprite or model for the next ones */
    unsigned int unused[5];
    XemProjection entries[XEM_GTE_PROJECTIONS];
} XemProjectionRing;

#ifdef __psx__
/* The game module (tools/game_module.py defines __psx__). */
#define xem_gte_ring ((XemProjectionRing *)XEM_GTE_RING_ADDRESS)
#else
extern XemProjectionRing xem_gte_ring_storage;
#define xem_gte_ring (&xem_gte_ring_storage)
#endif

#endif
