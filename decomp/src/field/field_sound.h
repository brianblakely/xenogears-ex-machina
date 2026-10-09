#ifndef FIELD_FIELD_SOUND_H
#define FIELD_FIELD_SOUND_H

/* Field music and sound effects (800854d0-80086d8c, field_800854D0.c): the
 * music-wave stream and the shared wave bank, the field's sound-effect bank,
 * sound effects and the three positional emitters. */

#include "common.h"
#include "psyq/libgte.h"

/* One 2 KiB music-wave stream chunk. */
typedef struct {
    u32 words[0x200];
} WaveChunk;

/* The music-wave stream: an eight-sector ring whose arrivals go to a chunk
 * callback (800859dc gathers four chunks into a wave bank). */
extern s32 D_800ADB2C;           /* the stream is running */
extern void *D_800ADBB8;         /* music-wave stream ring */
extern s32 D_800ADBBC;           /* stream arrivals */
extern void (*D_800AFEA4)(s32);  /* stream chunk callback */
extern void *D_800C3A1C;         /* music-wave gather buffer */
extern s32 D_800B2370;           /* music-wave chunks gathered */
extern s32 D_800AFC54;           /* the music's sequence read is still to start */
extern void *D_800B00E0;         /* shared wave bank buffer */

s32 func_800854D0(void);                 /* one stream step; -1 once finished */
void func_80085560(s32 file, s32 unused, void (*callback)(s32)); /* start a stream */
void func_800859DC(WaveChunk *chunk);    /* the music-wave chunk callback */
void func_80085B20(s32 music, s32 unused); /* change the field music */
s32 func_80085C90(s32 music);            /* advance its load; 0 once complete */
void func_80085EEC(void);                /* stop and release the cached sequence */
s32 func_80085F30(void);                 /* open the shared wave bank once read */
void func_80085FB8(void);                /* start reading the shared wave bank */
void func_80086024(void);                /* release the shared wave bank */

s32 func_8008A558(void);                 /* stop the stream once idle; -1 while busy */
void func_8008A520(void);                /* wait until the disc and the stream are idle */

/* Sound effects. */
void func_80085890();                    /* load the field's bank; called with an argument it ignores */
/* Instruction 0xb0 loads a wave bank into a resident slot (D_80062518). */
extern void *D_800AFD08;                 /* bank file being loaded */
extern s32 D_800AFD0C;                   /* bank file number */
extern s32 D_800AFD18;                   /* bank slot being loaded */
void func_80085988(void);                /* release the field's bank */
void func_800855C8(s32 id, s32 volume, s32 pan, s32 channel);
void func_80085634(s32 id, s32 channel); /* at full volume and centre pan; 0 stops */

/* The three positional emitters (800afe88): each follows a descriptor, with
 * a volume by distance and a pan by screen x. */
typedef struct EmitterSlot {
    u16 actor;  /* descriptor the sound follows */
    u16 sound;  /* 0xffff when free */
    u16 unk4;
} EmitterSlot;

extern EmitterSlot D_800AFE88[3];

void func_800862CC(s32 sound, s32 volume, s32 unused, s32 distance, s32 actor); /* start one */
void func_800863E8(s32 id);              /* stop the one following descriptor `id` */
void func_800864B4(void);                /* clear the slots */
void func_800864F0(void);                /* clear them and stop their voices */
void func_80086590(VECTOR *target);      /* keep those of the three actors nearest `target` */
void func_80086908(void);                /* point the listener */
void func_80086BA8(void);                /* follow the actors' positions */
void func_80086A1C(s32 emitter, s32 *position);
void func_80086078(s32 distance, u32 *out, s32 volume); /* volume at `distance` */
void func_80086200(s32 index, s32 *x, s32 *y); /* screen position of descriptor `index` */

#endif
