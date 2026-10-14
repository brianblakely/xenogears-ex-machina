//! Raw playback (the game's own Mono, Stereo and Wide-Stereo, no Binaural
//! renderer) against the original recordings of the same routes, at the
//! level of the relations the sound-mode findings rest on
//! (analysis/formats/sound-modes.md); and Binaural turned on and off again
//! on the Wide route giving back the Wide output it left. Local only: it
//! needs the user's disc 1, a game module build, the retained PCSX-ReARMed
//! recordings of tests/reference-inputs/new-game-sound-mode-*.json
//! (.local/scenarios/p1-original-sound-{stereo,wide,mono}-20261002) with
//! their report (p1-original-audio-modes-20261002.json, made by
//! tools/analysis/audio_modes.py), and FreeSurround (the nix/runtime shell's
//! `freesurround`, a GPL test tool, run as a separate program).
//!
//! The two SPUs differ (xem-spu's Gaussian interpolation against the
//! recordings' "simple" one, the recordings' HLE BIOS, their reverb units,
//! and a native level 1.2 to 1.35 times theirs that only a console
//! recording can settle), so native and original are compared by the
//! relations between the modes, by the dry mix's waveform and by
//! level-free statistics, within tolerances the findings justify; the level
//! ratio is printed, never normalized.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test original_sound

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::path::{Path, PathBuf};
use std::sync::OnceLock;
use xem_core::control::Session;
use xem_game::{BootOptions, NativeModule, boot};
use xem_settings::{AudioOutput, MemoryStore, SettingChange, SettingsService};

/// Samples per frame: 44 100 Hz at the 60 Hz of both the native clock and
/// the recordings' frontend frames.
const PER_FRAME: usize = 735;
/// The routes and the driver modes they choose on the title's Sound screen.
const MODES: [(&str, u32); 3] = [("stereo", 1), ("wide", 2), ("mono", 0)];
/// How far native and original may differ in level-free statistics: the
/// correlation of left and right, the ratio of the sum's energy to the
/// difference's (dB), FreeSurround's rear share (back left and right of
/// its 5.1, of the energy). The 1.3x level and the recordings' HLE BIOS
/// leave them.
struct Tolerance {
    correlation: f64,
    sum_difference_db: f64,
    rear_share: f64,
}

/// Without reverb (the movie: CD input, which the driver keeps out of the
/// reverb) both play the same decoded sound, and only the SPUs' resampling
/// and interpolation (Gaussian against simple: a different treble) move
/// the statistics. The changes the modes make are far larger: Wide flips
/// the correlation's and the ratio's signs (-0.29 against +0.29, -2.5
/// against +2.5 dB) and moves 0.5-0.7 of the mix to FreeSurround's rears
/// (docs/runtime.md, Sound modes).
const DRY: Tolerance = Tolerance { correlation: 0.03, sum_difference_db: 0.5, rear_share: 0.05 };
/// With the reverb (the field) the two reverb units differ: the left
/// channel's dry part, (Stereo + Wide) / 2 by the findings, is the
/// recording's own waveform (asserted below) while its reverb return,
/// (Stereo - Wide) / 2, matches the recording's only loosely (a peak
/// correlation of 0.67 here, against 0.98 for the dry part) and is
/// relatively 0.7 dB weaker. The reverb is what decorrelates the channels
/// (all of it in Mono, whose dry voices are centred), so the native mix
/// is more correlated there, most in Mono. These bounds take that and
/// still tell every mode from the others (in the recordings' field their
/// correlations are at least 0.37 apart, their ratios 4.7 dB).
const REVERB: Tolerance = Tolerance { correlation: 0.1, sum_difference_db: 2.0, rear_share: 0.05 };
/// The least waveform correlation of the dry left part with the
/// recording's at the best lag: the same notes, voices and volumes, apart
/// from the interpolation's treble and the 1.3x level.
const DRY_MATCH: f64 = 0.95;
/// How far apart in time the native and recorded routes may run, in
/// samples: a second. Their emulations of disc access and movie decoding
/// differ (natively the opening movie runs 32 frames later, the field
/// after it 4 frames earlier).
const LAG_SPAN: usize = 44_100;
/// The Wide-Stereo relation's rounding: the SPU scales the sum by the
/// master volume and shifts it (`>> 15`, flooring), and libspu writes Wide's
/// negated master volume as its 15 bits, so -R(Stereo) and R(Wide) differ
/// in the last bits; the recordings show at most 3.
const WIDE_RIGHT_RESIDUAL: i32 = 3;

