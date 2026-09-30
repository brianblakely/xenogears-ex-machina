#ifndef BATTLE_CORE_H
#define BATTLE_CORE_H

#include "common.h"
#include "psyq.h"
#include "combatant.h"
#include "scene.h"


/* Per-slot block of the turn state (0x40 bytes). */
typedef struct {
    u8 unk0[0x1C];
    u16 items[16];     /* menu item availability, 0 = available */
    u8 defaultTarget;  /* +0x3C */
    u8 unk3D[3];
} TurnSlot;

/* Turn and menu state: the heap block at *800c3eac. */
typedef struct {
    TurnSlot slots[11];
    u8 unk2C0[0x2CC - 0x2C0];
    u8 unk2CC[7];
    u8 actor;          /* +0x2D3 acting slot */
    u8 unk2D4[2];
    u8 unk2D6;
    u8 unk2D7[0x2DA - 0x2D7];
    u8 eventCount;     /* +0x2DA queued presentation events */
    u8 eventsDone;     /* +0x2DB */
    u8 unk2DC;         /* action index + 1 */
    u8 page;           /* +0x2DD command page */
    u8 menuDone;       /* +0x2DE */
    u8 unk2DF[0x2E8 - 0x2DF];
    u8 unk2E8;         /* attack page target */
    u8 unk2E9;
    u8 unk2EA;
    u8 reaction[3];    /* +0x2EB per party member */
    u8 unk2EE[0x2F6 - 0x2EE];
    u8 repeatArmed;    /* +0x2F6 */
} TurnState;

extern TurnState *D_800C3EAC;

/* Battle UI state: the heap block at *800d2d28. */
typedef struct {
    u8 unk0[0x30];
    u32 unk30;         /* CLUT cycle position */
    s32 unk34;         /* +0x34 panel cursor window: x */
    s32 unk38;
    s32 unk3C;         /* y */
    s32 unk40;
    s32 unk44;         /* width */
    s32 unk48;
    s32 unk4C;         /* height */
    s32 unk50;
    u32 unk54;         /* x step (8.8) */
    s32 unk58;
    u32 unk5C;         /* step count */
    s32 unk60;
    s32 unk64;
    s32 unk68;
    s32 unk6C;
    u8 unk70[0x7B - 0x70];
    u8 unk7B;          /* AP text part count */
    u8 reaction[3];    /* +0x7C */
    u8 unk7F[0x8E - 0x7F];
    u8 unk8E;
    u8 unk8F;
    u8 unk90[3];       /* per party member */
    u8 unk93[3];
    u8 unk96;          /* file 3 block loaded */
    u8 unk97;          /* selected list row */
    u8 unk98;
    u8 unk99[3];       /* party panel digit buffers */
    u8 unk9C;
    u8 unk9D;
    u8 unk9E;
    u8 unk9F[0xA3 - 0x9F];
    u8 unkA3;
    u8 unkA4;
    u8 unkA5;
    u8 unkA6;
    u8 unkA7;
    u8 unkA8;
    u8 unkA9;
    u8 unkAA;          /* frame counter */
    u8 unkAB;
    u8 unkAC;
    u8 unkAD;
    u8 unkAE;          /* menu module block loaded */
    u8 unkAF;
    u8 windows[7];     /* +0xB0 window shown */
    u8 unkB7;          /* command window page */
    u8 unkB8[7];       /* window opening */
    u8 unkBF[7];       /* window fully open */
    u8 unkC6;
    u8 unkC7;
    u8 unkC8;
    u8 unkC9;
    u8 unkCA;
    u8 unkCB;
    u8 unkCC[4];
    s32 unkD0[2];
    u8 unkD8[0xE0 - 0xD8];
    s32 unkE0[3];
    s32 unkEC[3];
    s32 unkF8;
    s32 unkFC;
    s32 unk100;
    u16 unk104;
    u16 unk106;
    u8 unk108[4];
} BattleUi;

