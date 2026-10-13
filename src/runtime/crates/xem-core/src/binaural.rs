//! The Binaural sound mode's game side (plan.md, Phase 9; docs/runtime.md,
//! Binaural sound mode). The SPU device mixes its voices again through
//! [`xem_binaural::Renderer`] while the mode is on; this module tells it
//! what each voice is and where it is heard. Everything here only reads:
//! the game, the sound driver and the SPU run as they do without it.
//!
//! - **What a voice is.** When the SPU keys a voice on, the sound driver's
//!   records say who keyed it (`sound_voice_owners`): a channel of a
//!   playing sequence (a part of the music), one of the effect channels (an
//!   effect, by its id and start time; an effect's two channels are one
//!   source), or neither. The voice keeps that identity through its
//!   release, until it is keyed on again.
//! - **Music.** A sequence's parts are profiled from its own data the first
//!   time one of them plays (notes, pitch, length, drum-kit and noise
//!   notes, pan before the first note) and arranged by
//!   [`xem_binaural::music`]'s rules; the parts keep their places for the
//!   whole song, the music's own pan movement turning them.
//! - **Effects.** An effect heard in the world goes where its emitter is:
//!   the field's three positional emitters (`field_sound_emitter_slots`,
//!   each following an actor) and the arena's four positional voices
//!   (`arena_sound_voices`, a fixed point or a followed vector). The
//!   direction is from the listening position (the field's controlled
//!   character, the arena's player Gear) in the camera's orientation, the
//!   distance dropped: the game's own volume keeps its distance law. Every
//!   other effect, the reverb return, voices nobody owns and movie audio
//!   keep the original mix; effects left so are counted by mode and id
//!   (`coverage`), not guessed.

use std::collections::{BTreeMap, HashMap};

use serde::{Deserialize, Serialize};
use serde_json::{Value, json};
use xem_binaural::{CdRoute, Renderer, Route, Target, music};
use xem_spu::{TapFrame, VOICES};

use crate::memory::GameMemory;

// The sound driver (src/decomp/include/resident/sound.h).
/// `sound_voice_owners[24]`: the SoundChannel holding each hardware voice.
const VOICE_OWNERS: u32 = 0x8006_252C;
/// `sound_effect_channels` (the effect channels' SoundSeq) and their count.
const EFFECT_CHANNELS: u32 = 0x8005_95D8;
const EFFECT_CHANNEL_COUNT: u32 = 0x8005_9478;
/// `sound_playing_seq_list`, linked through SoundSeq.next (+0).
const PLAYING_SEQS: u32 = 0x8005_9564;
/// SoundSeq: its data's header, its channel count, its channels.
const SEQ_HEADER: u32 = 0x08;
const SEQ_CHANNELS: u32 = 0x14;
const SEQ_CHANNEL_ARRAY: u32 = 0x94;
/// SoundSeqChannel: size, effect id, start time, wave bank, and the
/// SoundChannel `sound_voice_owners` points at.
const CHANNEL_SIZE: u32 = 0x158;
const CHANNEL_ID: u32 = 0x08;
const CHANNEL_STAMP: u32 = 0x0C;
const CHANNEL_INSTRUMENTS: u32 = 0x2C;
const CHANNEL_STATE: u32 = 0x30;
/// SoundSeqHeader: each channel's data offset (from the header).
const HEADER_CHANNEL_OFFSETS: u32 = 0x22;
/// A wave bank's instruments (16 bytes each; the note offset, 1/256
/// semitones, at +6).
const BANK_INSTRUMENTS: u32 = 0x30;

// The field (src/decomp/src/field/field_sound.h, xem_core::field).
const FIELD_VIEW_SCALED_WORLD: u32 = 0x800A_F880 + 0x1E4;
const FIELD_COMPONENTS: u32 = 0x800A_FB0C;
const FIELD_CONTROLLED: u32 = 0x800B_2078 + 0x1F4;
const FIELD_DESCRIPTOR_SIZE: u32 = 0x5C;
/// `field_sound_emitter_slots[3]`: the descriptor each follows and its
/// sound (u16 each; 0xFFFF free).
const FIELD_EMITTER_SLOTS: u32 = 0x800A_FE88;

// The arena (src/decomp/src/menu/sound.h, actor.h, node.h).
/// `arena_sound_voices[4]` (0x24 bytes each).
const ARENA_SOUND_VOICES: u32 = 0x8009_6EA0;
/// `arena_first_actor`, the player's Gear: its position, and its node
/// (`object`, +0x60), whose view (+0x0C) is the camera's.
const ARENA_PLAYER: u32 = 0x8009_872C;

/// `Services::mode` values.
const MODE_FIELD: u32 = 1;
const MODE_ARENA: u32 = 4;

/// Note ticks by key (`sound_note_durations[k] = DURATIONS[k % 19]`; 0: the
/// next byte holds them).
const DURATIONS: [u8; 19] = [0, 192, 144, 96, 72, 64, 48, 36, 32, 24, 18, 16, 12, 9, 8, 6, 4, 3, 2];
/// What each sequence opcode 0x80-0xFF consumes, opcode included (0:
/// unused), as the handlers do (tools/analysis/sound_sequence.py).
const OPCODE_LENGTHS: [u8; 128] = [
    2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 2, 4, 1, 1, 1, 0, 0, 2, 1, 1, 3, 2, 1, 1, 0, 4, 3, 4, 0, //
    2, 2, 3, 0, 2, 2, 2, 3, 0, 2, 2, 0, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 4, 0, 1, 1, 4, 1, 1, 0, //
    1, 4, 2, 2, 2, 2, 2, 3, 2, 2, 2, 0, 0, 0, 0, 0, 2, 2, 2, 3, 3, 1, 2, 2, 4, 4, 1, 1, 1, 0, 0, 0, //
    2, 2, 3, 2, 4, 4, 1, 1, 2, 2, 3, 2, 4, 4, 1, 1, 4, 4, 3, 0, 0, 1, 2, 2, 4, 3, 0, 0, 3, 2, 2, 1,
];
/// The most a part's data is read for its profile.
const PROFILE_BYTES: u32 = 0x8000;

/// Reviewed placements for particular songs, by their fingerprint: (part,
/// role) replacing the rules' choice. None are needed yet.
const OVERRIDES: &[(u64, &[(usize, music::Role)])] = &[];

