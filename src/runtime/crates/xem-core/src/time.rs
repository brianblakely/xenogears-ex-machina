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
impl fmt::Display for FastForwardRate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self { Self::Fixed(factor) => write!(f,"{factor}×"), Self::Uncapped => f.write_str("Uncapped") }
    }
}
