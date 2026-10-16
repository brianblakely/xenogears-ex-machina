//! The renderer's effective binaural impulse responses, measured through
//! [`Renderer`] as the game's voices reach it, for the auditory models that
//! stand in for listeners (`tools/binaural_models/evaluate.m`;
//! docs/runtime.md, Binaural sound mode).
//!
//! A voice at the centre of the Stereo pan law (volume 0x3FFF, the main
//! volume full) plays a periodic noise (a flat spectrum with random phases,
//! 8 192 samples a period) placed in a direction. The output's average over
//! five periods, after a first one in which the filters' tails and the bass
//! shelf settle, divided by the noise's spectrum (circular deconvolution) is
//! the response, per unit of the source's signal at its gain before
//! panning: the HRTF pair as the renderer applies it, its blocks,
//! interpolation, interaural delay, levelling and bass shelf included,
//! kept for 2 048 samples from 64 before the voice's sample (the
//! fractional interaural delay rings on both sides of it). The noise keeps
//! the output far below the renderer's soft knee, where it is linear; the
//! peak, the noise floor and the energy outside the samples kept are
//! reported.
//!
//! Directions: the KEMAR set's measured ones (368 on the right, mirrored to
//! 710), a 5-degree grid (azimuth 0 to 355, elevation -40 to 85, and
//! straight up), the places the music's arrangement puts parts at full
//! immersion and the premixed-music estimate's four loudspeakers. Also
//! written: the KEMAR set's own responses at its measured directions,
//! untouched, and, as a control, the original Stereo pan law heard over
//! headphones (a direction's lateral position as the pan, nothing else).
//!
//!     cargo run --release -p xem-binaural --example rendered_hrirs [-- OUT_DIR]
//!
//! OUT_DIR (`build/binaural` at the repository's root by default) receives
//! `hrirs.json`, which says what each set is (its file, response length,
//! directions and the measurement's conditions), and one little-endian
//! float32 file a set holding its responses as [direction][ear: left,
//! right][sample]. Directions are (azimuth clockwise from ahead, elevation
//! up) in degrees, the renderer's convention. `tests/models.rs` measures
//! the sets again through [`sets`] and checks they are the ones the models
//! last heard.

use serde_json::{Value, json};
use std::path::PathBuf;
use xem_binaural::fft::{Complex, Fft};
use xem_binaural::hrtf::{self, Hrtf};
use xem_binaural::{BLOCK, Renderer, Route, SAMPLE_RATE, Target, UPMIX_SPEAKERS, music, pan};
use xem_spu::TapFrame;

/// Samples a period of the noise: far longer than any response (the
/// filters reach 128 samples, the bass shelf rings for under a thousand).
const PERIOD: usize = 8192;
/// Periods played: the first settles, the others are averaged.
const PERIODS: usize = 6;
/// Samples kept of each rendered response, from [`LEAD`] before the
/// voice's sample reaches it.
const LENGTH: usize = 2048;
/// The interaural delay's fractional part, applied by phase, rings on both
/// sides of the response, and a block's convolution can put it ahead of
/// the sample (up to 2e-4 of the energy, in the last samples of the period
/// before it): the samples kept start this far before.
const LEAD: usize = 64;
/// The noise's level (RMS, full scale 1).
const RMS: f64 = 0.03;
/// The driver's volume for the voice (through the pan law's centre to its
/// registers) and the SPU's current main volume.
const VOLUME: i32 = 0x3FFF;
const MAIN: i16 = 0x7FFE;
/// The output stays below this share of full scale: the knee is at 0.85.
const MOST: f32 = 0.6;

