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

#[derive(Debug, Clone, Default)]
pub struct Clock {
    pub now: u64,
    pub vblanks: u64,
    pub counters: [RootCounter; 3],
}

/// An interrupt the clock raises.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Tick {
    VBlank,
    RootCounter(usize),
}

impl Clock {
    pub fn next_vblank(&self) -> u64 {
        (self.vblanks + 1) * VBLANK_CYCLES
    }

    fn next_counter_interrupt(&self, n: usize) -> Option<u64> {
        let counter = &self.counters[n];
        (counter.running && counter.mode & RCNT_MODE_INTERRUPT != 0).then_some(counter.due)
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
                let period = self.counters[n].period(n);
                self.counters[n].due += period;
            }
        }
        Some(best.1)
    }

    pub fn set_counter(&mut self, n: usize, target: u32, mode: u32) {
        let counter = &mut self.counters[n];
        counter.target = target & 0xFFFF;
        counter.mode = mode;
        counter.origin = self.now;
        counter.due = self.now + counter.period(n);
    }

    pub fn start_counter(&mut self, n: usize) {
        let counter = &mut self.counters[n];
        counter.running = true;
        counter.origin = self.now;
        counter.due = self.now + counter.period(n);
    }

    pub fn stop_counter(&mut self, n: usize) {
        self.counters[n].running = false;
    }

    pub fn read_counter(&self, n: usize) -> u32 {
        let counter = &self.counters[n];
        let counts = (self.now - counter.origin) / RootCounter::divisor(n);
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
}
