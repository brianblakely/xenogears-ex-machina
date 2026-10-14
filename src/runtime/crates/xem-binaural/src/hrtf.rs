//! The head-related transfer functions the renderer places sounds with: the
//! MIT KEMAR measurements, diffuse-field equalized (hrtf/README.md), as
//! filter pairs for the renderer's 256-point blocks.
//!
//! Each measured response is cut from just before its onset ([`TAPS`]
//! taps), so that the responses of neighbouring directions line up and mix
//! without comb filtering; the onsets' difference between the ears (the
//! interaural delay) is kept apart and applied after mixing, as a phase
//! ramp. Every direction's pair is scaled to one pink-weighted energy
//! ([`PAIR_ENERGY`]), so a sound keeps its loudness wherever it is placed
//! while its level difference, delay and spectrum between the ears stay
//! the measurement's. A direction between the measured ones mixes the four
//! around it: two azimuths on each of the two elevation rings around it.

use std::sync::OnceLock;

use crate::fft::{Complex, Fft};

/// The renderer's transform size: a block of [`crate::BLOCK`] samples and a
/// filter of up to as many taps (delay included) convolve without wrapping.
pub const FFT_SIZE: usize = 2 * crate::BLOCK;
/// Spectrum bins of a real signal.
pub const BINS: usize = FFT_SIZE / 2 + 1;
/// Taps kept of each measured response, from [`PRE`] before its onset
/// (99.9% of the responses' energy).
pub const TAPS: usize = 92;
/// Samples kept before a response's onset.
pub const PRE: usize = 2;
/// The longest interaural delay applied, in samples, so that a response
/// and its delay fit [`crate::BLOCK`] taps (the measured ones reach 28).
pub const MAX_DELAY: f32 = (crate::BLOCK - TAPS - PRE) as f32;
/// Every direction's filter pair has this loudness-weighted energy (both
/// ears together, [`loudness_weight`]): the Stereo pan law's centre gain
/// (0x5A00 of 0x8000) on both sides, so a sound placed ahead is as loud as
/// the original mix's centre and a sound anywhere as loud as there.
pub const PAIR_ENERGY: f32 = 2.0 * (23040.0 / 32768.0) * (23040.0 / 32768.0);
/// The levelled band: bins 1 to 93 of 256 at 44.1 kHz (172 Hz to 16 kHz).
pub const BAND: std::ops::RangeInclusive<usize> = 1..=93;

/// How much bin `k` counts towards loudness in the levelling: a pink
/// spectrum (falling 3 dB an octave, as music's does) heard through
/// ITU-R BS.1770's K-weighting, whose shelf adds 4 dB above about 1.5 kHz.
pub fn loudness_weight(k: usize) -> f32 {
    let f = k as f32 * crate::SAMPLE_RATE as f32 / FFT_SIZE as f32;
    let x = (f / 1500.0) * (f / 1500.0);
    (1.0 + 1.512 * x / (1.0 + x)) / f
}

/// The loudness-weighted energy of a spectrum (the mean over [`BAND`] of
/// [`loudness_weight`] times the bins' power).
pub fn loudness_energy(spectrum: &[Complex]) -> f32 {
    let total: f32 = BAND.map(loudness_weight).sum();
    BAND.map(|k| loudness_weight(k) * spectrum[k].norm_sqr()).sum::<f32>() / total
}

const DATA: &[u8] = include_bytes!("../hrtf/kemar-diffuse.hrir");
/// Samples of each measured response.
pub const FRAMES: usize = 128;

/// One measured direction: its aligned spectra and the delay of each ear.
struct Entry {
    spectra: [[Complex; BINS]; 2],
    delay: [f32; 2],
}

/// A measured direction's responses as the set holds them, before the
/// renderer prepares them (no alignment, delay or levelling): azimuth
/// clockwise from ahead and elevation in degrees, and each ear's samples
/// (left, right) at full scale 1.
pub struct Measured {
    pub azimuth: f32,
    pub elevation: f32,
    pub ears: [[f32; FRAMES]; 2],
}

