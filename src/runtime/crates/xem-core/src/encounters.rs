//! Where the field's random battles meet platforming (plan.md, Phase 7):
//! the areas whose maps need jumps to get between their entrances, as
//! `python3 -m tools.analysis.platforming` classifies both discs' maps
//! (maps of the same number together; disc 2's bundles are disc 1's).
//! `--rust` prints `PLATFORMING_AREAS` and `--check` proves this file
//! holds exactly that; docs/runtime.md ("Random battles in platforming
//! areas") gives the method, the rule and every area's metrics.
//!
//! An area is the field maps that arm the random draw (a script reaches f7
//! with a nonzero period and count, and player control runs it) whose draws
//! start the same encounters and that doors join. It is platforming-heavy
//! when at least half of its maps need a jump between two of their
//! entrances and those maps' jumps sum to at least three, as measured. Only
//! maps that arm the draw are listed: elsewhere it never runs. Areas whose
//! maps the walkmesh model cannot fully measure, and that could be heavy,
//! are not listed; docs/runtime.md reviews them.

use std::sync::LazyLock;

/// The platforming-heavy areas: the world map's name for the nearest
/// entrance, and the maps that arm the draw, ascending.
pub const PLATFORMING_AREAS: &[(&str, &[u16])] = &[
    ("Blackmoon Forest", &[22]),
    ("Babel Tower", &[407, 409, 412, 413, 414, 415, 416]),
];

/// Every map of `PLATFORMING_AREAS`, ascending (field map numbers, the
/// low 12 bits of `mode_field_map_id`).
pub fn platforming_maps() -> &'static [u16] {
    static MAPS: LazyLock<Vec<u16>> = LazyLock::new(|| {
        let mut maps: Vec<u16> = PLATFORMING_AREAS.iter().flat_map(|(_, maps)| maps.iter().copied()).collect();
        maps.sort_unstable();
        maps
    });
    &MAPS
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn each_area_lists_its_maps_ascending_and_no_map_twice() {
        for (name, maps) in PLATFORMING_AREAS {
            assert!(maps.windows(2).all(|pair| pair[0] < pair[1]), "{name}: ascending, once each");
            assert!(maps.iter().all(|map| *map < 0x1000), "{name}: field map numbers");
        }
        let maps = platforming_maps();
        assert!(maps.windows(2).all(|pair| pair[0] < pair[1]), "no map is in two areas");
        assert_eq!(maps.len(), PLATFORMING_AREAS.iter().map(|(_, maps)| maps.len()).sum::<usize>());
        assert!(maps.binary_search(&414).is_ok() && maps.binary_search(&23).is_err());
    }
}
