//! Musical coherence of the Binaural arrangement by proxies that are not
//! calibrated against listeners (local only: the user's disc 1 and a game
//! module build). No validated model of a mix's spatial coherence exists
//! (the SAQI's spatial disintegration has none), so these are regression
//! measures of the signals, not predictions of what people hear: in Lahan's
//! opening scene (field 2 from its entry, 17 parts of the village's song
//! sounding and nothing else placed), presented at music immersion 0, 35,
//! 70 and 100 with a tap capture over ten seconds,
//!
//! - the mix keeps the original Stereo mix's loudness (ITU-R BS.1770-4,
//!   gated) and, ear by ear, its long-term spectrum in third octaves but
//!   for the HRTF pairs' colouring (within 3 dB to 4 kHz; above, the
//!   frontal pairs lower 5 to 8 kHz);
//! - every immersion above 0 is much more diffuse than the stereo arc from
//!   500 Hz to 4 kHz (the interaural cross-correlation of each octave there
//!   well under immersion 0's), though not step by step: the parts placed
//!   behind on or near the median plane, which the ears hear alike, pass
//!   the sides on their way there and correlate again at full immersion
//!   (each group alone shows it); the other octaves stay about as at
//!   immersion 0 (at 125 and 250 Hz the places matter little, and at 8 kHz
//!   the HRTF pairs already decorrelate the arc); the ears stay correlated
//!   far above chance;
//! - the lead, the bass and the rhythm, each alone (its voices replayed
//!   from the capture), are heard where the original pans them on the
//!   stereo arc at every immersion: the interaural delay of each 93 ms
//!   window (200 Hz to 1.4 kHz) read through the KEMAR head's own delays,
//!   against the arc angle of the original's left and right levels.
//!
//! The bounds are regression guards with some room around what this scene
//! measured, not perceptual criteria.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test coherence

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::sync::OnceLock;
use xem_binaural::fft::{Complex, Fft};
use xem_binaural::{BLOCK, Journal, Renderer, Route, Target, hrtf};
use xem_core::binaural::Config;
use xem_core::control::Session;
use xem_game::{BootOptions, NativeModule, boot};
use xem_spu::{TapFrame, VOICES};

/// Lahan's opening scene: the village's song, three scripted camera cuts,
/// no positional effect (tests/binaural.rs).
const FIELD: u32 = 2;
/// The capture: frames after the field's entry.
const FROM: u64 = 300;
const FRAMES: u64 = 600;
const IMMERSIONS: [u8; 4] = [0, 35, 70, 100];
const RATE: f64 = 44_100.0;
/// Samples skipped at the capture's start: what the block under way then
/// held has left the filters' tails and the bass shelf.
const SKIP: usize = 24 * BLOCK;

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn execute(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command}: {reply}");
    reply
}

/// What one presentation of the scene left: the capture's journal, the
/// output it presented, and each music source's role and place at the
/// immersion set (azimuth, elevation), by source id.
struct Heard {
    immersion: u8,
    journal: Journal,
    output: Vec<i16>,
    roles: BTreeMap<u32, String>,
    places: BTreeMap<u32, (f64, f64)>,
}

