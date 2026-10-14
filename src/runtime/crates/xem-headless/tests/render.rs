//! Rendering never changes the game (local only: it needs the user's disc 1
//! and a game module build; the GPU is whatever wgpu finds, lavapipe in the
//! runtime shell). From the same snapshot on field 12, the same frames end at
//! the same RAM hash whether nothing renders, the renderer draws every frame
//! under switching profiles (HD-2D with wider framing and interpolation,
//! wider framing with the camera pulled back, which has the port record the
//! scenery the game culls, PS1, Custom), a spectator view attaches, changes
//! and detaches midway, or the view looks around in first person (the port
//! then records every face the game culls and where its character stands),
//! or a headset's diorama shows it from a moving head at changing scales
//! (the port records every face and each frame's camera).
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

#[test]
fn battle_damage_totals_are_interface_and_do_not_change_scene_focus() {
    use xem_gpu::{Class, Primitive, TraceOp};
    use xem_render::ps1::{DisplayArea, SceneAnalysis};

    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    for gears in [false, true] {
        let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
        for cmd in [
            json!({"cmd":"frames", "count":60}),
            json!({"cmd":"write", "path":"mode_battle_standalone", "value":1}),
            json!({"cmd":"boot_mode", "mode":2}),
            json!({"cmd":"frames", "count":900}),
        ] { assert!(session.execute(&cmd).get("error").is_none()); }
        if gears {
            for button in ["down", "down", "cross", "right", "cross", "right", "cross"] {
                session.execute(&json!({"cmd":"press", "buttons":[button], "frames":1}));
                session.execute(&json!({"cmd":"frames", "count":20}));
            }
        }
        session.execute(&json!({"cmd":"press", "buttons":["start"], "frames":1}));
        session.execute(&json!({"cmd":"frames", "count":1520}));
        session.runtime.services().gpu.device_mut().set_trace(true);
        let mut checked = 0;
        for frame in 0..420 {
            session.execute(&json!({"cmd":"pad", "buttons":if frame % 10 < 2 { vec!["cross"] } else { vec![] }}));
            assert!(session.frame().unwrap());
            let trace = session.runtime.services().gpu.device_mut().take_trace();
            let mut total = [0u8; 0x68 + 24 * 5];
            session.runtime.memory().read(0x800D_30EC, &mut total).unwrap();
            let count = u32::from_le_bytes(total[0x64..0x68].try_into().unwrap()) as usize;
            if count == 0 || count > 5 { continue; }
            // Original TotalPopup's SpriteParts identify the numeric glyphs
            // by their texture page, palette and first texel. They are drawn
            // by battle_popup_draw_glyph, also used for floating damage.
            let is_glyph = |p: &Primitive| p.textured && total[0x68..0x68 + count * 24].chunks_exact(24).any(|g| {
                let half = |i| u16::from_le_bytes([g[i], g[i + 1]]);
                p.clut == half(12) && p.environment.draw_mode & 0x1FF == u32::from(half(10)) & 0x1FF &&
                    p.vertices[0].u == i16::from(g[4]) && p.vertices[0].v == i16::from(g[5])
            });
            let mut without = Vec::new();
            for op in &trace.ops {
                if let TraceOp::Draw(p) = op && is_glyph(p) {
                    assert_eq!(p.class, Class::Flat, "gears={gears}, frame={frame}: damage glyph is interface");
                    assert_eq!(p.depth(), 0.0);
                    checked += 1;
                } else {
                    without.push(op.clone());
                }
            }
            let area = DisplayArea { x: 0, y: 0, width: 320, height: 216 };
            let all = SceneAnalysis::of(&trace.ops, area);
            let scene = SceneAnalysis::of(&without, area);
            assert_eq!(all.depths, scene.depths, "damage glyphs cannot change the scene's depth range");
            assert_eq!(all.focus, scene.focus, "damage glyphs cannot move the focus");
        }
        assert!(checked > 50, "gears={gears}: observed only {checked} damage glyphs");
        eprintln!("gears={gears}: checked {checked} original damage glyphs");
    }
}

