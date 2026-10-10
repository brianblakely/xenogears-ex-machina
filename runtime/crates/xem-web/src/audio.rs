//! The audio the page plays once the user starts it: until the port drives
//! xem-spu, a test tone (A4 left, C#5 right) mixed by the audio settings.

use xem_settings::{AudioOutput, MAX_VOLUME, Settings};

pub struct Tone {
    phase: [f64; 2],
    pub frames: u64,
}

const PITCH: [f64; 2] = [440.0, 554.37];
const LEVEL: f32 = 0.2;

impl Tone {
    pub fn new() -> Self {
        Tone { phase: [0.0; 2], frames: 0 }
    }

    /// `frames` interleaved stereo frames at `rate` Hz.
    pub fn render(&mut self, frames: usize, rate: f32, settings: &Settings) -> Vec<f32> {
        let gain = LEVEL * f32::from(settings.master_volume) / f32::from(MAX_VOLUME);
        let mut out = Vec::with_capacity(frames * 2);
        for _ in 0..frames {
            let [left, right] = [0, 1].map(|channel| {
                self.phase[channel] = (self.phase[channel] + PITCH[channel] / f64::from(rate)).fract();
                (self.phase[channel] * std::f64::consts::TAU).sin() as f32
            });
            let (mid, side) = ((left + right) / 2.0, (left - right) / 2.0);
            let (left, right) = match settings.audio_output {
                AudioOutput::Mono => (mid, mid),
                AudioOutput::Stereo => (left, right),
                AudioOutput::Wide => (mid + side * 1.5, mid - side * 1.5),
            };
            out.extend([left * gain, right * gain]);
        }
        self.frames += frames as u64;
        out
    }
}
