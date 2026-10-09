#ifndef WORLDMAP_SCENE_H
#define WORLDMAP_SCENE_H

/* The world map's scene objects, the areas' own actors and the scripted
 * scene modes. Scene objects (worldmap_80083A00) are the area data's sprite
 * models placed in the world and linked into hierarchies; the solid ones
 * carry collision meshes. Each open map area starts actors of its own (the
 * spinning objects, the ferry, the airship). The scene modes 8-18 each start
 * a set of actors: the screen fade, a cue sequencer (the director) and the
 * camera and object actors it commands; the text split leaves each mode's
 * set-up and leave handlers and its director's sequence start at the end of
 * the unit before the director's (worldmap_8007DE98). */

#include "worldmap.h"
#include "resident/model.h"

/* Scene object (0x54 bytes): a sprite model of the area data, transformed
 * and linked to a parent. */
typedef struct SceneObject {
    s16 visible;
    s16 unk2;
    u16 flags;                  /* 0x04: 1 solid */
    s16 unk6;
    VECTOR position;            /* 0x08 */
    SVECTOR angle;              /* 0x18 */
    MATRIX matrix;              /* 0x20 */
    SpriteModel *def;           /* 0x40 */
    s32 unk44;                  /* its collision mesh (Mesh) */
    void *prims;                /* 0x48 */
    void *prims2;               /* 0x4C: second buffer's copy */
    struct SceneObject *parent; /* 0x50 */
} SceneObject;

extern SceneObject *D_8009C620; /* scene objects */
extern s16 D_8009D7E0;          /* scene object count */
extern s16 D_8009BD28;          /* animation count */
extern u16 D_8009BCE0[16];      /* the area image's faded CLUT ids */
extern MATRIX D_8009A140, D_8009A160; /* colour and light matrices */

void func_8008440C(void); /* upload the area image, build its faded CLUTs */
void func_80084580(void); /* build the scene objects */
void func_80084818(void); /* free them */
void func_800848B4(s32 parent, s32 child);
void func_800848F4(void); /* draw them */

/* A resident model call the world map declares itself: it passes the scene
 * object as a fourth argument the resident's definition does not take. */
void func_8002CB54(SpriteModel *def, void **prims, void **prims2, SceneObject *object);

/* Collision mesh of a scene object (behind SceneObject.unk44). */
typedef struct {
    s16 corner[3];
    s16 next[3]; /* neighbouring face across each edge; -1 at the border */
    u16 kind;    /* 1: wall */
} MeshFace;

typedef struct {
    s32 unk0;
    SVECTOR *vertices;
    MeshFace faces[1];
} Mesh;

extern s16 D_8009D718[];        /* probe hits: face and kind pairs */
extern s32 D_8009C16C, D_8009C840; /* the face and object the walker stands on, -1 none */

void func_80085158(VECTOR *position, VECTOR *offset, VECTOR *normal, u16 index, u16 face);
s32 func_80085418(VECTOR *probe, s32 radius, u16 object, u16 other);
s32 func_80085760(VECTOR *from, VECTOR *to, s32 index, s32 face);

/* Per area: the scene objects of the area's actors (a spinning pair, a
 * further pair and single objects). */
extern u16 D_8009B624[][2], D_8009B64C[][2];
extern u16 D_8009B674[], D_8009B688[], D_8009B69C[], D_8009B6B0[];

/* The areas' own actors (start, update; D_8009A034) and the updates
 * installed again when resuming a saved state. */
s32 func_80087710(s32 index), func_80087734(s32 index); /* spinning pair */
s32 func_800877E0(s32 index), func_80087804(s32 index); /* the area's spinning pair */
s32 func_800879A8(s32 index), func_80087A8C(s32 index); /* the area's rolling pair */
s32 func_80087C6C(s32 index), func_80087FD0(s32 index); /* the ferry */
s32 func_80088570(s32 index), func_80088720(s32 index); /* the airship */
s32 func_80088B40(s32 index), func_80088D00(s32 index);
s32 func_80088D64(s32 index), func_80088DE4(s32 index);
s32 func_80088E1C(s32 index), func_80088E68(s32 index);
s32 func_80088EA0(s32 index), func_80088F1C(s32 index);
s32 func_80088F54(void), func_80088F5C(void);
s32 func_800879E0(s32 index);
s32 func_80087F60(s32 index);
s32 func_8008868C(void);
s32 func_80088C90(void);