#[test]
fn worldmap_cloud_corners_keep_their_original_quad() {
    use xem_gpu::{Class, Shape, TraceOp};

    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    session.execute(&json!({"cmd":"boot_mode", "mode":3}));
    session.execute(&json!({"cmd":"frames", "count":900}));
    session.runtime.services().gpu.device_mut().set_trace(true);
    let mut checked = 0;
    for frame in 0..600 {
        let buttons = if frame < 2 { vec!["up", "cross"] } else { vec!["up"] };
        session.execute(&json!({"cmd":"pad", "buttons":buttons}));
        assert!(session.frame().unwrap());
        let trace = session.runtime.services().gpu.device_mut().take_trace();
        for op in trace.ops {
            let TraceOp::Draw(p) = op else { continue };
            // worldmap_clouds_alloc_quads: the cloud page and palette.
            if !p.textured || p.shape != Shape::Quad || p.clut != (0x1FE << 6 | 0x130 >> 4) ||
                p.environment.draw_mode & 0x1FF != 0x3F { continue; }
            assert_eq!(p.class, Class::World);
            let v = p.vertices.map(|v| v.view.expect("each cloud corner has its own projection"));
            // All three original cloud-detail paths project a rectangular
            // planar quad. Fixed-point matrix rounding can lose one unit at
            // each corner; borrowing a corner from another layer cannot.
            for axis in 0..3 {
                let error = (v[0][axis] + v[3][axis] - v[1][axis] - v[2][axis]).abs();
                assert!(error <= 2.0, "frame={frame}, axis={axis}: cloud corner escaped its quad ({v:?})");
            }
            checked += 1;
        }
    }
    assert!(checked > 100, "the flight route should draw clouds ({checked} seen)");
    eprintln!("checked {checked} original worldmap cloud quads");
}

/// The field menu's frames replay its own buffers' (in both, its windows,
/// commands and portraits), and closing it, the field fades back in from
/// the screen it saved and holds it while its sprites load: the wider
/// picture presented before the menu, sides included, not the original's
/// own picture of it (the screen taken twice more meanwhile, the menu's
/// dimmed copy and its last frame, keeps the field's; its place given back
/// to textures leaves the frame shown as it is).
#[test]
fn the_field_menu_closes_back_to_the_wider_picture() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    session.execute(&json!({"cmd":"enter_field", "map": 22}));
    session.execute(&json!({"cmd":"frames", "count": 340}));
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let graphics = Graphics { widescreen: true, zoom_out: 125, ..Graphics::default() };
    let (mut checked, mut menu_frames) = (0, 0);
    for n in 0..480u64 {
        // The menu opens, then closes from its commands.
        let buttons = match n { 60 | 61 => vec!["square"], 260 | 261 => vec!["circle"], _ => vec![] };
        session.execute(&json!({"cmd":"pad", "buttons": buttons}));
        assert!(session.frame().unwrap());
        view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (1280, 720), view.max_scale()));
        let device = session.runtime.services().gpu.device_mut();
        if view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(view.reveals());
        view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let (width, height) = view.present(&mut encoder, &display, &scene, n as f64 / 60.0).expect("presented").size;
        gpu.queue.submit([encoder.finish()]);
        if matches!(session.runtime.context(), xem_core::context::Context::Menu { commands: true, .. }) {
            assert!(view.frame().drawn() > 50, "frame {n}: the menu's buffer at {:?} replays another's frame", (display.x, display.y));
            menu_frames += 1;
        }
        // From the fade back in until the field has drawn again.
        if n < 300 {
            continue;
        }
        let rgba = xem_render::read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().unwrap());
        let side = view.config().unwrap().side;
        let edge = (width * side / (display.width + 2 * side)) as usize;
        let mean = |x0: usize, x1: usize| {
            let (mut sum, mut count) = (0u64, 0u64);
            for y in (0..height as usize).step_by(4) {
                for x in (x0..x1).step_by(4) {
                    let p = &rgba[(y * width as usize + x) * 4..][..3];
                    sum += p.iter().map(|&c| u64::from(c)).sum::<u64>();
                    count += 3;
                }
            }
            sum as f32 / count as f32
        };
        let (left, centre, right) = (mean(0, edge), mean(edge, width as usize - edge), mean(width as usize - edge, width as usize));
        if centre > 8.0 {
            assert!(left > centre * 0.4 && right > centre * 0.4, "frame {n}: sides {left} and {right}, the middle {centre}");
            checked += 1;
        }
    }
    assert!(menu_frames > 30, "{menu_frames} frames of the menu's commands");
    assert!(checked > 100, "{checked} frames of the field shown back");
}

