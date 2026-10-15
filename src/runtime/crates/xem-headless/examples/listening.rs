//! The stimuli of the self-administered listening test (tools/listening/,
//! its README; docs/runtime.md, Binaural sound mode): sounds the user hears
//! over headphones in a local page and answers about, so that a person's
//! localization and judgement of the music's coherence can be set beside
//! the auditory models' predictions. Nothing here is a result: it only
//! makes what is played and the plan of the sessions.
//!
//!     cargo run --release -p xem-headless --example listening -- localization [--out DIR] [--seed N]
//!     cargo run --release -p xem-headless --example listening -- music [--out DIR] [--seed N]
//!         [--only NAME,...] [--state NAME=FILE]...
//!
//! DIR is `.local/listening` at the repository's root by default (ignored:
//! the music is the game's, from the user's disc, and stays on this
//! machine). Every random choice comes from the seed (2026 by default), so
//! the same commit and seed make the same files. Each manifest's
//! `generated` block names the commit and the seed and holds `content`, the
//! SHA-256 of every file written (its path and digest) and of the rest of
//! the manifest: the page resumes a session, and the analysis takes its
//! answers, only where `generated` is the one the manifest has now, so
//! stimuli made in any other way (another seed, `--only`, `--state`, a
//! changed tree) never share answers.
//!
//! `localization` (no disc needed) writes `localization.json` and
//! `localization/`: half-second bursts of seeded Gaussian noise in 48
//! directions (elevations -30, 0, 30 and 60 degrees, azimuths every 30),
//! each in three conditions: placed by [`Renderer`] as the game's voices
//! are (a voice at the Stereo pan law's centre routed to one source),
//! convolved with the KEMAR set's raw responses at those (measured)
//! directions, and panned by the original Stereo law to the direction's
//! lateral position (the control). Moving sources sweep 60 degrees in 1.5
//! seconds (rendered and Stereo: the raw set has no directions between its
//! measured ones); catch trials sound in one ear only. Each condition is
//! scaled so that its bursts' median loudness is -23 LUFS (ITU-R BS.1770-4),
//! each keeping the level differences its directions have. Also written:
//! the calibration sounds (pink noise at -23 LUFS, one ear at a time, the
//! same noise in and out of phase) and three sessions of trials: each
//! direction twice per condition and session (96 a condition), 32 moving
//! and 8 catch trials, shuffled into four blocks with a level rove of up to
//! 3 dB either way, after training trials with feedback.
//!
//! `music` (disc 1 and the game module) writes `coherence.json`,
//! `coherence/` and `states/`: eight excerpts of the game's music, each
//! reached from boot by the recipe below and saved as a state (a host's
//! .xemstate; `--state NAME=FILE` takes one of the user's instead, labelled
//! as theirs, a disc 2 one too when disc 2 lies beside disc 1; `--only`
//! makes the named excerpts alone; a name that is not an excerpt's is
//! refused), then
//! presented from that state four times, the game in Stereo with Binaural's
//! Spatial Music at immersion 0, 35, 70 and 100 (Positional Effects off, so
//! effects keep their original mix in every condition), with a tap capture
//! (presentation only). Each excerpt's conditions: the original Stereo
//! output (the hidden reference, also the labelled one), the four
//! immersions as presented, and a disintegrated low anchor mixed again from
//! the captured voices with every musical part at a random place of its
//! own. The title movie's music is CD audio, which the runtime keeps in its
//! original mix while a movie plays: its immersions are that capture mixed
//! again through the premixed-music estimate (`CdRoute::Upmix`), labelled
//! so, and its anchor places the CD pair's frequency bands at random
//! places. Every condition's loudness, the gain to -23 LUFS the page
//! applies, its peak and its interaural cross-correlation by octave (a
//! proxy, not a model of what people hear) go to the manifest, with two
//! coherence sessions of trials (each excerpt once per session and
//! attribute: coherence, lead and foundation, envelopment, tone colour,
//! preference).

use serde_json::{Value, json};
use std::path::{Path, PathBuf};
use xem_binaural::{BLOCK, Renderer, Route, SAMPLE_RATE, Target, hrtf, measure, pan};
use xem_core::snapshot::digest;
use xem_disc::identify::hex;
use xem_spu::TapFrame;

/// The loudness every condition is brought to (ITU-R BS.1770-4).
const TARGET_LUFS: f64 = -23.0;
/// The localization trials' level rove, either way (dB).
const ROVE_DB: f64 = 3.0;
/// No sample may exceed this, roved up (full scale 1).
const MOST: f32 = 0.9;
const RATE: f64 = SAMPLE_RATE as f64;

/// A stimulus's two sides (left, right), full scale 1.
type Sides = [Vec<f32>; 2];
/// A direction: azimuth clockwise from ahead and elevation, degrees.
type Direction = (f32, f32);

fn repository() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../..")
}

/// A pseudo-random sequence from a seed (splitmix64).
struct Rng(u64);

impl Rng {
    fn new(seed: u64) -> Rng {
        Rng(seed)
    }