fn root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../..")
}

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1").map(PathBuf::from).unwrap_or_else(|| root().join("discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn recording(mode: &str) -> PathBuf {
    root().join(format!(".local/scenarios/p1-original-sound-{mode}-20261002/audio.wav"))
}

fn report() -> PathBuf {
    root().join(".local/scenarios/p1-original-audio-modes-20261002.json")
}

/// The disc, and the retained recordings with their report.
fn inputs() -> Option<(PathBuf, Value)> {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return None;
    };
    if !report().exists() || MODES.iter().any(|(mode, _)| !recording(mode).exists()) {
        eprintln!("skipping: no retained original recordings (.local/scenarios/p1-original-sound-*-20261002 and its report)");
        return None;
    }
    Some((disc, serde_json::from_str(&std::fs::read_to_string(report()).unwrap()).unwrap()))
}

fn execute(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command}: {reply}");
    reply
}

/// Pad 1 by frame from a retained route, from frame `from` on, as a
/// schedule of pad commands.
fn route(mode: &str, from: usize) -> Value {
    let path = root().join(format!("tests/reference-inputs/new-game-sound-mode-{mode}.json"));
    let holds: Vec<Value> = serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
    let mut commands = Vec::new();
    for hold in holds {
        commands.push(json!([hold["start"], {"cmd": "pad", "port": 0, "buttons": hold["buttons"]}]));
        commands.push(json!([hold["end"], {"cmd": "pad", "port": 0, "buttons": []}]));
    }
    commands.retain(|c| c[0].as_u64().unwrap() >= from as u64);
    commands.sort_by_key(|c| c[0].as_u64());
    json!({"cmd": "schedule", "commands": commands})
}

/// Where two captures first differ (interleaved sample index).
fn first_difference(a: &[i16], b: &[i16]) -> Option<usize> {
    a.iter().zip(b).position(|(x, y)| x != y).or((a.len() != b.len()).then_some(a.len().min(b.len())))
}

/// One frame and the samples it produced.
fn step(session: &mut Session<NativeModule>, sound: &mut Vec<i16>) {
    assert!(session.frame().expect("a frame"), "a native disc never blocks");
    let spu = &mut session.runtime.services().spu;
    let start = sound.len();
    sound.resize(start + spu.audio_queued(), 0);
    spu.take_audio(&mut sound[start..]);
}

/// The report's windows: name and frames [start, end).
fn windows(report: &Value) -> Vec<(String, usize, usize)> {
    report["windows"]
        .as_object()
        .unwrap()
        .iter()
        .map(|(name, window)| (name.clone(), window["frames"][0].as_u64().unwrap() as usize, window["frames"][1].as_u64().unwrap() as usize))
        .collect()
}

/// A window of a recording (interleaved left, right), as
/// tools/analysis/audio_modes.py reads it: samples `start * 735` on.
fn original_window(mode: &str, start: usize, end: usize) -> Vec<i16> {
    use std::io::{Read, Seek, SeekFrom};
    let mut file = std::fs::File::open(recording(mode)).unwrap();
    let mut header = [0u8; 12];
    file.read_exact(&mut header).unwrap();
    assert!(&header[0..4] == b"RIFF" && &header[8..12] == b"WAVE");
    loop {
        let mut chunk = [0u8; 8];
        file.read_exact(&mut chunk).unwrap();
        let size = u32::from_le_bytes([chunk[4], chunk[5], chunk[6], chunk[7]]) as i64;
        if &chunk[0..4] == b"data" {
            break;
        }
        file.seek(SeekFrom::Current(size + (size & 1))).unwrap();
    }
    file.seek(SeekFrom::Current((start * PER_FRAME * 4) as i64)).unwrap();
    let mut bytes = vec![0u8; (end - start) * PER_FRAME * 4];
    file.read_exact(&mut bytes).unwrap();
    bytes.chunks_exact(2).map(|b| i16::from_le_bytes([b[0], b[1]])).collect()
}

/// The windows' samples of each mode's route played natively from boot,
/// and the Wide route's state where its last window starts.
struct Native {
    windows: Vec<[Vec<i16>; 3]>,
    wide_state: Vec<u8>,
    wide_state_frame: usize,
}