/// One of the file's entries: elevation, azimuth (0 to 180, the right
/// half) and each ear's samples.
type Record = (i16, i16, [[f32; FRAMES]; 2]);

/// The file's entries, in its order.
fn records(data: &[u8]) -> Result<Vec<Record>, String> {
    let u16_at = |at: usize| data.get(at..at + 2).map(|b| u16::from_le_bytes([b[0], b[1]]));
    if data.get(..4) != Some(b"KEMR") || u16_at(4) != Some(1) {
        return Err("not a version 1 KEMR file".into());
    }
    let count = usize::from(u16_at(6).ok_or("truncated")?);
    let size = 4 + FRAMES * 4;
    if data.len() != 8 + count * size {
        return Err("wrong size".into());
    }
    let mut records = Vec::with_capacity(count);
    for i in 0..count {
        let record = &data[8 + i * size..8 + (i + 1) * size];
        let elevation = i16::from_le_bytes([record[0], record[1]]);
        let azimuth = i16::from_le_bytes([record[2], record[3]]);
        if !(-40..=90).contains(&elevation) || !(0..=180).contains(&azimuth) {
            return Err(format!("entry {i}: direction out of range"));
        }
        let mut ears = [[0f32; FRAMES]; 2];
        for n in 0..FRAMES {
            for (e, ear) in ears.iter_mut().enumerate() {
                let at = 4 + n * 4 + e * 2;
                ear[n] = f32::from(i16::from_le_bytes([record[at], record[at + 1]])) / 32768.0;
            }
        }
        records.push((elevation, azimuth, ears));
    }
    Ok(records)
}

/// A measured direction on its elevation's ring, over the whole circle:
/// the measurements cover the right half, and the left is their mirror
/// image (the ears swapped).
#[derive(Clone, Copy)]
struct Point {
    azimuth: f32,
    entry: u16,
    mirrored: bool,
}

struct Ring {
    elevation: f32,
    /// By azimuth, from 0 (ahead) clockwise.
    points: Vec<Point>,
}

/// The measured set, prepared.
pub struct Hrtf {
    entries: Vec<Entry>,
    rings: Vec<Ring>,
    shelf: Biquad,
}

/// A biquad filter's coefficients (a0 = 1).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Biquad {
    pub b: [f32; 3],
    pub a: [f32; 2],
}

impl Biquad {
    /// The low shelf of the RBJ audio-EQ cookbook (slope 1): `gain_db` at
    /// 0 Hz, half of it at `f0`, none well above.
    pub fn low_shelf(f0: f32, gain_db: f32) -> Biquad {
        let a = 10f32.powf(gain_db / 40.0);
        let w0 = 2.0 * std::f32::consts::PI * f0 / crate::SAMPLE_RATE as f32;
        let (sin, cos) = w0.sin_cos();
        let alpha = sin / 2.0 * std::f32::consts::SQRT_2;
        let root = 2.0 * a.sqrt() * alpha;
        let a0 = (a + 1.0) + (a - 1.0) * cos + root;
        Biquad {
            b: [
                a * ((a + 1.0) - (a - 1.0) * cos + root) / a0,
                2.0 * a * ((a - 1.0) - (a + 1.0) * cos) / a0,
                a * ((a + 1.0) - (a - 1.0) * cos - root) / a0,
            ],
            a: [-2.0 * ((a - 1.0) + (a + 1.0) * cos) / a0, ((a + 1.0) + (a - 1.0) * cos - root) / a0],
        }
    }

    /// The gain at `f` Hz.
    pub fn magnitude(&self, f: f32) -> f32 {
        let w = 2.0 * std::f64::consts::PI * f64::from(f) / f64::from(crate::SAMPLE_RATE);
        let z1 = Complex::cis(-w);
        let z2 = Complex::cis(-2.0 * w);
        let one = Complex::new(1.0, 0.0);
        let num = Complex::new(self.b[0], 0.0).add(z1.scale(self.b[1])).add(z2.scale(self.b[2]));
        let den = one.add(z1.scale(self.a[0])).add(z2.scale(self.a[1]));
        (num.norm_sqr() / den.norm_sqr()).sqrt()
    }