    fn next(&mut self) -> u64 {
        self.0 = self.0.wrapping_add(0x9E37_79B9_7F4A_7C15);
        let mut x = self.0;
        x = (x ^ (x >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
        x = (x ^ (x >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
        x ^ (x >> 31)
    }

    /// Uniform in [0, 1).
    fn uniform(&mut self) -> f64 {
        (self.next() >> 11) as f64 / (1u64 << 53) as f64
    }

    /// Standard normal (Box-Muller).
    fn gaussian(&mut self) -> f64 {
        let u = 1.0 - self.uniform();
        let v = self.uniform();
        (-2.0 * u.ln()).sqrt() * (std::f64::consts::TAU * v).cos()
    }

    fn shuffle<T>(&mut self, items: &mut [T]) {
        for i in (1..items.len()).rev() {
            let j = (self.next() % (i as u64 + 1)) as usize;
            items.swap(i, j);
        }
    }
}

/// A seed for one named thing, from the run's seed.
fn seed_of(seed: u64, name: &str) -> u64 {
    let mut hash: u64 = 0xCBF2_9CE4_8422_2325 ^ seed;
    for byte in name.bytes() {
        hash = (hash ^ u64::from(byte)).wrapping_mul(0x0000_0100_0000_01B3);
    }
    Rng::new(hash).next()
}

/// The commit the stimuli come from, and whether tracked files differed.
fn commit() -> Value {
    let git = |args: &[&str]| {
        std::process::Command::new("git").arg("-C").arg(repository()).args(args).output().ok().filter(|o| o.status.success()).map(|o| String::from_utf8_lossy(&o.stdout).trim().to_owned())
    };
    json!({"commit": git(&["rev-parse", "HEAD"]), "dirty": git(&["status", "--porcelain", "--untracked-files=no"]).map(|s| !s.is_empty())})
}

/// What a run writes under its output directory: each file's path there,
/// with its SHA-256.
struct Files {
    out: PathBuf,
    written: Vec<(String, String)>,
}

impl Files {
    fn new(out: &Path) -> Files {
        Files { out: out.to_owned(), written: Vec::new() }
    }

    /// Two sides as a 16-bit stereo WAV file at the SPU's rate, at `file`
    /// under the output directory.
    fn wav(&mut self, file: &str, sides: &Sides) {
        let frames = sides[0].len();
        let data = (frames * 4) as u32;
        let mut out = Vec::with_capacity(44 + data as usize);
        out.extend_from_slice(b"RIFF");
        out.extend_from_slice(&(36 + data).to_le_bytes());
        out.extend_from_slice(b"WAVEfmt ");
        out.extend_from_slice(&16u32.to_le_bytes());
        out.extend_from_slice(&1u16.to_le_bytes());
        out.extend_from_slice(&2u16.to_le_bytes());
        out.extend_from_slice(&SAMPLE_RATE.to_le_bytes());
        out.extend_from_slice(&(SAMPLE_RATE * 4).to_le_bytes());
        out.extend_from_slice(&4u16.to_le_bytes());
        out.extend_from_slice(&16u16.to_le_bytes());
        out.extend_from_slice(b"data");
        out.extend_from_slice(&data.to_le_bytes());
        for n in 0..frames {
            for side in sides {
                out.extend_from_slice(&((side[n] * 32768.0).round().clamp(-32768.0, 32767.0) as i16).to_le_bytes());
            }
        }
        let path = self.out.join(file);
        std::fs::create_dir_all(path.parent().expect("a directory")).expect("the stimuli's directory");
        std::fs::write(&path, &out).expect("a stimulus");
        self.written.push((file.to_owned(), hex(&digest(&out))));
    }

    /// `manifest` with its `generated` block, to which `content` is added:
    /// the SHA-256 of every file written (path and digest, in the order
    /// written) and of the rest of the manifest.
    fn manifest(&self, mut manifest: Value, mut generated: Value) -> Value {
        let covered = serde_json::to_vec(&json!({"files": self.written, "manifest": manifest})).expect("JSON");
        generated["content"] = json!({"sha256": hex(&digest(&covered)), "of": "every file written (its path and SHA-256) and the rest of this manifest"});
        manifest["generated"] = generated;
        manifest
    }
}

/// Stops the run over a wrong command line.
fn refuse(message: &str) -> ! {
    eprintln!("listening: {message}");
    std::process::exit(2);
}

fn peak(sides: &Sides) -> f32 {
    sides.iter().flatten().fold(0f32, |m, s| m.max(s.abs()))
}

fn scaled(sides: &Sides, gain_db: f64) -> Sides {
    let g = 10f64.powf(gain_db / 20.0) as f32;
    [0, 1].map(|e| sides[e].iter().map(|s| s * g).collect())
}

fn loudness(sides: &Sides) -> f64 {
    measure::loudness(&[&sides[0], &sides[1]], RATE)
}

fn rounded(x: f64, places: i32) -> f64 {
    let scale = 10f64.powi(places);
    (x * scale).round() / scale
}

fn write_json(path: &Path, value: &Value) {
    std::fs::write(path, serde_json::to_string_pretty(value).expect("JSON") + "\n").expect("a manifest");
    println!("wrote {}", path.display());
}

fn main() {
    let mut args = std::env::args().skip(1);
    let part = args.next().unwrap_or_default();
    let mut out = repository().join(".local/listening");
    let mut seed = 2026u64;
    let mut only: Option<Vec<String>> = None;
    let mut states: Vec<(String, PathBuf)> = Vec::new();
    while let Some(arg) = args.next() {
        let mut value = || args.next().unwrap_or_else(|| refuse(&format!("{arg} needs a value")));
        match arg.as_str() {
            "--out" => out = PathBuf::from(value()),
            "--seed" => seed = value().parse().unwrap_or_else(|_| refuse("--seed takes a number")),
            "--only" => only = Some(value().split(',').map(str::to_owned).collect()),
            "--state" => {
                let v = value();
                let (name, path) = v.split_once('=').unwrap_or_else(|| refuse("--state takes NAME=FILE"));
                states.push((name.to_owned(), PathBuf::from(path)));
            }
            other => refuse(&format!("unknown option {other}")),
        }
    }
    let usage = "usage: listening localization|music [--out DIR] [--seed N] [--only NAME,...] [--state NAME=FILE]...";
    match part.as_str() {
        "localization" if only.is_some() || !states.is_empty() => refuse("--only and --state choose music excerpts; localization takes neither"),
        "localization" | "music" => {}
        _ => refuse(usage),
    }
    std::fs::create_dir_all(&out).expect("the output directory");
    let out = out.canonicalize().expect("the output directory");
    if part == "localization" {
        localization::main(&out, seed);
    } else {
        music::main(&out, seed, only, states);
    }
}

mod localization {
    use super::*;

    /// The directions heard (azimuth clockwise from ahead, elevation;
    /// degrees): every one is measured in the KEMAR set, so the raw
    /// responses need no interpolation.
    fn directions() -> Vec<Direction> {
        [-30.0, 0.0, 30.0, 60.0].iter().flat_map(|&e| (0..12).map(move |a| (a as f32 * 30.0, e))).collect()
    }

    const CONDITIONS: [&str; 3] = ["rendered", "kemar", "stereo"];
    const SESSIONS: usize = 3;
    /// Each direction's trials per condition and session.
    const REPEATS: usize = 2;
    const BLOCKS: usize = 4;
    const CATCH: usize = 8;
    /// Training trials (with feedback) in the first session, and in later ones.
    const TRAINING: [usize; 2] = [24, 12];
    const BURST_SECONDS: f64 = 0.5;
    const MOVING_SECONDS: f64 = 1.5;
    const RAMP_SECONDS: f64 = 0.02;
    /// Samples after a burst for the filters' tails.
    const TAIL: usize = 4 * BLOCK;
    /// The voice's level as the SPU hands it over (RMS, full scale 1).
    const VOICE_RMS: f64 = 0.05;
    /// A voice at the pan law's centre of this volume has a pre-pan gain
    /// of 1; the main volume is full.
    const VOLUME: i32 = 0x3FFF;
    const MAIN: i16 = 0x7FFE;

    /// Gaussian noise of RMS 1 with raised-cosine ramps at both ends.
    fn noise(seed: u64, seconds: f64) -> Vec<f32> {
        let n = (seconds * RATE) as usize;
        let ramp = (RAMP_SECONDS * RATE) as usize;
        let mut rng = Rng::new(seed);
        (0..n)
            .map(|t| {
                let edge = t.min(n - 1 - t);
                let w = if edge < ramp { 0.5 - 0.5 * (std::f64::consts::PI * edge as f64 / ramp as f64).cos() } else { 1.0 };
                (rng.gaussian() * w) as f32
            })
            .collect()
    }

    /// Where a moving source is at `u` (0 to 1) of its way: (azimuth,
    /// elevation), degrees.
    fn along(start: Direction, end: Direction, u: f32) -> Direction {
        (start.0 + (end.0 - start.0) * u, start.1 + (end.1 - start.1) * u)
    }

    /// The renderer placing `signal` (a voice at the pan law's centre, the
    /// pre-pan gain 1) as one source, at `path(u)` each block (u: the share
    /// of the signal played); the two sides, and where the source was heard
    /// in the first and the last block of the signal.
    fn rendered(signal: &[f32], path: impl Fn(f32) -> Direction) -> (Sides, Direction, Direction) {
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        renderer.set_route(0, Route::Source(1));
        let (l, r) = pan::gains(0x4000);
        let volume = [(((l * VOLUME) >> 15) << 1) as i16, (((r * VOLUME) >> 15) << 1) as i16];
        let mut frame = TapFrame { main: [MAIN, MAIN], ..Default::default() };
        frame.volume[0] = volume;
        let total = (signal.len() + TAIL).div_ceil(BLOCK) * BLOCK;
        let mut out = Vec::with_capacity(2 * total);
        let (mut first, mut last) = ((0.0, 0.0), (0.0, 0.0));
        for n in 0..total {
            if n % BLOCK == 0 {
                let (azimuth, elevation) = path((n as f32 / signal.len() as f32).min(1.0));
                renderer.set_source(1, Target::Listener(hrtf::direction(azimuth, elevation)), true);
            }
            let s = (f64::from(*signal.get(n).unwrap_or(&0.0)) * VOICE_RMS * 32768.0).round().clamp(-32768.0, 32767.0) as i16;
            frame.voice[0] = s;
            let mixed = |e: usize| ((((i32::from(s) * i32::from(volume[e])) >> 15) * i32::from(MAIN)) >> 15) as i16;
            frame.output = [mixed(0), mixed(1)];
            renderer.push(&frame, &mut out);
            if (n + 1) % BLOCK == 0 && n < signal.len() {
                let heard = renderer.sources().first().map_or((0.0, 0.0), |s| (s.azimuth, s.elevation));
                if n < BLOCK {
                    first = heard;
                }
                last = heard;
            }
        }
        ([0, 1].map(|e| out.iter().skip(e).step_by(2).map(|s| f32::from(*s) / 32768.0).collect()), first, last)
    }

    /// `signal` convolved with the KEMAR set's own responses at a measured
    /// direction, as the set holds them.
    fn raw(signal: &[f32], ears: &[[f32; hrtf::FRAMES]; 2]) -> Sides {
        let x: Vec<f32> = signal.iter().map(|s| (f64::from(*s) * VOICE_RMS) as f32).collect();
        [0, 1].map(|e| {
            let mut y = vec![0f32; x.len() + TAIL];
            for (n, &s) in x.iter().enumerate() {
                for (k, h) in ears[e].iter().enumerate() {
                    y[n + k] += s * h;
                }
            }
            y
        })
    }

    /// The pan the original Stereo law gives a direction: its lateral
    /// position (sin(azimuth) cos(elevation)) as the pan, as the
    /// rendered_hrirs example's control does.
    fn stereo_gains(azimuth: f32, elevation: f32) -> [f32; 2] {
        let x = azimuth.to_radians().sin() * elevation.to_radians().cos();
        let pan = (16384.0 + x * if x < 0.0 { 16384.0 } else { 16128.0 }).round() as i32;
        let (l, r) = pan::gains(pan);
        [l as f32 / 32768.0, r as f32 / 32768.0]
    }

    /// `signal` (the voice's, pre-pan gain 1) panned by the Stereo law to
    /// `path(u)` each block.
    fn stereo(signal: &[f32], path: impl Fn(f32) -> Direction) -> Sides {
        let total = signal.len() + TAIL;
        let mut sides = [vec![0f32; total], vec![0f32; total]];
        let mut gains = [0f32; 2];
        for (n, s) in signal.iter().enumerate() {
            if n % BLOCK == 0 {
                let (azimuth, elevation) = path(n as f32 / signal.len() as f32);
                gains = stereo_gains(azimuth, elevation);
            }
            for e in 0..2 {
                sides[e][n] = (f64::from(*s) * VOICE_RMS) as f32 * gains[e];
            }
        }
        sides
    }

    /// Pink noise (3 dB down an octave; Paul Kellet's filter) of RMS about
    /// 1, with 50 ms ramps.
    fn pink(seed: u64, seconds: f64) -> Vec<f32> {
        let mut rng = Rng::new(seed);
        let mut b = [0f64; 7];
        let n = (seconds * RATE) as usize;
        let ramp = (0.05 * RATE) as usize;
        (0..n)
            .map(|t| {
                let white = rng.gaussian();
                b[0] = 0.99886 * b[0] + white * 0.0555179;
                b[1] = 0.99332 * b[1] + white * 0.0750759;
                b[2] = 0.96900 * b[2] + white * 0.1538520;
                b[3] = 0.86650 * b[3] + white * 0.3104856;
                b[4] = 0.55000 * b[4] + white * 0.5329522;
                b[5] = -0.7616 * b[5] - white * 0.0168980;
                let pink = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + white * 0.5362;
                b[6] = white * 0.115926;
                let edge = t.min(n - 1 - t);
                let w = if edge < ramp { 0.5 - 0.5 * (std::f64::consts::PI * edge as f64 / ramp as f64).cos() } else { 1.0 };
                (pink * 0.2 * w) as f32
            })
            .collect()
    }

    /// Noise from 100 Hz to 800 Hz (one-pole filters), for the polarity check.
    fn low_noise(seed: u64, seconds: f64) -> Vec<f32> {
        let white = noise(seed, seconds);
        let low = (-std::f64::consts::TAU * 800.0 / RATE).exp();
        let high = (-std::f64::consts::TAU * 100.0 / RATE).exp();
        let (mut a, mut b, mut c, mut previous) = (0f64, 0f64, 0f64, 0f64);
        white
            .iter()
            .map(|&x| {
                a = (1.0 - low) * f64::from(x) + low * a;
                b = (1.0 - low) * a + low * b;
                // High pass: the input less its own low pass.
                c = high * (c + b - previous);
                previous = b;
                c as f32
            })
            .collect()
    }

    /// One stimulus made: its description, and its sides before the
    /// condition's gain.
    struct Made {
        entry: Value,
        sides: Sides,
    }

    pub fn main(out: &Path, seed: u64) {
        let started = std::time::Instant::now();
        let hrtf = hrtf::Hrtf::kemar();
        let measured = hrtf.measured();
        let ears_at = |azimuth: f32, elevation: f32| -> [[f32; hrtf::FRAMES]; 2] {
            measured
                .iter()
                .find(|m| (m.azimuth - azimuth).abs() < 0.01 && (m.elevation - elevation).abs() < 0.01)
                .unwrap_or_else(|| panic!("{azimuth}/{elevation} is not a measured direction"))
                .ears
        };
        let id_of = |condition: &str, (azimuth, elevation): Direction, token: usize| format!("{condition}-{:03}{:+03}-t{token}", azimuth as i32, elevation as i32);
        let mut made: Vec<Made> = Vec::new();
        let tokens = SESSIONS * REPEATS;
        // Static bursts: every direction, condition and token; the same
        // token is the same noise in every condition.
        for (d, &direction) in directions().iter().enumerate() {
            for token in 0..tokens {
                let signal = noise(seed_of(seed, &format!("burst-{d}-{token}")), BURST_SECONDS);
                for condition in CONDITIONS {
                    let (sides, heard) = match condition {
                        "rendered" => {
                            let (sides, first, _) = rendered(&signal, |_| direction);
                            (sides, json!([rounded(f64::from(first.0), 2), rounded(f64::from(first.1), 2)]))
                        }
                        "kemar" => (raw(&signal, &ears_at(direction.0, direction.1)), Value::Null),
                        _ => (stereo(&signal, |_| direction), Value::Null),
                    };
                    let id = id_of(condition, direction, token);
                    made.push(Made {
                        entry: json!({"id": id, "file": format!("localization/{condition}/{id}.wav"), "condition": condition, "motion": "static",
                            "start": [direction.0, direction.1], "end": [direction.0, direction.1], "token": token, "heard": heard}),
                        sides,
                    });
                }
            }
        }
        // Moving sources: 60 degrees across the front, the sides and the
        // back on the horizontal plane, and from 30 below to 30 above at
        // four azimuths, each way, one token a session.
        let mut paths: Vec<(String, Direction, Direction)> = Vec::new();
        for centre in [0.0f32, 90.0, 180.0, 270.0] {
            paths.push((format!("h{:03}cw", centre as i32), (centre - 30.0, 0.0), (centre + 30.0, 0.0)));
            paths.push((format!("h{:03}ccw", centre as i32), (centre + 30.0, 0.0), (centre - 30.0, 0.0)));
        }
        for azimuth in [0.0f32, 90.0, 180.0, 270.0] {
            paths.push((format!("v{:03}up", azimuth as i32), (azimuth, -30.0), (azimuth, 30.0)));
            paths.push((format!("v{:03}down", azimuth as i32), (azimuth, 30.0), (azimuth, -30.0)));
        }
        let wrap = |(azimuth, elevation): Direction| json!([azimuth.rem_euclid(360.0), elevation]);
        for (p, (name, start, end)) in paths.iter().enumerate() {
            for token in 0..SESSIONS {
                let signal = noise(seed_of(seed, &format!("moving-{p}-{token}")), MOVING_SECONDS);
                for condition in ["rendered", "stereo"] {
                    let path = |u: f32| along(*start, *end, u);
                    let (sides, heard) = if condition == "rendered" {
                        let (sides, first, last) = rendered(&signal, path);
                        (sides, json!({"start": [rounded(f64::from(first.0), 2), rounded(f64::from(first.1), 2)], "end": [rounded(f64::from(last.0), 2), rounded(f64::from(last.1), 2)]}))
                    } else {
                        (stereo(&signal, path), Value::Null)
                    };
                    let id = format!("{condition}-move-{name}-t{token}");
                    made.push(Made {
                        entry: json!({"id": id, "file": format!("localization/{condition}/{id}.wav"), "condition": condition, "motion": "moving",
                            "path": name, "start": wrap(*start), "end": wrap(*end), "token": token, "heard": heard}),
                        sides,
                    });
                }
            }
        }
        // Each condition's gain: its static bursts' median loudness at the
        // target.
        let mut gains = serde_json::Map::new();
        for condition in CONDITIONS {
            let mut levels: Vec<f64> = made.iter().filter(|m| m.entry["condition"] == condition && m.entry["motion"] == "static").map(|m| loudness(&m.sides)).collect();
            levels.sort_by(f64::total_cmp);
            gains.insert(condition.to_owned(), json!(rounded(TARGET_LUFS - levels[levels.len() / 2], 3)));
        }
        // The cues' sanity: a burst well to one side is louder in that ear.
        for m in &made {
            let (azimuth, elevation) = (m.entry["start"][0].as_f64().unwrap(), m.entry["start"][1].as_f64().unwrap());
            let lateral = (azimuth.to_radians().sin() * elevation.to_radians().cos()).asin().to_degrees();
            if m.entry["motion"] == "static" && lateral.abs() >= 30.0 {
                let energy = |e: usize| m.sides[e].iter().map(|s| f64::from(*s).powi(2)).sum::<f64>();
                let (near, far) = if lateral > 0.0 { (energy(1), energy(0)) } else { (energy(0), energy(1)) };
                assert!(near > far, "{}: the near ear is the louder", m.entry["id"]);
            }
        }
        let mut files = Files::new(out);
        let mut stimuli = Vec::new();
        let mut loudest = 0f32;
        for m in made {
            let gain = gains[m.entry["condition"].as_str().unwrap()].as_f64().unwrap();
            let sides = scaled(&m.sides, gain);
            loudest = loudest.max(peak(&sides));
            let mut entry = m.entry;
            entry["loudness"] = json!(rounded(loudness(&sides), 2));
            entry["seconds"] = json!(rounded(sides[0].len() as f64 / RATE, 4));
            files.wav(entry["file"].as_str().unwrap(), &sides);
            stimuli.push(entry);
        }
        // Catch trials: the burst in one ear only, at the target loudness.
        for token in 0..SESSIONS * CATCH {
            let side = if token % 2 == 0 { "left" } else { "right" };
            let signal = noise(seed_of(seed, &format!("catch-{token}")), BURST_SECONDS);
            let mut sides = [vec![0f32; signal.len() + TAIL], vec![0f32; signal.len() + TAIL]];
            let e = usize::from(side == "right");
            for (n, s) in signal.iter().enumerate() {
                sides[e][n] = (f64::from(*s) * VOICE_RMS) as f32;
            }
            let sides = scaled(&sides, TARGET_LUFS - loudness(&sides));
            loudest = loudest.max(peak(&sides));
            let id = format!("catch-{side}-t{token}");
            let file = format!("localization/catch/{id}.wav");
            files.wav(&file, &sides);
            let azimuth = if side == "right" { 90.0 } else { 270.0 };
            stimuli.push(json!({"id": id, "file": file, "condition": "catch", "motion": "static", "side": side, "start": [azimuth, 0.0], "end": [azimuth, 0.0],
                "token": token, "loudness": rounded(loudness(&sides), 2), "seconds": rounded(sides[0].len() as f64 / RATE, 4)}));
        }
        // Training: rendered bursts of their own noise at shuffled directions.
        let mut training_ids: Vec<Vec<String>> = Vec::new();
        for session in 0..SESSIONS {
            let count = TRAINING[usize::from(session > 0)];
            let mut order = directions();
            Rng::new(seed_of(seed, &format!("training-order-{session}"))).shuffle(&mut order);
            let mut ids = Vec::new();
            for (i, &direction) in order.iter().cycle().take(count).enumerate() {
                let signal = noise(seed_of(seed, &format!("training-{session}-{i}")), BURST_SECONDS);
                let (sides, _, _) = rendered(&signal, |_| direction);
                let sides = scaled(&sides, gains["rendered"].as_f64().unwrap());
                loudest = loudest.max(peak(&sides));
                let id = format!("training-s{session}-{i:02}-{:03}{:+03}", direction.0 as i32, direction.1 as i32);
                let file = format!("localization/training/{id}.wav");
                files.wav(&file, &sides);
                stimuli.push(json!({"id": id, "file": file, "condition": "rendered", "motion": "static", "training": true, "start": [direction.0, direction.1],
                    "end": [direction.0, direction.1], "token": i, "loudness": rounded(loudness(&sides), 2), "seconds": rounded(sides[0].len() as f64 / RATE, 4)}));
                ids.push(id);
            }
            training_ids.push(ids);
        }
        let headroom = 10f64.powf(ROVE_DB / 20.0) as f32;
        assert!(loudest * headroom < MOST, "the loudest stimulus, roved up, reaches {} of full scale", loudest * headroom);
        // Calibration.
        let level = |sides: Sides| scaled(&sides, TARGET_LUFS - loudness(&sides));
        let p = pink(seed_of(seed, "pink"), 8.0);
        files.wav("localization/calibration/pink.wav", &level([p.clone(), p]));
        for (e, side) in ["left", "right"].iter().enumerate() {
            let mut one = Vec::new();
            for burst in 0..3 {
                one.extend(noise(seed_of(seed, &format!("calibration-{side}-{burst}")), 0.25));
                one.extend(std::iter::repeat_n(0.0, (0.15 * RATE) as usize));
            }
            let silent = vec![0f32; one.len()];
            let sides = if e == 0 { [one, silent] } else { [silent, one] };
            files.wav(&format!("localization/calibration/{side}.wav"), &level(sides));
        }
        let low = low_noise(seed_of(seed, "polarity"), 2.0);
        let negated: Vec<f32> = low.iter().map(|s| -s).collect();
        files.wav("localization/calibration/in-phase.wav", &level([low.clone(), low.clone()]));
        files.wav("localization/calibration/anti-phase.wav", &level([low, negated]));
        // The sessions.
        let test_ids = |condition: &str, motion: &str| -> Vec<Value> { stimuli.iter().filter(|s| s["condition"] == condition && s["motion"] == motion && s.get("training").is_none()).cloned().collect() };
        let mut sessions = Vec::new();
        for (session, ids) in training_ids.iter().enumerate() {
            let mut rng = Rng::new(seed_of(seed, &format!("session-{session}")));
            let mut trials: Vec<(String, bool)> = Vec::new();
            for condition in CONDITIONS {
                for s in test_ids(condition, "static") {
                    let token = s["token"].as_u64().unwrap() as usize;
                    if token / REPEATS == session {
                        trials.push((s["id"].as_str().unwrap().to_owned(), false));
                    }
                }
            }
            for condition in ["rendered", "stereo"] {
                for s in test_ids(condition, "moving") {
                    if s["token"].as_u64().unwrap() as usize == session {
                        trials.push((s["id"].as_str().unwrap().to_owned(), false));
                    }
                }
            }
            for s in stimuli.iter().filter(|s| s["condition"] == "catch") {
                if s["token"].as_u64().unwrap() as usize / CATCH == session {
                    trials.push((s["id"].as_str().unwrap().to_owned(), true));
                }
            }
            rng.shuffle(&mut trials);
            let per_block = trials.len().div_ceil(BLOCKS);
            let name = format!("L{}", session + 1);
            let blocks: Vec<Value> = trials
                .chunks(per_block)
                .enumerate()
                .map(|(b, chunk)| {
                    json!(chunk.iter().enumerate().map(|(i, (id, catch))| {
                        let rove = rounded((rng.uniform() * 2.0 - 1.0) * ROVE_DB, 1);
                        json!({"trial": format!("{name}-{}-{:03}", b + 1, i + 1), "stimulus": id, "rove_db": rove, "catch": catch})
                    }).collect::<Vec<_>>())
                })
                .collect();
            let training: Vec<Value> = ids.iter().enumerate().map(|(i, id)| json!({"trial": format!("{name}-T-{:02}", i + 1), "stimulus": id, "rove_db": 0.0, "feedback": true})).collect();
            sessions.push(json!({"name": name, "training": training, "blocks": blocks}));
        }
        let count = stimuli.len();
        let generated = json!({
            "tool": "cargo run --release -p xem-headless --example listening -- localization",
            "source": commit(),
            "seed": seed,
            "sample_rate": SAMPLE_RATE,
            "renderer": {"block": BLOCK, "voice": "one voice at the Stereo pan law's centre, pre-pan gain 1, main volume full, routed to one source (Target::Listener)",
                "moving": "the source's target set every block along the path; the renderer glides to it (30 ms) as for the game's sources"},
            "hrtf": "MIT KEMAR, diffuse-field equalized (crates/xem-binaural/hrtf/README.md)",
        });
        let manifest = json!({
            "kind": "xem-listening-localization",
            "version": 1,
            "level": {"target_lufs": TARGET_LUFS, "rove_db": ROVE_DB, "condition_gain_db": gains, "loudest": rounded(f64::from(loudest), 4),
                "note": "each condition is scaled so that its static bursts' median loudness (ITU-R BS.1770-4) is the target; each keeps the level differences its directions have; the page roves each trial's level"},
            "conditions": {
                "rendered": "the Binaural renderer: KEMAR interpolated, levelled, its bass shelf, interaural delay as phase, 128-sample blocks",
                "kemar": "the KEMAR set's raw responses at the measured direction, convolved (static only)",
                "stereo": "the original Stereo pan law over headphones: the direction's lateral position as the pan (the control)",
                "catch": "the burst in one ear only (the expected answer: that side)",
            },
            "calibration": {"pink": "localization/calibration/pink.wav", "left": "localization/calibration/left.wav", "right": "localization/calibration/right.wav",
                "in_phase": "localization/calibration/in-phase.wav", "anti_phase": "localization/calibration/anti-phase.wav"},
            "stimuli": stimuli,
            "sessions": sessions,
        });
        let manifest = files.manifest(manifest, generated);
        write_json(&out.join("localization.json"), &manifest);
        println!("{count} stimuli in {:.1} s; gains {:?}; loudest {:.3} of full scale", started.elapsed().as_secs_f64(), gains, loudest);
    }
}

#[cfg(not(has_game_module))]
mod music {
    use std::path::{Path, PathBuf};

    pub fn main(_: &Path, _: u64, _: Option<Vec<String>>, _: Vec<(String, PathBuf)>) {
        eprintln!(
            "the music excerpts need the game module: `python3 tools/game_module.py --stubs` builds it, from the matched links of `make -C src/decomp all-verify` (docs/runtime.md, The game module)"
        );
        std::process::exit(1);
    }
}

#[cfg(has_game_module)]
mod music {
    use super::*;
    use std::collections::BTreeMap;
    use xem_binaural::{CdRoute, Journal};
    use xem_core::binaural::Config;
    use xem_core::control::Session;
    use xem_game::{BootOptions, NativeModule, boot};
    use xem_spu::VOICES;

    const IMMERSIONS: [u8; 4] = [0, 35, 70, 100];
    /// Frames presented from the state before the capture starts: the
    /// presentation has placed what sounds and its filters have settled.
    const WARM: u64 = 120;
    /// Samples skipped at the capture's start (the block under way then).
    const SKIP: usize = 4 * BLOCK;
    /// The fades at an excerpt's ends (the page loops it).
    const FADE_SECONDS: f64 = 0.05;
    /// The anchor's bands of the CD pair (Hz, upper edges; the last open).
    const BANDS: [f64; 6] = [250.0, 500.0, 1000.0, 2000.0, 4000.0, f64::INFINITY];

    enum Setup {
        /// `enter_field` and frames after it.
        Field { map: u32, frames: u64 },
        /// Boot into the world map.
        WorldMap,
        /// The original's debug battle selector, its battle as it stands.
        Battle,
        /// Lahan in flames (field 2, the game's opening scene) with Cross
        /// at every dialogue until Weltall's battle starts.
        EventBattle,
        /// Boot to the title movie.
        TitleMovie,
    }

    struct Excerpt {
        name: &'static str,
        label: &'static str,
        kind: &'static str,
        setup: Setup,
        seconds: f64,
    }

    const EXCERPTS: [Excerpt; 8] = [
        Excerpt { name: "lahan", label: "Lahan village (field 1)", kind: "town", setup: Setup::Field { map: 1, frames: 300 }, seconds: 12.0 },
        Excerpt { name: "kislev", label: "Kislev (field 213)", kind: "town", setup: Setup::Field { map: 213, frames: 300 }, seconds: 12.0 },
        Excerpt { name: "kislev-199", label: "Kislev (field 199)", kind: "field", setup: Setup::Field { map: 199, frames: 300 }, seconds: 12.0 },
        Excerpt { name: "solaris", label: "Solaris (field 466)", kind: "field", setup: Setup::Field { map: 466, frames: 300 }, seconds: 12.0 },
        Excerpt { name: "world-map", label: "The world map", kind: "world map", setup: Setup::WorldMap, seconds: 12.0 },
        Excerpt { name: "battle", label: "A battle: the ordinary battle theme (the debug battle selector's battle)", kind: "battle", setup: Setup::Battle, seconds: 12.0 },
        Excerpt { name: "lahan-gear-battle", label: "Weltall's battle in burning Lahan (an event battle; it keeps the scene's music)", kind: "event battle (in place of a boss)", setup: Setup::EventBattle, seconds: 12.0 },
        Excerpt { name: "title-movie", label: "The title movie's music through the premixed-music estimate", kind: "CD estimate", setup: Setup::TitleMovie, seconds: 10.0 },
    ];

    fn disc() -> PathBuf {
        std::env::var_os("XEM_DISC1").map(PathBuf::from).unwrap_or_else(|| repository().join("discs/Xenogears disc 1.chd"))
    }

    fn execute(session: &mut Session<NativeModule>, command: Value) -> Value {
        let reply = xem_game::storage::command(session, &command);
        assert!(reply.get("error").is_none(), "{command}: {reply}");
        reply
    }

    /// Reaches the excerpt's scene from boot and saves it as a state.
    fn make_state(excerpt: &Excerpt, path: &Path) {
        let (mut session, _) = boot(&disc(), &BootOptions::default()).expect("boot");
        let commands: Vec<Value> = match excerpt.setup {
            Setup::Field { map, frames } => vec![json!({"cmd": "enter_field", "map": map}), json!({"cmd": "frames", "count": frames})],
            Setup::WorldMap => vec![json!({"cmd": "boot_mode", "mode": 3}), json!({"cmd": "frames", "count": 900})],
            Setup::Battle => vec![
                json!({"cmd": "frames", "count": 60}),
                json!({"cmd": "write", "path": "mode_battle_standalone", "value": 1}),
                json!({"cmd": "boot_mode", "mode": 2}),
                json!({"cmd": "frames", "count": 900}),
                json!({"cmd": "press", "buttons": ["start"], "frames": 1}),
                json!({"cmd": "frames", "count": 1200}),
            ],
            Setup::EventBattle => vec![json!({"cmd": "enter_field", "map": 2})],
            Setup::TitleMovie => vec![json!({"cmd": "frames", "count": 360})],
        };
        for command in commands {
            execute(&mut session, command);
        }
        if let Setup::EventBattle = excerpt.setup {
            let mut tries = 0;
            while session.runtime.services().mode != Some(2) {
                tries += 1;
                assert!(tries < 400, "Weltall's battle did not start within 12 000 frames");
                execute(&mut session, json!({"cmd": "press", "buttons": ["cross"], "frames": 2}));
                execute(&mut session, json!({"cmd": "frames", "count": 30}));
            }
            execute(&mut session, json!({"cmd": "frames", "count": 300}));
        }
        execute(&mut session, json!({"cmd": "state_save", "path": path.to_str().expect("a UTF-8 path")}));
    }

    /// One presentation from the state: the tap capture's journal, the
    /// output as presented, and the music's sources (id: song, part, role,
    /// placement) as the status reported them.
    struct Presented {
        immersion: u8,
        journal: Journal,
        output: Vec<i16>,
        parts: BTreeMap<u32, Value>,
    }

    fn present(state: &Path, immersion: u8, frames: u64) -> Presented {
        let (mut session, _) = boot(&disc(), &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
        execute(&mut session, json!({"cmd": "state_load", "path": state.to_str().expect("a UTF-8 path")}));
        session.set_sound_mode(1).expect("Stereo");
        let spu = &mut session.runtime.services().spu;
        let mut stale = vec![0; spu.audio_queued()];
        spu.take_audio(&mut stale);
        session.set_binaural(Config { enabled: true, spatial_music: true, positional_effects: false, music_immersion: immersion, head_tracked_music: false, bypass: false });
        let mut parts = BTreeMap::new();
        for n in 0..WARM + frames {
            if n == WARM {
                execute(&mut session, json!({"cmd": "audio_capture", "action": "start", "tap": true}));
            }
            assert!(session.frame().expect("a frame"), "the game ran a frame");
            let spu = &mut session.runtime.services().spu;
            let mut audio = vec![0; spu.audio_queued()];
            spu.take_audio(&mut audio);
            if n >= WARM {
                let status = execute(&mut session, json!({"cmd": "binaural"}));
                for source in status["sources"].as_array().into_iter().flatten().filter(|s| s["kind"] == "music") {
                    let id = source["id"].as_u64().expect("an id") as u32;
                    parts.entry(id).or_insert_with(|| json!({"song": source["song"], "part": source["part"], "role": source["role"], "placement": source["placement"]}));
                }
            }
        }
        let spu = &mut session.runtime.services().spu;
        let journal = spu.take_tap_capture().expect("a tap capture");
        let output = spu.take_capture().expect("the output");
        assert!(!journal.truncated(), "the capture fits the journal");
        Presented { immersion, journal, output, parts }
    }

    /// Samples `SKIP..SKIP + count` of the frames, by side, from an
    /// interleaved output whose frame 0 is at `lead`.
    fn window(output: &[i16], lead: usize, count: usize) -> Sides {
        [0, 1].map(|e| (SKIP..SKIP + count).map(|n| f32::from(output[2 * (lead + n) + e]) / 32768.0).collect())
    }

    fn faded(mut sides: Sides) -> Sides {
        let fade = (FADE_SECONDS * RATE) as usize;
        let n = sides[0].len();
        for side in &mut sides {
            for t in 0..fade {
                let w = (0.5 - 0.5 * (std::f64::consts::PI * t as f64 / fade as f64).cos()) as f32;
                side[t] *= w;
                side[n - 1 - t] *= w;
            }
        }
        sides
    }

    /// A direction uniformly on the part of the sphere the KEMAR set covers
    /// (elevation -40 to 90).
    fn somewhere(rng: &mut Rng) -> Direction {
        let low = (-40f64).to_radians().sin();
        let z = low + (1.0 - low) * rng.uniform();
        ((rng.uniform() * 360.0) as f32, z.asin().to_degrees() as f32)
    }

    /// The journal's frames mixed again by a fresh renderer: each voice
    /// routed as the journal had it at that frame, the presentation's
    /// sources placed where `place` says (once each, kept in `places`), and
    /// the CD as `cd` says. The output's sample n is frame n's.
    fn remix(journal: &Journal, places: &mut BTreeMap<u32, Direction>, mut place: impl FnMut(u32) -> Direction, cd: CdRoute) -> Vec<i16> {
        // The journal's routes, where they changed.
        let mut changes: Vec<(usize, [Route; VOICES])> = Vec::new();
        let mut n = 0;
        journal.replay(|routes, _| {
            if changes.last().map(|(_, last)| last) != Some(routes) {
                changes.push((n, *routes));
            }
            n += 1;
        });
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        renderer.set_cd_route(cd);
        let mut out = Vec::with_capacity(2 * journal.frames().len());
        let mut current = [Route::Original; VOICES];
        let mut next = 0;
        for (n, frame) in journal.frames().iter().enumerate() {
            while next < changes.len() && changes[next].0 == n {
                for (v, &route) in changes[next].1.iter().enumerate() {
                    if route != current[v] {
                        renderer.set_route(v, route);
                        if let Route::Source(id) = route {
                            let (azimuth, elevation) = *places.entry(id).or_insert_with(|| place(id));
                            renderer.set_source(id, Target::Listener(hrtf::direction(azimuth, elevation)), true);
                        }
                        current[v] = route;
                    }
                }
                next += 1;
            }
            renderer.push(frame, &mut out);
        }
        out
    }

    /// The CD pair's bands at random places, as voices the renderer places:
    /// each band of each side (an ideal band of the excerpt's transform)
    /// becomes a voice at the pan law's centre on a voice silent throughout.
    fn cd_bands(journal: &Journal, places: &mut BTreeMap<u32, Direction>, rng: &mut Rng) -> Vec<i16> {
        let frames = journal.frames();
        let free: Vec<usize> = (0..VOICES).rev().filter(|&v| frames.iter().all(|f| f.voice[v] == 0)).take(2 * BANDS.len()).collect();
        assert_eq!(free.len(), 2 * BANDS.len(), "voices to carry the bands");
        let n = frames.len().next_power_of_two();
        let fft = xem_binaural::fft::Fft::new(n);
        let mut bands: Vec<Vec<i16>> = Vec::new();
        for e in 0..2 {
            let mut spectrum: Vec<xem_binaural::fft::Complex> = (0..n).map(|t| xem_binaural::fft::Complex::new(frames.get(t).map_or(0.0, |f| f.cd[e] as f32), 0.0)).collect();
            fft.forward(&mut spectrum);
            let mut low = 0.0;
            for &high in &BANDS {
                let mut band = spectrum.clone();
                for (k, bin) in band.iter_mut().enumerate() {
                    let f = k.min(n - k) as f64 * RATE / n as f64;
                    if !(f >= low && f < high) {
                        *bin = xem_binaural::fft::Complex::ZERO;
                    }
                }
                fft.inverse(&mut band);
                bands.push(band.iter().take(frames.len()).map(|c| c.re.round().clamp(-32768.0, 32767.0) as i16).collect());
                low = high;
            }
        }
        let ids: Vec<u32> = (0..bands.len() as u32).map(|b| 1000 + b).collect();
        for &id in &ids {
            places.entry(id).or_insert_with(|| somewhere(rng));
        }
        let (l, r) = pan::gains(0x4000);
        let volume = [(((l * 0x3FFF) >> 15) << 1) as i16, (((r * 0x3FFF) >> 15) << 1) as i16];
        let mut renderer = Renderer::new();
        renderer.set_enabled(true);
        renderer.reset();
        for (b, &v) in free.iter().enumerate() {
            renderer.set_route(v, Route::Source(ids[b]));
            let (azimuth, elevation) = places[&ids[b]];
            renderer.set_source(ids[b], Target::Listener(hrtf::direction(azimuth, elevation)), true);
        }
        let mut out = Vec::with_capacity(2 * frames.len());
        for (n, frame) in frames.iter().enumerate() {
            let mut frame = *frame;
            frame.cd = [0, 0];
            for (b, &v) in free.iter().enumerate() {
                frame.voice[v] = bands[b][n];
                frame.volume[v] = volume;
            }
            renderer.push(&frame, &mut out);
        }
        out
    }

    /// The excerpts `--only` names (every one without it), each with the
    /// user's state `--state` gives it; refused when a name is not an
    /// excerpt's, a state is given twice or for an excerpt left out, or a
    /// state file is missing.
    fn chosen(only: Option<Vec<String>>, states: Vec<(String, PathBuf)>) -> Vec<(&'static Excerpt, Option<PathBuf>)> {
        let names: Vec<&str> = EXCERPTS.iter().map(|e| e.name).collect();
        let valid = names.join(", ");
        for name in only.iter().flatten().chain(states.iter().map(|(name, _)| name)) {
            if !names.contains(&name.as_str()) {
                refuse(&format!("no excerpt is called {name:?} (the excerpts: {valid})"));
            }
        }
        let made: Vec<&'static Excerpt> = EXCERPTS.iter().filter(|e| only.as_ref().is_none_or(|o| o.iter().any(|n| n == e.name))).collect();
        for (i, (name, path)) in states.iter().enumerate() {
            if states[..i].iter().any(|(other, _)| other == name) {
                refuse(&format!("--state gives {name} twice"));
            }
            if !made.iter().any(|e| e.name == name) {
                refuse(&format!("--state gives {name}, which --only leaves out"));
            }
            if !path.is_file() {
                refuse(&format!("--state {name}={}: no such file", path.display()));
            }
        }
        made.into_iter().map(|e| (e, states.iter().find(|(name, _)| name == e.name).map(|(_, path)| path.clone()))).collect()
    }

    pub fn main(out: &Path, seed: u64, only: Option<Vec<String>>, states: Vec<(String, PathBuf)>) {
        let started = std::time::Instant::now();
        let chosen = chosen(only, states);
        let states_dir = out.join("states");
        std::fs::create_dir_all(&states_dir).expect("the states' directory");
        let mut files = Files::new(out);
        let mut excerpts = Vec::new();
        for (excerpt, user_state) in chosen {
            let begun = std::time::Instant::now();
            let state = match &user_state {
                Some(path) => path.clone(),
                None => {
                    let path = states_dir.join(format!("{}.xemstate", excerpt.name));
                    make_state(excerpt, &path);
                    path
                }
            };
            let count = (excerpt.seconds * RATE) as usize;
            let frames = ((SKIP + count) as f64 / (RATE / 60.0)).ceil() as u64 + 10;
            let presented: Vec<Presented> =
                std::thread::scope(|scope| IMMERSIONS.map(|i| scope.spawn({ let state = state.clone(); move || present(&state, i, frames) })).into_iter().map(|h| h.join().expect("a presentation")).collect());
            for p in &presented {
                assert_eq!(p.journal.replay(|_, _| {}), p.output, "{}: the capture mixed again is the output", excerpt.name);
                assert!(p.journal.frames().len() >= SKIP + count, "{}: the capture is long enough", excerpt.name);
            }
            // The game is the same in every presentation: the SPU's own
            // output (the original Stereo mix) is identical.
            let original: Vec<[i16; 2]> = presented[0].journal.frames()[..SKIP + count].iter().map(|f| f.output).collect();
            for p in &presented[1..] {
                assert!(p.journal.frames()[..SKIP + count].iter().map(|f| f.output).eq(original.iter().copied()), "{}: immersion {} ran the game differently", excerpt.name, p.immersion);
            }
            let reference: Sides = [0, 1].map(|e| original[SKIP..].iter().map(|o| f32::from(o[e]) / 32768.0).collect());
            let music: std::collections::BTreeSet<u32> = presented.iter().flat_map(|p| p.parts.keys().copied()).collect();
            let cd = presented[0].journal.frames().iter().any(|f| f.cd != [0, 0]);
            let mut rng = Rng::new(seed_of(seed, &format!("anchor-{}", excerpt.name)));
            let mut places: BTreeMap<u32, Direction> = BTreeMap::new();
            let mut conditions: Vec<(String, Sides)> = vec![("reference".to_owned(), reference)];
            let estimate = matches!(excerpt.setup, Setup::TitleMovie);
            if estimate {
                assert!(cd && music.is_empty(), "{}: the movie's music is CD audio and nothing is placed", excerpt.name);
            }
            for p in &presented {
                let sides = if estimate {
                    // The movie keeps its mix: the estimate is mixed again.
                    let remixed = remix(&p.journal, &mut BTreeMap::new(), |_| (0.0, 0.0), CdRoute::Upmix { immersion: f32::from(p.immersion) / 100.0 });
                    window(&remixed, 0, count)
                } else {
                    window(&p.output, p.journal.lead(), count)
                };
                conditions.push((format!("immersion-{}", p.immersion), sides));
            }
            let seventy = presented.iter().find(|p| p.immersion == 70).expect("immersion 70");
            let anchor = if estimate {
                cd_bands(&seventy.journal, &mut places, &mut rng)
            } else {
                // Positional Effects are off: every placed source is a part.
                remix(&seventy.journal, &mut places, |_| somewhere(&mut rng), CdRoute::Original)
            };
            if !estimate {
                let unknown: Vec<&u32> = places.keys().filter(|id| !music.contains(*id)).collect();
                assert!(unknown.is_empty(), "{}: placed sources the status never listed as music: {unknown:?}", excerpt.name);
            }
            conditions.push(("anchor".to_owned(), window(&anchor, 0, count)));
            // Identical conditions (an estimate with nothing to the sides,
            // say) are noted.
            let mut identical: Vec<Vec<String>> = Vec::new();
            for (i, (a, x)) in conditions.iter().enumerate() {
                for (b, y) in &conditions[i + 1..] {
                    if x == y {
                        match identical.iter_mut().find(|g| g.contains(a)) {
                            Some(group) if !group.contains(b) => group.push(b.clone()),
                            Some(_) => {}
                            None => identical.push(vec![a.clone(), b.clone()]),
                        }
                    }
                }
            }
            let mut described = serde_json::Map::new();
            for (name, sides) in conditions {
                let sides = faded(sides);
                let file = format!("coherence/{}/{name}.wav", excerpt.name);
                files.wav(&file, &sides);
                let lufs = loudness(&sides);
                let iacc: Vec<f64> = measure::iacc(&sides, RATE).iter().map(|c| rounded(*c, 3)).collect();
                described.insert(name, json!({"file": file, "loudness": rounded(lufs, 2), "gain_db": rounded(TARGET_LUFS - lufs, 2), "peak": rounded(f64::from(peak(&sides)), 4), "iacc": iacc}));
            }
            let parts: Vec<Value> = IMMERSIONS
                .iter()
                .zip(&presented)
                .map(|(i, p)| json!({"immersion": i, "parts": p.parts.iter().map(|(id, part)| { let mut part = part.clone(); part["id"] = json!(id); part }).collect::<Vec<_>>()}))
                .collect();
            let how = match excerpt.setup {
                Setup::Field { map, frames } => format!("enter_field {map}, {frames} frames"),
                Setup::WorldMap => "boot_mode 3 (the world map), 900 frames".to_owned(),
                Setup::Battle => "the debug battle selector (mode_battle_standalone, boot_mode 2) started as it stands, 1 200 frames".to_owned(),
                Setup::EventBattle => "enter_field 2, Cross every 30 frames until the battle mode, 300 frames".to_owned(),
                Setup::TitleMovie => "boot, 360 frames (the title movie)".to_owned(),
            };
            // A state of the user's is labelled as theirs: the page shows the
            // label, and the built-in one names another scene.
            let (label, kind, shown) = match &user_state {
                Some(path) => {
                    let file = path.file_name().map_or_else(|| path.display().to_string(), |n| n.to_string_lossy().into_owned());
                    (format!("Your own state {file} (in place of the {} excerpt)", excerpt.name), "the user's state".to_owned(), path.display().to_string())
                }
                None => (excerpt.label.to_owned(), excerpt.kind.to_owned(), format!("states/{}.xemstate", excerpt.name)),
            };
            let state_sha256 = hex(&digest(&std::fs::read(&state).expect("the state")));
            println!("{}: {} parts, CD {}, identical {:?}, {:.1} s", excerpt.name, music.len(), cd, identical, begun.elapsed().as_secs_f64());
            excerpts.push(json!({
                "id": excerpt.name, "label": label, "kind": kind, "seconds": excerpt.seconds,
                "state": shown, "state_sha256": state_sha256,
                "how": if user_state.is_some() { "the user's state".to_owned() } else { how },
                "capture": {"warm_frames": WARM, "skipped_samples": SKIP, "fade_seconds": FADE_SECONDS},
                "conditions": described,
                "identical": identical,
                "cd_input": cd,
                "estimate": estimate,
                "anchor": {"how": if estimate { "the CD pair's bands (to 250, 500, 1000, 2000, 4000 Hz and above) at random places" } else { "each musical part (the presentation's source at immersion 70) at a random place" },
                    "places": places.iter().map(|(id, (a, e))| json!({"id": id, "azimuth": rounded(f64::from(*a), 1), "elevation": rounded(f64::from(*e), 1)})).collect::<Vec<_>>()},
                "parts": parts,
            }));
        }
        // The sessions: every excerpt once per attribute, its attributes
        // together (in a shuffled order), the excerpts shuffled, the
        // conditions shuffled on every trial.
        let attributes = [
            ("coherence", "Coherence", "Do the music's parts hold together as one ensemble?", "One ensemble: every part belongs", "Falls apart into scattered, unrelated sounds", true),
            ("stability", "Lead and foundation", "Do the melody and the bass and rhythm stay steady in their places?", "Rock steady", "Wandering or jumping", true),
            ("envelopment", "Envelopment", "How much does the music surround you?", "All around me", "Not at all", false),
            ("tone", "Tone colour", "How close is the timbre to the reference's?", "The same", "Very different", true),
            ("preference", "Preference", "How much do you like it?", "Best", "Worst", false),
        ];
        let rated = ["reference", "anchor", "immersion-0", "immersion-35", "immersion-70", "immersion-100"];
        let mut sessions = Vec::new();
        for session in 0..2 {
            let mut rng = Rng::new(seed_of(seed, &format!("coherence-session-{session}")));
            let mut order: Vec<String> = excerpts.iter().map(|e| e["id"].as_str().unwrap().to_owned()).collect();
            rng.shuffle(&mut order);
            let name = format!("C{}", session + 1);
            let mut trials = Vec::new();
            for excerpt in &order {
                let mut attribute_order: Vec<&str> = attributes.iter().map(|a| a.0).collect();
                rng.shuffle(&mut attribute_order);
                for attribute in attribute_order {
                    let mut conditions = rated.map(str::to_owned).to_vec();
                    rng.shuffle(&mut conditions);
                    trials.push(json!({"trial": format!("{name}-{:02}", trials.len() + 1), "excerpt": excerpt, "attribute": attribute, "order": conditions}));
                }
            }
            sessions.push(json!({"name": name, "trials": trials}));
        }
        let generated = json!({
            "tool": "cargo run --release -p xem-headless --example listening -- music",
            "source": commit(),
            "seed": seed,
            "sample_rate": SAMPLE_RATE,
            "disc": disc().file_name().map(|n| n.to_string_lossy().into_owned()),
            "presentation": {"sound_mode": "Stereo (the game's, under Binaural)", "spatial_music": true, "positional_effects": false, "head_tracked_music": false, "immersions": IMMERSIONS},
        });
        let manifest = json!({
            "kind": "xem-listening-coherence",
            "version": 1,
            "level": {"target_lufs": TARGET_LUFS, "note": "the page applies each condition's gain_db, which brings its loudness (ITU-R BS.1770-4) to the target"},
            "attributes": attributes.iter().map(|(id, label, question, high, low, top)| json!({"id": id, "label": label, "question": question, "high": high, "low": low, "reference_top": top})).collect::<Vec<_>>(),
            "conditions": {
                "reference": "the original Stereo output (hidden among the rated ones; also the labelled reference)",
                "anchor": "the disintegrated low anchor: the parts (or the CD pair's bands) at random places",
                "immersion-0": "Binaural Spatial Music at immersion 0 (the stereo arc ahead)",
                "immersion-35": "Binaural Spatial Music at immersion 35",
                "immersion-70": "Binaural Spatial Music at immersion 70 (the default)",
                "immersion-100": "Binaural Spatial Music at immersion 100",
            },
            "excerpts": excerpts,
            "sessions": sessions,
        });
        let manifest = files.manifest(manifest, generated);
        write_json(&out.join("coherence.json"), &manifest);
        println!("music excerpts in {:.1} s", started.elapsed().as_secs_f64());
    }
}
