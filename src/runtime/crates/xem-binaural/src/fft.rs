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
        let mut size = 2;
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
