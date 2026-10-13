//! The block renderer (the crate's documentation says what it mixes).

use crate::fft::{Complex, Fft};
use crate::hrtf::{self, BINS, Biquad, BiquadState, FFT_SIZE, Filter, Hrtf};
use crate::{BLOCK, SAMPLE_RATE, pan};
use xem_spu::{TapFrame, VOICES};

/// How long a direction takes to glide most (1 - 1/e) of the way to a new one.
const GLIDE_SECONDS: f32 = 0.03;
/// How long a source takes to move between its placement and the original mix.
const SOURCE_FADE_SECONDS: f32 = 0.05;
/// How long the whole presentation takes to come in or go.
const MIX_FADE_SECONDS: f32 = 0.1;
/// A filter is remade once its direction moved further than this (degrees).
const FILTER_STEP_DEGREES: f32 = 0.5;
/// The output's headroom: above this level (-1.4 dBFS) a peak is compressed
/// smoothly into full scale instead of clipping.
const KNEE: f32 = 0.85;
/// Blocks a source with no voice left stays before it is forgotten.
const IDLE_BLOCKS: u32 = 4;
/// The premixed-music estimate's virtual loudspeakers: the original pair
/// ahead, and where its side ambience spreads (azimuth, degrees).
const FRONT_SPEAKERS: f32 = 30.0;
const REAR_SPEAKERS: f32 = 110.0;
/// The share of the side signal the estimate moves behind at full immersion.
const MAX_REAR_SHARE: f32 = 0.6;
/// Delays of the rear speakers (samples), unequal so they do not image.
const REAR_DELAYS: [usize; 2] = [441, 617];

/// Where a source is heard.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Target {
    /// A direction in the listener's space (right, down, forward), of any
    /// length; zero keeps the direction it had (a source at the listener).
    Listener([f32; 3]),
    /// A place on the musical stage, in degrees, for the source's voices at
    /// `home_pan`; their pan away from it turns the place by `spread`
    /// degrees per 0x4000 (the music's own pan movement), and the stage's
    /// rotation ([`Renderer::set_stage_rotation`]) turns it all.
    Stage { azimuth: f32, elevation: f32, home_pan: f32, spread: f32 },
}

/// How a voice is mixed.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum Route {
    /// As the SPU mixed it, with its own volumes.
    #[default]
    Original,
    /// Placed as source `id` ([`Renderer::set_source`]).
    Source(u32),
}

/// What the CD input is.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub enum CdRoute {
    /// Mixed as the SPU mixed it (movies, voices, anything not music).
    #[default]
    Original,
    /// Premixed music: an estimated arrangement, the original pair kept
    /// ahead and its side ambience spread behind as far as `immersion`
    /// (0 to 1) says.
    Upmix { immersion: f32 },
}

/// A source as the renderer has it (diagnostics).
#[derive(Clone, Debug, PartialEq)]
pub struct SourceStatus {
    pub id: u32,
    /// Where it is heard now: azimuth clockwise from ahead and elevation,
    /// degrees.
    pub azimuth: f32,
    pub elevation: f32,
    /// How far it is placed (1) rather than mixed as the original (0).
    pub placed: f32,
    /// Its gain before panning, as the driver resolved it (the largest of
    /// its voices', last block).
    pub gain: f32,
    /// The level of its signal in the last block (RMS, dB of full scale;
    /// none when silent).
    pub level_db: Option<f32>,
    pub voices: u32,
}

struct Source {
    id: u32,
    target: Target,
    /// Placed (1) or back to the original mix (0).
    placed: bool,
    amount: f32,
    /// The direction heard now, unit length, listener space.
    current: Option<[f32; 3]>,
    /// The direction the filter was made for.
    filtered: Option<[f32; 3]>,
    filter: Filter,
    input: [f32; BLOCK],
    /// The gain-weighted pan of its voices in this block.
    pan_sum: f32,
    pan_weight: f32,
    /// The largest pre-pan gain of its voices in this block.
    gain: f32,
    last_gain: f32,
    level: f32,
    idle: u32,
}

impl Source {
    fn new(id: u32, target: Target, placed: bool) -> Self {
        Source {
            id,
            target,
            placed,
            amount: if placed { 1.0 } else { 0.0 },
            current: None,
            filtered: None,
            filter: Filter::default(),
            input: [0.0; BLOCK],
            pan_sum: 0.0,
            pan_weight: 0.0,
            gain: 0.0,
            last_gain: 0.0,
            level: 0.0,
            idle: 0,
        }
    }
}

