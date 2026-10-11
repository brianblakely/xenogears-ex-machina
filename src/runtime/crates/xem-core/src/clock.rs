//! The virtual clock: the console's time in CPU cycles, advanced only at the
//! game's waits. The recovered C runs in zero time between them, so time,
//! interrupts and their order depend only on the sequence of waits and are
//! reproducible.

/// CPU cycles per second (33.8688 MHz).
pub const CPU_HZ: u64 = 33_868_800;
/// Cycles per NTSC frame (about 60 Hz).
pub const VBLANK_CYCLES: u64 = 564_480;
/// Lines per NTSC frame and cycles per line (root counter 1 counts lines).
pub const LINES_PER_FRAME: u64 = 263;
pub const LINE_CYCLES: u64 = VBLANK_CYCLES / LINES_PER_FRAME;

/// PsyQ's root counter modes (libapi.h): interrupt on reaching the target.
pub const RCNT_MODE_INTERRUPT: u32 = 0x1000;

#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct RootCounter {
    pub target: u32,
    pub mode: u32,
    pub running: bool,
    /// Cycle at which the count was last zero.
    pub origin: u64,
    /// Cycle of the next target interrupt.
    pub due: u64,
}

impl RootCounter {
    /// CPU cycles per count: counter 0 the system clock, 1 the lines, 2 the
    /// system clock / 8 (the sound driver's 240 Hz tick: target 0x44E8).
    pub fn divisor(n: usize) -> u64 {
        match n {
            1 => LINE_CYCLES,
            2 => 8,
            _ => 1,
        }
    }

    fn period(&self, n: usize) -> u64 {
        // The count resets when it reaches the target (0 counts to 0x10000).
        let counts = if self.target == 0 { 0x1_0000 } else { self.target as u64 };
        counts * Self::divisor(n)
    }
}

#[derive(Debug, Clone)]
pub struct Clock {
    pub now: u64,
    pub vblanks: u64,
    pub counters: [RootCounter; 3],
    /// Only the original sound driver's counter (2, target 0x44e8) uses this
    /// factor. Gameplay counters and vertical blanks keep their original rate.
    pub audio_speed: u32,
    pub audio_now: u64,
    pub audio_due: u64,
    /// Host pacing attachment, like an output device: not saved in game state.
    pub external_audio: bool,
}

impl Default for Clock {
    fn default() -> Self {
        Self { now: 0, vblanks: 0, counters: [RootCounter::default(); 3], audio_speed: 1, audio_now: 0, audio_due: 0, external_audio: false }
    }
}

/// An interrupt the clock raises.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tick {
    VBlank,
    RootCounter(usize),
}

impl Clock {
    fn counter_speed(&self, n: usize) -> u64 {
        if n == 2 && self.counters[n].target == 0x44e8 { self.audio_speed as u64 } else { 1 }
    }

    pub fn sound_counter(&self) -> bool { self.counters[2].target == 0x44e8 }
    pub fn audio_period(&self) -> u64 { self.counters[2].period(2) }
    pub fn audio_running(&self) -> bool { self.sound_counter() && self.counters[2].running && self.counters[2].mode & RCNT_MODE_INTERRUPT != 0 }
    pub fn reconcile_audio_due(&mut self) {
        if self.sound_counter() {
            self.counters[2].due = self.now + self.audio_due.saturating_sub(self.audio_now) * u64::from(self.audio_speed);
        }
    }

    /// Preserve the remaining fraction of the sound tick when pacing changes.
    pub fn set_audio_speed(&mut self, speed: u32) {
        assert!((1..=20).contains(&speed));
        if speed == self.audio_speed { return; }
        if self.counters[2].target == 0x44e8 {
            let counter = &mut self.counters[2];
            let remaining = counter.due.saturating_sub(self.now);
            counter.due = self.now + (remaining * u64::from(speed)).div_ceil(u64::from(self.audio_speed));
        }
        self.audio_speed = speed;
    }
    pub fn next_vblank(&self) -> u64 {
        (self.vblanks + 1) * VBLANK_CYCLES
    }

    fn next_counter_interrupt(&self, n: usize) -> Option<u64> {
        let counter = &self.counters[n];
        (counter.running && counter.mode & RCNT_MODE_INTERRUPT != 0 && !(n == 2 && self.sound_counter() && self.external_audio)).then_some(counter.due)
    }

    /// The next interrupt at or before `limit`, in time order (a vertical
    /// blank first on a tie), with the clock moved to it.
    /// The cycle and kind of the next interrupt.
    pub fn peek(&self) -> (u64, Tick) {
        let mut best = (self.next_vblank(), Tick::VBlank);
        for n in 0..3 {
            if let Some(at) = self.next_counter_interrupt(n) {
                if at < best.0 {
                    best = (at, Tick::RootCounter(n));
                }
            }
        }
        best
    }

