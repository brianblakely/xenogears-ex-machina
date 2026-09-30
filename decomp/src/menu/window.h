#ifndef MENU_WINDOW_H
#define MENU_WINDOW_H

#include "menu.h"

/* A centred one-line caption: its text image and one sprite per buffer. */
typedef struct {
    s32 image;         /* 0x00 */
    Sprite sprite[2];  /* 0x04 */
    s16 width;         /* 0x2C */
    s16 x;             /* 0x2E */
} Caption;

/* One line of a choice menu. */
typedef struct {
    u8 flags;          /* 0x00: 1 = runs every frame, 2 = pad to the widest
                        * line, 4 = skipped by the cursor */
    u8 caption;        /* 0x01: caption text while selected */
    u8 unk2[2];
    s32 text;          /* 0x04 */
    void (*handler)(s32 arg); /* 0x08: run on confirm */
    s32 arg;           /* 0x0C */
    u8 unk10[2];
    s16 half_width;    /* 0x12 */
} MenuItem;

typedef struct Menu Menu;

/* A choice menu drawn on a translucent panel (eight in D_800915AC). */
struct Menu {
    u8 title_width;    /* 0x00: nonzero shifts the lines right */
    u8 unk1[3];
    MenuItem *items;   /* 0x04 */
    s16 count;         /* 0x08 */
    s16 parent;        /* 0x0A: menu index returned to on cancel */
    void (*draw)(Menu *menu); /* 0x0C */
    u8 unk10[2];
    s16 cursor;        /* 0x12 */
    s16 y;             /* 0x14 */
    s16 x;             /* 0x16 */
    u8 unk18[4];
    Tile panel[2];     /* 0x1C: one per buffer */
};

/* A 3D panel's per-buffer packet block. */
typedef struct {
    u32 unk0;
    u8 unk4[4];
    u8 unk8[4];
} PanelPacket;

typedef struct {
    s32 unk0;
    PanelPacket *buffers[2];
} Panel;

typedef struct {
    u8 unk0[0x284];
    Panel *panel;      /* 0x284 */
} PanelOwner;

/* Per-buffer overlay packets (map screen). */
typedef struct {
    u32 unk0[2];
    u8 packets[0x308];
} OverlayBuffer;

extern OverlayBuffer D_8009A2F8[2];
extern u8 *D_800927CC; /* per map row: right edge of the drawn span */
extern u8 *D_800927D0; /* per map row: left edge of the drawn span */
extern u8 D_80091834[]; /* per map row: leftmost allowed column */
extern u8 D_800918B4[]; /* per map row: rightmost allowed column */

void func_80085EC8(OverlayBuffer *buffer);
s32 func_8008F530(s32 entry, s32 which);

/* Flat line packet (libgpu LINE_F2). */
typedef struct {
    u32 tag;
    u8 r0, g0, b0, code;
    s16 x0, y0;
    s16 x1, y1;
} LineF2;

/* A 3D debug line with its packets (one per buffer). */
typedef struct {
    LineF2 packets[2]; /* 0x00 */
    SVector from;      /* 0x20 */
    SVector to;        /* 0x28 */
    s16 timer;         /* 0x30: frames left, 0 = free */
    s16 pad;
} Line3D;

extern Line3D D_80095938[100];
extern Vector D_80096FA8; /* view origin */
void func_800316C0(void *ot, void *packet);

s32 func_8002DC9C(s32 x, s32 y, s32 z);
void func_80048D7C(Vector *vector, void *out);

/* A recorded path position. */
typedef struct {
    s16 x, y, z;
    u8 unk6[0x62];
} PathPoint;

extern PathPoint D_8009A988[0x1F];
extern s32 D_800928F8; /* recorded path points */
extern PolyFT3 *D_80092854[2]; /* triangle pools: template, working copy */
extern u16 D_800927D4; /* backdrop texture page */
extern u16 D_800927D8; /* backdrop palette */
extern u8 D_800927DC;  /* backdrop texel u */
extern u8 D_800927E0;  /* backdrop texel v */

void func_800732AC(void *dst, void *src, s32 size); /* copy bytes */

extern u32 D_80059598; /* resident map colour (r, g, b, code) */
void func_80072D18(s32 arg0, s32 arg1, s32 arg2);

/* Stage colours (17 bytes each). */
typedef struct {
    u8 top[3];         /* sky gradient top */
    u8 unk3;
    u8 unk4, unk5, unk6;
    u8 unk7;
    u8 bottom[3];      /* sky gradient bottom, far and fade colour */
    u8 unkB;
    u8 back[3];        /* back colour */
    u8 unkF;
    u8 dim;            /* 0x10: halve the actor glow */
} Environment;

extern Environment D_8009178C[];
extern u8 D_800928B4;          /* stage */
extern Environment *D_8009288C; /* current stage colours */
extern s32 D_8009291C;
extern s32 D_80092910;
extern s32 D_80092908;
extern PolyG4 D_80095580[2];   /* sky gradient, one per buffer */
void func_8002C6E0(s32 r, s32 g, s32 b); /* back colour */
void func_8004A10C(s32 r, s32 g, s32 b); /* far colour */
void func_80048AB0(s32 near, s32 far, s32 arg);

