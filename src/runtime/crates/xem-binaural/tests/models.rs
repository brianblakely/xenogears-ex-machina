//! The renderer's measured responses are still the ones the calibrated
//! computational listeners heard (docs/runtime.md, Binaural sound mode:
//! Calibrated computational listeners). The models run by hand
//! (`tools/binaural_models/evaluate.m`, a local GPL tool), so their
//! documented results hold only for the responses `rendered_hrirs` measured
//! for that run. This measures every set again as the example does
//! (`rendered_hrirs::sets`) and compares each with the evaluated set's
//! digest: FNV-1a 64 over the set's directions (little-endian float32
//! azimuth and elevation) and then every sample in the files' order
//! ([direction][ear][sample]) rounded to 1e-6 (the responses' unit: the
//! source's signal at its gain before panning; half away from zero) as a
//! little-endian int32. A set that differs asks for the models to be run
//! again on the new responses and the docs' results updated with them.
//! Release builds only (the suite profile, the desktop tier, where it
//! takes 3 to 5 s); a debug build, as CI's `cargo test`, builds it empty.
//!
//!     cargo test --profile suite -p xem-binaural --test models

#![cfg(not(debug_assertions))]

#[allow(dead_code)]
#[path = "../examples/rendered_hrirs.rs"]
mod rendered_hrirs;

/// Each set's digest as the models heard it: the responses
/// `rendered_hrirs` wrote for the run docs/runtime.md documents (Octave
/// 11.3.0, AMT 1.6.0; its acceptance passed, the largest difference 0.32
/// against a bound of 2).
const EVALUATED: [(&str, &str); 6] = [
    ("rendered-measured", "ffc178ae6ca9f71e"),
    ("rendered-grid", "08397ac1adda9a85"),
    ("rendered-music", "dd17f9715683c972"),
    ("rendered-upmix", "35d5552c3a20490b"),
    ("kemar-raw", "b9bd0fb3542368c7"),
    ("stereo-measured", "efd37d4559df1b14"),
];

fn digest(set: &rendered_hrirs::Set) -> String {
    let mut hash = 0xCBF2_9CE4_8422_2325u64;
    let mut add = |bytes: &[u8]| {
        for b in bytes {
            hash = (hash ^ u64::from(*b)).wrapping_mul(0x0000_0100_0000_01B3);
        }
    };
    for &(azimuth, elevation) in &set.directions {
        add(&azimuth.to_le_bytes());
        add(&elevation.to_le_bytes());
    }
    for ears in &set.ears {
        for ear in ears {
            assert_eq!(ear.len(), set.length);
            for &sample in ear {
                add(&((f64::from(sample) * 1e6).round() as i32).to_le_bytes());
            }
        }
    }
    format!("{hash:016x}")
}

#[test]
fn the_renderers_responses_are_the_ones_the_calibrated_models_heard() {
    let measured: Vec<(&str, String)> = rendered_hrirs::sets().iter().map(|set| (set.name, digest(set))).collect();
    eprintln!("the responses' digests: {measured:?}");
    let names: Vec<&str> = measured.iter().map(|(name, _)| *name).collect();
    assert_eq!(names, EVALUATED.map(|(name, _)| name), "the sets the models hear");
    let changed: Vec<&str> = measured.iter().zip(EVALUATED).filter(|((_, now), (_, then))| now != then).map(|((name, _), _)| *name).collect();
    assert!(
        changed.is_empty(),
        "the renderer's responses differ from those the calibrated models heard ({changed:?}): measure them again \
         (`cargo run --release -p xem-binaural --example rendered_hrirs`), run the models on them \
         (`nix develop path:./nix/amt -c octave-cli --no-init-file tools/binaural_models/evaluate.m`), update docs/runtime.md's \
         results (Calibrated computational listeners) from build/binaural/models.json and set EVALUATED to {measured:?}"
    );
}