    /// The low shelf whose gains at 0, 172 and 344 Hz (bins 0 to 2) come
    /// closest to `wanted` (linear): its full gain the 0 Hz one, its corner
    /// the best of 60 to 400 Hz.
    fn fit_bass_shelf(wanted: [f32; 3]) -> Biquad {
        let gain_db = 20.0 * wanted[0].max(1.0).log10();
        let bin = |k: usize| k as f32 * crate::SAMPLE_RATE as f32 / FFT_SIZE as f32;
        let error = |shelf: &Biquad| (1..3).map(|k| (shelf.magnitude(bin(k)) / wanted[k]).ln().powi(2)).sum::<f32>();
        (60..=400)
            .step_by(2)
            .map(|f0| Biquad::low_shelf(f0 as f32, gain_db))
            .min_by(|a, b| error(a).total_cmp(&error(b)))
            .expect("a range of corners")
    }
}

/// A biquad's state, run sample by sample (direct form I).
#[derive(Clone, Copy, Debug, Default)]
pub struct BiquadState {
    x: [f32; 2],
    y: [f32; 2],
}

impl BiquadState {
    pub fn run(&mut self, filter: &Biquad, x: f32) -> f32 {
        let y = filter.b[0] * x + filter.b[1] * self.x[0] + filter.b[2] * self.x[1] - filter.a[0] * self.y[0] - filter.a[1] * self.y[1];
        self.x = [x, self.x[0]];
        self.y = [y, self.y[0]];
        y
    }
}

/// The filters of one direction for both ears (left, right): spectra of
/// [`FFT_SIZE`] points (bins 0 to [`BINS`] - 1), interaural delay included.
#[derive(Clone)]
pub struct Filter {
    pub ears: [[Complex; BINS]; 2],
}

impl Default for Filter {
    fn default() -> Self {
        Filter { ears: [[Complex::ZERO; BINS]; 2] }
    }
}

/// Where a response starts: where it first reaches 15% of its peak,
/// between samples.
fn onset(samples: &[f32]) -> f32 {
    let peak = samples.iter().fold(0f32, |m, s| m.max(s.abs()));
    let threshold = 0.15 * peak;
    let n = samples.iter().position(|s| s.abs() >= threshold).unwrap_or(0);
    if n == 0 {
        return 0.0;
    }
    let (before, at) = (samples[n - 1].abs(), samples[n].abs());
    n as f32 - 1.0 + (threshold - before) / (at - before).max(f32::EPSILON)
}

/// Multiplies `spectrum` by `e^(-2 pi i k delay / FFT_SIZE)` (delays it by
/// `delay` samples) and clears the Nyquist bin, which a fractional delay
/// makes complex.
fn delay_spectrum(spectrum: &mut [Complex; BINS], delay: f32) {
    let step = Complex::cis(-2.0 * std::f64::consts::PI * f64::from(delay) / FFT_SIZE as f64);
    let mut turn = Complex::new(1.0, 0.0);
    for bin in spectrum.iter_mut() {
        *bin = bin.mul(turn);
        turn = turn.mul(step);
    }
    spectrum[BINS - 1] = Complex::ZERO;
}