/* Stage floor. */
typedef struct {
    u8 unk0[0x38];
    void *backdrop_tim;   /* 0x38 */
    u8 unk3C[0xC];
    void *icon_tims[4];   /* 0x48 */
    u8 unk58[0x14];
    void *floor_tim;      /* 0x6C */
    void *extra_tims[9];  /* 0x70 */
} StageFiles;

/* Texture page and palette of an icon. */
typedef struct {
    u16 tpage;
    u16 clut;
} TexRef;

/* Map drawing table copied into the scratchpad; ends with the icons. */
typedef struct {
    u8 unk0[0x20];
    TexRef icons[4];   /* 0x20 */
} MapTable;

extern MapTable D_80091934;

extern u16 D_800927A0; /* floor palette */
extern u16 D_800927A4; /* floor texture page */
extern u16 D_800927A8; /* floor texture row */
extern PolyFT4 *D_80092788[2]; /* floor quad pools: template, working copy */
void func_800471B4(void *tim); /* open a TIM */
void func_800471C4(TimImage *image); /* read the next TIM image */
u16 func_80043A58(s32 x, s32 y); /* palette id */
u16 func_80043A1C(s32 mode, s32 rate, s32 x, s32 y); /* texture page id */

/* A three-part gauge bar. */
typedef struct {
    PolyF4 parts[3];
} GaugeBar;

void func_80085E90(s32 mirrored, s16 *out, s32 x);
void func_80085EAC(s32 mirrored, s16 *out, s32 y);

/* Fading overlay. */
extern s16 D_80092780; /* fade level */
extern s32 D_80092784;
extern void *D_80092938; /* overlay packets */
extern s32 D_80092948;
extern s32 D_8009292C;

extern u8 D_8009273C; /* caption text shown in D_80095510 */
extern u8 D_80092740; /* caption text shown in D_80095540 */
extern u8 D_800928A0; /* buffer being built */
extern s32 D_80092880;
extern Caption D_80095510;
extern Caption D_80095540;
extern u8 D_80095570[2][8]; /* per-buffer mode packets for the captions */
extern Menu D_800915AC[8];
extern Menu *D_80092734; /* menu being shown */
extern s32 D_80092700;
extern s32 D_80092744; /* caption of the selected line */
extern s32 D_80092748; /* pad buttons held */
extern s32 D_8009274C; /* pad buttons repeated */
extern s32 D_80092750; /* pad buttons pressed */
extern s32 D_80091364; /* pad port of the menu input */
extern u8 D_80092764;  /* stick is deflected */
/* Resident pad state, per port. */
extern u16 D_80059570, D_80059574;
extern u16 D_800594A4, D_800594A8;
extern u16 D_8005948C, D_80059490;
extern u8 D_80059438, D_8005943C; /* stick x */
extern u8 D_80059430, D_80059434; /* stick y */
extern s32 D_80092704;

extern u8 D_8009275C;  /* menu refresh pending */
extern u8 D_800926FC;
extern u8 D_800928FC;  /* pad port driving the menus */
extern Tile D_8009A1C0; /* screen fade tile, buffer 0 */
extern Tile D_8009A2B8; /* screen fade tile, buffer 1 */
extern u16 D_8009286C;  /* screen height */

/* Map view. */
extern SVector D_80092768; /* stored map position */
extern s32 *D_800928DC;    /* 128 x 128 map cells; low half is the height */

void func_80080AE8(void);
void func_80036420(void);
s32 func_80081A44(void);
void func_80087698(s32 x0, s32 y0, s32 x1, s32 y1); /* draw a line */
void func_80044764(Rect *rect, s32 r, s32 g, s32 b); /* clear a VRAM area */
void func_800445D0(s32 mode); /* wait for drawing */
void func_8008AC0C(Panel *panel);
void func_8008AE1C(Panel *panel);
void func_80080D20(void *packets);
void func_80086E24(void);
void func_8008E3CC(void *packets, s32 level, s32 arg);
void func_8008E120(void);
void func_8007F258(void *packets, s32 arg);
void func_8008BC04(void);
void func_8003A838(s32 arg0, s32 arg1, s32 arg2);
void func_8008E064(void);
s32 func_80033728(s32 table, s32 index); /* text string of an index */
s32 func_80034EAC(s32 string, s32 image, s32 colour, s32 arg); /* returns width */
void func_80043B48(void *ot, void *packet); /* link a packet into an ordering table */
void func_80044894(Rect *rect, void *pixels); /* load pixels into VRAM */
s32 func_8007EB6C(s32 text);
void func_8007EE08(s32 arg);
void func_80080F04(void);
void func_8007E894(s32 x, s32 y);
void func_8007F948(Menu *menu, s32 line);
void func_8007EBE0(s32 text);
void func_8007ED84(s32 text, s32 half_width);
s32 func_80035734(s32 port); /* pad type */
void func_8008EB4C(s32 sound);
void func_80080964(s32 menu);

#endif
