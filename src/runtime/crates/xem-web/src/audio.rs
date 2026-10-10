//! The game's sound for Web Audio: the SPU's 44.1 kHz output (interleaved
//! 16-bit stereo, `Spu::take_audio`), mixed by the audio settings into the
//! float samples the page's AudioWorklet plays (its context runs at 44.1 kHz;
//! the browser resamples to the device).

use std::collections::VecDeque;

use xem_settings::{AudioOutput, MAX_VOLUME, Settings};

/// The SPU's output rate; the page's AudioContext runs at it.
pub const SAMPLE_RATE: u32 = 44_100;
/// Samples (L and R count separately) kept for the page between animation
/// frames; older ones are dropped (a session running frames faster than real
/// time, e.g. by automation, or a page that does not play audio yet).
const MAX_PENDING: usize = SAMPLE_RATE as usize / 4 * 2;

#[derive(Default)]
pub struct Mixer {
    pending: VecDeque<i16>,
    /// Stereo frames handed to the page.
    pub frames: u64,
    /// Stereo frames dropped because nobody took them in time.
    pub dropped: u64,
}

impl Mixer {
    /// Keep the SPU's new samples (interleaved L/R) for the page.
    pub fn push(&mut self, samples: &[i16]) {
        self.pending.extend(samples);
        let excess = self.pending.len().saturating_sub(MAX_PENDING) & !1;
        if excess > 0 {
            self.pending.drain(..excess);
            self.dropped += excess as u64 / 2;
        }
    }

    /// Everything pending as interleaved stereo floats, mixed by the volume
    /// and output settings.
    pub fn take(&mut self, settings: &Settings) -> Vec<f32> {
        let gain = f32::from(settings.master_volume) / f32::from(MAX_VOLUME) / 32768.0;
        let mut out = Vec::with_capacity(self.pending.len());
        let mut samples = self.pending.drain(..);
        while let (Some(left), Some(right)) = (samples.next(), samples.next()) {
            let (left, right) = (f32::from(left), f32::from(right));
            let (mid, side) = ((left + right) / 2.0, (left - right) / 2.0);
            let (left, right) = match settings.audio_output {
                AudioOutput::Mono => (mid, mid),
                AudioOutput::Stereo => (left, right),
                AudioOutput::Wide => (mid + side * 1.5, mid - side * 1.5),
            };
            out.extend([(left * gain).clamp(-1.0, 1.0), (right * gain).clamp(-1.0, 1.0)]);
        }
        self.frames += out.len() as u64 / 2;
        out
    }
}