/// A held rewind's steps show the game's frames as the renderer draws them
/// (wider, in high resolution), not the restored VRAM as it is: each takes
/// a state off the queue and plays on from it until the display shows a
/// frame the game drew since, never as far as where the step began.
#[test]
fn held_rewind_steps_show_frames_the_game_drew() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (session, _) = boot(&disc, &Default::default()).expect("boot");
    let clock = std::rc::Rc::new(std::cell::Cell::new(std::time::Duration::ZERO));
    let source = clock.clone();
    let mut session = Session::new(session.runtime, session.schema, move || source.get());
    // A room: walking there starts no battle (a battle's transition holds
    // the field's last picture, drawn before any state).
    session.execute(&json!({"cmd":"enter_field", "map": 30}));
    session.execute(&json!({"cmd":"frames", "count": 340}));
    session.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES, 1, 30).unwrap();
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let graphics = Graphics { widescreen: true, ..Graphics::default() };
    let mut tick = 0u64;
    // One host presentation: what the game drew since the last, drawn;
    // how many of the shown frame's ops the renderer replayed.
    let mut present = |session: &mut Session<NativeModule>, view: &mut GameView| -> usize {
        view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (1280, 720), view.max_scale()));
        let device = session.runtime.services().gpu.device_mut();
        if view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(view.reveals());
        view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        tick += 1;
        assert!(view.present(&mut encoder, &display, &scene, tick as f64 / 60.0).is_some());
        gpu.queue.submit([encoder.finish()]);
        view.frame().drawn()
    };
    // Ten seconds of walking about, a state a second.
    for n in 0..600u64 {
        clock.set(std::time::Duration::from_millis(n * 1000 / 60));
        session.execute(&json!({"cmd":"pad", "buttons": if (n / 90) % 2 == 0 { vec!["left"] } else { vec!["right"] }}));
        assert!(session.frame().unwrap());
        present(&mut session, &mut view);
    }
    for step in 0..8 {
        let was = session.runtime.frames();
        assert!(session.rewind_step().unwrap());
        let landed = session.runtime.frames();
        assert!(landed < was, "step {step}: {was} -> {landed}");
        assert!(present(&mut session, &mut view) > 0, "step {step}: the restored VRAM shown instead of a frame drawn since");
    }
}

