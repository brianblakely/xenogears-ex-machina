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
/// The estimate's four loudspeakers (azimuth, degrees, all level): front
/// left and right, rear left and right.
pub const UPMIX_SPEAKERS: [f32; 4] = [-FRONT_SPEAKERS, FRONT_SPEAKERS, -REAR_SPEAKERS, REAR_SPEAKERS];
/// The share of the side signal the estimate moves behind at full immersion.
const MAX_REAR_SHARE: f32 = 0.6;
/// Delays of the rear speakers (samples), unequal so they do not image.
const REAR_DELAYS: [usize; 2] = [441, 617];
/// The most samples a journal keeps ([`Renderer::start_journal`]): 30 s, at
/// about 7.4 MB a second.
pub const JOURNAL_LIMIT: usize = 30 * SAMPLE_RATE as usize;

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
    /// Where it is heading, the direction it glides to (azimuth and
    /// elevation, degrees): its target as set, a place on the stage turned
    /// by its voices' pan in the last block and by the stage's rotation;
    /// none while it keeps the direction it has (a source at the listener).
    pub target: Option<(f32, f32)>,
    /// Its voices' pan in the last block (weighted by their gains; 0x4000
    /// the centre), none when they were silent.
    pub pan: Option<f32>,
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

/// A change to what the renderer mixes, as a [`Journal`] keeps it.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Change {
    Enabled(bool),
    Stage([[f32; 3]; 3]),
    Cd(CdRoute),
    Route { voice: usize, route: Route },
    Source { id: u32, target: Target, placed: bool },
    Placed { id: u32, placed: bool },
    FadeIn { id: u32, target: Target },
    Reset,
    FlushOriginal,
}

/// What a renderer mixed since [`Renderer::start_journal`]: the state it
/// was in then, every sample's tap and every change, at the sample it came
/// before. Mixed again ([`replay`](Self::replay)) it gives the output it
/// gave, sample for sample; mixed with parts left out, what those parts
/// added. Presentation only: it reads what the SPU mixed and keeps it.
#[derive(Clone)]
pub struct Journal {
    start: Box<Renderer>,
    frames: Vec<TapFrame>,
    changes: Vec<(usize, Change)>,
    truncated: bool,
}

impl Journal {
    /// The taps, one a sample.
    pub fn frames(&self) -> &[TapFrame] {
        &self.frames
    }

    /// The changes, each with the frame it came before.
    pub fn changes(&self) -> &[(usize, Change)] {
        &self.changes
    }

    /// Whether it reached [`JOURNAL_LIMIT`] and kept no more.
    pub fn truncated(&self) -> bool {
        self.truncated
    }

    /// The output samples (per side) that come before the first frame's:
    /// the block under way when the journal started.
    pub fn lead(&self) -> usize {
        self.start.filled
    }

    /// Mixes the frames again from the state the journal started in, each
    /// change where it came; `edit` may change each frame first, seeing the
    /// voices' routes for its sample. The output (interleaved): the
    /// [`lead`](Self::lead), then each frame's sample, up to the last whole
    /// block.
    pub fn replay(&self, mut edit: impl FnMut(&[Route; VOICES], &mut TapFrame)) -> Vec<i16> {
        let mut renderer = (*self.start).clone();
        let mut out = Vec::with_capacity(2 * (self.frames.len() + BLOCK));
        let mut changes = self.changes.iter().peekable();
        for (n, frame) in self.frames.iter().enumerate() {
            while let Some((_, change)) = changes.next_if(|(at, _)| *at <= n) {
                renderer.apply(*change, &mut out);
            }
            let mut frame = *frame;
            edit(&renderer.routes, &mut frame);
            renderer.push(&frame, &mut out);
        }
        for (_, change) in changes {
            renderer.apply(*change, &mut out);
        }
        out
    }

