/* The field overlay's common (uninitialized global) variables. The field
 * BSS starts at the program's end (800af5e8) with the units' own variables,
 * field_800854D0's and field_800A9274's statics; the original linker then
 * allocated the commons, which every unit reaches, in an order of its own up
 * to the end the resident's mode table clears (the word at 800c426c). This
 * unit, linked last, defines them in that order, each in a slot of whole
 * words (decomp/Makefile, uninitialized variables). Nothing addresses the
 * words marked unreferenced. GCC emits tentative definitions in the order of
 * their first declaration, so they are defined ahead of the field headers
 * that declare them, the field's own types by their tags; the headers then
 * complete and check the types. */
#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gpu.h"

s32 D_800AF858;
MATRIX D_800AF85C;
u16 *D_800AF87C; /* the band's working pixels */
struct FieldView D_800AF880;
s32 D_800AFB58[44]; /* unreferenced */
u16 D_800AFC08[16]; /* compass colours read back from VRAM */
RECT D_800AFC28;
MATRIX D_800AFC30; /* sprite view rotation matrix */
u8 *D_800AFC50;
s32 D_800AFC54;
RECT D_800AFC58; /* screen band saved by event op dd */
POLY_FT4 *D_800AFC60[2];
struct FieldSprites *D_800AFC68;
u16 D_800AFC6C; /* buttons held since the last record */
struct ScreenColumn *D_800AFC70; /* saved screen column */
s32 D_800AFC74; /* sprites created */
s32 D_800AFC78;
s32 D_800AFC7C; /* batch limit */
RECT D_800AFC80[16]; /* text texture windows */
s32 D_800AFD00; /* unreferenced */
s32 D_800AFD04; /* dialogue gate */
void *D_800AFD08; /* bank file being loaded */
s32 D_800AFD0C; /* bank file number */
s32 D_800AFD10; /* attributes before the first triangle */
s32 D_800AFD14;
s32 D_800AFD18; /* bank slot being loaded */
s32 D_800AFD1C; /* current actor index */
s16 D_800AFD20;
u16 D_800AFD24[128]; /* compass palette */
DR_MODE D_800AFE24[2]; /* fade draw mode per buffer */
RECT D_800AFE3C[2]; /* fade texture windows */
RECT D_800AFE4C; /* fade copy source */
TILE D_800AFE54[2]; /* fade tile per buffer */
s32 D_800AFE74;
s32 D_800AFE78;
s32 D_800AFE7C;
void *D_800AFE80;
s32 D_800AFE84;
struct EmitterSlot D_800AFE88[3];
u16 D_800AFE9C; /* held pad buttons */
u16 D_800AFEA0;
void (*D_800AFEA4)(s32); /* stream chunk callback */
struct WindowList D_800AFEA8;
s32 D_800AFFEC;
s32 D_800AFFF0[21]; /* unreferenced */
s32 D_800B0044;
s32 D_800B0048;
RECT D_800B004C; /* compass colour strip */
s8 *D_800B0054[2]; /* pointer pad buffers */
u16 D_800B005C; /* pointer X divisor */
u16 D_800B0060; /* pointer Y divisor */
s32 D_800B0064;
s32 D_800B0068[2]; /* pointer X per port */
s32 D_800B0070[2]; /* pointer Y per port */
struct FieldActor *D_800B0078; /* current event actor */
Panorama *D_800B007C;
struct FieldEventParams D_800B0080;
s32 D_800B00B4; /* camera pitch */
SVECTOR D_800B00B8; /* piece rotation */
s32 D_800B00C0; /* yield */
struct ScreenGrid *D_800B00C4;
struct FileRequest D_800B00C8[3]; /* file list read by 80029afc */
void *D_800B00E0; /* shared wave bank buffer */
s32 D_800B00E4;
MATRIX D_800B00E8; /* instance view: the rotation with its translation */
s16 D_800B0108[64]; /* effect slot owners, -1 free */
struct OverlaySprites D_800B0188;
u8 D_800B02C8;
struct Record78 D_800B02CC[8]; /* the eight particle emitters */
s32 D_800B068C[4];
void *D_800B069C;
s32 D_800B06A0;
struct FieldSlot6 D_800B06A4[3];
struct FieldDescriptor *D_800B06B8; /* descriptor of the running actor */
struct FieldMarker D_800B06BC[25]; /* ring, letters, needle and pointer quads */
struct ScreenPieces D_800B11AC;
s32 D_800B14A4;
s32 D_800B14A8; /* nibble counter */
u16 D_800B14AC;
u8 D_800B14B0[64]; /* effect slot states */
struct FieldHistory D_800B14F0[32];
struct PictureMarks *D_800B1DF0;
/* Draw modes per buffer and texture window: 0 text, 1 compass, 3 distortion, 4 grid. */
DR_MODE D_800B1DF4[2][16];
u32 *D_800B1F74; /* TIM image held by instruction 0x77 */
struct SpriteSlotTable D_800B1F78;
struct FieldWork D_800B2078;
struct SoundBank *D_800B235C; /* movie sound-effect bank */
s32 D_800B2360[3]; /* movement history index per party slot */
u16 D_800B236C; /* menu parameter set by ext 99 */
s32 D_800B2370; /* music-wave chunks gathered */
struct FieldLaunch D_800B2374; /* effect launch for ext 90 and 93 */
s32 D_800B2388[3]; /* unreferenced */
/* The 801e module's file list: two files per layer (at most four), the
 * module file and the zero end. */
struct FileRequest D_800B2394[10];
s32 D_800B23E4[46]; /* unreferenced */
struct FieldDrawBlock D_800B249C[2];
s32 D_800C2684; /* piece scale, 0x1000 = 1 */
u32 D_800C2688; /* current word */
s32 D_800C268C;
s16 D_800C2690[2]; /* only cleared (80077620) */
u16 D_800C2694; /* newly pressed pad buttons */
struct DialogueWindow D_800C2698[4];
u16 D_800C38F8;
s16 D_800C38FC[2]; /* only cleared (80077620) */
u16 D_800C3900; /* pad buttons held */
u32 *D_800C3904; /* packed stream */
u16 D_800C3908; /* pad buttons pressed */
u32 *D_800C390C; /* converted pixels */
s32 D_800C3910; /* history reset */
s32 D_800C3914;
struct Record78 *D_800C3918[64]; /* effect slot emitters */
s32 D_800C3A18;
void *D_800C3A1C; /* music-wave gather buffer */
struct FieldMovieRequest D_800C3A20;
struct ScreenPieces *D_800C3A3C; /* the picture's three pieces */
s32 D_800C3A40; /* fade radius */
s32 D_800C3A44; /* pointer bounds */
u16 *D_800C3A48; /* the band's saved pixels */
s32 D_800C3A4C;
s32 D_800C3A50;
s32 D_800C3A54;
s32 D_800C3A58; /* unreferenced */
s32 D_800C3A5C;
s32 D_800C3A60;
s32 D_800C3A64; /* movie sound timeline position */
/* Event variables: the game state's 0x200, then 0x200 cleared on entry. */
s16 D_800C3A68[0x400];
s32 D_800C4268; /* dialogue windows opened this pass */
struct FieldDrawBlock *D_800C426C; /* current draw block */

#include "field.h"
#include "field_gte.h"
#include "field_motion.h"
#include "field_script.h"
#include "field_actor_events.h"
#include "field_screen.h"
#include "field_movie.h"
#include "field_panel.h"
#include "field_effect.h"
#include "field_picture.h"
#include "field_glyph.h"
#include "field_party.h"
