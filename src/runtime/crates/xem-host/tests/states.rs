//! A state restored in a host goes on as in a plain session (local only:
//! the user's disc 1 and a game module build; the GPU is whatever wgpu
//! finds, lavapipe in the runtime shell). Lahan (field 1, its villagers
//! walking about) 400 frames after entering it, restored into a `Game`
//! (`Game::restore_bytes`, as Load state does) and played 600 frames as the
//! hosts play it: paced by the wall clock, the game's own settings applied
//! every frame, each host frame presented. In the desktop's window
//! (`View`, as xem-desktop's frame runs it, under the HD-2D profile) the
//! game ends where a plain session restored from the state ends with no
//! host at all: the same RAM and scratchpad hash, and the same whole state
//! (`Session::snapshot_bytes`) byte for byte, as the window asks for no
//! reveals. In a headset's presentation (the Headset class, as `xr_play`
//! runs it: both eyes looking from within the scene for 300 frames, then
//! both eyes seeing the diorama) the game ends where a plain session ends
//! that replays the host's recording of those frames, whose controls (the
//! view the field's characters keep moving in) a restoring host applies,
//! not the state; a plain session without them ends elsewhere. Its hash is
//! the host's, and so is its whole state outside the port's own memory;
//! within it the presentation's bookkeeping for faces revealed all around
//! differs, and two frames on from the same presentation records only dead
//! stack does (xem-headless's tests/render.rs `presentation_only`).
//!
//!     VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD XEM_DISC1=<disc 1> cargo test --release -p xem-host --test states

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::{Value, json};
use std::cell::RefCell;
use std::path::PathBuf;
use std::rc::Rc;
use std::time::{Duration, Instant};
use xem_core::control::Session;
use xem_game::{NativeModule, boot};
use xem_host::{Game, View};
use xem_render::glam::{Mat4, Vec3};
use xem_render::ps1::{DioramaEye, GameView, HeadsetEye, HostClass, ViewConfig, xr_view};
use xem_render::{Gpu, wgpu};
use xem_settings::{Graphics, GraphicsProfile, HeadsetView, MemoryStore, SettingChange, SettingsService};

/// Game frames each host plays from the state.
const FRAMES: u64 = 600;

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn ok(session: &mut Session<NativeModule>, command: Value) -> Value {
    let reply = session.execute(&command);
    assert!(reply.get("error").is_none(), "{command}: {reply}");
    reply
}

/// Lahan 400 frames after entering it.
fn lahan(disc: &std::path::Path) -> Vec<u8> {
    let mut session = boot(disc, &Default::default()).expect("boot").0;
    ok(&mut session, json!({"cmd": "enter_field", "map": 1}));
    ok(&mut session, json!({"cmd": "frames", "count": 400}));
    session.snapshot_bytes().unwrap()
}

/// The state restored into a host's game (booted as hosts boot it, its
/// sound kept for a device), recording from there.
fn host_game(disc: &std::path::Path, state: &[u8]) -> Game {
    let options = xem_game::BootOptions { audio_output: true, ..Default::default() };
    let mut game = Game::new(boot(disc, &options).expect("boot").0);
    game.restore_bytes(state).expect("the state loads");
    game.command(&json!({"cmd": "record"})).unwrap();
    game.set_frame_limit(Some(game.frames_run() + FRAMES));
    game
}

/// Host frames until the game has run its frames: `frame` is one host
/// frame (the game's due frames, then its presentation); the wall clock
/// paces them, as in a host.
fn play(game: &mut Game, mut frame: impl FnMut(&mut Game, f64)) -> u32 {
    let end = game.frames_run() + FRAMES;
    let started = Instant::now();
    let mut presented = 0;
    while game.frames_run() < end {
        frame(game, started.elapsed().as_secs_f64());
        presented += 1;
        assert!(started.elapsed() < Duration::from_secs(300), "the host runs its frames");
        std::thread::sleep(Duration::from_millis(2));
    }
    presented
}

/// The whole state and its parts, to name where two differ (render.rs).
struct Whole {
    bytes: Vec<u8>,
    parts: Vec<(String, u32, Vec<u8>)>,
}

