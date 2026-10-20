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
#define XEM_GTE_PROJECTIONS 65536
#define XEM_GTE_RING_ADDRESS 0x70000000u

typedef struct {
    unsigned int sxy;    /* SXY2 as the command left it */
    unsigned int sz;     /* bits 0-15 SZ3, bits 16-23 the projection class */
    int x, y;            /* the screen point in 16.16, before saturation */
    unsigned int offset; /* OFX, OFY in whole pixels (16 bits each) */
    unsigned int h;      /* H, the projection distance */
    unsigned int owner;  /* the sprite or model being drawn (present.c), else 0 */
    int view[3];         /* the point before the division: MAC1, MAC2 and the
                          * depth SZ3 saturates from (also behind the camera) */
} XemProjection;

/* What draws the projections (present.c sets it around the wrapped seams):
 * INTERFACE is interface the GTE places (the world map's player marker),
 * drawn as the original draws it; BACKDROP the sky a battle's stage draws
 * far behind its scene (battle_draw_stage_sky), the world to a renderer of
 * the game's view, the backdrop to one seeing the scene from elsewhere. */
enum {
    XEM_PROJECTION_OTHER = 0,
    XEM_PROJECTION_MODEL = 1,
    XEM_PROJECTION_SPRITE = 2,
    XEM_PROJECTION_INTERFACE = 5,
    XEM_PROJECTION_BACKDROP = 10,
};

typedef struct {
    unsigned int count;      /* projections ever written */
    unsigned int class_;     /* XEM_PROJECTION_* for the next ones */
    unsigned int owner;      /* the sprite or model for the next ones */
    unsigned int fifo[3];    /* 1 + the projection SXY0, SXY1, SXY2 hold, 0 for another point */
    /* Set by the host: whether a draw trace takes these records (0: the
     * projection and store rings record nothing, as without a renderer). */
    unsigned int recording;
    unsigned int unused;
    XemProjection entries[XEM_GTE_PROJECTIONS];
} XemProjectionRing;

/* More presentation records beside it, by the same rules:
 * - every screen-coordinate word a port routine stores to memory (swc2 of
 *   SXY0-SXYP, and the handwritten renderers' stores of SXY values), with
 *   the projection that made it, so a renderer tells which projection made
 *   a packet's vertex by the packet word's address even when two points
 *   round to the same screen pixel;
 * - while the host asks for them (`wanted`: how far beyond the original
 *   screen a view wider or farther out than the original's reaches), the
 *   faces the game culls against its own screen: the
 *   renderers' per-face screen tests and the model and field-piece bounding
 *   tests that skip whole models (present.c then draws the model again with
 *   its links captured, and memory, the GTE and the renderers' state put
 *   back). Each is recorded as the GP0 words it would have had and the
 *   ordering-table entry it would have been linked at, when its points
 *   projected without overflow, it faces the camera and it lies within the
 *   reach (field scenery pieces the field's bounding test culls are drawn
 *   again only when their bounds reach that far). With a camera pulled back
 *   (`pull_back`: how many depth units behind the game's), all of that is
 *   judged as that camera sees the points (their view-space positions), the
 *   reach is measured on its screen, and the faces the renderers drop as
 *   facing away or at depth 0 count too (a quad's fourth point, not yet
 *   projected when it is dropped, is projected for the record with the GTE
 *   put back), and what the game skips because a projection overflowed and
 *   that camera sees is drawn again for the record (the field's sprite
 *   actors, present.c; the world map's scene objects, worldmap_present.c).
 *   With a view that looks from within or around the scene (`around`:
 *   first-person inspection, or a view from anywhere around a field, a
 *   battle or the world map), every face counts wherever it lies, as for a
 *   camera elsewhere but without a screen to reach: also behind the game's
 *   camera, facing away or at depth 0, and every scenery piece, sprite
 *   actor and world map scene object the game culls or skips is drawn for
 *   the record.
 * None of them changes what the game draws, links or keeps. */
#define XEM_PRESENT_STORES 65536
#define XEM_PRESENT_STORE_ADDRESS 0x70300000u
#define XEM_PRESENT_REVEALS 65536
#define XEM_PRESENT_REVEAL_ADDRESS 0x70500000u
#define XEM_PRESENT_CAPTURES 8192
#define XEM_PRESENT_BACKUP 0x40000
#define XEM_PRESENT_CAPTURE_ADDRESS 0x70A00000u

typedef struct {
    unsigned int address;     /* where the word went (24-bit physical) */
    unsigned int value;       /* the SXY word */
    unsigned int projections; /* the projection count at the store */
    unsigned int source;      /* 1 + the projection that made it, 0 unknown */
} XemSxyStore;