fn play(disc: &Path, mode: &str, game_mode: u32, windows: &[(String, usize, usize)], keep: Option<usize>) -> (Vec<Vec<i16>>, Option<Vec<u8>>) {
    let (mut session, _) = boot(disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
    execute(&mut session, route(mode, 0));
    let end = windows.iter().map(|w| w.2).max().unwrap();
    let mut kept = vec![Vec::new(); windows.len()];
    let mut state = None;
    let mut produced = 0usize;
    let mut sound = Vec::new();
    for frame in 0..end {
        if keep == Some(frame) {
            state = Some(session.snapshot_bytes().unwrap());
        }
        sound.clear();
        step(&mut session, &mut sound);
        // Keep what falls in a window, by the sample's index from boot.
        for (i, (_, start, end)) in windows.iter().enumerate() {
            let (from, to) = (start * PER_FRAME * 2, end * PER_FRAME * 2);
            for (k, sample) in sound.iter().enumerate() {
                if (from..to).contains(&(produced + k)) {
                    kept[i].push(*sample);
                }
            }
        }
        produced += sound.len();
    }
    // The route chose its mode on the Sound screen, as the recording's did.
    assert_eq!(execute(&mut session, json!({"cmd": "status"}))["sound_mode"], json!(game_mode), "{mode}: the Sound screen's choice");
    (kept, state)
}

/// The three routes played natively, each on its own thread, once for all
/// the tests here.
fn native(disc: &Path, report: &Value) -> &'static Native {
    static NATIVE: OnceLock<Native> = OnceLock::new();
    NATIVE.get_or_init(|| {
        let windows = windows(report);
        let wide_state_frame = windows.iter().map(|w| w.1).max().unwrap();
        let started = std::time::Instant::now();
        let runs: Vec<(Vec<Vec<i16>>, Option<Vec<u8>>)> = std::thread::scope(|scope| {
            let threads: Vec<_> = MODES
                .iter()
                .map(|(mode, game_mode)| {
                    let windows = &windows;
                    scope.spawn(move || play(disc, mode, *game_mode, windows, (*mode == "wide").then_some(wide_state_frame)))
                })
                .collect();
            threads.into_iter().map(|t| t.join().expect("a route")).collect()
        });
        eprintln!("the three routes to frame {} in {:.1} s", windows.iter().map(|w| w.2).max().unwrap(), started.elapsed().as_secs_f64());
        let wide_state = runs[1].1.clone().unwrap();
        let windows = (0..windows.len()).map(|w| [runs[0].0[w].clone(), runs[1].0[w].clone(), runs[2].0[w].clone()]).collect();
        Native { windows, wide_state, wide_state_frame }
    })
}

/// Per-channel levels and the relation between the channels, as
/// tools/analysis/audio_modes.py's `levels` computes them.
#[derive(Debug)]
struct Levels {
    rms: [f64; 2],
    correlation: f64,
    sum_over_difference_db: f64,
}

fn levels(samples: &[i16]) -> Levels {
    let n = (samples.len() / 2) as f64;
    let (mut ll, mut rr, mut lr) = (0f64, 0f64, 0f64);
    for pair in samples.chunks_exact(2) {
        let (l, r) = (f64::from(pair[0]), f64::from(pair[1]));
        ll += l * l;
        rr += r * r;
        lr += l * r;
    }
    let (sum, difference) = ((ll + rr + 2.0 * lr) / 4.0, (ll + rr - 2.0 * lr) / 4.0);
    Levels { rms: [(ll / n).sqrt(), (rr / n).sqrt()], correlation: lr / (ll * rr).sqrt(), sum_over_difference_db: 10.0 * (sum / difference).log10() }
}

/// Channel `c` of `a` against `sign` times channel `c` of `b`: the largest
/// residual and the share of samples where it is zero.
fn relation(a: &[i16], b: &[i16], c: usize, sign: i32) -> (i32, f64) {
    let residuals: Vec<i32> = a.iter().skip(c).step_by(2).zip(b.iter().skip(c).step_by(2)).map(|(x, y)| i32::from(*x) - sign * i32::from(*y)).collect();
    let exact = residuals.iter().filter(|r| **r == 0).count() as f64 / residuals.len() as f64;
    (residuals.iter().map(|r| r.abs()).max().unwrap(), exact)
}