    /// The sources heard in each block of the frames ([`BLOCK`] of them
    /// from the first): each source's id and the voices (a bit each) routed
    /// to it that sounded in the block.
    pub fn block_routes(&self) -> Vec<Vec<(u32, u32)>> {
        let mut routes = self.start.routes;
        let mut changes = self.changes.iter().peekable();
        let mut blocks = Vec::with_capacity(self.frames.len().div_ceil(BLOCK));
        for (n, frame) in self.frames.iter().enumerate() {
            while let Some((_, change)) = changes.next_if(|(at, _)| *at <= n) {
                match change {
                    Change::Route { voice, route } => routes[*voice] = *route,
                    Change::Reset => routes = [Route::Original; VOICES],
                    _ => {}
                }
            }
            if n % BLOCK == 0 {
                blocks.push(std::collections::BTreeMap::<u32, u32>::new());
            }
            let block = blocks.last_mut().expect("pushed above");
            for (v, route) in routes.iter().enumerate() {
                if let Route::Source(id) = route
                    && frame.voice[v] != 0
                    && !frame.muted
                {
                    *block.entry(*id).or_default() |= 1 << v;
                }
            }
        }
        blocks.into_iter().map(|block| block.into_iter().collect()).collect()
    }
}

#[derive(Clone)]
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
    /// The filter it had before, and whether this block remade it: the
    /// block then fades from the one to the other.
    previous: Filter,
    changed: bool,
    /// The last block's input spectrum (zero when it was silent), whose
    /// tail a new filter also hears.
    spectrum: [Complex; BINS],
    input: [f32; BLOCK],
    /// The gain-weighted pan of its voices in this block, and in the last.
    pan_sum: f32,
    pan_weight: f32,
    last_pan: Option<f32>,
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
            previous: Filter::default(),
            changed: false,
            spectrum: [Complex::ZERO; BINS],
            input: [0.0; BLOCK],
            pan_sum: 0.0,
            pan_weight: 0.0,
            last_pan: None,
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
#[derive(Clone)]
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
    /// The block through the filters the sources had before (those whose
    /// filter it remade), which it fades out; and what the last block's
    /// tail gains through their new ones.
    before: [[Complex; BINS]; 2],
    buffer_before: Vec<Complex>,
    late: [[Complex; BINS]; 2],
    buffer_late: Vec<Complex>,
    journal: Option<Box<Journal>>,
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
        for (filter, azimuth) in speakers.iter_mut().zip(UPMIX_SPEAKERS) {
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
            before: [[Complex::ZERO; BINS]; 2],
            buffer_before: vec![Complex::ZERO; FFT_SIZE],
            late: [[Complex::ZERO; BINS]; 2],
            buffer_late: vec![Complex::ZERO; FFT_SIZE],
            journal: None,
        }
    }

    /// Starts keeping a [`Journal`] of what it mixes (anew if one is under
    /// way), up to [`JOURNAL_LIMIT`] samples.
    pub fn start_journal(&mut self) {
        self.journal = None;
        let start = Box::new(self.clone());
        self.journal = Some(Box::new(Journal { start, frames: Vec::new(), changes: Vec::new(), truncated: false }));
    }

    /// The journal kept since [`start_journal`](Self::start_journal), ending it.
    pub fn take_journal(&mut self) -> Option<Journal> {
        self.journal.take().map(|journal| *journal)
    }

    fn note(&mut self, change: Change) {
        if let Some(journal) = self.journal.as_mut().filter(|journal| !journal.truncated) {
            journal.changes.push((journal.frames.len(), change));
        }
    }

    /// Makes a journal's change (`out`: a flush's samples).
    fn apply(&mut self, change: Change, out: &mut Vec<i16>) {
        match change {
            Change::Enabled(enabled) => self.set_enabled(enabled),
            Change::Stage(rotation) => self.set_stage_rotation(rotation),
            Change::Cd(route) => self.set_cd_route(route),
            Change::Route { voice, route } => self.set_route(voice, route),
            Change::Source { id, target, placed } => self.set_source(id, target, placed),
            Change::Placed { id, placed } => self.set_placed(id, placed),
            Change::FadeIn { id, target } => self.fade_in_source(id, target),
            Change::Reset => self.reset(),
            Change::FlushOriginal => self.flush_original(out),
        }
    }

    /// Forgets the sound so far (a restored state, a new timeline): the
    /// block under way, the filters' tails, the sources, their glides and
    /// the voices' routes; the presentation is at its target at once.
    pub fn reset(&mut self) {
        self.note(Change::Reset);
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
        self.note(Change::Enabled(enabled));
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
        self.note(Change::FlushOriginal);
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
        self.note(Change::Stage(rotation));
        self.stage = rotation;
    }

    pub fn set_cd_route(&mut self, route: CdRoute) {
        self.note(Change::Cd(route));
        self.cd_route = route;
    }

    pub fn route(&self, voice: usize) -> Route {
        self.routes[voice]
    }

    /// Mix `voice` as `route` says from its next sample (a source not set
    /// yet starts ahead of the listener until [`set_source`](Self::set_source)).
    pub fn set_route(&mut self, voice: usize, route: Route) {
        self.note(Change::Route { voice, route });
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
        self.note(Change::Source { id, target, placed });
        let index = self.source_index(id, target, placed);
        let source = &mut self.sources[index];
        source.target = target;
        source.placed = placed;
    }

    /// Whether source `id` is placed (`false` fades it back to the original
    /// mix), where it was.
    pub fn set_placed(&mut self, id: u32, placed: bool) {
        self.note(Change::Placed { id, placed });
        if let Some(source) = self.sources.iter_mut().find(|s| s.id == id) {
            source.placed = placed;
        }
    }

    /// As [`set_source`](Self::set_source) placed, but a new source fades in
    /// from the original mix (its voices are already sounding).
    pub fn fade_in_source(&mut self, id: u32, target: Target) {
        self.note(Change::FadeIn { id, target });
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
                    target: self.aim(source, source.last_pan).map(hrtf::angles),
                    pan: source.last_pan,
                    placed: source.amount,
                    gain: source.last_gain,
                    level_db: (source.level > 1e-6).then(|| 20.0 * source.level.log10()),
                    voices: self.slots.iter().filter(|slot| **slot == Some(index)).count() as u32,
                }
            })
            .collect()
    }

    /// The direction `source` is heading for, unit length: on the stage,
    /// its place turned by its voices' `pan` (none: their home).
    fn aim(&self, source: &Source, pan: Option<f32>) -> Option<[f32; 3]> {
        match source.target {
            Target::Listener(direction) => normalize(direction),
            Target::Stage { azimuth, elevation, home_pan, spread } => {
                let pan = pan.unwrap_or(home_pan);
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
        if let Some(journal) = self.journal.as_mut().filter(|journal| !journal.truncated) {
            if journal.frames.len() < JOURNAL_LIMIT {
                journal.frames.push(*frame);
            } else {
                journal.truncated = true;
            }
        }
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
            let source = &self.sources[index];
            let target = self.aim(source, (source.pan_weight > 0.0).then(|| source.pan_sum / source.pan_weight));
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
            source.changed = false;
            if source.filtered.is_none_or(|made| dot(made, current) < filter_step) {
                let (azimuth, elevation) = hrtf::angles(current);
                // A source starting has no filter to fade from.
                source.changed = source.filtered.is_some();
                std::mem::swap(&mut source.previous, &mut source.filter);
                self.hrtf.filter(azimuth, elevation, &mut source.filter);
                source.filtered = Some(current);
            }
        }
        // Convolve the sources heard in this block, two to a transform. When
        // a heard source's filter was remade, the block fades from all the
        // filters before give (this block and the last one's tail through
        // them) to all the new ones give (the last block's tail through
        // them too): a filter, and its interaural delay, never changes from
        // one sample to the next.
        self.ears = [[Complex::ZERO; BINS]; 2];
        let heard: Vec<usize> = (0..self.sources.len()).filter(|&i| self.sources[i].input.iter().any(|s| *s != 0.0)).collect();
        let fading = heard.iter().any(|&i| self.sources[i].changed);
        if fading {
            self.before = [[Complex::ZERO; BINS]; 2];
            self.late = [[Complex::ZERO; BINS]; 2];
            for source in heard.iter().map(|&i| &self.sources[i]).filter(|source| source.changed) {
                for e in 0..2 {
                    for (k, bin) in self.late[e].iter_mut().enumerate() {
                        *bin = bin.add(source.spectrum[k].mul(source.filter.ears[e][k].sub(source.previous.ears[e][k])));
                    }
                }
            }
        }
        for source in &mut self.sources {
            source.spectrum = [Complex::ZERO; BINS];
        }
        let (mut first, mut second) = ([Complex::ZERO; BINS], [Complex::ZERO; BINS]);
        for pair in heard.chunks(2) {
            let b = pair.get(1).map(|&i| &self.sources[i].input);
            Self::spectra_of_two(&self.fft, &mut self.buffer, &self.sources[pair[0]].input, b, &mut first, &mut second);
            for (&i, spectrum) in pair.iter().zip([&first, &second]) {
                let source = &self.sources[i];
                Self::accumulate(&mut self.ears, spectrum, &source.filter);
                if fading {
                    Self::accumulate(&mut self.before, spectrum, if source.changed { &source.previous } else { &source.filter });
                }
                self.sources[i].spectrum = *spectrum;
            }
        }
        if let CdRoute::Upmix { immersion } = self.cd_route {
            self.upmix(immersion.clamp(0.0, 1.0), fading, &mut first, &mut second);
        }
        // Both ears back at once: left real, right imaginary.
        Self::to_time(&self.fft, &self.ears, &mut self.buffer);
        if fading {
            Self::to_time(&self.fft, &self.before, &mut self.buffer_before);
            Self::to_time(&self.fft, &self.late, &mut self.buffer_late);
        }
        let mix_step = 1.0 / (MIX_FADE_SECONDS * SAMPLE_RATE as f32);
        for n in 0..BLOCK {
            let mut heard = [self.buffer[n].re + self.tail[0][n], self.buffer[n].im + self.tail[1][n]];
            if fading {
                let w = (n + 1) as f32 / BLOCK as f32;
                let before = [self.buffer_before[n].re + self.tail[0][n], self.buffer_before[n].im + self.tail[1][n]];
                let after = [heard[0] + self.buffer_late[n + BLOCK].re, heard[1] + self.buffer_late[n + BLOCK].im];
                heard = [before[0] + (after[0] - before[0]) * w, before[1] + (after[1] - before[1]) * w];
            }
            let wet = [self.bass[0].run(&self.shelf, heard[0]), self.bass[1].run(&self.shelf, heard[1])];
            // The tail is the new filters' (the next block hears through them).
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
            source.last_pan = (source.pan_weight > 0.0).then(|| source.pan_sum / source.pan_weight);
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

    /// Both ears' spectra back to time in one inverse transform: left real,
    /// right imaginary.
    fn to_time(fft: &Fft, ears: &[[Complex; BINS]; 2], buffer: &mut [Complex]) {
        for k in 0..BINS {
            let (l, r) = (ears[0][k], ears[1][k]);
            buffer[k] = Complex::new(l.re - r.im, l.im + r.re);
            if k > 0 && k < FFT_SIZE / 2 {
                let (l, r) = (l.conj(), r.conj());
                buffer[FFT_SIZE - k] = Complex::new(l.re - r.im, l.im + r.re);
            }
        }
        fft.inverse(buffer);
    }

    /// The premixed-music estimate: the CD pair as two loudspeakers ahead,
    /// part of its side (left minus right) signal moved to two behind,
    /// delayed so they stay diffuse; the mid signal and the side's energy
    /// are kept (also in the block heard before, when one is `fading`).
    fn upmix(&mut self, immersion: f32, fading: bool, first: &mut [Complex; BINS], second: &mut [Complex; BINS]) {
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
            for (spectrum, speaker) in [(&*first, pair.0), (&*second, pair.1)] {
                Self::accumulate(&mut self.ears, spectrum, &self.speakers[speaker]);
                if fading {
                    Self::accumulate(&mut self.before, spectrum, &self.speakers[speaker]);
                }
            }
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

    #[test]
    fn a_direction_jump_glides_without_steps_at_the_blocks() {
        // A tone placed on the right turns to the left at once (a camera
        // cut, a jump of the music's immersion): the glide sweeps it past
        // the front at up to 6 degrees a block, its filters remade block
        // after block, and the output's largest second difference stays
        // within one and a half times (plus 4) the tone's held still at
        // either end: 79 and 116 against 55 and 91 on the two sides, where
        // without the fade a block boundary steps, 4 568.
        let tone = |n: usize| ((n as f32 * std::f32::consts::TAU * 300.0 / 44_100.0).sin() * 12_000.0) as i16;
        let (start, end) = ([1.0, 0.0, 0.0], [-1.0, 0.0, 0.2]);
        let placed = |direction: [f32; 3]| {
            let mut renderer = Renderer::new();
            renderer.set_enabled(true);
            renderer.reset();
            renderer.set_route(0, Route::Source(1));
            renderer.set_source(1, Target::Listener(direction), true);
            renderer
        };
        let roughness = |samples: &[i16], e: usize| {
            let side: Vec<i32> = samples.chunks(2).map(|p| i32::from(p[e])).collect();
            side.windows(3).map(|w| (w[2] - 2 * w[1] + w[0]).abs()).max().unwrap()
        };
        let mut renderer = placed(start);
        render(&mut renderer, (0..40 * BLOCK).map(|n| frame(tone(n), 0x3000, 0x4000)));
        renderer.set_source(1, Target::Listener(end), true);
        let turning = render(&mut renderer, (40 * BLOCK..80 * BLOCK).map(|n| frame(tone(n), 0x3000, 0x4000)));
        let still: Vec<Vec<i16>> = [start, end].iter().map(|&d| render(&mut placed(d), (0..40 * BLOCK).map(|n| frame(tone(n), 0x3000, 0x4000)))).collect();
        for e in 0..2 {
            let calm = still.iter().map(|s| roughness(&s[2 * 20 * BLOCK..], e)).max().unwrap();
            let glide = roughness(&turning, e);
            eprintln!("side {e}: a second difference of {glide} while turning, {calm} held still");
            assert!(2 * glide <= 3 * calm + 8, "side {e}: a second difference of {glide} while turning, {calm} held still");
        }
    }

    #[test]
    fn a_sources_status_says_where_it_is_heading_and_how_its_voices_pan() {
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        renderer.set_route(0, Route::Source(4));
        renderer.set_source(4, Target::Listener([0.0, 0.0, 1.0]), true);
        render(&mut renderer, (0..4 * BLOCK).map(|n| frame(noise(n), 0x3000, 0x7F00)));
        // Turned to the right: heading there at once, heard there once it
        // has glided.
        renderer.set_source(4, Target::Listener([2.0, 0.0, 0.0]), true);
        let status = renderer.sources()[0].clone();
        let (azimuth, elevation) = status.target.expect("a target");
        assert!((azimuth - 90.0).abs() < 1e-3 && elevation.abs() < 1e-3, "{status:?}");
        assert!(status.azimuth.abs() < 0.01, "still heard ahead: {status:?}");
        render(&mut renderer, (0..80 * BLOCK).map(|n| frame(noise(n), 0x3000, 0x7F00)));
        assert!((renderer.sources()[0].azimuth - 90.0).abs() < 0.5, "{:?}", renderer.sources()[0]);
        // A source at the listener keeps the direction it has.
        renderer.set_source(4, Target::Listener([0.0; 3]), true);
        assert_eq!(renderer.sources()[0].target, None);
        // On the stage its voices' pan in the last block turns its place.
        renderer.set_source(4, Target::Stage { azimuth: 20.0, elevation: 10.0, home_pan: 16384.0, spread: 40.0 }, true);
        render(&mut renderer, (0..BLOCK).map(|n| frame(noise(n), 0x3000, 0x7F00)));
        let status = renderer.sources()[0].clone();
        let pan = status.pan.expect("the voice sounded");
        assert!((pan - 32512.0).abs() < 256.0, "panned hard right: {status:?}");
        let (azimuth, elevation) = status.target.expect("a target");
        assert!((azimuth - (20.0 + (pan - 16384.0) / 16384.0 * 40.0)).abs() < 1e-3 && (elevation - 10.0).abs() < 1e-3, "{status:?}");
    }

    #[test]
    fn a_journal_mixes_again_what_was_mixed_and_each_part_alone() {
        // Voice 0 placed (pink noise), voice 1 in the original mix, then
        // placed on the stage while sounding; a reverb return throughout.
        let signal = pink(120 * BLOCK);
        let frames: Vec<TapFrame> = (0..120 * BLOCK)
            .map(|n| {
                let mut f = frame(signal[n], 0x3000, 0x2000);
                f.voice[1] = noise(n + 11) / 2;
                f.volume[1] = [0x1800, 0x0800];
                f.reverb = [noise(n + 7) / 8, noise(n + 9) / 8];
                let sum = |e: usize| {
                    let voices: i32 = (0..2).map(|v| (i32::from(f.voice[v]) * i32::from(f.volume[v][e])) >> 15).sum();
                    ((voices + i32::from(f.reverb[e])) * 0x7FFE) >> 15
                };
                f.output = [sum(0) as i16, sum(1) as i16];
                f
            })
            .collect();
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        renderer.set_route(0, Route::Source(1));
        renderer.set_source(1, Target::Listener([1.0, 0.0, 0.0]), true);
        let mut out = Vec::new();
        // The journal starts in the middle of a block.
        for f in &frames[..50] {
            renderer.push(f, &mut out);
        }
        renderer.start_journal();
        let from = out.len();
        for (n, f) in frames[50..].iter().enumerate() {
            if n == 5000 {
                renderer.set_source(1, Target::Listener([-1.0, -0.5, 0.3]), true);
            }
            if n == 8000 {
                renderer.set_route(1, Route::Source(2));
                renderer.fade_in_source(2, Target::Stage { azimuth: -40.0, elevation: 20.0, home_pan: 16384.0, spread: 30.0 });
            }
            renderer.push(f, &mut out);
        }
        let journal = renderer.take_journal().expect("a journal");
        assert_eq!((journal.lead(), journal.frames().len(), journal.truncated()), (50, frames.len() - 50, false));
        assert_eq!(journal.replay(|_, _| {}), out[from..], "the same samples again");
        // Each source alone and the rest (the voices nobody's, the reverb)
        // add up to the mix, once what the block it started in held has
        // left the filters' tails and the bass shelf (24 blocks, 70 ms).
        let alone = |id: u32| {
            journal.replay(|routes, frame| {
                for (v, route) in routes.iter().enumerate() {
                    if *route != Route::Source(id) {
                        frame.voice[v] = 0;
                    }
                }
                frame.reverb = [0; 2];
            })
        };
        let rest = journal.replay(|routes, frame| {
            for (v, route) in routes.iter().enumerate() {
                if *route != Route::Original {
                    frame.voice[v] = 0;
                }
            }
        });
        let (one, two) = (alone(1), alone(2));
        assert!(one.iter().skip(2 * 24 * BLOCK).any(|s| *s != 0) && two.iter().skip(2 * 8100).any(|s| *s != 0));
        for n in 2 * 24 * BLOCK..out.len() - from {
            let sum = i32::from(one[n]) + i32::from(two[n]) + i32::from(rest[n]);
            assert!((sum - i32::from(out[from + n])).abs() <= 2, "sample {n}: {sum} against {}", out[from + n]);
        }
        // The blocks say which voices each source had.
        let blocks = journal.block_routes();
        assert_eq!(blocks.len(), (frames.len() - 50).div_ceil(BLOCK));
        assert_eq!(blocks[0], [(1, 1)]);
        assert_eq!(blocks[8000 / BLOCK + 1], [(1, 1), (2, 2)]);
    }
}

