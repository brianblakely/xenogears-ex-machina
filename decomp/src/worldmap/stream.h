#ifndef WORLDMAP_STREAM_H
#define WORLDMAP_STREAM_H

/* The world map's stream reader (worldmap_80094A5C), which reads the terrain
 * blocks around the camera in the background: a ring of 16 request lists,
 * from disc (sectors read through the CD callbacks) or from the host's files
 * (PC file server), each list sorted by position. */

#include "worldmap.h"
#include "psyq/libcd.h"

/* A disc read request: sector, bytes and destination. */
typedef struct DiscReadRequest {
    s32 sector;
    s32 bytes;
    u8 *destination;
} DiscReadRequest;

/* A host-file read request: name, offset, bytes and destination. */
typedef struct HostReadRequest {
    char *path;
    s32 offset;
    s32 bytes;
    u8 *destination;
} HostReadRequest;

extern void *D_8009BE08, *D_8009D3C0; /* disc and host-file request buffers */
extern DiscReadRequest *D_8009D788[16]; /* submitted disc request lists */
extern HostReadRequest *D_8009C624[16]; /* submitted host-file request lists */
extern s32 D_8009D808, D_8009BE44, D_8009BCB8; /* ring positions */
extern s8 D_8009C588[8];
extern s32 D_8009CD44, D_8009BD2C; /* reader state and its wait */

/* The disc request being read. */
extern DiscReadRequest *volatile D_8009D3BC; /* next disc request (shared with the CD callbacks) */
extern s32 D_8009BE48, D_8009CCB0, D_8009CCA8, D_8009CCA0;
extern s32 D_8009D7F4, D_8009D614, D_8009CEB8;
extern u8 *D_8009C590;    /* destination of the next sector's data */
extern u32 D_8009D56C;    /* sectors left */
extern s32 D_8009BCCC[3]; /* sector header */
extern CdlLOC D_8009CEBC; /* request position */
extern void *D_8009D7D4;  /* unused bytes drained from the final CD sector */

void func_80095F78(void);  /* reset the queue, allocate its buffers */
void func_800960BC(void);  /* free them */
void func_80096130(void);  /* wait for a free write slot */
s32 func_8009623C(s32 sector, s32 bytes, u8 *destination);
s32 func_800962B0(char *path, s32 offset, s32 bytes, u8 *destination);
s32 func_80096328(void);
s32 func_800965A4(void);
s32 func_80096668(void);   /* lists queued */
void func_80096694(void);  /* drain the queue */
s32 func_800967E4(void);   /* stream step: func_800968E0 status */

#endif
