#ifndef RESIDENT_SOUND_DRIVER_H
#define RESIDENT_SOUND_DRIVER_H

#include "common.h"
#include "psyq/libspu.h"
#include "resident/sound.h"

/* The sound driver's own state and calls between its two units, the end of
 * main2_800366E0.c (80037b88-80039e18: start-up, banks, SPU memory, volumes)
 * and sound.c (80039e18-8003f738). No other unit or target uses them;
 * resident/sound.h has the driver's interface. */

/* A voice whose volume pair follows the output mode (D_80059518). */
typedef struct SoundModeVoice {
    u16 flags;         /* bit 0: in use */
    u8 unk2[0x10];
    u16 volume;
    u8 unk14[0x22];
    s16 unk36;
    s16 left;
    s16 right;
    u8 unk3C[0x26];
    s16 unk62;
    s16 unk64;
    s16 unk66;
} SoundModeVoice;

/* One queued SPU transfer (the ring D_80059458 holds eight). */
typedef struct SoundTransfer {
    u16 type;          /* 1: write, 2: read, 3/4: read decoded CD data */
    u16 unk2;
    u8 *data;
    u32 address;       /* SPU address */
    s32 size;
    void (*callback)(void);
} SoundTransfer;

/* The driver state both units use: commons, and sound.c's reverb sizes. */
extern SoundSeq *D_80059564;      /* playing sequences */
extern SoundSeq *D_800595D8;      /* the sound effect channels */
extern u32 D_80059504;            /* the driver's tick count, the effects' start clock */
extern u32 D_800594FC;            /* voices held (keyed on) */
extern u32 D_80059550;            /* voices to key off */
extern u32 D_80059554;            /* voices whose registers changed */
extern s32 D_80059514;            /* SPU interrupts counted */
extern void (*D_8005950C)(void);  /* the SPU interrupt hook (8003c010) */
extern s32 D_800595C4;            /* root counter time spent in ticks */
extern s32 D_80059540;            /* timed ticks */
extern u16 D_8005955C;            /* pending SPU IRQ re-enable */
extern s32 D_800594E4;            /* random state */
extern SoundTransfer *D_80059458; /* the SPU transfer ring */
extern u16 D_800594F4;            /* transfer ring write index */
extern u16 D_80059510;            /* transfer ring read index */
extern s16 D_80059548;            /* result of the last decoded-data read */
extern s32 D_800595A4;            /* the zeroed transfer buffer */
extern s32 D_800595DC;            /* next SPU address to clear */
extern s32 D_800595E0;            /* bytes left to clear */
extern s32 D_80059584;            /* SPU address of a streamed wave bank's next part */
extern s32 D_80059588;            /* bytes of it still missing */
extern u32 D_800594D8;            /* SPU address of the reverb work area, -1 none */
extern SpuVolume D_8005940C;      /* reverb depth */
extern s32 D_800508E8[10];        /* reverb work area size of each reverb type */
extern SoundModeVoice *D_80059518;
extern s32 D_8005951C;            /* size of the driver memory pool */
extern u8 D_80065B0C[0x6300];     /* the driver memory pool */
extern u8 D_8006FAC8[0x28];       /* the SPU memory management table (SpuInitMalloc, 4 blocks) */

/* Calls between the two sound units. */
void func_80038934(s32 type, s32 depth, s32 delay, s32 feedback); /* set the reverb */
s32 func_8003C020(void);           /* the driver tick (root counter 2 event) */
void func_8003BB64(void);          /* SPU transfer callback */
void func_8003BFA0(void);          /* SPU interrupt callback */
void func_8003E700(void);
SoundSeq *func_8003B148(s32 count); /* create the sound effect channels */

#endif