/// Where `a` best matches `b` shifted by up to [`LAG_SPAN`] samples either
/// way: the lag and the normalized correlation there. Coarsely over 10 ms
/// envelopes, then sample by sample around that.
fn best_match(a: &[f64], b: &[f64]) -> (isize, f64) {
    const HOP: usize = 441;
    let envelope = |x: &[f64]| -> Vec<f64> {
        let e: Vec<f64> = x.chunks_exact(HOP).map(|c| (c.iter().map(|v| v * v).sum::<f64>() / HOP as f64).sqrt()).collect();
        let mean = e.iter().sum::<f64>() / e.len() as f64;
        let deviation = (e.iter().map(|v| (v - mean).powi(2)).sum::<f64>() / e.len() as f64).sqrt();
        e.iter().map(|v| (v - mean) / deviation).collect()
    };
    // a[i] against b[i - lag], over where both are.
    let at = |a: &[f64], b: &[f64], lag: isize| -> f64 {
        let (mut ab, mut aa, mut bb) = (0.0, 0.0, 0.0);
        for i in lag.max(0) as usize..a.len().min((b.len() as isize + lag) as usize) {
            let y = b[(i as isize - lag) as usize];
            ab += a[i] * y;
            aa += a[i] * a[i];
            bb += y * y;
        }
        ab / (aa * bb).sqrt()
    };
    let (ea, eb) = (envelope(a), envelope(b));
    let reach = (LAG_SPAN / HOP) as isize;
    let coarse = (-reach..=reach).max_by(|x, y| at(&ea, &eb, *x).total_cmp(&at(&ea, &eb, *y))).unwrap() * HOP as isize;
    (coarse - HOP as isize..=coarse + HOP as isize).map(|lag| (lag, at(a, b, lag))).max_by(|x, y| x.1.total_cmp(&y.1)).unwrap()
}

/// The left channel's dry part, (Stereo + Wide) / 2, and its reverb
/// return, (Stereo - Wide) / 2 (Wide negates only the reverb's left).
fn left_parts(stereo: &[i16], wide: &[i16]) -> (Vec<f64>, Vec<f64>) {
    let pairs = || stereo.iter().step_by(2).zip(wide.iter().step_by(2)).map(|(s, w)| (f64::from(*s), f64::from(*w)));
    (pairs().map(|(s, w)| (s + w) / 2.0).collect(), pairs().map(|(s, w)| (s - w) / 2.0).collect())
}

fn rms(x: &[f64]) -> f64 {
    (x.iter().map(|v| v * v).sum::<f64>() / x.len() as f64).sqrt()
}

/// FreeSurround's 5.1 energy shares (FL, FC, FR, BL, BR, LFE) of a window.
fn surround(samples: &[i16]) -> [f64; 6] {
    use std::io::{Read, Write};
    let mut child = std::process::Command::new("freesurround")
        .arg("44100")
        .stdin(std::process::Stdio::piped())
        .stdout(std::process::Stdio::piped())
        .spawn()
        .expect("freesurround, the nix/runtime shell's FreeSurround decoder");
    let input: Vec<u8> = samples.iter().flat_map(|s| (f32::from(*s) / 32768.0).to_le_bytes()).collect();
    let mut stdin = child.stdin.take().unwrap();
    let writer = std::thread::spawn(move || stdin.write_all(&input).unwrap());
    let mut output = Vec::new();
    child.stdout.take().unwrap().read_to_end(&mut output).unwrap();
    writer.join().unwrap();
    assert!(child.wait().unwrap().success());
    let mut energy = [0f64; 6];
    for (i, bytes) in output.chunks_exact(4).enumerate() {
        energy[i % 6] += f64::from(f32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]])).powi(2);
    }
    let total: f64 = energy.iter().sum();
    energy.map(|e| e / total)
}

