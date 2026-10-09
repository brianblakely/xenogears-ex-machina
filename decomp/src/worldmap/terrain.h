#ifndef WORLDMAP_TERRAIN_H
#define WORLDMAP_TERRAIN_H

/* The world map terrain: the queries of its cells, heights and slopes, the
 * path regions, the wrapping of positions on the area (worldmap_80090A84),
 * the movement over terrain and solid objects (worldmap_80094A5C), the 9x9
 * blocks streamed around the camera and drawn in 5x5 (worldmap_80094A5C) and
 * the billboards standing on them (worldmap_80083A00). */

#include "worldmap.h"
#include "camera.h"

/* A path region (16 bytes) of an area file's path tables or the fixed ones:
 * its bounds, the scene and entry a world-map exit from it stores (game data
 * map and entry[2]), its path link (-1 none) and its kind: 1 leaves
 * without the button (func_80090A84), 2 makes a flying vehicle descend first
 * (func_8008E190), 3 takes the exit from the camera target's region of path
 * table 3 (func_80070CFC), 4 only records a destination (func_80094238). A
 * scene of -1 ends a table. */
typedef struct PathRegion {
    s16 x, z, w, h;
    s16 scene;
    s16 entry;
    s16 link; /* 0x0C: path or destination id */
    s16 kind; /* 0x0E */
} PathRegion;

extern PathRegion D_8009B6C4[3];
extern PathRegion *D_8009D7D8; /* the current path, -1 none */

s32 func_80094238(VECTOR *position, s32 table);
s32 func_80094364(VECTOR *position, s32 table, s32 kind);

/* Terrain queries (worldmap_80090A84). */
void func_80093354(VECTOR *position);  /* wrap a position (20.12) onto the area */
void func_80093484(VECTOR *offset);    /* wrap an offset (20.12) */
void func_80093534(VECTOR *delta);     /* wrap a world-unit offset */
void func_800935DC(VECTOR *point, VECTOR *origin, VECTOR *normal);
u8 *func_80093660(s32 x, s32 z);       /* terrain cell at a position */
void func_80093740(VECTOR *normal, s32 x, s32 z); /* ground normal */
s32 func_80093978(s32 x, s32 z);       /* ground height at a position */
s32 func_80093A5C(s32 x, s32 z);       /* terrain height */
s32 func_80093E8C(VECTOR *position);   /* terrain attribute at a position */
s16 func_80093F18(VECTOR *position);
s32 func_80094028(VECTOR *position);
s16 func_80094060(s16 row, s16 column);
s32 func_80094088(VECTOR *position, VECTOR *direction, VECTOR *out);
s32 func_80094154(VECTOR *a, VECTOR *b); /* distance */
void func_800941C4(VECTOR *from, VECTOR *to, VECTOR *direction, s16 *heading);
s32 func_8009443C(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 func_800945C8(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 func_80094750(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);
s32 func_800948D8(VECTOR *origin, VECTOR *direction, VECTOR *step, s16 row);

extern s16 D_8009BAC8[]; /* 8 columns per row */
extern s32 D_8009C5BC, D_8009C618; /* water wave phases: x, z */

/* Movement over the terrain and the solid scene objects (worldmap_80094A5C). */
s32 func_80094A5C(VECTOR *position, VECTOR *direction, s32 scale, s32 mode);
s32 func_80095414(VECTOR *position, VECTOR *direction, VECTOR *hit, s32 range, s32 mode);
s32 func_80095CD4(VECTOR *position, VECTOR *direction, VECTOR *out, s32 scale, s32 mode);

/* Terrain streaming origin (world units, wrapped to the map) and the block
 * cell the camera is in. */
extern u8 D_8009BBB4[];
#define TERRAIN_ORIGIN (*(VECTOR *)D_8009BBB4)
#define GROUND_SCROLL ((s32 *)D_8009BBB4) /* ground scroll offset x, y, z */
extern SVECTOR D_8009C838; /* block cell */
extern s16 D_8009D558;     /* the map edges the camera crossed (8/4 x, 2/1 z) */

/* 9x9 terrain blocks around the camera: block numbers, row-major. */
typedef struct BlockGrid {
    s16 cells[81];
} BlockGrid;

extern BlockGrid D_8009D570; /* current */
extern BlockGrid D_8009D318; /* previous */
extern void *D_8009C184[0x100]; /* terrain block buffers */
extern s16 D_8009D618[25];      /* 5x5 visible blocks; -1 empty */
extern s16 D_8009D650[25][4];   /* per block: visibility of its 4 quarters */
extern s32 D_8009D7DC;          /* terrain POLY_FT3 packets used this frame */
extern MATRIX D_8009D534;
extern VECTOR D_8009C7F0, D_8009C828, D_8009C844, D_8009C874; /* horizon plane normals */

/* Terrain palettes: 64 CLUT ids (two 256-colour palettes faded in 32 steps
 * towards the background colour) and seven texture pages. */
extern u16 D_8009CCB4[0x40];
extern u16 D_8009CD54[7];

extern s16 D_800523F0[0x1000][2]; /* PsyQ rcossin_tbl: sine, cosine */

void func_800978FC(void); /* allocate the terrain packets */
void func_800979C8(void); /* upload the terrain image, build its palettes */
void func_80097BC0(VECTOR *position); /* reset the loader around a position */
void func_80097CB8(Camera *camera);   /* ... around the camera */
void func_80097D64(void); /* free the loaded blocks */
void func_80098044(void); /* the horizon plane normals */
void func_800980D4(void *);
void func_800981C8(Camera *);
void func_800983A0(Camera *);
void func_80098CC0(void);
void func_8009932C(u32 *ot, s32, Camera *);

/* The billboards standing on the terrain blocks (worldmap_80083A00): each
 * block's list (data and count) of the area data's billboard section, their
 * quads per display buffer and CLUTs. */
typedef struct TerrainTexture {
    u8 *data;
    s32 unk4;
} TerrainTexture;

extern TerrainTexture *D_8009C7EC;
extern void *D_8009D7E8[2]; /* billboard quads, per display buffer */
extern s16 D_8009BE04;     /* quads used this frame; a word in 80099BFC */
extern u16 D_8009D478[16]; /* billboard CLUTs */

void func_80085F58(void); /* resolve the lists, create the CLUTs */
void func_80085FE0(void); /* allocate the quads */
void func_80086124(void); /* free them */
void func_8008615C(void); /* draw the billboards */
void func_80099BFC(u8 *data, s32 count, u32 *ot, POLY_FT4 *quads); /* draw billboards (assembly) */

#endif