typedef struct {
    unsigned int count;
    /* Temporary packet range used by the presentation-only sprite replay.
     * Its low 24-bit addresses lie beyond original RAM, so they never match
     * a packet in the game's DMA trace. Zero outside that replay. */
    unsigned int private_begin, private_end;
    unsigned int unused;
    XemSxyStore entries[XEM_PRESENT_STORES];
} XemStoreRing;

typedef struct {
    unsigned int entry;       /* the node it would be drawn just before: the
                               * newest packet of the OT entry it would be
                               * linked at, or that entry when it ends the
                               * list (24-bit) */
    unsigned int shape;       /* GP0 words (bits 0-7), vertices (bits 8-15) */
    unsigned int projections; /* its points are the projections just before this count */
    unsigned int order;       /* 0: in that order; else point k is the projection
                               * (order >> 8k & 0xFF) before the count */
    unsigned int words[12];   /* the GP0 words, without the packet's tag */
} XemReveal;

typedef struct {
    unsigned int count;
    /* Set by the host: the reach beyond the original screen within which
     * culled faces are recorded, in pixels (x bits 0-15, y bits 16-31); 0
     * records none. */
    unsigned int wanted;
    /* Set by the host: how far behind the game's camera, in depth units,
     * the view that wants them looks from (0: the game's camera). */
    unsigned int pull_back;
    /* Set by the host: XEM_AROUND_* bits while that view looks from within
     * or around the scene; 0 while it looks from the game's camera. */
    unsigned int around;
    XemReveal entries[XEM_PRESENT_REVEALS];
} XemRevealRing;

/* The `around` bits: the view looks around from the field's controlled
 * character (first-person inspection: sprites are drawn again facing its
 * eyes, sprite_facing.c), or from anywhere around the scene (a diorama:
 * the battle's stage and the world map's ground are recorded too). */
enum {
    XEM_AROUND_LOOK = 1,
    XEM_AROUND_ANYWHERE = 2,
};

/* A model drawn again for the record (present.c): the renderers' links,
 * captured instead of made, and the packets they wrote, saved beforehand. */
typedef struct {
    unsigned int entry, slot, tag, projections;
} XemCapture;

typedef struct {
    unsigned int active;
    unsigned int count;
    unsigned int unused[2];
    XemCapture entries[XEM_PRESENT_CAPTURES];
    unsigned int backup[XEM_PRESENT_BACKUP / 4];
} XemCaptureArea;

/* What a non-drawn point in the ring is: the floor under a sprite
 * (present.c), the depth of the field camera's target, the transform each
 * model and sprite is drawn with, and the camera each frame of a field, a
 * battle or the world map is drawn with. A transform (present.c) is four
 * PLACEMENT entries the model or sprite owns, made before it projects
 * anything: its x, y and z axes through the rotation (`sz` 0-2, `view` the
 * matrix's column in 4.12) and its translation (`sz` 3), so that a point's
 * view is the rotation times its local point, shifted 12 bits, plus the
 * translation; for a model, the translation's `x` is the packet buffer it
 * is drawn into and `y` the size of one (an instance's two buffers are one
 * block, model_alloc_packet_buffers, so its draws alternate between two
 * buffers that far apart). A camera comes in this order: the world's x, y
 * and z axes through its rotation (AXIS entries, `view` the matrix's column
 * in 4.12, times the field's scale in a field; `owner` the axis, 0-2), its
 * translation (an AXIS entry owned by 3), and the point it looks at in the
 * same world units (CAMERA: `view` the point, `sz` its depth, `owner` the
 * mode, XEM_CAMERA_*, plus the host's `around` bits as it was recorded,
 * shifted 8 bits), so that a point's view is the rotation times the world
 * point, shifted 12 bits, plus the translation; then, while the host looks
 * from within or around the scene (`around`), in a field, the controlled
 * character's position through the camera (EYE, its sprite the owner). The
 * world map's camera maps points relative to the camera's target, its z
 * negated, as the world map places its objects (worldmap_present.c): its
 * target is (0, the target's height, 0), and before it comes the world's
 * point there (a STAGE entry owned by XEM_STAGE_ORIGIN: `view` the target's
 * x, 0 and its z negated, in whole units, as the objects are placed
 * relative to it) and, for a view from anywhere, the ground under the
 * target (a STAGE entry owned by XEM_STAGE_GROUND: `view` (0, the ground's
 * height, 0)), which lies below a target in flight. Before a battle's
 * camera comes, for a view from anywhere, its stage (battle_present.c), in
 * the battle's world: each combatant the battle shows (STAGE: `view` its
 * sprite's place, `sz` its height, `owner` its slot, 0-10), then the
 * scene's start view, the camera the battle begins with (STAGE entries
 * owned by XEM_STAGE_LOOK_AT, its look-at, `sz` 1 once the battle's turns
 * run after its opening, and XEM_STAGE_EYE, its eye). */
