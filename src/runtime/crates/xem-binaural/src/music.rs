//! Arranging a sequence's parts on a musical stage around the listener.
//!
//! The rules read only what a part plays (its [`PartProfile`], from the
//! sequence's own data): how many notes, how high, how long, whether they
//! are drum-kit or noise notes, the part's pan before it starts. Instrument
//! numbers are not read as labels. The roles, in order:
//!
//! - **rhythm**: half its notes or more are drum-kit notes or noise;
//! - **bass**: the lowest melodic part, an octave or more (9 semitones)
//!   below the melodic parts' median, and any part within 3 semitones of
//!   it as low;
//! - **pad**: a sustained part, its notes a half note (96 ticks) or longer
//!   on average;
//! - **lead**: of the rest, at or above the median, the busiest (notes
//!   times their mean volume), with 16 notes or more;
//! - **support**: the other melodic parts;
//! - **uncertain**: fewer than 4 notes, or data the profile could not read
//!   to its end.
//!
//! At full immersion the lead, bass and rhythm stay where the music put
//! them ahead: their own pan within the stereo arc (30 degrees either side
//! of ahead), steady. Supporting parts and pads the music panned a quarter
//! of the way or more to a side go to that side (supports from 70 to 130
//! degrees, pads at 70 to 110 and 25 up); those it left near the centre
//! keep the mix's balance, supports at the rear centre (180, then 160 and
//! 140 either side) and pads above (40 up, ahead then behind). An uncertain
//! part keeps the stereo arc, which is where every part is at immersion 0.
//! A part's pan movement in the music turns its place (`spread` degrees per
//! 0x4000).

/// What a part plays, from its sequence data.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct PartProfile {
    pub notes: u32,
    /// Notes played as drum-kit entries or as noise.
    pub percussive_notes: u32,
    /// The melodic notes' pitches summed (semitones, instrument offsets in).
    pub pitch_sum: f64,
    /// Ticks the notes take, summed.
    pub ticks: u64,
    /// The notes' volumes (0 to 127) summed.
    pub velocity_sum: u64,
    /// The part's pan before its first note (0 left, 0x4000 centre, 0x7F00
    /// right), when it sets one.
    pub home_pan: Option<u16>,
    /// The profile read the part to its end or loop.
    pub complete: bool,
}

impl PartProfile {
    fn melodic_notes(&self) -> u32 {
        self.notes - self.percussive_notes
    }

    fn mean_pitch(&self) -> Option<f64> {
        (self.melodic_notes() > 0).then(|| self.pitch_sum / f64::from(self.melodic_notes()))
    }

    fn mean_ticks(&self) -> f64 {
        if self.notes == 0 { 0.0 } else { self.ticks as f64 / f64::from(self.notes) }
    }

    fn mean_velocity(&self) -> f64 {
        if self.notes == 0 { 0.0 } else { self.velocity_sum as f64 / f64::from(self.notes) }
    }

    fn pan(&self) -> f32 {
        f32::from(self.home_pan.unwrap_or(0x4000))
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Role {
    Lead,
    Bass,
    Rhythm,
    Pad,
    Support,
    Uncertain,
}

impl Role {
    pub fn name(self) -> &'static str {
        match self {
            Role::Lead => "lead",
            Role::Bass => "bass",
            Role::Rhythm => "rhythm",
            Role::Pad => "pad",
            Role::Support => "support",
            Role::Uncertain => "uncertain",
        }
    }
}

/// Where a part plays on the stage.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Placement {
    pub role: Role,
    /// At full immersion, degrees: azimuth clockwise from ahead, elevation.
    pub azimuth: f32,
    pub elevation: f32,
    /// The part's pan its place is for, and how far its pan movement turns
    /// it (degrees per 0x4000) at full immersion.
    pub home_pan: f32,
    pub spread: f32,
}

/// The stereo arc: the original pan as a direction ahead, within 30
/// degrees either side.
const ARC: f32 = 30.0;
/// A pan this far from the centre (a quarter of the way to an edge) puts a
/// supporting part or pad on that side.
const SIDE_PAN: f32 = 4096.0;

fn arc(pan: f32) -> f32 {
    (pan - 16384.0) / 16384.0 * ARC
}

impl Placement {
    /// The place at `immersion` (0: the stereo arc; 1: the arrangement):
    /// (azimuth, elevation, spread), degrees.
    pub fn at(&self, immersion: f32) -> (f32, f32, f32) {
        let i = immersion.clamp(0.0, 1.0);
        let lerp = |a: f32, b: f32| a + (b - a) * i;
        (lerp(arc(self.home_pan), self.azimuth), lerp(0.0, self.elevation), lerp(ARC, self.spread))
    }
}