fn whole(session: &mut Session<NativeModule>) -> Whole {
    use xem_core::devices::Device;
    let mut parts = Vec::new();
    for (start, len) in xem_core::snapshot::REGIONS {
        let mut data = vec![0; len as usize];
        session.runtime.memory().read(start, &mut data).unwrap();
        parts.push(("memory".to_string(), start, data));
    }
    let s = session.runtime.services();
    for (name, saved) in [("the GPU", s.gpu.save()), ("the drive", s.cd.save()), ("the MDEC", s.mdec.save()), ("the SPU", s.spu.save()), ("the cards", s.card.save())] {
        parts.push((name.to_string(), 0, saved));
    }
    Whole { bytes: session.snapshot_bytes().unwrap(), parts }
}

/// Where the parts `which` picks differ, if they do.
fn differences(a: &Whole, b: &Whole, which: impl Fn(u32) -> bool) -> Option<String> {
    let mut found = Vec::new();
    for ((name, start, x), (_, _, y)) in a.parts.iter().zip(&b.parts) {
        if x == y || !(name != "memory" || which(*start)) {
            continue;
        }
        let differing: Vec<usize> = (0..x.len().min(y.len())).filter(|&i| x[i] != y[i]).collect();
        match (differing.first(), differing.last()) {
            (Some(first), Some(last)) if name == "memory" => found.push(format!(
                "{} bytes of memory from {:#010x} to {:#010x}", differing.len(), *start as usize + first, *start as usize + last)),
            _ => found.push(format!("{name}'s state ({} bytes against {})", x.len(), y.len())),
        }
    }
    (!found.is_empty()).then(|| found.join("; "))
}

/// The port's own memory (tools/game_module.py PORT_BASE) and shadow stack
/// size (STACK_BYTES), and its presentation records outside every state
/// region (xem/gte.h, sprite_facing.c).
const PORT: u32 = 0x1000_0000;
const STACK_BYTES: u32 = 0x10_0000;
const PRESENTATION: [u32; 5] = [0x7000_0000, 0x7030_0000, 0x7050_0000, 0x70A0_0000, 0x70B0_0000];

/// The module's stack pointer a state holds (runtime.rs `snapshot_content`).
fn stack_pointer(state: &[u8]) -> u32 {
    let mut r = xem_core::snapshot::Reader::new(xem_core::snapshot::checked(state).unwrap());
    r.u64().unwrap();
    r.u32().unwrap();
    r.bytes().unwrap();
    for _ in 0..3 {
        r.u32().unwrap();
    }
    for _ in 0..2 {
        for _ in 0..r.u32().unwrap() {
            r.u32().unwrap();
        }
    }
    r.u32().unwrap()
}

/// The host's whole state `shown` against the plain session's (`plain`
/// holds it): the same outside the port's memory, and within it, two frames
/// on from the same presentation records with no trace, the same but for
/// dead stack below the suspended stack pointer, which nothing reads.
fn presentation_only(plain: &mut Session<NativeModule>, shown: &[u8], what: &str) {
    use xem_core::module::GameModule;
    let unseen = whole(plain);
    let records: Vec<Vec<u8>> = PRESENTATION.iter().map(|&address| {
        let mut data = vec![0; 0x1_0000];
        plain.runtime.memory().read(address, &mut data).unwrap();
        data
    }).collect();
    plain.restore_bytes(shown).unwrap();
    let drawn = whole(plain);
    if let Some(at) = differences(&drawn, &unseen, |start| start != PORT) {
        panic!("{what}: the host's state differs from the plain session's outside the port's memory: {at}");
    }
    let mut on = Vec::new();
    for state in [&drawn.bytes, &unseen.bytes] {
        plain.restore_bytes(state).unwrap();
        for (&address, data) in PRESENTATION.iter().zip(&records) {
            plain.runtime.memory().write(address, data).unwrap();
        }
        plain.runtime.services().gpu.device_mut().set_trace(false);
        for _ in 0..2 {
            assert!(plain.frame().expect("frame"));
        }
        on.push(whole(plain));
    }
    if on[0].bytes == on[1].bytes {
        eprintln!("{what}: two frames on, the same whole state");
        return;
    }
    let (sp, top) = (stack_pointer(&on[0].bytes), NativeModule::new().stack_pointer());
    assert_eq!(stack_pointer(&on[1].bytes), sp, "{what}: two frames on, the same stack pointer");
    let dead = top - STACK_BYTES..sp;
    if let Some(at) = differences(&on[0], &on[1], |start| start != PORT) {
        panic!("{what}: two frames on, outside the port's memory: {at}");
    }
    fn port(w: &Whole) -> &[u8] {
        &w.parts.iter().find(|(name, start, _)| name == "memory" && *start == PORT).unwrap().2
    }
    let (a, b) = (port(&on[0]), port(&on[1]));
    let differing: Vec<usize> = (0..a.len()).filter(|&i| a[i] != b[i]).collect();
    let outside = differing.iter().find(|&&i| !dead.contains(&(PORT + i as u32)));
    assert!(outside.is_none(), "{what}: two frames on, port memory at {:#010x} differs outside the dead stack {dead:#x?}", PORT + *outside.unwrap() as u32);
    // The plain session holds the second: with the first's dead stack, the same.
    let from = (dead.start - PORT) as usize..(dead.end - PORT) as usize;
    plain.runtime.memory().write(dead.start, &a[from]).unwrap();
    assert!(plain.snapshot_bytes().unwrap() == on[0].bytes, "{what}: two frames on, the states differ beside the dead stack");
    eprintln!("{what}: two frames on, {} bytes of dead stack alone differ, {:#010x} to {:#010x} (the suspended stack pointer {sp:#010x})",
        differing.len(), PORT as usize + differing[0], PORT as usize + differing[differing.len() - 1]);
}