extern BattleUi *D_800D2D28;

/* The 0x670-byte graphics block (*800c3ea4 + 0xa230). */
typedef struct {
    POLY_FT4 unk0[2];
    POLY_FT4 unk50[2];
    POLY_FT4 unkA0[2];
    POLY_FT4 unkF0[2];
    POLY_FT4 unk140[2]; /* page title quads */
    POLY_FT4 unk190[6]; /* second title quad, or up to three cost digits */
    POLY_FT4 unk280[2];
    POLY_FT4 unk2D0[2];
    POLY_FT4 unk320[2];
    POLY_FT4 unk370[2];
    POLY_FT4 unk3C0[2]; /* EP page icons */
    POLY_FT4 unk410[2];
    POLY_FT4 unk460[2]; /* EP digits */
    POLY_FT4 unk4B0[2];
    POLY_FT4 unk500[2]; /* maximum EP digits */
    POLY_FT4 unk550[2];
    POLY_FT4 unk5A0[5]; /* EP label glyphs */
    u8 unk668;         /* buffer of the +0x0..+0xf0 quads */
    u8 unk669;
    u8 buffer;         /* +0x66A */
    u8 unk66B;
    u8 unk66C;
    u8 unk66D;
    u8 unk66E;         /* cost digits shown */
    u8 unk66F;
} GraphicsBlock;

/* Party status panel state of the graphics block (0x1e4 bytes). */
typedef struct {
    u8 unk0[0x1E1];
    u8 unk1E1;         /* 1: show the 800d32a0 value */
    u8 unk1E2[2];
} PartyPanel;

/* A texture location of the graphics state (0x18 bytes). */
typedef struct {
    s32 mode;
    s32 clutX;
    s32 clutY;
    s32 x;
    s32 y;
    s32 unk14;
} GraphicsTexture;

/* Battle graphics state (*800c3ea4). */
typedef struct {
    u8 unk0[0x908];
    LINE_F2 unk908[12];
    POLY_FT4 unk9C8[6][2];
    POLY_FT4 unkBA8[120];
    POLY_FT4 unk1E68[60];
    POLY_FT4 unk27C8[1];
    u8 unk27F0[0x3A88 - 0x27F0];
    POLY_FT4 unk3A88[3][80]; /* party panel name glyphs */
    POLY_FT4 unk6008[3][8];  /* party panel value digits */
    POLY_F4 unk63C8[2];
    DR_MODE unk63F8[2];
    s32 unk6410;       /* shade */
    u8 unk6414;
    u8 unk6415;
    u8 unk6416;        /* fading down */
    u8 unk6417[5];
    POLY_FT4 unk641C[2][100];
    PartyPanel panels[3];    /* +0x835C */
    u8 unk8908[0x8950 - 0x8908];
    RECT unk8950[4];
    u32 unk8970[4][0x630 / 4]; /* four CLUT strips, cycled */
    GraphicsBlock *unkA230;
    u8 unkA234[4];
    GraphicsTexture textures[5]; /* +0xA238 */
} BattleGraphics;

extern BattleGraphics *D_800C3EA4;
extern u8 D_800C492A;

/* Battle drawing state (800ccb04). */
typedef struct {
    u32 *ot;           /* current ordering table */
    u8 unk4[0x2C];
    s32 buffer;        /* +0x30 draw buffer index */
} BattleDraw;

extern BattleDraw D_800CCB04;
extern void *D_800D2F5C; /* glyph table */
extern u8 D_800C3E4C;   /* battle end state */

/* The battle message image (800d39b8): upload rectangle and pixels. */
typedef struct {
    RECT rect;
    u32 *pixels;
    u8 unkC[2];
    s8 width;          /* +0xE */
} MessageImage;

extern MessageImage D_800D39B8;
extern void *D_800D39F0;   /* battle message table */
extern u16 D_800C48E8;

