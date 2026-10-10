//! The envelope stepper shared by the ADSR generator and the sweep volumes
//! (psx-spx "SPU Volume and ADSR Generator", "Envelope Operation depending on
//! Shift/Step/Mode/Direction", in the counter form of its later revision).
//!
//! A rate is 7 bits: `shift << 2 | step`. Per 44.1 kHz cycle:
//!
//! ```text
//! step = 7 - (rate & 3), inverted (+7..+4 -> -8..-5) if decreasing xor phase-negative
//! step <<= max(0, 11 - shift);  increment = 0x8000 >> max(0, shift - 11)
//! exponential increase above 0x6000: shift < 10: step >>= 2;
//!     shift >= 11: increment >>= 2; shift 10: both >>= 1
//! exponential decrease: step = step * level >> 15
//! increment = max(increment, 1) unless the rate is all ones (never steps)
//! counter += increment; on bit 15: counter = 0, level += step, saturated
//! ```
//!
//! Choices the documentation leaves open: the counter restarts at 0 after a
//! step (every increment is a power of two, so this equals clearing bit 15),
//! the exponential decrease scales with an arithmetic shift, and the counter
//! restarts at every phase change and key on.

/// One envelope's operating parameters.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Params {
    /// `shift << 2 | step`, 7 bits.
    pub rate: u8,
    pub decreasing: bool,
    pub exponential: bool,
    /// Sweep phase bit (volume sweeps only).
    pub phase_negative: bool,
    /// The rate field is all ones: the envelope never steps (nor saturates).
    pub frozen: bool,
}

/// Advances `level` by one 44.1 kHz cycle.
#[inline]
pub fn tick(level: &mut i16, counter: &mut u16, p: Params) {
    let shift = (p.rate >> 2) as i32;
    let mut step = 7 - (p.rate & 3) as i32;
    if p.decreasing != p.phase_negative {
        step = !step;
    }
    step <<= (11 - shift).max(0);
    let mut increment: u32 = 0x8000 >> (shift - 11).max(0);
    let lvl = *level as i32;
    if p.exponential && !p.decreasing && lvl > 0x6000 {
        if shift < 10 {
            step >>= 2;
        } else if shift >= 11 {
            increment >>= 2;
        } else {
            step >>= 1;
            increment >>= 1;
        }
    } else if p.exponential && p.decreasing {
        step = (step * lvl) >> 15;
    }
    if !p.frozen {
        increment = increment.max(1);
    }
    let c = *counter as u32 + increment;
    if c & 0x8000 == 0 {
        *counter = c as u16;
        return;
    }
    *counter = 0;
    let next = lvl + step;
    let next = if !p.decreasing {
        next.clamp(-0x8000, 0x7FFF)
    } else if p.phase_negative {
        next.clamp(-0x8000, 0)
    } else {
        next.max(0)
    };
    *level = next as i16;
}

/// ADSR phases.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum Phase {
    #[default]
    Off = 0,
    Attack = 1,
    Decay = 2,
    Sustain = 3,
    Release = 4,
}

impl Phase {
    pub(crate) fn from_u8(v: u8) -> Option<Phase> {
        Some(match v {
            0 => Phase::Off,
            1 => Phase::Attack,
            2 => Phase::Decay,
            3 => Phase::Sustain,
            4 => Phase::Release,
            _ => return None,
        })
    }
}

/// The sustain level of ADSR1 bits 0-3: `(n + 1) * 0x800`.
#[inline]
pub fn sustain_level(adsr1: u16) -> i32 {
    ((adsr1 & 0xF) as i32 + 1) * 0x800
}

