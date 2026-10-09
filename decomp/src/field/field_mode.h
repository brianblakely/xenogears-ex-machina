#ifndef FIELD_FIELD_MODE_H
#define FIELD_FIELD_MODE_H

/* The field mode (field.c): its frame, the requests that leave it (menus,
 * battles, map changes) and random encounters, the play record, and the
 * control words the events test before they let such a request through
 * (field_800854D0.c). */

#include "common.h"
#include "field.h"

/* The field loop's control words; an event that requests a transition waits
 * until 800adbdc and 800adbe4 are set (with 800adbec for map changes and
 * battles) and 800adb2c and 800adb90 are clear. */
extern s32 D_800ADBD8;
extern s32 D_800ADBDC;
extern s32 D_800ADBE0;
extern s32 D_800ADBE4;
extern s32 D_800ADBE8;
extern s32 D_800ADBEC;         /* publish the field id on the next walk */
extern s32 D_800ADB18;         /* set by event 800933f8: the exit skips the state save */
extern s32 D_800ADBD0;         /* 1 when the field may leave (80077e10) */
extern s32 D_800ADBD4;         /* set once the field left for a battle */
extern s32 D_800ADB64;         /* requested menu (scripts 0-6, menu button 0x80), 0xff none */
extern u16 D_800B236C;         /* menu parameter set by ext 99; the field loop and ext 55
                                * pass it to the menu (80059171) */
extern s32 D_800ADB70;         /* movie requested */
extern s32 D_800ADB88;         /* an event's wait (8008a244 yields while it is set) */
extern s32 D_800ADB8C;         /* set while the party is rebuilt: no effects or scrolls start */
extern s32 D_800ADB90;         /* an actor block is being read */
extern s32 D_800AFD14;         /* map change: the transition's frames */
extern s32 D_800B0048;         /* map change: the transition kind */
extern s32 D_800B0064;         /* the kind of the next exit (8007954c) */
extern s32 D_800AFC78;         /* 8004f324 as the field was left, restored on return */
extern s32 D_800AFE84;         /* after a movie or a menu, nonzero sets 800adb50 (80089f94) */
extern u8 D_800ADB04;          /* random encounters enabled */
extern u8 D_800ADB05;          /* 1 while character drawing is off */
extern s16 D_800ADB54;         /* 1 once an event switched to the 640-wide screen */
extern s32 D_800ADB4C;         /* set while the second ordering tables are drawn */
extern s32 D_800ADB9C;         /* frame start time */
extern s32 D_800ADBA0;         /* frame draw (CPU) time */
extern s32 D_800ADBA4;         /* GPU time (the VSync counter, 8007781c) */
extern s32 D_800B14A4;         /* only cleared (800705dc) */

void func_80078D44(void);      /* the field entry */
void func_80077DAC(void);      /* the pre-frame work */
s32 func_80078B5C(void);       /* the post-frame work */
void func_8007554C(void);      /* one field frame */
void func_8008110C(void);      /* the field update: events, then every actor's motion */
void func_80071F64(s32 x, s32 y, s32 w, s32 h); /* both draw buffers' clip areas */
void func_80086D8C(void);      /* both draw blocks' display areas */
void func_800775F8(void);      /* DrawSync, then VSync */
void func_8007999C(void);      /* sync, then flush the instruction cache */
void func_80078C5C(void);      /* brighten the text strip (with 800b2344 set) */
s32 func_80079288(void);       /* count down the random-encounter steps */
void func_800798BC(void);      /* set the battle-entry flag (80059179) */
s32 func_80078BC8(void);       /* 0 when nothing keeps the field from leaving */
void func_800799D4(void);      /* run a menu over the field */
void func_8007954C(s32 kind);  /* leave the field for another mode */
void func_800A2714(void);      /* reload the actors' extra blocks after a return */
void func_800931F8(void);      /* empty; the map-change events call it */
void func_800A24C4(void);      /* after a return to the field */
void func_800A28D4(void);      /* rebuild the actors after a return (D_8004F30C set) */

/* The field state the field writes to the resident's snapshot block
 * (D_8005A4E4) when it leaves (800a3f4c) and reads back on return
 * (800a3474), through a cursor. */
extern u8 *D_800AFC50;         /* the snapshot cursor */
void func_800A3F4C(void);      /* write the snapshot */
void func_800A3474(void);      /* read it back */

/* The view's world block (800afa54, 0x74 bytes) as copied byte-wise. */
typedef struct {
    u8 bytes[0x74];
} ViewSnapshot;

/* Copy `size` bytes as one unaligned block (a byte-struct assignment). */
#define COPY_BLOCK(destination, source, size)                    \
    {                                                            \
        typedef struct {                                         \
            u8 bytes[size];                                      \
        } Block;                                                 \
        *(Block *)(destination) = *(Block *)(source);            \
    }

/* The play record (800a31e8). */
extern u8 D_800B02C8;          /* 1 stops the record */
extern u16 D_800AFC6C;         /* buttons held since the last record */
void func_800A31E8(void);      /* update the play record */
void func_800A30FC(void);      /* record the map and camera, save the event variables */

#endif