/* The ferry's saved route state is D_8006D634.unk1844 (x, z in world units,
 * next waypoint); its runs started, which func_80087C6C reads and counts by
 * a name of its own (as the member unk184A it compiles differently). */
extern u16 D_8006EE7E;
extern u16 D_8009AF80[8], D_8009AF90[8]; /* ferry waypoints (x, z) */

/* Ferry heading history (ring of 32). */
typedef struct FerryHeading {
    s16 dx;
    s16 pad2;
    s16 dz;
    s16 pad6;
} FerryHeading;

extern FerryHeading D_8009CD68[32];

/* Timed sequence: state per step and the step durations. */
typedef struct {
    s16 *states;
    u16 *durations;
} Sequence;

/* The directors' cue sequences, user-supplied script data (INCLUDE_ASSET):
 * states and waits of modes 14 (worldmap_8007A9F8), 12 (worldmap_8007C3B8),
 * 15 (D_8009A65C per entry, worldmap_8007DE98), 13 (worldmap_80080370) and
 * 16 (worldmap_800811C0), and the actor scripts of modes 17 and 18. */
extern u16 D_8009A450[], D_8009A46C[];
extern u16 D_8009A4D8[], D_8009A4E8[];
extern Sequence D_8009A65C[];
extern u16 D_8009A698[], D_8009A6AC[];
extern u16 D_8009A6C0[], D_8009A70C[];
extern s16 D_8009A758[], D_8009AC60[];

/* Mode 15's entries: the player's start position and the resident flag word
 * set on leaving. */
extern SVECTOR D_8009A5B4[];
extern u16 D_8009A5CC[];

/* Scene sprite quads (worldmap_80080370): set their colour. */
void func_800809EC(POLY_FT4 *quads, s32 count, s32 r, s32 g, s32 b);
/* func_8007A06C (void, worldmap_80077E68) has no prototype here: the other units
 * call it undeclared (implicit int), which lets the caller schedule its return value
 * early after the call (8007FC8C). */

/* Scratchpad work area of the scaled scene objects. */
typedef struct {
    VECTOR scale[2];
    u8 pad20[0x80];
    SVECTOR angle;    /* 0xA0 */
    u8 padA8[0x48];
    MATRIX matrix[2]; /* 0xF0 */
} ScaleScratch;

#define SCALE_SCRATCH ((ScaleScratch *)0x1F800000)

/* The heat haze of mode 16: the shared quad pool (192 quads, one copy per
 * display buffer), the per-row wobble and the copy-back per buffer. */
typedef struct QuadBuffer {
    POLY_FT4 quads[0xC0];
} QuadBuffer;

extern QuadBuffer *D_8009D158[2];
extern u16 *D_8009D148;
extern DR_MOVE D_8009D164[2];

/* The scene modes' set-up and leave handlers (D_8009A058). */
void func_80077214(void), func_80077480(void); /* modes 8 and 11 */
void func_80077A64(void), func_80077CC0(void); /* mode 9 */
void func_80078A60(void), func_80078D24(void); /* mode 10 */
void func_8007BF50(void), func_8007C260(void); /* mode 12 */
void func_8007FF70(void), func_80080218(void); /* mode 13 */
void func_8007A5DC(void), func_8007A8AC(void); /* mode 14 */
void func_8007D918(void), func_8007DCE0(void); /* mode 15 */
void func_80080D00(void), func_8008106C(void); /* mode 16 */
void func_80082324(void), func_800826B4(void); /* mode 17 */
void func_8008355C(void), func_800837DC(void); /* mode 18 */

