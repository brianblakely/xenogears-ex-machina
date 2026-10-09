/* The world map's uninitialized variables. The BSS starts at the program's
 * end (8009bbb4) and runs through the word at 8009d80c, the span the
 * resident's mode table clears. No unit has statics, which would come first
 * in unit order: the first variable (seven units) and the last (two) are
 * shared, and those one unit reaches lie among shared ones throughout
 * (tools/data_users.py --range 8009bbb4:8009d810). All of them are commons,
 * which the original linker allocated in an order of its own; this unit,
 * linked last, defines them in that order. Each takes a slot of whole words
 * (decomp/Makefile, uninitialized variables): consecutive halfwords lie a
 * word apart (8009bd10-8009bd1c, 8009bd24/8009bd28, 8009cd4c/8009cd50),
 * where a size-aligned allocation would leave two bytes. Nothing addresses
 * the words marked unreferenced. GCC emits tentative definitions in the
 * order of their first declaration, so they are defined ahead of the world
 * map headers that declare them, the world map's own types by their tags;
 * the headers then complete the types. */
#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/window.h"

u8 D_8009BBB4[0x10]; /* terrain origin, a VECTOR (TERRAIN_ORIGIN, GROUND_SCROLL) */
s32 D_8009BBC4;
struct DisplayBuffer D_8009BBC8[2];
s32 D_8009BCB8;
s32 D_8009BCBC; /* unreferenced */
struct AreaObject *D_8009BCC0;
s32 D_8009BCC4;
s32 D_8009BCC8;
s32 D_8009BCCC[3]; /* sector header */
s32 D_8009BCD8;
s32 D_8009BCDC;
u16 D_8009BCE0[16]; /* faded CLUT ids */
s32 *D_8009BD00;
s16 D_8009BD04;
s32 D_8009BD08;
s32 D_8009BD0C;
u16 D_8009BD10;
u16 D_8009BD14;
u16 D_8009BD18;
u16 D_8009BD1C;
void *D_8009BD20;
s16 D_8009BD24;
s16 D_8009BD28; /* animation count */
s32 D_8009BD2C;
void *D_8009BD30;
s32 D_8009BD34;
SVECTOR D_8009BD38; /* camera angle */
u8 D_8009BD40[0x20]; /* view setup: eye, look-at point and up vector (VIEW) */
u8 D_8009BD60;
Window D_8009BD64; /* destination name window */
struct EffectSlot *D_8009BDF4;
void *D_8009BDF8[3]; /* gear model buffers */
s16 D_8009BE04; /* quads used this frame; a word in 80099BFC */
void *D_8009BE08; /* disc request buffer */
s32 D_8009BE0C;
s32 D_8009BE10; /* movement mode */
void *D_8009BE14;
void *D_8009BE18;
void *D_8009BE1C[2]; /* effect quads, per display buffer */
struct WorldmapActor *D_8009BE24;
struct Camera D_8009BE28;
s32 D_8009BE38;
struct WorldmapView *D_8009BE3C;
s32 D_8009BE40;
s32 D_8009BE44;
s32 D_8009BE48;
MATRIX D_8009BE4C;
struct PlaceRequest D_8009BE6C[32];
s32 D_8009C16C;
s32 D_8009C170; /* loaded party members */
s32 D_8009C174;
s32 D_8009C178;
s32 D_8009C17C;
void *D_8009C180;
void *D_8009C184[0x100]; /* terrain block buffers */
s32 D_8009C584; /* player heading */
s8 D_8009C588[8];
u8 *D_8009C590; /* destination of the next sector's data */
s32 D_8009C594[2]; /* unreferenced */
void *D_8009C59C;
DR_TPAGE D_8009C5A0;
s32 D_8009C5A8; /* arrival kind */
VECTOR D_8009C5AC; /* player position (20.12) */
s32 D_8009C5BC;
POLY_FT4 D_8009C5C0[2]; /* overlay picture, per buffer */
s32 D_8009C610;
void *D_8009C614;
s32 D_8009C618;
s32 D_8009C61C; /* unreferenced */
struct SceneObject *D_8009C620; /* scene objects */
struct HostReadRequest *D_8009C624[16]; /* submitted host-file request lists */
POLY_G3 D_8009C664[8];
POLY_FT4 D_8009C744[2][2]; /* textured horizon quads, per buffer */
void *D_8009C7E4; /* free memory block kept while away */
s32 D_8009C7E8;
struct TerrainTexture *D_8009C7EC;
VECTOR D_8009C7F0;
void *D_8009C800; /* saved VRAM area */
s32 D_8009C804; /* unreferenced */
MATRIX D_8009C808; /* camera matrix */
VECTOR D_8009C828;
SVECTOR D_8009C838; /* block cell */
s32 D_8009C840;
VECTOR D_8009C844;
s16 D_8009C854[16];
VECTOR D_8009C874;
void *D_8009C884;
void *D_8009C888;
void *D_8009C88C;
void *D_8009C890; /* saved VRAM area */
s32 D_8009C894; /* nonzero when resuming a saved state */
TILE D_8009C898[0x40];
s32 D_8009CC98;
s32 D_8009CC9C;
s32 D_8009CCA0;
s32 D_8009CCA4;
s32 D_8009CCA8;
s32 D_8009CCAC; /* unreferenced */
s32 D_8009CCB0;
u16 D_8009CCB4[0x40]; /* terrain palette CLUT ids */
void *D_8009CD34[3]; /* character model buffers */
void (*D_8009CD40)(void); /* per-frame hook */
s32 D_8009CD44;
void *D_8009CD48;
u16 D_8009CD4C; /* pad buttons held */
u16 D_8009CD50;
u16 D_8009CD54[7]; /* terrain texture pages */
s32 D_8009CD64;
struct FerryHeading D_8009CD68[32];
s16 D_8009CE68; /* destination id, -1 none */
POLY_G4 D_8009CE6C[2]; /* full-screen fade, per display buffer */
struct DriftVelocity *D_8009CEB4;
s32 D_8009CEB8;
CdlLOC D_8009CEBC; /* request position */
s32 D_8009CEC0;
struct TrailPoint D_8009CEC4[32];
s32 D_8009D144;
u16 *D_8009D148; /* per-row wobble spread */
s32 D_8009D14C;
struct Drift *D_8009D150;
s16 D_8009D154; /* trail index */
struct QuadBuffer *D_8009D158[2];
s32 D_8009D160;
DR_MOVE D_8009D164[2]; /* haze copy-back, per display buffer */
struct PolyG4 D_8009D194[4][2]; /* sky gradient bands, per buffer */
s32 D_8009D2B4;
POLY_FT4 D_8009D2B8[2]; /* destination marker, per display buffer */
void *D_8009D308;
struct WorldmapSpot *D_8009D30C; /* ring of 16 recent positions */
DR_TPAGE D_8009D310; /* fade blend mode */
struct BlockGrid D_8009D318; /* previous terrain blocks */
struct DiscReadRequest *volatile D_8009D3BC; /* next disc request */
void *D_8009D3C0; /* host-file request buffer */
s32 D_8009D3C4;
s32 D_8009D3C8;
s32 D_8009D3CC;
s32 D_8009D3D0;
s32 D_8009D3D4;
DR_TWIN D_8009D3D8[2];
s32 D_8009D3F0; /* camera distance */
struct WorldmapSpot *D_8009D3F4;
FileRequest D_8009D3F8[16]; /* shared read list */
u16 D_8009D478[16]; /* terrain CLUTs */
Window D_8009D498; /* path name window */
void *D_8009D528;
u16 D_8009D52C;
s32 D_8009D530; /* unreferenced */
MATRIX D_8009D534;
s32 D_8009D554;
s16 D_8009D558;
struct Camera D_8009D55C; /* saved camera */
u32 D_8009D56C; /* sectors left */
struct BlockGrid D_8009D570; /* current terrain blocks */
s32 D_8009D614;
s16 D_8009D618[25]; /* 5x5 visible blocks; -1 empty */
s32 D_8009D64C;
s16 D_8009D650[25][4]; /* per block: visibility of its 4 quarters */
s16 D_8009D718[16]; /* probe hits: face and kind pairs */
u8 D_8009D738;
void *D_8009D73C[16];
s32 *D_8009D77C;
struct TexAnim *D_8009D780;
void *D_8009D784;
struct DiscReadRequest *D_8009D788[16]; /* submitted disc request lists */
s32 *D_8009D7C8;
s32 D_8009D7CC;
struct TexAnim *D_8009D7D0;
void *D_8009D7D4; /* unused bytes drained from the final CD sector */
struct PathRegion *D_8009D7D8; /* the current path, -1 none */
s32 D_8009D7DC; /* terrain POLY_FT3 packets used this frame */
s16 D_8009D7E0; /* scene object count */
s32 D_8009D7E4; /* unreferenced */
void *D_8009D7E8[2];
s32 D_8009D7F0; /* current buffer */
s32 D_8009D7F4;
void *D_8009D7F8[2];
s32 D_8009D800;
s32 D_8009D804;
s32 D_8009D808;
s32 D_8009D80C;

#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "party.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"