fn normalize(v: [f32; 3]) -> Option<[f32; 3]> {
    let length = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    (length > 1e-6 && length.is_finite()).then(|| v.map(|c| c / length))
}

fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

/// A sample after the output's soft knee: above [`KNEE`] the excess `u`
/// (in units of the headroom) becomes `u / (1 + u)` of it, so the level
/// keeps rising, ever more slowly, and never reaches full scale.
fn knee(y: f32) -> f32 {
    let magnitude = y.abs();
    if magnitude <= KNEE {
        return y;
    }
    let excess = (magnitude - KNEE) / (1.0 - KNEE);
    (KNEE + (1.0 - KNEE) * excess / (1.0 + excess)).copysign(y)
}

/// The step that moves a fade of `seconds` along in one block.
fn block_step(seconds: f32) -> f32 {
    BLOCK as f32 / (seconds * SAMPLE_RATE as f32)
}

/// The SPU's voices mixed again, placed where the caller says.
pub struct Renderer {
    hrtf: &'static Hrtf,
    fft: Fft,
    routes: [Route; VOICES],
    /// Each voice's source, by index into `sources`.
    slots: [Option<usize>; VOICES],
    /// Each voice's volumes, and what they say before the pan law.
    unpanned: [([i16; 2], Option<pan::Unpanned>); VOICES],
    sources: Vec<Source>,
    /// What keeps the original mix: voices not placed, the reverb return,
    /// the CD input unless spread.
    direct: [[f32; BLOCK]; 2],
    /// The SPU's own output, for the crossfades and the bypass.
    original: [[i16; BLOCK]; 2],
    /// The CD input, for the premixed-music estimate.
    cd: [[f32; BLOCK]; 2],
    cd_route: CdRoute,
    speakers: [Filter; 4],
    rear_line: [Vec<f32>; 2],
    rear_at: usize,
    /// The placed sources' amounts at the block's start.
    amount_start: Vec<f32>,
    filled: usize,
    tail: [[f32; BLOCK]; 2],
    /// The bass shelf the placed sound passes ([`Hrtf::bass_shelf`]).
    shelf: Biquad,
    bass: [BiquadState; 2],
    mix: f32,
    mix_target: f32,
    stage: [[f32; 3]; 3],
    buffer: Vec<Complex>,
    ears: [[Complex; BINS]; 2],
}

impl Default for Renderer {
    fn default() -> Self {
        Self::new()
    }
}