/* Eight 0x60-byte message entries from 800d36c8. */
typedef struct {
    POLY_FT4 prims[2];
    RECT rect;         /* +0x50 text image upload rectangle */
    u32 *pixels;       /* +0x58 */
    u8 alternate;      /* +0x5C odd texture row */
    u8 shown;          /* +0x5D */
    u8 width;          /* +0x5E */
    u8 unk5F;
} BattleMessage;

extern BattleMessage D_800D36C8[8];
extern u8 D_800D3014;
/* Formation data (*800d3364). */
typedef struct {
    u8 distance;
    u8 unk1[7];
} GroupLink;

/* A formation group's position (8 bytes). */
typedef struct {
    s16 x;
    s16 z;
    u8 pad4[4];
} GroupPosition;

typedef struct {
    u8 unk0[0x100];
    GroupPosition positions[8]; /* +0x100 */
    GroupLink links[8][8];      /* +0x140 per formation-group pair */
} Formation;

extern Formation *D_800D3364;


/* Action list entry (8 bytes, 32 from 800d2e5c). */
typedef struct {
    u8 type;
    u8 arg1;
    u8 animation;
    u8 named;
    u8 param;
    u8 unk5;
    u16 targets;
} BattleAction;

extern BattleAction D_800D2E5C[32];
extern u16 D_800D39E0;     /* mask of slots that act together */
extern u8 D_800D2CE0[0x30]; /* item ids */
extern u8 D_800D2CB0[0x30]; /* item counts */
extern u8 D_800D2C8B[8];
extern s32 D_800D2C60[8];

typedef struct {
    u8 unk0[0x34];
    u8 active;         /* +0x34 */
    u8 unk35[3];
} BattleUnk3278Entry;

typedef struct {
    BattleUnk3278Entry entries[16];
    u8 unk380[0x394 - 0x380];
    s16 unk394[(0x7A4 - 0x394) / 2];
    POLY_FT4 unk7A4[2];
    u8 unk7F4;
    u8 unk7F5[0x800 - 0x7F5];
    u8 unk800;
} BattleUnk3278;

extern BattleUnk3278 *D_800D3278;

typedef struct {
    u8 unk0[0x3AC0];
    POLY_FT4 unk3AC0[24];   /* list 0 */
    POLY_FT4 unk3E80[34];   /* list 2 */
    POLY_FT4 unk43D0[58];   /* list 10 */
    POLY_FT4 unk4CE0[10];
    POLY_FT4 unk4E70[8];    /* list 3 */
    POLY_FT4 unk4FB0[6];    /* list 4 */
    POLY_FT4 unk50A0[8];    /* list 5 */
    POLY_FT4 unk51E0[22];   /* list 6 */
    POLY_FT4 unk5550[6];    /* list 11 */
    POLY_FT4 unk5640[40];   /* list 12 */
    POLY_FT4 unk5C80[6];    /* list 13 */
    u8 unk5D70[5];
    u8 counts[15];          /* +0x5D75 primitive count per list */
    u8 buffers[15];         /* +0x5D84 draw buffer per list */
    u8 unk5D93[0x5D9C - 0x5D93];
    s16 unk5D9C;
    s16 unk5D9E;
    u8 unk5DA0;
    u8 unk5DA1;
    u16 unk5DA2;
} BattleUnk2DB4;

extern BattleUnk2DB4 *D_800D2DB4;
extern s32 D_800D2DAC;

/* Enemy AI block (0x40 bytes per enemy slot 3..10, from 800d3400). */
typedef struct {
    u8 *script;        /* +0x00 */
    u8 *unk4;
    u8 *reaction;      /* +0x08 reaction script */
    u8 unkC[4];
    s32 longs[4];      /* +0x10 */
    u16 vars[8];       /* +0x20 */
    u8 bytes[16];      /* +0x30 */
} EnemyAi;

extern EnemyAi D_800D3400[8];