/// The envelope parameters of an ADSR phase, from the two ADSR registers.
pub fn adsr_params(phase: Phase, adsr1: u16, adsr2: u16) -> Option<Params> {
    Some(match phase {
        Phase::Off => return None,
        Phase::Attack => {
            let rate = ((adsr1 >> 8) & 0x7F) as u8;
            Params {
                rate,
                decreasing: false,
                exponential: adsr1 & 0x8000 != 0,
                phase_negative: false,
                frozen: rate == 0x7F,
            }
        }
        // Decay: shift in bits 4-7, step fixed -8, always exponential.
        Phase::Decay => Params {
            rate: (((adsr1 >> 4) & 0xF) as u8) << 2,
            decreasing: true,
            exponential: true,
            phase_negative: false,
            frozen: false,
        },
        Phase::Sustain => {
            let rate = ((adsr2 >> 6) & 0x7F) as u8;
            Params {
                rate,
                decreasing: adsr2 & 0x4000 != 0,
                exponential: adsr2 & 0x8000 != 0,
                phase_negative: false,
                frozen: rate == 0x7F,
            }
        }
        // Release: shift in bits 0-4, step fixed -8. psx-spx: a release
        // field of all ones (0x1F) never steps.
        Phase::Release => {
            let shift = (adsr2 & 0x1F) as u8;
            Params {
                rate: shift << 2,
                decreasing: true,
                exponential: adsr2 & 0x20 != 0,
                phase_negative: false,
                frozen: shift == 0x1F,
            }
        }
    })
}

/// Advances an ADSR envelope by one cycle. Transitions are taken at the
/// start of the cycle from the current level (Attack ends at 0x7FFF, Decay at
/// or below the sustain level, Release at 0), then the phase steps once.
pub fn adsr_tick(phase: &mut Phase, level: &mut i16, counter: &mut u16, adsr1: u16, adsr2: u16) {
    let next = match *phase {
        Phase::Attack if *level as i32 >= 0x7FFF => Phase::Decay,
        Phase::Decay if (*level as i32) <= sustain_level(adsr1) => Phase::Sustain,
        Phase::Release if *level <= 0 => {
            *level = 0;
            Phase::Off
        }
        p => p,
    };
    if next != *phase {
        *phase = next;
        *counter = 0;
        // A decay that starts at or below its sustain level ends at once.
        if next == Phase::Decay && (*level as i32) <= sustain_level(adsr1) {
            *phase = Phase::Sustain;
        }
    }
    if let Some(p) = adsr_params(*phase, adsr1, adsr2) {
        tick(level, counter, p);
    }
}

/// The parameters of a sweep-mode volume register (bit 15 set).
pub fn sweep_params(reg: u16) -> Params {
    let rate = (reg & 0x7F) as u8;
    Params {
        rate,
        decreasing: reg & 0x2000 != 0,
        exponential: reg & 0x4000 != 0,
        phase_negative: reg & 0x1000 != 0,
        frozen: rate == 0x7F,
    }
}

