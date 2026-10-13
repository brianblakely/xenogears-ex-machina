//! Front and back, left and right, up and down survive the renderer: noise
//! placed in a direction is rendered (blocks, interpolated filters, levelling,
//! bass shelf, glide), and the direction is recovered from the two ears'
//! output alone, by the cues a listener uses (each ear's spectrum, the level
//! difference between them, the delay between them), matched against every
//! measured direction. The model listener is the measured head itself, so
//! this checks that the cues reach the ears intact; it is not a listening
//! test with people.

use xem_binaural::fft::{Complex, Fft};
use xem_binaural::hrtf::{self, BINS, FFT_SIZE, Filter, Hrtf};
use xem_binaural::{BLOCK, Renderer, Route, Target};
use xem_spu::TapFrame;

/// Bands of the spectra compared: third octaves from 500 Hz to 14 kHz, as
/// bins of the 256-point transform.
fn bands() -> Vec<(usize, usize)> {
    let bin = |f: f32| (f * FFT_SIZE as f32 / 44_100.0).round() as usize;
    let mut out = Vec::new();
    let mut f = 500.0f32;
    while f < 14_000.0 {
        let (lo, hi) = (bin(f), bin(f * 2f32.powf(1.0 / 3.0)).max(bin(f) + 1));
        out.push((lo, hi));
        f *= 2f32.powf(1.0 / 3.0);
    }
    out
}

/// What a listener's ears tell: each ear's band levels (dB) and the delay
/// of the left ear behind the right (samples).
struct Cues {
    levels: Vec<[f32; 2]>,
    delay: f32,
}

fn band_levels(power: &[[f32; BINS]; 2]) -> Vec<[f32; 2]> {
    bands()
        .iter()
        .map(|&(lo, hi)| [0, 1].map(|e| 10.0 * ((lo..hi).map(|k| power[e][k]).sum::<f32>() / (hi - lo) as f32).max(1e-12).log10()))
        .collect()
}

/// The lag (samples) where the cross-correlation of `left` and `right` peaks,
/// within 40 either way: positive when the left lags.
fn delay_of(left: &[f32], right: &[f32]) -> f32 {
    let mut best = (f32::MIN, 0i32);
    for lag in -40i32..=40 {
        let mut sum = 0.0;
        for n in 40..left.len().saturating_sub(40) {
            sum += left[n] * right[(n as i32 - lag) as usize];
        }
        if sum > best.0 {
            best = (sum, lag);
        }
    }
    best.1 as f32
}

/// The cues of a measured direction's filters.
fn filter_cues(hrtf: &Hrtf, azimuth: f32, elevation: f32, fft: &Fft) -> Cues {
    let mut filter = Filter::default();
    hrtf.filter(azimuth, elevation, &mut filter);
    let shelf = hrtf.bass_shelf();
    let mut power = [[0f32; BINS]; 2];
    let mut impulse = [vec![0f32; FFT_SIZE], vec![0f32; FFT_SIZE]];
    for e in 0..2 {
        for k in 0..BINS {
            let gain = shelf.magnitude(k as f32 * 44_100.0 / FFT_SIZE as f32);
            power[e][k] = filter.ears[e][k].norm_sqr() * gain * gain;
        }
        let mut buffer = vec![Complex::ZERO; FFT_SIZE];
        for k in 0..BINS {
            buffer[k] = filter.ears[e][k];
            if k > 0 && k < FFT_SIZE / 2 {
                buffer[FFT_SIZE - k] = filter.ears[e][k].conj();
            }
        }
        fft.inverse(&mut buffer);
        impulse[e] = buffer.iter().map(|c| c.re).collect();
    }
    // The impulse responses' own cross-correlation, padded so every lag fits.
    let pad = |v: &[f32]| [vec![0.0; 40], v.to_vec(), vec![0.0; 40]].concat();
    Cues { levels: band_levels(&power), delay: delay_of(&pad(&impulse[0]), &pad(&impulse[1])) }
}

