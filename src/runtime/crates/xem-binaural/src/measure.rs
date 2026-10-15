//! Measurements of two-channel signals for tests and tools (the musical
//! coherence proxies, the listening test's stimuli): ITU-R BS.1770-4
//! integrated loudness and the interaural cross-correlation by octave.
//! Nothing presented uses them.

use crate::fft::{Complex, Fft};

/// ITU-R BS.1770-4's K-weighting at `rate` (the analogue design of
/// libebur128: the head's shelf and the high pass), then the integrated
/// loudness of the channels (LUFS; full scale 1): 400 ms blocks
/// overlapping by 75%, the absolute gate at -70 and the relative one 10 LU
/// under. Minus infinity when no block passes the gates (silence, or less
/// than 400 ms).
pub fn loudness(channels: &[&[f32]], rate: f64) -> f64 {
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
    let k = (std::f64::consts::PI * f0 / rate).tan();
    let (vh, vb) = (10f64.powf(gain / 20.0), 10f64.powf(gain / 20.0).powf(0.4996667741545416));
    let a0 = 1.0 + k / q + k * k;
    let shelf_b = [(vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0];
    let shelf_a = [1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0];
    let (f0, q) = (38.13547087602444, 0.5003270373238773);
    let k = (std::f64::consts::PI * f0 / rate).tan();
    let a0 = 1.0 + k / q + k * k;
    let pass_a = [1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0];
    let weighted: Vec<Vec<f64>> = channels
        .iter()
        .map(|x| {
            let shelved: Vec<f32> = biquad(shelf_b, shelf_a, x).iter().map(|&y| y as f32).collect();
            biquad([1.0, -2.0, 1.0], pass_a, &shelved)
        })
        .collect();
    let (size, hop) = ((0.4 * rate) as usize, (0.1 * rate) as usize);
    let len = weighted.first().map_or(0, Vec::len);
    let blocks: Vec<f64> = (0..)
        .map(|b| b * hop)
        .take_while(|&at| at + size <= len)
        .map(|at| weighted.iter().map(|y| y[at..at + size].iter().map(|v| v * v).sum::<f64>() / size as f64).sum::<f64>())
        .collect();
    let level = |z: f64| -0.691 + 10.0 * z.log10();
    let gated: Vec<f64> = blocks.iter().copied().filter(|&z| level(z) > -70.0).collect();
    if gated.is_empty() {
        return f64::NEG_INFINITY;
    }
    let relative = level(gated.iter().sum::<f64>() / gated.len() as f64) - 10.0;
    let kept: Vec<f64> = gated.into_iter().filter(|&z| level(z) > relative).collect();
    level(kept.iter().sum::<f64>() / kept.len() as f64)
}

/// Octave bands' centres for the interaural cross-correlation.
pub const OCTAVES: [f64; 7] = [125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0];

/// The transform of `x`, zero-padded to `n` points.
fn spectrum(fft: &Fft, x: &[f32], n: usize) -> Vec<Complex> {
    let mut data: Vec<Complex> = (0..n).map(|t| Complex::new(x.get(t).copied().unwrap_or(0.0), 0.0)).collect();
    fft.forward(&mut data);
    data
}

/// The interaural cross-correlation coefficient of each octave band of
/// [`OCTAVES`] (signals at `rate`): the largest normalized
/// cross-correlation of the two sides, band-passed (an ideal band in the
/// whole signal's transform), within a millisecond either way.
pub fn iacc(sides: &[Vec<f32>; 2], rate: f64) -> Vec<f64> {
    let n = sides[0].len().next_power_of_two() * 2;
    let fft = Fft::new(n);
    let [l, r] = [spectrum(&fft, &sides[0], n), spectrum(&fft, &sides[1], n)];
    OCTAVES
        .iter()
        .map(|&c| {
            let (lo, hi) = ((c / 2f64.sqrt() * n as f64 / rate) as usize, (c * 2f64.sqrt() * n as f64 / rate) as usize);
            let mut cross = vec![Complex::ZERO; n];
            let (mut el, mut er) = (0f64, 0f64);
            for k in lo..=hi {
                cross[k] = l[k].mul(r[k].conj());
                cross[n - k] = cross[k].conj();
                el += 2.0 * f64::from(l[k].norm_sqr());
                er += 2.0 * f64::from(r[k].norm_sqr());
            }
            fft.inverse(&mut cross);
            let lags = (rate / 1000.0) as usize;
            let best = (0..=lags).chain(n - lags..n).map(|t| f64::from(cross[t].re) * n as f64).fold(f64::MIN, f64::max);
            best / (el * er).sqrt()
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A sine at `f` Hz and `amplitude` (full scale 1), `seconds` long.
    fn sine(f: f64, amplitude: f64, seconds: f64) -> Vec<f32> {
        (0..(seconds * 44_100.0) as usize).map(|n| (amplitude * (std::f64::consts::TAU * f * n as f64 / 44_100.0).sin()) as f32).collect()
    }

    #[test]
    fn a_full_scale_997_hz_sine_in_one_channel_reads_minus_three_lufs() {
        // BS.1770-4's own calibration: a 0 dBFS 997 Hz sine in one channel
        // reads -3.01 LKFS.
        let x = sine(997.0, 1.0, 5.0);
        let measured = loudness(&[&x], 44_100.0);
        assert!((measured + 3.01).abs() < 0.05, "{measured}");
        // Both channels: 3 dB more; silence and short signals: nothing.
        assert!((loudness(&[&x, &x], 44_100.0) - measured - 3.01).abs() < 0.05);
        assert_eq!(loudness(&[&vec![0.0; 44_100]], 44_100.0), f64::NEG_INFINITY);
        assert_eq!(loudness(&[&x[..1000]], 44_100.0), f64::NEG_INFINITY);
    }

    #[test]
    fn identical_sides_correlate_fully_and_unrelated_noises_hardly() {
        let noise = |seed: u64| -> Vec<f32> {
            (0..44_100u64)
                .map(|n| {
                    let mut x = (n ^ (seed << 32)).wrapping_mul(0x9E37_79B9_7F4A_7C15);
                    x ^= x >> 31;
                    (x.wrapping_mul(0xBF58_476D_1CE4_E5B9) >> 40) as f32 / (1u64 << 24) as f32 - 0.5
                })
                .collect()
        };
        let x = noise(1);
        let same = iacc(&[x.clone(), x.clone()], 44_100.0);
        assert!(same.iter().all(|c| (c - 1.0).abs() < 1e-3), "{same:?}");
        let unrelated = iacc(&[x, noise(2)], 44_100.0);
        assert!(unrelated[3..].iter().all(|c| *c < 0.15), "{unrelated:?}");
    }
}