/// Advances a volume register's current level by one cycle: a fixed volume
/// (bit 15 clear) is `bits 0-14 * 2`; a sweep steps from the current level.
#[inline]
pub fn volume_tick(reg: u16, current: &mut i16, counter: &mut u16) {
    if reg & 0x8000 == 0 {
        *current = (reg << 1) as i16;
        *counter = 0;
    } else {
        tick(current, counter, sweep_params(reg));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn run(level: &mut i16, counter: &mut u16, p: Params, cycles: usize) {
        for _ in 0..cycles {
            tick(level, counter, p);
        }
    }

    fn lin_inc(rate: u8) -> Params {
        Params {
            rate,
            decreasing: false,
            exponential: false,
            phase_negative: false,
            frozen: rate == 0x7F,
        }
    }

    #[test]
    fn linear_steps_and_cycles() {
        // Shift 0, step +7: +7 << 11 every cycle.
        let (mut l, mut c) = (0i16, 0u16);
        tick(&mut l, &mut c, lin_inc(0));
        assert_eq!(l, 7 << 11);
        // Shift 11, step +4 (field 3): +4 every cycle.
        let (mut l, mut c) = (0i16, 0u16);
        run(&mut l, &mut c, lin_inc((11 << 2) | 3), 10);
        assert_eq!(l, 40);
        // Shift 13: one step of +7 every 4 cycles.
        let (mut l, mut c) = (0i16, 0u16);
        run(&mut l, &mut c, lin_inc(13 << 2), 3);
        assert_eq!(l, 0);
        tick(&mut l, &mut c, lin_inc(13 << 2));
        assert_eq!(l, 7);
        // Saturation at 0x7FFF.
        let (mut l, mut c) = (0x7FF0i16, 0u16);
        tick(&mut l, &mut c, lin_inc(0));
        assert_eq!(l, 0x7FFF);
    }

    #[test]
    fn exponential_increase_slows_above_0x6000() {
        // Shift 4, step +7: +0x380 per cycle.
        let p = Params {
            rate: 4 << 2,
            decreasing: false,
            exponential: true,
            phase_negative: false,
            frozen: false,
        };
        let (mut l, mut c) = (0x6000i16, 0u16);
        tick(&mut l, &mut c, p);
        assert_eq!(l, 0x6000 + 0x380, "at 0x6000 the full step applies");
        let (mut l, mut c) = (0x6001i16, 0u16);
        tick(&mut l, &mut c, p);
        assert_eq!(l, 0x6001 + (0x380 >> 2));
        // Shift >= 11: the step stays, the rate drops to a quarter.
        let p = Params { rate: 12 << 2, ..p };
        let (mut l, mut c) = (0x6001i16, 0u16);
        run(&mut l, &mut c, p, 7);
        assert_eq!(l, 0x6001);
        tick(&mut l, &mut c, p);
        assert_eq!(l, 0x6001 + 7);
    }

    #[test]
    fn exponential_decrease_scales_with_level() {
        let p = Params {
            rate: 0,
            decreasing: true,
            exponential: true,
            phase_negative: false,
            frozen: false,
        };
        let (mut l, mut c) = (0x4000i16, 0u16);
        tick(&mut l, &mut c, p);
        // -8 << 11 = -0x4000, scaled by 0x4000 / 0x8000.
        assert_eq!(l, 0x4000 - 0x2000);
        let (mut l, mut c) = (1i16, 0u16);
        tick(&mut l, &mut c, p);
        assert_eq!(l, 0);
    }

    #[test]
    fn frozen_rate_never_steps() {
        let (mut l, mut c) = (0x1234i16, 0u16);
        run(&mut l, &mut c, lin_inc(0x7F), 1 << 17);
        assert_eq!(l, 0x1234);
        // Shift 31, step field 0 still steps, every 0x8000 cycles.
        let (mut l, mut c) = (0i16, 0u16);
        run(&mut l, &mut c, lin_inc(0x7C), 0x8000);
        assert_eq!(l, 7);
    }

    #[test]
    fn adsr_walks_attack_decay_sustain() {
        // Attack linear shift 0 step +7; decay shift 0; sustain level 0x4000 (n=7);
        // sustain linear decrease, frozen; release linear shift 0.
        let adsr1: u16 = 7;
        let adsr2: u16 = (0x7F << 6) | 0x4000;
        let (mut ph, mut l, mut c) = (Phase::Attack, 0i16, 0u16);
        let mut levels = Vec::new();
        for _ in 0..8 {
            adsr_tick(&mut ph, &mut l, &mut c, adsr1, adsr2);
            levels.push((ph, l));
        }
        assert_eq!(levels[0], (Phase::Attack, 0x3800));
        assert_eq!(levels[1], (Phase::Attack, 0x7000));
        assert_eq!(levels[2], (Phase::Attack, 0x7FFF));
        // Decay: (-0x4000 * 0x7FFF) >> 15 = -0x4000.
        assert_eq!(levels[3], (Phase::Decay, 0x3FFF));
        // At or below the sustain level 0x4000: sustain, frozen.
        assert_eq!(levels[4], (Phase::Sustain, 0x3FFF));
        assert_eq!(levels[7], (Phase::Sustain, 0x3FFF));
    }
}
