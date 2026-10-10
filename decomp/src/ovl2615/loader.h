#ifndef OVL2615_LOADER_H
#define OVL2615_LOADER_H

/* The setup module's loading task and enemy set loader (battle_loader.c):
 * the loading task, the enemy set file's entries, the member sprite files,
 * the battle sprite as the loader reads it, and the calls it makes. The
 * battle overlay's slots, gear object loads, start mode and enemy set copy
 * come from the shared battle headers. */

#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/flow.h"
#include "resident/cd.h"
#include "resident/heap.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "ovl2615.h"

/* The battle's sprite (the resident Sprite that BattleArea.sprites points
 * to) as the loader reads it: the whole parts of its 16.16 position, its
 * renderer, image binding, sequencer, floor height, countdown and saved
 * position, and its slot, split over bits 30-31 at +0xa8 and bits 0-1 at
 * +0xac (the resident's frame_bits.unknown30 and motion.bits.unknown0). */
typedef struct BattleSprite {
    s16 pad0;
    s16 x;             /* +02: whole parts of 16.16 coordinates */
    s16 pad4;
    s16 y;             /* +06 */
    s16 pad8;
    s16 z;             /* +0a */
    u8 padC[0x14];
    SpriteRenderer *renderer; /* +20 */
    u16 *image;        /* +24: +4 image x, +6 image y */
    u8 pad28[0x54];
    u32 *sequencer;    /* +7c: +0 sequencer word, +e image position */
    u8 pad80[4];
    s16 ground;        /* +84: resting height */
    u8 pad86[0x18];
    s16 countdown;     /* +9e */
    s16 target[3];     /* +a0 */
    u8 padA6[2];
    u32 flagsA8 : 30;  /* +a8 */
    u32 slotLow : 2;   /* +a8 bits 30-31: the slot's low bits */
    u32 slotHigh : 2;  /* +ac bits 0-1: the slot's high bits */
    u32 flagsAC : 30;
} BattleSprite;

/* Callers convert arguments differently from the resident definition (s16
 * angles and positions, a by-value image position the resident takes as an
 * SVECTOR, u16 coordinates; a narrow result): sprite placement and
 * orientation, an image list upload, the resource binding and the sound
 * transfer test. */
void sprite_set_position_xz(BattleSprite *sprite, s16 x, s16 z);   /* place a sprite */
void sprite_set_facing(BattleSprite *sprite, s32 angle);       /* sprite orientation */
void sprite_set_direction(BattleSprite *sprite, s32 angle);       /* sprite heading */
void sprite_upload_images_side_by_side(void *images, s32 x, s32 y);            /* upload an image list */
void sprite_resolve_resource(void *binding, void *data, DVECTOR image, DVECTOR clut, s32 mode);
s16 sound_sync_transfer(s32 arg);                                /* sound transfer busy */

/* The loading task (801e7098), run once per frame until the sprites, the
 * battle images and the effect bank are loaded. */
typedef struct {
    Task task;           /* +00 */
    u32 unk1C;
    u8 *data;            /* +20: the enemy set file */
    void *shared;        /* +24: battle file 2 */
    void *images;        /* +28: battle file 1 */
    void *effects;       /* +2c: battle file 3 */
    FileRequest members[4]; /* +30: the members' sprite files */
    FileRequest files[4];   /* +50: battle files 1-3 */
    u8 pad70[0x20];
    s32 timer;           /* +90: frames before the settle check */
} LoaderTask;

/* An enemy set file entry (12 bytes). */
typedef struct {
    s32 offset;          /* +0: sprite data offset */
    u32 images;          /* +4: image list offset, or an earlier list's index */
    u8 model;            /* +8: nonzero for a 3D model */
    u8 pad9[2];
    u8 variant;          /* +b */
} EnemyEntry;

/* Member sprite files by type (directory 2c). */
typedef struct {
    s32 file;
    u32 sequence;        /* the sprite's sequencer word */
} MemberFile;

/* Battle file 2's block, in the first word of what the battle declares as
 * the party panel's maximum digits (u8[7], battle/graphics.h), which the
 * battle releases from there (800b8098). */
extern void *battle_file2_block_and_max_hp_digits;

/* The battle overlay's sprite task for a sprite row: a resident task whose
 * data is the sprite (BattleArea.tasks and .sprites take the two). */
Task *battle_sprite_task_create(void *resource, s32 clut_x, s32 clut_y, s16 texture_x, s16 texture_y, s32 unused5,
                    s32 x, s32 y, s32 z, s32 animation, s32 direction, s32 unused11, s32 unused12,
                    s32 palette_bank);
void battle_object_follower_create(s32 slot);
void battle_enable_shadows(void);

void func_801E6314(u8 *data);
void func_801E6710(u8 *data);
void func_801E67A4(s32 slot, s32 row, s32 animation);
void func_801E693C(FileRequest *list);
void func_801E6A4C(void);
void func_801E6AC4(void);
void func_801E6C80(Task *node);
void func_801E6D34(Task *node);
void func_801E6D6C(Task *node);
void func_801E6DC8(void);
void func_801E6E48(Task *node);
void func_801E6F00(Task *node);
void func_801E6FEC(Task *node);
void func_801E7098(u8 *data);

#endif