/// White noise placed at (azimuth, elevation), rendered: the ears' cues.
fn rendered_cues(azimuth: f32, elevation: f32, fft: &Fft) -> Cues {
    let mut renderer = Renderer::new();
    renderer.set_enabled(true);
    renderer.reset();
    renderer.set_route(0, Route::Source(1));
    renderer.set_source(1, Target::Listener(hrtf::direction(azimuth, elevation)), true);
    let mut out = Vec::new();
    let mut state = 0x2545_F491_4F6C_DD1Du64;
    for _ in 0..64 * BLOCK {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        let mut frame = TapFrame { main: [0x7FFE, 0x7FFE], ..Default::default() };
        frame.voice[0] = ((state >> 48) as i16) / 8;
        frame.volume[0] = [0x3000, 0x3000];
        renderer.push(&frame, &mut out);
    }
    let ears: [Vec<f32>; 2] = [0, 1].map(|e| out.iter().skip(e).step_by(2).map(|s| f32::from(*s)).collect());
    // Welch: Hann-windowed 256-point segments, half overlapping, averaged.
    let mut power = [[0f32; BINS]; 2];
    for e in 0..2 {
        let mut start = 0;
        while start + FFT_SIZE <= ears[e].len() {
            let mut buffer: Vec<Complex> = (0..FFT_SIZE)
                .map(|n| {
                    let window = 0.5 - 0.5 * (2.0 * std::f32::consts::PI * n as f32 / FFT_SIZE as f32).cos();
                    Complex::new(ears[e][start + n] * window, 0.0)
                })
                .collect();
            fft.forward(&mut buffer);
            for k in 0..BINS {
                power[e][k] += buffer[k].norm_sqr();
            }
            start += FFT_SIZE / 2;
        }
    }
    Cues { levels: band_levels(&power), delay: delay_of(&ears[0], &ears[1]) }
}

/// How far apart two sets of cues are: the ears' spectral shapes (the
/// overall level aside) and their delay.
fn distance(a: &Cues, b: &Cues) -> f32 {
    let mean = |c: &Cues| c.levels.iter().map(|l| l[0] + l[1]).sum::<f32>() / (2 * c.levels.len()) as f32;
    let (ma, mb) = (mean(a), mean(b));
    let spectral: f32 = a.levels.iter().zip(&b.levels).map(|(x, y)| (0..2).map(|e| ((x[e] - ma) - (y[e] - mb)).powi(2)).sum::<f32>()).sum();
    spectral + 4.0 * (a.delay - b.delay).powi(2)
}

#[test]
fn directions_are_recovered_from_the_ears_alone() {
    let hrtf = Hrtf::kemar();
    let fft = Fft::new(FFT_SIZE);
    let candidates: Vec<((f32, f32), Cues)> = hrtf.directions().into_iter().map(|(a, e)| ((a, e), filter_cues(hrtf, a, e, &fft))).collect();
    let mut errors = Vec::new();
    for elevation in [-30.0f32, 0.0, 30.0, 60.0] {
        for step in 0..12 {
            // Between the measured azimuths (and so interpolated).
            let azimuth = step as f32 * 30.0 + 7.0;
            let cues = rendered_cues(azimuth, elevation, &fft);
            let (found, _) = candidates
                .iter()
                .map(|(direction, candidate)| (*direction, distance(&cues, candidate)))
                .min_by(|a, b| a.1.total_cmp(&b.1))
                .expect("candidates");
            let truth = hrtf::direction(azimuth, elevation);
            let guess = hrtf::direction(found.0, found.1);
            let error = (truth[0] * guess[0] + truth[1] * guess[1] + truth[2] * guess[2]).clamp(-1.0, 1.0).acos().to_degrees();
            // Left and right always; front and back off the sides; up and down off the level.
            assert_eq!(truth[0] > 0.0, guess[0] > 0.0, "{azimuth}/{elevation} heard at {found:?}: left and right");
            if truth[2].abs() > 0.3 {
                assert_eq!(truth[2] > 0.0, guess[2] > 0.0, "{azimuth}/{elevation} heard at {found:?}: front and back");
            }
            if elevation.abs() >= 30.0 {
                assert_eq!(truth[1] < 0.0, guess[1] < 0.0, "{azimuth}/{elevation} heard at {found:?}: up and down");
            }
            errors.push(error);
        }
    }
    errors.sort_by(f32::total_cmp);
    let median = errors[errors.len() / 2];
    let worst = errors[errors.len() - 1];
    eprintln!("direction recovered within {median:.1} degrees (median), {worst:.1} at worst, of {} directions", errors.len());
    assert!(median < 10.0 && worst < 30.0, "median {median:.1}, worst {worst:.1}");
}