#[test]
fn raw_playback_keeps_the_relations_the_original_recordings_show() {
    let Some((disc, report)) = inputs() else { return };
    let windows = windows(&report);
    let native = native(&disc, &report);
    let mut failures = Vec::new();
    for (w, (name, start, end)) in windows.iter().enumerate() {
        let entry = &report["windows"][name];
        let original: Vec<Vec<i16>> = MODES.iter().map(|(mode, _)| original_window(mode, *start, *end)).collect();
        let native = &native.windows[w];
        assert!(native.iter().all(|n| n.len() == (end - start) * PER_FRAME * 2), "{name}: whole windows natively");
        // The recordings read here are the ones the report measured.
        for (m, (mode, _)) in MODES.iter().enumerate() {
            let measured = levels(&original[m]);
            let reported = &entry["modes"][mode];
            let close = |value: f64, key: &str, places: i32| (value - reported[key].as_f64().unwrap()).abs() <= 0.5 * 10f64.powi(-places) + 1e-9;
            assert!(close(measured.rms[0], "rms_left", 2) && close(measured.rms[1], "rms_right", 2) && close(measured.correlation, "correlation", 4)
                && close(measured.sum_over_difference_db, "sum_over_difference_db", 2), "{name} {mode}: {measured:?} against the report's {reported}");
        }
        let (wide_right, _) = relation(&original[1], &original[0], 1, -1);
        assert_eq!(json!(wide_right), entry["wide_vs_stereo"]["right_inversion"]["max_abs_residual"], "{name}: the report's Wide right");
        // The findings, natively: Wide's right is Stereo's negated within the
        // master volume's rounding, in the movie and in the field.
        let (residual, exact) = relation(&native[1], &native[0], 1, -1);
        eprintln!("{name}: native R(wide) = -R(stereo) within {residual} ({:.1}% exact), the recording within {wide_right}", exact * 100.0);
        assert!(residual <= WIDE_RIGHT_RESIDUAL, "{name}: R(wide) = -R(stereo) within {WIDE_RIGHT_RESIDUAL}, natively {residual}");
        // The recording's Wide left equals its Stereo left exactly where
        // there is no reverb return (the movie).
        let movie = entry["wide_vs_stereo"]["left_identity"]["exact_fraction"] == json!(1.0);
        if movie {
            // The movie's sound is CD input, which neither Mono nor Wide
            // changes and which bypasses the reverb: Mono is Stereo, and
            // Wide's left is Stereo's, sample for sample.
            assert_eq!(first_difference(&native[2], &native[0]), None, "{name}: Mono is Stereo sample for sample");
            assert_eq!(relation(&native[1], &native[0], 0, 1), (0, 1.0), "{name}: L(wide) = L(stereo) without reverb");
        } else {
            // In the field the reverb returns: Wide's left differs from
            // Stereo's (its reverb part negated), Mono differs (the centre law).
            assert_ne!(relation(&native[1], &native[0], 0, 1).0, 0, "{name}: the reverb's left is Wide's own");
            assert!(first_difference(&native[2], &native[0]).is_some(), "{name}: Mono's centre law");
        }
        // Wide splits the left channel into its dry part and its reverb
        // return. The dry part is the recording's own waveform, a lag apart.
        let ((dry, reverb), (original_dry, original_reverb)) = (left_parts(&native[0], &native[1]), left_parts(&original[0], &original[1]));
        let (lag, matched) = best_match(&dry, &original_dry);
        eprintln!("{name}: the left dry part matches the recording's {matched:.4} at {:+.1} ms, {:.3} times its level", lag as f64 / 44.1, rms(&dry) / rms(&original_dry));
        assert!(matched >= DRY_MATCH, "{name}: the dry mix is the recording's: {matched:.4} at {lag} samples");
        let tolerance = if movie { &DRY } else { &REVERB };
        if !movie {
            let (reverb_lag, reverb_matched) = best_match(&reverb, &original_reverb);
            eprintln!("{name}: the left reverb return matches the recording's {reverb_matched:.4} at {:+.1} ms, {:.3} times its level ({:+.2} dB against the dry part's ratio)",
                reverb_lag as f64 / 44.1, rms(&reverb) / rms(&original_reverb), 20.0 * ((rms(&reverb) / rms(&original_reverb)) / (rms(&dry) / rms(&original_dry))).log10());
        }
        for (m, (mode, _)) in MODES.iter().enumerate() {
            let (here, there) = (levels(&native[m]), levels(&original[m]));
            let (ours, theirs) = (surround(&native[m]), surround(&original[m]));
            let (rear, original_rear) = (ours[3] + ours[4], theirs[3] + theirs[4]);
            eprintln!("{name} {mode}: correlation {:+.4} (original {:+.4}), sum/difference {:+.2} dB ({:+.2}), FreeSurround centre {:.3} rear {:.3} ({:.3}, {:.3}), level native/original L {:.3} R {:.3}",
                here.correlation, there.correlation, here.sum_over_difference_db, there.sum_over_difference_db, ours[1], rear, theirs[1], original_rear,
                here.rms[0] / there.rms[0], here.rms[1] / there.rms[1]);
            if (here.correlation - there.correlation).abs() > tolerance.correlation {
                failures.push(format!("{name} {mode}: correlation {:+.4} against {:+.4}", here.correlation, there.correlation));
            }
            if (here.sum_over_difference_db - there.sum_over_difference_db).abs() > tolerance.sum_difference_db {
                failures.push(format!("{name} {mode}: sum/difference {:+.2} dB against {:+.2}", here.sum_over_difference_db, there.sum_over_difference_db));
            }
            if (rear - original_rear).abs() > tolerance.rear_share {
                failures.push(format!("{name} {mode}: FreeSurround's rear share {rear:.3} against {original_rear:.3}"));
            }
        }
    }
    assert!(failures.is_empty(), "{failures:#?}");
}