fn hear(disc: &Path, immersion: u8) -> Heard {
    let (mut session, _) = boot(disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
    session.set_sound_mode(1).unwrap();
    execute(&mut session, json!({"cmd": "enter_field", "map": FIELD}));
    let spu = &mut session.runtime.services().spu;
    let mut stale = vec![0; spu.audio_queued()];
    spu.take_audio(&mut stale);
    session.set_binaural(Config { enabled: true, spatial_music: true, positional_effects: true, music_immersion: immersion, head_tracked_music: false, bypass: false });
    let (mut roles, mut places) = (BTreeMap::new(), BTreeMap::new());
    for n in 0..FROM + FRAMES {
        if n == FROM {
            assert_eq!(execute(&mut session, json!({"cmd": "audio_capture", "action": "start", "tap": true})), json!({"capturing": true, "tap": true}));
        }
        assert!(session.frame().expect("a frame"));
        let spu = &mut session.runtime.services().spu;
        let mut audio = vec![0; spu.audio_queued()];
        spu.take_audio(&mut audio);
        if n >= FROM && n % 10 == 0 {
            let status = execute(&mut session, json!({"cmd": "binaural"}));
            for source in status["sources"].as_array().unwrap() {
                assert_eq!(source["kind"], "music", "only the music is placed in the scene: {source}");
                let id = source["id"].as_u64().unwrap() as u32;
                roles.insert(id, source["role"].as_str().unwrap().to_owned());
                let place = &source["placement"];
                places.insert(id, (place["azimuth"].as_f64().unwrap(), place["elevation"].as_f64().unwrap()));
            }
        }
    }
    let spu = &mut session.runtime.services().spu;
    let journal = spu.take_tap_capture().expect("a tap capture");
    let output = spu.take_capture().expect("the output alongside");
    Heard { immersion, journal, output, roles, places }
}

static HEARD: OnceLock<Result<Vec<Heard>, String>> = OnceLock::new();

/// The four presentations, each on its own thread, played once for all
/// the tests; none without the disc.
fn heard() -> Option<&'static [Heard]> {
    let disc = disc()?;
    let disc = disc.as_path();
    let result = HEARD.get_or_init(|| {
        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            std::thread::scope(|scope| IMMERSIONS.map(|i| scope.spawn(move || hear(disc, i))).into_iter().map(|h| h.join().expect("a presentation")).collect())
        }))
        .map_err(|_| "the scene's presentations failed (see the first test's panic)".to_owned())
    });
    Some(result.as_ref().expect("the scene's presentations").as_slice())
}

/// One side of what the SPU mixed, as it mixed it, of the voices `keep`
/// lets through (and the reverb return and CD input with `rest`).
fn mixed(frame: &TapFrame, e: usize, keep: impl Fn(usize) -> bool, rest: bool) -> f32 {
    if frame.muted {
        return 0.0;
    }
    let sat = |x: i32| x.clamp(-32768, 32767);
    let voices: i32 = (0..VOICES).filter(|&v| keep(v)).map(|v| (i32::from(frame.voice[v]) * i32::from(frame.volume[v][e])) >> 15).sum();
    let m = if rest { voices + i32::from(frame.reverb[e]) + frame.cd[e] } else { voices };
    (sat((sat(m) * i32::from(frame.main[e])) >> 15)) as f32 / 32768.0
}

/// A presented output's two sides (full scale 1) from the capture's first
/// frame on, without the samples skipped.
fn sides(output: &[i16], lead: usize, frames: usize) -> [Vec<f32>; 2] {
    let end = (output.len() / 2).min(lead + frames);
    [0, 1].map(|e| (lead + SKIP..end).map(|n| f32::from(output[2 * n + e]) / 32768.0).collect())
}

/// The original Stereo mix of the capture's frames.
fn original(journal: &Journal) -> [Vec<f32>; 2] {
    [0, 1].map(|e| journal.frames()[SKIP..].iter().map(|f| f32::from(f.output[e]) / 32768.0).collect())
}

/// Some sources' voices alone: presented (replayed from the capture with
/// every other voice, the reverb and the CD input left out) and in the
/// original mix.
fn stem(heard: &Heard, ids: &[u32]) -> ([Vec<f32>; 2], [Vec<f32>; 2]) {
    let mut plain = [Vec::new(), Vec::new()];
    let presented = heard.journal.replay(|routes, frame| {
        let ours = |v: usize| matches!(routes[v], Route::Source(id) if ids.contains(&id));
        for (e, side) in plain.iter_mut().enumerate() {
            side.push(mixed(frame, e, ours, false));
        }
        for v in 0..VOICES {
            if !ours(v) {
                frame.voice[v] = 0;
            }
        }
        frame.reverb = [0; 2];
        frame.cd = [0; 2];
    });
    let frames = heard.journal.frames().len();
    (sides(&presented, heard.journal.lead(), frames), plain.map(|side| side[SKIP..].to_vec()))
}