/// The Binaural sound mode's settings. They are presentation only: under
/// the mode the hosts run the game in its Stereo (`Session::set_sound_mode`),
/// and nothing here reaches game state.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(default, deny_unknown_fields)]
pub struct Config {
    pub enabled: bool,
    /// Music arranged around the listener (else its original mix).
    pub spatial_music: bool,
    /// World effects placed where their emitters are (else their original
    /// mix).
    pub positional_effects: bool,
    /// How far the music spreads: 0, its stereo arc ahead, to 100, the
    /// whole arrangement.
    pub music_immersion: u8,
    /// In a headset the music's stage stays put while the head turns
    /// (else it turns with the head).
    pub head_tracked_music: bool,
    /// Present the original mix instead, for comparison.
    pub bypass: bool,
}

impl Default for Config {
    fn default() -> Self {
        Config { enabled: false, spatial_music: true, positional_effects: true, music_immersion: 70, head_tracked_music: false, bypass: false }
    }
}

impl Config {
    pub fn validate(&self) -> Result<(), String> {
        if self.music_immersion > 100 {
            return Err("music immersion must be 0-100".into());
        }
        Ok(())
    }
}

/// What keyed a voice on.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Tag {
    /// Part `part` of the sequence at `seq` (its data's fingerprint).
    Music { seq: u32, song: u64, part: u8 },
    /// Effect channel `channel` playing effect `id` (bank << 16 | effect)
    /// started at driver tick `stamp`.
    Effect { channel: u8, id: u32, stamp: u32 },
    /// No sequence or effect channel of the driver (`owner`, 0: none).
    Other { owner: u32 },
}

/// A source: one part of one song, or one effect start (both its channels).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
enum SourceKey {
    Music { seq: u32, song: u64, part: u8 },
    Effect { pair: u8, stamp: u32, id: u32 },
}

/// A song, profiled.
struct Song {
    header: u32,
    fingerprint: u64,
    placements: Vec<music::Placement>,
}

/// A positional emitter the game drives: the effect channels it plays on,
/// the sound, where it is (world), what it follows.
#[derive(Clone, Copy, Debug, PartialEq)]
struct Emitter {
    pair: u8,
    sound: u32,
    /// The whole id must match (the arena's); else its low 16 bits (the
    /// field keeps the effect number only).
    full_id: bool,
    position: [f32; 3],
    follows: u32,
}

/// Where the listener is and what it hears positionally, read once a frame.
#[derive(Clone, Debug, Default)]
struct Scene {
    mode: Option<u32>,
    /// The camera's orientation: world to view space (rows, unit).
    camera: Option<[[f32; 3]; 3]>,
    /// The listening position in the world.
    listener: Option<[f32; 3]>,
    emitters: Vec<Emitter>,
    movie: bool,
}