/* Enemy reaction state (4 bytes per enemy from 800c3d18). */
typedef struct {
    u8 armed;          /* the reaction script runs */
    u8 unk1[2];
    u8 unk3;
} EnemyReaction;

extern EnemyReaction D_800C3D18[8];
extern u8 D_800C3E8C;      /* pending battle message + 1 */
extern u8 D_800D366C;      /* menu effects enabled */
/* The committed action (800d2c94). */
typedef struct {
    u16 targets;      /* +0x00 target mask */
    u16 alive;        /* +0x02 alive mask at commit */
    s16 animation;    /* +0x04 */
    u8 pad6[0xA - 0x6];
    u16 held;         /* +0x0A party members whose timers are held */
    u8 padC[0x10 - 0xC];
    u8 enemyBytes[5]; /* +0x10 copied to an enemy's AI bytes 9-13 */
    u8 actor;         /* +0x15 committing actor */
    u8 action;        /* +0x16 committed action index */
    u8 pad17[0x1B - 0x17];
    u8 message;       /* +0x1B pending battle message id */
} ActionCommit;

extern ActionCommit D_800D2C94;
extern u8 D_800C204C;
extern u8 D_800C3E18;
extern u8 D_800C3D70[0x30];
extern void *D_800D367C;   /* menu module block */
extern void *D_800C3DE8;   /* file 3 block */
extern u8 D_800C3CF4[9];   /* decimal digits */

typedef struct {
    u8 unk0[0x14];
    u16 bank;
} SoundSystem;

extern SoundSystem *D_8005919C;
/* Window rectangle (*800d2d90[window]). */
typedef struct {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u16 curW;          /* +0x8 opening size */
    u16 curH;
    u8 style;          /* +0xC */
} WindowRect;

/* A window's primitives (0x5a8-byte heap block). */
typedef struct {
    u8 unk0[0x140];
    POLY_FT4 frame[4][4]; /* +0x140 edge pieces, textures 1-4 */
    POLY_G4 shade[2];     /* +0x3C0 background, one per draw buffer */
    DR_MODE mode[2];      /* +0x408 */
    u8 unk420[0x5A8 - 0x420];
} WindowBlock;

extern WindowBlock *D_800D2E38[7];
extern u8 D_800594D4[3]; /* window colour */
extern WindowRect *D_800D2D90[7];


/* Item effect table (0x10 bytes from 800d2200). */
typedef struct {
    u8 unk0[4];
    u16 target;    /* +0x4 target selection */
    u8 unk6[0xE - 0x6];
    u16 animation; /* +0xE */
} ItemEffect;

extern ItemEffect D_800D2200[];
extern s32 D_800D2C54[12]; /* per-slot damage */
extern u8 D_800D2C88[12];  /* per-slot result code */
extern void *D_800D329C;   /* item name table */

/* Stepped line state (80088 87c). */
extern s32 D_800C3A7C;
extern s32 D_800C3A80;
extern s32 D_800C3A84;
extern s32 D_800C3A88;
extern s32 D_800C3A8C;
extern s32 D_800C3A90;
extern s8 D_800C3A94;
extern s8 D_800C3A98;
extern s32 D_800C3A9C;
extern s8 D_800C207C;
extern s32 D_800C2080;
extern s32 D_800C2084;


/* Resolver globals. */

extern u8 D_800C3E90[12];  /* default-target candidates */
extern u8 D_800D3274;      /* candidate count */
extern u16 D_800C3D64;
extern u8 D_800C3E2C;
extern u16 D_800D2C30;
extern u8 D_800D2C38;
extern u8 D_800CCC58;
extern s32 D_800D3288;
extern u8 D_800D39D4;
extern u16 D_800C3608;     /* slots that still count while down */
extern u8 D_800D3280;      /* party panel layout */
extern u16 D_800C3254[]; /* party panel x: [layout * 3 + member] */
extern s16 D_800C3068[]; /* party panel glyph x: [member * 24 + glyph] */
extern s16 D_800C3076[]; /* party panel name glyph x: [member * 24 + glyph] */
extern u8 D_800D2D88[5];   /* name glyph codes */
extern u16 D_800D2C2A;
extern u8 D_800D2C35;
extern u8 D_800D2C36;
extern u16 D_800D2C3A;
extern u8 D_800C3200[]; /* list separator rows: [count * 6 + row] */
extern u8 D_800C34CC[];    /* combo step flags */

