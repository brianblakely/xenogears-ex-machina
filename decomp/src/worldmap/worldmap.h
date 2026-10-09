#ifndef WORLDMAP_H
#define WORLDMAP_H

/* The world map overlay (mode 3: 8006faf0-8009bbb4, its uninitialized data to
 * 8009d810): the modes and their frame loop, the area files, the player, the
 * pad state, the display buffers with the sky, horizon, map and footprints,
 * the encounters, the actor slots and the state saved across another scene.
 * Beside it: camera.h, scene.h (scene objects, the areas' actors and the
 * scripted scene modes), effect.h (particles and drifting clouds), party.h
 * (the party and its vehicles), terrain.h (terrain, paths and movement),
 * screen.h (the fade and the name windows), stream.h (the terrain stream
 * reader) and gte.h (GTE macros). */

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/sprite.h"

#define ABS(x) ((x) < 0 ? -(x) : (x))

/* World-map modes (D_8009A058): 0-7 the open map, 8-18 the scripted scenes.
 * The overlay entry enters the mode, then starts it and runs the frame loop
 * for as long as the loop's result asks for it, leaving it after each run. */
typedef struct {
    void (*enter)(void);
    void (*start)(void);
    void (*leave)(void);
} WorldmapMode;

extern WorldmapMode D_8009A058[]; /* per mode */
extern s32 D_8009C5A8;            /* the current mode */

void func_80071CDC(void);         /* modes 0-7: enter (read the party's models) */
void func_80071EF0(void);         /* modes 8-18: enter (read the area files) */
void func_80072238(void);         /* modes 0-7: start */
void func_8007299C(void);         /* modes 0-7: leave */

/* The frame loop (func_800712D0) and its state. */
extern s32 D_8009D554; /* nonzero while the loop runs */
extern s32 D_8009D7CC; /* its result: 0 leave for a scene, 1 battle, 2 and more run the mode again */
extern s32 D_8009BBC4; /* nonzero once a leave handler chose the next scene */
extern s32 D_8009C894; /* nonzero when resuming a saved state */
extern s32 D_8009C178; /* nonzero while a screen fade runs */
extern s32 D_8009D804; /* the menu was requested */
extern s32 D_8009BD34; /* the two-button combination was pressed (func_80090A18) */
extern s32 D_8009CEC0, D_8009C7E8; /* that combination held this frame and the last */
extern void (*D_8009CD40)(void); /* per-frame hook */
extern MATRIX D_8009BE4C; /* set to the identity by each mode's set-up */
extern MATRIX D_8009A180; /* identity matrix */
extern s32 D_8009BD0C;    /* the saved map at entry less 0x400 (event variable 2 on leaving) */

/* The world map's first flag word, game data entry[2], by a name of its own
 * where the overlay entry tests and sets it before choosing the mode: as the
 * member it compiles differently there. */
extern u16 D_8006F954[];

/* The map flags: event variables 254-255, one bit per map dot (bits 24-26
 * the vehicles). */
#define MAP_FLAGS (*(u32 *)&D_8006D634.vars[254])

/* Per-area file set: the first disc file, the area's extent in blocks (x, z)
 * and the frame loop's first result. */
typedef struct {
    s16 file;
    s16 param2;
    s16 param4;
    s16 param6;
} WorldmapArea;

extern u16 D_8009B564[];          /* open map area thresholds, indexed from 1 */
extern WorldmapArea D_8009B584[]; /* area file sets */
extern s32 D_8009C610;            /* open map area */
extern s32 D_8009D3D4;            /* entry: arrival point, or the scene's entry */

/* The area's disc files (func_80071B9C) and the buffers they are read into. */
extern s32 D_8009D3C4, D_8009C17C, D_8009C174, D_8009CC98, D_8009D3D0; /* files 1-5 */
extern s32 D_8009D3C8, D_8009D800, D_8009BCC8, D_8009BCD8, D_8009BD08; /* files 6-10 */
extern s32 D_8009D160, D_8009D2B4; /* extent in blocks: x, z */
extern void *D_8009C180;           /* area data (file 1) */
extern void *D_8009C59C;           /* terrain texture image (file 2) */
extern void *D_8009BD20;           /* area image (file 3) */
extern void *D_8009C88C;           /* wave bank (file 4) */
extern void *D_8009C884, *D_8009C888, *D_8009C614; /* music (files 5, 7, 8) */
extern void *D_8009D528;           /* the shared file 0x25 */
extern FileRequest D_8009D3F8[];   /* shared read list, a zero file ends it */
/* The list's first destination member by a name of its own (worldmap.data.ld):
 * two loaders pass the list from it. Formed from D_8009D3F8 itself, the
 * constant lets cse store the first entry through the argument register. */