/// The transform of `x`, zero-padded to `n` points.
fn spectrum(fft: &Fft, x: &[f32], n: usize) -> Vec<Complex> {
    let mut data: Vec<Complex> = (0..n).map(|t| Complex::new(x.get(t).copied().unwrap_or(0.0), 0.0)).collect();
    fft.forward(&mut data);
    data
}

/// ITU-R BS.1770-4's K-weighting at 44.1 kHz (the analogue design of
/// libebur128: the head's shelf and the high pass), then the integrated
/// loudness of the channels (LUFS): 400 ms blocks overlapping by 75%, the
/// absolute gate at -70 and the relative one 10 LU under.
fn loudness(channels: &[&[f32]]) -> f64 {
    let biquad = |b: [f64; 3], a: [f64; 3], x: &[f32]| -> Vec<f64> {
        let (mut x1, mut x2, mut y1, mut y2) = (0.0, 0.0, 0.0, 0.0);
        x.iter()
            .map(|&s| {
                let s = f64::from(s);
                let y = b[0] * s + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2;
                (x2, x1, y2, y1) = (x1, s, y1, y);
                y
            })
            .collect()
    };
    let (f0, gain, q) = (1681.974450955533, 3.999843853973347, 0.7071752369554196);
    let k = (std::f64::consts::PI * f0 / RATE).tan();
    let (vh, vb) = (10f64.powf(gain / 20.0), 10f64.powf(gain / 20.0).powf(0.4996667741545416));
    let a0 = 1.0 + k / q + k * k;
    let shelf_b = [(vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0];
    let shelf_a = [1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0];
    let (f0, q) = (38.13547087602444, 0.5003270373238773);
    let k = (std::f64::consts::PI * f0 / RATE).tan();
    let a0 = 1.0 + k / q + k * k;
    let pass_a = [1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0];
    let weighted: Vec<Vec<f64>> = channels
        .iter()
        .map(|x| {
            let shelved: Vec<f32> = biquad(shelf_b, shelf_a, x).iter().map(|&y| y as f32).collect();
            biquad([1.0, -2.0, 1.0], pass_a, &shelved)
        })
        .collect();
    let (size, hop) = ((0.4 * RATE) as usize, (0.1 * RATE) as usize);
    let len = weighted[0].len();
    let blocks: Vec<f64> = (0..)
        .map(|b| b * hop)
        .take_while(|&at| at + size <= len)
        .map(|at| weighted.iter().map(|y| y[at..at + size].iter().map(|v| v * v).sum::<f64>() / size as f64).sum::<f64>())
        .collect();
    let level = |z: f64| -0.691 + 10.0 * z.log10();
    let gated: Vec<f64> = blocks.iter().copied().filter(|&z| level(z) > -70.0).collect();
    let relative = level(gated.iter().sum::<f64>() / gated.len() as f64) - 10.0;
    let kept: Vec<f64> = gated.into_iter().filter(|&z| level(z) > relative).collect();
    level(kept.iter().sum::<f64>() / kept.len() as f64)
}

/// Third-octave bands, 100 Hz to 10 kHz (centres).
fn third_octave_centres() -> Vec<f64> {
    (-10..=10).map(|i| 1000.0 * 2f64.powf(f64::from(i) / 3.0)).collect()
}

/// The long-term level of each third octave (dB, Hann-windowed blocks of
/// 8 192 samples overlapping by half).
fn third_octaves(x: &[f32]) -> Vec<f64> {
    const N: usize = 8192;
    let fft = Fft::new(N);
    let window: Vec<f32> = (0..N).map(|t| (0.5 - 0.5 * (std::f64::consts::TAU * t as f64 / N as f64).cos()) as f32).collect();
    let mut power = vec![0f64; N / 2 + 1];
    for at in (0..).map(|b| b * N / 2).take_while(|at| at + N <= x.len()) {
        let block: Vec<f32> = x[at..at + N].iter().zip(&window).map(|(s, w)| s * w).collect();
        for (k, bin) in spectrum(&fft, &block, N).iter().take(N / 2 + 1).enumerate() {
            power[k] += f64::from(bin.norm_sqr());
        }
    }
    third_octave_centres()
        .iter()
        .map(|&c| {
            let (lo, hi) = ((c / 2f64.powf(1.0 / 6.0) * N as f64 / RATE).round() as usize, (c * 2f64.powf(1.0 / 6.0) * N as f64 / RATE).round() as usize);
            10.0 * power[lo..hi.max(lo + 1)].iter().sum::<f64>().max(1e-30).log10()
        })
        .collect()
}

/// Octave bands' centres for the interaural cross-correlation.
const OCTAVES: [f64; 7] = [125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0];

/// The interaural cross-correlation coefficient of each octave band: the
/// largest normalized cross-correlation of the two sides, band-passed (an
/// ideal band in the whole signal's transform), within a millisecond either
/// way.
fn iacc(sides: &[Vec<f32>; 2]) -> Vec<f64> {
    let n = sides[0].len().next_power_of_two() * 2;
    let fft = Fft::new(n);
    let [l, r] = [spectrum(&fft, &sides[0], n), spectrum(&fft, &sides[1], n)];
    OCTAVES
        .iter()
        .map(|&c| {
            let (lo, hi) = ((c / 2f64.sqrt() * n as f64 / RATE) as usize, (c * 2f64.sqrt() * n as f64 / RATE) as usize);
            let mut cross = vec![Complex::ZERO; n];
            let (mut el, mut er) = (0f64, 0f64);
            for k in lo..=hi {
                cross[k] = l[k].mul(r[k].conj());
                cross[n - k] = cross[k].conj();
                el += 2.0 * f64::from(l[k].norm_sqr());
                er += 2.0 * f64::from(r[k].norm_sqr());
            }
            fft.inverse(&mut cross);
            let lags = 44;
            let best = (0..=lags).chain(n - lags..n).map(|t| f64::from(cross[t].re) * n as f64).fold(f64::MIN, f64::max);
            best / (el * er).sqrt()
        })
        .collect()
}

/// The delay of the left side behind the right (samples, between samples)
/// in 200 Hz to 1.4 kHz, searched within 20 either way (the stereo arc's
/// delays and some: a tone there has one peak in that range).
fn delay(l: &[f32], r: &[f32], fft: &Fft) -> f32 {
    let n = fft.len();
    let (a, b) = (spectrum(fft, l, n), spectrum(fft, r, n));
    let (lo, hi) = ((200.0 * n as f64 / RATE) as usize, (1400.0 * n as f64 / RATE) as usize);
    let mut cross = vec![Complex::ZERO; n];
    for k in lo..=hi {
        cross[k] = a[k].mul(b[k].conj());
        cross[n - k] = cross[k].conj();
    }
    fft.inverse(&mut cross);
    let at = |lag: i32| cross[lag.rem_euclid(n as i32) as usize].re;
    let best = (-20..=20).max_by(|&x, &y| at(x).total_cmp(&at(y))).expect("lags");
    let (y0, y1, y2) = (at(best - 1), at(best), at(best + 1));
    let curve = y0 - 2.0 * y1 + y2;
    best as f32 + if curve < 0.0 { 0.5 * (y0 - y2) / curve } else { 0.0 }
}

/// Samples of an analysis window (93 ms) and its transform.
const WINDOW: usize = 4096;

/// The KEMAR head's interaural delay along the horizontal plane's front,
/// azimuth -40 to 40 (degrees, clockwise), as the renderer places noise
/// there and [`delay`] reads it.
fn kemar_delays() -> Vec<(f32, f32)> {
    let fft = Fft::new(2 * WINDOW);
    (-40..=40)
        .map(|azimuth| {
            let mut renderer = Renderer::new();
            renderer.set_enabled(true);
            renderer.reset();
            renderer.set_route(0, Route::Source(1));
            renderer.set_source(1, Target::Listener(hrtf::direction(azimuth as f32, 0.0)), true);
            let mut frame = TapFrame { main: [0x7FFE, 0x7FFE], ..Default::default() };
            frame.volume[0] = [0x2000, 0x2000];
            let mut out = Vec::new();
            for n in 0..4 * WINDOW {
                let mut x = (n as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15);
                x ^= x >> 31;
                frame.voice[0] = ((x.wrapping_mul(0xBF58_476D_1CE4_E5B9) >> 48) as i16) / 4;
                renderer.push(&frame, &mut out);
            }
            let side = |e: usize| -> Vec<f32> { out.chunks(2).skip(2 * WINDOW).take(WINDOW).map(|p| f32::from(p[e]) / 32768.0).collect() };
            (azimuth as f32, delay(&side(0), &side(1), &fft))
        })
        .collect()
}

/// The azimuth whose KEMAR delay is `d` (between the table's entries).
fn azimuth_of(table: &[(f32, f32)], d: f32) -> f32 {
    let (first, last) = (table[0], table[table.len() - 1]);
    if d <= first.1 {
        return first.0;
    }
    if d >= last.1 {
        return last.0;
    }
    let i = table.windows(2).position(|w| w[0].1 <= d && d <= w[1].1).expect("a monotonic table");
    let (a, b) = (table[i], table[i + 1]);
    a.0 + (b.0 - a.0) * if b.1 > a.1 { (d - a.1) / (b.1 - a.1) } else { 0.0 }
}

/// Where the original pans a sound with these levels on the stereo arc
/// (degrees, 30 at an edge): the driver's pan law read back
/// (xem_binaural::pan::unpan).
fn arc_of(left: f32, right: f32) -> f32 {
    let top = left.max(right);
    let volume = [(left / top * 16_000.0).round() as i16, (right / top * 16_000.0).round() as i16];
    let unpanned = xem_binaural::pan::unpan(volume).expect("levels are positive");
    (unpanned.pan - 16384.0) / 16384.0 * xem_binaural::music::ARC
}

fn rms(x: &[f32]) -> f32 {
    (x.iter().map(|s| f64::from(*s).powi(2)).sum::<f64>() / x.len().max(1) as f64).sqrt() as f32
}

#[test]
fn the_binaural_mix_keeps_the_originals_loudness_and_its_spectrum_but_for_the_pairs_colouring() {
    let Some(heard) = heard() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    for h in heard {
        let frames = h.journal.frames().len();
        assert_eq!(h.journal.replay(|_, _| {}), h.output, "mixed again from the capture, the output is the same");
        let [l, r] = sides(&h.output, h.journal.lead(), frames);
        let [ol, or] = original(&h.journal);
        let n = l.len().min(ol.len());
        let (l, r, ol, or) = (&l[..n], &r[..n], &ol[..n], &or[..n]);
        let (presented, plain) = (loudness(&[l, r]), loudness(&[ol, or]));
        let ears = [loudness(&[l]) - loudness(&[ol]), loudness(&[r]) - loudness(&[or])];
        let [tl, tr, tol, tor] = [l, r, ol, or].map(third_octaves);
        let bands: Vec<[f64; 2]> = (0..tl.len()).map(|b| [tl[b] - tol[b], tr[b] - tor[b]]).collect();
        let worst = bands.iter().flatten().fold(0f64, |m, d| m.max(d.abs()));
        eprintln!(
            "immersion {}: loudness {presented:.2} LUFS against the original's {plain:.2} ({:+.2} LU; each ear alone {:+.2} and {:+.2}); third octaves 100 Hz-10 kHz, left and right against the original's (dB): {:?}; at most {worst:.1}",
            h.immersion,
            presented - plain,
            ears[0],
            ears[1],
            bands.iter().map(|b| format!("{:+.1}/{:+.1}", b[0], b[1])).collect::<Vec<_>>()
        );
        assert!((presented - plain).abs() <= 1.0, "immersion {}: the mix's loudness is the original's within 1 LU: {:+.2}", h.immersion, presented - plain);
        for (c, band) in third_octave_centres().iter().zip(&bands) {
            let off = band[0].abs().max(band[1].abs());
            assert!(off <= if *c <= 4000.0 { 3.0 } else { 8.0 }, "immersion {}: the {c:.0} Hz third octave is the original's but for the pairs' colouring: {band:+.1?}", h.immersion);
        }
    }
}

#[test]
fn immersion_makes_the_music_more_diffuse_than_the_arc_from_500_hz_to_4_khz_and_the_ears_stay_correlated() {
    let Some(heard) = heard() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The same sources, roles and places in every session (the presentation
    // never changes the game), placed by the immersion set.
    for h in heard {
        assert_eq!(h.roles, heard[0].roles, "immersion {}: the same parts", h.immersion);
    }
    let full = &heard[heard.len() - 1];
    assert_eq!(full.immersion, 100);
    let lateral = |&(azimuth, elevation): &(f64, f64)| (azimuth.to_radians().sin() * elevation.to_radians().cos()).asin().to_degrees();
    // The parts the arrangement moves (not the lead, bass, rhythm or the
    // uncertain ones, which keep the arc): to a side, or behind or above
    // within 25 degrees of the median plane, at full immersion.
    let moved = |near_median: bool| -> Vec<u32> {
        full.places
            .iter()
            .filter(|(id, place)| !["lead", "bass", "rhythm", "uncertain"].contains(&full.roles[*id].as_str()) && (lateral(place).abs() <= 25.0) == near_median)
            .map(|(id, _)| *id)
            .collect()
    };
    let (sides_ids, median_ids) = (moved(false), moved(true));
    let described: Vec<String> = full.places.iter().map(|(id, (a, e))| format!("{id} {} {a:.0}/{e:.0}", full.roles[id])).collect();
    eprintln!("the song's parts at full immersion (source, role, azimuth/elevation): {described:?}");
    let plain = iacc(&original(&heard[0].journal));
    let presented: Vec<Vec<f64>> = heard.iter().map(|h| iacc(&sides(&h.output, h.journal.lead(), h.journal.frames().len()))).collect();
    eprintln!("interaural cross-correlation by octave {OCTAVES:?}: original {plain:.3?}");
    for (h, c) in heard.iter().zip(&presented) {
        let groups = [&sides_ids, &median_ids].map(|ids| iacc(&stem(h, ids).0));
        eprintln!("  immersion {:3}: {c:.3?}; the parts moving to the sides {:.3?}, behind or above {:.3?}", h.immersion, groups[0], groups[1]);
    }
    // Every immersion above 0 is much more diffuse than the stereo arc from
    // 500 Hz to 4 kHz (each octave's correlation at least 0.2 under
    // immersion 0's: an immersion that moved nothing would fail); the other
    // octaves stay about as at immersion 0, at most 0.02 over it (at 125
    // and 250 Hz the places matter little, and at 8 kHz the pairs already
    // decorrelate the arc); the ears stay correlated far above chance (a
    // hundredth for unrelated ten-second signals).
    for (b, &centre) in OCTAVES.iter().enumerate() {
        let arc = presented[0][b];
        for (h, c) in heard[1..].iter().zip(&presented[1..]) {
            let i = h.immersion;
            assert!(c[b] <= arc + 0.02, "immersion {i}, {centre} Hz: at most a little more correlated than the arc: {:.3} against {arc:.3}", c[b]);
            if (500.0..=4000.0).contains(&centre) {
                assert!(c[b] <= arc - 0.2, "immersion {i}, {centre} Hz: much more diffuse than the arc: {:.3} against {arc:.3}", c[b]);
            }
            assert!(c[b] >= 0.05, "immersion {i}, {centre} Hz: the ears stay correlated: {:.3}", c[b]);
        }
    }
}

#[test]
fn the_lead_bass_and_rhythm_are_heard_where_the_original_pans_them_at_every_immersion() {
    let Some(heard) = heard() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let table = kemar_delays();
    assert!(table.windows(2).all(|w| w[1].1 >= w[0].1), "the head's delay grows towards the right: {table:?}");
    let fft = Fft::new(2 * WINDOW);
    let mut deviations: BTreeMap<String, Vec<(u8, f32)>> = BTreeMap::new();
    for h in heard {
        for role in ["lead", "bass", "rhythm"] {
            let ids: Vec<u32> = h.roles.iter().filter(|(_, r)| *r == role).map(|(id, _)| *id).collect();
            assert!(!ids.is_empty(), "the song has a {role}: {:?}", h.roles);
            let ([l, r], [ol, or]) = stem(h, &ids);
            let n = l.len().min(ol.len()) / WINDOW;
            let loud = (0..n).map(|w| rms(&l[w * WINDOW..(w + 1) * WINDOW])).fold(0f32, f32::max);
            let (mut heard_at, mut panned_at) = (Vec::new(), Vec::new());
            for w in 0..n {
                let span = w * WINDOW..(w + 1) * WINDOW;
                // Windows where the part sounds (within 20 dB of its loudest).
                if rms(&l[span.clone()]).max(rms(&r[span.clone()])) < 0.1 * loud || rms(&ol[span.clone()]).max(rms(&or[span.clone()])) <= 0.0 {
                    continue;
                }
                heard_at.push(azimuth_of(&table, delay(&l[span.clone()], &r[span.clone()], &fft)));
                panned_at.push(arc_of(rms(&ol[span.clone()]), rms(&or[span])));
            }
            let off: Vec<f32> = heard_at.iter().zip(&panned_at).map(|(a, b)| a - b).collect();
            let mean = |x: &[f32]| x.iter().sum::<f32>() / x.len() as f32;
            let sd = |x: &[f32]| (x.iter().map(|v| (v - mean(x)).powi(2)).sum::<f32>() / x.len() as f32).sqrt();
            let deviation = (off.iter().map(|d| d * d).sum::<f32>() / off.len() as f32).sqrt();
            eprintln!(
                "immersion {:3} {role:6}: {} windows; heard at {:+.1} (sd {:.1}), panned at {:+.1} (sd {:.1}); heard minus panned: rms {deviation:.1}, mean {:+.1}",
                h.immersion,
                off.len(),
                mean(&heard_at),
                sd(&heard_at),
                mean(&panned_at),
                sd(&panned_at),
                mean(&off)
            );
            assert!(off.len() >= 20, "immersion {}: the {role} sounds in the window", h.immersion);
            deviations.entry(role.to_owned()).or_default().push((h.immersion, deviation));
        }
    }
    for (role, by_immersion) in &deviations {
        for &(immersion, deviation) in by_immersion {
            assert!(deviation <= 5.0, "immersion {immersion}: the {role} is heard within 5 degrees (rms) of where the original pans it: {deviation:.1}");
        }
        let spread = by_immersion.iter().map(|d| d.1).fold(f32::MIN, f32::max) - by_immersion.iter().map(|d| d.1).fold(f32::MAX, f32::min);
        assert!(spread <= 2.0, "the {role} stays put as immersion changes: {by_immersion:?}");
    }
}