/// The parts' roles, by the rules above; `overrides` (part, role) replace
/// a rule's choice.
pub fn roles(parts: &[PartProfile], overrides: &[(usize, Role)]) -> Vec<Role> {
    let mut roles = vec![Role::Uncertain; parts.len()];
    let mut melodic = Vec::new();
    for (i, part) in parts.iter().enumerate() {
        if part.notes < 4 || !part.complete {
            continue;
        }
        if 2 * part.percussive_notes >= part.notes {
            roles[i] = Role::Rhythm;
        } else if let Some(pitch) = part.mean_pitch() {
            melodic.push((i, pitch));
        }
    }
    if !melodic.is_empty() {
        let mut pitches: Vec<f64> = melodic.iter().map(|(_, p)| *p).collect();
        pitches.sort_by(f64::total_cmp);
        let median = pitches[pitches.len() / 2];
        let lowest = pitches[0];
        for &(i, pitch) in &melodic {
            roles[i] = if melodic.len() > 1 && lowest <= median - 9.0 && pitch <= lowest + 3.0 {
                Role::Bass
            } else if parts[i].mean_ticks() >= 96.0 {
                Role::Pad
            } else {
                Role::Support
            };
        }
        let lead = melodic
            .iter()
            .filter(|(i, pitch)| roles[*i] == Role::Support && *pitch >= median && parts[*i].notes >= 16)
            .max_by(|(a, _), (b, _)| {
                let score = |i: usize| f64::from(parts[i].notes) * parts[i].mean_velocity();
                score(*a).total_cmp(&score(*b)).then(b.cmp(a))
            });
        if let Some(&(i, _)) = lead {
            roles[i] = Role::Lead;
        }
    }
    for &(part, role) in overrides {
        if let Some(slot) = roles.get_mut(part) {
            *slot = role;
        }
    }
    roles
}

/// Arranges the parts: their roles ([`roles`]) and places.
pub fn arrange(parts: &[PartProfile], overrides: &[(usize, Role)]) -> Vec<Placement> {
    let roles = roles(parts, overrides);
    // A supporting part or pad the music panned to a side goes to that side,
    // taking the next free angle there; one it left near the centre goes to
    // the rear centre or above, where it keeps the mix's balance.
    let mut next = [0usize; 3];
    roles
        .iter()
        .enumerate()
        .map(|(i, &role)| {
            let pan = parts[i].pan();
            let lean = if pan > 16384.0 + SIDE_PAN { 1 } else if pan < 16384.0 - SIDE_PAN { -1 } else { 0 };
            let mut take = |angles: &[f32], centre: &[f32]| -> f32 {
                let slot = &mut next[(lean + 1) as usize];
                let angle = if lean == 0 { centre[*slot % centre.len()] } else { angles[*slot % angles.len()] * lean as f32 };
                *slot += 1;
                angle
            };
            let (azimuth, elevation, spread) = match role {
                Role::Lead | Role::Bass | Role::Rhythm | Role::Uncertain => (arc(pan), 0.0, ARC),
                Role::Pad if lean == 0 => (take(&[], &[0.0, 180.0]), 40.0, 20.0),
                Role::Pad => (take(&[90.0, 110.0, 70.0], &[]), 25.0, 20.0),
                Role::Support => (take(&[70.0, 100.0, 130.0], &[180.0, 160.0, -160.0, 140.0, -140.0]), 5.0, 20.0),
            };
            Placement { role, azimuth, elevation, home_pan: pan, spread }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn part(notes: u32, pitch: f64, ticks: u64, pan: Option<u16>) -> PartProfile {
        PartProfile {
            notes,
            percussive_notes: 0,
            pitch_sum: pitch * f64::from(notes),
            ticks: ticks * u64::from(notes),
            velocity_sum: 100 * u64::from(notes),
            home_pan: pan,
            complete: true,
        }
    }

    #[test]
    fn the_rules_pick_each_role() {
        let mut drums = part(200, 0.0, 24, Some(0x4000));
        drums.percussive_notes = 180;
        let parts = vec![
            part(300, 72.0, 24, Some(0x4000)), // busy and high: lead
            part(150, 40.0, 48, Some(0x4000)), // low: bass
            drums,
            part(40, 60.0, 192, Some(0x2000)), // long notes: pad, left
            part(120, 64.0, 24, Some(0x6000)), // support, right
            part(100, 66.0, 24, Some(0x4000)), // support, centred
            part(2, 60.0, 24, None),           // too little to tell
        ];
        let placed = arrange(&parts, &[]);
        let roles: Vec<Role> = placed.iter().map(|p| p.role).collect();
        assert_eq!(roles, [Role::Lead, Role::Bass, Role::Rhythm, Role::Pad, Role::Support, Role::Support, Role::Uncertain]);
        assert_eq!(placed[0].azimuth, 0.0, "the lead ahead, where its pan put it");
        assert!(placed[3].azimuth < -45.0 && placed[3].elevation > 0.0, "the pad up on its own side");
        assert!(placed[4].azimuth > 45.0, "a support on its side");
        assert_eq!(placed[5].azimuth, 180.0, "a centred support behind, keeping the balance");
        // At immersion 0 everything is the stereo arc.
        for p in &placed {
            let (azimuth, elevation, _) = p.at(0.0);
            assert!(azimuth.abs() <= 30.0 && elevation == 0.0);
        }
    }

    #[test]
    fn the_same_parts_arrange_the_same_way_and_overrides_win() {
        let parts = vec![part(50, 60.0, 24, None), part(50, 62.0, 24, None), part(50, 64.0, 24, None)];
        assert_eq!(arrange(&parts, &[]), arrange(&parts, &[]));
        assert_eq!(roles(&parts, &[(1, Role::Pad)])[1], Role::Pad);
        // Without an octave between them nothing is a bass.
        assert!(!roles(&parts, &[]).contains(&Role::Bass));
    }

    #[test]
    fn incomplete_data_is_uncertain_and_stays_in_the_arc() {
        let mut p = part(400, 60.0, 24, Some(0x7F00));
        p.complete = false;
        let placed = arrange(&[p], &[]);
        assert_eq!(placed[0].role, Role::Uncertain);
        assert!((placed[0].at(1.0).0 - 29.53).abs() < 0.1);
    }
}