/* Direction arrow block (*800c3e24, 0xec bytes). */
typedef struct {
    POLY_G3 prims[8]; /* per direction, one per draw buffer */
    s32 shade;           /* +0xE0 pulsing red level */
    u8 buffer;           /* +0xE4 */
    u8 fading;           /* +0xE5 the shade is going down */
    u8 arrows[4];        /* +0xE6 a target lies that way */
    u8 unkEA[2];
} DirectionArrows;

extern DirectionArrows *D_800C3E24;

/* Persistent character records (resident, 0x20 bytes from 8006ecf8). */
typedef struct {
    u16 combos;
    u8 unk2[0x1E];
} CharacterCombos;

extern CharacterCombos D_8006ECF8[];


/* Per-slot flags (8 bytes from 800d32a0). */
typedef struct {
    u8 unk0;
    u8 unk1;
    u8 unk2[6];
} SlotFlags;

extern SlotFlags D_800D32A0[11];

/* Formation group entries (4 bytes from 800d301c). */
typedef struct {
    u8 count;
    u8 members;        /* member bits */
    u8 unk2[2];
} GroupEntry;

extern GroupEntry D_800D301C[16];
extern u16 D_800D39DC;     /* alive mask */
extern u16 D_800C3448[16]; /* slot bits */
extern u16 D_800C3468[16]; /* flag bits */
extern u8 D_800D2D5C[11];  /* running result code per slot */
extern s16 D_800D2D70[11]; /* running result amount per slot */
extern u8 D_800D2DC0;      /* forced next turn: slot + 1 */

extern u8 D_800C3D48;      /* the 801e5000 module is loaded */
extern u8 D_800C48EA;      /* battle outcome */
extern s32 D_800C3E54;
extern s32 D_800D3284;
extern s32 D_800D328C;
extern u8 D_800D3298;      /* ATB enabled */

/* Turn queue (800d2dcc). */
typedef struct {
    u8 present[11];    /* slot takes part */
    u8 cursor;         /* +0x0B turn order position */
    u8 order[11];      /* +0x0C slots in turn order */
    u8 unk17;
    u8 ready[11];      /* +0x18 slot ready to act (0xff: out) */
    u8 unk23;
    s16 timers[3][11]; /* +0x24 [0] reload values, [1] counters,
                        * [2] slow-status alternation */
} TurnQueue;

extern TurnQueue D_800D2DCC;
extern s32 *D_8005917C;
extern u8 D_8005959C;
extern s32 D_800595A0;
extern u16 D_80059414;
extern u16 D_800595D4;
extern s16 D_8005A3A0[];
extern s32 D_8006EF58;     /* party gold */

/* Command menu. */
void func_8008BC40(u8 keep);
void func_8008BC98(u8 member);
u8 func_80085084(u16 target, u8 member, s32 mode);
s32 func_8009A7E4(u8 index);
void func_8009A854(u8 index, u8 k);
extern u8 D_800D3688[0x30]; /* gear part counts */
void func_8007FD38(u8 member);
void func_8008AA74(u8 id);   /* play a menu sound */
void func_8008ADD0(u8 member);
u8 func_8008B478(u8 member); /* the member has a gear list */
u8 func_8008BED8(u8 member);
void func_8008B908(u8 member);
void func_8009AA44(u8 member);
void func_80087A38(u8 member);
void func_80084A7C(u8 member);
void func_80077698(void);
void func_800826CC(u8 member);
s32 func_8009A9D0(void); /* the escape succeeds */
extern u16 D_800D2C32; /* fuel gained by charging */

