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

s32 sound_channels_per_effect; /* 80059404 */
struct SoundReverb sound_reverb_settings; /* 80059408: reverb type, delay and feedback */
SpuVolume sound_reverb_depth; /* 8005940C: reverb depth */
struct SoundBlock *sound_memory_pool_head; /* 80059410: the sound driver's pool head */
u16 text_plane1_clut; /* 80059414: text CLUTs */
u8 pad_play_time_seconds; /* 80059418: play time seconds */
u16 mode_battle_turn_count; /* 8005941C: battle */
u8 pad_play_time_minutes; /* 80059420: play time minutes */
struct RenderPacket *model_current_packet; /* 80059424: the primitive being built */
s32 task_main_pause_timer; /* 80059428: frames the main task list stays paused */
u8 mode_battle_return_fade; /* 8005942C */
u8 pad_port0_left_stick_x; /* 80059430 */
u8 pad_port1_left_stick_x; /* 80059434 */
u8 pad_port0_left_stick_y; /* 80059438 */
u8 pad_port1_left_stick_y; /* 8005943C */
struct SoundBank *sound_effect_bank_list; /* 80059440: loaded banks */
u8 pad_port0_right_stick_x; /* 80059444 */
u8 pad_port1_right_stick_x; /* 80059448 */
u8 pad_port0_right_stick_y; /* 8005944C */
u8 pad_port1_right_stick_y; /* 80059450 */
u16 mode_battle_camera_range; /* 80059454: battle */
struct SoundTransfer *sound_transfer_ring; /* 80059458: the SPU transfer ring */
void *menu_state_resource_file; /* 8005945C */
u8 menu_state_screen; /* 80059460: menu screen */
s32 task_active_main_count; /* 80059464: active main-list tasks */
u8 mode_battle_party_ids[3]; /* 80059468: battle */
u8 mode_unread_battle_setup_byte; /* 8005946C */
u8 *mode_battle_stage_file; /* 80059470: the battle stage file (mode_load_battle_stage; ovl2615 func_801E7210) */
s32 commons_unused_word_a; /* 80059474: unreferenced */
s32 sound_effect_channel_count; /* 80059478: voice count of the effect channels */
u8 mode_pending_battle_formation; /* 8005947C: the next battle's formation + 1 (resident/mode.h) */
void *mode_battle_heap_marker; /* 80059480: heap marker for the high-memory reservation */
u8 pad_play_time_hours; /* 80059484: play time hours */
s32 pad_vblank_count; /* 80059488: vertical blank count */
u16 pad_port0_pressed; /* 8005948C: pad buttons pressed */
u16 pad_port1_pressed; /* 80059490: pad buttons pressed, second port */
s16 task_catch_up_frame_count; /* 80059494 */
s32 *model_lit_color_cache; /* 80059498: lit-color cache */
u8 *mode_battle_scene_file; /* 8005949C: the battle scene data (mode_load_battle_stage; ovl2615 func_801E7210) */
s32 commons_unused_word_b; /* 800594A0: unreferenced */
u16 pad_port0_repeated; /* 800594A4: pad buttons repeated */
u16 pad_port1_repeated; /* 800594A8: pad buttons repeated, second port */
void *mode_battle_heap_reservation; /* 800594AC: reservation below the heap marker */
s32 commons_unused_word_c; /* 800594B0: unreferenced */
u8 *sprite_queue_entry_blocks; /* 800594B4: the first sprite queue's entry block */
u8 *sprite_queue_second_entry_block; /* 800594B8: the second's */
void *mode_battle_action_stream_ring; /* 800594BC: battle: the action file */
struct Task *task_current_node; /* 800594C0 */
struct ImageUpload *sprite_queue_upload_lists[2]; /* 800594C4: the upload list of each sprite queue */
u8 menu_state_saved_cursor; /* 800594CC */
u8 mode_result_code; /* 800594D0 */
u8 window_color[3]; /* 800594D4: window colour */
u32 sound_reverb_work_address; /* 800594D8: SPU address of the reverb work area, -1 none */
u16 pad_port0_unread_buttons_a; /* 800594DC */
u16 pad_port1_unread_buttons_a; /* 800594E0 */
s32 sound_random_state; /* 800594E4: random state */
u16 pad_port0_unread_buttons_b; /* 800594E8 */
u16 pad_port1_unread_buttons_b; /* 800594EC */
s32 *mode_battle_action_command_file; /* 800594F0: battle */
u16 sound_transfer_ring_write_index; /* 800594F4: transfer ring write index */
u8 mode_battle_standalone; /* 800594F8 */
u32 sound_pending_key_on_mask; /* 800594FC: voices held */
s16 sound_unread_last_error; /* 80059500: last sound driver error */
u32 sound_tick_count; /* 80059504: the sound driver's tick count */
u8 formation_selected_index; /* 80059508: battle, field, world map */
void (*sound_spu_irq_hook)(void); /* 8005950C */
u16 sound_transfer_ring_read_index; /* 80059510: transfer ring read index */
s32 sound_unread_spu_irq_count; /* 80059514 */
struct SoundModeVoice *sound_output_mode_voice; /* 80059518 */
s32 sound_unread_memory_pool_size; /* 8005951C */
s32 mode_battle_stage_unused_word; /* 80059520 */
u8 *sprite_queue_block_start; /* 80059524: the sprite queue block being filled */
struct PrimitiveGroup *model_current_primitive_group; /* 80059528: the primitive group being drawn */
SVECTOR *model_current_normals; /* 8005952C: vertex normals of the model being drawn */
CdlATV sound_cd_mix; /* 80059530: CD audio mix */
u8 *sprite_queue_block_end; /* 80059534: the end of the sprite queue block */
u8 *model_current_aux_data; /* 80059538 */
SVECTOR *model_current_vertices; /* 8005953C: vertices of the model being drawn */
s32 sound_unread_timed_tick_count; /* 80059540: timed ticks */
s32 sound_effect_voice_count; /* 80059544: voices kept for music */
s16 sound_unread_decoded_read_result; /* 80059548: result of the last decoded-data read */
u8 mode_battle_kind; /* 8005954C */
u32 sound_pending_key_off_mask; /* 80059550: voices to key off */
u32 sound_changed_voice_mask; /* 80059554: voices whose registers changed */
struct SoundSequence *sound_wave_bank_list; /* 80059558: loaded wave banks */
u16 sound_pending_irq_enable; /* 8005955C: pending SPU IRQ re-enable */
struct SoundSequence *mode_shared_wave_bank; /* 80059560: resident wave banks */
struct SoundSeq *sound_playing_seq_list; /* 80059564: playing sequences */
u32 *model_ot; /* 80059568: the ordering table models are drawn into */
s32 sprite_ot; /* 8005956C */
u16 pad_port0_held; /* 80059570: held pad buttons */
u16 pad_port1_held; /* 80059574: held pad buttons, second port */
s32 model_drawn_primitive_count; /* 80059578: primitives drawn */
s16 sound_driver_flags; /* 8005957C: sound driver state flags */
struct SpriteQueueEntry *sprite_queue_next_free; /* 80059580: the next free sprite queue entry */
s32 sound_wave_bank_stream_address; /* 80059584 */
s32 sound_wave_bank_stream_bytes_left; /* 80059588 */
struct Task *task_main_list; /* 8005958C */
struct Task *task_next_node; /* 80059590 */
struct Task *task_draw_list; /* 80059594 */
CVECTOR model_color; /* 80059598: the model colour 8002c6e0 sets; the renderers load it into the GTE */
u8 mode_battle_debug_page; /* 8005959C */
s32 window_semi_transparency_mode; /* 800595A0 */
s32 sound_reverb_clear_buffer; /* 800595A4: the zeroed transfer buffer */
void *mode_battle_setup_archive; /* 800595A8 */
struct SoundSequence *mode_wave_bank_5; /* 800595AC: wave bank 5 */
s32 commons_unused_3_words_a[3]; /* 800595B0: unreferenced */
s32 sound_tick_event; /* 800595BC: sound driver event */
s32 model_submitted_primitive_count; /* 800595C0: primitives submitted */
s32 sound_unread_tick_time_total; /* 800595C4: root counter time spent in ticks */
u16 pad_port0_unread_buttons_c; /* 800595C8 */
u16 pad_port1_unread_buttons_c; /* 800595CC */
struct SoundBank *mode_battle_effect_bank; /* 800595D0 */
u16 text_plane0_clut; /* 800595D4 */
struct SoundSeq *sound_effect_channels; /* 800595D8: the sound effect channels */
s32 sound_reverb_clear_address; /* 800595DC */
s32 sound_reverb_clear_bytes_left; /* 800595E0 */
u32 sound_memory_pool_end; /* 800595E4: end of the sound driver's pool */

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
#include "../sound_driver.h"