/// The flat bands a battle stage's sky draws across the screen go on beside
/// it as culled faces drawn just before each band, in the band's drawing
/// mode: over what the stage draws before the band and under what after,
/// as the band itself (the desert's translucent haze adds over the horizon
/// drawn before it; drawn first, its sides were covered and averaged).
#[test]
fn battle_sky_bands_continue_beside_the_screen_where_they_are_drawn() {
    use xem_gpu::{Primitive, Shape, TraceOp};

    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    start_desert_battle(&mut session);
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let graphics = Graphics { widescreen: true, ..Graphics::default() };
    view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (1280, 720), view.max_scale()));
    session.runtime.services().gpu.device_mut().set_trace(true);
    let (mut checked, mut haze) = (0, false);
    for _ in 0..1700 {
        session.runtime.services().gpu.device_mut().set_reveals(view.reveals());
        assert!(session.frame().unwrap());
        let ops = session.runtime.services().gpu.device_mut().take_trace().ops;
        let draws: Vec<&Primitive> = ops.iter().filter_map(|op| match op { TraceOp::Draw(p) => Some(p), _ => None }).collect();
        for (i, copy) in draws.iter().enumerate() {
            let xs = copy.vertices.map(|v| v.x);
            let beside = xs.iter().all(|&x| x <= 0) || xs.iter().all(|&x| x >= 320);
            if !(copy.revealed && !copy.textured && copy.shape == Shape::Quad && beside && (xs.contains(&0) || xs.contains(&320))) {
                continue;
            }
            // Just before the band (after the other side's copy, if any).
            let others = draws[i + 1..].iter().take_while(|p| p.revealed).count();
            let band = draws.get(i + 1 + others).expect("the band");
            assert!(others <= 1 && band.vertices.iter().all(|v| v.x == 0 || v.x == 320), "{copy:?} is not just before its band: {band:?}");
            assert_eq!((band.vertices[0].color, band.semi), (copy.vertices[0].color, copy.semi));
            assert_eq!(band.environment.draw_mode, copy.environment.draw_mode, "drawn in its band's mode");
            checked += 1;
            haze |= copy.semi;
        }
    }
    assert!(checked > 100 && haze, "{checked} band copies seen, the haze among them: {haze}");
}

/// Below Shevat's towers the game draws a sea of clouds whose texture holds,
/// around each cloud, the colour its buffer was cleared with: on the console
/// that sky meets the cleared sky without an edge. Here too: the clear shows
/// the colour VRAM keeps (as the texels are expanded), and the HD-2D effects
/// take the painted sky for the sky, without depth (no light, shade or haze
/// that the sky beside it does not get).
#[test]
fn the_sky_painted_around_shevats_clouds_meets_the_sky_without_an_edge() {
    use xem_render::ps1::DebugView;

    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    session.execute(&json!({"cmd":"enter_field", "map": 424}));
    session.execute(&json!({"cmd":"frames", "count": 240}));
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    // Modern's picture, and the HD-2D effects' inputs (black: background).
    let mut views = [Graphics::default(), Graphics { profile: GraphicsProfile::Hd2d, ..Graphics::default() }].map(|graphics| {
        let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
        view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (1280, 720), view.max_scale()));
        view
    });
    views[1].effects_mut().set_debug_view(DebugView::Class);
    let mut sizes = [(0, 0); 2];
    for n in 0..40 {
        assert!(session.frame().unwrap());
        let device = session.runtime.services().gpu.device_mut();
        let restart = views.iter_mut().fold(false, |restart, view| view.take_restart() | restart);
        if restart || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(views[0].reveals());
        let trace = device.take_trace();
        let display = device.display();
        let scene = session.scene();
        for (view, size) in views.iter_mut().zip(&mut sizes) {
            view.push(trace.clone());
            let mut encoder = gpu.device.create_command_encoder(&Default::default());
            *size = view.present(&mut encoder, &display, &scene, n as f64 / 60.0).expect("presented").size;
            gpu.queue.submit([encoder.finish()]);
        }
    }
    let pictures: Vec<_> = views.iter().zip(sizes).map(|(view, size)| (size, xem_render::read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().unwrap()))).collect();
    // Each native pixel showing the sky (the frame's commonest colour), and
    // its neighbours too, where the console shows it (the cleared sky and
    // the painted one alike).
    let (width, height, native) = session.runtime.services().gpu.display_rgba8();
    let (width, height) = (width as usize, height as usize);
    let at = |x: usize, y: usize| &native[(y * width + x) * 4..][..3];
    let mut counts = std::collections::HashMap::new();
    for pixel in native.chunks_exact(4) {
        *counts.entry(&pixel[..3]).or_insert(0) += 1;
    }
    let sky = *counts.iter().max_by_key(|(_, n)| **n).unwrap().0;
    let (mut skies, mut modern) = (0, std::collections::HashMap::new());
    for y in 1..height - 1 {
        for x in 1..width - 1 {
            if !(y - 1..=y + 1).all(|y| (x - 1..=x + 1).all(|x| at(x, y) == sky)) {
                continue;
            }
            let shown = |(size, rgba): &((u32, u32), Vec<u8>)| {
                let (px, py) = ((x * 2 + 1) * size.0 as usize / (width * 2), (y * 2 + 1) * size.1 as usize / (height * 2));
                rgba[(py * size.0 as usize + px) * 4..][..3].to_vec()
            };
            *modern.entry(shown(&pictures[0])).or_insert(0) += 1;
            assert_eq!(shown(&pictures[1]), [0, 0, 0], "native ({x}, {y}): sky the effects take for scenery");
            skies += 1;
        }
    }
    assert!(skies > 10_000, "{skies} pixels of sky");
    // In Modern, one colour (the translucent clouds over it, mapped in
    // perspective, lie a little elsewhere than on the console).
    let most = modern.values().max().unwrap();
    eprintln!("sky {sky:?}: {skies} pixels; Modern {modern:?}");
    assert!(most * 100 >= skies * 97, "the sky in Modern: {modern:?}");
}

