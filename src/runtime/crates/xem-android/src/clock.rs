//! The host's simulation clock: it advances only while the app is in the
//! foreground and never by more than one step's worth per frame, so a resume
//! (or a stall) brings no catch-up burst.

use std::time::{Duration, Instant};

/// The most one frame may advance simulation time.
pub const MAX_STEP: Duration = Duration::from_millis(100);

#[derive(Debug)]
pub struct SimClock {
    time: Duration,
    last: Option<Instant>,
}

impl SimClock {
    /// A running clock at zero.
    pub fn start(now: Instant) -> Self {
        Self {
            time: Duration::ZERO,
            last: Some(now),
        }
    }

    pub fn is_paused(&self) -> bool {
        self.last.is_none()
    }

    /// Simulation time so far.
    pub fn time(&self) -> Duration {
        self.time
    }

    /// Stops the clock (backgrounded).
    pub fn pause(&mut self) {
        self.last = None;
    }

    /// Restarts the clock from `now`; the paused span does not count.
    pub fn resume(&mut self, now: Instant) {
        if self.last.is_none() {
            self.last = Some(now);
        }
    }

    /// Advances to `now` (by at most [`MAX_STEP`]) and returns the time.
    pub fn tick(&mut self, now: Instant) -> Duration {
        if let Some(last) = self.last {
            self.time += now.saturating_duration_since(last).min(MAX_STEP);
            self.last = Some(now);
        }
        self.time
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn paused_time_does_not_count_and_steps_are_capped() {
        let t0 = Instant::now();
        let ms = Duration::from_millis;
        let mut clock = SimClock::start(t0);
        assert_eq!(clock.tick(t0 + ms(16)), ms(16));
        clock.pause();
        assert!(clock.is_paused());
        assert_eq!(clock.tick(t0 + ms(5000)), ms(16));
        clock.resume(t0 + ms(10_000));
        assert_eq!(clock.tick(t0 + ms(10_016)), ms(32));
        // A stall advances one capped step.
        assert_eq!(clock.tick(t0 + ms(12_016)), ms(32) + MAX_STEP);
    }
}
