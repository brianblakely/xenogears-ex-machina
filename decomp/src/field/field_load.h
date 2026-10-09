#ifndef FIELD_FIELD_LOAD_H
#define FIELD_FIELD_LOAD_H

/* The field load and teardown (80070cc8, 800700b0, field.c): the map bundle
 * read ahead of the load, the sprite slots, the trigger zones, the texture
 * scrolls the events add and the panorama backdrop. */

#include "common.h"
#include "resident/gpu.h"
#include "resident/mode.h"

/* One sprite slot's VRAM area; slots with `shared` set keep their image. */
typedef struct {
    u16 x;
    u16 y;
    u16 unk4;
    s16 shared;
} SpriteSlot;

typedef struct SpriteSlotTable {
    SpriteSlot slot[32];
} SpriteSlotTable;

/* Components of the map bundle: sizes at +10c and offsets at +130. */
enum {
    BUNDLE_PALETTES,
    BUNDLE_COLLISION,
    BUNDLE_MODELS,
    BUNDLE_SPRITES,
    BUNDLE_IMAGES,
    BUNDLE_EVENTS,
    BUNDLE_ENCOUNTERS, /* the encounter set and its weights (resident/formation.h) */
    BUNDLE_MESSAGES,   /* the field message table (D_800ADBF0) */
    BUNDLE_ZONES
};

/* The map bundle read ahead of the field load (*8005a4e0). */
typedef struct {
    SpriteSlotTable slots; /* 000 */
    u8 unk100[0x10C - 0x100];
    s32 sizes[9];          /* 10C */
    s32 offsets[9];        /* 130 */
    s16 view[0x1C];        /* 154: lights and background (8006fdec) */
    u16 descriptor_count;  /* 18C */
    u16 unk18E;
    u16 descriptors[1];    /* 190: flags, rotation[3], position[3], model */
} FieldBundle;

/* The map bundle as the field load reads it (mode.h's block D_8005A4E0). */
#define FIELD_BUNDLE ((FieldBundle *)D_8005A4E0)

/* The size and the data of component k, read as words at their byte offsets
 * in the header (the field load reads them so, not as struct members). */
#define BUNDLE_SIZE(k) (*(s32 *)((u8 *)D_8005A4E0 + 0x10C + (k) * 4))
#define BUNDLE_COMPONENT(k) ((void *)(*(s32 *)((u8 *)D_8005A4E0 + 0x130 + (k) * 4) + (s32)D_8005A4E0))

extern SpriteSlotTable D_800B1F78; /* the bundle's sprite slots, kept by the load */
extern s32 D_800AFD10;         /* attributes before the first triangle */
extern u32 D_800ADB30;         /* heap top */

void func_80070CC8(void);      /* load the field from the bundle */
void func_800700B0(void);      /* tear the field down */

/* The map's own stream (file 0xb9 + 2 * map) in a four-sector ring. */
extern s32 D_800ADB60;         /* the stream is running */
extern void *D_800ADC14;       /* its ring */
void func_80070488(void);      /* start it unless one runs */
void func_80070508(void);      /* stop it and release the ring */

/* Trigger zone (field component 8): four x, y, z corners. */
typedef struct {
    s16 x;
    s16 y;
    s16 z;
} ZonePoint;

typedef struct {
    ZonePoint corner[4];
} Zone;

extern Zone *D_800ADBF4;       /* trigger zones */

/* Up to 32 texture scrolls (80027d64) created by func_800921E8. */
typedef struct WindowList {
    s16 count;
    TextureScroll *scrolls[32];
    u8 *buffers[32];
    s16 lengths[32];
} WindowList;

extern WindowList D_800AFEA8;
void func_800920D8(void);      /* run every scroll (80027eac) */

/* The panorama backdrop (resident 8002709c) built from the parameters events
 * set (D_800B0080, field_event.h). */
extern Panorama *D_800B007C;

#endif