/* A resident sound call the world map declares itself (the camera flight's
 * engine volume): the shared headers leave it out, since other targets'
 * calls convert its arguments differently. */
void func_8003A2E4(s32 sound, s32 volume);

/* Their actors (start, update), by mode. The frame steps that end each
 * mode's list draw the scene. */
s32 func_80071A50(void), func_80071A58(void);           /* the open map's frame step */
s32 func_80076A14(void), func_80076A1C(void);           /* frame step without input */
s32 func_80078948(void), func_80078950(void);           /* frame step */
s32 func_8007756C(s32 index), func_800776E0(s32 index); /* 8, 11: the pad-steered camera */
s32 func_80077DC8(s32 index), func_80077E68(s32 index); /* 9: the camera flight */
s32 func_8007828C(s32 index), func_800783E8(s32 index); /* 9: the rig */
s32 func_80078E2C(s32 index), func_80078EA4(s32 index); /* 10 */
s32 func_800794D8(s32 index), func_80079538(s32 index);
s32 func_800795E4(s32 index), func_80079778(s32 index);
s32 func_8007A144(s32 index), func_8007A1B4(s32 index);
s32 func_8007A410(s32 index), func_8007A430(s32 index);
s32 func_8007A568(void), func_8007A570(s32 index);
s32 func_8007A9B4(s32 index), func_8007A9F8(s32 index); /* 14: the director */
s32 func_8007AD34(s32 index), func_8007ADD4(s32 index);
s32 func_8007B200(s32 index), func_8007B394(s32 index);
s32 func_8007B604(s32 index), func_8007B798(s32 index);
s32 func_8007BA08(void), func_8007BA10(s32 index);
s32 func_8007BB60(s32 index), func_8007BBEC(s32 index);
s32 func_8007C36C(s32 index), func_8007C3B8(s32 index); /* 12: the director */
s32 func_8007C724(s32 index), func_8007C7D8(s32 index);
s32 func_8007CC6C(s32 index), func_8007CD20(s32 index);
s32 func_8007CE84(s32 index), func_8007CF18(s32 index);
s32 func_8007D078(s32 index), func_8007D110(s32 index);
s32 func_8007D228(s32 index), func_8007D2B8(s32 index);
s32 func_8007D414(s32 index), func_8007D4A4(s32 index);
s32 func_8007D600(s32 index), func_8007D690(s32 index);
s32 func_8007D774(s32 index), func_8007D7FC(s32 index);
s32 func_8007DE14(s32 index), func_8007DE98(s32 index); /* 15: the director */
s32 func_8007E450(s32 index), func_8007E4E4(s32 index);
s32 func_8007ECA4(s32 index), func_8007EE34(s32 index);
s32 func_8007F8AC(s32 index), func_8007F968(s32 index);
s32 func_8007FC8C(s32 index), func_8007FD30(s32 index);
s32 func_8008032C(s32 index), func_80080370(s32 index); /* 13: the director */
s32 func_80080578(s32 index), func_80080600(s32 index);
s32 func_80080900(s32 index), func_80080944(s32 index);
s32 func_80080A28(s32 index), func_80080AC4(s32 index);
s32 func_80081174(s32 index), func_800811C0(s32 index); /* 16: the director */
s32 func_800813E8(s32 index), func_80081470(s32 index);
s32 func_800817A0(s32 index), func_80081868(s32 index);
s32 func_800819C8(s32 index), func_80081B24(s32 index);
s32 func_80081C3C(void), func_80081D80(void);
s32 func_80081FB4(s32 index), func_80081FD8(s32 index);
s32 func_800827C8(s32 index), func_80076B34(s32 index); /* 17: the actor script */
s32 func_800827EC(s32 index), func_800828DC(s32 index);
s32 func_80083214(s32 index), func_80083264(s32 index);
s32 func_800834D0(void), func_800834D8(s32 index);
s32 func_800838E8(s32 index);                           /* 18: the actor script */
s32 func_8008390C(s32 index), func_80083A00(s32 index);
s32 func_80083FE4(s32 index), func_80084068(s32 index);

#endif