impl Hrtf {
    /// The KEMAR set, prepared once.
    pub fn kemar() -> &'static Hrtf {
        static KEMAR: OnceLock<Hrtf> = OnceLock::new();
        KEMAR.get_or_init(|| Hrtf::parse(DATA).expect("the KEMAR file is well formed"))
    }

    fn parse(data: &[u8]) -> Result<Hrtf, String> {
        let records = records(data)?;
        let fft = Fft::new(FFT_SIZE);
        let mut entries = Vec::with_capacity(records.len());
        let mut directions = Vec::with_capacity(records.len());
        for (elevation, azimuth, ears) in records {
            let onsets = [onset(&ears[0]), onset(&ears[1])];
            let first = onsets[0].min(onsets[1]);
            let mut spectra = [[Complex::ZERO; BINS]; 2];
            let mut delay = [0f32; 2];
            for e in 0..2 {
                let start = (onsets[e].floor() as usize).saturating_sub(PRE);
                let mut buffer = [Complex::ZERO; FFT_SIZE];
                for (n, slot) in buffer.iter_mut().take(TAPS).enumerate() {
                    let sample = ears[e].get(start + n).copied().unwrap_or(0.0);
                    // A half-Hann fade over the last eight taps.
                    let fade = if n + 8 >= TAPS {
                        let x = (TAPS - n) as f32 / 8.0;
                        0.5 - 0.5 * (std::f32::consts::PI * x).cos()
                    } else {
                        1.0
                    };
                    *slot = Complex::new(sample * fade, 0.0);
                }
                fft.forward(&mut buffer);
                spectra[e].copy_from_slice(&buffer[..BINS]);
                // Advance the onset to exactly PRE; the ear's delay goes back on
                // when the filter is made.
                let advance = onsets[e] - (start + PRE) as f32;
                delay_spectrum(&mut spectra[e], -advance);
                delay[e] = (onsets[e] - first).min(MAX_DELAY);
            }
            entries.push(Entry { spectra, delay });
            directions.push((elevation, azimuth));
        }
        // The measurements fall away below about 300 Hz in every direction
        // alike (their diffuse-field mean is 0.92 from 500 Hz up, 0.65 at
        // 172 Hz, 0.30 at 0 Hz: the response the set could hold, not the
        // head's): a low shelf fitted to that mean brings the bass back,
        // once, on everything the filters place.
        let diffuse = |k: usize| {
            let power: f32 = entries.iter().map(|e: &Entry| e.spectra[0][k].norm_sqr() + e.spectra[1][k].norm_sqr()).sum();
            (power / (2 * entries.len()) as f32).sqrt()
        };
        let reference = (3..=23).map(diffuse).sum::<f32>() / 21.0;
        let shelf = Biquad::fit_bass_shelf([diffuse(0), diffuse(1), diffuse(2)].map(|m| reference / m));
        let bin_frequency = |k: usize| k as f32 * crate::SAMPLE_RATE as f32 / FFT_SIZE as f32;
        for entry in &mut entries {
            let shelved = |ear: &[Complex; BINS]| -> f32 {
                let total: f32 = BAND.map(loudness_weight).sum();
                BAND.map(|k| loudness_weight(k) * ear[k].norm_sqr() * shelf.magnitude(bin_frequency(k)).powi(2)).sum::<f32>() / total
            };
            let energy = shelved(&entry.spectra[0]) + shelved(&entry.spectra[1]);
            let gain = (PAIR_ENERGY / energy).sqrt();
            for ear in entry.spectra.iter_mut() {
                for bin in ear.iter_mut() {
                    *bin = bin.scale(gain);
                }
            }
        }
        let mut rings: Vec<Ring> = Vec::new();
        for (i, &(elevation, azimuth)) in directions.iter().enumerate() {
            if rings.last().is_none_or(|ring| ring.elevation != f32::from(elevation)) {
                rings.push(Ring { elevation: f32::from(elevation), points: Vec::new() });
            }
            let ring = rings.last_mut().expect("pushed");
            ring.points.push(Point { azimuth: f32::from(azimuth), entry: i as u16, mirrored: false });
            if azimuth > 0 && azimuth < 180 {
                ring.points.push(Point { azimuth: f32::from(360 - azimuth), entry: i as u16, mirrored: true });
            }
        }
        for ring in &mut rings {
            ring.points.sort_by(|a, b| a.azimuth.total_cmp(&b.azimuth));
        }
        if rings.windows(2).any(|w| w[1].elevation <= w[0].elevation) {
            return Err("elevations out of order".into());
        }
        Ok(Hrtf { entries, rings, shelf })
    }

    /// The bass shelf the placed sounds pass once (levelling included).
    pub fn bass_shelf(&self) -> Biquad {
        self.shelf
    }

    /// The measured directions: (azimuth, elevation) in degrees, azimuth
    /// clockwise from ahead over the whole circle.
    pub fn directions(&self) -> Vec<(f32, f32)> {
        self.rings.iter().flat_map(|ring| ring.points.iter().map(|p| (p.azimuth, ring.elevation))).collect()
    }

    /// The measured responses as the set holds them, in the order of
    /// [`directions`](Self::directions): each direction measured on the
    /// right, and its mirror image on the left (the ears swapped).
    pub fn measured(&self) -> Vec<Measured> {
        let records = records(DATA).expect("the KEMAR file is well formed");
        self.rings
            .iter()
            .flat_map(|ring| {
                ring.points.iter().map(|point| {
                    let ears = records[usize::from(point.entry)].2;
                    Measured { azimuth: point.azimuth, elevation: ring.elevation, ears: if point.mirrored { [ears[1], ears[0]] } else { ears } }
                })
            })
            .collect()
    }

    /// The two points of `ring` around `azimuth` and the second's weight.
    fn around(ring: &Ring, azimuth: f32) -> (Point, Point, f32) {
        let points = &ring.points;
        if points.len() == 1 {
            return (points[0], points[0], 0.0);
        }
        let next = points.partition_point(|p| p.azimuth <= azimuth);
        let (a, b) = match next {
            0 => (points[points.len() - 1], points[0]),
            n if n == points.len() => (points[n - 1], points[0]),
            n => (points[n - 1], points[n]),
        };
        let span = (b.azimuth - a.azimuth).rem_euclid(360.0);
        let offset = (azimuth - a.azimuth).rem_euclid(360.0);
        (a, b, if span > 0.0 { (offset / span).clamp(0.0, 1.0) } else { 0.0 })
    }

    /// The filter pair for a direction: `azimuth` in degrees clockwise from
    /// ahead (90 to the right), `elevation` in degrees up (measured from -40,
    /// below which it stays at -40).
    pub fn filter(&self, azimuth: f32, elevation: f32, out: &mut Filter) {
        let azimuth = azimuth.rem_euclid(360.0);
        let lowest = self.rings[0].elevation;
        let elevation = elevation.clamp(lowest, 90.0);
        let upper = self.rings.partition_point(|ring| ring.elevation <= elevation).clamp(1, self.rings.len() - 1);
        let (low, high) = (&self.rings[upper - 1], &self.rings[upper]);
        let t = ((elevation - low.elevation) / (high.elevation - low.elevation)).clamp(0.0, 1.0);
        let (a, b, u) = Self::around(low, azimuth);
        let (c, d, v) = Self::around(high, azimuth);
        let weights = [(a, (1.0 - t) * (1.0 - u)), (b, (1.0 - t) * u), (c, t * (1.0 - v)), (d, t * v)];
        let mut delay = [0f32; 2];
        for e in 0..2 {
            let ear = &mut out.ears[e];
            ear.fill(Complex::ZERO);
            for (point, weight) in weights {
                if weight == 0.0 {
                    continue;
                }
                let entry = &self.entries[usize::from(point.entry)];
                let source = if point.mirrored { 1 - e } else { e };
                for (bin, value) in ear.iter_mut().zip(&entry.spectra[source]) {
                    *bin = bin.add(value.scale(weight));
                }
                delay[e] += weight * entry.delay[source];
            }
            delay_spectrum(ear, delay[e]);
        }
    }
}