/// Placing an effect: where (listener space; zero keeps the direction), and
/// how it was resolved.
#[derive(Clone, Copy, Debug, PartialEq)]
enum Placing {
    Placed { direction: [f32; 3], follows: u32, emitter: &'static str },
    Unresolved(&'static str),
}

/// A head's axes (right, down, forward) in the game camera's view space,
/// from its orientation `[x, y, z, w]` in an XR reference space (+X right,
/// +Y up, -Z ahead: OpenXR's and WebXR's) whose -Z faces the game's
/// presentation, as `Session::set_head` takes them.
pub fn head_axes(orientation: [f32; 4]) -> [[f32; 3]; 3] {
    let [x, y, z, w] = orientation;
    let length = (x * x + y * y + z * z + w * w).sqrt().max(f32::EPSILON);
    let (q, w) = ([x / length, y / length, z / length], w / length);
    let cross = |a: [f32; 3], b: [f32; 3]| [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
    let rotate = |v: [f32; 3]| {
        let t = cross(q, v).map(|c| 2.0 * c);
        let u = cross(q, t);
        [v[0] + w * t[0] + u[0], v[1] + w * t[1] + u[1], v[2] + w * t[2] + u[2]]
    };
    let view = |v: [f32; 3]| [v[0], -v[1], -v[2]];
    [view(rotate([1.0, 0.0, 0.0])), view(rotate([0.0, -1.0, 0.0])), view(rotate([0.0, 0.0, -1.0]))]
}

fn ram(pointer: u32) -> Option<u32> {
    (0x8000_0000..0x8020_0000).contains(&pointer).then_some(pointer)
}

fn read<const N: usize>(memory: &dyn GameMemory, address: u32) -> Option<[u8; N]> {
    let mut bytes = [0; N];
    memory.read(address, &mut bytes).ok().map(|()| bytes)
}

fn u8_at(memory: &dyn GameMemory, address: u32) -> Option<u8> {
    read::<1>(memory, address).map(|b| b[0])
}

fn u16_at(memory: &dyn GameMemory, address: u32) -> Option<u16> {
    read::<2>(memory, address).map(u16::from_le_bytes)
}

fn s16_at(memory: &dyn GameMemory, address: u32) -> Option<i16> {
    read::<2>(memory, address).map(i16::from_le_bytes)
}

fn u32_at(memory: &dyn GameMemory, address: u32) -> Option<u32> {
    read::<4>(memory, address).map(u32::from_le_bytes)
}

fn vector_at(memory: &dyn GameMemory, address: u32) -> Option<[f32; 3]> {
    let b = read::<12>(memory, address)?;
    Some([0, 1, 2].map(|i| i32::from_le_bytes([b[i * 4], b[i * 4 + 1], b[i * 4 + 2], b[i * 4 + 3]]) as f32))
}

/// A GTE matrix's rotation rows (4.12), each made unit length.
fn rotation_at(memory: &dyn GameMemory, address: u32) -> Option<[[f32; 3]; 3]> {
    let b = read::<18>(memory, address)?;
    let element = |i: usize| f32::from(i16::from_le_bytes([b[i * 2], b[i * 2 + 1]]));
    let mut rows = [[0f32; 3]; 3];
    for (r, row) in rows.iter_mut().enumerate() {
        *row = [element(r * 3), element(r * 3 + 1), element(r * 3 + 2)];
        let length = row.iter().map(|c| c * c).sum::<f32>().sqrt();
        if length < 1.0 {
            return None;
        }
        *row = row.map(|c| c / length);
    }
    Some(rows)
}

fn apply(rows: &[[f32; 3]; 3], v: [f32; 3]) -> [f32; 3] {
    rows.map(|row| row[0] * v[0] + row[1] * v[1] + row[2] * v[2])
}

/// FNV-1a over `bytes`, continuing `hash`.
fn fnv(mut hash: u64, bytes: &[u8]) -> u64 {
    for byte in bytes {
        hash = (hash ^ u64::from(*byte)).wrapping_mul(0x0000_0100_0000_01B3);
    }
    hash
}

/// The profile of one part's data at `data`; `offsets` gives an
/// instrument's note offset (semitones) where the part's wave bank says.
fn profile_part(memory: &dyn GameMemory, data: u32, offset: &dyn Fn(u8) -> f64) -> music::PartProfile {
    let mut part = music::PartProfile::default();
    let end = data.saturating_add(PROFILE_BYTES).min(0x8020_0000);
    let mut bytes = vec![0u8; end.saturating_sub(data) as usize];
    if memory.read(data, &mut bytes).is_err() {
        return part;
    }
    let mut at = 0usize;
    let (mut transpose, mut drums, mut noise, mut instrument_offset) = (60i32, false, false, offset(0));
    let byte = |at: usize| bytes.get(at).copied();
    while let Some(op) = byte(at) {
        if op < 0x80 {
            let Some(key) = byte(at + 1).filter(|k| *k < 228) else { return part };
            let mut ticks = DURATIONS[usize::from(key % 19)];
            at += 2;
            if ticks == 0 {
                let Some(extra) = byte(at) else { return part };
                ticks = extra;
                at += 1;
            }
            part.notes += 1;
            part.ticks += u64::from(ticks);
            part.velocity_sum += u64::from(op);
            if drums || noise {
                part.percussive_notes += 1;
            } else {
                part.pitch_sum += f64::from(transpose + i32::from(key / 19)) + instrument_offset;
            }
            continue;
        }
        let argument = byte(at + 1);
        match op {
            // end: the data ends here (or loops back).
            0x90 | 0x9E => {
                part.complete = true;
                return part;
            }
            0x94 => transpose = i32::from(argument.unwrap_or(5)) * 12,
            0x95 => transpose += 12,
            0x96 => transpose -= 12,
            0xAC => instrument_offset = offset(argument.unwrap_or(0)),
            0xAE => drums = true,
            0xAF => drums = false,
            0xB6 => noise = true,
            0xB7 => noise = false,
            0xE8 if part.notes == 0 && part.home_pan.is_none() => part.home_pan = argument.map(|p| u16::from(p) << 8),
            _ => {}
        }
        let length = OPCODE_LENGTHS[usize::from(op - 0x80)];
        if length == 0 {
            return part;
        }
        at += usize::from(length);
    }
    part
}

/// The Binaural presentation of the SPU's voices (one per SPU device while
/// the mode is on or fading out).
pub struct Presenter {
    config: Config,
    renderer: Renderer,
    tags: [Option<Tag>; VOICES],
    ids: HashMap<SourceKey, u32>,
    next_id: u32,
    songs: HashMap<u32, Song>,
    scene: Scene,
    /// The listener's axes in the camera's view space (first-person
    /// inspection), and the head's in the presentation's (headsets).
    view: Option<[[f32; 3]; 3]>,
    head: Option<[[f32; 3]; 3]>,
    /// Every sounding voice's identity is to be read again (after a restore).
    retag: bool,
    /// Effects that played without a position: (mode, id) -> voices keyed on.
    unresolved: BTreeMap<(u32, u32), u64>,
    out: Vec<i16>,
}

impl Presenter {
    pub fn new(config: Config) -> Self {
        let mut renderer = Renderer::new();
        renderer.set_enabled(config.enabled && !config.bypass);
        Presenter {
            config,
            renderer,
            tags: [None; VOICES],
            ids: HashMap::new(),
            next_id: 1,
            songs: HashMap::new(),
            scene: Scene::default(),
            view: None,
            head: None,
            retag: true,
            unresolved: BTreeMap::new(),
            out: Vec::new(),
        }
    }

    pub fn config(&self) -> Config {
        self.config
    }

    /// New settings: routes follow at once, placements glide and the mix
    /// crossfades.
    pub fn configure(&mut self, config: Config) {
        let reroute = (config.spatial_music, config.positional_effects, config.music_immersion)
            != (self.config.spatial_music, self.config.positional_effects, self.config.music_immersion);
        self.config = config;
        self.renderer.set_enabled(config.enabled && !config.bypass);
        if reroute {
            self.update_sources();
        }
    }

    /// Whether the mode is off and its fade is over: the device may drop it.
    pub fn finished(&self) -> bool {
        !self.config.enabled && self.renderer.presenting_original()
    }

    /// The listener's axes in the game camera's view space while the
    /// presentation looks from elsewhere (first-person inspection).
    pub fn set_view(&mut self, axes: Option<[[f32; 3]; 3]>) {
        if self.view != axes {
            self.view = axes;
            self.update_sources();
        }
    }

    /// The head's axes in the presentation's reference space (as the game
    /// camera's view space is oriented: [`head_axes`]); none: no head (out
    /// of a headset). A host that loses tracking keeps the last by not
    /// calling.
    pub fn set_head(&mut self, axes: Option<[[f32; 3]; 3]>) {
        if self.head != axes {
            self.head = axes;
            self.update_sources();
        }
    }

    /// Forgets the sound so far (a restored state): the renderer's history
    /// and the voices' identities, which are read again at the next sync.
    pub fn reset(&mut self) {
        self.renderer.reset();
        self.renderer.set_enabled(self.config.enabled && !self.config.bypass);
        self.tags = [None; VOICES];
        self.ids.clear();
        self.scene = Scene::default();
        self.retag = true;
    }

    /// Whether every sounding voice is to be identified again.
    pub fn wants_retag(&self) -> bool {
        self.retag
    }

    /// Reads who keyed on the voices of `mask` (the SPU's key-on bits of
    /// one register sync) and routes them.
    pub fn key_on(&mut self, mask: u32, memory: &dyn GameMemory) {
        let mut emitters_read = false;
        for v in 0..VOICES {
            if mask & (1 << v) == 0 {
                continue;
            }
            let tag = self.identify(v, memory);
            if let Tag::Effect { id, .. } = tag {
                // An emitter starting now: the game wrote its slot before the
                // effect, so the slot is read with the key-on, not a frame later.
                if !emitters_read {
                    self.read_scene(memory, self.scene.mode, self.scene.movie);
                    emitters_read = true;
                }
                if matches!(self.place_effect(tag), Placing::Unresolved(_)) {
                    *self.unresolved.entry((self.scene.mode.unwrap_or(u32::MAX), id)).or_default() += 1;
                }
            }
            self.tags[v] = Some(tag);
            self.route(v, false);
        }
        self.retag = false;
    }

    /// Identifies every voice still sounding (`sounding` bits), as after a
    /// restore.
    pub fn retag(&mut self, sounding: u32, memory: &dyn GameMemory) {
        self.key_on(sounding, memory);
        self.retag = false;
    }

    fn identify(&mut self, v: usize, memory: &dyn GameMemory) -> Tag {
        let owner = u32_at(memory, VOICE_OWNERS + 4 * v as u32).unwrap_or(0);
        let Some(owner) = ram(owner) else { return Tag::Other { owner: 0 } };
        let channel_of = |seq: u32, count: u32| -> Option<u32> {
            let first = seq + SEQ_CHANNEL_ARRAY + CHANNEL_STATE;
            let offset = owner.checked_sub(first)?;
            (offset % CHANNEL_SIZE == 0 && offset / CHANNEL_SIZE < count).then_some(offset / CHANNEL_SIZE)
        };
        let effects = u32_at(memory, EFFECT_CHANNELS).and_then(ram);
        if let Some(effects) = effects {
            let count = u32_at(memory, EFFECT_CHANNEL_COUNT).unwrap_or(0).min(24);
            if let Some(c) = channel_of(effects, count) {
                let record = effects + SEQ_CHANNEL_ARRAY + c * CHANNEL_SIZE;
                return Tag::Effect {
                    channel: c as u8,
                    id: u32_at(memory, record + CHANNEL_ID).unwrap_or(0),
                    stamp: u32_at(memory, record + CHANNEL_STAMP).unwrap_or(0),
                };
            }
        }
        let mut seq = u32_at(memory, PLAYING_SEQS).and_then(ram);
        for _ in 0..16 {
            let Some(s) = seq else { break };
            if Some(s) != effects {
                let count = u32::from(u8_at(memory, s + SEQ_CHANNELS).unwrap_or(0));
                if let Some(part) = channel_of(s, count) {
                    let song = self.song(s, memory);
                    return Tag::Music { seq: s, song, part: part as u8 };
                }
            }
            seq = u32_at(memory, s).and_then(ram);
        }
        Tag::Other { owner }
    }

    /// The fingerprint of the song at `seq`, profiling and arranging it the
    /// first time it is seen.
    fn song(&mut self, seq: u32, memory: &dyn GameMemory) -> u64 {
        let header = u32_at(memory, seq + SEQ_HEADER).and_then(ram).unwrap_or(0);
        let count = u32::from(u8_at(memory, seq + SEQ_CHANNELS).unwrap_or(0)).min(32);
        let offsets: Vec<u16> = (0..count).map(|i| u16_at(memory, header + HEADER_CHANNEL_OFFSETS + 2 * i).unwrap_or(0)).collect();
        let mut fingerprint = fnv(0xCBF2_9CE4_8422_2325, &header.to_le_bytes());
        for offset in &offsets {
            let mut start = [0u8; 32];
            if *offset != 0 && memory.read(header + u32::from(*offset), &mut start).is_ok() {
                fingerprint = fnv(fingerprint, &start);
            }
            fingerprint = fnv(fingerprint, &offset.to_le_bytes());
        }
        if self.songs.get(&seq).is_some_and(|song| song.header == header && song.fingerprint == fingerprint) {
            return fingerprint;
        }
        let parts: Vec<music::PartProfile> = offsets
            .iter()
            .enumerate()
            .map(|(i, offset)| {
                if *offset == 0 || header == 0 {
                    return music::PartProfile::default();
                }
                let bank = u32_at(memory, seq + SEQ_CHANNEL_ARRAY + i as u32 * CHANNEL_SIZE + CHANNEL_INSTRUMENTS).and_then(ram);
                let offset_of = |instrument: u8| -> f64 {
                    bank.and_then(|bank| s16_at(memory, bank + BANK_INSTRUMENTS + 16 * u32::from(instrument) + 6)).map_or(0.0, |n| f64::from(n) / 256.0)
                };
                profile_part(memory, header + u32::from(*offset), &offset_of)
            })
            .collect();
        let overrides = OVERRIDES.iter().find(|(song, _)| *song == fingerprint).map_or(&[][..], |(_, o)| o);
        let placements = music::arrange(&parts, overrides);
        self.songs.insert(seq, Song { header, fingerprint, placements });
        fingerprint
    }

    fn source_id(&mut self, key: SourceKey) -> u32 {
        if let Some(id) = self.ids.get(&key) {
            return *id;
        }
        let id = self.next_id;
        self.next_id = self.next_id.wrapping_add(1).max(1);
        self.ids.insert(key, id);
        id
    }

    /// Where an effect is heard, by the scene's emitters.
    fn place_effect(&self, tag: Tag) -> Placing {
        let Tag::Effect { channel, id, .. } = tag else { return Placing::Unresolved("not an effect") };
        let pair = channel & !1;
        let Some(emitter) = self.scene.emitters.iter().find(|e| e.pair == pair && if e.full_id { e.sound == id } else { e.sound == id & 0xFFFF }) else {
            return Placing::Unresolved(match self.scene.mode {
                Some(MODE_FIELD) | Some(MODE_ARENA) => "not one of the mode's positional emitters",
                _ => "no positional emitters recovered in this mode",
            });
        };
        let (Some(camera), Some(listener)) = (self.scene.camera, self.scene.listener) else {
            return Placing::Unresolved("no listener");
        };
        let world = [0, 1, 2].map(|i| emitter.position[i] - listener[i]);
        let distance = world.iter().map(|c| c * c).sum::<f32>().sqrt();
        // At the listener the direction is kept (zero), not made up.
        let direction = if distance < 1.0 { [0.0; 3] } else { self.to_listener(apply(&camera, world.map(|c| c / distance))) };
        let kind = if self.scene.mode == Some(MODE_ARENA) { "arena voice" } else { "field emitter" };
        Placing::Placed { direction, follows: emitter.follows, emitter: kind }
    }

    /// A direction in the game camera's view space as the listener hears it:
    /// through the first-person view, then the head, each once.
    fn to_listener(&self, view: [f32; 3]) -> [f32; 3] {
        let looked = self.view.map_or(view, |axes| apply(&axes, view));
        self.head.map_or(looked, |axes| apply(&axes, looked))
    }

    /// Routes voice `v` by its identity, the settings and the scene. A voice
    /// keyed on now is placed at once; one already `sounding` fades between
    /// its original mix and its place, and keeps its source once it has
    /// one (its emitter gone, it keeps its last direction to its end).
    fn route(&mut self, v: usize, sounding: bool) {
        let current = self.renderer.route(v);
        let next = match self.tags[v] {
            Some(Tag::Music { seq, song, part }) => {
                let id = self.source_id(SourceKey::Music { seq, song, part });
                self.music_target(seq, part).and_then(|target| self.choose(id, target, self.config.spatial_music, current, sounding))
            }
            Some(tag @ Tag::Effect { channel, id, stamp }) => {
                let source = self.source_id(SourceKey::Effect { pair: channel & !1, stamp, id });
                match self.place_effect(tag) {
                    Placing::Placed { direction, .. } => self.choose(source, Target::Listener(direction), self.config.positional_effects, current, sounding),
                    Placing::Unresolved(_) if current == Route::Source(source) => {
                        self.renderer.set_placed(source, self.config.positional_effects);
                        Some(source)
                    }
                    Placing::Unresolved(_) => None,
                }
            }
            _ => None,
        };
        let route = next.map_or(Route::Original, Route::Source);
        if current != route {
            self.renderer.set_route(v, route);
        }
    }

    /// The source a voice takes, if any: placed at `target` when `wanted`,
    /// else fading back to the original mix if it was placed.
    fn choose(&mut self, id: u32, target: Target, wanted: bool, current: Route, sounding: bool) -> Option<u32> {
        let routed = current == Route::Source(id);
        if wanted {
            if sounding && !routed {
                self.renderer.fade_in_source(id, target);
            } else {
                self.renderer.set_source(id, target, true);
            }
            Some(id)
        } else if routed {
            self.renderer.set_source(id, target, false);
            Some(id)
        } else {
            None
        }
    }

    /// Where part `part` of the song at `seq` plays, at the immersion set.
    fn music_target(&self, seq: u32, part: u8) -> Option<Target> {
        let placement = self.songs.get(&seq)?.placements.get(usize::from(part))?;
        let (azimuth, elevation, spread) = placement.at(f32::from(self.config.music_immersion) / 100.0);
        Some(Target::Stage { azimuth, elevation, home_pan: placement.home_pan, spread })
    }

    /// Places every voice's source again (settings, scene or view changed).
    fn update_sources(&mut self) {
        let stage = match (self.config.head_tracked_music, self.head) {
            (true, Some(head)) => head,
            _ => [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
        };
        self.renderer.set_stage_rotation(stage);
        for v in 0..VOICES {
            if self.tags[v].is_some() {
                self.route(v, true);
            }
        }
    }

    /// Reads the scene of `mode`: its camera, listening position and
    /// positional emitters.
    fn read_scene(&mut self, memory: &dyn GameMemory, mode: Option<u32>, movie: bool) {
        let mut scene = Scene { mode, movie, ..Scene::default() };
        match mode {
            Some(MODE_FIELD) => read_field(memory, &mut scene),
            Some(MODE_ARENA) => read_arena(memory, &mut scene),
            _ => {}
        }
        self.scene = scene;
    }

    /// Reads the scene (once a frame, after it ran): the camera, the
    /// listening position and the positional emitters of `mode`; `movie`
    /// says whether a movie plays (its audio keeps its original mix).
    pub fn update_scene(&mut self, memory: &dyn GameMemory, mode: Option<u32>, movie: bool) {
        self.read_scene(memory, mode, movie);
        let immersion = f32::from(self.config.music_immersion) / 100.0;
        self.renderer.set_cd_route(if movie || !self.config.spatial_music { CdRoute::Original } else { CdRoute::Upmix { immersion } });
        self.update_sources();
    }

    /// Mixes the samples `tap` says the SPU made into `ring`.
    pub fn render(&mut self, tap: &[TapFrame], ring: &mut std::collections::VecDeque<i16>) {
        self.out.clear();
        for frame in tap {
            self.renderer.push(frame, &mut self.out);
        }
        ring.extend(self.out.iter().copied());
    }

    /// Ends the presentation once [`finished`](Self::finished): the samples
    /// of the block under way go to `ring` as the SPU mixed them.
    pub fn finish(&mut self, ring: &mut std::collections::VecDeque<i16>) {
        self.out.clear();
        self.renderer.flush_original(&mut self.out);
        ring.extend(self.out.iter().copied());
    }

    /// What the presentation is doing (the session's `binaural` command).
    pub fn status(&self) -> Value {
        let keys: HashMap<u32, SourceKey> = self.ids.iter().map(|(key, id)| (*id, *key)).collect();
        let sources: Vec<Value> = self
            .renderer
            .sources()
            .into_iter()
            .map(|source| {
                let mut entry = json!({
                    "id": source.id,
                    "azimuth": source.azimuth,
                    "elevation": source.elevation,
                    "placed": source.placed,
                    "gain": source.gain,
                    "level_db": source.level_db,
                    "voices": source.voices,
                });
                match keys.get(&source.id) {
                    Some(SourceKey::Music { seq, song, part }) => {
                        let role = self.songs.get(seq).and_then(|s| s.placements.get(usize::from(*part))).map(|p| p.role.name());
                        entry["kind"] = json!("music");
                        entry["song"] = json!(format!("{song:016x}"));
                        entry["part"] = json!(part);
                        entry["role"] = json!(role);
                    }
                    Some(SourceKey::Effect { pair, id, stamp }) => {
                        entry["kind"] = json!("effect");
                        entry["effect"] = json!(format!("{id:#x}"));
                        entry["channels"] = json!([pair, pair + 1]);
                        let tag = Tag::Effect { channel: *pair, id: *id, stamp: *stamp };
                        if let Placing::Placed { follows, emitter, .. } = self.place_effect(tag) {
                            entry["emitter"] = json!(emitter);
                            entry["follows"] = json!(follows);
                        } else {
                            entry["emitter"] = json!("gone: keeps its last direction");
                        }
                    }
                    None => {}
                }
                entry
            })
            .collect();
        let voices: Vec<Value> = self
            .tags
            .iter()
            .enumerate()
            .filter_map(|(v, tag)| {
                let route = match self.renderer.route(v) {
                    Route::Original => json!("original"),
                    Route::Source(id) => json!(id),
                };
                Some(match (*tag)? {
                    Tag::Music { part, .. } => json!({"voice": v, "music_part": part, "route": route}),
                    Tag::Effect { channel, id, .. } => {
                        let why = match self.place_effect(tag.expect("some")) {
                            Placing::Unresolved(why) => json!(why),
                            Placing::Placed { .. } => Value::Null,
                        };
                        json!({"voice": v, "effect_channel": channel, "effect": format!("{id:#x}"), "route": route, "unresolved": why})
                    }
                    Tag::Other { owner } => json!({"voice": v, "owner": format!("{owner:#x}"), "route": route}),
                })
            })
            .collect();
        let coverage: Vec<Value> = self
            .unresolved
            .iter()
            .map(|((mode, id), key_ons)| json!({"mode": if *mode == u32::MAX { Value::Null } else { json!(mode) }, "effect": format!("{id:#x}"), "key_ons": key_ons}))
            .collect();
        let listener = match (self.scene.mode, self.scene.listener) {
            (Some(MODE_FIELD), Some(_)) => "field: the controlled character, the camera's orientation",
            (Some(MODE_ARENA), Some(_)) => "arena: the player's Gear, the camera's orientation",
            _ => "none: no world positions in this mode (music and original mixes only)",
        };
        json!({
            "config": self.config,
            "presenting": !self.renderer.presenting_original(),
            "listener": listener,
            "first_person": self.view.map(|axes| facing(axes[2])),
            "head": self.head.map(|axes| facing(axes[2])),
            "cd": if self.scene.movie { "original (a movie plays)" } else if self.config.spatial_music { "estimated arrangement (premixed music)" } else { "original" },
            "sources": sources,
            "voices": voices,
            "coverage_gaps": coverage,
        })
    }
}

/// Where a listener's forward axis (in the space it turned in) faces:
/// `{yaw, pitch}`, degrees, yaw to the right.
fn facing(forward: [f32; 3]) -> Value {
    let [x, y, z] = forward;
    json!({"yaw": x.atan2(z).to_degrees(), "pitch": (-y).atan2(x.hypot(z)).to_degrees()})
}

/// The field's camera, controlled character and emitters.
fn read_field(memory: &dyn GameMemory, scene: &mut Scene) {
    scene.camera = rotation_at(memory, FIELD_VIEW_SCALED_WORLD);
    let Some(components) = read::<8>(memory, FIELD_COMPONENTS) else { return };
    let count = i32::from_le_bytes([components[0], components[1], components[2], components[3]]).clamp(0, 256) as u32;
    let Some(descriptors) = ram(u32::from_le_bytes([components[4], components[5], components[6], components[7]])) else { return };
    let position = |index: u32| (index < count).then(|| vector_at(memory, descriptors + index * FIELD_DESCRIPTOR_SIZE + 0x20)).flatten();
    scene.listener = u32_at(memory, FIELD_CONTROLLED).and_then(position);
    for slot in 0..3 {
        let (Some(actor), Some(sound)) = (u16_at(memory, FIELD_EMITTER_SLOTS + slot * 6), u16_at(memory, FIELD_EMITTER_SLOTS + slot * 6 + 2)) else { continue };
        if sound == 0xFFFF {
            continue;
        }
        if let Some(at) = position(u32::from(actor)) {
            // field_sound_start_emitter plays slot i on voice pair 2i, the
            // effect channels (2i ^ 8) and the next.
            scene.emitters.push(Emitter { pair: ((slot * 2) ^ 8) as u8, sound: u32::from(sound), full_id: false, position: at, follows: u32::from(actor) });
        }
    }
}

/// The arena's camera, player Gear and positional voices.
fn read_arena(memory: &dyn GameMemory, scene: &mut Scene) {
    scene.listener = vector_at(memory, ARENA_PLAYER);
    scene.camera = u32_at(memory, ARENA_PLAYER + 0x60).and_then(ram).and_then(|node| rotation_at(memory, node + 0x0C));
    for i in 0..4 {
        let voice = ARENA_SOUND_VOICES + i * 0x24;
        let Some(head) = read::<4>(memory, voice) else { continue };
        let (mode, active, channel) = (head[0], head[1], head[2]);
        if active == 0 || mode == 0 {
            continue;
        }
        let position = match mode {
            1 => vector_at(memory, voice + 0x10),
            _ => u32_at(memory, voice + 0x20).and_then(ram).and_then(|follow| vector_at(memory, follow)),
        };
        let (Some(position), Some(sound)) = (position, u32_at(memory, voice + 8)) else { continue };
        scene.emitters.push(Emitter { pair: (channel & 0xFE) ^ 8, sound, full_id: true, position, follows: i });
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::OutOfBounds;

    /// Game RAM for the driver's records.
    struct Ram(Vec<u8>);

    impl Ram {
        fn new() -> Self {
            Ram(vec![0; 0x20_0000])
        }
        fn put(&mut self, address: u32, bytes: &[u8]) {
            let at = (address - 0x8000_0000) as usize;
            self.0[at..at + bytes.len()].copy_from_slice(bytes);
        }
        fn word(&mut self, address: u32, value: u32) {
            self.put(address, &value.to_le_bytes());
        }
    }

    impl GameMemory for Ram {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            let at = address.checked_sub(0x8000_0000).ok_or(OutOfBounds { address, len: out.len() })? as usize;
            out.copy_from_slice(self.0.get(at..at + out.len()).ok_or(OutOfBounds { address, len: out.len() })?);
            Ok(())
        }
        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            let at = (address - 0x8000_0000) as usize;
            self.0[at..at + data.len()].copy_from_slice(data);
            Ok(())
        }
    }

    const EFFECTS: u32 = 0x8010_0000;
    const MUSIC: u32 = 0x8011_0000;
    const HEADER: u32 = 0x8012_0000;

    /// A driver with 16 effect channels and a four-part song: part 0 a busy
    /// high melody, 1 a low line, 2 drums, 3 long notes panned left.
    fn driver() -> Ram {
        let mut ram = Ram::new();
        ram.word(EFFECT_CHANNELS, EFFECTS);
        ram.word(EFFECT_CHANNEL_COUNT, 16);
        ram.word(PLAYING_SEQS, MUSIC);
        ram.word(MUSIC, EFFECTS);
        ram.word(EFFECTS, 0);
        ram.word(MUSIC + SEQ_HEADER, HEADER);
        ram.put(MUSIC + SEQ_CHANNELS, &[4]);
        let parts: [Vec<u8>; 4] = [
            // octave 6, 40 sixteenth notes
            [vec![0x94, 6], (0..40).flat_map(|i| [100, 19 * (i % 12) + 11]).collect(), vec![0x90]].concat(),
            [vec![0x94, 3], (0..20).flat_map(|_| [90, 19 * 2 + 6]).collect(), vec![0x90]].concat(),
            [vec![0xAE], (0..30).flat_map(|_| [110, 19 * 1 + 9]).collect(), vec![0x90]].concat(),
            [vec![0xE8, 0x20, 0x94, 5], (0..8).flat_map(|_| [70, 19 * 4 + 1]).collect(), vec![0x90]].concat(),
        ];
        let mut at = 0x40u16;
        for (i, data) in parts.iter().enumerate() {
            ram.put(HEADER + HEADER_CHANNEL_OFFSETS + 2 * i as u32, &at.to_le_bytes());
            ram.put(HEADER + u32::from(at), data);
            at += data.len() as u16 + 4;
        }
        ram
    }

    fn own(ram: &mut Ram, voice: u32, seq: u32, channel: u32) -> u32 {
        let state = seq + SEQ_CHANNEL_ARRAY + channel * CHANNEL_SIZE + CHANNEL_STATE;
        ram.word(VOICE_OWNERS + 4 * voice, state);
        state
    }

    fn effect(ram: &mut Ram, voice: u32, channel: u32, id: u32, stamp: u32) {
        own(ram, voice, EFFECTS, channel);
        let record = EFFECTS + SEQ_CHANNEL_ARRAY + channel * CHANNEL_SIZE;
        ram.word(record + CHANNEL_ID, id);
        ram.word(record + CHANNEL_STAMP, stamp);
    }

    fn on() -> Config {
        Config { enabled: true, ..Config::default() }
    }

    #[test]
    fn voices_are_known_by_the_driver_records_that_keyed_them() {
        let mut ram = driver();
        own(&mut ram, 0, MUSIC, 2);
        effect(&mut ram, 22, 14, 0x0004_0021, 77);
        effect(&mut ram, 23, 15, 0x0004_0021, 77);
        ram.word(VOICE_OWNERS + 4 * 5, 0x8015_0000);
        let mut presenter = Presenter::new(on());
        presenter.key_on((1 << 0) | (1 << 22) | (1 << 23) | (1 << 5), &ram);
        assert!(matches!(presenter.tags[0], Some(Tag::Music { seq: MUSIC, part: 2, .. })));
        assert_eq!(presenter.tags[22], Some(Tag::Effect { channel: 14, id: 0x0004_0021, stamp: 77 }));
        assert_eq!(presenter.tags[5], Some(Tag::Other { owner: 0x8015_0000 }));
        // Music is placed; an effect without an emitter keeps its mix and is
        // counted as a gap; so does a voice nobody owns.
        assert!(matches!(presenter.renderer.route(0), Route::Source(_)));
        assert_eq!(presenter.renderer.route(22), Route::Original);
        assert_eq!(presenter.renderer.route(5), Route::Original);
        assert_eq!(presenter.unresolved.values().sum::<u64>(), 2, "both channels of the effect start");
    }

    #[test]
    fn songs_are_profiled_from_their_data_and_arranged() {
        let ram = driver();
        let mut presenter = Presenter::new(on());
        let song = presenter.song(MUSIC, &ram);
        let roles: Vec<music::Role> = presenter.songs[&MUSIC].placements.iter().map(|p| p.role).collect();
        assert_eq!(roles, [music::Role::Lead, music::Role::Bass, music::Role::Rhythm, music::Role::Pad]);
        assert_eq!(presenter.songs[&MUSIC].placements[3].home_pan, 8192.0, "the pan before its first note");
        assert_eq!(presenter.song(MUSIC, &ram), song, "the same data is the same song");
    }

    #[test]
    fn a_field_emitter_is_heard_from_its_actor_in_the_cameras_orientation() {
        let mut ram = driver();
        // A camera looking along the world's +z (rows: right, down, forward).
        let rows: [i16; 9] = [4096, 0, 0, 0, 4096, 0, 0, 0, 4096];
        ram.put(FIELD_VIEW_SCALED_WORLD, &rows.iter().flat_map(|r| r.to_le_bytes()).collect::<Vec<u8>>());
        let descriptors = 0x8013_0000;
        ram.word(FIELD_COMPONENTS, 3);
        ram.word(FIELD_COMPONENTS + 4, descriptors);
        ram.word(FIELD_CONTROLLED, 0);
        let place = |ram: &mut Ram, index: u32, at: [i32; 3]| {
            ram.put(descriptors + index * FIELD_DESCRIPTOR_SIZE + 0x20, &at.iter().flat_map(|c| c.to_le_bytes()).collect::<Vec<u8>>());
        };
        place(&mut ram, 0, [100, 0, 100]);
        place(&mut ram, 2, [300, 0, 100]);
        // Slot 1 follows actor 2 with sound 0x21 (on channels 10 and 11).
        for slot in 0..3u32 {
            let (actor, sound) = if slot == 1 { (2u16, 0x21u16) } else { (0xFFFF, 0xFFFF) };
            ram.put(FIELD_EMITTER_SLOTS + slot * 6, &[actor.to_le_bytes(), sound.to_le_bytes()].concat());
        }
        effect(&mut ram, 10, 10, 0x21, 5);
        let mut presenter = Presenter::new(on());
        presenter.update_scene(&ram, Some(MODE_FIELD), false);
        presenter.key_on(1 << 10, &ram);
        let Route::Source(id) = presenter.renderer.route(10) else { panic!("placed") };
        match presenter.place_effect(presenter.tags[10].unwrap()) {
            Placing::Placed { direction, follows, .. } => {
                assert_eq!(follows, 2);
                assert!(direction[0] > 0.99, "to the right of the character: {direction:?}");
            }
            other => panic!("{other:?}"),
        }
        // The camera turns half round: the emitter is now on the left,
        // whatever the character faces; its distance never matters.
        let turned: [i16; 9] = [-4096, 0, 0, 0, 4096, 0, 0, 0, -4096];
        ram.put(FIELD_VIEW_SCALED_WORLD, &turned.iter().flat_map(|r| r.to_le_bytes()).collect::<Vec<u8>>());
        place(&mut ram, 2, [2100, 0, 100]);
        presenter.update_scene(&ram, Some(MODE_FIELD), false);
        assert_eq!(presenter.renderer.route(10), Route::Source(id), "the same source");
        match presenter.place_effect(presenter.tags[10].unwrap()) {
            Placing::Placed { direction, .. } => assert!(direction[0] < -0.99, "{direction:?}"),
            other => panic!("{other:?}"),
        }
        // First-person, looking a quarter to the left: it is ahead.
        presenter.set_view(Some([[0.0, 0.0, 1.0], [0.0, 1.0, 0.0], [-1.0, 0.0, 0.0]]));
        match presenter.place_effect(presenter.tags[10].unwrap()) {
            Placing::Placed { direction, .. } => assert!(direction[2] > 0.99, "{direction:?}"),
            other => panic!("{other:?}"),
        }
        // Positional effects off: the sounding effect fades back to its
        // original mix; its next start is mixed so from the first sample.
        presenter.configure(Config { positional_effects: false, ..on() });
        assert_eq!(presenter.renderer.route(10), Route::Source(id), "it fades rather than jumps");
        let mut frame = TapFrame { main: [0x7FFE, 0x7FFE], ..Default::default() };
        frame.voice[10] = 1000;
        frame.volume[10] = [0x2000, 0x2000];
        let mut ring = std::collections::VecDeque::new();
        presenter.render(&vec![frame; 44_100 / 10], &mut ring);
        let placed = presenter.renderer.sources().iter().find(|s| s.id == id).map(|s| s.placed);
        assert_eq!(placed, Some(0.0), "back in the original mix within 100 ms");
        effect(&mut ram, 10, 10, 0x21, 6);
        presenter.key_on(1 << 10, &ram);
        assert_eq!(presenter.renderer.route(10), Route::Original);
    }

    /// The field of `a_field_emitter_...` with the camera's rotation `rows`
    /// and translation `t`, the character at `listener` and slot 0's emitter
    /// (actor 1, sound 0x30) at `emitter`; the direction it is heard from.
    fn field_direction(rows: [i16; 9], t: [i32; 3], listener: [i32; 3], emitter: [i32; 3]) -> [f32; 3] {
        let mut ram = driver();
        let mut matrix: Vec<u8> = rows.iter().flat_map(|r| r.to_le_bytes()).collect();
        matrix.extend([0, 0]);
        matrix.extend(t.iter().flat_map(|c| c.to_le_bytes()));
        ram.put(FIELD_VIEW_SCALED_WORLD, &matrix);
        let descriptors = 0x8013_0000;
        ram.word(FIELD_COMPONENTS, 2);
        ram.word(FIELD_COMPONENTS + 4, descriptors);
        ram.word(FIELD_CONTROLLED, 0);
        for (index, at) in [listener, emitter].iter().enumerate() {
            ram.put(descriptors + index as u32 * FIELD_DESCRIPTOR_SIZE + 0x20, &at.iter().flat_map(|c| c.to_le_bytes()).collect::<Vec<u8>>());
        }
        for slot in 0..3u32 {
            let (actor, sound) = if slot == 0 { (1u16, 0x30u16) } else { (0xFFFF, 0xFFFF) };
            ram.put(FIELD_EMITTER_SLOTS + slot * 6, &[actor.to_le_bytes(), sound.to_le_bytes()].concat());
        }
        effect(&mut ram, 8, 8, 0x30, 9);
        let mut presenter = Presenter::new(on());
        presenter.update_scene(&ram, Some(MODE_FIELD), false);
        presenter.key_on(1 << 8, &ram);
        match presenter.place_effect(presenter.tags[8].unwrap()) {
            Placing::Placed { direction, .. } => direction,
            other => panic!("{other:?}"),
        }
    }

    #[test]
    fn distance_zoom_and_scale_change_nothing_heard() {
        let level: [i16; 9] = [4096, 0, 0, 0, 4096, 0, 0, 0, 4096];
        let near = field_direction(level, [0, 0, 1000], [0, 0, 0], [30, -10, 40]);
        // Ten times as far the same way: the same direction (the game's own
        // volume carries the distance).
        assert_eq!(field_direction(level, [0, 0, 1000], [0, 0, 0], [300, -100, 400]), near);
        // The camera pulled back or zoomed (its translation; a scaled
        // matrix): the same direction.
        assert_eq!(field_direction(level, [0, 0, 9000], [0, 0, 0], [30, -10, 40]), near);
        let scaled: [i16; 9] = [2048, 0, 0, 0, 2048, 0, 0, 0, 2048];
        assert_eq!(field_direction(scaled, [0, 0, 1000], [0, 0, 0], [30, -10, 40]), near);
        // The character moving along with its emitter: the same direction.
        assert_eq!(field_direction(level, [0, 0, 1000], [500, 0, 500], [530, -10, 540]), near);
        // The emitter at the character: none made up (kept).
        assert_eq!(field_direction(level, [0, 0, 1000], [7, 7, 7], [7, 7, 7]), [0.0; 3]);
    }

    #[test]
    fn the_arenas_positional_voices_are_heard_from_the_player_gear() {
        let mut ram = driver();
        // The player's Gear at the origin, its node's view the camera: a
        // quarter turn (the world's +x ahead).
        ram.put(ARENA_PLAYER, &[0i32, 0, 0].iter().flat_map(|c| c.to_le_bytes()).collect::<Vec<u8>>());
        let node = 0x8014_0000;
        ram.word(ARENA_PLAYER + 0x60, node);
        let rows: [i16; 9] = [0, 0, -4096, 0, 4096, 0, 4096, 0, 0];
        ram.put(node + 0x0C, &rows.iter().flat_map(|r| r.to_le_bytes()).collect::<Vec<u8>>());
        // Voice 1: mode 1 (a fixed point) at +x, effect 0x60005 on voice pair
        // 2 (effect channels 10 and 11).
        let voice = ARENA_SOUND_VOICES + 0x24;
        ram.put(voice, &[1, 1, 2, 0]);
        ram.word(voice + 8, 0x0006_0005);
        ram.put(voice + 0x10, &[500i32, 0, 0].iter().flat_map(|c| c.to_le_bytes()).collect::<Vec<u8>>());
        effect(&mut ram, 10, 10, 0x0006_0005, 3);
        let mut presenter = Presenter::new(on());
        presenter.update_scene(&ram, Some(MODE_ARENA), false);
        presenter.key_on(1 << 10, &ram);
        match presenter.place_effect(presenter.tags[10].unwrap()) {
            Placing::Placed { direction, emitter, follows } => {
                assert_eq!((emitter, follows), ("arena voice", 1));
                assert!(direction[2] > 0.999, "ahead of the camera: {direction:?}");
            }
            other => panic!("{other:?}"),
        }
        // An unpositioned arena voice (mode 0) stays in its original mix.
        ram.put(voice, &[0, 1, 2, 0]);
        effect(&mut ram, 10, 10, 0x0006_0005, 4);
        presenter.key_on(1 << 10, &ram);
        assert_eq!(presenter.renderer.route(10), Route::Original);
    }

    #[test]
    fn a_head_turned_right_hears_the_screen_on_its_left() {
        assert_eq!(head_axes([0.0, 0.0, 0.0, 1.0]), [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]);
        // A quarter turn right: -90 degrees about +Y.
        let half = (-std::f32::consts::FRAC_PI_4).sin_cos();
        let axes = head_axes([0.0, half.0, 0.0, half.1]);
        let ahead = apply(&axes, [0.0, 0.0, 1.0]);
        assert!(ahead[0] < -0.999, "the screen's front is on the left: {ahead:?}");
    }

    #[test]
    fn a_restore_forgets_identities_until_the_voices_are_read_again() {
        let mut ram = driver();
        own(&mut ram, 3, MUSIC, 0);
        let mut presenter = Presenter::new(on());
        presenter.key_on(1 << 3, &ram);
        assert!(!presenter.wants_retag());
        presenter.reset();
        assert!(presenter.wants_retag());
        assert_eq!(presenter.tags[3], None);
        assert_eq!(presenter.renderer.route(3), Route::Original);
        presenter.retag(1 << 3, &ram);
        assert!(matches!(presenter.renderer.route(3), Route::Source(_)));
    }
}