impl Renderer {
    /// A renderer at rest: presenting the original mix until
    /// [`set_enabled`](Self::set_enabled).
    pub fn new() -> Self {
        let hrtf = Hrtf::kemar();
        let mut speakers: [Filter; 4] = Default::default();
        for (filter, azimuth) in speakers.iter_mut().zip([-FRONT_SPEAKERS, FRONT_SPEAKERS, -REAR_SPEAKERS, REAR_SPEAKERS]) {
            hrtf.filter(azimuth, 0.0, filter);
        }
        Renderer {
            hrtf,
            fft: Fft::new(FFT_SIZE),
            routes: [Route::Original; VOICES],
            slots: [None; VOICES],
            unpanned: [([0, 0], Some(pan::Unpanned { gain: 0.0, pan: 16384.0 })); VOICES],
            sources: Vec::new(),
            direct: [[0.0; BLOCK]; 2],
            original: [[0; BLOCK]; 2],
            cd: [[0.0; BLOCK]; 2],
            cd_route: CdRoute::Original,
            speakers,
            rear_line: [vec![0.0; REAR_DELAYS[0] + 1], vec![0.0; REAR_DELAYS[1] + 1]],
            rear_at: 0,
            amount_start: Vec::new(),
            filled: 0,
            tail: [[0.0; BLOCK]; 2],
            shelf: hrtf.bass_shelf(),
            bass: Default::default(),
            mix: 0.0,
            mix_target: 0.0,
            stage: [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]],
            buffer: vec![Complex::ZERO; FFT_SIZE],
            ears: [[Complex::ZERO; BINS]; 2],
        }
    }

    /// Forgets the sound so far (a restored state, a new timeline): the
    /// block under way, the filters' tails, the sources, their glides and
    /// the voices' routes; the presentation is at its target at once.
    pub fn reset(&mut self) {
        self.sources.clear();
        self.slots = [None; VOICES];
        self.routes = [Route::Original; VOICES];
        self.direct = [[0.0; BLOCK]; 2];
        self.original = [[0; BLOCK]; 2];
        self.cd = [[0.0; BLOCK]; 2];
        self.rear_line.iter_mut().for_each(|line| line.fill(0.0));
        self.filled = 0;
        self.tail = [[0.0; BLOCK]; 2];
        self.bass = Default::default();
        self.mix = self.mix_target;
    }

    /// Present the placed mix (`true`) or the SPU's own (`false`, the
    /// bypass); either way it crossfades.
    pub fn set_enabled(&mut self, enabled: bool) {
        self.mix_target = if enabled { 1.0 } else { 0.0 };
    }

    /// Whether only the SPU's own mix is heard and nothing is fading.
    pub fn presenting_original(&self) -> bool {
        self.mix == 0.0 && self.mix_target == 0.0
    }

    /// Samples of the block under way (the renderer's delay).
    pub fn pending(&self) -> usize {
        self.filled
    }

    /// Appends the block under way as the SPU mixed it and starts the next
    /// (presentation ending: exact once [`presenting_original`](Self::presenting_original)).
    pub fn flush_original(&mut self, out: &mut Vec<i16>) {
        for n in 0..self.filled {
            out.extend([self.original[0][n], self.original[1][n]]);
        }
        self.direct = [[0.0; BLOCK]; 2];
        self.cd = [[0.0; BLOCK]; 2];
        for source in &mut self.sources {
            source.input = [0.0; BLOCK];
        }
        self.filled = 0;
    }

    /// The rotation of the musical stage: the listener's axes (right, down,
    /// forward) in the stage's space (head-tracked music; identity keeps the
    /// stage with the head).
    pub fn set_stage_rotation(&mut self, rotation: [[f32; 3]; 3]) {
        self.stage = rotation;
    }

    pub fn set_cd_route(&mut self, route: CdRoute) {
        self.cd_route = route;
    }

    pub fn route(&self, voice: usize) -> Route {
        self.routes[voice]
    }

    /// Mix `voice` as `route` says from its next sample (a source not set
    /// yet starts ahead of the listener until [`set_source`](Self::set_source)).
    pub fn set_route(&mut self, voice: usize, route: Route) {
        self.routes[voice] = route;
        self.slots[voice] = match route {
            Route::Original => None,
            Route::Source(id) => Some(self.source_index(id, Target::Listener([0.0, 0.0, 1.0]), true)),
        };
    }

    /// Where source `id` is heard, and whether it is placed there (`false`
    /// fades its voices back to their original mix). A new source starts
    /// where `placed` says, at once (its voices are starting).
    pub fn set_source(&mut self, id: u32, target: Target, placed: bool) {
        let index = self.source_index(id, target, placed);
        let source = &mut self.sources[index];
        source.target = target;
        source.placed = placed;
    }

    /// Whether source `id` is placed (`false` fades it back to the original
    /// mix), where it was.
    pub fn set_placed(&mut self, id: u32, placed: bool) {
        if let Some(source) = self.sources.iter_mut().find(|s| s.id == id) {
            source.placed = placed;
        }
    }

    /// As [`set_source`](Self::set_source) placed, but a new source fades in
    /// from the original mix (its voices are already sounding).
    pub fn fade_in_source(&mut self, id: u32, target: Target) {
        let index = self.source_index(id, target, false);
        let source = &mut self.sources[index];
        source.target = target;
        source.placed = true;
    }

    fn source_index(&mut self, id: u32, target: Target, placed: bool) -> usize {
        match self.sources.iter().position(|s| s.id == id) {
            Some(index) => index,
            None => {
                self.sources.push(Source::new(id, target, placed));
                self.sources.len() - 1
            }
        }
    }

    /// The sources heard now.
    pub fn sources(&self) -> Vec<SourceStatus> {
        self.sources
            .iter()
            .enumerate()
            .map(|(index, source)| {
                let (azimuth, elevation) = source.current.map_or((0.0, 0.0), hrtf::angles);
                SourceStatus {
                    id: source.id,
                    azimuth,
                    elevation,
                    placed: source.amount,
                    gain: source.last_gain,
                    level_db: (source.level > 1e-6).then(|| 20.0 * source.level.log10()),
                    voices: self.slots.iter().filter(|slot| **slot == Some(index)).count() as u32,
                }
            })
            .collect()
    }

    /// The direction source `index` is heading for, unit length.
    fn target_direction(&self, index: usize) -> Option<[f32; 3]> {
        let source = &self.sources[index];
        match source.target {
            Target::Listener(direction) => normalize(direction),
            Target::Stage { azimuth, elevation, home_pan, spread } => {
                let pan = if source.pan_weight > 0.0 { source.pan_sum / source.pan_weight } else { home_pan };
                let turned = azimuth + (pan - home_pan) / 16384.0 * spread;
                let stage = hrtf::direction(turned, elevation);
                let r = &self.stage;
                normalize([dot(r[0], stage), dot(r[1], stage), dot(r[2], stage)])
            }
        }
    }

    /// Mixes one sample; a completed block's samples are appended to `out`
    /// (interleaved left, right).
    pub fn push(&mut self, frame: &TapFrame, out: &mut Vec<i16>) {
        if self.filled == 0 {
            self.amount_start.clear();
            let step = block_step(SOURCE_FADE_SECONDS);
            for source in &mut self.sources {
                self.amount_start.push(source.amount);
                let target = if source.placed { 1.0 } else { 0.0 };
                source.amount = if source.amount < target { (source.amount + step).min(target) } else { (source.amount - step).max(target) };
            }
        }
        let n = self.filled;
        let t = n as f32 / BLOCK as f32;
        let main = frame.main.map(|m| f32::from(m) / 32768.0);
        let mono_main = main[0].abs().max(main[1].abs());
        for e in 0..2 {
            self.original[e][n] = frame.output[e];
        }
        if !frame.muted {
            for v in 0..VOICES {
                let sample = frame.voice[v];
                if sample == 0 {
                    continue;
                }
                let s = f32::from(sample) / 32768.0;
                let volume = frame.volume[v];
                let direct = [s * f32::from(volume[0]) / 32768.0 * main[0], s * f32::from(volume[1]) / 32768.0 * main[1]];
                let slot = self.slots[v];
                let unpanned = match slot {
                    Some(_) => {
                        if self.unpanned[v].0 != volume {
                            self.unpanned[v] = (volume, pan::unpan(volume));
                        }
                        self.unpanned[v].1
                    }
                    None => None,
                };
                match (slot, unpanned) {
                    (Some(index), Some(unpanned)) => {
                        let source = &mut self.sources[index];
                        let start = self.amount_start.get(index).copied().unwrap_or(source.amount);
                        let amount = start + (source.amount - start) * t;
                        source.input[n] += s * unpanned.gain * mono_main * amount;
                        source.pan_sum += unpanned.pan * unpanned.gain;
                        source.pan_weight += unpanned.gain;
                        source.gain = source.gain.max(unpanned.gain);
                        for e in 0..2 {
                            self.direct[e][n] += direct[e] * (1.0 - amount);
                        }
                    }
                    _ => {
                        for e in 0..2 {
                            self.direct[e][n] += direct[e];
                        }
                    }
                }
            }
            for e in 0..2 {
                self.direct[e][n] += f32::from(frame.reverb[e]) / 32768.0 * main[e];
            }
        }
        for e in 0..2 {
            let cd = frame.cd[e] as f32 / 32768.0 * main[e];
            match self.cd_route {
                CdRoute::Original => self.direct[e][n] += cd,
                CdRoute::Upmix { .. } => self.cd[e][n] = cd,
            }
        }
        self.filled += 1;
        if self.filled == BLOCK {
            self.finish_block(out);
        }
    }

    /// Adds the product of `input`'s spectrum and `filter` to the ears.
    fn accumulate(ears: &mut [[Complex; BINS]; 2], spectrum: &[Complex], filter: &Filter) {
        for e in 0..2 {
            for (k, bin) in ears[e].iter_mut().enumerate() {
                *bin = bin.add(spectrum[k].mul(filter.ears[e][k]));
            }
        }
    }

    /// The spectra of two real blocks `a` and `b` at once, into `first` and
    /// `second` (bins 0 to BINS - 1).
    fn spectra_of_two(fft: &Fft, buffer: &mut [Complex], a: &[f32; BLOCK], b: Option<&[f32; BLOCK]>, first: &mut [Complex; BINS], second: &mut [Complex; BINS]) {
        for (t, slot) in buffer.iter_mut().enumerate() {
            *slot = if t < BLOCK { Complex::new(a[t], b.map_or(0.0, |b| b[t])) } else { Complex::ZERO };
        }
        fft.forward(buffer);
        for k in 0..BINS {
            let z = buffer[k];
            let mirror = buffer[(FFT_SIZE - k) % FFT_SIZE].conj();
            first[k] = z.add(mirror).scale(0.5);
            // (z - mirror) / 2i
            let d = z.sub(mirror);
            second[k] = Complex::new(d.im * 0.5, -d.re * 0.5);
        }
    }

    fn finish_block(&mut self, out: &mut Vec<i16>) {
        // Directions glide, filters follow.
        let glide = 1.0 - (-block_step(GLIDE_SECONDS)).exp();
        let filter_step = FILTER_STEP_DEGREES.to_radians().cos();
        for index in 0..self.sources.len() {
            let target = self.target_direction(index);
            let source = &mut self.sources[index];
            source.current = match (source.current, target) {
                (None, target) => Some(target.unwrap_or([0.0, 0.0, 1.0])),
                (Some(current), Some(target)) => {
                    let moved = [0, 1, 2].map(|i| current[i] + (target[i] - current[i]) * glide);
                    Some(normalize(moved).unwrap_or(target))
                }
                (current, None) => current,
            };
            let current = source.current.expect("set above");
            if source.filtered.is_none_or(|made| dot(made, current) < filter_step) {
                let (azimuth, elevation) = hrtf::angles(current);
                self.hrtf.filter(azimuth, elevation, &mut source.filter);
                source.filtered = Some(current);
            }
        }
        // Convolve the sources heard in this block, two to a transform.
        self.ears = [[Complex::ZERO; BINS]; 2];
        let heard: Vec<usize> = (0..self.sources.len()).filter(|&i| self.sources[i].input.iter().any(|s| *s != 0.0)).collect();
        let (mut first, mut second) = ([Complex::ZERO; BINS], [Complex::ZERO; BINS]);
        for pair in heard.chunks(2) {
            let b = pair.get(1).map(|&i| &self.sources[i].input);
            Self::spectra_of_two(&self.fft, &mut self.buffer, &self.sources[pair[0]].input, b, &mut first, &mut second);
            Self::accumulate(&mut self.ears, &first, &self.sources[pair[0]].filter);
            if let Some(&i) = pair.get(1) {
                Self::accumulate(&mut self.ears, &second, &self.sources[i].filter);
            }
        }
        if let CdRoute::Upmix { immersion } = self.cd_route {
            self.upmix(immersion.clamp(0.0, 1.0), &mut first, &mut second);
        }
        // Both ears back at once: left real, right imaginary.
        for k in 0..BINS {
            let (l, r) = (self.ears[0][k], self.ears[1][k]);
            self.buffer[k] = Complex::new(l.re - r.im, l.im + r.re);
            if k > 0 && k < FFT_SIZE / 2 {
                let (l, r) = (l.conj(), r.conj());
                self.buffer[FFT_SIZE - k] = Complex::new(l.re - r.im, l.im + r.re);
            }
        }
        self.fft.inverse(&mut self.buffer);
        let mix_step = 1.0 / (MIX_FADE_SECONDS * SAMPLE_RATE as f32);
        for n in 0..BLOCK {
            let wet = [
                self.bass[0].run(&self.shelf, self.buffer[n].re + self.tail[0][n]),
                self.bass[1].run(&self.shelf, self.buffer[n].im + self.tail[1][n]),
            ];
            self.tail[0][n] = self.buffer[n + BLOCK].re;
            self.tail[1][n] = self.buffer[n + BLOCK].im;
            self.mix = if self.mix < self.mix_target { (self.mix + mix_step).min(self.mix_target) } else { (self.mix - mix_step).max(self.mix_target) };
            for e in 0..2 {
                if self.mix == 0.0 {
                    // The original mix itself, sample for sample.
                    out.push(self.original[e][n]);
                    continue;
                }
                let placed = wet[e] + self.direct[e][n];
                let y = f32::from(self.original[e][n]) / 32768.0 * (1.0 - self.mix) + placed * self.mix;
                out.push((knee(y) * 32768.0).round().clamp(-32768.0, 32767.0) as i16);
            }
        }
        // Levels for the diagnostics; forget sources without voices.
        for source in &mut self.sources {
            let energy: f32 = source.input.iter().map(|s| s * s).sum();
            source.level = (energy / BLOCK as f32).sqrt();
            source.last_gain = source.gain;
            source.gain = 0.0;
            source.input = [0.0; BLOCK];
            source.pan_sum = 0.0;
            source.pan_weight = 0.0;
        }
        for (index, source) in self.sources.iter_mut().enumerate() {
            let routed = self.slots.contains(&Some(index));
            source.idle = if routed { 0 } else { source.idle + 1 };
        }
        if self.sources.iter().any(|s| s.idle > IDLE_BLOCKS) {
            let ids: Vec<Option<u32>> = self.slots.iter().map(|slot| slot.map(|i| self.sources[i].id)).collect();
            self.sources.retain(|s| s.idle <= IDLE_BLOCKS);
            for (v, id) in ids.into_iter().enumerate() {
                self.slots[v] = id.and_then(|id| self.sources.iter().position(|s| s.id == id));
            }
        }
        self.direct = [[0.0; BLOCK]; 2];
        self.cd = [[0.0; BLOCK]; 2];
        self.filled = 0;
    }

    /// The premixed-music estimate: the CD pair as two loudspeakers ahead,
    /// part of its side (left minus right) signal moved to two behind,
    /// delayed so they stay diffuse; the mid signal and the side's energy
    /// are kept.
    fn upmix(&mut self, immersion: f32, first: &mut [Complex; BINS], second: &mut [Complex; BINS]) {
        let rear = immersion * MAX_REAR_SHARE;
        let front = (1.0 - rear * rear).sqrt();
        let mut channels = [[0f32; BLOCK]; 4];
        for n in 0..BLOCK {
            let (l, r) = (self.cd[0][n], self.cd[1][n]);
            let (mid, side) = ((l + r) * 0.5, (l - r) * 0.5);
            channels[0][n] = mid + side * front;
            channels[1][n] = mid - side * front;
            for (e, line) in self.rear_line.iter_mut().enumerate() {
                let len = line.len();
                line[self.rear_at % len] = if e == 0 { side * rear } else { -side * rear };
                channels[2 + e][n] = line[(self.rear_at + 1) % len];
            }
            self.rear_at = self.rear_at.wrapping_add(1);
        }
        for pair in [(0, 1), (2, 3)] {
            Self::spectra_of_two(&self.fft, &mut self.buffer, &channels[pair.0], Some(&channels[pair.1]), first, second);
            Self::accumulate(&mut self.ears, first, &self.speakers[pair.0]);
            Self::accumulate(&mut self.ears, second, &self.speakers[pair.1]);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A voice playing `signal` at the driver's registers for volume `v`
    /// and `pan` (current volumes), main volume full.
    fn frame(signal: i16, v: i32, pan: i32) -> TapFrame {
        let (l, r) = pan::gains(pan);
        let mut frame = TapFrame { main: [0x7FFE, 0x7FFE], ..Default::default() };
        frame.voice[0] = signal;
        frame.volume[0] = [(((l * v) >> 15) << 1) as i16, (((r * v) >> 15) << 1) as i16];
        let mix = |g: i16| ((((signal as i32 * g as i32) >> 15) * 0x7FFE) >> 15) as i16;
        frame.output = [mix(frame.volume[0][0]), mix(frame.volume[0][1])];
        frame
    }

    /// White noise: sample `n` of a hashed counter.
    fn noise(n: usize) -> i16 {
        let mut x = (n as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15);
        x ^= x >> 29;
        x = x.wrapping_mul(0xBF58_476D_1CE4_E5B9);
        x ^= x >> 32;
        ((x >> 48) as i16) / 4
    }

    /// Pink noise (3 dB down an octave, as music's spectrum falls; Paul
    /// Kellet's filter), the spectrum the filters are levelled for.
    fn pink(count: usize) -> Vec<i16> {
        let mut b = [0f32; 7];
        (0..count)
            .map(|n| {
                let white = f32::from(noise(n)) / 8192.0;
                b[0] = 0.99886 * b[0] + white * 0.0555179;
                b[1] = 0.99332 * b[1] + white * 0.0750759;
                b[2] = 0.96900 * b[2] + white * 0.1538520;
                b[3] = 0.86650 * b[3] + white * 0.3104856;
                b[4] = 0.55000 * b[4] + white * 0.5329522;
                b[5] = -0.7616 * b[5] - white * 0.0168980;
                let pink = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + white * 0.5362;
                b[6] = white * 0.115926;
                (pink * 2500.0) as i16
            })
            .collect()
    }

    fn render(renderer: &mut Renderer, frames: impl Iterator<Item = TapFrame>) -> Vec<i16> {
        let mut out = Vec::new();
        for frame in frames {
            renderer.push(&frame, &mut out);
        }
        out
    }

    fn energy(samples: &[i16], side: usize) -> f64 {
        samples.chunks(2).map(|p| f64::from(p[side]).powi(2)).sum()
    }

    #[test]
    fn at_rest_it_presents_the_original_mix_a_block_late() {
        let mut renderer = Renderer::new();
        let frames: Vec<_> = (0..1000).map(|n| frame(noise(n), 0x3000, 0x2000)).collect();
        let out = render(&mut renderer, frames.iter().copied());
        assert_eq!(out.len(), 2 * (1000 / BLOCK * BLOCK));
        for (n, pair) in out.chunks(2).enumerate() {
            assert_eq!(pair, frames[n].output, "sample {n}");
        }
        assert_eq!(renderer.pending(), 1000 % BLOCK);
        let mut rest = Vec::new();
        renderer.flush_original(&mut rest);
        assert_eq!(rest, frames[1000 / BLOCK * BLOCK..].iter().flat_map(|f| f.output).collect::<Vec<_>>());
    }

    #[test]
    fn a_placed_voice_keeps_its_gain_and_loses_its_pan() {
        let n = 2 * 44_100;
        let signal = pink(n);
        let run = |pan: i32, direction: [f32; 3]| {
            let mut renderer = Renderer::new();
            renderer.set_enabled(true);
            renderer.reset();
            renderer.set_route(0, Route::Source(7));
            renderer.set_source(7, Target::Listener(direction), true);
            render(&mut renderer, (0..n).map(|i| frame(signal[i], 0x3000, pan)))
        };
        let both = |s: &[i16]| energy(s, 0) + energy(s, 1);
        let db = |ratio: f64| 10.0 * ratio.log10();
        // Placed ahead, the pan it had makes no difference.
        let ahead = run(0x4000, [0.0, 0.0, 1.0]);
        assert!((energy(&ahead, 0) / energy(&ahead, 1) - 1.0).abs() < 0.01, "ahead is centred");
        for pan in [0, 0x7F00] {
            let other = run(pan, [0.0, 0.0, 1.0]);
            assert!(db(both(&other) / both(&ahead)).abs() < 0.1, "pan {pan:#x}");
        }
        // As loud as the original's centre, and as loud anywhere.
        let original: Vec<i16> = (0..n / BLOCK * BLOCK).flat_map(|i| frame(signal[i], 0x3000, 0x4000).output).collect();
        assert!(db(both(&ahead) / both(&original)).abs() < 1.5, "ahead against the centre: {:.2} dB", db(both(&ahead) / both(&original)));
        let right = run(0, [1.0, 0.0, 0.0]);
        assert!(energy(&right, 1) > 2.0 * energy(&right, 0), "placed right, though panned hard left: {}", energy(&right, 1) / energy(&right, 0));
        for direction in [[1.0, 0.0, 0.0], [0.0, 0.0, -1.0], [-1.0, -1.0, 1.0]] {
            let placed = run(0x4000, direction);
            assert!(db(both(&placed) / both(&ahead)).abs() < 1.5, "{direction:?} against ahead: {:.2} dB", db(both(&placed) / both(&ahead)));
        }
    }

    #[test]
    fn original_voices_reverb_and_cd_keep_their_mix() {
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        let frames: Vec<TapFrame> = (0..2048)
            .map(|n| {
                let mut f = frame(noise(n), 0x2000, 0x1000);
                f.reverb = [noise(n + 7) / 4, noise(n + 9) / 4];
                f.cd = [i32::from(noise(n + 3)) / 3, -i32::from(noise(n + 5)) / 3];
                let sum = |e: usize| {
                    let voice = (i32::from(f.voice[0]) * i32::from(f.volume[0][e])) >> 15;
                    ((voice + i32::from(f.reverb[e]) + f.cd[e]) * 0x7FFE) >> 15
                };
                f.output = [sum(0) as i16, sum(1) as i16];
                f
            })
            .collect();
        let out = render(&mut renderer, frames.iter().copied());
        for (n, pair) in out.chunks(2).enumerate() {
            for e in 0..2 {
                assert!((i32::from(pair[e]) - i32::from(frames[n].output[e])).abs() <= 2, "sample {n} side {e}");
            }
        }
    }

    #[test]
    fn switching_crossfades_without_a_jump() {
        let mut renderer = Renderer::new();
        let frames: Vec<_> = (0..20 * BLOCK).map(|n| frame(((n as f32 * 0.05).sin() * 8000.0) as i16, 0x3000, 0)).collect();
        renderer.set_route(0, Route::Source(1));
        renderer.set_source(1, Target::Listener([1.0, 0.0, 0.0]), true);
        let mut out = Vec::new();
        for (n, f) in frames.iter().enumerate() {
            if n == 3 * BLOCK {
                renderer.set_enabled(true);
            }
            renderer.push(f, &mut out);
        }
        let max_step = out.chunks(2).collect::<Vec<_>>().windows(2).map(|w| (i32::from(w[1][0]) - i32::from(w[0][0])).abs()).max().unwrap();
        // The tone itself moves at most 8000 * 0.05 a sample.
        assert!(max_step < 500, "largest step {max_step}");
    }

    #[test]
    fn a_reset_forgets_the_block_under_way_and_the_tails() {
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        renderer.set_route(0, Route::Source(3));
        renderer.set_source(3, Target::Listener([0.0, 0.0, 1.0]), true);
        let mut out = Vec::new();
        for n in 0..(3 * BLOCK + 50) {
            renderer.push(&frame(noise(n), 0x3F00, 0x4000), &mut out);
        }
        renderer.reset();
        assert_eq!(renderer.pending(), 0);
        out.clear();
        for _ in 0..BLOCK {
            renderer.push(&TapFrame { main: [0x7FFE, 0x7FFE], ..Default::default() }, &mut out);
        }
        assert!(out.iter().all(|s| *s == 0), "no tail of the abandoned sound");
        assert_eq!(renderer.route(0), Route::Original, "routes start again from the original mix");
    }

    #[test]
    fn stage_targets_follow_the_musics_own_pan_and_the_stage() {
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        renderer.set_route(0, Route::Source(9));
        renderer.set_source(9, Target::Stage { azimuth: 0.0, elevation: 0.0, home_pan: 16384.0, spread: 60.0 }, true);
        // Panned fully right of home: 60 degrees to the right.
        render(&mut renderer, (0..40 * BLOCK).map(|n| frame(noise(n), 0x3000, 0x7F00)));
        let status = renderer.sources();
        assert!((status[0].azimuth - 59.3).abs() < 1.5, "{status:?}");
        // The head turned a quarter to the right (the listener's forward is
        // the stage's right): the stage's front is heard on the left.
        let quarter = [[0.0, 0.0, -1.0], [0.0, 1.0, 0.0], [1.0, 0.0, 0.0]];
        renderer.set_stage_rotation(quarter);
        render(&mut renderer, (0..80 * BLOCK).map(|n| frame(noise(n), 0x3000, 0x4000)));
        let status = renderer.sources();
        assert!((status[0].azimuth + 90.0).abs() < 2.0, "{status:?}");
    }

    #[test]
    fn peaks_bend_into_full_scale_instead_of_clipping() {
        assert_eq!(knee(0.5), 0.5, "below the knee nothing changes");
        assert_eq!(knee(-KNEE), -KNEE);
        let mut last = KNEE;
        for i in 1..100 {
            let y = KNEE + i as f32 * 0.005;
            let bent = knee(y);
            assert!(bent > last && bent < 1.0, "monotonic and below full scale at {y}: {bent}");
            last = bent;
        }
        // Four voices at full scale, placed together, peak at about twice
        // full scale: the output stays inside it, keeping its shape.
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        for v in 0..4 {
            renderer.set_route(v, Route::Source(v as u32));
            renderer.set_source(v as u32, Target::Listener([1.0, 0.0, 0.2]), true);
        }
        let out = render(
            &mut renderer,
            (0..8 * BLOCK).map(|n| {
                let mut f = frame(((n as f32 * 0.03).sin() * 32000.0) as i16, 0x3F80, 0x7F00);
                for v in 1..4 {
                    f.voice[v] = f.voice[0];
                    f.volume[v] = f.volume[0];
                }
                f
            }),
        );
        let right: Vec<i16> = out.chunks(2).map(|p| p[1]).collect();
        assert!(right.iter().any(|s| s.abs() > 28_000), "loud");
        let flat = right.windows(2).filter(|w| w[0] == w[1] && w[0].abs() >= 32_767).count();
        assert!(flat < right.len() / 50, "no flat-topped runs at full scale: {flat}");
    }
}