extern void *D_8009D3FC;
#define WORLD_READ_LIST ((FileRequest *)((u8 *)&D_8009D3FC - 4))

void func_80071FEC(void); /* read the shared files 0x25 and 0x26 */
void func_80072090(void); /* read the area's files 4-8 */
void func_800721E4(void); /* read its sound bank (file 6) */

/* The music files' sequence data (D_80062648, resident/mode.h), as the
 * sequence header the sound driver reads. A declaration of its own for the
 * same symbol: func_80072238 forms the address anew rather than from the
 * destination of the copy before it, which cse keeps in a saved register
 * when both are D_80062648. */
extern struct SoundSeqHeader D_80062648_sequence __asm__("D_80062648");

/* The area data's header (file 1): section offsets from its start. */
typedef struct {
    s32 unk0;
    s32 spots;          /* spot block */
    s32 models;         /* sprite models */
    s32 meshes;         /* their collision meshes */
    s32 placements;     /* scene object placements */
    s32 billboards;     /* the terrain blocks' billboard lists */
    s32 emitters;       /* area objects */
    s32 names;          /* path and destination names */
    s32 animations;     /* texture animations */
    s32 animations2;    /* the second set */
    s32 unk28;
    s32 encounters[16]; /* encounter sets, per terrain kind */
} AreaHeader;

/* Spot block: arrival points and a table of four sections. */
typedef struct {
    s32 spots;
    s32 table;
} SpotHeader;

/* Named arrival point: position in world units and its id; -1 ends a list. */
typedef struct WorldmapSpot {
    s16 x;
    s16 id;
    s16 z;
    s16 pad;
} WorldmapSpot;

/* The area data's sections (func_80073530). */
extern void *D_8009CD48;          /* sprite models */
extern void *D_8009D308;          /* their collision meshes */
extern void *D_8009BD30;          /* scene object placements */
extern void *D_8009D784;          /* path and destination names */
extern s32 *D_8009D77C, *D_8009D7C8; /* texture animations, both sets */
extern void *D_8009D73C[16];      /* encounter sets, per terrain kind */
extern s32 *D_8009BD00;           /* the four path tables */
extern WorldmapSpot *D_8009D3F4;  /* arrival points */

void func_80076954(void); /* unpack a scene mode's area data */

/* Texture animations: each slot uploads one image of a frame sequence. */
typedef struct {
    s16 image;    /* image index in the animation's data */
    s16 duration; /* frames; negative ends the sequence */
} TexAnimFrame;

typedef struct {
    RECT rect;
    s32 unk8;
    TexAnimFrame *frames;
} TexAnimSlot;

typedef struct TexAnim {
    u8 *images;
    TexAnimSlot *slot;
    s16 frame;
    s16 timer;
} TexAnim;

extern TexAnim *D_8009D780, *D_8009D7D0;
extern s32 D_8009CC9C, D_8009CD64; /* animations of each set */

void func_80074E58(void), func_80074F04(void), func_80074F2C(void); /* create, free, advance */
void func_80075030(void), func_800750DC(void), func_80075104(void); /* the second set */

/* The player: movement mode (1-3 on foot, 4-7 in a vehicle, 6-7 flying),
 * position (20.12) and heading. */
extern s32 D_8009BE10;
extern VECTOR D_8009C5AC;
extern s32 D_8009C584;

void func_80073300(void); /* choose the movement mode */
void func_80075228(void); /* reset the movement state */
void func_80075D4C(void); /* apply the party's riding changes */

/* Pad buttons gathered per frame from the queued input events (resident/pad.h
 * words OR-ed together): held, pressed and repeated, each with the word that
 * follows it in the resident; and the resident's vertical blank count. */
extern u16 D_8009CD4C, D_8009CD50; /* held */
extern u16 D_8009BD10, D_8009BD14; /* pressed */
extern u16 D_8009BD18, D_8009BD1C; /* repeated */
extern s32 D_80059488;

/* Per display buffer: its environments, its ordering table (0x400 entries)
 * and its terrain triangle packets (0x10000 bytes). */
typedef struct DisplayBuffer {
    DRAWENV draw;
    DISPENV disp;
    u_long *ot;    /* 0x70 */
    void *packets; /* 0x74 */
} DisplayBuffer;

extern DisplayBuffer D_8009BBC8[2];
extern DisplayBuffer *D_8009BE3C; /* the buffer being drawn */
extern s32 D_8009D7F0;           /* its index */
extern s32 D_8009BCDC;           /* projection distance */
extern u8 D_8009BB48[3];         /* background colour */