enum {
    XEM_PROJECTION_GROUND = 3,
    XEM_PROJECTION_FOCUS = 4,
    XEM_PROJECTION_AXIS = 6,
    XEM_PROJECTION_EYE = 7,
    XEM_PROJECTION_CAMERA = 8,
    XEM_PROJECTION_STAGE = 9,
    XEM_PROJECTION_PLACEMENT = 11,
};

/* A CAMERA entry's owner: which of the game's 3D modes drew the frame. */
enum {
    XEM_CAMERA_FIELD = 1,
    XEM_CAMERA_BATTLE = 2,
    XEM_CAMERA_WORLDMAP = 3,
};

/* The start view's points among a battle's STAGE entries (owners past the
 * eleven slots), and the world map's ground under its camera's target and
 * world point at that target. */
enum {
    XEM_STAGE_LOOK_AT = 16,
    XEM_STAGE_EYE = 17,
    XEM_STAGE_GROUND = 18,
    XEM_STAGE_ORIGIN = 19,
};

#ifdef __psx__
/* The game module (tools/game_module.py defines __psx__). */
#define xem_gte_ring ((XemProjectionRing *)XEM_GTE_RING_ADDRESS)
#define xem_present_stores ((XemStoreRing *)XEM_PRESENT_STORE_ADDRESS)
#define xem_present_reveals ((XemRevealRing *)XEM_PRESENT_REVEAL_ADDRESS)
#define xem_present_capture ((XemCaptureArea *)XEM_PRESENT_CAPTURE_ADDRESS)
#else
extern XemProjectionRing xem_gte_ring_storage;
extern XemStoreRing xem_present_store_storage;
extern XemRevealRing xem_present_reveal_storage;
extern XemCaptureArea xem_present_capture_storage;
#define xem_gte_ring (&xem_gte_ring_storage)
#define xem_present_stores (&xem_present_store_storage)
#define xem_present_reveals (&xem_present_reveal_storage)
#define xem_present_capture (&xem_present_capture_storage)
#endif

/* Record a screen-coordinate word stored at `address` (presentation), made
 * by projection `source` - 1 (0 unknown). */
void xem_present_store(unsigned int address, unsigned int value, unsigned int source);

/* The same for a store through a C pointer. */
void xem_present_store_at(const void *p, unsigned int value, unsigned int source);

/* The `source` of the point a data register (SXY0-SXYP) holds. */
unsigned int xem_present_sxy_source(int reg);

/* Whether the view that wants culled faces looks from elsewhere than the
 * game's camera: pulled back, or around from within the field. */
int xem_present_elsewhere(void);

/* Whether a culled face is worth recording: its `vertices` points are the
 * projections just before `projections`; each projected without overflow,
 * the first three wind as NCLIP keeps, and one lies within the host's reach
 * of a `width` x `height` screen (as the pulled-back camera sees them, when
 * the host pulls it back; any face while it looks around). */
int xem_present_face_visible(unsigned int projections, unsigned int vertices, int width, int height);

/* The same without the winding: a primitive the game draws whichever way
 * its points wind (the world map's clouds). */
int xem_present_points_visible(unsigned int projections, unsigned int vertices, int width, int height);

/* Whether a square of half-size `radius` facing the camera around the
 * view-space point (x, y, z) reaches a `width` x `height` screen widened by
 * `margin_x`, `margin_y` and the host's reach, as the pulled-back camera
 * sees it (with the GTE's screen offset and projection distance); any
 * square while the host looks around. */
int xem_present_square_visible(int x, int y, int z, int radius, int width, int height, int margin_x, int margin_y);

/* Record a culled face (presentation): `count` GP0 words of a primitive
 * whose `vertices` points are the projections just before `projections`
 * (none for a flat one), as if linked now at the ordering-table entry
 * `entry`. */
void xem_present_reveal(unsigned int entry, unsigned int vertices, unsigned int projections, const unsigned int *words,
                        unsigned int count);

/* The same, drawn just before the packet (or ordering-table entry) `node`,
 * its points in the projection `order` (XemReveal). */
void xem_present_reveal_at(unsigned int node, unsigned int vertices, unsigned int projections, unsigned int order,
                           const unsigned int *words, unsigned int count);

/* While a capture is active (present.c), a renderer's link at `entry` of
 * the packet at `slot` with tag `tag`. */
void xem_present_capture_link(unsigned int entry, unsigned int slot, unsigned int tag);

#endif
