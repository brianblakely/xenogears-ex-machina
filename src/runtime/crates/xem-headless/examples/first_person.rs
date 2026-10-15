//! The Binaural mode's first-person listener as the native desktop host has
//! it, for web/tests/first-person.test.mjs, which compares the browser's
//! with it (local only: it needs the user's disc 1 and the game module):
//!
//!     cargo run --release -p xem-headless --example first_person -- --disc DISC --state STATE \
//!         --look YAW,PITCH [--frames N]
//!
//! It restores STATE (a session's state), the game in Stereo under the
//! Binaural mode's defaults, turns first-person inspection on in a desktop's
//! game view (the default graphics, `GameView`) and turns it YAW radians to
//! the right and PITCH up from the heading it begins with (what the view's
//! look angles say), plays N frames (8 by default: the view shows where the
//! character stands from the third, once the port records it) drawn as the
//! desktop host draws them, giving the session the view's listener before
//! each (`GameView::listener_axes`, `Session::set_listener_view`), and
//! prints the session's `binaural` status, a line of JSON.

/// A build without the game module has nothing to run.
#[cfg(not(has_game_module))]
fn main() {
    eprintln!("this build has no game module (python3 tools/game_module.py builds it)");
    std::process::exit(1);
}

#[cfg(has_game_module)]
fn main() {
    use serde_json::json;
    use xem_render::ps1::{GameView, HostClass, ViewConfig};
    let (mut disc, mut state, mut look, mut frames) = (None, None, None, 8u64);
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        let mut value = || args.next().unwrap_or_else(|| panic!("{arg} needs a value"));
        match arg.as_str() {
            "--disc" => disc = Some(std::path::PathBuf::from(value())),
            "--state" => state = Some(std::path::PathBuf::from(value())),
            "--look" => {
                let angles: Vec<f32> = value().split(',').map(|v| v.parse().expect("radians")).collect();
                look = Some((angles[0], angles[1]));
            }
            "--frames" => frames = value().parse().expect("a count"),
            other => panic!("unknown option {other}"),
        }
    }
    let (disc, state, (yaw, pitch)) = (disc.expect("--disc"), state.expect("--state"), look.expect("--look"));
    let (mut session, _) = xem_game::boot(&disc, &xem_game::BootOptions { audio_output: true, ..Default::default() }).expect("boot");
    session.set_sound_mode(1).expect("Stereo");
    session.restore_bytes(&std::fs::read(&state).expect("the state")).expect("restore");
    session.set_binaural(xem_core::binaural::Config { enabled: true, ..Default::default() });
    let gpu = pollster::block_on(xem_render::Gpu::headless()).expect("a wgpu adapter");
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    view.configure(ViewConfig::resolve(&Default::default(), HostClass::Desktop, (640, 448), view.max_scale()));
    view.set_first_person(true);
    view.look_around(yaw, pitch);
    for n in 0..frames {
        session.set_inspecting(true);
        session.set_listener_view(view.listener_axes());
        assert!(session.frame().expect("a frame"), "a native disc never blocks");
        let device = session.runtime.services().gpu.device_mut();
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
    }
    // The view the last frame showed, as the host gives it before the next.
    session.set_listener_view(view.listener_axes());
    let status = session.execute(&json!({"cmd": "binaural"}));
    println!("{}", json!({"look": view.look_angles().map(|(yaw, pitch)| [yaw, pitch]), "frames": session.runtime.frames(), "binaural": status}));
}