/// A pseudo-random u64 from a counter (splitmix64).
fn hash(n: u64) -> u64 {
    let mut x = n.wrapping_add(0x9E37_79B9_7F4A_7C15);
    x = (x ^ (x >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    x = (x ^ (x >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    x ^ (x >> 31)
}

/// One period of the noise as the voice's samples, and the spectrum of
/// those samples (full scale 1).
fn noise(fft: &Fft) -> (Vec<i16>, Vec<Complex>) {
    let mut spectrum = vec![Complex::ZERO; PERIOD];
    spectrum[0] = Complex::new(1.0, 0.0);
    spectrum[PERIOD / 2] = Complex::new(1.0, 0.0);
    for k in 1..PERIOD / 2 {
        let phase = (hash(k as u64) >> 11) as f64 / (1u64 << 53) as f64 * std::f64::consts::TAU;
        spectrum[k] = Complex::cis(phase);
        spectrum[PERIOD - k] = spectrum[k].conj();
    }
    fft.inverse(&mut spectrum);
    let rms = (spectrum.iter().map(|c| f64::from(c.re).powi(2)).sum::<f64>() / PERIOD as f64).sqrt();
    let samples: Vec<i16> = spectrum.iter().map(|c| (f64::from(c.re) / rms * RMS * 32768.0).round().clamp(-32768.0, 32767.0) as i16).collect();
    let mut exact: Vec<Complex> = samples.iter().map(|s| Complex::new(f32::from(*s) / 32768.0, 0.0)).collect();
    fft.forward(&mut exact);
    (samples, exact)
}

/// What one direction's measurement gave.
struct Response {
    ears: [Vec<f32>; 2],
    /// The output's largest sample (full scale 1).
    peak: f32,
    /// The RMS of the period's middle half (where nothing is left but the
    /// output's rounding), against the response's largest sample, dB.
    floor_db: f32,
    /// The share of the period's energy outside the samples kept (that
    /// rounding's included).
    outside: f64,
}

/// The renderer's response in `direction` (azimuth clockwise, elevation;
/// degrees).
fn measure(azimuth: f32, elevation: f32, samples: &[i16], spectrum: &[Complex], fft: &Fft) -> Response {
    let mut renderer = Renderer::new();
    renderer.set_enabled(true);
    renderer.reset();
    renderer.set_route(0, Route::Source(1));
    renderer.set_source(1, Target::Listener(hrtf::direction(azimuth, elevation)), true);
    let (l, r) = pan::gains(0x4000);
    let volume = [(((l * VOLUME) >> 15) << 1) as i16, (((r * VOLUME) >> 15) << 1) as i16];
    let mut frame = TapFrame { main: [MAIN, MAIN], ..Default::default() };
    frame.volume[0] = volume;
    let mut out = Vec::with_capacity(2 * PERIOD * PERIODS);
    for _ in 0..PERIODS {
        for &s in samples {
            frame.voice[0] = s;
            let mixed = |e: usize| ((((i32::from(s) * i32::from(volume[e])) >> 15) * i32::from(MAIN)) >> 15) as i16;
            frame.output = [mixed(0), mixed(1)];
            renderer.push(&frame, &mut out);
        }
    }
    assert_eq!(out.len(), 2 * PERIOD * PERIODS, "the periods are whole blocks");
    let peak = out[2 * PERIOD..].iter().map(|s| f32::from(*s).abs() / 32768.0).fold(0.0, f32::max);
    assert!(peak < MOST, "{azimuth}/{elevation}: the output reaches {peak} of full scale, too close to the knee");
    // The gain the voice reaches the filters with: its pre-pan gain and the
    // main volume.
    let gain = pan::unpan(volume).expect("the pan law's volumes").gain * f32::from(MAIN) / 32768.0;
    let ears = [0, 1].map(|e| {
        let mut average = vec![Complex::ZERO; PERIOD];
        for (n, slot) in average.iter_mut().enumerate() {
            let sum: f64 = (1..PERIODS).map(|p| f64::from(out[2 * (p * PERIOD + n) + e])).sum();
            *slot = Complex::new((sum / (PERIODS - 1) as f64 / 32768.0) as f32, 0.0);
        }
        fft.forward(&mut average);
        for (y, x) in average.iter_mut().zip(spectrum) {
            let d = x.norm_sqr();
            *y = y.mul(x.conj()).scale(1.0 / d);
        }
        fft.inverse(&mut average);
        average.iter().map(|c| c.re / gain).collect::<Vec<f32>>()
    });
    // The period is circular: the response from LEAD before the sample.
    let ears = ears.map(|ear| (0..PERIOD).map(|n| ear[(n + PERIOD - LEAD) % PERIOD]).collect::<Vec<f32>>());
    let energy = |range: std::ops::Range<usize>| ears.iter().flat_map(|ear| &ear[range.clone()]).map(|s| f64::from(*s).powi(2)).sum::<f64>();
    let largest = ears.iter().flatten().fold(0f32, |m, s| m.max(s.abs()));
    let floor = (energy(PERIOD / 4..3 * PERIOD / 4) / PERIOD as f64).sqrt();
    Response {
        outside: 1.0 - energy(0..LENGTH) / energy(0..PERIOD),
        ears: ears.map(|mut ear| {
            ear.truncate(LENGTH);
            ear
        }),
        peak,
        floor_db: 20.0 * (floor as f32 / largest).log10(),
    }
}

/// A set of responses, written as float32 and described in the JSON.
pub struct Set {
    pub name: &'static str,
    pub kind: &'static str,
    pub length: usize,
    pub directions: Vec<(f32, f32)>,
    pub labels: Vec<String>,
    pub ears: Vec<[Vec<f32>; 2]>,
    pub notes: Value,
}

fn rendered(name: &'static str, directions: Vec<(f32, f32)>, labels: Vec<String>, samples: &[i16], spectrum: &[Complex]) -> Set {
    let threads = std::thread::available_parallelism().map_or(1, |n| n.get()).min(4);
    let chunk = directions.len().div_ceil(threads);
    let responses: Vec<Response> = std::thread::scope(|scope| {
        let workers: Vec<_> = directions
            .chunks(chunk)
            .map(|part| {
                scope.spawn(move || {
                    let fft = Fft::new(PERIOD);
                    part.iter().map(|&(a, e)| measure(a, e, samples, spectrum, &fft)).collect::<Vec<_>>()
                })
            })
            .collect();
        workers.into_iter().flat_map(|w| w.join().expect("a measurement")).collect()
    });
    let peak = responses.iter().map(|r| r.peak).fold(0.0, f32::max);
    let floor = responses.iter().map(|r| r.floor_db).fold(f32::MIN, f32::max);
    let outside = responses.iter().map(|r| r.outside).fold(0.0, f64::max);
    println!(
        "{name}: {} directions, output peak {peak:.3} of full scale, noise floor {floor:.1} dB under each response's peak at worst, {outside:.1e} of the energy outside the {LENGTH} samples kept at most",
        directions.len()
    );
    Set {
        name,
        kind: "rendered",
        length: LENGTH,
        directions,
        labels,
        ears: responses.into_iter().map(|r| r.ears).collect(),
        notes: json!({"lead": LEAD, "peak": peak, "noise_floor_db": floor, "energy_outside": outside}),
    }
}

/// Every set the models hear, in the order `hrirs.json` lists them: the
/// rendered responses at the measured directions, the grid, the music's
/// places and the estimate's loudspeakers, the KEMAR set's own and the
/// Stereo control.
pub fn sets() -> Vec<Set> {
    let hrtf = Hrtf::kemar();
    let (samples, spectrum) = noise(&Fft::new(PERIOD));
    let measured = hrtf.directions();
    let mut grid: Vec<(f32, f32)> = (-8..18).flat_map(|e| (0..72).map(move |a| (a as f32 * 5.0, e as f32 * 5.0))).collect();
    grid.push((0.0, 90.0));
    // Every place the arrangement can put a part at full immersion: the
    // stereo arc ahead, supports to the sides and behind, pads to the sides
    // and above (music.rs).
    let mut places: Vec<(f32, f32, String)> = [-music::ARC, -music::ARC / 2.0, 0.0, music::ARC / 2.0, music::ARC].iter().map(|&a| (a, 0.0, "arc".to_owned())).collect();
    for &a in &music::SUPPORT_SIDES {
        places.extend([(a, music::SUPPORT_ELEVATION, "support side".to_owned()), (-a, music::SUPPORT_ELEVATION, "support side".to_owned())]);
    }
    places.extend(music::SUPPORT_REAR.iter().map(|&a| (a, music::SUPPORT_ELEVATION, "support rear".to_owned())));
    for &a in &music::PAD_SIDES {
        places.extend([(a, music::PAD_SIDE_ELEVATION, "pad side".to_owned()), (-a, music::PAD_SIDE_ELEVATION, "pad side".to_owned())]);
    }
    places.extend(music::PAD_CENTRES.iter().map(|&a| (a, music::PAD_CENTRE_ELEVATION, "pad centre".to_owned())));
    let speakers: Vec<(f32, f32)> = UPMIX_SPEAKERS.iter().map(|&a| (a, 0.0)).collect();
    let mut sets = vec![
        rendered("rendered-measured", measured.clone(), Vec::new(), &samples, &spectrum),
        rendered("rendered-grid", grid, Vec::new(), &samples, &spectrum),
        rendered("rendered-music", places.iter().map(|p| (p.0, p.1)).collect(), places.iter().map(|p| p.2.clone()).collect(), &samples, &spectrum),
        rendered("rendered-upmix", speakers, ["front left", "front right", "rear left", "rear right"].map(str::to_owned).to_vec(), &samples, &spectrum),
    ];
    // The set itself, as measured.
    let raw = hrtf.measured();
    sets.push(Set {
        name: "kemar-raw",
        kind: "raw",
        length: hrtf::FRAMES,
        directions: raw.iter().map(|m| (m.azimuth, m.elevation)).collect(),
        labels: Vec::new(),
        ears: raw.iter().map(|m| m.ears.map(|ear| ear.to_vec())).collect(),
        notes: json!({"source": "MIT KEMAR, diffuse-field equalized (crates/xem-binaural/hrtf/README.md); the left half mirrors the right"}),
    });
    // The control: a voice at its pre-pan gain panned by the Stereo law to
    // the direction's lateral position (sin(azimuth) cos(elevation): the
    // centre ahead, behind, above and below; an edge at either side), as the
    // original mix sends it to the headphones.
    let stereo = |(azimuth, elevation): (f32, f32)| {
        let x = azimuth.to_radians().sin() * elevation.to_radians().cos();
        let pan = (16384.0 + x * if x < 0.0 { 16384.0 } else { 16128.0 }).round() as i32;
        let (l, r) = pan::gains(pan);
        [l, r].map(|g| {
            let mut ear = vec![0f32; hrtf::FRAMES];
            ear[0] = g as f32 / 32768.0;
            ear
        })
    };
    sets.push(Set {
        name: "stereo-measured",
        kind: "stereo",
        length: hrtf::FRAMES,
        directions: measured.clone(),
        labels: Vec::new(),
        ears: measured.iter().map(|&d| stereo(d)).collect(),
        notes: json!({"pan": "0x4000 + sin(azimuth) cos(elevation) times 0x4000 (left) or 0x3F00 (right), through the driver's Stereo law"}),
    });
    sets
}

fn main() {
    let out = std::env::args()
        .nth(1)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../build/binaural"));
    std::fs::create_dir_all(&out).expect("the output directory");
    let out = out.canonicalize().expect("the output directory");
    let sets = sets();
    let mut described = Vec::new();
    for set in &sets {
        let file = format!("{}.f32", set.name);
        let mut bytes = Vec::with_capacity(set.ears.len() * 2 * set.length * 4);
        for ears in &set.ears {
            for ear in ears {
                assert_eq!(ear.len(), set.length);
                bytes.extend(ear.iter().flat_map(|s| s.to_le_bytes()));
            }
        }
        std::fs::write(out.join(&file), bytes).expect("a set's file");
        let mut entry = json!({
            "name": set.name,
            "kind": set.kind,
            "file": file,
            "length": set.length,
            "directions": set.directions.iter().map(|&(a, e)| json!([a, e])).collect::<Vec<_>>(),
            "notes": set.notes,
        });
        if !set.labels.is_empty() {
            entry["labels"] = json!(set.labels);
        }
        described.push(entry);
    }
    let description = json!({
        "sample_rate": SAMPLE_RATE,
        "layout": "float32 little-endian, [direction][ear: left, right][sample]",
        "directions": "degrees: azimuth clockwise from ahead (90 to the right), elevation up",
        "measurement": {
            "renderer": "xem_binaural::Renderer, enabled, one voice routed to one source placed in the direction",
            "noise": {"kind": "flat spectrum, random phases (splitmix64 of the bin)", "period": PERIOD, "periods": PERIODS, "averaged": PERIODS - 1, "rms": RMS},
            "voice_volume": VOLUME,
            "main_volume": MAIN,
            "block": BLOCK,
            "lead": LEAD,
            "scale": "per unit of the source's signal at its gain before panning (the pan law's centre is 0x5A00 of 0x8000 a side)",
        },
        "sets": described,
    });
    let path = out.join("hrirs.json");
    std::fs::write(&path, serde_json::to_string_pretty(&description).expect("JSON")).expect("hrirs.json");
    println!("wrote {}", path.display());
}
