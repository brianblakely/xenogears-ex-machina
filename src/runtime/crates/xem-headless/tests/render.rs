//! Rendering never changes the game (local only: it needs the user's disc 1
//! and a game module build; the GPU is whatever wgpu finds, lavapipe in the
//! runtime shell). From the same snapshot on field 12, the same frames end at
//! the same RAM hash whether nothing renders, the renderer draws every frame
//! under switching profiles (HD-2D with wider framing and interpolation,
//! wider framing with the camera pulled back, which has the port record the
//! scenery the game culls, PS1, Custom), a spectator view attaches, changes
//! and detaches midway, or the view looks around in first person (the port
//! then records every face the game culls and where its character stands).
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test render

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::json;
use std::path::PathBuf;
use xem_core::control::Session;
use xem_game::{NativeModule, boot};
use xem_render::Gpu;
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_settings::{EffectLevels, Fidelity, Graphics, GraphicsProfile};

const FRAMES: u64 = 240;

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn hash(session: &mut Session<NativeModule>) -> String {
    format!("{:016x}", session.memory_hash().unwrap())
}

/// Runs `FRAMES` frames; before frame `n`, `view(n)` says which view (if
/// any) draws it and with which settings, and `look(n)` how far it turns
/// looking around in first person (`None`: it does not). Returns the RAM
/// hash and the frames shown from the field character's eyes.
fn run(
    session: &mut Session<NativeModule>,
    gpu: &Gpu,
    mut view: impl FnMut(u64) -> Option<(usize, Graphics)>,
    look: impl Fn(u64) -> Option<f32>,
) -> (String, u64) {
    let mut views: Vec<GameView> = (0..2).map(|_| GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter))).collect();
    let mut looked = 0;
    for n in 0..FRAMES {
        assert!(session.frame().expect("frame"));
        let device = session.runtime.services().gpu.device_mut();
        match view(n) {
            Some((index, graphics)) => {
                let view = &mut views[index];
                match look(n) {
                    Some(turn) => {
                        if !view.first_person() {
                            view.set_first_person(true);
                        }
                        view.look_around(turn, 0.0);
                    }
                    None => view.set_first_person(false),
                }
                view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (640, 448), view.max_scale()));
                if view.take_restart() || !device.trace_enabled() {
                    device.set_trace(true);
                }
                device.set_reveals(view.reveals());
                view.push(device.take_trace());
                let display = device.display();
                let scene = session.scene();
                let mut encoder = gpu.device.create_command_encoder(&Default::default());
                let _ = view.present(&mut encoder, &display, &scene, n as f64 / 60.0);
                gpu.queue.submit([encoder.finish()]);
                looked += u64::from(view.first_person() && view.frame().eye().is_some());
            }
            None => {
                if device.trace_enabled() {
                    device.set_trace(false);
                }
            }
        }
    }
    (hash(session), looked)
}

#[test]
fn rendering_skipped_offscreen_or_as_a_spectator_leaves_the_game_unchanged() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    assert!(session.execute(&json!({"cmd": "enter_field", "map": 12})).get("error").is_none());
    assert!(session.execute(&json!({"cmd": "frames", "count": 400})).get("error").is_none());
    let start = session.snapshot_bytes().unwrap();
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");

    let (skipped, _) = run(&mut session, &gpu, |_| None, |_| None);

    let profiles = [
        Graphics { profile: GraphicsProfile::Hd2d, widescreen: true, interpolation: true, ..Default::default() },
        Graphics { widescreen: true, zoom_out: 400, ..Default::default() },
        Graphics { profile: GraphicsProfile::Ps1, ..Default::default() },
        Graphics {
            profile: GraphicsProfile::Custom,
            custom_fidelity: Fidelity { dithering: true, painter_ordering: false, ..Fidelity::MODERN },
            custom_effects: EffectLevels { bloom: 100, depth_of_field: 100, ..EffectLevels::OFF },
            ..Default::default()
        },
        Graphics::default(),
    ];
    session.restore_bytes(&start).unwrap();
    let (every_frame, _) = run(&mut session, &gpu, |n| Some((0, profiles[(n / 30) as usize % profiles.len()])), |_| None);
    assert_eq!(every_frame, skipped, "drawing every frame under switching profiles changes nothing");

    session.restore_bytes(&start).unwrap();
    let (spectator, _) = run(&mut session, &gpu, |n| match n {
        // A spectator attaches, changes its look, and detaches.
        60..120 => Some((1, profiles[0])),
        120..180 => Some((1, profiles[1])),
        _ => None,
    }, |_| None);
    assert_eq!(spectator, skipped, "a spectator attaching and detaching changes nothing");

    // Looking around in first person midway, under two profiles, turning.
    session.restore_bytes(&start).unwrap();
    let (looking, looked) = run(&mut session, &gpu, |n| Some((0, profiles[(n / 60) as usize % 2])), |n| (40..200).contains(&n).then_some(0.05));
    assert!(looked > 100, "the field's character's eyes show ({looked} frames)");
    assert_eq!(looking, skipped, "looking around in first person changes nothing");
}