/// A direction in the listener's space (right, down, forward) as the
/// filters take it: (azimuth clockwise from ahead, elevation up), degrees.
pub fn angles(direction: [f32; 3]) -> (f32, f32) {
    let [right, down, forward] = direction;
    let azimuth = right.atan2(forward).to_degrees();
    let elevation = (-down).atan2(right.hypot(forward)).to_degrees();
    (azimuth, elevation)
}

/// The unit vector of (azimuth, elevation) in degrees, in the listener's
/// space (right, down, forward).
pub fn direction(azimuth: f32, elevation: f32) -> [f32; 3] {
    let (a, e) = (azimuth.to_radians(), elevation.to_radians());
    [a.sin() * e.cos(), -e.sin(), a.cos() * e.cos()]
}

#[cfg(test)]
mod tests {
    use super::*;

    fn bin_frequency(k: usize) -> f32 {
        k as f32 * crate::SAMPLE_RATE as f32 / FFT_SIZE as f32
    }

    /// The loudness-weighted energy of one ear's filter with the bass shelf.
    fn energy(filter: &Filter, ear: usize) -> f32 {
        let shelf = Hrtf::kemar().bass_shelf();
        let total: f32 = BAND.map(loudness_weight).sum();
        BAND.map(|k| loudness_weight(k) * filter.ears[ear][k].norm_sqr() * shelf.magnitude(bin_frequency(k)).powi(2)).sum::<f32>() / total
    }