/// A held rewind in a battle that draws its stage over its last frame
/// instead of clearing it (the desert) shows the frames it draws: they
/// start their logs again after each step's state, as after a state loaded.
#[test]
fn held_rewind_in_a_battle_that_never_clears_shows_its_frames() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (session, _) = boot(&disc, &Default::default()).expect("boot");
    let clock = std::rc::Rc::new(std::cell::Cell::new(std::time::Duration::ZERO));
    let source = clock.clone();
    let mut session = Session::new(session.runtime, session.schema, move || source.get());
    start_desert_battle(&mut session);
    session.execute(&json!({"cmd":"frames", "count": 1500}));
    session.configure_rewind(xem_core::rewind::MAX_REWIND_BYTES, 1, 30).unwrap();
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let graphics = Graphics { widescreen: true, ..Graphics::default() };
    let mut tick = 0u64;
    let mut present = |session: &mut Session<NativeModule>, view: &mut GameView| -> usize {
        view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (1280, 720), view.max_scale()));
        let device = session.runtime.services().gpu.device_mut();
        if view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(view.reveals());
        view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        tick += 1;
        assert!(view.present(&mut encoder, &display, &scene, tick as f64 / 60.0).is_some());
        gpu.queue.submit([encoder.finish()]);
        view.frame().drawn()
    };
    // The trace starts in the battle; five seconds of it, a state a second.
    for n in 0..300u64 {
        clock.set(std::time::Duration::from_millis(n * 1000 / 60));
        assert!(session.frame().unwrap());
        present(&mut session, &mut view);
    }
    assert!(present(&mut session, &mut view) > 0, "the battle's frames are drawn again");
    for step in 0..4 {
        let was = session.runtime.frames();
        assert!(session.rewind_step().unwrap());
        assert!(session.runtime.frames() < was);
        assert!(present(&mut session, &mut view) > 0, "step {step}: the restored VRAM shown instead of a frame drawn since");
    }
}

/// Starts the debug battle selector's (`battle_scene_select_values`) first
/// event file's formation 15: the desert, its party in their Gears.
fn start_desert_battle(session: &mut Session<NativeModule>) {
    for cmd in [
        json!({"cmd":"frames", "count":60}),
        json!({"cmd":"write", "path":"mode_battle_standalone", "value":1}),
        json!({"cmd":"boot_mode", "mode":2}),
        json!({"cmd":"frames", "count":900}),
    ] { assert!(session.execute(&cmd).get("error").is_none()); }
    let mut put = |row: u32, column: u32, value: u32| {
        session.runtime.memory().write(0x801E_1DA0 + (row * 3 + column) * 4, &value.to_le_bytes()).unwrap();
    };
    put(0, 0, 15);
    put(3, 0, 253);
    for column in 0..3 {
        put(2, column, 1);
    }
    session.execute(&json!({"cmd":"press", "buttons":["start"], "frames":1}));
}

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

