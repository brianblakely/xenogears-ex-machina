//! The sound driver's Stereo pan law (`sound_stage_seq_voices`;
//! analysis/formats/sound-modes.md) and its inverse: what a voice's
//! volumes say of the gain the driver resolved for it before panning, and
//! of its pan.
//!
//! The driver gives a voice of volume `v` and pan `p` (0 left, 0x4000
//! centre, 0x7F00 right) the registers `gL(p) * v >> 15` and `gR(p) * v >> 15`,
//! two linear ramps meeting at 0x5A00 at the centre, 0x7F00 and 0 at the
//! edges; the SPU's current volume is twice the register. Mono gives both
//! sides the centre gain, which reads as the centre here, at the same gain.

/// The law's gains (left, right) of 0x8000 at `pan`.
pub fn gains(pan: i32) -> (i32, i32) {
    let pan = pan.clamp(0, 0x7F00);
    if pan < 0x4000 {
        (0x7F00 - ((pan * 0x2500) >> 14), (pan * 0x5A00) >> 14)
    } else {
        let mirrored = 0x8000 - pan;
        ((mirrored * 0x5A00) >> 14, 0x7F00 - ((mirrored * 0x2500) >> 14))
    }
}

/// A voice before the pan law.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Unpanned {
    /// The voice's gain before panning: its output on each side at the
    /// centre is `sample * gain * 0x5A00 / 0x8000`.
    pub gain: f32,
    /// Its pan, 0 (left) to 0x7F00 (right), 0x4000 the centre.
    pub pan: f32,
}

const EDGE: f32 = 32512.0;
const CENTRE: f32 = 23040.0;
const RAMP: f32 = 9472.0;

/// The gain and pan of a voice mixed with the current `volume` (left,
/// right); `None` when a side is negative, which the pan law never gives
/// (the voice is not the driver's, or the game is in another mode).
pub fn unpan(volume: [i16; 2]) -> Option<Unpanned> {
    let [left, right] = volume.map(f32::from);
    if left < 0.0 || right < 0.0 {
        return None;
    }
    if left == 0.0 && right == 0.0 {
        return Some(Unpanned { gain: 0.0, pan: 16384.0 });
    }
    let (near, far, right_side) = if left >= right { (left, right, false) } else { (right, left, true) };
    // far / near = (CENTRE x) / (EDGE - RAMP x), x the pan's share of the way
    // from the near edge to the centre.
    let x = (EDGE * far / (CENTRE * near + RAMP * far)).clamp(0.0, 1.0);
    let gain = near / (EDGE - RAMP * x);
    let pan = if right_side { 32768.0 - x * 16384.0 } else { x * 16384.0 };
    Some(Unpanned { gain, pan })
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The registers the driver writes for volume `v` at `pan`, as current
    /// volumes.
    fn current(v: i32, pan: i32) -> [i16; 2] {
        let (l, r) = gains(pan);
        [(((l * v) >> 15) << 1) as i16, (((r * v) >> 15) << 1) as i16]
    }

    #[test]
    fn the_law_meets_at_the_centre() {
        assert_eq!(gains(0x4000), (0x5A00, 0x5A00));
        assert_eq!(gains(0), (0x7F00, 0));
        assert_eq!(gains(0x7F00), (0x168, 0x7E6C));
    }

    #[test]
    fn unpanning_recovers_the_volume_and_pan() {
        for v in [0x400, 0x1000, 0x3F7F] {
            for pan in [0, 0x1000, 0x3000, 0x4000, 0x5000, 0x7000, 0x7F00] {
                let u = unpan(current(v, pan)).unwrap();
                let gain = v as f32 / 16384.0;
                assert!((u.gain / gain - 1.0).abs() < 0.01, "v {v:#x} pan {pan:#x}: {} for {gain}", u.gain);
                assert!((u.pan - pan as f32).abs() < 256.0, "v {v:#x} pan {pan:#x}: {}", u.pan);
            }
        }
        // The centre gives each side 0x5A00 of the gain.
        let u = unpan(current(0x2000, 0x4000)).unwrap();
        assert!((u.pan - 16384.0).abs() < 1.0);
        assert!((u.gain * 23040.0 / 32768.0 * 32768.0 - f32::from(current(0x2000, 0x4000)[0])).abs() < 2.0);
        assert_eq!(unpan([0, 0]), Some(Unpanned { gain: 0.0, pan: 16384.0 }));
        assert_eq!(unpan([-2, 4]), None);
    }
}