    /// Where each ear's response starts (samples), from the filter.
    fn onsets(filter: &Filter) -> [f32; 2] {
        let fft = Fft::new(FFT_SIZE);
        [0, 1].map(|e| {
            let mut buffer = vec![Complex::ZERO; FFT_SIZE];
            for k in 0..BINS {
                buffer[k] = filter.ears[e][k];
                if k > 0 && k < FFT_SIZE / 2 {
                    buffer[FFT_SIZE - k] = filter.ears[e][k].conj();
                }
            }
            fft.inverse(&mut buffer);
            let samples: Vec<f32> = buffer.iter().map(|c| c.re).collect();
            onset(&samples)
        })
    }

    #[test]
    fn the_set_has_every_measured_direction() {
        let hrtf = Hrtf::kemar();
        assert_eq!(hrtf.entries.len(), 368);
        assert_eq!(hrtf.rings.len(), 14);
        // 368 measured, mirrored to 710 over the whole sphere.
        assert_eq!(hrtf.directions().len(), 710);
    }

    #[test]
    fn the_measured_responses_follow_the_directions_and_mirror_the_ears() {
        let hrtf = Hrtf::kemar();
        let measured = hrtf.measured();
        let directions = hrtf.directions();
        assert_eq!(measured.len(), directions.len());
        for (m, &(azimuth, elevation)) in measured.iter().zip(&directions) {
            assert_eq!((m.azimuth, m.elevation), (azimuth, elevation));
        }
        let at = |azimuth: f32| measured.iter().find(|m| m.azimuth == azimuth && m.elevation == 0.0).expect("measured");
        let (right, left) = (at(90.0), at(270.0));
        assert_eq!((left.ears[0], left.ears[1]), (right.ears[1], right.ears[0]), "the left is the right's mirror image");
        let energy = |ear: &[f32; FRAMES]| ear.iter().map(|s| s * s).sum::<f32>();
        assert!(energy(&right.ears[1]) > 5.0 * energy(&right.ears[0]), "on the right the right ear is the near one");
    }

    #[test]
    fn every_direction_has_the_same_weighted_energy() {
        let hrtf = Hrtf::kemar();
        let mut filter = Filter::default();
        for (azimuth, elevation) in [(0.0, 0.0), (90.0, 0.0), (180.0, 0.0), (270.0, 30.0), (37.0, -25.0), (123.0, 65.0), (0.0, 90.0)] {
            hrtf.filter(azimuth, elevation, &mut filter);
            let both = energy(&filter, 0) + energy(&filter, 1);
            assert!((both / PAIR_ENERGY - 1.0).abs() < 0.08, "{azimuth}/{elevation}: {both}");
        }
    }

    #[test]
    fn the_bass_shelf_restores_the_low_end_the_measurements_lack() {
        let hrtf = Hrtf::kemar();
        let shelf = hrtf.bass_shelf();
        // 0.30 at 0 Hz and 0.65 at 172 Hz against 0.92 above.
        assert!((shelf.magnitude(0.0) - 3.0).abs() < 0.3, "{}", shelf.magnitude(0.0));
        assert!((shelf.magnitude(172.0) - 1.4).abs() < 0.2, "{}", shelf.magnitude(172.0));
        assert!((shelf.magnitude(1000.0) - 1.0).abs() < 0.05);
        let mut filter = Filter::default();
        hrtf.filter(0.0, 0.0, &mut filter);
        let at = |k: usize| filter.ears[0][k].norm_sqr().sqrt() * shelf.magnitude(bin_frequency(k));
        let mid = (3..12).map(at).sum::<f32>() / 9.0;
        assert!((at(0) / mid - 1.0).abs() < 0.35 && (at(1) / mid - 1.0).abs() < 0.35, "ahead: {} {} against {mid}", at(0), at(1));
    }