#[test]
fn first_person_sprite_facings_replay_only_when_looking_and_leave_original_ram_vram_and_sound_unchanged() {
    use xem_gpu::{Reveals, TraceOp};
    let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    assert!(session.execute(&json!({"cmd":"enter_field","map":1})).get("error").is_none());
    assert!(session.execute(&json!({"cmd":"frames","count":300})).get("error").is_none());
    let initial = session.snapshot_bytes().unwrap();
    let run = |session: &mut Session<NativeModule>, around| {
        let gpu = session.runtime.services().gpu.device_mut();
        gpu.set_trace(true);
        gpu.set_reveals(Some(Reveals { reach:[320,240], around, ..Default::default() }));
        let mut changed = 0;
        for _ in 0..90 {
            assert!(session.frame().unwrap());
            let trace = session.runtime.services().gpu.device_mut().take_trace();
            for op in trace.ops {
                if let TraceOp::Draw(p) = op && let Some(facing) = p.facing {
                    assert!(!facing.parts.is_empty(),"an assembled facing must have visible parts");
                    changed += 1;
                }
            }
        }
        let state = (session.memory_hash().unwrap(),
            session.runtime.services().gpu.device().save(),
            session.runtime.services().spu.chip().save(),
            session.runtime.services().clock.now);
        (state, changed)
    };
    let (original, no_facings) = run(&mut session, 0);
    assert_eq!(no_facings,0);
    // A view from anywhere has no viewer's position to face: no facings.
    // (The port takes the bits its GPU commands last handed over, and its
    // rings are no part of a state: the first frame after a restore is
    // built with the run before's, so this one follows the original's.)
    session.restore_bytes(&initial).unwrap();
    let (anywhere, anywhere_facings) = run(&mut session, Reveals::ANYWHERE);
    assert_eq!(anywhere_facings,0,"sprite facings replay for first-person looking only");
    assert_eq!(anywhere,original,"a view from anywhere cannot change original RAM, VRAM, sound or time");
    session.restore_bytes(&initial).unwrap();
    let (looking, facings) = run(&mut session, Reveals::LOOK);
    assert!(facings > 0,"Lahan NPCs must select frames from the player's viewing position");
    assert_eq!(looking,original,"alternate texture assembly cannot change original RAM (including RNG), VRAM, sound or time");
}

#[test]
fn first_person_directional_sprites_render_their_alternate_texture_cells() {
    use xem_gpu::TraceOp;
    let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    assert!(session.execute(&json!({"cmd":"enter_field","map":1})).get("error").is_none());
    assert!(session.execute(&json!({"cmd":"frames","count":400})).get("error").is_none());
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut views: Vec<_> = (0..2).map(|_| {
        let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
        view.configure(ViewConfig::resolve(&Graphics::default(), HostClass::Desktop, (960, 672), view.max_scale()));
        view.set_first_person(true);
        view
    }).collect();
    session.runtime.services().gpu.device_mut().set_trace(true);
    let mut changed_cells = 0;
    for frame in 0..8 {
        session.runtime.services().gpu.device_mut().set_reveals(views[0].reveals());
        assert!(session.frame().unwrap());
        let trace = session.runtime.services().gpu.device_mut().take_trace();
        let mut original_facing = trace.clone();
        for op in &mut original_facing.ops {
            if let TraceOp::Draw(p) = op {
                if let Some(facing) = &p.facing {
                    changed_cells += facing.textures.iter().zip(&facing.restore)
                        .filter(|((_, pixels), (_, original))| pixels != original).count();
                }
                // Reproduce the old first-person rendering from the identical
                // draw trace, keeping the normal camera's sprite frame.
                p.facing = None;
            }
        }
        views[0].push(original_facing);
        views[1].push(trace);
        let display = session.runtime.services().gpu.device().display();
        let scene = session.scene();
        for view in &mut views {
            let mut encoder = gpu.device.create_command_encoder(&Default::default());
            view.present(&mut encoder, &display, &scene, frame as f64 / 60.0).expect("a presented frame");
            gpu.queue.submit([encoder.finish()]);
        }
    }
    assert!(changed_cells > 0, "the viewpoint must select different original frame cells");
    let pixels: Vec<_> = views.iter().map(|view| {
        xem_render::read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().unwrap())
    }).collect();
    let different = pixels[0].chunks_exact(4).zip(pixels[1].chunks_exact(4)).filter(|(a,b)| a != b).count();
    assert!(different > 100, "the visible NPC artwork must change ({different} pixels)");
    eprintln!("first-person facing changes {different} visible pixels in Lahan");
    // Optional local inspection artifacts; original game assets stay local.
    if let Some(directory) = std::env::var_os("XEM_SPRITE_FACING_CAPTURE") {
        std::fs::create_dir_all(&directory).unwrap();
        for (name, view, mut pixels) in ["original-facing", "player-facing"].into_iter().zip(&views).zip(pixels)
            .map(|((name, view), pixels)| (name, view, pixels)) {
            pixels.chunks_exact_mut(4).for_each(|p| p[3] = 255);
            let texture = view.presented_texture().unwrap();
            let path = PathBuf::from(&directory).join(format!("{name}.png"));
            let mut encoder = png::Encoder::new(std::fs::File::create(path).unwrap(), texture.width(), texture.height());
            encoder.set_color(png::ColorType::Rgba);
            encoder.write_header().unwrap().write_image_data(&pixels).unwrap();
        }
    }
}