#[test]
fn binaural_turned_off_on_the_wide_route_gives_back_the_wide_output() {
    let Some((disc, report)) = inputs() else { return };
    let native = native(&disc, &report);
    let from = native.wide_state_frame;
    let (on, off, to) = (from + 60, from + 300, from + 600);
    // As a host applies the settings before each frame: Binaural runs the
    // game in Stereo and gives back the original mode chosen before.
    let run = |round_trip: bool| {
        let (mut session, _) = boot(&disc, &BootOptions { audio_output: true, ..Default::default() }).expect("boot");
        let mut settings = SettingsService::with_defaults(MemoryStore::default());
        settings.apply(SettingChange::AudioOutput(AudioOutput::Wide)).unwrap();
        session.set_sound_mode(settings.settings().game_sound_mode()).unwrap();
        session.restore_bytes(&native.wide_state).unwrap();
        execute(&mut session, route("wide", from));
        let mut sound = Vec::new();
        for frame in from..to {
            if round_trip && (frame == on || frame == off) {
                let binaural = settings.settings().binaural;
                settings.apply(SettingChange::Binaural(xem_settings::Binaural { enabled: frame == on, ..binaural })).unwrap();
            }
            session.set_binaural(settings.settings().binaural);
            session.set_sound_mode(settings.settings().game_sound_mode()).unwrap();
            step(&mut session, &mut sound);
            if frame == on {
                assert_eq!(execute(&mut session, json!({"cmd": "binaural"}))["presenting"], json!(round_trip));
            }
        }
        let chip = session.runtime.services().spu.chip().save();
        (sound, session.memory_hash().unwrap(), chip)
    };
    let (wide, wide_hash, wide_chip) = run(false);
    let (round_trip, round_trip_hash, round_trip_chip) = run(true);
    assert_eq!(wide.len(), round_trip.len(), "the same number of samples");
    let sample = |frame: usize| (frame - from) * PER_FRAME * 2;
    let first = first_difference(&wide, &round_trip).expect("Binaural presents something else");
    let last = wide.iter().zip(&round_trip).rposition(|(a, b)| a != b).unwrap();
    eprintln!("round trip: the same Wide output to sample {} (Binaural on at {}), again from {} (off at {}: {} samples, {:.1} ms, after it)",
        first / 2, sample(on) / 2, last / 2 + 1, sample(off) / 2, (last / 2 + 1 - sample(off) / 2), (last / 2 + 1 - sample(off) / 2) as f64 / 44.1);
    assert!(first >= sample(on), "nothing changes before Binaural is on");
    // The presentation fades out over 100 ms, then hands the block under
    // way back as the SPU mixed it; from there the output is the never-
    // enabled run's, sample for sample.
    let fade = (0.1 * 44_100.0) as usize + xem_binaural::BLOCK;
    assert!(last / 2 < sample(off) / 2 + fade, "the Wide output is back after the fade and one block: {} samples after Binaural went off", last / 2 + 1 - sample(off) / 2);
    assert_eq!(round_trip_hash, wide_hash, "the game is where the never-enabled run is");
    assert!(round_trip_chip == wide_chip, "and so is the SPU");
}
