//! Simulation time controls shared with application settings.
use serde::{Deserialize, Serialize};
use std::fmt;

pub const DEFAULT_REWIND_INTERVAL_SECONDS: u32 = 1;
pub const MAX_REWIND_INTERVAL_SECONDS: u32 = 3600;
pub const DEFAULT_REWIND_HISTORY_SECONDS: u32 = 30;
pub const MAX_REWIND_HISTORY_SECONDS: u32 = 3600;

/// The configured fast-forward rate. Turning fast-forward off always runs at 1×.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum FastForwardRate {
    Fixed(u32),
    Uncapped,
}
pub const FAST_FORWARD_RATES: [FastForwardRate; 6] = [
    FastForwardRate::Fixed(1), FastForwardRate::Fixed(5), FastForwardRate::Fixed(10),
    FastForwardRate::Fixed(15), FastForwardRate::Fixed(20), FastForwardRate::Uncapped,
];
impl Default for FastForwardRate {
    fn default() -> Self { Self::Fixed(10) }
}
impl FastForwardRate {
    pub fn validate(self) -> Result<(), String> {
        if FAST_FORWARD_RATES.contains(&self) { Ok(()) }
        else { Err("fast-forward rate must be 1×, 5×, 10×, 15×, 20× or Uncapped".into()) }
    }
    /// No frame limit when uncapped; music then uses the host's normal clock.
    pub fn factor(self) -> Option<u32> {
        match self { Self::Fixed(factor) => Some(factor), Self::Uncapped => None }
    }
    pub fn increase(self) -> Self {
        FAST_FORWARD_RATES.iter().position(|rate| *rate == self)
            .map_or(self, |i| FAST_FORWARD_RATES[(i + 1).min(FAST_FORWARD_RATES.len() - 1)])
    }
    pub fn decrease(self) -> Self {
        FAST_FORWARD_RATES.iter().position(|rate| *rate == self)
            .map_or(self, |i| FAST_FORWARD_RATES[i.saturating_sub(1)])
    }
}
/// Measures how fast fast-forward runs: a fixed rate the host cannot keep up
/// with would slow the music (timed by game frames) and starve the sound, so
/// the host paces it at the rate it reaches instead
/// (`Session::pace_fast_forward`), and tries a step faster while it keeps up.
#[derive(Clone, Copy, Debug, Default)]
pub struct FastForwardPacer {
    seconds: f64,
    frames: u64,
    behind: bool,
}

impl FastForwardPacer {
    /// After a host batch: `frames` run over `seconds` of wall time since
    /// the last batch, `behind` when the batch stopped short of the frames
    /// due. Every half second, the pace to use when it should change
    /// (`pace` the one in use, `rate` the configured one).
    pub fn note(&mut self, seconds: f64, frames: u32, behind: bool, pace: u32, rate: u32) -> Option<u32> {
        self.seconds += seconds;
        self.frames += u64::from(frames);
        self.behind |= behind;
        if self.seconds < 0.5 {
            return None;
        }
        let reached = (self.frames as f64 / self.seconds / 60.0).floor() as u32;
        let next = if self.behind { reached.min(pace) } else { pace + 1 }.clamp(1, rate.max(1));
        *self = Self::default();
        (next != pace).then_some(next)
    }

    pub fn reset(&mut self) {
        *self = Self::default();
    }
}

impl fmt::Display for FastForwardRate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self { Self::Fixed(factor) => write!(f,"{factor}×"), Self::Uncapped => f.write_str("Uncapped") }
    }
}

#[cfg(test)]
mod tests {
    use super::FastForwardPacer;

    #[test]
    fn the_pace_follows_the_rate_the_host_reaches() {
        let mut pacer = FastForwardPacer::default();
        // Behind at 10x: 180 frames in 0.6 s is 5x.
        assert_eq!(pacer.note(0.3, 90, true, 10, 10), None);
        assert_eq!(pacer.note(0.3, 90, false, 10, 10), Some(5));
        // Keeping up, it tries a step faster, up to the configured rate.
        assert_eq!(pacer.note(0.5, 150, false, 5, 10), Some(6));
        assert_eq!(pacer.note(0.5, 300, false, 10, 10), None);
        // Never below 1x.
        assert_eq!(pacer.note(0.5, 10, true, 3, 10), Some(1));
    }
}