#[test]
fn a_diorama_seen_from_a_moving_head_leaves_the_game_unchanged() {
    use xem_render::glam::{Mat4, Vec3};
    use xem_render::ps1::{Table, Turns, XrView};
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
    session.restore_bytes(&start).unwrap();
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    let target = gpu.device.create_texture(&xem_render::wgpu::TextureDescriptor {
        label: None,
        size: xem_render::wgpu::Extent3d { width: 320, height: 240, depth_or_array_layers: 1 },
        mip_level_count: 1,
        sample_count: 1,
        dimension: xem_render::wgpu::TextureDimension::D2,
        format: xem_render::wgpu::TextureFormat::Rgba8Unorm,
        usage: xem_render::wgpu::TextureUsages::RENDER_ATTACHMENT,
        view_formats: &[],
    });
    let target_view = target.create_view(&Default::default());
    let mut dioramas = 0;
    for n in 0..FRAMES {
        // The scale and the turns change midway, as a player's would.
        let table = Table { scale: if n < 120 { 1.0 } else { 1.6 }, turns: if n < 160 { Turns::Smooth } else { Turns::Snap }, ..Table::default() };
        view.set_xr_view(XrView::Diorama(table));
        view.configure(ViewConfig::resolve(&Graphics::default(), HostClass::Headset, (640, 480), view.max_scale()));
        assert!(session.frame().expect("frame"));
        let device = session.runtime.services().gpu.device_mut();
        if view.take_restart() || !device.trace_enabled() {
            device.set_trace(true);
        }
        device.set_reveals(view.reveals());
        view.push(device.take_trace());
        let display = device.display();
        let scene = session.scene();
        // A head swaying round the table.
        let t = n as f32 / 60.0;
        let head = Vec3::new(0.3 * t.sin(), 0.1 * (2.0 * t).sin(), 0.2 * t.cos());
        let eyes = [-0.032f32, 0.032].map(|x| xem_render::ViewTarget {
            view: Mat4::look_at_rh(head + Vec3::X * x, Vec3::new(0.0, -0.45, -0.6), Vec3::Y),
            proj: Mat4::perspective_rh(1.6, 4.0 / 3.0, 0.05, 20.0),
            target: &target_view,
            viewport: xem_render::Viewport::full(320, 240),
        });
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let shown = view.present_diorama(&mut encoder, &display, &scene, n as f64 / 60.0, &eyes, Mat4::IDENTITY).is_some_and(|presented| presented.screen < 1.0);
        gpu.queue.submit([encoder.finish()]);
        dioramas += u32::from(shown && view.diorama_space().is_some());
    }
    assert!(dioramas > 150, "the field shows as a diorama ({dioramas} of {FRAMES} frames)");
    assert_eq!(hash(&mut session), skipped, "showing a diorama changes nothing");
}
