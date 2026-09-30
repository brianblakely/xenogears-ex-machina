#ifndef MENU_WINDOW_H
#define MENU_WINDOW_H

#include "menu.h"
#include "spark.h"

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
    TileRgb panel[2];  /* 0x1C: one per buffer */
};

extern s32 D_800911D4; /* second actor also posed by func_8007661C */
extern s32 D_800928B0; /* selects the look-at marker (func_80082300 or func_80082178) */

/* A playable character's record (12 bytes). */
typedef struct {
    char *name;
    u8 unk4[8];
} Character;

extern Character D_8009196C[];

/* Bout-end sequence effects. */
extern Vector D_800929F4[3]; /* sparking embers; pad counts down to the next spark */
extern s16 D_8009260C;       /* knock-down flash level */
void func_8008E2B8(u32 *ot, s32 level, s32 subtract);
void func_8003463C(MenuWindow *window);
void func_800851D4(void);

/* libgpu DR_TPAGE. */
typedef struct {
    u32 tag;
    u32 code[1];
} DrawTPage;

/* Per-buffer overlay packets (map screen, 0x310 bytes). */
typedef struct {
    DrawTPage tpage[2];   /* 0x000 */
    LineF4 frame[4];      /* 0x010: map frame outline, both sides */
    PolyF4 bars[6];       /* 0x080: gauge backgrounds */
    DrawTPage bar_tpage;  /* 0x110 */
    PolyF4 bars_dim[6];   /* 0x118 */
    PolyF4 bars_lit[6];   /* 0x1A8 */
    PolyF3 arrows[2][3];  /* 0x238: three per side */
    PolyF4 marks[4];      /* 0x2B0 */
} OverlayBuffer;

extern OverlayBuffer D_8009A2F8[2];
extern DVector D_800917F4[8]; /* map frame corner layout */
void MargePrim(void *packet, void *next); /* chain two packets */
extern u8 *D_800927CC; /* per map row: right edge of the drawn span */
extern u8 *D_800927D0; /* per map row: left edge of the drawn span */
extern u8 D_80091834[]; /* per map row: leftmost allowed column */
extern u8 D_800918B4[]; /* per map row: rightmost allowed column */

void func_80085EC8(OverlayBuffer *buffer);
s32 func_8008F530(Actor *actor, s32 which);

/* A 3D debug line with its packets (one per buffer). */
typedef struct {
    LineF2Tag packets[2]; /* 0x00 */
    SVector from;      /* 0x20 */
    SVector to;        /* 0x28 */
    s16 timer;         /* 0x30: frames left, 0 = free */
    s16 pad;
} Line3D;

extern Line3D D_80095938[100];
extern Vector D_80096FA8; /* view origin */

s32 func_8002DC9C(s32 x, s32 y, s32 z);

/* A recorded path position and its debug marker: three axis lines (red
 * x, green y, blue z) per buffer. */
typedef struct {
    LineF2Tag axes[2][3]; /* 0x00 */
    s16 x, y, z;          /* 0x60 */
    u8 pad[2];
} PathPoint;

extern PathPoint D_8009A928[0x1F];
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
void SetFogNearFar(s32 near, s32 far, s32 arg);

/* Stage floor. */
typedef struct {
    u8 unk0[0x38];
    void *backdrop_tim;   /* 0x38 */
    u8 unk3C[0xC];
    void *icon_tims[4];   /* 0x48 */
    void *name_tim;       /* 0x58 */
    void *bar_tim;        /* 0x5C */
    u8 unk60[0xC];
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
u16 GetTPage(s32 mode, s32 rate, s32 x, s32 y); /* texture page id */

/* A three-part gauge bar. */
typedef struct {
    PolyF4 parts[3];
} GaugeBar;

void func_80085E90(s32 mirrored, s16 *out, s32 x);
void func_80085EAC(s32 mirrored, s16 *out, s32 y);

/* Backdrop texture pages and sprites. */

extern DrawTPage D_800955C8[4];
extern Sprite D_800955F8[6];
void func_800875EC(void);
void func_80087830(void);

/* HUD packets (D_80095698, 0x280 bytes). */
typedef struct {
    Sprite s[2];
} SpritePair;

typedef struct {
    PolyFT4 name_l[2];    /* 0x000 */
    PolyFT4 name_r[2];    /* 0x050 */
    SpritePair icon[4];   /* 0x0A0 */
    SpritePair gauge[4];  /* 0x140 */
    PolyFT4 bar_l[2];     /* 0x1E0 */
    PolyFT4 bar_r[2];     /* 0x230 */
} Hud;

extern Hud D_80095698;
extern DrawTPage D_80095918[4]; /* HUD texture page modes, two per buffer */
extern u8 D_80092860; /* left bar texel row */
extern u8 D_80092864; /* right bar texel row */
extern u16 D_80091814[16]; /* gauge palette */
void func_800864B4(TimImage *tim, s32 x, s32 y, PolyFT4 *quad, s32 depth);
void func_800866D4(TimImage *tim, s32 x, s32 y, PolyFT4 *quad, s32 depth);

/* Fading overlay. */
extern s16 D_80092780; /* fade level */
extern s32 D_80092784;
extern s32 D_80092948;
extern s32 D_8009292C;

extern u8 D_800928A0; /* buffer being built */
extern s32 D_80092880;
extern Caption D_80095540;
extern Menu D_800915AC[8];
extern Menu *D_80092734; /* menu being shown */
extern s32 D_80092700;
extern s32 D_80092744; /* caption of the selected line */
extern s32 D_80092748; /* pad buttons held */
extern s32 D_80091364; /* pad port of the menu input */
extern u8 D_80092764;  /* stick is deflected */
/* Resident pad state, per port. */
extern u16 D_80059570, D_80059574;
extern u16 D_800594A4, D_800594A8;
extern u16 D_8005948C, D_80059490;
extern u8 D_80059438, D_8005943C; /* stick x */
extern u8 D_80059430, D_80059434; /* stick y */
extern s32 D_80092704;

extern u8 D_800928FC;  /* pad port driving the menus */
extern TileRgb D_8009A1C0; /* screen fade tile, buffer 0 */
extern TileRgb D_8009A2B8; /* screen fade tile, buffer 1 */

/* Map view. */
extern SVector D_80092768; /* stored map position */

void func_80080AE8(void);
void func_80036420(void);
s32 func_80081A44(void);
void func_80087698(s32 x0, s32 y0, s32 x1, s32 y1); /* draw a line */
void ClearImage(Rect *rect, s32 r, s32 g, s32 b); /* clear a VRAM area */
void DrawSync(s32 mode); /* wait for drawing */
void func_80080D20(void *packets);
void func_80086E24(void);
void func_8008E120(void);
void func_8007F258(void *packets, s32 arg);
void func_8008BC04(void);
void func_8003A838(s32 arg0, s32 arg1, s32 arg2);
void func_8008E064(void);
s32 func_80033728(s32 table, s32 index); /* text string of an index */
s32 func_80034EAC(s32 string, s32 image, s32 colour, s32 arg); /* returns width */
void LoadImage(Rect *rect, void *pixels); /* load pixels into VRAM */
void func_8007EE08(s32 arg);
void func_80080F04(void);
void func_8007E894(s32 x, s32 y);
s32 func_80035734(s32 port); /* pad type */
void func_8008EB4C(s32 sound);
void func_80080964(s32 menu);

extern Menu *D_80092738; /* menu to return to */

#endif