/// A plain session (no host) restored from `state`, replaying `log` (none:
/// the original's own controls) for the host's frames.
fn plain(disc: &std::path::Path, state: &[u8], log: Option<&Value>) -> Session<NativeModule> {
    let mut session = boot(disc, &Default::default()).expect("boot").0;
    session.restore_bytes(state).unwrap();
    if let Some(log) = log {
        let mut replay = log.clone();
        replay["cmd"] = json!("replay");
        ok(&mut session, replay);
    }
    ok(&mut session, json!({"cmd": "frames", "count": FRAMES}));
    session
}

#[test]
fn the_desktops_window_plays_a_restored_state_as_a_plain_session_does() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let state = lahan(&disc);
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let graphics = Graphics { profile: GraphicsProfile::Hd2d, ..settings.borrow().settings().graphics };
    settings.borrow_mut().apply(SettingChange::Graphics(graphics)).unwrap();
    let (width, height) = (960u32, 540u32);
    let mut view = View::new(&gpu, wgpu::TextureFormat::Rgba8UnormSrgb, (width, height), 1.0, settings.clone(), false).unwrap();
    let target = gpu.device.create_texture(&wgpu::TextureDescriptor {
        label: Some("window"),
        size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
        mip_level_count: 1, sample_count: 1, dimension: wgpu::TextureDimension::D2,
        format: wgpu::TextureFormat::Rgba8UnormSrgb,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        view_formats: &[],
    });
    let output = target.create_view(&Default::default());
    let mut game = host_game(&disc, &state);
    // xem-desktop's frame (Host::frame) without the window's events: touch
    // play's taps, the pad's context, the player's buttons (none held),
    // the game's due frames with the settings, then the presentation.
    let presented = play(&mut game, |game, time| {
        let current = view.settings().borrow().settings().clone();
        view.assist(game);
        let (context, targets) = (game.pad_context(), game.targets());
        let buttons = view.pad_buttons(context, targets);
        game.want_attack_page(view.attack_pointed());
        game.want_inspection(view.first_person());
        let ran = game.update(buttons, &current, || true).unwrap();
        view.frames_ran(ran);
        game.follow_sound_mode(view.settings());
        view.set_game_frame(&gpu, game);
        view.render(&gpu, &output, time as f32).unwrap();
    });
    let log = game.command(&json!({"cmd": "stop_recording"})).unwrap();
    eprintln!("the window: {presented} host frames for {FRAMES} game frames; recorded controls {}", log["controls"]);
    assert!(presented > 100, "the window presents as it plays ({presented} host frames)");
    let views: Vec<&Value> = log["controls"].as_array().unwrap().iter().map(|c| &c[1]).filter(|c| c["cmd"] == "actor_view").collect();
    assert!(views.is_empty(), "the window under HD-2D keeps the original's view: {views:?}");
    let shown = game.snapshot_bytes().unwrap();
    let hash = game.memory_hash();
    // The original's own: a plain session with none of the host's controls.
    let mut original = plain(&disc, &state, None);
    assert_eq!(format!("{:016x}", original.memory_hash().unwrap()), hash, "the window plays the state as the original does");
    // The window asked for no reveals (no wider framing, pull-back or
    // inspection): the whole state is the plain session's.
    assert!(original.snapshot_bytes().unwrap() == shown, "the window's whole state is the plain session's, byte for byte");
}