    #[test]
    fn sides_lead_and_are_louder_at_the_near_ear() {
        let hrtf = Hrtf::kemar();
        let mut filter = Filter::default();
        hrtf.filter(90.0, 0.0, &mut filter);
        assert!(energy(&filter, 1) > 5.0 * energy(&filter, 0), "right: the right ear is louder");
        let [left, right] = onsets(&filter);
        assert!((24.0..32.0).contains(&(left - right)), "right: the left ear lags {} samples", left - right);
        hrtf.filter(270.0, 0.0, &mut filter);
        assert!(energy(&filter, 0) > 5.0 * energy(&filter, 1), "left: the left ear is louder");
        let [left, right] = onsets(&filter);
        assert!((24.0..32.0).contains(&(right - left)));
        hrtf.filter(0.0, 0.0, &mut filter);
        assert!((energy(&filter, 0) / energy(&filter, 1) - 1.0).abs() < 0.01, "ahead: both ears alike");
        let [left, right] = onsets(&filter);
        assert!((left - right).abs() < 0.5);
        assert!((left - PRE as f32).abs() < 1.0, "the near ear starts at once: {left}");
    }

    #[test]
    fn front_and_back_differ_in_spectrum_not_level() {
        let hrtf = Hrtf::kemar();
        let (mut front, mut back) = (Filter::default(), Filter::default());
        hrtf.filter(30.0, 0.0, &mut front);
        hrtf.filter(150.0, 0.0, &mut back);
        let total = |f: &Filter| energy(f, 0) + energy(f, 1);
        assert!((total(&front) / total(&back) - 1.0).abs() < 0.08);
        // The pinna shadows sounds from behind above 3 kHz.
        let high = |f: &Filter| (18..60).map(|k| f.ears[1][k].norm_sqr()).sum::<f32>();
        let low = |f: &Filter| (2..10).map(|k| f.ears[1][k].norm_sqr()).sum::<f32>();
        assert!(high(&front) / low(&front) > 1.5 * high(&back) / low(&back));
    }

    #[test]
    fn interpolation_is_continuous_across_measured_points_and_the_back() {
        let hrtf = Hrtf::kemar();
        let (mut a, mut b) = (Filter::default(), Filter::default());
        for (azimuth, elevation) in [(4.9, 0.0), (179.9, 0.0), (359.9, 10.0), (172.0, 49.9), (5.0, 89.9)] {
            hrtf.filter(azimuth, elevation, &mut a);
            hrtf.filter(azimuth + 0.2, elevation + 0.1, &mut b);
            let diff: f32 = (0..2).map(|e| (0..BINS).map(|k| a.ears[e][k].sub(b.ears[e][k]).norm_sqr()).sum::<f32>()).sum();
            let size: f32 = (0..2).map(|e| (0..BINS).map(|k| a.ears[e][k].norm_sqr()).sum::<f32>()).sum();
            assert!(diff / size < 0.01, "{azimuth}/{elevation}: {}", diff / size);
        }
    }

    #[test]
    fn angles_and_directions_agree() {
        for (azimuth, elevation) in [(0.0, 0.0), (90.0, 0.0), (-135.0, 20.0), (45.0, -30.0)] {
            let (a, e) = angles(direction(azimuth, elevation));
            assert!((a - azimuth).abs() < 1e-3 && (e - elevation).abs() < 1e-3);
        }
        assert_eq!(angles([1.0, 0.0, 0.0]), (90.0, 0.0), "right");
        assert!((angles([0.0, -1.0, 0.0]).1 - 90.0).abs() < 1e-4, "up is y negative");
    }
}