/* Gear boarding (800826cc). */
extern u8 D_80059179;
extern u8 D_8006F368[3]; /* the party's character ids */
extern u8 D_8006F8E5[3]; /* per party member: entered a gear */
void func_80088490(s32 slot);
void func_8009AEFC(u8 slot);
void func_800BAF48(u8 slot);

/* Turn start (80071b94). */
extern u8 D_800C4922;      /* acting slot */
extern void *D_800C3DDC;   /* enemy name table */
extern u8 D_800C3E3D[];    /* enemy name per slot */
extern u16 D_8005941C;     /* count of turns taken with 2ea set */
extern u8 D_800D36C0;      /* the party member whose menu is open */
void func_80079778(u8 actor);
void func_800799C8(u8 slot, u16 attacking);
void func_80079C24(void);
void func_80080160(u8 member);
void func_80080C94(u8 member);
void func_80085B58(u8 slot);
s32 func_80085310(u8 slot, u8 target);
void func_800BA4E0(s32 value);
void func_800BFE48(void);
void func_80071964(void);
void func_80071AE0(void);
void func_80071A38(void);
void func_80071A08(void);
void func_80072270(void);
void func_800718BC(void);
u16 func_80099890(u8 slot);

/* Battle setup (80070f40). */
extern u8 D_80059180;      /* battle music playing */
extern u8 D_8005947C;      /* pending scene + 1 */
extern void *D_80059480;
extern Formation *D_8005949C; /* the formation data */
extern void *D_800594AC;
extern u8 D_800594D0;      /* battle result: 0 won, 1, 2, 3 */
extern u8 D_800594F8;      /* the 801e0000 module runs first */
extern u8 D_80059508;      /* scene index */
extern u8 D_8005954C;      /* battle kind */
extern u8 *D_800595D0;     /* scene texture block */
extern s32 D_80062528;     /* battle music */
extern u8 D_80062648[];
extern u8 D_800658DC[][0x20]; /* scene settings */
extern u8 D_8006F9DC[0x20];   /* the current scene settings */
extern u8 D_800C3D44;
extern u8 D_800C3D5C;
extern s32 D_800C3DEC;
extern u8 D_800C3E28;
extern u8 D_800C3E29;
extern Formation *D_800C3EB0;
extern s32 D_800D2D3C;     /* 801de000 module blocks */
extern s32 D_800D2F60;
extern u8 D_800D2D50;
extern u8 D_800D2FC4;      /* battle exit requested */

/* Party members' battle masks (from the character battle data). */
typedef struct {
    u16 mask0;
    u16 mask2;
} MemberMasks;

extern MemberMasks D_800C3E0C[3];

void func_80070EB0(s32 value);
void func_80070EDC(void);
void func_800723E0(void);
void func_8007252C(void);
void func_8007819C(void);
void func_8008F8F4(u8 window, u16 x, u16 y, u16 w, u16 h, u8 animate, u8 wait);
void func_800780A8(BattleMessage *message, u32 row);
void func_80077990(void);
void func_8009892C(void);
void func_800B39C0(s32 a, s32 b, s32 c, s32 d, s32 e);
void func_800B8098(u8 kind);
void func_800B81BC(s32 a);
void func_800B853C(u8 mode);
void func_800B8D7C(void);
void func_800C0F70(void);
void func_801E0A34(void);
void func_801E252C(void);