#[test]
fn a_headsets_eyes_and_its_diorama_play_a_restored_state_as_a_plain_session_given_its_view() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let state = lahan(&disc);
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = RefCell::new(SettingsService::with_defaults(MemoryStore::default()));
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let mut game = host_game(&disc, &state);
    let start = game.frames_run();
    // The eyes 64 mm apart, each a 90-degree view: from within the scene
    // (first-person inspection, `HeadsetEye`) and at the diorama's table.
    let eye = (512u32, 512u32);
    let eyes = [-0.032f32, 0.032].map(|x| HeadsetEye { orientation: [0.0, 0.0, 0.0, 1.0], position: [x, 0.0, 0.0], fov: [-1.0, 1.0, 1.0, -1.0] });
    let mut within = 0;
    let mut dioramas = 0;
    view.set_first_person(true);
    // xr_play's headset frame without OpenXR: the game's due frames (no
    // buttons held; inspection keeps the character where it stands), then
    // the presentation configured for the Headset class, the view the
    // field's characters keep moving in, the trace taken and both eyes drawn.
    let presented = play(&mut game, |game, now| {
        let half = game.frames_run() >= start + FRAMES / 2;
        if half && view.first_person() {
            view.set_first_person(false);
            let graphics = settings.borrow().settings().graphics;
            let diorama = Graphics { xr: xem_settings::XrScreen { view: HeadsetView::Diorama, ..graphics.xr }, profile: GraphicsProfile::Hd2d, ..graphics };
            settings.borrow_mut().apply(SettingChange::Graphics(diorama)).unwrap();
        }
        let current = settings.borrow().settings().clone();
        let context = game.pad_context();
        let inspecting = view.first_person();
        game.want_inspection(inspecting);
        let buttons = xem_settings::look::game_buttons(0, inspecting, context.field, &context.assignment);
        game.update(buttons, &current, || true).unwrap();
        game.follow_sound_mode(&settings);
        view.configure(ViewConfig::resolve(&current.graphics, HostClass::Headset, eye, view.max_scale()));
        view.set_xr_view(xr_view(&current.graphics));
        view.set_room(false);
        let restart = view.take_restart();
        game.want_actor_view(if current.graphics.actors_in_view { view.actor_view() } else { 0 });
        game.set_listener_view(view.listener_axes());
        let (trace, display, scene) = game.take_frame(restart, view.reveals());
        view.push(trace);
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        if half {
            let head = Vec3::new(0.0, 0.0, 0.0);
            let diorama = [-0.032f32, 0.032].map(|x| DioramaEye {
                view: Mat4::look_at_rh(head + Vec3::X * x, Vec3::new(0.0, -0.45, -0.6), Vec3::Y),
                proj: Mat4::perspective_rh(1.6, 1.0, 0.05, 20.0),
                size: eye,
            });
            let shown = view.present_diorama(&mut encoder, &display, &scene, now, &diorama, Mat4::IDENTITY);
            dioramas += u32::from(shown.is_some_and(|p| p.screen < 1.0 && p.eyes.len() == 2));
        } else {
            let shown = view.present_headset(&mut encoder, &display, &scene, now, &eyes);
            within += u32::from(shown.is_some_and(|p| p.eyes.len() == 2));
        }
        gpu.queue.submit([encoder.finish()]);
    });
    let log = game.command(&json!({"cmd": "stop_recording"})).unwrap();
    let views: Vec<&Value> = log["controls"].as_array().unwrap().iter().map(|c| &c[1]).filter(|c| c["cmd"] == "actor_view").collect();
    eprintln!("the headset: {presented} host frames, {within} with both eyes within the scene, {dioramas} showing both eyes' diorama; recorded views {views:?}");
    assert!(within > 30 && dioramas > 30, "both eyes drew within the scene ({within}) and the diorama ({dioramas})");
    assert!(views.iter().any(|v| v["view"] == 1000 | 1024 << 16), "the headset's views let the field's characters move anywhere: {views:?}");
    let shown = game.snapshot_bytes().unwrap();
    let hash = game.memory_hash();
    // A plain session given the host's recorded controls ends there.
    let mut replayed = plain(&disc, &state, Some(&log));
    assert_eq!(format!("{:016x}", replayed.memory_hash().unwrap()), hash, "the headset plays the state as a plain session given its view does");
    presentation_only(&mut replayed, &shown, "the headset");
    // Without them, the original's view stops the villagers it does not show.
    let mut original = plain(&disc, &state, None);
    assert_ne!(format!("{:016x}", original.memory_hash().unwrap()), hash, "the view is the host's control, not the state's");
}
