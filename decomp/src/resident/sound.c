/* The sound driver (80039e18-8003f738), GCC 2.6.3 at -G0: effect requests
 * and the effect channels, sequence control (fades, tempo, pitch, pan,
 * muting, snapshots), sequence start-up, the SPU transfer ring, the driver
 * tick that advances the sequences and stages the voice registers, the
 * sequence opcode handlers, the modulators, direct voice register writes and
 * sound file checks; its .data (0x80050624) holds the opcode handler table,
 * the note and reverb tables and the built-in error sound. The rest of the
 * driver (its start-up, banks, SPU memory and volumes) is in the preceding
 * unit, console_and_sound_driver.c: this unit starts in 80039e18-8003a094, since
 * 80039db8 needs GCC 2.7.2, 8003a094 and later match only under 2.6.3 and
 * the functions between compile identically. */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libspu.h"
#include "resident/sound.h"
#include "own_declarations.h"
#include "sound_driver.h"

/* The sequence opcode handlers (8003cd00-8003e54c): each takes the position
 * after the opcode and returns the position after its arguments. */
u8 *sound_seq_unused_opcode(u8 *data);
u8 *sound_seq_rest(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_tie(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_nop_8a(u8 *data);
u8 *sound_seq_loop_point_if(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_skip3_8e(u8 *data);
u8 *sound_seq_nop_8f(u8 *data);
u8 *sound_seq_end(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_loop_point(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_octave(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_octave_up(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_octave_down(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_time_signature(u8 *data, SoundSeq *seq);
u8 *sound_seq_position(u8 *data, SoundSeq *seq);
u8 *sound_seq_set_1a(u8 *data, SoundSeq *seq);
u8 *sound_seq_add_1a(u8 *data, SoundSeq *seq);
u8 *sound_seq_repeat(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_repeat_end(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_repeat_break(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_play_effect(u8 *data);
u8 *sound_seq_stop_effect(u8 *data);
u8 *sound_seq_goto_effect(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_rate(u8 *data, SoundSeq *seq);
u8 *sound_seq_rate_add(s8 *data, SoundSeq *seq);
u8 *sound_seq_rate_slide(u8 *data, SoundSeq *seq);
u8 *sound_seq_fade_level(u8 *data, SoundSeq *seq);
u8 *sound_seq_fade_slide(u8 *data, SoundSeq *seq);
u8 *sound_seq_gate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_voice(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_instrument(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_duration_adjust(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_drum_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_drum_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_legato_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_legato_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_pitch_mod_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_pitch_mod_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_noise_clock(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_noise_clock_add(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_noise_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_noise_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_reverb_settings(u8 *data, SoundSeq *seq);
u8 *sound_seq_reverb_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_reverb_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_skip3_bc(u8 *data);
u8 *sound_seq_nop_bd(u8 *data);
u8 *sound_seq_nop_be(u8 *data);
u8 *sound_seq_instrument_reload(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_envelope_modes(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_attack_rate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_decay_rate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_sustain_rate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_release_rate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_sustain_level(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_decay_sustain(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_attack_mode(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_sustain_mode(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_release_mode(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_detune(s8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_detune_add(s8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_detune_add_fine(s8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_detune_add_word(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_pitch_slide(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_pitch_slide_hold(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_pitch_slide_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_portamento(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_vibrato(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_vibrato_wave(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_vibrato_period(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_vibrato_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_vibrato_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_level(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_level_add(s8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_level_slide(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_level_sweep(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_tremolo(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_tremolo_wave(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_tremolo_period(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_tremolo_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_tremolo_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_pan(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_pan_add(s8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_pan_slide(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_autopan_period(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_autopan(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_autopan_wave(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_autopan_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_autopan_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_modulator_select(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_modulator_depth(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_modulator_timing(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_nop_f5(u8 *data);
u8 *sound_seq_modulator_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_modulator_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_wave_bank_instrument(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_tempo(u8 *data, SoundSeq *seq);
u8 *sound_seq_wave_bank(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
u8 *sound_seq_stop_when_silent(u8 *data, SoundSeq *seq, SoundSeqChannel *channel);
s32 sound_step_pulse_wave(SoundModulator *modulator);
s32 sound_step_square_wave(SoundModulator *modulator);
s32 sound_step_sawtooth_wave(SoundModulator *modulator);
s32 sound_step_triangle_wave(SoundModulator *modulator);
s32 sound_step_ramp_wave(SoundModulator *modulator);
s32 sound_step_random_wave(SoundModulator *modulator);
s32 sound_step_signed_random_wave(SoundModulator *modulator);
void sound_switch_modulator_off(SoundModulator *modulator);

/* Handlers of the sequence opcodes 0x80-0xff. */
u8 *(*sound_seq_opcode_handlers[128])() = { /* 80050624 */
    sound_seq_rest, sound_seq_tie, sound_seq_unused_opcode, sound_seq_unused_opcode,
    sound_seq_unused_opcode, sound_seq_unused_opcode, sound_seq_unused_opcode, sound_seq_unused_opcode,
    sound_seq_unused_opcode, sound_seq_unused_opcode, sound_seq_nop_8a, sound_seq_unused_opcode,
    sound_seq_unused_opcode, sound_seq_loop_point_if, sound_seq_skip3_8e, sound_seq_nop_8f,
    sound_seq_end, sound_seq_loop_point, sound_seq_unused_opcode, sound_seq_unused_opcode,
    sound_seq_octave, sound_seq_octave_up, sound_seq_octave_down, sound_seq_time_signature,
    sound_seq_repeat, sound_seq_repeat_end, sound_seq_repeat_break, sound_seq_unused_opcode,
    sound_seq_play_effect, sound_seq_stop_effect, sound_seq_goto_effect, sound_seq_unused_opcode,
    sound_seq_rate, sound_seq_rate_add, sound_seq_rate_slide, sound_seq_unused_opcode,
    sound_seq_set_1a, sound_seq_add_1a, sound_seq_fade_level, sound_seq_fade_slide,
    sound_seq_unused_opcode, sound_seq_gate, sound_seq_voice, sound_seq_unused_opcode,
    sound_seq_instrument, sound_seq_duration_adjust, sound_seq_drum_on, sound_seq_drum_off,
    sound_seq_legato_on, sound_seq_legato_off, sound_seq_pitch_mod_on, sound_seq_pitch_mod_off,
    sound_seq_noise_clock, sound_seq_noise_clock_add, sound_seq_noise_on, sound_seq_noise_off,
    sound_seq_reverb_settings, sound_seq_unused_opcode, sound_seq_reverb_on, sound_seq_reverb_off,
    sound_seq_skip3_bc, sound_seq_nop_bd, sound_seq_nop_be, sound_seq_unused_opcode,
    sound_seq_instrument_reload, sound_seq_envelope_modes, sound_seq_attack_rate, sound_seq_decay_rate,
    sound_seq_sustain_rate, sound_seq_release_rate, sound_seq_sustain_level, sound_seq_decay_sustain,
    sound_seq_attack_mode, sound_seq_sustain_mode, sound_seq_release_mode, sound_seq_unused_opcode,
    sound_seq_unused_opcode, sound_seq_unused_opcode, sound_seq_unused_opcode, sound_seq_unused_opcode,
    sound_seq_detune, sound_seq_detune_add, sound_seq_detune_add_fine, sound_seq_detune_add_word,
    sound_seq_pitch_slide, sound_seq_pitch_slide_hold, sound_seq_portamento, sound_seq_vibrato_period,
    sound_seq_vibrato, sound_seq_vibrato_wave, sound_seq_vibrato_on, sound_seq_vibrato_off,
    sound_seq_pitch_slide_off, sound_seq_unused_opcode, sound_seq_unused_opcode, sound_seq_unused_opcode,
    sound_seq_level, sound_seq_level_add, sound_seq_level_slide, sound_seq_tremolo_period,
    sound_seq_tremolo, sound_seq_tremolo_wave, sound_seq_tremolo_on, sound_seq_tremolo_off,
    sound_seq_pan, sound_seq_pan_add, sound_seq_pan_slide, sound_seq_autopan_period,
    sound_seq_autopan, sound_seq_autopan_wave, sound_seq_autopan_on, sound_seq_autopan_off,
    sound_seq_modulator_select, sound_seq_modulator_depth, sound_seq_modulator_timing, sound_seq_unused_opcode,
    sound_seq_unused_opcode, sound_seq_nop_f5, sound_seq_modulator_on, sound_seq_modulator_off,
    sound_seq_level_sweep, sound_seq_position, sound_seq_unused_opcode, sound_seq_unused_opcode,
    sound_seq_wave_bank_instrument, sound_seq_tempo, sound_seq_wave_bank, sound_seq_stop_when_silent,
};

/* Lengths of the sequence opcodes 0x80-0xff, including the opcode byte. */
u8 sound_seq_opcode_lengths[128] = { /* 80050824 */
    2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 4, 1, 1, 1, 0, 0, 2, 1, 1, 3, 2, 1, 1, 0, 4, 4, 4, 0,
    2, 2, 3, 0, 2, 2, 2, 3, 0, 2, 2, 0, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 4, 0, 1, 1, 4, 1, 1, 0,
    1, 4, 2, 2, 2, 2, 2, 3, 2, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 3, 3, 1, 2, 2, 4, 4, 1, 1, 1, 0, 0, 0,
    2, 2, 3, 2, 4, 4, 1, 1, 2, 2, 3, 2, 4, 4, 1, 1, 4, 4, 3, 0, 0, 2, 2, 2, 4, 3, 0, 0, 3, 2, 2, 1,
};

/* Modulator waves by shape, indexed by mode & 0xF (D9/E5/ED/F0); shapes 8-15
 * only switch the modulator off. tools/analysis/sound_sequence.py counts the
 * shapes the data install. */
s32 (*sound_modulator_waves[16])(SoundModulator *modulator) = { /* 800508A4 */
    sound_step_pulse_wave, sound_step_square_wave, sound_step_sawtooth_wave, sound_step_triangle_wave,
    sound_step_ramp_wave, sound_step_ramp_wave, sound_step_random_wave, sound_step_signed_random_wave,
    (s32 (*)(SoundModulator *))sound_switch_modulator_off, (s32 (*)(SoundModulator *))sound_switch_modulator_off,
    (s32 (*)(SoundModulator *))sound_switch_modulator_off, (s32 (*)(SoundModulator *))sound_switch_modulator_off,
    (s32 (*)(SoundModulator *))sound_switch_modulator_off, (s32 (*)(SoundModulator *))sound_switch_modulator_off,
    (s32 (*)(SoundModulator *))sound_switch_modulator_off, (s32 (*)(SoundModulator *))sound_switch_modulator_off,
};

SpuRegs *sound_spu_registers = (SpuRegs *)0x1F801C00; /* 800508E4: the SPU registers */

/* Reverb work area size of each reverb type (80038934). */
s32 sound_reverb_work_area_sizes[10] = { /* 800508E8 */
    0x80, 0x26C0, 0x1F40, 0x4840, 0x6FE0, 0xADE0, 0xF6C0, 0x18040, 0x18040, 0x3C00,
};

/* The built-in error sound (8003f6b0): an effect bank ("seds", used in place
 * as a SoundBank with id 0x7f04) and its wave bank ("wds "). */
extern u8 sound_error_effect_bank[];
INCLUDE_ASSET(".data", sound_error_effect_bank, 0x80050910, 0x30);
extern u8 sound_error_wave_bank[];
INCLUDE_ASSET(".data", sound_error_wave_bank, 0x80050940, 0x70);

/* Encoded notes: 12 semitones of 19 durations (0: a duration byte follows).
 * Each note's duration, then its semitone. */
u8 sound_note_durations[228] = { /* 800509B0 */
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
    0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2,
};
u8 sound_note_semitones[228] = { /* 80050A94 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
};

/* Octave (high nibble) and pitch row (low nibble) of each semitone. */
u8 sound_semitone_octave_rows[120] = { /* 80050B78 */
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 80, 81, 82, 83,
    84, 85, 86, 87, 88, 89, 90, 91, 96, 97, 98, 99, 100, 101, 102, 103,
    104, 105, 106, 107, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123,
    128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 144, 145, 146, 147,
    148, 149, 150, 151, 152, 0, 0, 0,
};

/* SPU pitch per 1/256 semitone over one octave: 12 rows of 256. */
s16 sound_pitch_table[3072] = { /* 80050BF0 */
    8192, 8194, 8196, 8198, 8199, 8201, 8203, 8205, 8207, 8209, 8211, 8212,
    8214, 8216, 8218, 8220, 8222, 8223, 8225, 8227, 8229, 8231, 8233, 8235,
    8236, 8238, 8240, 8242, 8244, 8246, 8248, 8250, 8251, 8253, 8255, 8257,
    8259, 8261, 8263, 8264, 8266, 8268, 8270, 8272, 8274, 8276, 8277, 8279,
    8281, 8283, 8285, 8287, 8289, 8291, 8292, 8294, 8296, 8298, 8300, 8302,
    8304, 8306, 8307, 8309, 8311, 8313, 8315, 8317, 8319, 8321, 8322, 8324,
    8326, 8328, 8330, 8332, 8334, 8336, 8337, 8339, 8341, 8343, 8345, 8347,
    8349, 8351, 8353, 8354, 8356, 8358, 8360, 8362, 8364, 8366, 8368, 8369,
    8371, 8373, 8375, 8377, 8379, 8381, 8383, 8385, 8387, 8388, 8390, 8392,
    8394, 8396, 8398, 8400, 8402, 8404, 8405, 8407, 8409, 8411, 8413, 8415,
    8417, 8419, 8421, 8423, 8424, 8426, 8428, 8430, 8432, 8434, 8436, 8438,
    8440, 8442, 8443, 8445, 8447, 8449, 8451, 8453, 8455, 8457, 8459, 8461,
    8463, 8464, 8466, 8468, 8470, 8472, 8474, 8476, 8478, 8480, 8482, 8484,
    8485, 8487, 8489, 8491, 8493, 8495, 8497, 8499, 8501, 8503, 8505, 8507,
    8508, 8510, 8512, 8514, 8516, 8518, 8520, 8522, 8524, 8526, 8528, 8530,
    8532, 8533, 8535, 8537, 8539, 8541, 8543, 8545, 8547, 8549, 8551, 8553,
    8555, 8557, 8559, 8560, 8562, 8564, 8566, 8568, 8570, 8572, 8574, 8576,
    8578, 8580, 8582, 8584, 8586, 8588, 8590, 8591, 8593, 8595, 8597, 8599,
    8601, 8603, 8605, 8607, 8609, 8611, 8613, 8615, 8617, 8619, 8621, 8623,
    8624, 8626, 8628, 8630, 8632, 8634, 8636, 8638, 8640, 8642, 8644, 8646,
    8648, 8650, 8652, 8654, 8656, 8658, 8660, 8662, 8663, 8665, 8667, 8669,
    8671, 8673, 8675, 8677, 8679, 8681, 8683, 8685, 8687, 8689, 8691, 8693,
    8695, 8697, 8699, 8701, 8703, 8705, 8707, 8709, 8711, 8712, 8714, 8716,
    8718, 8720, 8722, 8724, 8726, 8728, 8730, 8732, 8734, 8736, 8738, 8740,
    8742, 8744, 8746, 8748, 8750, 8752, 8754, 8756, 8758, 8760, 8762, 8764,
    8766, 8768, 8770, 8772, 8774, 8776, 8778, 8780, 8782, 8784, 8786, 8787,
    8789, 8791, 8793, 8795, 8797, 8799, 8801, 8803, 8805, 8807, 8809, 8811,
    8813, 8815, 8817, 8819, 8821, 8823, 8825, 8827, 8829, 8831, 8833, 8835,
    8837, 8839, 8841, 8843, 8845, 8847, 8849, 8851, 8853, 8855, 8857, 8859,
    8861, 8863, 8865, 8867, 8869, 8871, 8873, 8875, 8877, 8879, 8881, 8883,
    8885, 8887, 8889, 8891, 8893, 8895, 8897, 8899, 8901, 8903, 8905, 8907,
    8909, 8911, 8913, 8915, 8917, 8919, 8921, 8923, 8925, 8927, 8929, 8931,
    8933, 8935, 8937, 8939, 8942, 8944, 8946, 8948, 8950, 8952, 8954, 8956,
    8958, 8960, 8962, 8964, 8966, 8968, 8970, 8972, 8974, 8976, 8978, 8980,
    8982, 8984, 8986, 8988, 8990, 8992, 8994, 8996, 8998, 9000, 9002, 9004,
    9006, 9008, 9010, 9012, 9014, 9016, 9019, 9021, 9023, 9025, 9027, 9029,
    9031, 9033, 9035, 9037, 9039, 9041, 9043, 9045, 9047, 9049, 9051, 9053,
    9055, 9057, 9059, 9061, 9063, 9065, 9067, 9070, 9072, 9074, 9076, 9078,
    9080, 9082, 9084, 9086, 9088, 9090, 9092, 9094, 9096, 9098, 9100, 9102,
    9104, 9106, 9108, 9111, 9113, 9115, 9117, 9119, 9121, 9123, 9125, 9127,
    9129, 9131, 9133, 9135, 9137, 9139, 9141, 9143, 9146, 9148, 9150, 9152,
    9154, 9156, 9158, 9160, 9162, 9164, 9166, 9168, 9170, 9172, 9174, 9177,
    9179, 9181, 9183, 9185, 9187, 9189, 9191, 9193, 9195, 9197, 9199, 9201,
    9204, 9206, 9208, 9210, 9212, 9214, 9216, 9218, 9220, 9222, 9224, 9226,
    9228, 9231, 9233, 9235, 9237, 9239, 9241, 9243, 9245, 9247, 9249, 9251,
    9253, 9256, 9258, 9260, 9262, 9264, 9266, 9268, 9270, 9272, 9274, 9276,
    9279, 9281, 9283, 9285, 9287, 9289, 9291, 9293, 9295, 9297, 9300, 9302,
    9304, 9306, 9308, 9310, 9312, 9314, 9316, 9318, 9321, 9323, 9325, 9327,
    9329, 9331, 9333, 9335, 9337, 9339, 9342, 9344, 9346, 9348, 9350, 9352,
    9354, 9356, 9358, 9361, 9363, 9365, 9367, 9369, 9371, 9373, 9375, 9377,
    9380, 9382, 9384, 9386, 9388, 9390, 9392, 9394, 9397, 9399, 9401, 9403,
    9405, 9407, 9409, 9411, 9414, 9416, 9418, 9420, 9422, 9424, 9426, 9428,
    9431, 9433, 9435, 9437, 9439, 9441, 9443, 9445, 9448, 9450, 9452, 9454,
    9456, 9458, 9460, 9463, 9465, 9467, 9469, 9471, 9473, 9475, 9477, 9480,
    9482, 9484, 9486, 9488, 9490, 9492, 9495, 9497, 9499, 9501, 9503, 9505,
    9507, 9510, 9512, 9514, 9516, 9518, 9520, 9522, 9525, 9527, 9529, 9531,
    9533, 9535, 9538, 9540, 9542, 9544, 9546, 9548, 9550, 9553, 9555, 9557,
    9559, 9561, 9563, 9566, 9568, 9570, 9572, 9574, 9576, 9579, 9581, 9583,
    9585, 9587, 9589, 9591, 9594, 9596, 9598, 9600, 9602, 9604, 9607, 9609,
    9611, 9613, 9615, 9617, 9620, 9622, 9624, 9626, 9628, 9631, 9633, 9635,
    9637, 9639, 9641, 9644, 9646, 9648, 9650, 9652, 9654, 9657, 9659, 9661,
    9663, 9665, 9668, 9670, 9672, 9674, 9676, 9678, 9681, 9683, 9685, 9687,
    9689, 9692, 9694, 9696, 9698, 9700, 9702, 9705, 9707, 9709, 9711, 9713,
    9716, 9718, 9720, 9722, 9724, 9727, 9729, 9731, 9733, 9735, 9738, 9740,
    9742, 9744, 9746, 9749, 9751, 9753, 9755, 9757, 9760, 9762, 9764, 9766,
    9768, 9771, 9773, 9775, 9777, 9779, 9782, 9784, 9786, 9788, 9790, 9793,
    9795, 9797, 9799, 9802, 9804, 9806, 9808, 9810, 9813, 9815, 9817, 9819,
    9821, 9824, 9826, 9828, 9830, 9833, 9835, 9837, 9839, 9841, 9844, 9846,
    9848, 9850, 9853, 9855, 9857, 9859, 9861, 9864, 9866, 9868, 9870, 9873,
    9875, 9877, 9879, 9881, 9884, 9886, 9888, 9890, 9893, 9895, 9897, 9899,
    9902, 9904, 9906, 9908, 9910, 9913, 9915, 9917, 9919, 9922, 9924, 9926,
    9928, 9931, 9933, 9935, 9937, 9940, 9942, 9944, 9946, 9949, 9951, 9953,
    9955, 9958, 9960, 9962, 9964, 9967, 9969, 9971, 9973, 9976, 9978, 9980,
    9982, 9985, 9987, 9989, 9991, 9994, 9996, 9998, 10000, 10003, 10005, 10007,
    10009, 10012, 10014, 10016, 10018, 10021, 10023, 10025, 10027, 10030, 10032, 10034,
    10037, 10039, 10041, 10043, 10046, 10048, 10050, 10052, 10055, 10057, 10059, 10061,
    10064, 10066, 10068, 10071, 10073, 10075, 10077, 10080, 10082, 10084, 10086, 10089,
    10091, 10093, 10096, 10098, 10100, 10102, 10105, 10107, 10109, 10112, 10114, 10116,
    10118, 10121, 10123, 10125, 10127, 10130, 10132, 10134, 10137, 10139, 10141, 10144,
    10146, 10148, 10150, 10153, 10155, 10157, 10160, 10162, 10164, 10166, 10169, 10171,
    10173, 10176, 10178, 10180, 10182, 10185, 10187, 10189, 10192, 10194, 10196, 10199,
    10201, 10203, 10205, 10208, 10210, 10212, 10215, 10217, 10219, 10222, 10224, 10226,
    10229, 10231, 10233, 10235, 10238, 10240, 10242, 10245, 10247, 10249, 10252, 10254,
    10256, 10259, 10261, 10263, 10266, 10268, 10270, 10272, 10275, 10277, 10279, 10282,
    10284, 10286, 10289, 10291, 10293, 10296, 10298, 10300, 10303, 10305, 10307, 10310,
    10312, 10314, 10317, 10319, 10321, 10324, 10326, 10328, 10331, 10333, 10335, 10338,
    10340, 10342, 10345, 10347, 10349, 10352, 10354, 10356, 10359, 10361, 10363, 10366,
    10368, 10370, 10373, 10375, 10377, 10380, 10382, 10384, 10387, 10389, 10391, 10394,
    10396, 10398, 10401, 10403, 10405, 10408, 10410, 10412, 10415, 10417, 10420, 10422,
    10424, 10427, 10429, 10431, 10434, 10436, 10438, 10441, 10443, 10445, 10448, 10450,
    10453, 10455, 10457, 10460, 10462, 10464, 10467, 10469, 10471, 10474, 10476, 10478,
    10481, 10483, 10486, 10488, 10490, 10493, 10495, 10497, 10500, 10502, 10505, 10507,
    10509, 10512, 10514, 10516, 10519, 10521, 10524, 10526, 10528, 10531, 10533, 10535,
    10538, 10540, 10543, 10545, 10547, 10550, 10552, 10554, 10557, 10559, 10562, 10564,
    10566, 10569, 10571, 10573, 10576, 10578, 10581, 10583, 10585, 10588, 10590, 10593,
    10595, 10597, 10600, 10602, 10605, 10607, 10609, 10612, 10614, 10617, 10619, 10621,
    10624, 10626, 10629, 10631, 10633, 10636, 10638, 10641, 10643, 10645, 10648, 10650,
    10653, 10655, 10657, 10660, 10662, 10665, 10667, 10669, 10672, 10674, 10677, 10679,
    10681, 10684, 10686, 10689, 10691, 10693, 10696, 10698, 10701, 10703, 10706, 10708,
    10710, 10713, 10715, 10718, 10720, 10722, 10725, 10727, 10730, 10732, 10735, 10737,
    10739, 10742, 10744, 10747, 10749, 10752, 10754, 10756, 10759, 10761, 10764, 10766,
    10769, 10771, 10773, 10776, 10778, 10781, 10783, 10786, 10788, 10790, 10793, 10795,
    10798, 10800, 10803, 10805, 10807, 10810, 10812, 10815, 10817, 10820, 10822, 10825,
    10827, 10829, 10832, 10834, 10837, 10839, 10842, 10844, 10847, 10849, 10851, 10854,
    10856, 10859, 10861, 10864, 10866, 10869, 10871, 10873, 10876, 10878, 10881, 10883,
    10886, 10888, 10891, 10893, 10896, 10898, 10901, 10903, 10905, 10908, 10910, 10913,
    10915, 10918, 10920, 10923, 10925, 10928, 10930, 10933, 10935, 10937, 10940, 10942,
    10945, 10947, 10950, 10952, 10955, 10957, 10960, 10962, 10965, 10967, 10970, 10972,
    10975, 10977, 10980, 10982, 10984, 10987, 10989, 10992, 10994, 10997, 10999, 11002,
    11004, 11007, 11009, 11012, 11014, 11017, 11019, 11022, 11024, 11027, 11029, 11032,
    11034, 11037, 11039, 11042, 11044, 11047, 11049, 11052, 11054, 11057, 11059, 11062,
    11064, 11067, 11069, 11072, 11074, 11077, 11079, 11082, 11084, 11087, 11089, 11092,
    11094, 11097, 11099, 11102, 11104, 11107, 11109, 11112, 11114, 11117, 11119, 11122,
    11124, 11127, 11129, 11132, 11134, 11137, 11139, 11142, 11144, 11147, 11149, 11152,
    11154, 11157, 11159, 11162, 11164, 11167, 11169, 11172, 11174, 11177, 11179, 11182,
    11185, 11187, 11190, 11192, 11195, 11197, 11200, 11202, 11205, 11207, 11210, 11212,
    11215, 11217, 11220, 11222, 11225, 11228, 11230, 11233, 11235, 11238, 11240, 11243,
    11245, 11248, 11250, 11253, 11255, 11258, 11261, 11263, 11266, 11268, 11271, 11273,
    11276, 11278, 11281, 11283, 11286, 11288, 11291, 11294, 11296, 11299, 11301, 11304,
    11306, 11309, 11311, 11314, 11317, 11319, 11322, 11324, 11327, 11329, 11332, 11334,
    11337, 11340, 11342, 11345, 11347, 11350, 11352, 11355, 11357, 11360, 11363, 11365,
    11368, 11370, 11373, 11375, 11378, 11381, 11383, 11386, 11388, 11391, 11393, 11396,
    11399, 11401, 11404, 11406, 11409, 11411, 11414, 11417, 11419, 11422, 11424, 11427,
    11429, 11432, 11435, 11437, 11440, 11442, 11445, 11448, 11450, 11453, 11455, 11458,
    11460, 11463, 11466, 11468, 11471, 11473, 11476, 11479, 11481, 11484, 11486, 11489,
    11492, 11494, 11497, 11499, 11502, 11504, 11507, 11510, 11512, 11515, 11517, 11520,
    11523, 11525, 11528, 11530, 11533, 11536, 11538, 11541, 11543, 11546, 11549, 11551,
    11554, 11557, 11559, 11562, 11564, 11567, 11570, 11572, 11575, 11577, 11580, 11583,
    11585, 11588, 11590, 11593, 11596, 11598, 11601, 11604, 11606, 11609, 11611, 11614,
    11617, 11619, 11622, 11625, 11627, 11630, 11632, 11635, 11638, 11640, 11643, 11646,
    11648, 11651, 11653, 11656, 11659, 11661, 11664, 11667, 11669, 11672, 11674, 11677,
    11680, 11682, 11685, 11688, 11690, 11693, 11696, 11698, 11701, 11703, 11706, 11709,
    11711, 11714, 11717, 11719, 11722, 11725, 11727, 11730, 11733, 11735, 11738, 11740,
    11743, 11746, 11748, 11751, 11754, 11756, 11759, 11762, 11764, 11767, 11770, 11772,
    11775, 11778, 11780, 11783, 11786, 11788, 11791, 11794, 11796, 11799, 11802, 11804,
    11807, 11810, 11812, 11815, 11818, 11820, 11823, 11826, 11828, 11831, 11834, 11836,
    11839, 11842, 11844, 11847, 11850, 11852, 11855, 11858, 11860, 11863, 11866, 11868,
    11871, 11874, 11876, 11879, 11882, 11884, 11887, 11890, 11892, 11895, 11898, 11901,
    11903, 11906, 11909, 11911, 11914, 11917, 11919, 11922, 11925, 11927, 11930, 11933,
    11935, 11938, 11941, 11944, 11946, 11949, 11952, 11954, 11957, 11960, 11962, 11965,
    11968, 11971, 11973, 11976, 11979, 11981, 11984, 11987, 11989, 11992, 11995, 11998,
    12000, 12003, 12006, 12008, 12011, 12014, 12017, 12019, 12022, 12025, 12027, 12030,
    12033, 12036, 12038, 12041, 12044, 12046, 12049, 12052, 12055, 12057, 12060, 12063,
    12065, 12068, 12071, 12074, 12076, 12079, 12082, 12085, 12087, 12090, 12093, 12095,
    12098, 12101, 12104, 12106, 12109, 12112, 12115, 12117, 12120, 12123, 12125, 12128,
    12131, 12134, 12136, 12139, 12142, 12145, 12147, 12150, 12153, 12156, 12158, 12161,
    12164, 12167, 12169, 12172, 12175, 12178, 12180, 12183, 12186, 12189, 12191, 12194,
    12197, 12200, 12202, 12205, 12208, 12211, 12213, 12216, 12219, 12222, 12224, 12227,
    12230, 12233, 12235, 12238, 12241, 12244, 12246, 12249, 12252, 12255, 12258, 12260,
    12263, 12266, 12269, 12271, 12274, 12277, 12280, 12282, 12285, 12288, 12291, 12294,
    12296, 12299, 12302, 12305, 12307, 12310, 12313, 12316, 12319, 12321, 12324, 12327,
    12330, 12332, 12335, 12338, 12341, 12344, 12346, 12349, 12352, 12355, 12357, 12360,
    12363, 12366, 12369, 12371, 12374, 12377, 12380, 12383, 12385, 12388, 12391, 12394,
    12397, 12399, 12402, 12405, 12408, 12411, 12413, 12416, 12419, 12422, 12425, 12427,
    12430, 12433, 12436, 12439, 12441, 12444, 12447, 12450, 12453, 12455, 12458, 12461,
    12464, 12467, 12470, 12472, 12475, 12478, 12481, 12484, 12486, 12489, 12492, 12495,
    12498, 12501, 12503, 12506, 12509, 12512, 12515, 12517, 12520, 12523, 12526, 12529,
    12532, 12534, 12537, 12540, 12543, 12546, 12549, 12551, 12554, 12557, 12560, 12563,
    12566, 12568, 12571, 12574, 12577, 12580, 12583, 12585, 12588, 12591, 12594, 12597,
    12600, 12602, 12605, 12608, 12611, 12614, 12617, 12620, 12622, 12625, 12628, 12631,
    12634, 12637, 12639, 12642, 12645, 12648, 12651, 12654, 12657, 12659, 12662, 12665,
    12668, 12671, 12674, 12677, 12679, 12682, 12685, 12688, 12691, 12694, 12697, 12700,
    12702, 12705, 12708, 12711, 12714, 12717, 12720, 12722, 12725, 12728, 12731, 12734,
    12737, 12740, 12743, 12745, 12748, 12751, 12754, 12757, 12760, 12763, 12766, 12768,
    12771, 12774, 12777, 12780, 12783, 12786, 12789, 12792, 12794, 12797, 12800, 12803,
    12806, 12809, 12812, 12815, 12818, 12820, 12823, 12826, 12829, 12832, 12835, 12838,
    12841, 12844, 12847, 12849, 12852, 12855, 12858, 12861, 12864, 12867, 12870, 12873,
    12876, 12878, 12881, 12884, 12887, 12890, 12893, 12896, 12899, 12902, 12905, 12908,
    12910, 12913, 12916, 12919, 12922, 12925, 12928, 12931, 12934, 12937, 12940, 12943,
    12945, 12948, 12951, 12954, 12957, 12960, 12963, 12966, 12969, 12972, 12975, 12978,
    12981, 12983, 12986, 12989, 12992, 12995, 12998, 13001, 13004, 13007, 13010, 13013,
    13016, 13019, 13022, 13025, 13027, 13030, 13033, 13036, 13039, 13042, 13045, 13048,
    13051, 13054, 13057, 13060, 13063, 13066, 13069, 13072, 13075, 13078, 13081, 13083,
    13086, 13089, 13092, 13095, 13098, 13101, 13104, 13107, 13110, 13113, 13116, 13119,
    13122, 13125, 13128, 13131, 13134, 13137, 13140, 13143, 13146, 13149, 13152, 13154,
    13157, 13160, 13163, 13166, 13169, 13172, 13175, 13178, 13181, 13184, 13187, 13190,
    13193, 13196, 13199, 13202, 13205, 13208, 13211, 13214, 13217, 13220, 13223, 13226,
    13229, 13232, 13235, 13238, 13241, 13244, 13247, 13250, 13253, 13256, 13259, 13262,
    13265, 13268, 13271, 13274, 13277, 13280, 13283, 13286, 13289, 13292, 13295, 13298,
    13301, 13304, 13307, 13310, 13313, 13316, 13319, 13322, 13325, 13328, 13331, 13334,
    13337, 13340, 13343, 13346, 13349, 13352, 13355, 13358, 13361, 13364, 13367, 13370,
    13373, 13376, 13379, 13382, 13385, 13388, 13391, 13394, 13397, 13400, 13403, 13406,
    13409, 13412, 13415, 13418, 13421, 13424, 13427, 13430, 13433, 13436, 13440, 13443,
    13446, 13449, 13452, 13455, 13458, 13461, 13464, 13467, 13470, 13473, 13476, 13479,
    13482, 13485, 13488, 13491, 13494, 13497, 13500, 13503, 13506, 13509, 13512, 13516,
    13519, 13522, 13525, 13528, 13531, 13534, 13537, 13540, 13543, 13546, 13549, 13552,
    13555, 13558, 13561, 13564, 13567, 13571, 13574, 13577, 13580, 13583, 13586, 13589,
    13592, 13595, 13598, 13601, 13604, 13607, 13610, 13613, 13617, 13620, 13623, 13626,
    13629, 13632, 13635, 13638, 13641, 13644, 13647, 13650, 13653, 13657, 13660, 13663,
    13666, 13669, 13672, 13675, 13678, 13681, 13684, 13687, 13690, 13694, 13697, 13700,
    13703, 13706, 13709, 13712, 13715, 13718, 13721, 13725, 13728, 13731, 13734, 13737,
    13740, 13743, 13746, 13749, 13752, 13756, 13759, 13762, 13765, 13768, 13771, 13774,
    13777, 13780, 13783, 13787, 13790, 13793, 13796, 13799, 13802, 13805, 13808, 13811,
    13815, 13818, 13821, 13824, 13827, 13830, 13833, 13836, 13840, 13843, 13846, 13849,
    13852, 13855, 13858, 13861, 13865, 13868, 13871, 13874, 13877, 13880, 13883, 13886,
    13890, 13893, 13896, 13899, 13902, 13905, 13908, 13912, 13915, 13918, 13921, 13924,
    13927, 13930, 13934, 13937, 13940, 13943, 13946, 13949, 13952, 13956, 13959, 13962,
    13965, 13968, 13971, 13974, 13978, 13981, 13984, 13987, 13990, 13993, 13997, 14000,
    14003, 14006, 14009, 14012, 14016, 14019, 14022, 14025, 14028, 14031, 14035, 14038,
    14041, 14044, 14047, 14050, 14054, 14057, 14060, 14063, 14066, 14069, 14073, 14076,
    14079, 14082, 14085, 14088, 14092, 14095, 14098, 14101, 14104, 14108, 14111, 14114,
    14117, 14120, 14123, 14127, 14130, 14133, 14136, 14139, 14143, 14146, 14149, 14152,
    14155, 14159, 14162, 14165, 14168, 14171, 14175, 14178, 14181, 14184, 14187, 14191,
    14194, 14197, 14200, 14203, 14207, 14210, 14213, 14216, 14219, 14223, 14226, 14229,
    14232, 14235, 14239, 14242, 14245, 14248, 14252, 14255, 14258, 14261, 14264, 14268,
    14271, 14274, 14277, 14280, 14284, 14287, 14290, 14293, 14297, 14300, 14303, 14306,
    14310, 14313, 14316, 14319, 14322, 14326, 14329, 14332, 14335, 14339, 14342, 14345,
    14348, 14352, 14355, 14358, 14361, 14365, 14368, 14371, 14374, 14377, 14381, 14384,
    14387, 14390, 14394, 14397, 14400, 14403, 14407, 14410, 14413, 14416, 14420, 14423,
    14426, 14429, 14433, 14436, 14439, 14443, 14446, 14449, 14452, 14456, 14459, 14462,
    14465, 14469, 14472, 14475, 14478, 14482, 14485, 14488, 14491, 14495, 14498, 14501,
    14505, 14508, 14511, 14514, 14518, 14521, 14524, 14527, 14531, 14534, 14537, 14541,
    14544, 14547, 14550, 14554, 14557, 14560, 14564, 14567, 14570, 14573, 14577, 14580,
    14583, 14587, 14590, 14593, 14596, 14600, 14603, 14606, 14610, 14613, 14616, 14620,
    14623, 14626, 14629, 14633, 14636, 14639, 14643, 14646, 14649, 14653, 14656, 14659,
    14663, 14666, 14669, 14672, 14676, 14679, 14682, 14686, 14689, 14692, 14696, 14699,
    14702, 14706, 14709, 14712, 14716, 14719, 14722, 14725, 14729, 14732, 14735, 14739,
    14742, 14745, 14749, 14752, 14755, 14759, 14762, 14765, 14769, 14772, 14775, 14779,
    14782, 14785, 14789, 14792, 14795, 14799, 14802, 14805, 14809, 14812, 14815, 14819,
    14822, 14826, 14829, 14832, 14836, 14839, 14842, 14846, 14849, 14852, 14856, 14859,
    14862, 14866, 14869, 14872, 14876, 14879, 14882, 14886, 14889, 14893, 14896, 14899,
    14903, 14906, 14909, 14913, 14916, 14919, 14923, 14926, 14930, 14933, 14936, 14940,
    14943, 14946, 14950, 14953, 14957, 14960, 14963, 14967, 14970, 14973, 14977, 14980,
    14984, 14987, 14990, 14994, 14997, 15000, 15004, 15007, 15011, 15014, 15017, 15021,
    15024, 15028, 15031, 15034, 15038, 15041, 15045, 15048, 15051, 15055, 15058, 15062,
    15065, 15068, 15072, 15075, 15079, 15082, 15085, 15089, 15092, 15096, 15099, 15102,
    15106, 15109, 15113, 15116, 15119, 15123, 15126, 15130, 15133, 15136, 15140, 15143,
    15147, 15150, 15154, 15157, 15160, 15164, 15167, 15171, 15174, 15178, 15181, 15184,
    15188, 15191, 15195, 15198, 15202, 15205, 15208, 15212, 15215, 15219, 15222, 15226,
    15229, 15232, 15236, 15239, 15243, 15246, 15250, 15253, 15256, 15260, 15263, 15267,
    15270, 15274, 15277, 15281, 15284, 15288, 15291, 15294, 15298, 15301, 15305, 15308,
    15312, 15315, 15319, 15322, 15325, 15329, 15332, 15336, 15339, 15343, 15346, 15350,
    15353, 15357, 15360, 15364, 15367, 15371, 15374, 15377, 15381, 15384, 15388, 15391,
    15395, 15398, 15402, 15405, 15409, 15412, 15416, 15419, 15423, 15426, 15430, 15433,
    15437, 15440, 15444, 15447, 15450, 15454, 15457, 15461, 15464, 15468, 15471, 15475,
    15478, 15482, 15485, 15489, 15492, 15496, 15499, 15503, 15506, 15510, 15513, 15517,
    15520, 15524, 15527, 15531, 15534, 15538, 15541, 15545, 15548, 15552, 15555, 15559,
    15562, 15566, 15569, 15573, 15576, 15580, 15584, 15587, 15591, 15594, 15598, 15601,
    15605, 15608, 15612, 15615, 15619, 15622, 15626, 15629, 15633, 15636, 15640, 15643,
    15647, 15650, 15654, 15658, 15661, 15665, 15668, 15672, 15675, 15679, 15682, 15686,
    15689, 15693, 15696, 15700, 15704, 15707, 15711, 15714, 15718, 15721, 15725, 15728,
    15732, 15735, 15739, 15743, 15746, 15750, 15753, 15757, 15760, 15764, 15767, 15771,
    15775, 15778, 15782, 15785, 15789, 15792, 15796, 15799, 15803, 15807, 15810, 15814,
    15817, 15821, 15824, 15828, 15832, 15835, 15839, 15842, 15846, 15849, 15853, 15857,
    15860, 15864, 15867, 15871, 15875, 15878, 15882, 15885, 15889, 15892, 15896, 15900,
    15903, 15907, 15910, 15914, 15918, 15921, 15925, 15928, 15932, 15936, 15939, 15943,
    15946, 15950, 15954, 15957, 15961, 15964, 15968, 15972, 15975, 15979, 15982, 15986,
    15990, 15993, 15997, 16000, 16004, 16008, 16011, 16015, 16018, 16022, 16026, 16029,
    16033, 16037, 16040, 16044, 16047, 16051, 16055, 16058, 16062, 16066, 16069, 16073,
    16076, 16080, 16084, 16087, 16091, 16095, 16098, 16102, 16105, 16109, 16113, 16116,
    16120, 16124, 16127, 16131, 16135, 16138, 16142, 16145, 16149, 16153, 16156, 16160,
    16164, 16167, 16171, 16175, 16178, 16182, 16186, 16189, 16193, 16197, 16200, 16204,
    16208, 16211, 16215, 16218, 16222, 16226, 16229, 16233, 16237, 16240, 16244, 16248,
    16251, 16255, 16259, 16262, 16266, 16270, 16273, 16277, 16281, 16284, 16288, 16292,
    16296, 16299, 16303, 16307, 16310, 16314, 16318, 16321, 16325, 16329, 16332, 16336,
    16340, 16343, 16347, 16351, 16354, 16358, 16362, 16366, 16369, 16373, 16377, 16380,
};

/* 80039E18: Effect requests, while effects are on (driver flag 0x800): each plays
 * `effect` (its bank id in the high half) on two effect channels
 * (sound_channels_per_effect) through 8003b644, at volume 0x6000 and centre pan 0x4000
 * unless given (as 0-0x7f, shifted into the high byte). This one uses
 * channels 12-13 with priority 0x60. */
void sound_play_effect_on_channels_12_13(s32 effect) {
    if (sound_driver_flags & 0x800) {
        sound_channels_per_effect = 2;
        sound_start_effect(0x600C, effect, 0x6000, 0x4000);
    }
}

/* 80039E60: On the two channels 8003a65c frees for it, with priority 0x20. */
void sound_play_effect(s32 effect) {
    if (sound_driver_flags & 0x800) {
        s32 id = sound_find_effect_channels(effect, 2);

        sound_channels_per_effect = 2;
        sound_start_effect(id | 0x2000, effect, 0x6000, 0x4000);
    }
}

/* 80039EC4: On the channel pair of `channel` in the other half (its even index with
 * bit 3 flipped), with priority 0x20. */
void sound_play_effect_on_channel(s32 effect, s32 channel) {
    if (sound_driver_flags & 0x800) {
        sound_channels_per_effect = 2;
        sound_start_effect(((channel & 0xFE) ^ 8) | 0x2000, effect, 0x6000, 0x4000);
    }
}

/* 80039F18: As 80039e60, at `volume` and `pan`. */
void sound_play_effect_volume_pan(s32 effect, s32 volume, s32 pan) {
    if (sound_driver_flags & 0x800) {
        s32 id = sound_find_effect_channels(effect, 2);

        sound_channels_per_effect = 2;
        sound_start_effect(id | 0x2000, effect, volume << 8, pan << 8);
    }
}

/* 80039F9C: As 80039ec4, at `volume` and `pan`. */
void sound_play_effect_on_channel_volume_pan(s32 effect, s32 channel, s32 volume, s32 pan) {
    if (sound_driver_flags & 0x800) {
        sound_channels_per_effect = 2;
        sound_start_effect(((channel & 0xFE) ^ 8) | 0x2000, effect, volume << 8, pan << 8);
    }
}

/* 80039FF8: Stop every sound effect channel and release its voice. */
void sound_stop_all_effects(void) {
    s32 count = sound_effect_channel_count;
    SoundSeq *effects = sound_effect_channels;
    SoundSeqChannel *channel = effects->channel;

    DisableEvent(sound_tick_event);
    do {
        count--;
        if (channel->flags & 1) {
            channel->flags = 0;
            sound_release_voice(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);
    effects->voices = 0;
    EnableEvent(sound_tick_event);
}

/* 8003A094: Stop the effect channels playing effects of `bank`. */
void sound_stop_bank_effects(SoundBank *bank) {
    s32 count = sound_effect_channel_count;
    SoundSeq *effects = sound_effect_channels;
    s16 id = bank->id;
    SoundSeqChannel *channel = effects->channel;

    do {
        count--;
        if ((channel->flags & 1) && channel->id.part.bank == id) {
            channel->flags = 0;
            effects->voices &= ~(1 << channel->voice_bit);
            sound_release_voice(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);
}

/* 8003A14C: Stop the effect channels playing effect `id`. */
void sound_stop_effect(s32 id) {
    SoundSeq *effects = sound_effect_channels;
    s32 count = sound_effect_channel_count;
    SoundSeqChannel *channel = effects->channel;

    do {
        count--;
        if ((channel->flags & 1) && channel->id.full == id) {
            channel->flags = 0;
            effects->voices &= ~(1 << channel->voice_bit);
            sound_release_voice(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);
}

/* 8003A20C: Stop the two effect channels of `sound`. */
void sound_stop_effect_on_channel(s32 sound) {
    SoundSeq *effects = sound_effect_channels;
    SoundSeqChannel *channel;
    s32 count;

    sound &= 0xFE;
    sound ^= 8;
    channel = &effects->channel[sound];
    count = 2;

    do {
        count--;
        if (channel->flags & 1) {
            channel->flags = 0;
            effects->voices &= ~(1 << channel->voice_bit);
            sound_release_voice(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);
}

/* 8003A2D4: Two empty driver entries. */
void sound_empty_entry_after_effect_stops(void) {
}

/* 8003A2DC */
void sound_empty_entry_before_effect_volume(void) {
}

/* 8003A2E4: Set the volume of the effect channels playing effect `id`. */
void sound_set_effect_volume(s32 id, s32 volume) {
    s32 count = sound_effect_channel_count;
    SoundSeqChannel *channel = sound_effect_channels->channel;

    do {
        if ((channel->flags & 1) && channel->id.full == id) {
            channel->volume = volume << 8;
            channel->flags2 = 0x100;
        }
        channel++;
        count--;
    } while (count != 0);
}

/* 8003A344: Set the volume of the two effect channels of `sound`. */
void sound_set_effect_volume_on_channel(s32 sound, s32 volume) {
    SoundSeqChannel *channel;
    s32 count;

    sound &= 0xFE;
    sound ^= 8;
    channel = &sound_effect_channels->channel[sound];
    count = 2;

    do {
        if (channel->flags & 1) {
            channel->volume = volume << 8;
            channel->flags2 = 0x100;
        }
        channel++;
        count--;
    } while (count != 0);
}

/* 8003A3B8: Slide the volume of the effect channels playing effect `id` over
 * `frames` (at least one). */
void sound_slide_effect_volume(s32 id, s32 volume, s32 frames) {
    s32 count = sound_effect_channel_count;
    SoundSeqChannel *channel = sound_effect_channels->channel;
    s32 delta;

    volume <<= 8;
    do {
        if ((channel->flags & 1) && channel->id.full == id) {
            delta = volume - channel->volume;
            if (delta != 0) {
                if (frames == 0) {
                    frames = 1;
                }
                channel->volume_target = volume;
                channel->volume_frames = frames;
                channel->volume_step = delta / frames;
                channel->flags3 |= 0x20;
            }
        }
        channel++;
        count--;
    } while (count != 0);
}

/* 8003A450: Slide the volume of the two effect channels of `sound`. */
void sound_slide_effect_volume_on_channel(s32 sound, s32 volume, s32 frames) {
    SoundSeqChannel *channel;
    s32 count;
    s32 delta;

    sound &= 0xFE;
    sound ^= 8;
    channel = &sound_effect_channels->channel[sound];
    count = 2;
    volume <<= 8;
    do {
        if (channel->flags & 1) {
            delta = volume - channel->volume;
            if (delta != 0) {
                if (frames == 0) {
                    frames = 1;
                }
                channel->volume_target = volume;
                channel->volume_frames = frames;
                channel->volume_step = delta / frames;
                channel->flags3 |= 0x20;
            }
        }
        channel++;
        count--;
    } while (count != 0);
}

/* 8003A4FC: Set the pan of the effect channels playing effect `id`. */
void sound_set_effect_pan(s32 id, s32 pan) {
    s32 count = sound_effect_channel_count;
    SoundSeqChannel *channel = sound_effect_channels->channel;

    do {
        if ((channel->flags & 1) && channel->id.full == id) {
            channel->pan = pan << 8;
            channel->flags2 = 0x100;
        }
        channel++;
        count--;
    } while (count != 0);
}

/* 8003A55C: Set the pan of the two effect channels of `sound`. */
void sound_set_effect_pan_on_channel(s32 sound, s32 pan) {
    SoundSeqChannel *channel;
    s32 count;

    sound &= 0xFE;
    sound ^= 8;
    channel = &sound_effect_channels->channel[sound];
    count = 2;

    do {
        if (channel->flags & 1) {
            channel->pan = pan << 8;
            channel->flags2 = 0x100;
        }
        channel++;
        count--;
    } while (count != 0);
}


/* 8003A5D0: Mask of the active effect channels (playing effect `id`, or any for -1). */
s32 sound_get_active_effect_mask(s32 id) {
    s32 bit = 1;
    s32 count = sound_effect_channel_count;
    SoundSeqChannel *channel = sound_effect_channels->channel;
    s32 mask = 0;

    if (id == -1) {
        do {
            if (channel->flags & 1) {
                mask |= bit;
            }
            channel++;
            count--;
            bit <<= 1;
        } while (count != 0);
    } else {
        do {
            count--;
            if ((channel->flags & 1) && channel->id.full == id) {
                mask |= bit;
            }
            channel++;
            bit <<= 1;
        } while (count != 0);
    }
    return mask;
}

/* 8003A65C: Stop the effect channels playing effect `id`, then choose `width`
 * adjacent effect channels for it: the highest free group below the
 * reserved top pair, else the oldest channel of low priority seen. */
u32 sound_find_effect_channels(s32 id, s32 width) {
    SoundSeq *effects = sound_effect_channels;
    s32 count = sound_effect_channel_count;
    u32 reserved = 0;
    SoundSeqChannel *channel = effects->channel;
    u32 limit;
    u32 index;
    u32 mask;
    u32 used;
    u32 group;
    u32 oldest;
    u32 found;
    s32 span;

    do {
        count--;
        if ((channel->flags & 1) && channel->id.full == id) {
            channel->flags = 0;
            effects->voices &= ~(1 << channel->voice_bit);
            sound_release_voice(&channel->state, channel->voice);
        }
        channel++;
    } while (count != 0);

    span = width + 2;
    limit = effects->channels - sound_effect_voice_count;
    index = sound_effect_channel_count - span;
    effects = sound_effect_channels;
    group = 0xFFFFFFFF >> (32 - width);
    mask = group << index;
    channel = &effects->channel[index];
    used = ~reserved & effects->voices;
    oldest = 0xFFFFFFFF;
    if (used & mask) {
        do {
            if (channel->stamp < oldest && channel->priority < 0x21) {
                oldest = channel->stamp;
                found = index;
            }
            mask >>= width;
            if (mask < group || index <= limit) {
                index = found;
                break;
            }
            channel -= width;
            index -= width;
        } while (used & mask);
    }
    return index;
}

/* 8003A82C: Whether a sequence is playing (flag bit 15). */
u32 sound_is_seq_playing(SoundSeq *seq) {
    return seq->flags >> 15;
}

/* 8003A838: Set a sequence's tempo (0 means 0x100), at once or over `frames`. */
void sound_set_seq_tempo(SoundSeq *seq, s32 tempo, s32 frames) {
    s32 delta;

    if (tempo == 0) {
        tempo = 0x100;
    }
    seq->tempo_target = tempo;
    if (frames == 0) {
        seq->tick_step = seq->rate.part.whole * tempo;
        seq->tempo_frames = 0;
        seq->tempo.value = tempo << 16;
        return;
    }
    delta = (tempo << 16) - seq->tempo.value;
    if (delta != 0) {
        seq->tempo_frames = frames;
        seq->tempo_step = delta / frames;
    }
}

void sound_resume_seq(SoundSeq *seq);

/* 8003A89C: Set a sequence's fade level, at once or over `frames`; raising the level
 * of a sequence a fade stopped resumes it. */
void sound_set_seq_fade(SoundSeq *seq, s32 fade, s32 frames) {
    s32 delta;

    seq->fade_target = fade << 8;
    if (frames == 0) {
        seq->fade.value = fade << 24;
        seq->fade_frames = 0;
        sound_request_seq_channel_updates(0x100, seq);
    } else {
        delta = (fade << 16) - (seq->fade.value >> 8);
        if (delta == 0) {
            return;
        }
        seq->fade_frames = frames;
        seq->fade_step = (delta / frames) << 8;
    }
    if ((seq->flags & 0x100) && fade != 0) {
        sound_resume_seq(seq);
    }
}

/* 8003A948: Set a sequence's pitch shift (semitones), at once or over `frames`. */
void sound_set_seq_pitch(SoundSeq *seq, s32 pitch, s32 frames) {
    s32 delta;

    seq->pitch_target = pitch << 8;
    if (frames == 0) {
        seq->pitch.value = pitch << 24;
        seq->pitch_frames = 0;
        sound_request_seq_channel_updates(0x200, seq);
        return;
    }
    delta = (pitch << 16) - (seq->pitch.value >> 8);
    if (delta != 0) {
        seq->pitch_frames = frames;
        seq->pitch_step = (delta / frames) << 8;
    }
}

/* 8003A9BC: Set a sequence's pan, at once or over `frames`. */
void sound_set_seq_pan(SoundSeq *seq, s32 pan, s32 frames) {
    s32 delta;

    seq->pan_target = pan << 8;
    if (frames == 0) {
        seq->pan.value = pan << 24;
        seq->pan_frames = 0;
        sound_request_seq_channel_updates(0x100, seq);
        return;
    }
    delta = (pan << 16) - (seq->pan.value >> 8);
    if (delta != 0) {
        seq->pan_frames = frames;
        seq->pan_step = (delta / frames) << 8;
    }
}

void sound_request_seq_voice_updates(SoundSeq *seq, s32 voices);
void sound_request_seq_key_on(SoundSeq *seq);

/* 8003AA30: Resume a sequence: apply its reverb (when the driver owns the reverb),
 * refresh all its voices and mark it playing. */
void sound_resume_seq(SoundSeq *seq) {
    DisableEvent(sound_tick_event);
    if (sound_driver_flags & 0x1000) {
        sound_set_reverb(seq->reverb_type, seq->reverb_depth, seq->reverb_delay,
                      seq->reverb_feedback);
    }
    sound_request_seq_voice_updates(seq, 0xFFFF);
    sound_request_seq_key_on(seq);
    seq->flags = (seq->flags & ~0x100) | 0x8000;
    EnableEvent(sound_tick_event);
}

void sound_request_key_off(SoundChannel *state, u32 voice);
void sound_key_on_voice(SoundChannel *state, u32 voice);

/* 8003AAC4: Mute the channels of a sequence whose bit is set in `mask` (keying their
 * voices off while it plays) and unmute the others (keying on the ones
 * still sounding). The unmute test reads the flag word as 32 bits. */
void sound_set_seq_mute_mask(SoundSeq *seq, u32 mask) {
    SoundSeqChannel *channel;
    s32 count;

    if (seq == NULL) {
        return;
    }
    channel = seq->channel;
    count = seq->channels;
    seq->muted = mask;
    do {
        if (channel->flags != 0) {
            if (mask & 1) {
                if (!(channel->flags & 0x20)) {
                    channel->flags |= 0x20;
                    if ((s16)seq->flags & 0x8000) {
                        sound_request_key_off(&channel->state, channel->voice);
                    }
                }
            } else if (channel->flags & 0x20) {
                channel->flags &= ~0x20;
                if ((SEQ_CHANNEL_FLAGS32(channel) & 0x110) == 0x100 &&
                    ((s16)seq->flags & 0x8000)) {
                    sound_key_on_voice(&channel->state, channel->voice);
                }
            }
        }
        channel++;
        count--;
        mask >>= 1;
    } while (count != 0);
}

/* 8003ABE8: Set a sequence's byte 0x1b. */
void sound_set_seq_loop_selector(SoundSeq *seq, u8 value) {
    seq->unk1B = value;
}

/* 8003ABF0: A sequence's position: its first word, then frames, seconds and minutes
 * of its tick counter. */
void sound_get_seq_time(SoundSeq *seq, SoundTime *time) {
    u32 ticks = seq->ticks >> 8;
    u32 seconds = ticks / 240;

    time->unk0 = seq->unk24;
    time->frames = ticks % 240;
    time->seconds = seconds % 60;
    time->minutes = seconds / 60;
}

/* 8003AC58: Store (and return a pointer to) the lowest `unk20` of the active channels
 * of a sequence, 0 when none is active. */
u16 *sound_get_seq_loop_count(SoundSeq *seq) {
    SoundSeqChannel *channel = seq->channel;
    u16 count = seq->channels;
    u16 *out = &seq->unk30;
    u16 lowest = 0xFFFF;

    do {
        if (channel->flags != 0 && channel->unk20 < lowest) {
            lowest = channel->unk20;
        }
        channel++;
    } while (--count != 0);
    if (lowest == 0xFFFF) {
        lowest = 0;
    }
    *out = lowest;
    return out;
}

/* 8003ACC8: The block at header offset `unk1E` of a playing sequence (the first one when
 * `seq` is NULL); NULL when it is not playing. */
u8 *sound_get_seq_header_block(SoundSeq *seq) {
    SoundSeq *it = sound_playing_seq_list;

    if (seq != NULL) {
        while (it != NULL) {
            if (it == seq) {
                break;
            }
            it = it->next;
        }
    }
    if (it == NULL) {
        return NULL;
    }
    return (u8 *)it->header + it->header->unk1E;
}

void sound_discard_seq_snapshot(SoundSeq *seq);
void sound_take_seq_snapshot(SoundSeq *seq);
void sound_restore_seq_snapshot(SoundSeq *seq);

/* 8003AD20: Snapshot operations: 0 discards, 1 takes, 2 restores. */
void sound_run_seq_snapshot_op(SoundSeq *seq, s32 op) {
    switch (op) {
    case 0:
        sound_discard_seq_snapshot(seq);
        break;
    case 1:
        sound_take_seq_snapshot(seq);
        break;
    case 2:
        sound_restore_seq_snapshot(seq);
        break;
    }
}


/* 8003AD98: Discard a sequence's snapshot. */
void sound_discard_seq_snapshot(SoundSeq *seq) {
    if (seq->flags & 0x10) {
        seq->flags &= ~0x10;
        sound_free_seq_snapshots(seq);
    }
}


/* 8003ADCC: Save a copy of a sequence to restart from (reusing an earlier one). The
 * original leaves the copy pointer unset when one already exists. */
void sound_take_seq_snapshot(SoundSeq *seq) {
    s32 size;
    SoundSeq *snapshot;

    DisableEvent(sound_tick_event);
    size = sound_get_seq_size(seq->channels);
    if (seq->snapshot == NULL) {
        snapshot = sound_alloc_memory_low(size);
    }
    if (snapshot == NULL) {
        EnableEvent(sound_tick_event);
        return;
    }
    seq->snapshot = snapshot;
    seq->flags |= 0x10;
    sound_copy_memory(snapshot, seq, size);
    snapshot->next = NULL;
    snapshot->snapshot = NULL;
    seq->unk2C = 0;
    EnableEvent(sound_tick_event);
}

void sound_copy_seq_snapshot(SoundSeq *seq, SoundSeq *snapshot);

/* 8003AE84: Restart a sequence from its snapshot. */
void sound_restore_seq_snapshot(SoundSeq *seq) {
    s32 position;

    if (seq->snapshot != NULL && (seq->flags & 0x10)) {
        DisableEvent(sound_tick_event);
        sound_release_seq_voices(seq);
        position = seq->unk24;
        sound_copy_seq_snapshot(seq, seq->snapshot);
        seq->unk2C = position;
        sound_request_seq_voice_updates(seq, 0xFFFF);
        sound_request_seq_key_on(seq);
        EnableEvent(sound_tick_event);
    }
}

/* 8003AF24: Stop a sequence's voices and reset its tempo slide to 0x7F00, keeping
 * `value` in `unk1E`. */
void sound_skip_seq_to_bar(SoundSeq *seq, u16 value) {
    s32 tempo = 0x7F00;

    seq->unk1E = value;
    seq->flags |= 0x20;
    DisableEvent(sound_tick_event);
    sound_release_seq_voices(seq);
    EnableEvent(sound_tick_event);
    seq->tempo_target = tempo;
    seq->tempo_frames = 0;
    seq->tempo.value = tempo << 16;
    seq->tick_step = seq->rate.part.whole * tempo;
}

/* 8003AFA0: Request key-on for the unmuted channels that hold a sounding note. */
void sound_request_seq_key_on(SoundSeq *seq) {
    s32 count = seq->channels;
    SoundSeqChannel *channel = seq->channel;

    do {
        count--;
        if ((SEQ_CHANNEL_FLAGS32(channel) & 0x101) == 0x101 && !(channel->flags & 0x30)) {
            channel->flags2 |= 1;
        }
        channel++;
    } while (count != 0);
}

void sound_request_fast_key_off(SoundChannel *state, u32 voice);

/* 8003AFFC: Apply 8003e8a4 to the voice of every active channel of a sequence. */
void sound_fast_key_off_seq_voices(SoundSeq *seq) {
    s32 count = seq->channels;
    SoundSeqChannel *channel = seq->channel;

    do {
        if (channel->flags != 0) {
            sound_request_fast_key_off(&channel->state, channel->voice);
        }
        channel++;
        count--;
    } while (count != 0);
}

/* 8003B060: Release the voices of every channel of a sequence. */
void sound_release_seq_voices(SoundSeq *seq) {
    SoundSeqChannel *channel = seq->channel;
    s32 count = seq->channels;

    do {
        sound_release_voice(&channel->state, channel->voice);
        channel++;
        count--;
    } while (count != 0);
}

/* 8003B0AC: Fill a sequence's table (placed after its channels) from the 5-byte
 * (index, little-endian word) entries of its header. */
void sound_load_seq_table(SoundSeq *seq, SoundSeqHeader *header) {
    u32 *table = (u32 *)((u8 *)seq + sound_get_seq_size(header->channels));
    s32 count;
    u8 *entry;

    seq->table = table;
    count = header->entries;
    entry = (u8 *)header + header->table;
    do {
        u32 value = entry[1] | (entry[2] << 8) | (entry[3] << 16) | (entry[4] << 24);
        u32 *slot = &table[entry[0]];

        *slot = value;
        entry += 5;
        count--;
    } while (count != 0);
}

void sound_init_effect_channels(SoundSeq *seq);

/* 8003B148: Create the sound effect channel set with `count` (made even) channels on
 * the top hardware voices. */
SoundSeq *sound_create_effect_channels(s32 count) {
    SoundSeq *effects;
    SoundSeqChannel *channel;
    s32 i;
    s32 index;
    s32 voice;

    count &= ~1;
    sound_effect_channel_count = count;
    effects = sound_alloc_memory_low(sound_get_seq_size(count));
    if (effects == NULL) {
        sound_report_error(0x1E);
        return NULL;
    }
    sound_init_effect_channels(effects);
    channel = effects->channel;
    voice = 24 - count;
    i = count;
    index = 0;
    do {
        channel->flags = 0;
        channel->voice_bit = index++;
        channel->voice = voice;
        channel++;
        i--;
        voice++;
    } while (i != 0);
    sound_link_seq(effects);
    return effects;
}


/* 8003B1FC: Unlink a sequence from the playing list (stopping it) and release it. */
void sound_destroy_seq(SoundSeq *seq) {
    sound_unlink_seq(seq);
    sound_free_memory(seq);
}

void sound_reset_seq_playback(SoundSeq *seq);

/* 8003B22C: Read a sequence's header: channel count, reverb settings (applied when
 * the driver owns the reverb), then reset its playback state. */
void sound_read_seq_header(SoundSeq *seq) {
    SoundSeqHeader *header = seq->header;

    if (sound_check_seq_header(header) != 0) {
        sound_report_error(0xA);
        return;
    }
    seq->flags |= 1;
    seq->unk12 = header->unk10;
    seq->channels = header->channels;
    seq->unk16 = header->unk16;
    seq->unk18 = header->unk18;
    seq->reverb_type = header->reverb_type;
    seq->reverb_depth = header->reverb_depth << 8;
    seq->reverb_delay = header->reverb_delay;
    seq->reverb_feedback = header->reverb_feedback;
    if (sound_driver_flags & 0x1000) {
        sound_set_reverb((s8)seq->reverb_type, seq->reverb_depth, seq->reverb_delay,
                      seq->reverb_feedback);
    }
    sound_reset_seq_playback(seq);
}

/* 8003B32C: Initialise the sound effect channel set. */
void sound_init_effect_channels(SoundSeq *seq) {
    seq->flags = 2;
    seq->unk12 = 0x7FFF;
    seq->unk16 = 0;
    seq->unk18 = 0x7F;
    seq->channels = sound_effect_channel_count;
    sound_reset_seq_playback(seq);
}

/* 8003B370: Reset a sequence's playback state: 4/4 time, tempo 1, rate 0x66, full
 * fade level, no snapshot. */
void sound_reset_seq_playback(SoundSeq *seq) {
    sound_free_seq_snapshots(seq);
    seq->unk1A = 0;
    seq->unk1B = 0;
    seq->unk30 = 0;
    seq->unk32 = 1;
    seq->unk34 = 0;
    seq->unk36 = 1;
    seq->unk38 = 4;
    seq->unk3A = 0x30;
    seq->unk3C = 4;
    seq->unk3E = 4;
    seq->tempo.value = 0x1000000;
    seq->fade.value = 0x7F000000;
    seq->rate.value = 0x660000;
    seq->tick_step = 0x6600;
    seq->ticks = 0;
    seq->unk24 = 0;
    seq->unk20 = 0;
    seq->voices = 0;
    seq->pitch.value = 0;
    seq->pan.value = 0;
    seq->tempo_frames = 0;
    seq->fade_frames = 0;
    seq->pitch_frames = 0;
    seq->pan_frames = 0;
    seq->rate_step = 0;
    seq->rate_frames = 0;
    seq->unk50 = 0x10000;
}

void sound_select_instrument(s16 index, SoundSeqChannel *channel);
void sound_claim_voice(SoundChannel *state, u32 voice);

/* 8003B424: Start the channels of a sequence at the data offsets listed in its
 * header: default note, volume, pan and modulators, muted when the
 * sequence's mute mask says so. The voice index is stored before the
 * channel offset is reread, and the data pointers (start, position and
 * repeat return) are set together. */
void sound_start_seq_channels(SoundSeq *seq) {
    s32 count = seq->channels;
    SoundSeqChannel *channel = seq->channel;
    SoundSeqHeader *header;
    u16 *offset;
    SoundSequence *instruments;
    s32 index;
    s32 voice;
    u32 voices;
    u32 bit;
    s32 i;

    if (count == 0) {
        return;
    }
    index = 0;
    voice = -1;
    voices = 0;
    header = seq->header;
    offset = header->channel;
    instruments = sound_find_wave_bank(seq->unk16);
    if (instruments == NULL) {
        instruments = sound_wave_bank_list;
    }
    do {
        if (*offset != 0) {
            bit = 1 << index;
            voices |= bit;
            if (bit & seq->muted) {
                channel->flags = 0x421;
            } else {
                channel->flags = 0x401;
            }
            if (seq->flags & 4) {
                channel->flags |= 4;
            }
            channel->flags2 = 0x170;
            channel->flags3 = 0;
            channel->id.full = header->unk10;
            channel->priority = 0x10;
            channel->voice_bit = index;
            channel->position = channel->start = (u8 *)header + *offset;
            channel->loop = NULL;
            channel->transpose = 0x3C;
            channel->gate_fraction = 0xF;
            channel->loop_depth = 0xFFFF;
            channel->volume = 0x6000;
            channel->level.value = 0x7F000000;
            channel->unk1C = 0;
            channel->unk20 = 0;
            channel->unk22 = 0;
            channel->unk5C = 0;
            channel->duration_adjust = 0;
            channel->detune = 0;
            channel->previous_note = 0;
            channel->pan = 0x4000;
            channel->unk70 = 0;
            channel->pitch_mod = 0;
            channel->level_mod = 0;
            channel->pan_mod = 0;
            channel->state.unkC = 0;
            channel->state.unkE = 0;
            channel->modulators = 0;
            for (i = 3; i >= 0; i--) {
                channel->modulator[i].flags = 0;
            }
            channel->unk25 = seq->unk16;
            channel->instruments = instruments;
            if (instruments != NULL) {
                sound_select_instrument(0, channel);
            }
            channel->voice = voice;
            channel->state.mode = 0;
            channel->state.priority = 0x100;
            sound_claim_voice(&channel->state, voice);
        } else {
            channel->flags = 0;
        }
        offset++;
        channel++;
        index++;
        count--;
        voice++;
    } while (count != 0);
    seq->voices = voices;
}

/* 8003B644: Start effect `id` (bank in the high half) on the effect channels from
 * index `code & 0xFF` with priority `code >> 8`, at `volume` (scaled by
 * the bank's per-effect volume) and `pan`.
 * The level replaces the volume (16-bit store; clamped when bit 15 is set). */
void sound_start_effect(s16 code, s32 id, s16 volume, s16 pan) {
    SoundBank *bank = sound_effect_bank_list;
    s32 bank_id = id >> 16;
    SoundSeq *effects = sound_effect_channels;
    SoundSequence *instruments;
    u16 *offset;
    SoundSeqChannel *channel;
    s32 count;
    u8 priority;
    s32 i;
    u8 *volumes;

    while (bank->id != bank_id) {
        bank = bank->next;
        if (bank == NULL) {
            return;
        }
    }
    instruments = sound_find_wave_bank(bank->unk16);
    if (instruments == NULL) {
        instruments = sound_wave_bank_list;
    }
    volumes = (u8 *)bank + bank->volumes;
    volume = (u32)(volume * volumes[id & 0xFFFF]) >> 7;
    if ((u16)volume > 0x7FFF) {
        volume = 0x7FFF;
    }
    offset = &bank->effect[(id & 0xFFFF) * 2];
    priority = code >> 8;
    channel = &effects->channel[code & 0xFF];
    count = sound_channels_per_effect;
    DisableEvent(sound_tick_event);
    do {
        channel->id.full = id;
        channel->stamp = sound_tick_count;
        channel->priority = priority;
        if (*offset != 0) {
            effects->voices |= 1 << channel->voice_bit;
            channel->flags = 0x409;
            if (bank->flags & 1) {
                channel->flags = 0x40B;
            }
            channel->flags2 = 0x170;
            channel->flags3 = 0;
            channel->position = channel->start = (u8 *)bank + *offset;
            channel->transpose = 0x3C;
            channel->gate_fraction = 0xF;
            channel->loop_depth = 0xFFFF;
            channel->loop = NULL;
            channel->unk1C = 0;
            channel->unk20 = 0;
            channel->unk22 = 0;
            channel->unk5C = 0;
            channel->duration_adjust = 0;
            channel->detune = 0;
            channel->previous_note = 0;
            channel->volume = volume;
            channel->level.value = 0x7F000000;
            channel->pan = pan;
            channel->unk70 = 0;
            channel->pitch_mod = 0;
            channel->level_mod = 0;
            channel->pan_mod = 0;
            channel->state.unkC = 0;
            channel->state.unkE = 0;
            channel->modulators = 0;
            for (i = 3; i >= 0; i--) {
                channel->modulator[i].flags = 0;
            }
            channel->unk25 = bank->unk16;
            channel->instruments = instruments;
            if (instruments != NULL) {
                sound_select_instrument(0, channel);
            }
            channel->state.mode = 0;
            channel->state.priority = 0x200;
            sound_claim_voice(&channel->state, channel->voice);
        } else {
            effects->voices &= ~(1 << channel->voice_bit);
            channel->flags = 0;
            sound_release_voice(&channel->state, channel->voice);
        }
        offset++;
        channel++;
        count--;
    } while (count != 0);
    effects->flags |= 0x8000;
    EnableEvent(sound_tick_event);
}

/* 8003B930: Free a sequence's snapshot chain. */
void sound_free_seq_snapshots(SoundSeq *seq) {
    SoundSeq *snapshot = seq->snapshot;

    if (snapshot != NULL) {
        seq->snapshot = NULL;
        do {
            seq = snapshot->snapshot;
            sound_free_memory(snapshot);
            snapshot = seq;
        } while (seq != NULL);
    }
}

/* 8003B97C: Copy a snapshot over a sequence, keeping its list and snapshot links. */
void sound_copy_seq_snapshot(SoundSeq *seq, SoundSeq *snapshot) {
    SoundSeq *next = seq->next;
    SoundSeq *saved = seq->snapshot;

    sound_copy_memory(seq, snapshot, sound_get_seq_size(seq->channels));
    seq->next = next;
    seq->snapshot = saved;
}

/* 8003B9E4: Link a sequence at the head of the playing list. */
void sound_link_seq(SoundSeq *seq) {
    DisableEvent(sound_tick_event);
    seq->next = sound_playing_seq_list;
    sound_playing_seq_list = seq;
    EnableEvent(sound_tick_event);
}

/* 8003BA38: Unlink a sequence from the playing list, stopping it first; -1 when it
 * is not listed. */
s32 sound_unlink_seq(SoundSeq *seq) {
    SoundSeq *it = sound_playing_seq_list;
    SoundSeq *prev = NULL;

    while (it != NULL && it != seq) {
        prev = it;
        it = it->next;
    }
    if (it == NULL) {
        sound_report_error(0xF);
        return -1;
    }
    if ((s16)seq->flags & 0x8000) {
        if (seq == NULL) {
            sound_report_error(5);
        } else {
            seq->flags &= 0x7FFF;
            sound_release_seq_voices(seq);
        }
    }
    if (prev != NULL) {
        prev->next = seq->next;
    } else {
        sound_playing_seq_list = seq->next;
    }
    return 0;
}

/* 8003BB08: Set `bits` in every active channel of a sequence. */
void sound_set_seq_channel_flags(s32 bits, SoundSeq *seq) {
    SoundSeqChannel *channel = seq->channel;
    s32 count = seq->channels;

    do {
        count--;
        if (channel->flags != 0) {
            channel->flags = bits | channel->flags;
        }
        channel++;
    } while (count != 0);
}

/* 8003BB40: Byte offset of channel `index` in a sequence. */
s32 sound_get_seq_size(s32 index) {
    return index * sizeof(SoundSeqChannel) + 0x94;
}

void sound_start_next_transfer(void);

/* 8003BB64: SPU transfer completion: run the finished transfer's callback (flagged
 * busy), then start the next queued transfer. */
void sound_complete_transfer(void) {
    void (*callback)(void) = sound_transfer_ring[sound_transfer_ring_read_index].callback;

    sound_driver_flags |= 4;
    if (callback != NULL) {
        callback();
    }
    sound_driver_flags &= ~0x10;
    if (sound_transfer_ring_read_index != sound_transfer_ring_write_index) {
        sound_start_next_transfer();
    }
    sound_driver_flags &= ~4;
}


/* 8003BC10: SPU transfers of each type: 1 write, 2 read, 3 and 4 decoded CD data. */
void sound_queue_spu_write(u32 address, u8 *data, s32 size, void (*callback)(void)) {
    sound_queue_transfer(address, data, size, callback, 1);
}

/* 8003BC34 */
void sound_queue_spu_read(u32 address, u8 *data, s32 size, void (*callback)(void)) {
    sound_queue_transfer(address, data, size, callback, 2);
}

/* 8003BC58 */
void sound_queue_decoded_read(u32 address, u8 *data, s32 size, void (*callback)(void)) {
    sound_queue_transfer(address, data, size, callback, 3);
}

/* 8003BC7C */
void sound_queue_decoded_read_flag5(u32 address, u8 *data, s32 size, void (*callback)(void)) {
    sound_queue_transfer(address, data, size, callback, 4);
}

s32 sound_is_transfer_ring_full(void);

/* 8003BCA0: Queue an SPU transfer of `size` bytes between `data` and SPU address
 * `address` (8-byte aligned), starting it when the SPU is idle. Outside a
 * transfer callback, waits for ring space and runs in a critical section. */
void sound_queue_transfer(u32 address, u8 *data, s32 size, void (*callback)(void), u16 type) {
    u16 flags = sound_driver_flags;
    u16 index;
    SoundTransfer *transfer;

    if (!(flags & 4)) {
        while (sound_is_transfer_ring_full() != 0) {
        }
        EnterCriticalSection();
    }
    index = sound_transfer_ring_write_index + 1;
    if (index >= 8) {
        index = 0;
    }
    sound_transfer_ring_write_index = index;
    transfer = &sound_transfer_ring[index];
    transfer->type = type & 0xF;
    transfer->unk2 = 0;
    transfer->data = data;
    transfer->address = address & 0x7FFF8;
    transfer->size = size;
    transfer->callback = callback;
    if (!(sound_driver_flags & 0x10)) {
        sound_start_next_transfer();
    }
    if (!(flags & 4)) {
        ExitCriticalSection();
    }
}

/* 8003BDBC: Whether the eight-entry command ring has at least six entries queued. */
s32 sound_is_transfer_ring_full(void) {
    u16 write = sound_transfer_ring_write_index;

    if (write < sound_transfer_ring_read_index) {
        write += 8;
    }
    return write - sound_transfer_ring_read_index >= 6;
}

/* 8003BDF4: An empty driver entry. */
void sound_empty_entry_after_transfer_queue(void) {
}

/* 8003BDFC: The type of the transfer in progress (0 when idle), after waiting for it
 * to finish when `wait` has bit 4. */
s32 sound_sync_transfer(s32 wait) {
    if (wait & 0x10) {
    busy:
        if (sound_driver_flags & 0x10) {
            goto busy;
        }
    }
    if (sound_driver_flags & 0x10) {
        return (s16)sound_transfer_ring[sound_transfer_ring_read_index].type;
    }
    return 0;
}

/* 8003BE68: Start the next queued SPU transfer; its completion callback continues
 * the ring. */
void sound_start_next_transfer(void) {
    u16 index = sound_transfer_ring_read_index + 1;
    SoundTransfer *transfer;
    SpuTransferCallbackProc previous;

    if (index >= 8) {
        index = 0;
    }
    sound_transfer_ring_read_index = index;
    sound_driver_flags |= 0x10;
    transfer = &sound_transfer_ring[index];
    previous = SpuSetTransferCallback(sound_complete_transfer);
    SpuSetTransferMode(0);
    SpuSetTransferStartAddr(transfer->address);
    switch (transfer->type) {
    case 0:
        break;
    case 1:
        SpuWrite(transfer->data, transfer->size);
        break;
    case 2:
        SpuRead(transfer->data, transfer->size);
        break;
    case 3:
        sound_unread_decoded_read_result = SpuReadDecodedData(transfer->data, 0);
        break;
    case 4:
        sound_unread_decoded_read_result = SpuReadDecodedData(transfer->data, 5);
        break;
    }
    if (previous != sound_complete_transfer) {
        sound_report_error(0x26);
    }
}

/* 8003BFA0: SPU interrupt callback (80037b88 installs it): count the interrupt and run
 * the hook, flagged busy. */
void sound_dispatch_spu_irq(void) {
    sound_driver_flags |= 4;
    sound_unread_spu_irq_count++;
    if (sound_spu_irq_hook != NULL) {
        sound_spu_irq_hook();
    }
    sound_driver_flags &= ~4;
}

/* 8003C010: Install the SPU interrupt hook 8003bfa0 runs. */
void sound_set_spu_irq_hook(void (*callback)(void)) {
    sound_spu_irq_hook = callback;
}

void sound_step_slide(SoundSlide *slide);
void sound_step_seq_slides(SoundSeq *seq, SoundSeqChannel *channels, s16 count);
void sound_seq_interpret_channels(SoundSeq *seq, SoundSeqChannel *channels, s16 count);
void sound_flush_voice_registers(void);
void sound_key_off_voices(void);
void sound_stage_seq_voices(SoundSeq *seq, SoundSeqChannel *channels, s16 count);
void sound_run_seq_modulators(SoundSeq *seq, SoundSeqChannel *channels, s16 count);

/* 8003C020: The sound driver tick. Every other tick it steps the master and CD
 * volume fades; the master volume goes through 80038e6c, which gives it
 * the Wide mode's inverted right channel at every step. Then it writes
 * the staged voice registers, advances every playing sequence (tempo,
 * fade, pitch and pan slides, beats, channel data) and stages the next
 * voice registers (modulators, then volumes and pitches, where the
 * Mono/Stereo pan law applies). Each slide's new value is read through a
 * copy of its 16.16 value. */
s32 sound_run_tick(void) {
    u32 start;
    u32 end;
    SoundSeq *seq;
    s16 count;
    SoundSeqChannel *channels;
    s16 volume;
    Fixed value;

    if (sound_driver_flags & 0x40) {
        return 0;
    }
    start = GetRCnt(0xF2000002);
    if (sound_tick_count++ & 1) {
        if (sound_volumes.master_slide.frames != 0) {
            sound_step_slide(&sound_volumes.master_slide);
            value = sound_volumes.master_slide.value;
            volume = value.part.whole;
            sound_volumes.master = volume;
            sound_set_stereo_volume(volume, &sound_volumes.attr.mvol, 0);
            sound_volumes.attr.mask |= 3;
        }
        if (sound_volumes.cd_slide.frames != 0) {
            sound_step_slide(&sound_volumes.cd_slide);
            value = sound_volumes.cd_slide.value;
            volume = value.part.whole;
            sound_volumes.attr.cd.volume.left = sound_volumes.attr.cd.volume.right = sound_volumes.cd = volume;
            sound_volumes.attr.mask |= 0xC0;
        }
        if (sound_volumes.attr.mask != 0) {
            SpuSetCommonAttr(&sound_volumes.attr);
            sound_volumes.attr.mask = 0;
        }
    }
    sound_flush_voice_registers();
    for (seq = sound_playing_seq_list; seq != NULL; seq = seq->next) {
        if ((s16)seq->flags >= 0) {
            continue;
        }
        if (seq->unk2C != 0 && seq->unk24 >= seq->unk2C) {
            sound_restore_seq_snapshot(seq);
        }
        if (seq->tempo_frames != 0) {
            sound_step_slide((SoundSlide *)&seq->tempo);
            seq->tick_step = seq->rate.part.whole * seq->tempo.part.whole;
        }
        if (seq->fade_frames != 0) {
            sound_step_slide((SoundSlide *)&seq->fade);
            sound_request_seq_channel_updates(0x100, seq);
        }
        if (seq->pitch_frames != 0) {
            sound_step_slide((SoundSlide *)&seq->pitch);
            sound_request_seq_channel_updates(0x200, seq);
        }
        if (seq->pan_frames != 0) {
            sound_step_slide((SoundSlide *)&seq->pan);
            sound_request_seq_channel_updates(0x100, seq);
        }
        seq->unk20++;
        seq->ticks += seq->tempo.part.whole;
        seq->unk50 -= seq->tick_step;
        while (seq->unk50 < 0) {
            seq->unk50 += 0x10000;
            if (--seq->unk36 == 0) {
                seq->unk36 = seq->unk3A;
                if (++seq->unk34 > seq->unk38) {
                    seq->unk34 = 1;
                    seq->unk32++;
                }
            }
            count = seq->channels;
            channels = seq->channel;
            if (count != 0) {
                sound_step_seq_slides(seq, channels, count);
                sound_seq_interpret_channels(seq, channels, count);
            }
            if (seq->voices != 0) {
                seq->unk24++;
                if (seq->fade.value == 0) {
                    sound_stop_seq(seq);
                    seq->flags |= 0x100;
                }
                if (seq->unk32 == seq->unk1E) {
                    seq->flags &= ~0x20;
                    sound_set_seq_tempo(seq, 0, 0);
                    seq->unk1E = 0;
                }
            } else {
                seq->flags &= 0x7FFF;
                break;
            }
        }
    }
    for (seq = sound_playing_seq_list; seq != NULL; seq = seq->next) {
        if ((s16)seq->flags < 0) {
            count = seq->channels;
            channels = seq->channel;
            if (count != 0) {
                sound_run_seq_modulators(seq, channels, count);
                sound_stage_seq_voices(seq, channels, count);
            }
        }
    }
    sound_key_off_voices();
    if (sound_pending_irq_enable & 1) {
        sound_pending_irq_enable &= ~1;
        SpuSetIRQ(1);
    }
    end = GetRCnt(0xF2000002);
    if (end >= start) {
        sound_unread_tick_time_total += end - start;
        sound_unread_timed_tick_count++;
    }
    return 0;
}

/* 8003C484: Step a linear slide; on its last frame land exactly on the target. */
void sound_step_slide(SoundSlide *slide) {
    if (--slide->frames != 0) {
        slide->value.value += slide->step;
    } else {
        slide->value.value = slide->target << 16;
    }
}

/* 8003C4C4: Advance a sequence's rate slide and `count` channels' slides (note,
 * level, pan and volume) and timers by one tick. One tick before the
 * next note, change the release rate when flagged; when the gate timer
 * expires, request key off and remember it for the next note. */
void sound_step_seq_slides(SoundSeq *seq, SoundSeqChannel *channel, s16 count) {
    u16 frames;
    u16 flags;
    u16 flags2;
    u16 flags3;
    u32 timers;
    u32 note_ticks;
    u32 gate_ticks;

    frames = seq->rate_frames;
    if (frames != 0) {
        frames--;
        if (frames != 0) {
            seq->rate.value += seq->rate_step;
        } else {
            seq->rate.value = seq->rate_target << 16;
        }
        seq->rate_frames = frames;
        seq->tick_step = seq->rate.part.whole * seq->tempo.part.whole;
    }
    do {
        flags = channel->flags;
        if (flags != 0) {
            timers = *(u32 *)&channel->unk5C;
            flags2 = channel->flags2;
            note_ticks = timers & 0xFFFF;
            gate_ticks = timers >> 16;
            if (note_ticks != 0) {
                flags3 = channel->flags3;
                if (flags3 & 8) {
                    flags2 |= 0x100;
                    if (--channel->unk96 == 0) {
                        flags3 &= ~8;
                    }
                    channel->level.value += channel->unk88;
                }
                if (flags3 & 1) {
                    flags2 |= 0x200;
                    if (!(flags3 & 2)) {
                        if (--channel->unk94 == 0) {
                            flags3 &= ~1;
                        }
                    }
                    channel->note.value += channel->unk84;
                }
                if (flags3 & 0x10) {
                    if (--channel->pan_frames == 0) {
                        channel->pan = channel->pan_target;
                        flags3 &= ~0x10;
                    } else {
                        channel->pan += channel->pan_step;
                    }
                    flags2 |= 0x100;
                }
                if (flags3 & 0x20) {
                    if (--channel->volume_frames == 0) {
                        channel->volume = channel->volume_target;
                        flags3 &= ~0x20;
                    } else {
                        channel->volume += channel->volume_step;
                    }
                    flags2 |= 0x100;
                }
                channel->flags3 = flags3;
                note_ticks--;
                gate_ticks--;
                if (note_ticks == 1 && (flags & 0x1000)) {
                    channel->state.envelope.release_rate = 6;
                    channel->state.flags |= 0x80;
                }
                if (gate_ticks == 0) {
                    channel->flags |= 0x400;
                    flags2 |= 2;
                }
                *(u32 *)&channel->unk5C = note_ticks + (gate_ticks << 16);
            }
            channel->flags2 = flags2;
        }
        channel++;
    } while (--count != 0);
}

void sound_seq_apply_table_entry(SoundSeq *seq, SoundSeqChannel *channel, s32 index);

/* 8003C6E8: Decode channels whose note timer expired. Bytes below 0x80 give a volume
 * followed by an encoded note/duration; the other bytes dispatch sequence
 * opcodes. Look ahead through opcodes and repeats to decide whether to
 * release the note, without executing that future data or advancing the
 * saved channel position. A new note also restarts portamento, the level
 * sweep and the modulators marked for a per-note restart.
 *
 * The two tick counters share a word. Keep the original signed addition
 * when packing it: the low half is not masked before adding the high half.
 * One 16-bit variable holds the encoded note byte, then the note's duration
 * (the original copies the byte for the two table indexes). The duration
 * bias is stored before the duration is extended; the extension rereads
 * unk5C. */
void sound_seq_interpret_channels(SoundSeq *seq, SoundSeqChannel *channels, s16 count) {
    s16 remaining_channels;
    SoundSeqChannel *channel;
    u8 *position;
    SoundLoop *loop;
    SoundModulator *modulator;
    u16 previous_flags;
    u16 modulator_flags;
    s16 opcode;
    s16 duration;
    u16 gate;
    u16 fraction;
    s32 note;
    s32 new_note;
    s32 distance;
    s32 remaining;

    remaining_channels = count;
    channel = channels;
    do {
        if (channel->flags != 0 && (u16)channel->unk5C == 0) {
            new_note = 0;
            previous_flags = channel->flags;
            position = channel->position;
            channel->flags = previous_flags & ~0x700;
            while (1) {
                opcode = *position++;
                if (opcode < 0x80) {
                    if (!(channel->flags & 8)) {
                        channel->volume = opcode << 8;
                    }
                    channel->flags2 |= 0x100;
                    duration = *position++;
                    note = channel->current_note = (u8)channel->transpose + sound_note_semitones[duration];
                    duration = sound_note_durations[duration];
                    if (duration == 0) {
                        duration = *position++;
                    }
                    channel->unk5C = duration;
                    channel->state.envelope.release_rate = channel->unk28;
                    channel->state.flags |= 0x80;
                    if (channel->flags & 0x10) {
                        sound_seq_apply_table_entry(seq, channel, note);
                    } else {
                        channel->note.value = ((note << 8) + channel->detune + channel->unk6C) << 16;
                    }
                    channel->flags2 |= 0x200;
                    channel->flags |= 0x180;
                    new_note = 1;
                    if (previous_flags & 0x400) {
                        channel->flags2 |= 1;
                    }
                    if (channel->flags & 0x8000) {
                        channel->flags &= ~0x8000;
                        channel->state.flags = 0xFFFF;
                        channel->flags2 |= 0x300;
                    }
                } else {
                    position = sound_seq_opcode_handlers[(s16)(opcode - 0x80)](position, seq, channel);
                    if (channel->flags == 0) {
                        seq->voices &= ~(1 << channel->voice_bit);
                        break;
                    }
                }
                if (channel->flags & 0x500) {
                    break;
                }
            }
            channel->position = position;
            if (channel->flags == 0) {
                channel++;
                continue;
            }
            if (channel->flags & 0x800) {
                channel->flags |= 0x200;
            }
            loop = &channel->loops[channel->loop_depth];
            while ((opcode = *position) >= 0x80) {
                if (opcode == 0x90) {
                    position = channel->loop;
                    if (position != NULL) {
                        continue;
                    }
                    break;
                }
                if (opcode == 0x80) {
                    channel->flags &= ~0x200;
                    break;
                }
                if (opcode == 0x81) {
                    channel->flags |= 0x200;
                    break;
                }
                if (opcode == 0xB0 || opcode == 0xB1) {
                    channel->flags &= ~0x200;
                    break;
                }
                if (opcode == 0x99) {
                    if (loop->count != 0) {
                        position = loop->start;
                        continue;
                    }
                    loop--;
                }
                if (opcode == 0x9A && loop->count == 0) {
                    position = loop->end;
                    loop--;
                } else {
                    position += sound_seq_opcode_lengths[(s16)(opcode - 0x80)];
                }
            }
            if (opcode < 0x80) {
                channel->flags |= 0x1000;
            } else {
                channel->flags &= ~0x1000;
            }
            duration = (s8)channel->duration_adjust + channel->unk5C;
            if (duration <= 0) {
                channel->duration_adjust += channel->unk5C;
                duration += channel->unk5C;
            }
            gate = 0x7FFF;
            if (!(channel->flags & 0x600)) {
                fraction = channel->gate_fraction;
                switch (fraction) {
                case 16:
                    gate = duration;
                    break;
                case 15:
                    gate = duration - 1;
                    if (gate == 0) {
                        gate = 1;
                    }
                    break;
                default:
                    gate = (u32)(duration * fraction) >> 4;
                    if (gate == 0) {
                        gate = 1;
                    }
                    break;
                }
            }
            *(s32 *)&channel->unk5C = duration + (gate << 16);
            if (new_note) {
                if (channel->flags3 & 4) {
                    distance = (channel->current_note - channel->previous_note) << 24;
                    if (distance != 0) {
                        channel->unk84 = distance / channel->unk70;
                        channel->unk94 = channel->unk70;
                        channel->flags3 |= 1;
                        channel->note.value = ((channel->previous_note << 8) + channel->detune + channel->unk6C) << 16;
                    }
                }
                channel->previous_note = channel->current_note;
                if (channel->flags3 & 0x100) {
                    channel->unk96 = channel->unk80;
                    channel->unk88 = channel->unk7C;
                    channel->level.value = (u16)channel->unk82 << 16;
                    channel->flags3 |= 8;
                }
                remaining = 4;
                modulator = channel->modulator;
                do {
                    modulator_flags = modulator->flags;
                    if ((modulator_flags & 3) == 3) {
                        modulator->phase = 0;
                        modulator->count = 1;
                        modulator->delay_count = modulator->delay;
                        modulator->period_count = modulator->period;
                        channel->flags2 |= 0x100;
                        modulator->flags = modulator_flags & ~0xC;
                    }
                    modulator++;
                } while (--remaining != 0);
            }
        }
        channel++;
    } while (--remaining_channels != 0);
}

/* 8003CC84: Apply entry `index` of the sequence's table to a channel: instrument,
 * note offset and pan. */
void sound_seq_apply_table_entry(SoundSeq *seq, SoundSeqChannel *channel, s32 index) {
    u8 *entry = (u8 *)&seq->table[index & 0xFF];

    u8 pan;

    sound_select_instrument(entry[0], channel);
    channel->note.value = ((entry[1] << 8) + channel->detune + channel->unk6C) << 16;
    pan = entry[3];
    channel->flags2 |= 0x100;
    channel->pan = pan << 8;
}

/* Sequence opcode handlers: each takes the opcode's operands, the sequence
 * and the channel, and returns the position after the operands. Each comment
 * gives the opcode (sound_seq_opcode_handlers[op - 0x80]), its operands and its effect;
 * tools/analysis/sound_sequence.py decodes the sequences with them. */

/* 8003CD00: The 31 unused opcode slots: no operands, no effect. sound_seq_opcode_lengths gives them
 * length 0, so the look-ahead of 8003c6e8 would never step past one. */
u8 *sound_seq_unused_opcode(u8 *data) {
    return data;
}

/* 8003CD08: 80 rest(u8 ticks): wait `ticks` with the key released (unk5C; flags 0x400
 * and flags2 2, the key-off request of an expired gate). */
u8 *sound_seq_rest(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->unk5C = *data++;
    channel->flags |= 0x400;
    channel->flags2 |= 2;
    return data;
}

/* 8003CD30: 81 tie(u8 ticks): wait `ticks` holding the note (unk5C, flags 0x100). */
u8 *sound_seq_tie(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->flags |= 0x100;
    channel->unk5C = value;
    return data;
}

/* 8003CD4C: 8A nop_8a(): no effect. */
u8 *sound_seq_nop_8a(u8 *data) {
    return data;
}

/* 8003CD54: 8D loop_point_if(u8 selector): mark the loop point after the operand (and
 * the transpose) when `selector` equals sequence byte 0x1B (set by
 * 8003abe8). */
u8 *sound_seq_loop_point_if(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    if (*data++ == seq->unk1B) {
        channel->loop = data;
        channel->unk23 = channel->transpose;
    }
    return data;
}

/* 8003CD7C: 8E skip3_8e(unused byte, unused byte, unused byte): skip three operand
 * bytes; no effect. */
u8 *sound_seq_skip3_8e(u8 *data) {
    return data + 3;
}

/* 8003CD84: 8F nop_8f(): no effect. */
u8 *sound_seq_nop_8f(u8 *data) {
    return data;
}

/* 8003CD8C: 90 end(): continue at the loop point (counting the pass, restoring its
 * transpose); without one release the voice and stop the channel. */
u8 *sound_seq_end(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    if (channel->loop != NULL) {
        data = channel->loop;
        channel->unk20++;
        channel->transpose = channel->unk23;
    } else {
        channel->flags2 &= ~3;
        sound_release_voice(&channel->state, channel->voice);
        channel->flags = 0;
    }
    return data;
}

/* 8003CE04: 91 loop_point(): mark the loop point here and save the transpose. */
u8 *sound_seq_loop_point(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->loop = data;
    channel->unk23 = channel->transpose;
    return data;
}

/* 8003CE18: 94 octave(u8 octave): transpose = octave * 12. */
u8 *sound_seq_octave(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose = *data * 12;
    return data + 1;
}

/* 8003CE38: 95 octave_up(): transpose += 12. */
u8 *sound_seq_octave_up(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose += 12;
    return data;
}

/* 8003CE50: 96 octave_down(): transpose -= 12. */
u8 *sound_seq_octave_down(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->transpose -= 12;
    return data;
}

/* 8003CE68: 97 time_signature(u8 beats, u8 unit): `beats` beats per bar of 0xC0 / unit
 * ticks each (the bar and beat counters 8003c020 advances); restart the
 * current beat's ticks. */
u8 *sound_seq_time_signature(u8 *data, SoundSeq *seq) {
    s16 beats = data[0];
    s16 unit = data[1];

    seq->unk3A = 0xC0 / unit;
    seq->unk3C = unit;
    seq->unk38 = beats;
    seq->unk3E = beats;
    seq->unk36 = seq->unk3A;
    return data + 2;
}

/* 8003CE9C: F9 position(u8 bar, u8 beat): set the bar and beat counters and restart
 * the beat's ticks. */
u8 *sound_seq_position(u8 *data, SoundSeq *seq) {
    u16 length;

    seq->unk32 = data[0];
    length = seq->unk3A;
    seq->unk34 = data[1];
    seq->unk36 = length;
    return data + 2;
}

/* 8003CEC0: A4 set_1a(u8 value): sequence byte 0x1A = value (no recovered code reads
 * it). */
u8 *sound_seq_set_1a(u8 *data, SoundSeq *seq) {
    seq->unk1A = *data;
    return data + 1;
}

/* 8003CED4: A5 add_1a(u8 value): sequence byte 0x1A += value. */
u8 *sound_seq_add_1a(u8 *data, SoundSeq *seq) {
    seq->unk1A += *data;
    return data + 1;
}

/* 8003CEF0: 98 repeat(u8 count): open a repeat of `count` passes one level deeper,
 * saving its start and transpose. */
u8 *sound_seq_repeat(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    SoundLoop *loop;

    channel->loop_depth++;
    loop = &channel->loops[channel->loop_depth];
    loop->count = *data++ - 1;
    loop->start = data;
    loop->transpose = channel->transpose;
    return data;
}

/* 8003CF38: 99 repeat_end(): while passes remain go back to the repeat start
 * (recording this end, restoring the start transpose); else close the
 * repeat. */
u8 *sound_seq_repeat_end(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    SoundLoop *loop = &channel->loops[channel->loop_depth];

    if (--loop->count != 0xFF) {
        loop->end = data;
        data = loop->start;
        loop->exit_transpose = channel->transpose;
        channel->transpose = loop->transpose;
    } else {
        channel->loop_depth--;
    }
    return data;
}

/* 8003CFA4: 9A repeat_break(): on the last pass jump to the recorded repeat end and
 * close the repeat. */
u8 *sound_seq_repeat_break(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    SoundLoop *loop = &channel->loops[channel->loop_depth];

    if (loop->count == 0) {
        data = loop->end;
        channel->transpose = loop->exit_transpose;
        channel->loop_depth--;
    }
    return data;
}

/* 8003CFF0: 9C play_effect(u16 effect, unused byte): play effect `effect` of bank 0 at
 * volume 0x7F, pan 0x40 (80039f18). */
u8 *sound_seq_play_effect(u8 *data) {
    sound_play_effect_volume_pan(data[0] | (data[1] << 8), 0x7F, 0x40);
    return data + 3;
}

/* 8003D034: 9D stop_effect(u16 effect): stop the effect channels playing id `effect`
 * (8003a14c). The look-ahead (sound_seq_opcode_lengths) steps 4 bytes over it. */
u8 *sound_seq_stop_effect(u8 *data) {
    sound_stop_effect(data[0] | (data[1] << 8));
    return data + 2;
}

/* 8003D070: 9E goto_effect(u16 effect, u8 part): continue three bytes into channel
 * `part` of effect `effect` of the channel's bank (the first bank when it
 * has none); with no loaded bank of that id it returns the operand pointer,
 * so the operands are executed as opcodes. */
u8 *sound_seq_goto_effect(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    SoundBank *bank = sound_effect_bank_list;
    s16 effect = data[0] | (data[1] << 8);
    s16 id = channel->id.part.bank;
    u8 part = data[2];

    if (id != 0) {
        while (bank->id != id) {
            bank = bank->next;
            if (bank == NULL) {
                return data;
            }
        }
    }
    data = (u8 *)bank + bank->effect[effect * 2 + part];
    return data + 3;
}

/* 8003D0E8: A0 rate(u8 rate): tick rate = rate; ticks per frame = rate * tempo. */
u8 *sound_seq_rate(u8 *data, SoundSeq *seq) {
    u8 rate = *data++;
    seq->rate.value = rate << 16;
    seq->tick_step = rate * seq->tempo.part.whole;
    return data;
}

/* 8003D110: A1 rate_add(s8 delta): tick rate += delta; zero the ticks per frame until
 * a rate or tempo opcode or slide recomputes them. */
u8 *sound_seq_rate_add(s8 *data, SoundSeq *seq) {
    u8 reserved[4]; /* Retain the original unused eight-byte stack frame. */
    s32 rate = *data;
    s32 old_rate = seq->rate.value;

    seq->tick_step = 0;
    seq->rate.value = (rate << 16) + old_rate;
    return data + 1;
}

/* 8003D13C: A2 rate_slide(u8 frames, u8 target): slide the tick rate to `target` over
 * `frames`. */
u8 *sound_seq_rate_slide(u8 *data, SoundSeq *seq) {
    s16 frames = data[0];
    u8 target = data[1];
    s32 delta;

    seq->rate_target = target;
    delta = (target << 16) - seq->rate.value;
    if (frames != 0 && delta != 0) {
        seq->rate_frames = frames;
        seq->rate_step = delta / frames;
    }
    return data + 2;
}

/* 8003D17C: A6 fade_level(u8 level): sequence level = level << 24; flag every
 * channel's volume. */
u8 *sound_seq_fade_level(u8 *data, SoundSeq *seq) {
    seq->fade.value = *data++ << 24;
    sound_request_seq_channel_updates(0x100, seq);
    return data;
}

/* 8003D1BC: A7 fade_slide(u8 units, u8 target): slide the sequence level to `target`
 * over units * 32 frames. */
u8 *sound_seq_fade_slide(u8 *data, SoundSeq *seq) {
    s16 frames = data[0] << 5;
    u8 target = data[1];
    s32 delta = (target << 24) - seq->fade.value;

    if (frames != 0 && delta != 0) {
        seq->fade_frames = frames;
        seq->fade_target = target << 8;
        seq->fade_step = delta / frames;
    }
    return data + 2;
}

/* 8003D208: A9 gate(u8 sixteenths): release point of each note in sixteenths of its
 * ticks (15: ticks - 1, 16: all). */
u8 *sound_seq_gate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->gate_fraction = *data;
    return data + 1;
}

/* 8003D21C: AA voice(u8 voice): move the channel to hardware voice `voice` (< 25). */
u8 *sound_seq_voice(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s32 voice = *data++;

    if (voice < 25) {
        sound_release_voice(&channel->state, channel->voice);
        sound_claim_voice(&channel->state, channel->voice = voice);
    }
    return data;
}

/* 8003D298: AC instrument(u8 instrument): select an instrument of the wave bank
 * (8003e5bc). */
u8 *sound_seq_instrument(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    sound_select_instrument(*data++, channel);
    return data;
}

/* 8003D2D0: AD duration_adjust(u8 ticks): add to the signed note-length bias (0 resets
 * it). */
u8 *sound_seq_duration_adjust(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    if (value != 0) {
        channel->duration_adjust += value;
    } else {
        channel->duration_adjust = 0;
    }
    return data;
}

/* 8003D300: AE drum_on(): notes select entries of the sequence table (8003cc84), when
 * the sequence has one. */
u8 *sound_seq_drum_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    if (seq->table != NULL) {
        channel->flags |= 0x10;
    }
    return data;
}

/* 8003D328: AF drum_off(): notes play the channel's instrument. */
u8 *sound_seq_drum_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->flags &= ~0x10;
    return data;
}

/* 8003D340: B0 legato_on(): channel flag 0x800: notes keep the key held (no gate
 * release). */
u8 *sound_seq_legato_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->flags |= 0x800;
    return data;
}

/* 8003D358: B1 legato_off(): clear channel flag 0x800. */
u8 *sound_seq_legato_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->flags &= ~0x800;
    return data;
}

/* 8003D370: B2 pitch_mod_on(): SPU pitch modulation on (odd hardware voices). */
u8 *sound_seq_pitch_mod_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    if (channel->voice & 1) {
        channel->state.flags |= 0x1000;
        channel->state.mode |= 0x10;
    }
    return data;
}

/* 8003D3A4: B3 pitch_mod_off(): SPU pitch modulation off (odd hardware voices). */
u8 *sound_seq_pitch_mod_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    if (channel->voice & 1) {
        channel->state.flags |= 0x1000;
        channel->state.mode &= ~0x10;
    }
    return data;
}

/* 8003D3D8: B4 noise_clock(u8 clock): noise on at SPU noise clock `clock`. */
u8 *sound_seq_noise_clock(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    seq->noise_clock = *data++;
    SpuSetNoiseClock(seq->noise_clock);
    channel->state.flags |= 0x2000;
    channel->state.mode |= 0x20;
    return data;
}

/* 8003D438: B5 noise_clock_add(u8 delta): noise on, clock += delta (modulo 64). */
u8 *sound_seq_noise_clock_add(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    seq->noise_clock = (*data++ + seq->noise_clock) & 0x3F;
    SpuSetNoiseClock(seq->noise_clock);
    channel->state.flags |= 0x2000;
    channel->state.mode |= 0x20;
    return data;
}

/* 8003D4A4: B6 noise_on(): noise on. */
u8 *sound_seq_noise_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->state.flags |= 0x2000;
    channel->state.mode |= 0x20;
    return data;
}

/* 8003D4C4: B7 noise_off(): noise off. */
u8 *sound_seq_noise_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->state.flags |= 0x2000;
    channel->state.mode &= ~0x20;
    return data;
}

/* 8003D4E4: B8 reverb_settings(u8 depth, s8 delay, s8 feedback): reverb depth (<< 8),
 * delay and feedback through 80038934, keeping the type. */
u8 *sound_seq_reverb_settings(u8 *data, SoundSeq *seq) {
    s16 depth = data[0] << 8;
    s32 delay;
    s32 feedback;

    seq->reverb_depth = depth;
    seq->reverb_delay = delay = ((s8 *)data)[1];
    seq->reverb_feedback = feedback = ((s8 *)data)[2];
    sound_set_reverb(-1, depth, delay, feedback);
    return data + 3;
}

/* 8003D53C: BA reverb_on(): reverb on, unless an effect set's channel stays dry
 * (driver flag 0x2000 clear or channel flag 2). */
u8 *sound_seq_reverb_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    if ((seq->flags & 6) && (!(sound_driver_flags & 0x2000) || (channel->flags & 2))) {
        return data;
    }
    channel->state.flags |= 0x4000;
    channel->state.mode |= 0x40;
    return data;
}

/* 8003D59C: BB reverb_off(): reverb off. */
u8 *sound_seq_reverb_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->state.flags |= 0x4000;
    channel->state.mode &= ~0x40;
    return data;
}

/* 8003D5BC: BC skip3_bc(unused byte, unused byte, unused byte): skip three operand
 * bytes; no effect. */
u8 *sound_seq_skip3_bc(u8 *data) {
    return data + 3;
}

/* 8003D5C4: BD nop_bd(): no effect. */
u8 *sound_seq_nop_bd(u8 *data) {
    return data;
}

/* 8003D5CC: BE nop_be(): no effect. */
u8 *sound_seq_nop_be(u8 *data) {
    return data;
}

/* 8003D5D4: C0 instrument_reload(): reselect the current instrument. */
u8 *sound_seq_instrument_reload(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    sound_select_instrument(channel->instrument, channel);
    return data;
}

/* 8003D60C: C1 envelope_modes(u8 attack, u8 sustain, u8 release): ADSR attack, sustain
 * and release modes. */
u8 *sound_seq_envelope_modes(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value;

    channel->state.envelope.attack_mode = data[0];
    channel->state.envelope.sustain_mode = data[1];
    value = data[2];
    channel->state.flags |= 0x1F0;
    channel->state.envelope.release_mode = value;
    return data + 3;
}

/* 8003D640: C2 attack_rate(u8 rate): ADSR attack rate. */
u8 *sound_seq_attack_rate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->state.flags |= 0x10;
    channel->state.envelope.attack_rate = value;
    return data;
}

/* 8003D65C: C3 decay_rate(u8 rate): ADSR decay rate. */
u8 *sound_seq_decay_rate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->state.flags |= 0x20;
    channel->state.envelope.decay_rate = value;
    return data;
}

/* 8003D678: C4 sustain_rate(u8 rate): ADSR sustain rate. */
u8 *sound_seq_sustain_rate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->state.flags |= 0x40;
    channel->state.envelope.sustain_rate = value;
    return data;
}

/* 8003D694: C5 release_rate(u8 rate): ADSR release rate, also each note's default. */
u8 *sound_seq_release_rate(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data;

    channel->state.flags |= 0x80;
    channel->unk28 = value;
    channel->state.envelope.release_rate = value;
    return data + 1;
}

/* 8003D6B4: C6 sustain_level(u8 level): ADSR sustain level. */
u8 *sound_seq_sustain_level(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->state.flags |= 0x100;
    channel->state.envelope.sustain_level = value;
    return data;
}

/* 8003D6D0: C7 decay_sustain(u8 decay, u8 level): ADSR decay rate and sustain level. */
u8 *sound_seq_decay_sustain(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value;

    channel->state.envelope.decay_rate = data[0];
    value = data[1];
    channel->state.flags |= 0x120;
    channel->state.envelope.sustain_level = value;
    return data + 2;
}

/* 8003D6F8: C8 attack_mode(u8 mode): ADSR attack mode. */
u8 *sound_seq_attack_mode(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->state.flags |= 0x10;
    channel->state.envelope.attack_mode = value;
    return data;
}

/* 8003D714: C9 sustain_mode(u8 mode): ADSR sustain mode. */
u8 *sound_seq_sustain_mode(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->state.flags |= 0x40;
    channel->state.envelope.sustain_mode = value;
    return data;
}

/* 8003D730: CA release_mode(u8 mode): ADSR release mode. */
u8 *sound_seq_release_mode(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->state.flags |= 0x80;
    channel->state.envelope.release_mode = value;
    return data;
}

/* 8003D74C: D0 detune(s8 eighths): detune (1/256 semitones) = eighths << 5. */
u8 *sound_seq_detune(s8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->detune = *data << 5;
    channel->flags2 |= 0x200;
    return data + 1;
}

/* 8003D770: D1 detune_add(s8 eighths): detune += eighths << 5. */
u8 *sound_seq_detune_add(s8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->detune += *data << 5;
    channel->flags2 |= 0x200;
    return data + 1;
}

/* 8003D79C: D2 detune_add_fine(s8 steps): detune += steps << 3. */
u8 *sound_seq_detune_add_fine(s8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->detune += *data << 3;
    channel->flags2 |= 0x200;
    return data + 1;
}

/* 8003D7C8: D3 detune_add_word(s16 big-endian delta): detune += a big-endian signed
 * halfword. */
u8 *sound_seq_detune_add_word(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s32 delta = data[1] + (s16)(data[0] << 8);

    channel->detune += delta;
    channel->flags2 |= 0x200;
    return data + 2;
}

/* 8003D7FC: D4 pitch_slide(u8 frames, s8 semitones): slide the pitch by `semitones`
 * over `frames` (either zero: stop the slide). */
u8 *sound_seq_pitch_slide(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u16 frames = data[0];
    s32 delta = ((s8 *)data)[1] << 24;

    if (frames != 0 && delta != 0) {
        channel->unk94 = frames;
        channel->flags3 |= 1;
        channel->unk84 = delta / frames;
    } else {
        channel->flags3 &= ~1;
    }
    return data + 2;
}

/* 8003D854: D5 pitch_slide_hold(): toggle flags3 bit 1: the pitch slide keeps going
 * without counting frames. */
u8 *sound_seq_pitch_slide_hold(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->flags3 ^= 2;
    return data;
}

/* 8003D86C: DC pitch_slide_off(): stop the pitch slide. */
u8 *sound_seq_pitch_slide_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->flags3 &= ~1;
    return data;
}

/* 8003D884: D6 portamento(u8 frames): each new note slides from the previous one over
 * `frames` (0: off). */
u8 *sound_seq_portamento(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 value = *data++;

    channel->unk70 = value;
    if (value != 0) {
        channel->flags3 |= 4;
    } else {
        channel->flags3 &= ~4;
    }
    return data;
}

/* Declared without a prototype: callers pass the rate as an int. */
s32 sound_compute_modulator_step();
void sound_restart_modulator(SoundModulator *modulator);

/* 8003D8B8: D8 vibrato(u8 rate, s8 depth, u8 delay): pitch modulator on: signed
 * squared depth << 14, rate + rate*rate/64, delay * 4 frames, wave 3
 * (8003f2a0), restarted by each note; nothing when rate or depth is 0. */
u8 *sound_seq_vibrato(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s32 depth = ((s8 *)data)[1];
    s16 rate = data[0];
    SoundModulator *modulator;

    if (depth != 0 && rate != 0) {
        if (depth < 0) {
            depth = depth * -depth;
        } else {
            depth = depth * depth;
        }
        rate += rate * rate / 64;
        modulator = &channel->modulator[0];
        modulator->step = sound_compute_modulator_step(depth << 14, rate, 3);
        modulator->rate = rate;
        modulator->delay = data[2] * 4;
        modulator->period = 0x400;
        modulator->wave = sound_step_triangle_wave;
        modulator->shape = 3;
        modulator->flags = 3;
        modulator->target = 0;
        channel->modulators |= 1;
        sound_restart_modulator(modulator);
    }
    return data + 3;
}


/* 8003D9A4: D9 vibrato_wave(u8 rate, s8 depth, u8 mode): as vibrato without delay,
 * wave mode & 0xF (800508a4); mode bit 4 keeps it running across notes. The
 * mode is a halfword: the masked shape is computed for the call and copied
 * back into it, like the rate. */
u8 *sound_seq_vibrato_wave(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s16 rate = data[0];
    s32 depth = ((s8 *)data)[1];
    s16 mode = data[2];
    SoundModulator *modulator;
    s32 flags;

    if (depth != 0 && rate != 0) {
        if (depth < 0) {
            depth = depth * -depth;
        } else {
            depth = depth * depth;
        }
        depth <<= 14;
        rate += rate * rate / 64;
        flags = ((mode & 0x10) == 0) * 2;
        mode &= 0xF;
        modulator = &channel->modulator[0];
        modulator->step = sound_compute_modulator_step(depth, rate, mode);
        modulator->period = 0x400;
        modulator->rate = rate;
        modulator->delay = 0;
        modulator->wave = sound_modulator_waves[(u16)mode];
        modulator->shape = mode;
        modulator->target = 0;
        modulator->flags = flags + 1;
        channel->modulators |= 1;
        sound_restart_modulator(modulator);
    }
    return data + 3;
}

/* 8003DAB0: D7 vibrato_period(u8 period): fade the pitch modulator in over (period +
 * 1) * 4 frames (step 0x400 / that; 0xFF changes nothing). */
u8 *sound_seq_vibrato_period(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 period = *data++ + 1;

    if (period != 0) {
        channel->modulator[0].period_count = channel->modulator[0].period = 0x400 / (period * 4);
    }
    return data;
}

/* 8003DAEC: DA vibrato_on(): pitch modulator on. */
u8 *sound_seq_vibrato_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->modulators |= 1;
    channel->modulator[0].flags |= 1;
    return data;
}

/* 8003DB0C: DB vibrato_off(): pitch modulator off. */
u8 *sound_seq_vibrato_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->modulators &= ~1;
    channel->modulator[0].flags &= ~1;
    return data;
}

/* 8003DB2C: E0 level(u8 level): channel level = level << 24; stop level slides. */
u8 *sound_seq_level(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->level.value = *data << 24;
    channel->flags2 |= 0x100;
    channel->flags3 &= ~0x108;
    return data + 1;
}

/* 8003DB58: E1 level_add(s8 delta): channel level += delta << 24; stop level slides. */
u8 *sound_seq_level_add(s8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->level.value = ((*data << 24) + channel->level.value) & 0x7FFFFFFF;
    channel->flags2 |= 0x100;
    channel->flags3 &= ~0x108;
    return data + 1;
}

/* 8003DB98: E2 level_slide(u8 frames, s8 target): slide the channel level to `target`
 * over `frames`. */
u8 *sound_seq_level_slide(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u16 frames = data[0];
    s32 delta = (((s8 *)data)[1] << 24) - channel->level.value;

    if (frames != 0 && delta != 0) {
        channel->unk96 = frames;
        channel->flags3 = (channel->flags3 | 8) & ~0x100;
        channel->unk88 = delta / frames;
    }
    return data + 2;
}

/* 8003DBE4: F8 level_sweep(u8 from, u8 frames, u8 to): each note sweeps the channel
 * level from `from` to `to` over `frames` (equal levels or 0 frames: off). */
u8 *sound_seq_level_sweep(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s32 from = data[0] << 24;
    s16 frames = data[1];
    s32 delta = (data[2] << 24) - from;

    if (delta != 0 && frames != 0) {
        channel->unk82 = from >> 16;
        channel->unk80 = frames;
        channel->flags3 = (channel->flags3 | 0x100) & ~8;
        channel->unk7C = delta / frames;
    } else {
        channel->flags3 &= ~0x100;
    }
    return data + 3;
}

/* 8003DC50: E4 tremolo(u8 rate, s8 depth, u8 delay): volume modulator on: depth << 24,
 * rate + rate*rate/64, delay * 4 frames, wave 2 (8003f240), restarted by
 * each note; nothing when rate or depth is 0. */
u8 *sound_seq_tremolo(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s32 depth = ((s8 *)data)[1];
    s16 rate = data[0];
    SoundModulator *modulator;

    if (depth != 0 && rate != 0) {
        rate += rate * rate / 64;
        modulator = &channel->modulator[1];
        modulator->step = sound_compute_modulator_step(depth << 24, rate, 2);
        modulator->rate = rate;
        modulator->delay = data[2] * 4;
        modulator->period = 0x400;
        modulator->wave = sound_step_sawtooth_wave;
        modulator->shape = 2;
        modulator->target = 1;
        modulator->flags = 3;
        channel->modulators |= 2;
        sound_restart_modulator(modulator);
    }
    return data + 3;
}

/* 8003DD24: E5 tremolo_wave(u8 rate, s8 depth, u8 mode): as tremolo without delay,
 * wave mode & 0xF; mode bit 4 keeps it running across notes. */
u8 *sound_seq_tremolo_wave(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s16 rate = data[0];
    s32 depth = ((s8 *)data)[1];
    s16 mode = data[2];
    SoundModulator *modulator;
    s32 flags;

    if (depth != 0 && rate != 0) {
        depth <<= 24;
        rate += rate * rate / 64;
        flags = ((mode & 0x10) == 0) * 2;
        mode &= 0xF;
        modulator = &channel->modulator[1];
        modulator->step = sound_compute_modulator_step(depth, rate, mode);
        modulator->rate = rate;
        modulator->period = 0x400;
        modulator->delay = 0;
        modulator->wave = sound_modulator_waves[(u16)mode];
        modulator->shape = mode;
        modulator->target = 1;
        modulator->flags = flags + 1;
        channel->modulators |= 2;
        sound_restart_modulator(modulator);
    }
    return data + 3;
}

/* 8003DE18: E3 tremolo_period(u8 period): fade the volume modulator in over (period +
 * 1) * 4 frames (step 0x400 / that; 0xFF changes nothing). */
u8 *sound_seq_tremolo_period(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 period = *data++ + 1;

    if (period != 0) {
        channel->modulator[1].period_count = channel->modulator[1].period = 0x400 / (period * 4);
    }
    return data;
}

/* 8003DE54: E6 tremolo_on(): volume modulator on. */
u8 *sound_seq_tremolo_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->modulators |= 2;
    channel->modulator[1].flags |= 1;
    return data;
}

/* 8003DE74: E7 tremolo_off(): volume modulator off. */
u8 *sound_seq_tremolo_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->modulators &= ~2;
    channel->modulator[1].flags &= ~1;
    return data;
}

/* 8003DE94: E8 pan(u8 pan): pan = pan << 8 (0 left, 0x40 centre, 0x7F right). */
u8 *sound_seq_pan(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->pan = *data << 8;
    channel->flags2 |= 0x100;
    return data + 1;
}

/* 8003DEB4: E9 pan_add(s8 delta): pan += delta << 8 (wrapping in 15 bits). */
u8 *sound_seq_pan_add(s8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->pan = (channel->pan + (s16)(*data << 8)) & 0x7FFF;
    channel->flags2 |= 0x100;
    return data + 1;
}

/* 8003DEE4: EA pan_slide(u8 frames, s8 target): step the pan toward `target` over
 * `frames`; the last frame sets it to the stored difference (target - start)
 * << 8. */
u8 *sound_seq_pan_slide(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u16 frames = data[0];
    s32 delta = ((s8 *)data)[1] - (channel->pan >> 8);

    if (frames != 0 && delta != 0) {
        channel->pan_target = delta << 8;
        channel->pan_frames = frames;
        channel->flags3 |= 0x10;
        channel->pan_step = (delta << 8) / frames;
    }
    return data + 2;
}

/* 8003DF3C: EB autopan_period(u8 period): fade the pan modulator in over (period + 1)
 * * 4 frames (step 0x400 / that; 0xFF changes nothing). */
u8 *sound_seq_autopan_period(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 period = *data++ + 1;

    if (period != 0) {
        channel->modulator[2].period_count = channel->modulator[2].period = 0x400 / (period * 4);
    }
    return data;
}

/* 8003DF78: EC autopan(u8 rate, s8 depth, u8 delay): pan modulator on: depth << 24,
 * rate + rate*rate/64, delay * 4 frames, wave 3, restarted by each note;
 * nothing when rate or depth is 0. */
u8 *sound_seq_autopan(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s32 depth = ((s8 *)data)[1];
    s16 rate = data[0];
    SoundModulator *modulator;

    if (depth != 0 && rate != 0) {
        rate += rate * rate / 64;
        modulator = &channel->modulator[2];
        modulator->step = sound_compute_modulator_step(depth << 24, rate, 3);
        modulator->rate = rate;
        modulator->delay = data[2] * 4;
        modulator->period = 0x400;
        modulator->wave = sound_step_triangle_wave;
        modulator->shape = 3;
        modulator->target = 2;
        modulator->flags = 3;
        channel->modulators |= 4;
        sound_restart_modulator(modulator);
    }
    return data + 3;
}

/* 8003E04C: ED autopan_wave(u8 rate, s8 depth, u8 mode): as autopan without delay,
 * wave mode & 0xF; mode bit 4 keeps it running across notes. */
u8 *sound_seq_autopan_wave(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s16 rate = data[0];
    s32 depth = ((s8 *)data)[1];
    s16 mode = data[2];
    SoundModulator *modulator;
    s32 flags;

    if (depth != 0 && rate != 0) {
        depth <<= 24;
        rate += rate * rate / 64;
        flags = ((mode & 0x10) == 0) * 2;
        mode &= 0xF;
        modulator = &channel->modulator[2];
        modulator->step = sound_compute_modulator_step(depth, rate, mode);
        modulator->rate = rate;
        modulator->period = 0x400;
        modulator->delay = 0;
        modulator->wave = sound_modulator_waves[(u16)mode];
        modulator->shape = mode;
        modulator->target = 2;
        modulator->flags = flags + 1;
        channel->modulators |= 4;
        sound_restart_modulator(modulator);
    }
    return data + 3;
}

/* 8003E140: EE autopan_on(): pan modulator on. */
u8 *sound_seq_autopan_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->modulators |= 4;
    channel->modulator[2].flags |= 1;
    return data;
}

/* 8003E160: EF autopan_off(): pan modulator off. */
u8 *sound_seq_autopan_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    channel->modulators &= ~4;
    channel->modulator[2].flags &= ~1;
    return data;
}

/* 8003E180: F0 modulator_select(u8 index, u8 mode, u8 target): select modulator
 * `index` and set its wave (mode & 0xF), target (0 pitch, 1 level, 2 pan),
 * no delay; left off; mode bit 4 keeps it running across notes. */
u8 *sound_seq_modulator_select(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    SoundModulator *modulator;
    u8 mode;
    u8 target;

    channel->modulator_index = data[0];
    mode = data[1];
    modulator = &channel->modulator[channel->modulator_index];
    modulator->shape = mode & 0xF;
    modulator->wave = sound_modulator_waves[modulator->shape];
    if (!(mode & 0x10)) {
        modulator->flags = 2;
    } else {
        modulator->flags = 0;
    }
    target = data[2];
    modulator->period = 0x400;
    modulator->delay = 0;
    modulator->target = target;
    return data + 3;
}

/* 8003E1F8: F1 modulator_depth(u8 rate, s8 depth_high, u8 depth_low): selected
 * modulator: rate + rate*rate/64 and a 16-bit depth. */
u8 *sound_seq_modulator_depth(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s32 square = data[0] * data[0];
    SoundModulator *modulator = &channel->modulator[channel->modulator_index];
    s16 rate = square / 64 + data[0];

    modulator->step = sound_compute_modulator_step((((s8 *)data)[1] << 24) | (data[2] << 16), rate,
                                    modulator->shape);
    modulator->rate = rate;
    return data + 3;
}

/* 8003E290: A modulator's step for a depth: shapes 2-3 cover the depth once per
 * rate, shape 4 once per rate - 1; others use the depth itself. */
s32 sound_compute_modulator_step(depth, rate, shape)
    s32 depth;
    s16 rate;
    s16 shape;
{
    if (depth != 0 && rate != 0) {
        switch (shape) {
        case 2:
        case 3:
            depth /= rate;
            break;
        case 4:
            if (rate != 1) {
                depth /= rate - 1;
            }
            break;
        }
    }
    return depth;
}

/* 8003E308: F2 modulator_timing(u8 delay, u8 period): selected modulator: delay * 4
 * frames, fade-in over (period + 1) * 4 frames (period 0xFF: neither
 * changes). */
u8 *sound_seq_modulator_timing(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    SoundModulator *modulator = &channel->modulator[channel->modulator_index];
    u8 period = data[1] + 1;

    if (period != 0) {
        modulator->delay = data[0] * 4;
        modulator->period_count = modulator->period = 0x400 / (period * 4);
    }
    return data + 2;
}

/* 8003E358: F5 nop_f5(): no effect. The look-ahead (sound_seq_opcode_lengths) steps 2 bytes over it. */
u8 *sound_seq_nop_f5(u8 *data) {
    return data;
}

/* 8003E360: F6 modulator_on(u8 index): restart modulator `index` and switch it on. */
u8 *sound_seq_modulator_on(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    s32 index = *data++;
    SoundModulator *modulator = &channel->modulator[index];

    sound_restart_modulator(modulator);
    modulator->flags |= 1;
    channel->modulators |= 1 << index;
    return data;
}

/* 8003E3E0: Restart a modulator: counters reloaded, phase 0. */
void sound_restart_modulator(SoundModulator *modulator) {
    modulator->count = 1;
    modulator->phase = 0;
    modulator->flags &= ~0xC;
    modulator->delay_count = modulator->delay;
    modulator->period_count = modulator->period;
}

/* 8003E40C: F7 modulator_off(u8 index): switch modulator `index` off. */
u8 *sound_seq_modulator_off(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 index = *data;

    channel->modulator[index].flags &= ~1;
    channel->modulators &= ~(1 << index);
    return data + 1;
}

/* 8003E44C: FC wave_bank_instrument(u8 key, u8 instrument): select the wave bank with
 * `key` (the first loaded wave bank when none has it) and an instrument of
 * it. */
u8 *sound_seq_wave_bank_instrument(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 key = data[0];
    u8 instrument = data[1];
    SoundSequence *bank;

    channel->unk25 = key;
    bank = sound_find_wave_bank(key);
    if (bank == NULL) {
        bank = sound_wave_bank_list;
    }
    channel->instruments = bank;
    sound_select_instrument(instrument, channel);
    return data + 2;
}

/* 8003E4BC: FD tempo(u8 tempo): tempo = tempo << 24 unless 0. */
u8 *sound_seq_tempo(u8 *data, SoundSeq *seq) {
    s32 tempo = *data++;

    if (tempo != 0) {
        seq->tempo.value = tempo << 24;
        seq->tick_step = seq->rate.part.whole * (tempo << 8);
    }
    return data;
}

/* 8003E4F0: FE wave_bank(u8 key): select the wave bank with `key` (the first loaded
 * wave bank when none has it). */
u8 *sound_seq_wave_bank(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    u8 key = *data++;
    SoundSequence *bank;

    channel->unk25 = key;
    bank = sound_find_wave_bank(key);
    if (bank == NULL) {
        bank = sound_wave_bank_list;
    }
    channel->instruments = bank;
    return data;
}

/* 8003E54C: FF stop_when_silent(): stop the channel when its voice's envelope level is
 * 0; else continue. */
u8 *sound_seq_stop_when_silent(u8 *data, SoundSeq *seq, SoundSeqChannel *channel) {
    long status;
    short level;

    SpuGetVoiceEnvelopeAttr(channel->voice, &status, &level);
    if (level == 0) {
        channel->flags2 &= ~3;
        sound_release_voice(&channel->state, channel->voice);
        channel->flags = 0;
    }
    return data;
}

/* 8003E5BC: Select instrument `index` of the channel's wave bank: sample and loop
 * addresses, envelope and note offset. */
void sound_select_instrument(s16 index, SoundSeqChannel *channel) {
    SoundSequence *bank;
    SoundInstrument *instrument;
    u32 start;
    u32 bits;

    channel->instrument = index;
    bank = channel->instruments;
    instrument = &bank->instrument[index];
    start = instrument->start * 8;
    channel->state.sample_start = start + bank->address;
    channel->state.sample_loop = start + instrument->loop * 8;
    bits = instrument->modes;
    channel->state.envelope.attack_mode = bits & 7;
    channel->state.envelope.sustain_mode = (bits >> 4) & 7;
    channel->state.envelope.release_mode = (bits >> 8) & 7;
    bits = instrument->envelope;
    channel->state.envelope.attack_rate = bits & 0x7F;
    channel->state.envelope.decay_rate = (bits >> 8) & 0xF;
    channel->state.envelope.sustain_rate = (bits >> 16) & 0x7F;
    channel->state.envelope.sustain_level = (bits >> 12) & 0xF;
    channel->state.envelope.release_rate = channel->unk28 = (bits >> 24) & 0x1F;
    channel->unk6C = instrument->note;
    channel->flags |= 0x8000;
}

/* 8003E680: Set `bits` in flags2 of every active channel of a sequence. */
void sound_request_seq_channel_updates(s32 bits, SoundSeq *seq) {
    SoundSeqChannel *channel = seq->channel;
    s32 count = seq->channels;

    do {
        if (channel->flags != 0) {
            channel->flags2 = bits | channel->flags2;
        }
        channel++;
        count--;
    } while (count != 0);
}

/* 8003E6C0: Request an update of the `bits` voice registers of every active
 * channel of a sequence. */
void sound_request_seq_voice_updates(SoundSeq *seq, s32 bits) {
    SoundSeqChannel *channel = seq->channel;
    s32 count = seq->channels;

    do {
        if (channel->flags != 0) {
            channel->state.flags = channel->state.flags | bits;
        }
        channel++;
        count--;
    } while (count != 0);
}

/* 8003E700: Free every hardware voice (the original steps a byte offset). */
void sound_clear_voice_owners(void) {
    s32 offset;

    for (offset = 23 * 4; offset >= 0; offset -= 4) {
        *(SoundChannel **)((u8 *)sound_voice_owners + offset) = NULL;
    }
}

/* 8003E724: Claim hardware voice `voice` for a channel unless its holder has a
 * higher priority. */
void sound_claim_voice(SoundChannel *state, u32 voice) {
    SoundChannel **owner = &sound_voice_owners[voice];
    SoundChannel *holder;
    u32 bit;

    if (voice >= 24) {
        return;
    }
    holder = *owner;
    if (holder == state) {
        sound_changed_voice_mask |= 1 << voice;
        return;
    }
    if (holder == NULL || holder->priority <= state->priority) {
        state->flags = 0xFFFF;
        bit = 1 << voice;
        state->voice = voice;
        sound_voice_owners[voice] = state;
        sound_changed_voice_mask |= bit;
        sound_pending_key_on_mask &= ~bit;
    }
}

/* 8003E7E0: Claim hardware voice `voice` without requesting a register update. */
void sound_claim_voice_no_update(SoundChannel *state, u32 voice) {
    SoundChannel **owner = &sound_voice_owners[voice];
    SoundChannel *holder;

    if (voice >= 24) {
        return;
    }
    holder = *owner;
    if (holder != state && (holder == NULL || holder->priority <= state->priority)) {
        state->voice = voice;
        *owner = state;
    }
}

/* 8003E83C: Release hardware voice `voice` if the channel holds it. */
void sound_release_voice(SoundChannel *state, u32 voice) {
    SoundChannel **owner = &sound_voice_owners[voice];
    u32 bit;

    if (voice < 24 && *owner == state) {
        *owner = NULL;
        bit = 1 << voice;
        sound_changed_voice_mask |= bit;
        sound_pending_key_on_mask &= ~bit;
    }
}

/* 8003E8A4: Request a register update (and key-on) of hardware voice `voice` if the
 * channel holds it. */
void sound_request_fast_key_off(SoundChannel *state, u32 voice) {
    u32 bit;

    if (voice < 24 && sound_voice_owners[voice] == state) {
        bit = 1 << voice;
        sound_changed_voice_mask |= bit;
        sound_pending_key_on_mask &= ~bit;
    }
}

/* 8003E900: Write the staged registers of every claimed hardware voice to the SPU
 * (volume, pitch, sample addresses, envelope parts), then the pitch
 * modulation, noise and reverb voice masks and the pending key-ons. */
void sound_flush_voice_registers(void) {
    SoundChannel **owner = sound_voice_owners;
    u32 reverb = 0;
    u32 noise = 0;
    u32 modulation = 0;
    u16 masks = 0;
    SpuVoice *regs = sound_spu_registers->voice;
    s32 voice = 0;
    SoundChannel *state;
    u16 flags;
    u16 mode;
    u16 adsr;
    u32 on;

    do {
        state = *owner;
        if (state != NULL) {
            flags = state->flags;
            if (flags != 0) {
                if (flags & 1) {
                    regs->volume_left = state->volume_left;
                    regs->volume_right = state->volume_right;
                }
                if (flags & 4) {
                    regs->pitch = state->pitch;
                }
                if (flags & 8) {
                    regs->address = state->sample_start >> 3;
                    regs->repeat = state->sample_loop >> 3;
                }
                if (flags & 0x10) {
                    adsr = regs->adsr1 & 0xFF;
                    adsr += state->envelope.attack_rate << 8;
                    adsr += (state->envelope.attack_mode >> 2) << 15;
                    regs->adsr1 = adsr;
                }
                if (flags & 0x20) {
                    adsr = regs->adsr1;
                    adsr = (adsr & 0xFF0F) + (state->envelope.decay_rate << 4);
                    regs->adsr1 = adsr;
                }
                if (flags & 0x40) {
                    adsr = regs->adsr2;
                    adsr = (adsr & 0x3F) + (state->envelope.sustain_rate << 6) +
                           ((state->envelope.sustain_mode >> 1) << 14);
                    regs->adsr2 = adsr;
                }
                if (flags & 0x80) {
                    adsr = regs->adsr2 & 0xFFC0;
                    adsr += state->envelope.release_rate + ((state->envelope.release_mode >> 2) << 5);
                    regs->adsr2 = adsr;
                }
                if (flags & 0x100) {
                    adsr = regs->adsr1;
                    adsr = state->envelope.sustain_level + (adsr & 0xFFF0);
                    regs->adsr1 = adsr;
                }
                masks |= flags & 0x7000;
                state->flags = 0;
            }
            mode = state->mode;
            modulation |= ((mode >> 4) & 1) << voice;
            noise |= ((mode >> 5) & 1) << voice;
            reverb |= ((mode >> 6) & 1) << voice;
        }
        regs++;
        voice++;
        owner++;
    } while (voice < 24);
    /* The same register variable addresses the voice masks. */
    regs = sound_spu_registers->voice;
    if (masks != 0) {
        if (masks & 0x1000) {
            ((SpuRegs *)regs)->pitch_mod[0] = modulation;
            ((SpuRegs *)regs)->pitch_mod[1] = modulation >> 16;
        }
        if (masks & 0x2000) {
            ((SpuRegs *)regs)->noise[0] = noise;
            ((SpuRegs *)regs)->noise[1] = noise >> 16;
        }
        if (masks & 0x4000) {
            ((SpuRegs *)regs)->reverb[0] = reverb;
            ((SpuRegs *)regs)->reverb[1] = reverb >> 16;
        }
    }
    on = sound_pending_key_on_mask;
    if (on != 0) {
        ((SpuRegs *)regs)->key_on[0] = on;
        ((SpuRegs *)regs)->key_on[1] = on >> 16;
        sound_pending_key_on_mask = 0;
    }
}

/* 8003EB5C: Key off the requested voices; voices whose registers changed are first
 * switched to a fast linear release (release rate 6). */
void sound_key_off_voices(void) {
    u32 mask = sound_changed_voice_mask;
    SpuRegs *regs = sound_spu_registers;
    s32 voice;
    s32 bit;
    u16 *adsr;

    if (mask != 0) {
        voice = 0;
        bit = 1;
        adsr = &regs->voice[0].adsr2;
        do {
            if (mask & (bit << voice++)) {
                *adsr = (*adsr & 0xFFC0) | 6;
            }
            adsr = (u16 *)((u8 *)adsr + sizeof(SpuVoice));
        } while (voice < 24);
    }
    mask = sound_pending_key_off_mask | sound_changed_voice_mask;
    if (mask != 0) {
        regs->key_off[0] = mask;
        regs->key_off[1] = mask >> 16;
        sound_changed_voice_mask = 0;
        sound_pending_key_off_mask = 0;
    }
}

s16 sound_get_note_pitch(s16 note);

/* 8003EBF0: Stage the voice registers of `count` channels from their pending
 * changes (flags2): volume/pan (0x100), pitch (0x200), key on (1) and key
 * off (2). This is where the sound mode acts on every voice:
 *   Stereo and Wide (driver flag 0x100): the pan law gives each side two
 *     linear ramps, 0x7F00/0 at the edges and 0x5A00 on both sides at the
 *     centre (pan 0x4000);
 *   Mono (flag 0x100 clear): both sides get the centre gain 0x5A00 whatever
 *     the pan.
 * Wide differs from Stereo only in the master and reverb volume signs
 * (80038e6c), not here. */
void sound_stage_seq_voices(SoundSeq *seq, SoundSeqChannel *channels, s16 count) {
    SoundSeqChannel *channel;
    u16 changes;
    s32 pan;
    s32 volume;
    s32 left;
    s32 right;
    s16 level;
    s32 note;

    if (seq->flags & 0x20) {
        return;
    }
    channel = channels;
    do {
        if (channel->flags != 0) {
            changes = channel->flags2;
            if (changes & 0x100) {
                level = channel->level.part.whole;
                volume = level - ((level * channel->level_mod) >> 15);
                if (volume > 0x7FFF) {
                    volume = 0x7FFF;
                }
                if (volume < 0) {
                    volume = 0;
                }
                volume = (channel->volume * volume) >> 15;
                pan = channel->pan + channel->pan_mod + seq->pan.part.whole;
                volume = (seq->fade.part.whole * volume) >> 16;
                if (pan > 0x7F00) {
                    pan = 0x7F00;
                }
                if (pan < 0) {
                    pan = 0;
                }
                if (sound_driver_flags & 0x100) {
                    /* Stereo/Wide pan law. */
                    if (pan < 0x4000) {
                        right = (pan * 0x5A00) >> 14;
                        left = 0x7F00 - ((pan * 0x2500) >> 14);
                    } else {
                        pan = 0x8000 - pan;
                        left = (pan * 0x5A00) >> 14;
                        right = 0x7F00 - ((pan * 0x2500) >> 14);
                    }
                    left = (left * volume) >> 15;
                    right = (right * volume) >> 15;
                } else {
                    /* Mono: the centre gain on both sides. */
                    right = left = (volume * 0x5A00) >> 15;
                }
                channel->state.volume_left = left;
                channel->state.volume_right = right;
                channel->state.flags |= 1;
            }
            if (changes & 0x200) {
                note = channel->note.part.whole + channel->pitch_mod + seq->pitch.part.whole;
                channel->state.pitch = sound_get_note_pitch(note) & 0x3FFF;
                channel->state.flags |= 4;
            }
            if ((changes & 1) && !(channel->flags & 0x20)) {
                sound_key_on_voice(&channel->state, channel->voice);
            }
            if (changes & 2) {
                sound_request_key_off(&channel->state, channel->voice);
            }
            channel->flags2 = 0;
        }
        channel++;
    } while (--count != 0);
}


/* 8003EEA0: SPU pitch of an 8.8 note: a table lookup scaled by octave. */
s16 sound_get_note_pitch(s16 note) {
    s32 entry = sound_semitone_octave_rows[(note & 0x7FFF) >> 8];
    s32 shift = 6 - (entry >> 4);
    s32 pitch = sound_pitch_table[(note & 0xFF) + ((entry & 0xF) << 8)];

    if (shift < 0) {
        pitch <<= -shift;
    } else {
        pitch >>= shift;
    }
    return pitch;
}

/* 8003EF04: Claim hardware voice `voice` (unless a holder has a higher priority)
 * and key it on. */
void sound_key_on_voice(SoundChannel *state, u32 voice) {
    SoundChannel **owner = &sound_voice_owners[voice];
    SoundChannel *holder;

    if (voice >= 24) {
        return;
    }
    holder = *owner;
    if (holder != state) {
        if (holder != NULL && holder->priority > state->priority) {
            return;
        }
        state->flags = 0xFFFF;
        state->voice = voice;
        sound_voice_owners[voice] = state;
        sound_changed_voice_mask |= 1 << voice;
    }
    sound_pending_key_on_mask |= 1 << voice;
}

/* 8003EFA0: Request a key off of hardware voice `voice` if the channel holds it. */
void sound_request_key_off(SoundChannel *state, u32 voice) {
    if (voice < 24 && sound_voice_owners[voice] == state) {
        sound_pending_key_off_mask |= 1 << voice;
    }
}

/* 8003EFE4: Run the modulators of `count` channels: after its delay each running
 * modulator adds its wave (faded in over its period) to the pitch, level
 * or pan offset of its channel and flags the update. */
void sound_run_seq_modulators(SoundSeq *seq, SoundSeqChannel *channel, s16 count) {
    SoundModulator *modulator;
    u16 changes;
    s32 value;
    s16 period;
    s32 i;

    do {
        if (channel->flags != 0) {
            channel->pan_mod = 0;
            channel->level_mod = 0;
            channel->pitch_mod = 0;
            if (channel->modulators != 0) {
                i = 4;
                modulator = channel->modulator;
                changes = channel->flags2;
                do {
                    if (modulator->flags & 1) {
                        if (modulator->delay_count != 0) {
                            modulator->delay_count--;
                        } else {
                            value = modulator->wave(modulator);
                            if (modulator->period_count < 0x400) {
                                period = modulator->period_count;
                                modulator->period_count = period + modulator->period;
                                value = (value >> 10) * period;
                            }
                            value >>= 16;
                            switch (modulator->target) {
                            case 0:
                                changes |= 0x200;
                                channel->pitch_mod += value;
                                break;
                            case 1:
                                changes |= 0x100;
                                channel->level_mod += value;
                                break;
                            case 2:
                                changes |= 0x100;
                                channel->pan_mod += value;
                                break;
                            }
                        }
                    }
                    modulator++;
                } while (--i != 0);
                channel->flags2 = changes;
            }
        }
        channel++;
    } while (--count != 0);
}

/* 8003F190: Switch a modulator off. */
void sound_switch_modulator_off(SoundModulator *modulator) {
    modulator->flags &= ~1;
}

/* 8003F1A4: Modulator waves: each advances one frame and returns the output.
 * Pulse: alternates between 0 and the depth every `rate` frames. */
s32 sound_step_pulse_wave(SoundModulator *modulator) {
    s32 phase;

    if (--modulator->count == 0) {
        modulator->count = modulator->rate;
        phase = 0;
        if (modulator->phase == 0) {
            phase = modulator->step;
        }
        modulator->phase = phase;
    }
    return modulator->phase;
}

/* 8003F1EC: Square: alternates between +depth and -depth. */
s32 sound_step_square_wave(SoundModulator *modulator) {
    s32 phase;

    if (--modulator->count == 0) {
        phase = modulator->step;
        modulator->count = modulator->rate;
        if (modulator->flags & 8) {
            phase = -phase;
        }
        modulator->phase = phase;
        modulator->flags ^= 8;
    }
    return modulator->phase;
}

/* 8003F240: Sawtooth-like: the slope flips sign every `rate` frames. */
s32 sound_step_sawtooth_wave(SoundModulator *modulator) {
    s32 slope;

    if (--modulator->count == 0) {
        slope = modulator->step;
        modulator->count = modulator->rate;
        if (modulator->flags & 8) {
            slope = -slope;
        }
        modulator->slope = slope;
        modulator->flags ^= 8;
    }
    return modulator->phase += modulator->slope;
}

/* 8003F2A0: Triangle: the slope flips sign after `rate` frames, then every
 * 2 * `rate` frames. */
s32 sound_step_triangle_wave(SoundModulator *modulator) {
    s32 count;
    u16 flags;
    s32 step;

    count = modulator->count;
    if (--count == 0) {
        flags = modulator->flags;
        count = modulator->rate;
        if (flags & 4) {
            count *= 2;
        }
        step = modulator->step;
        modulator->slope = step;
        if (flags & 8) {
            modulator->slope = -step;
        }
        flags = (flags | 4) ^ 8;
        modulator->flags = flags;
    }
    modulator->count = count;
    return modulator->phase += modulator->slope;
}

/* 8003F308: Ramp: rises by the step each frame, back to 0 every `rate` frames. */
s32 sound_step_ramp_wave(SoundModulator *modulator) {
    if (--modulator->count == 0) {
        modulator->phase = 0;
        modulator->count = modulator->rate;
    } else {
        modulator->phase += modulator->step;
    }
    return modulator->phase;
}

s32 sound_generate_random(void);

/* 8003F354: Random: a new random level (0..depth) every `rate` frames. */
s32 sound_step_random_wave(SoundModulator *modulator) {
    sound_generate_random();
    if (--modulator->count == 0) {
        modulator->count = modulator->rate;
        modulator->phase = (modulator->step >> 15) * sound_generate_random();
    }
    return modulator->phase;
}

/* 8003F3C0: Random: a new random level (-depth..depth) every `rate` frames. */
s32 sound_step_signed_random_wave(SoundModulator *modulator) {
    if (--modulator->count == 0) {
        modulator->count = modulator->rate;
        modulator->phase = (modulator->step >> 14) * sound_generate_random() - modulator->step;
    }
    return modulator->phase;
}

/* 8003F42C: Seed the driver's random generator. */
void sound_seed_random(s32 seed) {
    sound_random_state = seed;
}

/* 8003F43C: Next random number (0-0x7FFF) of a xorshift generator. */
s32 sound_generate_random(void) {
    s32 x = sound_random_state;

    x ^= x << 17;
    x ^= x >> 15;
    sound_random_state = x;
    return x & 0x7FFF;
}

/* 8003F468: Direct SPU writes: key on, key off and reverb voice masks. */
void sound_write_key_on(u32 voices) {
    sound_spu_registers->key_on[0] = voices;
    sound_spu_registers->key_on[1] = voices >> 16;
}

/* 8003F484 */
void sound_write_key_off(u32 voices) {
    sound_spu_registers->key_off[0] = voices;
    sound_spu_registers->key_off[1] = voices >> 16;
}

/* 8003F4A0 */
void sound_write_reverb_voices(u32 voices) {
    sound_spu_registers->reverb[0] = voices;
    sound_spu_registers->reverb[1] = voices >> 16;
}

/* 8003F4BC: An empty driver entry. */
void sound_empty_entry_after_voice_masks(void) {
}

/* 8003F4C4: Direct SPU voice writes: sample start and loop addresses, volume,
 * pitch and envelope parts. */
void sound_write_voice_start_address(s32 voice, s32 address) {
    (voice + sound_spu_registers->voice)->address = address >> 3;
}

/* 8003F4E0 */
void sound_write_voice_loop_address(s32 voice, s32 address) {
    (voice + sound_spu_registers->voice)->repeat = address >> 3;
}

/* 8003F4FC */
void sound_write_voice_volume(s32 voice, s16 left, s16 right) {
    SpuVoice *regs = &sound_spu_registers->voice[voice];

    regs->volume_left = left;
    regs->volume_right = right;
}

/* 8003F518 */
void sound_write_voice_pitch(s32 voice, u16 pitch) {
    (voice + sound_spu_registers->voice)->pitch = pitch;
}

/* 8003F530: Attack rate and mode (ADSR1 bits 8-15). */
void sound_write_voice_attack(s32 voice, s32 rate, s32 mode) {
    SpuVoice *regs = &sound_spu_registers->voice[voice];

    regs->adsr1 = (regs->adsr1 & 0xFF) + (rate << 8) + ((mode >> 2) << 15);
}

/* 8003F560: Decay rate (ADSR1 bits 4-7). */
void sound_write_voice_decay(s32 voice, s32 rate) {
    SpuVoice *regs = &sound_spu_registers->voice[voice];

    regs->adsr1 = (regs->adsr1 & 0xFF0F) + (rate << 4);
}

/* 8003F588: Sustain rate, direction and mode (ADSR2 bits 6-15). */
void sound_write_voice_sustain(s32 voice, s32 rate, s32 mode) {
    SpuVoice *regs = &sound_spu_registers->voice[voice];

    regs->adsr2 = (regs->adsr2 & 0x3F) + (rate << 6) + ((mode >> 1) << 14);
}

/* 8003F5BC: Release rate and mode (ADSR2 bits 0-5). */
void sound_write_voice_release(s32 voice, s32 rate, s32 mode) {
    SpuVoice *regs = &sound_spu_registers->voice[voice];

    regs->adsr2 = (regs->adsr2 & 0xFFC0) + (rate + ((mode >> 2) << 5));
}

/* 8003F5EC: Sustain level (ADSR1 bits 0-3). */
void sound_write_voice_sustain_level(s32 voice, s32 level) {
    SpuVoice *regs = &sound_spu_registers->voice[voice];

    regs->adsr1 = (regs->adsr1 & 0xFFF0) + level;
}

s32 sound_sum_file_words(u32 *data);

/* 8003F614: Check a sound file: 1 wrong magic, 2 bad checksum, 4 wrong id, 0 good. */
s32 sound_check_file(u32 *data, u32 magic, s32 id) {
    if (data[0] != magic) {
        return 1;
    }
    if (sound_sum_file_words(data) != 0) {
        return 2;
    }
    return (*(u16 *)&data[3] != (id & 0xFFFF)) * 4;
}

/* 8003F67C: Sequence header check: always passes. */
s16 sound_check_seq_header(SoundSeqHeader *header) {
    return 0;
}

/* 8003F684: Sum of the words of a sound file (its byte size at word 2). */
s32 sound_sum_file_words(u32 *data) {
    s32 sum = 0;
    u32 count = (data[2] + 3) >> 2;

    do {
        sum += *data++;
        count--;
    } while (count != 0);
    return sum;
}

/* 8003F6B0: Report a driver error once (until cleared): remember the code, load the
 * built-in error bank and play its beep. */
void sound_report_error(s32 error) {
    if (sound_driver_flags & 0x88) {
        return;
    }
    sound_driver_flags |= 8;
    sound_unread_last_error = error;
    sound_free_spu_memory(0x10000);
    sound_load_wave_bank((SoundSequence *)sound_error_wave_bank, 0);
    sound_add_effect_bank((SoundBank *)sound_error_effect_bank);
    sound_sync_transfer(0x10);
    sound_play_effect((((SoundBank *)sound_error_effect_bank)->id << 16) | 1);
}