/* Resident services. */
void func_8003700C(char *format, ...); /* debug print */
void func_80028A60(s32 a);
s32 func_800397FC(u8 *a, s32 b, s32 c);
void func_8003A094(u8 *texture);
void *memmove(void *to, const void *from, u32 size);
void func_80039DB8(s32 effect);
u8 func_8001BD40(u8 low, u8 high);
void func_80034888(s32 arg0, u32 *ot, s32 buffer);
s32 func_8002675C(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 y, s32 scale);
s32 func_800263E4(void *table, s32 id, POLY_FT4 *prims, s32 buffer, s32 x, s32 w, s32 scale, s32 a, s32 b);
void *func_80033728(void *table, s32 index);
s32 func_80034EAC(void *text, u32 *pixels, s32 width, s32 mode);
void func_800295D8(s32, s32, s32, s32);
void func_8003A89C(s32, s32, s32);

/* Battle overlay. */
void func_8008AB4C(void);
s32 func_8008ABB8(s32 size, s32 mode);
void func_8008AC50(void);
void func_8008FAD8(void);
void func_80073FB8(void);
void func_80073538(void);
void func_80073F08(void);
void func_8007500C(void);
void func_80074F70(void);
void func_80088B80(void);
void func_80074AB8(void);
void func_80076418(void);
void func_800764B4(void);
void func_800764EC(void);
void func_80076AC8(POLY_FT4 *prim);
void func_800742A0(void);
void func_80075938(void);
void func_80073A58(void);
void func_80073B64(void);
void func_80073E88(void);
void func_80074EEC(void);
void func_800745EC(void);
void func_80074D4C(void);
void func_80077074(void);
void func_800785D4(u8 actor, u8 index);
void func_80078658(u8 index, u8 actor);
void func_800787E0(u8 value, u8 actor);
void func_8007887C(u8 actor);
void func_8007893C(u8 index, u8 actor);
void func_80085C88(u8 queue);
void func_80085CCC(u8 actor, u16 targets, s16 animation);
void func_80085350(void);
void func_80085388(void);
void func_800B89FC(s32 mode, u8 actor, s32 arg2, s32 arg3);
s32 func_80080AE4(u8 actor);
void func_800793F0(u8 actor);
void func_80079674(u8 actor);
void func_80079948(u8 **pc);
u8 func_8007F8C0(u8 **pc, u8 enemy);
u8 func_8007EF6C(u8 **pc, u8 enemy, u8 count);
void func_80079934(u8 **pc);
void func_800728B8(POLY_FT4 *prims, s32 count, s32 first);
void func_80073380(s32 member);
u8 func_80072F38(s32 member, u8 inGear);
extern s16 D_800D3330;     /* panel member maximum HP */
extern s16 D_800D2FE0;     /* its digits' remainder */
extern s16 D_800D2E58;     /* panel member HP */
extern s16 D_800D2D38;     /* its digits' remainder */
extern s32 D_800D333C;     /* panel gear HP */
extern s32 D_800D3018;     /* its digits' remainder */
extern s32 D_800D3668;     /* panel gear maximum HP */
void func_80072DA8(s32 member, s32 mode);
void func_80072A9C(s32 member, s32 mode);
extern u8 D_800C3E08[3]; /* panel value digits */
extern u8 D_800D2D54[7]; /* panel maximum digits */
void func_80072938(POLY_FT4 *prims, s32 first, s32 last, u8 mode);
void func_80076C78(POLY_FT4 *prim, u16 x, u16 y, u8 u, u8 v, u8 w);
void func_80076CE8(POLY_FT4 *prim, s16 x, s16 y, u8 u, u8 v, s32 w, s32 h);
void func_80076D58(POLY_FT4 *prims, u8 alternate, u8 page);
void *func_80033784(u8 character, u8 id); /* a character text */

/* A menu icon cell of the icon image (4 bytes, 800d2f68). */
typedef struct {
    u8 w;
    u8 alternate; /* uses the alternate CLUT */
    u8 u;
    u8 v;
} IconCell;