    pub fn next_tick(&mut self, limit: u64) -> Option<Tick> {
        let best = self.peek();
        if best.0 > limit {
            return None;
        }
        self.now = best.0;
        match best.1 {
            Tick::VBlank => self.vblanks += 1,
            Tick::RootCounter(n) => {
                let period = self.counters[n].period(n) * self.counter_speed(n);
                self.counters[n].due += period;
                if n == 2 && self.sound_counter() { self.audio_due += self.audio_period(); }
            }
        }
        Some(best.1)
    }

    pub fn set_counter(&mut self, n: usize, target: u32, mode: u32) {
        let counter = &mut self.counters[n];
        counter.target = target & 0xFFFF;
        counter.mode = mode;
        counter.origin = self.now;
        self.counters[n].due = self.now + self.counters[n].period(n) * self.counter_speed(n);
        if n == 2 && self.sound_counter() { self.audio_due = self.audio_now + self.audio_period(); }
    }

    pub fn start_counter(&mut self, n: usize) {
        let speed = self.counter_speed(n);
        let counter = &mut self.counters[n];
        counter.running = true;
        counter.origin = self.now;
        counter.due = self.now + counter.period(n) * speed;
        if n == 2 && self.sound_counter() { self.audio_due = self.audio_now + self.audio_period(); }
    }

    pub fn stop_counter(&mut self, n: usize) {
        self.counters[n].running = false;
    }

    pub fn read_counter(&self, n: usize) -> u32 {
        let counter = &self.counters[n];
        if n == 2 && counter.target == 0x44e8 && counter.running {
            if self.external_audio {
                let period = self.audio_period();
                return (((period - self.audio_due.saturating_sub(self.audio_now) % period) % period) / RootCounter::divisor(n)) as u32;
            }
            let period = counter.period(n) * self.counter_speed(n);
            let remaining = counter.due.saturating_sub(self.now) % period;
            return (((period - remaining) % period) / (RootCounter::divisor(n) * self.counter_speed(n))) as u32;
        }
        let counts = (self.now - counter.origin) / (RootCounter::divisor(n) * self.counter_speed(n));
        let wrap = if counter.target == 0 || counter.mode & RCNT_MODE_INTERRUPT == 0 {
            0x1_0000
        } else {
            counter.target as u64
        };
        // Counter 1 counts lines continuously, as the hardware's does.
        if n == 1 && !counter.running {
            return ((self.now / LINE_CYCLES) & 0xFFFF) as u32;
        }
        (counts % wrap) as u32
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sound_tick_runs_at_240_hz() {
        let mut clock = Clock::default();
        clock.set_counter(2, 0x44E8, RCNT_MODE_INTERRUPT);
        clock.start_counter(2);
        let mut ticks = 0;
        let mut vblanks = 0;
        while let Some(tick) = clock.next_tick(CPU_HZ) {
            match tick {
                Tick::RootCounter(2) => ticks += 1,
                Tick::VBlank => vblanks += 1,
                _ => unreachable!(),
            }
        }
        assert_eq!(ticks, 240);
        assert_eq!(vblanks, 60);
    }

    #[test]
    fn fast_forward_keeps_sound_ticks_at_normal_wall_time_rate() {
        for speed in 1..=20 {
            let mut clock = Clock::default();
            clock.set_audio_speed(speed);
            clock.set_counter(2, 0x44e8, RCNT_MODE_INTERRUPT);
            clock.start_counter(2);
            let mut sound = 0;
            let mut frames = 0;
            while let Some(tick) = clock.next_tick(CPU_HZ * u64::from(speed)) {
                match tick { Tick::RootCounter(2) => sound += 1, Tick::VBlank => frames += 1, _ => unreachable!() }
            }
            assert_eq!(sound, 240);
            assert_eq!(frames, 60 * speed);
        }
    }

    #[test]
    fn changing_speed_preserves_sound_tick_phase_and_other_timers() {
        let mut clock = Clock::default();
        clock.set_counter(2, 0x44e8, RCNT_MODE_INTERRUPT);
        clock.start_counter(2);
        clock.set_counter(0, 123, RCNT_MODE_INTERRUPT);
        clock.start_counter(0);
        clock.now = 800;
        let phase = clock.read_counter(2);
        let other = clock.counters[0];
        let remaining = clock.counters[2].due - clock.now;
        clock.set_audio_speed(16);
        assert_eq!(clock.read_counter(2), phase);
        assert_eq!(clock.counters[2].due - clock.now, remaining * 16);
        assert_eq!(clock.counters[0], other);
        clock.set_audio_speed(1);
        assert_eq!(clock.counters[2].due - clock.now, remaining);
    }
}
