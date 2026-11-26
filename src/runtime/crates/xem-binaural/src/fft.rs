//! A radix-2 complex FFT of one size (the renderer's 256 points).

/// A complex number.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Complex {
    pub re: f32,
    pub im: f32,
}

impl Complex {
    pub const ZERO: Complex = Complex { re: 0.0, im: 0.0 };

    pub const fn new(re: f32, im: f32) -> Self {
        Complex { re, im }
    }

    #[inline]
    pub fn mul(self, o: Complex) -> Complex {
        Complex::new(self.re * o.re - self.im * o.im, self.re * o.im + self.im * o.re)
    }

    #[inline]
    pub fn add(self, o: Complex) -> Complex {
        Complex::new(self.re + o.re, self.im + o.im)
    }

    #[inline]
    pub fn sub(self, o: Complex) -> Complex {
        Complex::new(self.re - o.re, self.im - o.im)
    }

    #[inline]
    pub fn scale(self, s: f32) -> Complex {
        Complex::new(self.re * s, self.im * s)
    }

    #[inline]
    pub fn conj(self) -> Complex {
        Complex::new(self.re, -self.im)
    }

    #[inline]
    pub fn norm_sqr(self) -> f32 {
        self.re * self.re + self.im * self.im
    }

    /// `e^(i * angle)`.
    pub fn cis(angle: f64) -> Complex {
        Complex::new(angle.cos() as f32, angle.sin() as f32)
    }
}

/// An in-place FFT of `n` points (a power of two).
#[derive(Clone)]
pub struct Fft {
    n: usize,
    /// `e^(-2 pi i k / n)` for k below n / 2.
    twiddles: Vec<Complex>,
    reverse: Vec<usize>,
}

impl Fft {
    pub fn new(n: usize) -> Self {
        assert!(n.is_power_of_two() && n >= 2, "FFT size must be a power of two");
        let bits = n.trailing_zeros();
        let twiddles = (0..n / 2).map(|k| Complex::cis(-2.0 * std::f64::consts::PI * k as f64 / n as f64)).collect();
        let reverse = (0..n).map(|i| i.reverse_bits() >> (usize::BITS - bits)).collect();
        Fft { n, twiddles, reverse }
    }

    pub fn len(&self) -> usize {
        self.n
    }

    pub fn is_empty(&self) -> bool {
        self.n == 0
    }

    /// `X(k) = sum x(t) e^(-2 pi i k t / n)`, unscaled.
    pub fn forward(&self, data: &mut [Complex]) {
        self.transform(data, false);
    }

    /// Transform one or two half-length real blocks, padded with zeroes.
    /// After bit reversal every odd input is zero. Form the four-point
    /// groups directly, retaining the butterfly order and twiddles used
    /// to calibrate the renderer's impulse responses.
    pub(crate) fn forward_block(&self, a: &[f32], b: Option<&[f32]>, data: &mut [Complex]) {
        debug_assert_eq!(data.len(), self.n);
        debug_assert_eq!(a.len(), self.n / 2);
        let quarter = self.n / 4;
        let w = self.twiddles[quarter];
        for (group, output) in data.chunks_exact_mut(4).enumerate() {
            let t = self.reverse[group * 4];
            let sample = |i: usize| Complex::new(a[i], b.map_or(0.0, |b| b[i]));
            let (even, odd) = (sample(t), sample(t + quarter));
            let rotated = odd.mul(w);
            output.copy_from_slice(&[even.add(odd), even.add(rotated), even.sub(odd), even.sub(rotated)]);
        }
        self.butterflies(data, false, 8);
    }

    /// The inverse, scaled by `1 / n`.
    pub fn inverse(&self, data: &mut [Complex]) {
        self.transform(data, true);
        let scale = 1.0 / self.n as f32;
        for x in data.iter_mut() {
            *x = x.scale(scale);
        }
    }

    fn transform(&self, data: &mut [Complex], inverse: bool) {
        assert_eq!(data.len(), self.n);
        for i in 0..self.n {
            let j = self.reverse[i];
            if i < j {
                data.swap(i, j);
            }
        }
        self.butterflies(data, inverse, 2);
    }

    fn butterflies(&self, data: &mut [Complex], inverse: bool, mut size: usize) {
        while size <= self.n {
            let half = size / 2;
            let step = self.n / size;
            for start in (0..self.n).step_by(size) {
                for k in 0..half {
                    let w = self.twiddles[k * step];
                    let w = if inverse { w.conj() } else { w };
                    let a = data[start + k];
                    let b = data[start + k + half].mul(w);
                    data[start + k] = a.add(b);
                    data[start + k + half] = a.sub(b);
                }
            }
            size *= 2;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_single_audio_block_matches_the_zero_padded_complex_fft() {
        let fft = Fft::new(256);
        let input: Vec<_> = (0..128).map(|i| ((i * 13 % 19) as f32 - 9.0) / 9.0).collect();
        let mut reference = vec![Complex::ZERO; 256];
        for (v, sample) in reference.iter_mut().zip(&input) { v.re = *sample; }
        fft.forward(&mut reference);
        let mut actual = [Complex::ZERO; 256];
        fft.forward_block(&input, None, &mut actual);
        assert_eq!(&actual[..], &reference);
    }

    #[test]
    fn matches_the_direct_transform_and_inverts() {
        let n = 64;
        let fft = Fft::new(n);
        let input: Vec<Complex> = (0..n).map(|t| Complex::new(((t * 7) % 11) as f32 - 5.0, ((t * 3) % 5) as f32)).collect();
        let mut data = input.clone();
        fft.forward(&mut data);
        for (k, x) in data.iter().enumerate() {
            let mut sum = Complex::ZERO;
            for (t, v) in input.iter().enumerate() {
                sum = sum.add(v.mul(Complex::cis(-2.0 * std::f64::consts::PI * (k * t) as f64 / n as f64)));
            }
            assert!((sum.re - x.re).abs() < 1e-3 && (sum.im - x.im).abs() < 1e-3, "bin {k}");
        }
        fft.inverse(&mut data);
        for (a, b) in data.iter().zip(&input) {
            assert!((a.re - b.re).abs() < 1e-4 && (a.im - b.im).abs() < 1e-4);
        }
    }
}
