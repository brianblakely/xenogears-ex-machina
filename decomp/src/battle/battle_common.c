/* The battle overlay's common (uninitialized global) variables. The original
 * linker allocated them after every unit's own variables, from 800c3cec, in
 * an order of its own, up to the end of the BSS that the resident's mode
 * table clears for the overlay (800d39f4), so this unit, linked last, defines
 * them, each in a slot of whole words (decomp/Makefile). GCC emits tentative
 * definitions in the order of their first declaration, so they are defined
 * ahead of the headers that declare them, structures by their tags; the
 * headers then complete the types and check the declarations. The
 * names the battle code uses for parts of these objects are in
 * battle.data.ld. Variables no battle code addresses are marked
 * unreferenced, or with the battle-time modules that use them. */
#include "common.h"
#include "psyq/libgte.h"

u8 D_800C3CEC;    /* a command file is loaded */
s16 D_800C3CF0;
u8 D_800C3CF4[9]; /* decimal digits */
u8 D_800C3D00;    /* item list row of the chosen item */
struct SpritePool D_800C3D04;
struct EffectPool D_800C3D0C;
u16 D_800C3D14; /* highlighted slots */
struct EnemyReaction D_800C3D18[8];
s32 D_800C3D38; /* the running total */
u8 *D_800C3D3C; /* the attacker's attack level and maximum (+0x148) */
u16 D_800C3D40;
u8 D_800C3D44;
u8 D_800C3D48;  /* the 801e5000 module is loaded */
s16 D_800C3D4C; /* the trail's blend */
struct Panorama *D_800C3D50[2]; /* the stage backdrops */
s32 D_800C3D58; /* gear enemies present */
u8 D_800C3D5C;
u8 *D_800C3D60; /* the target's field 0x148 */
u16 D_800C3D64;
u8 D_800C3D68;
u8 D_800C3D6C;
u8 D_800C3D70[0x30];
struct TextureScroll D_800C3DA0[2]; /* the stage's texture scrolls */
u8 *D_800C3DD0;    /* the enemy data file (ovl2615) */
s32 D_800C3DD4[2]; /* unreferenced */
void *D_800C3DDC;  /* enemy name table */
u8 D_800C3DE0[8];  /* the entered combo steps' buttons */
void *D_800C3DE8;  /* file 3 block */
s32 D_800C3DEC;
s16 D_800C3DF0; /* the acting sprite's command motion */
s32 D_800C3DF4; /* unreferenced */
u8 D_800C3DF8;  /* effects run */
struct CommandDescriptor *D_800C3DFC; /* current command descriptor */
struct Combatant *D_800C3E00;         /* attacker record */
u8 D_800C3E04;                        /* attacker slot */
u8 D_800C3E08[3];                     /* panel value digits */
struct KnownSkills D_800C3E0C[3];
u8 D_800C3E18;
struct Sprite *D_800C3E1C;
s32 D_800C3E20;
struct DirectionArrows *D_800C3E24;
u8 D_800C3E28[2]; /* direction input: [0] the previous, [1] the current */
u8 D_800C3E2C;
u16 D_800C3E30;               /* slot mask */
struct Combatant *D_800C3E34; /* target record */
struct ModelPart *D_800C3E38;
s32 D_800C3E3C;   /* unreferenced */
u8 D_800C3E40[8]; /* enemy name per enemy slot (3-10) */
struct ModelTable *D_800C3E48; /* the stage's models (hierarchy D_800C3E38) */
u8 D_800C3E4C;                /* battle end state */
u8 D_800C3E50;                /* target slot */
s32 D_800C3E54;
s32 D_800C3E58; /* unreferenced */
struct TextImage D_800C3E5C[10]; /* text images of battle messages 0-9: the decimal digits */
s32 D_800C3E84; /* unreferenced */
s32 D_800C3E88;
u8 D_800C3E8C;     /* pending battle message + 1 */
u8 D_800C3E90[12]; /* default-target candidates */
s16 D_800C3E9C;    /* the trail's colour count */
void *D_800C3EA0;
struct BattleGraphics *D_800C3EA4;
s16 D_800C3EA8; /* stage image height */
struct TurnState *D_800C3EAC;
struct BattleArea D_800C3EB0;
u8 D_800D2CB0[0x30]; /* item counts */
u8 D_800D2CE0[0x30]; /* item ids */
u8 D_800D2D10[4];    /* speeds 8009892c replaces by each gear part speed */
s32 D_800D2D14[4];   /* unreferenced */
u8 D_800D2D24[3];    /* party character ids, 0x7F none */
struct BattleUi *D_800D2D28;
s16 D_800D2D2C; /* stage image width */
s16 D_800D2D30; /* stage image x */
s16 D_800D2D34; /* stage image y */
s16 D_800D2D38; /* the panel member HP's digits' remainder */
s32 D_800D2D3C; /* 801de000 module blocks */
s32 D_800D2D40;
s32 D_800D2D44; /* unreferenced */
s32 D_800D2D48;
s16 D_800D2D4C; /* effect hits */
u8 D_800D2D50;
u8 D_800D2D54[7];  /* panel maximum digits */
u8 D_800D2D5C[11]; /* running result code per slot */
struct TotalPopup *D_800D2D68; /* the running total's task, if shown */
struct GearRecord *D_800D2D6C; /* attacker's gear record */
s16 D_800D2D70[11];            /* running result amount per slot */
u8 D_800D2D88[5];              /* name glyph codes */
struct WindowRect *D_800D2D90[7];
struct Window *D_800D2DAC; /* the message text window */
u32 *D_800D2DB0; /* blank text image */
struct ListPrims *D_800D2DB4;
u8 D_800D2DB8;  /* resolve status returned to the caller */
s32 D_800D2DBC; /* unreferenced */
u8 D_800D2DC0;  /* forced next turn: slot + 1 */
u8 D_800D2DC4;  /* an ether check failed */
struct GearRecord *D_800D2DC8; /* target's gear record */
struct TurnQueue D_800D2DCC;
s32 D_800D2E34; /* unreferenced */
struct WindowBlock *D_800D2E38[7];
s16 D_800D2E54;
s16 D_800D2E58; /* panel member HP */
struct BattleAction D_800D2E5C[32];
void *D_800D2F5C; /* glyph table */
s32 D_800D2F60;
u8 D_800D2F64; /* triangle visit stamp */
struct IconCell D_800D2F68[22];
MATRIX *D_800D2FC0; /* the stage colour matrix */
u8 D_800D2FC4;      /* battle exit requested */
s16 D_800D2FC8;     /* point count of D_800D2FD0 */
s32 D_800D2FCC;     /* segments drawn of the current curve */
u16 *D_800D2FD0;    /* (x, z, y) points */
s32 D_800D2FD4;     /* unreferenced */
u8 *D_800D2FD8;     /* the trail being drawn: its colours */
u8 D_800D2FDC;
s16 D_800D2FE0;     /* the panel member maximum HP's digits' remainder */
u8 D_800D2FE4[48];  /* battle item ids (ovl2596, ovl2615) */
u8 D_800D3014;
s32 D_800D3018; /* the panel gear HP's digits' remainder */
struct GroupEntry D_800D301C[32];
struct BattleCamera D_800D309C;
s32 D_800D30E8; /* unreferenced */
struct TotalPopup D_800D30EC;
s32 D_800D316C[66]; /* unreferenced */
u8 D_800D3274;      /* candidate count */
struct ScriptState *D_800D3278;
s32 D_800D327C; /* unreferenced */
u8 D_800D3280;  /* party panel layout */
s32 D_800D3284;
s32 D_800D3288;
s32 D_800D328C;
s32 D_800D3290; /* unreferenced */
u8 D_800D3294;
u8 D_800D3298;    /* ATB enabled */
void *D_800D329C; /* item name table */
struct SlotFlags D_800D32A0[11];
struct ResultPanel *D_800D32F8[3];
struct Tracker D_800D3304[2];
s32 D_800D332C; /* unreferenced */
s16 D_800D3330; /* panel member maximum HP */
s16 D_800D3334;
u8 D_800D3338;       /* (the resident's battle mode, ovl2596, ovl2615, ovl3087) */
s32 D_800D333C;      /* panel gear HP */
void *D_800D3340;    /* (ovl3087) */
SVECTOR *D_800D3344; /* scene points */
s32 D_800D3348;      /* scene triangle count */
void *D_800D334C;    /* the post-battle module's result summary (ovl2596) */
u8 D_800D3350;       /* the command file is started */
SVECTOR D_800D3354;  /* camera position */
SVECTOR D_800D335C;  /* camera look-at point */
struct Formation *D_800D3364;
struct BattleObject *D_800D3368[32]; /* stage objects */
/* The enemy files' disc read list (ovl2615): a file number and a destination
 * per entry, ended by file 0. */