void func_8007369C(void); /* allocate the ordering tables */
void func_80072BB0(void); /* set up the display */
void func_80072DB4(s32 a, s32 b, s32 c, s32 d); /* fade the saved screen */
void func_800762FC(void); /* wait for the GPU, flush the cache */

/* A resident display call the world map declares itself: its calls pass
 * words where the resident's definition takes bytes. */
void func_8002C6E0(s32 r, s32 g, s32 b);

/* Gouraud quad packet (PsyQ POLY_G4 layout); colour words carry the code
 * in their top byte. */
typedef struct PolyG4 {
    u32 tag;
    u32 rgb0;
    long xy0;
    u32 rgb1;
    long xy1;
    u32 rgb2;
    long xy2;
    u32 rgb3;
    long xy3;
} PolyG4;

/* The sky, the horizon, the map overlay and the footprints
 * (worldmap_80072238). */
extern PolyG4 D_8009D194[4][2];   /* sky gradient bands, per buffer */
extern POLY_FT4 D_8009C744[2][2]; /* textured horizon quads, per buffer */
extern DR_TWIN D_8009D3D8[2];     /* their texture windows */
extern POLY_FT4 D_8009C5C0[2];    /* overlay picture, per buffer */
extern DR_TPAGE D_8009C5A0;
extern POLY_G3 D_8009C664[8];     /* player marker triangles */
extern TILE D_8009C898[0x40];     /* map dots */
extern u16 D_8009B6F4[64];        /* 32 map dot positions: interleaved X/Z */
extern WorldmapSpot *D_8009D30C;  /* footprint ring of 16 positions */
extern s32 D_8009BE38;            /* footprints recorded */
extern void *D_8009BE14, *D_8009BE18; /* footprint quads, per buffer */

void func_800736DC(void), func_800737EC(void); /* the sky: set up, draw */
void func_800739B8(void), func_80073B04(void); /* the horizon */
void func_800740B8(void);                      /* draw the map overlay */
void func_80074794(s16 id, VECTOR *position);  /* record a footprint */
void func_800747DC(void);                      /* draw the footprints */

/* Random encounters: timers with distinct random delays per period, and the
 * terrain's encounter set copied for the battle. */
extern s16 D_8009C854[16];
extern s32 D_8009D64C, D_8009BE40, D_8009BCC4; /* countdown, period, timers */
extern s32 D_8009D80C; /* timers that expired this frame */
extern u16 D_8009B578[]; /* level bracket thresholds, from 1 */

/* Encounter tables of a terrain kind: 0x200 bytes of formations, then per level
 * bracket 16 formation weights. */
typedef struct {
    u8 data[0x200];
} EncounterSet;

extern EncounterSet D_800658DC; /* encounter set of the next battle */

void func_8007528C(void);                        /* count the timers down */
s32 func_80075E7C(VECTOR *position, s32 level);  /* roll an encounter */

/* The pause and controller check screens, and the music. */
void func_8007634C(void);
void func_80076594(void);
void func_800767D4(void *data, s32 file);

/* Screen fade (func_800925A0): rate of its blend page and brightness step. */
extern s32 D_8009CCA4, D_8009D3CC;

/* Actor slots (0x80 bytes each, 64 of them). */
typedef struct WorldmapActor {
    s16 command;     /* 0x00: pending command */
    s16 command_arg;
    s16 unk4;
    s16 unk6;
    s32 unk8;
    s32 unkC;
    s32 unk10;
    s32 unk14;
    s32 kind;        /* 0x18 */
    s32 update;      /* 0x1C: nonzero while the slot is in use */
    s16 state;       /* 0x20 */
    s16 wait;        /* 0x22: script wait counter */
    s16 unk24;
    s16 unk26;
    VECTOR position; /* 0x28 */
    VECTOR motion;   /* 0x38 */
    s16 heading;     /* 0x48 */
    s16 turn;        /* 0x4A: turn step */
    Sprite *handle;  /* 0x4C: its model sprite (func_80024524), NULL none */
    union {
        s16 *script; /* script position */
        u16 value;   /* low half of step */
        s32 step;
    } u;             /* 0x50 */
    s32 unk54;
    s32 unk58;
    s32 unk5C;
    s32 unk60;
    s32 unk64;
    s32 unk68;
    s32 unk6C;
    s32 unk70;
    s32 unk74;
    s32 unk78;
    s32 unk7C;
} WorldmapActor;

/* Actor slot entry points (kind: start, update: step); they return the
 * next command. */
typedef s32 (*ActorFunc)(s32 index);

extern WorldmapActor *D_8009BE24;

