/* The resident's commons (uninitialized global variables) that the original
 * linker allocated after every unit's own small variables, from 80059404 up
 * to the first unit's larger ones (main's kernel menu buffers, 800595e8), in
 * an order of its own. This unit, linked there, defines them in that order,
 * each in a slot of whole words (decomp/Makefile). The units that address
 * their own small commons through $gp (the sprite units, the menu support,
 * the battle entry) keep their tentative definitions, which merge with these.
 * GCC emits tentative definitions in the order of their first declaration,
 * so they are defined ahead of the headers that declare them, structures by
 * their tags; the headers then check the declarations. Names the code uses
 * for bytes inside one of these objects are in link.ld. Variables no
 * resident code addresses are marked unreferenced, or with the overlays that
 * use them. */
#include "common.h"
#include "psyq/libcd.h"
#include "psyq/libgte.h"
#include "psyq/libspu.h"

s32 D_80059404;
struct SoundReverb D_80059408; /* reverb type, delay and feedback */
SpuVolume D_8005940C; /* reverb depth */
struct SoundBlock *D_80059410; /* the sound driver's pool head */
u16 D_80059414; /* text CLUTs */
u8 D_80059418; /* play time seconds */
u16 D_8005941C; /* battle */
u8 D_80059420; /* play time minutes */
struct RenderPacket *D_80059424; /* the primitive being built */
s32 D_80059428; /* frames the main task list stays paused */
u8 D_8005942C;
u8 D_80059430;
u8 D_80059434;
u8 D_80059438;
u8 D_8005943C;
struct SoundBank *D_80059440; /* loaded banks */
u8 D_80059444;
u8 D_80059448;
u8 D_8005944C;
u8 D_80059450;
u16 D_80059454; /* battle */
struct SoundTransfer *D_80059458; /* the SPU transfer ring */
void *D_8005945C;
u8 D_80059460; /* menu screen */
s32 D_80059464; /* active main-list tasks */
u8 D_80059468[3]; /* battle */
u8 D_8005946C;
u8 *D_80059470; /* the scene music sequence */
s32 D_80059474; /* unreferenced */
s32 D_80059478; /* voice count of the effect channels */
u8 D_8005947C; /* pending scene + 1 */
void *D_80059480; /* heap marker for the high-memory reservation */
u8 D_80059484; /* play time hours */
s32 D_80059488; /* vertical blank count */
u16 D_8005948C; /* pad buttons pressed */
u16 D_80059490; /* pad buttons pressed, second port */
s16 D_80059494;
s32 *D_80059498; /* lit-color cache */
u8 *D_8005949C; /* the scene music instrument data */
s32 D_800594A0; /* unreferenced */
u16 D_800594A4; /* pad buttons repeated */
u16 D_800594A8; /* pad buttons repeated, second port */
void *D_800594AC; /* reservation below the heap marker */
s32 D_800594B0; /* unreferenced */
u8 *D_800594B4; /* the first sprite queue's entry block */
u8 *D_800594B8; /* the second's */
void *D_800594BC; /* battle: the action file */
struct Task *D_800594C0;
struct ImageUpload *D_800594C4[2]; /* the upload list of each sprite queue */
u8 D_800594CC;
u8 D_800594D0;
u8 D_800594D4[3]; /* window colour */
u32 D_800594D8; /* SPU address of the reverb work area, -1 none */
u16 D_800594DC;
u16 D_800594E0;
s32 D_800594E4; /* random state */
u16 D_800594E8;
u16 D_800594EC;
s32 *D_800594F0; /* battle */
u16 D_800594F4; /* transfer ring write index */
u8 D_800594F8;
u32 D_800594FC; /* voices held */
s16 D_80059500; /* last sound driver error */
u32 D_80059504; /* the sound driver's tick count */
u8 D_80059508; /* battle, field, world map */
void (*D_8005950C)(void);
u16 D_80059510; /* transfer ring read index */
s32 D_80059514;
struct SoundModeVoice *D_80059518;
s32 D_8005951C;
s32 D_80059520;
u8 *D_80059524; /* the sprite queue block being filled */
struct PrimitiveGroup *D_80059528; /* the primitive group being drawn */
SVECTOR *D_8005952C; /* vertex normals of the model being drawn */
CdlATV D_80059530; /* CD audio mix */
u8 *D_80059534; /* the end of the sprite queue block */
u8 *D_80059538;
SVECTOR *D_8005953C; /* vertices of the model being drawn */
s32 D_80059540; /* timed ticks */
s32 D_80059544; /* voices kept for music */
s16 D_80059548; /* result of the last decoded-data read */
u8 D_8005954C;
u32 D_80059550; /* voices to key off */
u32 D_80059554; /* voices whose registers changed */
struct SoundSequence *D_80059558; /* loaded wave banks */
u16 D_8005955C; /* pending SPU IRQ re-enable */
struct SoundSequence *D_80059560; /* resident wave banks */
struct SoundSeq *D_80059564; /* playing sequences */
u32 *D_80059568; /* the ordering table models are drawn into */
s32 D_8005956C;
u16 D_80059570; /* held pad buttons */
u16 D_80059574; /* held pad buttons, second port */
s32 D_80059578; /* primitives drawn */
s16 D_8005957C; /* sound driver state flags */
struct SpriteQueueEntry *D_80059580; /* the next free sprite queue entry */
s32 D_80059584;
s32 D_80059588;
struct Task *D_8005958C;
struct Task *D_80059590;
struct Task *D_80059594;
CVECTOR D_80059598; /* the model colour 8002c6e0 sets; the renderers load it into the GTE */
u8 D_8005959C;
s32 D_800595A0;
s32 D_800595A4; /* the zeroed transfer buffer */
void *D_800595A8;
struct SoundSequence *D_800595AC; /* wave bank 5 */
s32 D_800595B0[3]; /* unreferenced */
s32 D_800595BC; /* sound driver event */
s32 D_800595C0; /* primitives submitted */
s32 D_800595C4; /* root counter time spent in ticks */
u16 D_800595C8;
u16 D_800595CC;
struct SoundBank *D_800595D0;
u16 D_800595D4;
struct SoundSeq *D_800595D8; /* the sound effect channels */
s32 D_800595DC;
s32 D_800595E0;
u32 D_800595E4; /* end of the sound driver's pool */

#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/task.h"
#include "resident/text.h"
#include "resident/window.h"