extern IconCell D_800D2F68[];
s32 func_80098AF8(s32 slot, s32 mode);
void func_80079E18(u8 index);
void func_80079E4C(u8 index);
s32 func_800716D8(void);
u8 func_80079E7C(u16 mask);
u8 func_8007A628(u8 slot, u8 any);
u8 func_8007A6C8(u8 slot, u8 arg1);
u8 func_8007A744(u8 slot);
u16 func_80089C08(u8 slot);
u16 func_80089BEC(u8 bit);
u16 func_80089C48(u8 slot);
void func_80098C6C(u16 param);
void func_80085454(u8 queue);
void func_80085618(u8 queue);
void func_800941A4(void);
void func_8008860C(void);
void func_80089038(void);
void func_80089110(void);
void func_800891E4(void);
void func_80089348(void);
void func_8008946C(void);
void func_8008963C(void);
void func_800897CC(void);
void func_8007FCE8(void);
void func_8007FDEC(void);
void func_8009413C(u8 member, u8 release);
void func_800800E8(u8 member);
void func_8007FB70(u8 member);
s32 func_8009ADA0(u8 slot, s32 *amounts);
void func_800BE538(u8 slot, s32 a, s32 b, s32 c);
s32 func_80076A10(s32 id, POLY_FT4 *prims, s16 x, s16 y);
s32 func_800877E0(u8 actor, u8 target);
void func_80085D34(void);
void func_800879A8(u8 actor, u8 target);
void func_8008FA60(u8 window);
void func_8007765C(void);
void func_8008AB94(void);
void func_8008A684(u8 member);
void func_8008A274(u8 member);
void func_8008A3EC(u8 member);
void func_800BC404(u16 mask);
void func_800BCD98(u16 mask);
void func_80077980(void);
u8 func_80083FF4(u8 member, u8 slot);
void func_80098D2C(u8 slot, u8 param);
u8 func_80084854(u8 origin, u8 direction);
void func_80093B08(u8 member);
s32 func_8008AC00(s32 count);
void func_800769E8(RECT *rect, u32 *pixels);
void func_80097D08(void);
void func_80094D24(void);
u8 func_800841E0(u8 member);
u16 func_80089C6C(u16 mask, u8 bit);
void func_80093578(u8 member, u8 kind);
void func_8009382C(u8 member, u8 kind);
void func_800916D4(u8 column, u8 row, u8 member);
void func_8009187C(u8 member, u8 column, u8 row);
void func_80091B38(u8 member, u8 column, u8 row);
extern u16 D_800C3234[16]; /* single-bit masks: 0x80 down to 1, then 0x8000 down to 0x100 */
void func_8008AAA0(u32 value);
u8 func_8009A258(u8 member, u8 command);
void func_80076BF0(POLY_FT4 *prim);
void func_80089CCC(s32 mode);
void func_8008A144(void);
void func_8007171C(void);
void func_80076B68(POLY_FT4 *prim);
void func_8008FC1C(s16 x, s16 y, s16 w, s16 top, u8 rows);
void func_8008F6E4(u8 style, u16 x, u16 y, u16 w, u16 h);
void func_80077454(u8 window);
void func_80090310(u8 column, u8 row);
void func_8009070C(u8 column, u8 row);
void func_800904A0(u8 column, u8 row);
s8 func_80097964(u8 a, u8 b, u16 c);
void func_80078508(u8 *order);
void func_80087EDC(u8 actor, u8 target);
void func_800881B8(u8 actor, u8 target);
void func_800883AC(u8 slot);
u16 func_80089B50(u16 low, u16 high);
u8 func_80079ED8(u8 slot, u8 attribute, u8 value, u8 write);
u16 func_8007A280(u8 slot, u8 attribute, u16 value, u8 write);
u16 func_80089C9C(u16 mask, u8 slot);
void func_80085AC4(u8 slot);
void func_80071B94(u8 mode);

/* The 801e5000 module and the 80280000 module. */
void func_801E5160(void);
void func_801DE594(void);
s32 func_801E563C(void);
void func_801E879C(s32);
void func_8028022C(void);

#endif