void func_8009766C(void);                 /* allocate the slots */
void func_800976A0(void);                 /* free them */
void func_800976C8(void);                 /* mark every slot free */
void func_800976FC(s32 kind, s32 index);  /* change a slot's kind */
void func_80097718(s32 kind, s32 update); /* start an actor */
s32 func_80097770(s32 index, s32 arg);    /* send command 1 */
void func_80097800(void);                 /* run the pending commands */

/* An actor's model sprite: render bit 2 hides it (a new model starts hidden),
 * and the last byte of its motion word, read signed, is the animation
 * func_800245D8 set. */
#define SPRITE_HIDDEN 4
#define SPRITE_ANIMATION(sprite) ((s8)(sprite)->motion.bytes[3])

void func_80085CDC(void); /* draw the actors' model sprites */

/* Resident sprite calls the world map declares itself: its calls pass words
 * where the resident's definitions take halfwords. */
Sprite *func_80024524(s32 *data, s32 x, s32 y, s32 width, s32 height, s32 unused);
void func_800223B0(Sprite *sprite, s32 angle);

/* Scratchpad work area of the actor updaters. */
typedef struct {
    VECTOR work;      /* 0x00 */
    u8 pad10[0x20];
    VECTOR start;     /* 0x30: trail reset position */
    u8 pad40[0x50];
    VECTOR probe;     /* 0x90: move probe result */
    SVECTOR position; /* 0xA0 */
    SVECTOR angle;    /* 0xA8 */
} ActorScratch;

#define ACTOR_SCRATCH ((ActorScratch *)0x1F800000)
#define SCRIPT_VECTOR ((SVECTOR *)0x1F8000A0) /* scratchpad script vector */

/* Recent positions of the player's vehicle (ring of 32). */
typedef struct TrailPoint {
    VECTOR position;
    u16 heading;
    u16 pad;
} TrailPoint;

extern TrailPoint D_8009CEC4[32];
extern s16 D_8009D154; /* trail index */
extern u16 D_8009D52C; /* the leader's heading */

/* Suspending the open map for another scene (func_800758C0, func_80075B58):
 * the memory kept free while away, the VRAM areas saved and the battle
 * flag. */
extern void *D_8009C7E4;
extern void *D_8009C800, *D_8009C890;
extern s32 D_8009D14C;

void func_800758C0(void);
void func_80075B58(void);

/* The resident's save of the world-map state across a scene change (0x8005a4e4). */
typedef struct {
    WorldmapActor actor[64];
} ActorSet;

typedef struct {
    s16 timer[16];
} TimerSet;

typedef struct {
    TrailPoint points[32];
} WorldmapQueue; /* the vehicle trail, saved as a whole */

typedef struct {
    ActorSet actors;       /* 0x0000 */
    VECTOR position;       /* 0x2000 */
    s32 unk2010;           /* D_8009D52C */
    s32 timer_period;      /* D_8009BE40 */
    s32 timer_count;       /* D_8009BCC4 */
    s32 timer_countdown;   /* D_8009D64C */
    TimerSet timers;       /* 0x2020 */
    WorldmapQueue queue;   /* 0x2040 */
    s32 queue_count;       /* 0x22C0 */
    s32 camera_angle[2];   /* 0x22C4: SVECTOR D_8009BD38 as words */
    s32 camera_distance;   /* 0x22CC */
    s32 unk22D0;           /* D_8009BE0C */
    VECTOR unk22D4;        /* D_8009BBB4 */
    s32 unk22E4[2];        /* SVECTOR D_8009C838 as words */
    VECTOR camera_target;  /* 0x22EC */
} WorldmapSave;

extern WorldmapSave D_8005A4E4;

/* Model and object of the scene overlay at 0x801E0000, which draws the
 * distant landmark (func_80076098). */
typedef struct {
    u8 pad0[0x54];
    SVECTOR angle;    /* 0x54 */
    s32 x, y, z;      /* 0x5C */
} OverlayModel;

typedef struct {
    u8 pad0[4];
    OverlayModel *model; /* 0x04 */
    u8 pad8[0x14];
    s16 unk1C;
    u8 pad1E[0x3E];
    s16 unk5C;
} OverlayObject;

extern OverlayObject *D_801E8670[]; /* scene overlay objects; [0] is the landmark */
extern MATRIX *D_801E8644;
void func_801E7D14(MATRIX *view, MATRIX *light, u_long *ot, s32 buffer, s32 mode);

/* A resident window call the world map declares itself: its call passes
 * words where the resident's definition takes halfwords. */
void func_80033698(s32 a, s32 b);

#endif