u16 D_800D33E8;
void *D_800D33EC;
u16 D_800D33F0;
void *D_800D33F4;
u16 D_800D33F8;
void *D_800D33FC;
struct EnemyAi D_800D3400[8];
struct ImageAnim D_800D3600; /* the stage's image animation */
s32 D_800D3630; /* the popup colour kind */
u16 D_800D3634; /* the current event's targets */
u8 D_800D3638;
struct Sprite *D_800D363C[11]; /* the current event's target sprites, NULL ended */
s32 D_800D3668; /* panel gear maximum HP */
u8 D_800D366C;  /* menu effects enabled */
u8 D_800D3670;  /* item list column of the chosen item */
s32 D_800D3674; /* unreferenced */
s16 D_800D3678; /* the current event's target count */
void *D_800D367C; /* menu module block */
s32 D_800D3680; /* the total shown, -1 none */
s32 D_800D3684; /* unreferenced */
u8 D_800D3688[0x30]; /* gear part counts */
u8 D_800D36B8;       /* the battle's start mode */
s16 D_800D36BC;
u8 D_800D36C0;  /* the party member whose menu is open */
s32 D_800D36C4; /* unreferenced */
struct BattleMessage D_800D36C8[8];
void *D_800D39C8; /* the enemy set data copy */
struct SceneTriangle *D_800D39CC; /* scene triangles */
struct EventScriptFile *D_800D39D0; /* (ovl3087) */
u8 D_800D39D4;
void *D_800D39D8; /* (ovl2606) */
u16 D_800D39DC;   /* alive mask */
u16 D_800D39E0;   /* mask of slots that act together */
u16 D_800D39E4;   /* the single action to request (71, 73) */
s16 D_800D39E8;   /* a slow wave (4..9) */
struct Sprite *D_800D39EC; /* the sprite the camera circles */
void *D_800D39F0; /* battle message table */

#include "battle/action_file.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/combatant.h"
#include "battle/command.h"
#include "battle/effect.h"
#include "battle/enemy_ai.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/input.h"
#include "battle/item_command.h"
#include "battle/lists.h"
#include "battle/menu_pages.h"
#include "battle/objects.h"
#include "battle/resolver.h"
#include "battle/scene.h"
#include "battle/screen.h"
#include "battle/setup.h"
#include "battle/stage.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "action_resolve.h"
#include "curve.h"
#include "overlays.h"
#include "popup.h"
#include "sprite_effect.h"
