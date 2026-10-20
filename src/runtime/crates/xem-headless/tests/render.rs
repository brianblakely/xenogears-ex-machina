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
//! (the port records every face and each frame's camera). The whole state
//! (`Session::snapshot_bytes`) is compared too (`presentation_only`):
//! outside the port's own memory it is the same byte for byte, and what
//! differs within it is the port's presentation bookkeeping, gone two
//! frames on from the same presentation records but for dead stack that
//! nothing reads. `xem-headless --control`'s `reveal_frames` (Lahan drawn
//! wider and pulled back) counts every frame it plays, reveals culled faces
//! and leaves the RAM as plain frames do.
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

/// The whole state a run ended at (`Session::snapshot_bytes`) and its
/// parts, to name where two differ: each memory region a state keeps and
/// each device's saved state.
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

/// Two whole states the same byte for byte, or where they differ: each
/// memory region's differing bytes (how many, the first and the last
/// address), each device whose saved state differs, else the rest of the
/// state (fibers, globals, clock, pads, counters).
fn same(a: &Whole, b: &Whole) -> Result<(), String> {
    if a.bytes == b.bytes {
        return Ok(());
    }
    Err(differences(a, b, |_| true).unwrap_or_else(|| "outside memory and the devices (fibers, globals, clock, pads, counters)".into()))
}

/// Where the parts `which` picks differ (`same`), if they do.
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

/// The port's own memory (tools/game_module.py PORT_BASE): its data and
/// BSS (the fibers' asyncify save areas among them) and its shadow stack.
const PORT: u32 = 0x1000_0000;

/// The port's presentation records, outside every state region (xem/gte.h
/// and sprite_facing.c): the projection, store and culled-face rings, the
/// captures and the sprite facings' scratch. Each begins with its counts.
const PRESENTATION: [u32; 5] = [0x7000_0000, 0x7030_0000, 0x7050_0000, 0x70A0_0000, 0x70B0_0000];

/// The shadow stack's size (tools/game_module.py STACK_BYTES): it lies
/// below the top a fresh module starts the game fiber at.
const STACK_BYTES: u32 = 0x10_0000;

/// The module's stack pointer a state holds (after its header, identity,
/// entry, current fiber and two fibers: runtime.rs `snapshot_content`).
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

/// What presentation leaves in a whole state, `drawn` against `unseen`
/// from the same state and frames, is the port's own bookkeeping alone.
/// Everything outside the port's memory (PS1 RAM, the scratchpad and I/O
/// pages, KUSEG, every device) is the same byte for byte. Within it the
/// presentation's code leaves frames of its own below the game fiber's
/// suspended shadow stack, values the suspended frames saved on their way
/// (asyncify's save area: a pointer on one path, a count on the other),
/// and copies of the presentation rings' counts, which count only while a
/// trace records and lie outside states (so a rendering host's go on
/// across a restore). Played on two frames from the same presentation
/// records with no trace, the two hold the same saved values; what still
/// differs is dead stack alone, deeper than the game's own frames reach,
/// between the stack's bottom and the suspended stack pointer, which no
/// code reads (every port and game function zero-initialises its locals:
/// tools/game_module.py's -ftrivial-auto-var-init=zero), and with those
/// bytes the same the two are the same byte for byte.
fn presentation_only(session: &mut Session<NativeModule>, drawn: &Whole, unseen: &Whole) -> Result<(), String> {
    use xem_core::module::GameModule;
    if let Some(at) = differences(drawn, unseen, |start| start != PORT) {
        return Err(format!("outside the port's memory: {at}"));
    }
    if let Err(at) = same(drawn, unseen) {
        eprintln!("the port's bookkeeping right after: {at}");
    }
    let records: Vec<Vec<u8>> = PRESENTATION.iter().map(|&address| {
        let mut data = vec![0; 0x1_0000];
        session.runtime.memory().read(address, &mut data).unwrap();
        data
    }).collect();
    let mut on = Vec::new();
    for state in [&drawn.bytes, &unseen.bytes] {
        session.restore_bytes(state).unwrap();
        for (&address, data) in PRESENTATION.iter().zip(&records) {
            session.runtime.memory().write(address, data).unwrap();
        }
        session.runtime.services().gpu.device_mut().set_trace(false);
        for _ in 0..2 {
            assert!(session.frame().expect("frame"));
        }
        on.push(whole(session));
    }
    if same(&on[0], &on[1]).is_ok() {
        return Ok(());
    }
    // The dead stack: from the stack's bottom to the suspended pointer.
    let (sp, top) = (stack_pointer(&on[0].bytes), NativeModule::new().stack_pointer());
    if stack_pointer(&on[1].bytes) != sp {
        return Err("two frames on, the game fiber's stack pointers differ".into());
    }
    let dead = top - STACK_BYTES..sp;
    if let Some(at) = differences(&on[0], &on[1], |start| start != PORT) {
        return Err(format!("two frames on, outside the port's memory: {at}"));
    }
    fn port(w: &Whole) -> &[u8] {
        &w.parts.iter().find(|(name, start, _)| name == "memory" && *start == PORT).unwrap().2
    }
    let (a, b) = (port(&on[0]), port(&on[1]));
    if let Some(i) = (0..a.len()).find(|&i| a[i] != b[i] && !dead.contains(&(PORT + i as u32))) {
        return Err(format!("two frames on, port memory at {:#010x} differs outside the dead stack {dead:#x?}", PORT + i as u32));
    }
    // The session holds the second: with the first's dead stack, the same.
    let from = (dead.start - PORT) as usize..(dead.end - PORT) as usize;
    session.runtime.memory().write(dead.start, &a[from.clone()]).unwrap();
    if session.snapshot_bytes().unwrap() != on[0].bytes {
        return Err("two frames on, the states differ beside the dead stack".into());
    }
    let differing: Vec<usize> = from.filter(|&i| a[i] != b[i]).collect();
    eprintln!("two frames on: {} bytes of dead stack alone differ, {:#010x} to {:#010x} (the suspended stack pointer {sp:#010x})",
        differing.len(), PORT as usize + differing[0], PORT as usize + differing[differing.len() - 1]);
    Ok(())
}

/// Runs `FRAMES` frames; before frame `n`, `view(n)` says which view (if
/// any) draws it and with which settings, and `look(n)` how far it turns
/// looking around in first person (`None`: it does not). Returns the RAM
/// hash, the frames shown from the field character's eyes and the whole
/// state at the end.
fn run(
    session: &mut Session<NativeModule>,
    gpu: &Gpu,
    mut view: impl FnMut(u64) -> Option<(usize, Graphics)>,
    look: impl Fn(u64) -> Option<f32>,
) -> (String, u64, Whole) {
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
    (hash(session), looked, whole(session))
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

    let (skipped, _, unseen) = run(&mut session, &gpu, |_| None, |_| None);

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
    let (every_frame, _, drawn) = run(&mut session, &gpu, |n| Some((0, profiles[(n / 30) as usize % profiles.len()])), |_| None);
    assert_eq!(every_frame, skipped, "drawing every frame under switching profiles changes nothing");
    presentation_only(&mut session, &drawn, &unseen).unwrap_or_else(|at| panic!("drawing every frame changes the whole state: {at}"));

    session.restore_bytes(&start).unwrap();
    let (spectator, _, watched) = run(&mut session, &gpu, |n| match n {
        // A spectator attaches, changes its look, and detaches.
        60..120 => Some((1, profiles[0])),
        120..180 => Some((1, profiles[1])),
        _ => None,
    }, |_| None);
    assert_eq!(spectator, skipped, "a spectator attaching and detaching changes nothing");
    presentation_only(&mut session, &watched, &unseen).unwrap_or_else(|at| panic!("a spectator changes the whole state: {at}"));

    // Looking around in first person midway, under two profiles, turning.
    session.restore_bytes(&start).unwrap();
    let (looking, looked, looked_around) = run(&mut session, &gpu, |n| Some((0, profiles[(n / 60) as usize % 2])), |n| (40..200).contains(&n).then_some(0.05));
    assert!(looked > 100, "the field's character's eyes show ({looked} frames)");
    assert_eq!(looking, skipped, "looking around in first person changes nothing");
    presentation_only(&mut session, &looked_around, &unseen).unwrap_or_else(|at| panic!("looking around changes the whole state: {at}"));
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
        (state, changed, whole(session))
    };
    let (original, no_facings, unfaced) = run(&mut session, 0);
    assert_eq!(no_facings,0);
    // A view from anywhere has no viewer's position to face: no facings.
    // (The port takes the bits its GPU commands last handed over, and its
    // rings are no part of a state: the first frame after a restore is
    // built with the run before's, so this one follows the original's.)
    session.restore_bytes(&initial).unwrap();
    let (anywhere, anywhere_facings, from_anywhere) = run(&mut session, Reveals::ANYWHERE);
    assert_eq!(anywhere_facings,0,"sprite facings replay for first-person looking only");
    assert_eq!(anywhere,original,"a view from anywhere cannot change original RAM, VRAM, sound or time");
    presentation_only(&mut session, &from_anywhere, &unfaced).unwrap_or_else(|at| panic!("a view from anywhere changes the whole state: {at}"));
    session.restore_bytes(&initial).unwrap();
    let (looking, facings, faced) = run(&mut session, Reveals::LOOK);
    assert!(facings > 0,"Lahan NPCs must select frames from the player's viewing position");
    assert_eq!(looking,original,"alternate texture assembly cannot change original RAM (including RNG), VRAM, sound or time");
    presentation_only(&mut session, &faced, &unfaced).unwrap_or_else(|at| panic!("sprite facings change the whole state: {at}"));
}

/// In a large-scale field (Babel Tower's map 414, which runs event fe 1e:
/// the party on foot draws small sprites of the map's own) a traced frame's
/// small sprite parts carry the full-size frame the port assembles from the
/// character's own field sprite, which the host reads from the disc the
/// first time the port asks for it, outside every state region. Assembling
/// them leaves RAM (with the RNG), VRAM, sound, time and the whole state as
/// a run without a trace leaves them.
#[test]
fn large_scale_fields_get_full_size_party_frames_that_leave_original_ram_vram_and_sound_unchanged() {
    use xem_gpu::TraceOp;
    let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    assert!(session.execute(&json!({"cmd":"enter_field","map":414})).get("error").is_none());
    assert!(session.execute(&json!({"cmd":"frames","count":300})).get("error").is_none());
    let alternate = session.execute(&json!({"cmd":"inspect","path":"field_work.unk2268","depth":0}));
    assert_ne!(alternate, json!(0), "the map's party sprites are its alternate set's");
    // Fei's block, where the host keeps it (xem_core::presentation::FULL_SPRITES).
    let fei = |session: &mut Session<NativeModule>| session.runtime.memory().read_u32(0x70C0_0000).unwrap();
    let initial = session.snapshot_bytes().unwrap();
    let traced = |session: &mut Session<NativeModule>, trace: bool| {
        session.runtime.services().gpu.device_mut().set_trace(trace);
        let mut parts = 0;
        for _ in 0..90 {
            assert!(session.frame().unwrap());
            for op in session.runtime.services().gpu.device_mut().take_trace().ops {
                if let TraceOp::Draw(p) = op && let Some(full_size) = p.full_size {
                    assert!(!full_size.parts.is_empty() && !full_size.textures.is_empty(), "an assembled frame has parts and cells");
                    parts += 1;
                }
            }
        }
        let state = (session.memory_hash().unwrap(),
            session.runtime.services().gpu.device().save(),
            session.runtime.services().spu.chip().save(),
            session.runtime.services().clock.now);
        (state, parts, whole(session))
    };
    let (original, none, plain) = traced(&mut session, false);
    assert_eq!((none, fei(&mut session)), (0, 0), "no trace: no frames, and no block read");
    session.restore_bytes(&initial).unwrap();
    let (drawn, parts, assembled) = traced(&mut session, true);
    assert_eq!(fei(&mut session), 3, "Fei's block (its three sections) read from the disc");
    assert!(parts > 0, "the party's small sprites get their full-size frames");
    assert_eq!(drawn, original, "assembling them cannot change original RAM (including RNG), VRAM, sound or time");
    presentation_only(&mut session, &assembled, &plain).unwrap_or_else(|at| panic!("full-size frames change the whole state: {at}"));
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

/// The hosts' view draws those full-size frames in place of the small
/// sprites with the setting on (`Graphics::full_size_sprites`): the same
/// traced frames presented with it off and on differ only around the party
/// on screen, where the full sets' frames have more parts than the small.
#[test]
fn the_full_size_setting_draws_the_party_of_a_large_scale_field_with_its_own_sprites() {
    use xem_gpu::{Class, TraceOp};
    let Some(disc) = disc() else { eprintln!("skipping: no disc 1 (set XEM_DISC1)"); return };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    assert!(session.execute(&json!({"cmd":"enter_field","map":414})).get("error").is_none());
    assert!(session.execute(&json!({"cmd":"frames","count":300})).get("error").is_none());
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut views: Vec<_> = [false, true].into_iter().map(|full_size_sprites| {
        let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
        let graphics = Graphics { full_size_sprites, internal_scale: 4, ..Graphics::default() };
        view.configure(ViewConfig::resolve(&graphics, HostClass::Desktop, (1280, 896), view.max_scale()));
        view
    }).collect();
    session.runtime.services().gpu.device_mut().set_trace(true);
    for frame in 0..8 {
        assert!(session.frame().unwrap());
        let device = session.runtime.services().gpu.device_mut();
        device.set_reveals(views[0].reveals());
        let trace = device.take_trace();
        views[0].push(trace.clone());
        views[1].push(trace);
        let display = session.runtime.services().gpu.device().display();
        let scene = session.scene();
        for view in &mut views {
            let mut encoder = gpu.device.create_command_encoder(&Default::default());
            view.present(&mut encoder, &display, &scene, frame as f64 / 60.0).expect("a presented frame");
            gpu.queue.submit([encoder.finish()]);
        }
    }
    let sprites: Vec<usize> = views.iter().map(|view| view.frame().primitives().iter()
        .filter(|op| matches!(op, TraceOp::Draw(p) if p.class == Class::Sprite)).count()).collect();
    assert!(sprites[1] > sprites[0], "the full-size frames have more parts than the small sprites: {sprites:?}");
    let pixels: Vec<_> = views.iter().map(|view| xem_render::read_rgba8(&gpu.device, &gpu.queue, view.presented_texture().unwrap())).collect();
    let width = views[0].presented_texture().unwrap().width() as usize;
    let differing: Vec<(usize, usize)> = pixels[0].chunks_exact(4).zip(pixels[1].chunks_exact(4)).enumerate()
        .filter(|(_, (a, b))| a != b).map(|(i, _)| (i % width, i / width)).collect();
    let (x, y) = (differing.iter().map(|p| p.0), differing.iter().map(|p| p.1));
    let (w, h) = (x.clone().max().unwrap() - x.min().unwrap() + 1, y.clone().max().unwrap() - y.min().unwrap() + 1);
    eprintln!("{} pixels differ, within {w}x{h}", differing.len());
    assert!(differing.len() > 100, "the party's sprite is drawn anew ({} pixels)", differing.len());
    assert!(w < 120 && h < 120, "and nothing else: the pixels that differ lie within {w}x{h}");
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
    let (skipped, _, unseen) = run(&mut session, &gpu, |_| None, |_| None);
    session.restore_bytes(&start).unwrap();
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
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
        let eyes = [-0.032f32, 0.032].map(|x| xem_render::ps1::DioramaEye {
            view: Mat4::look_at_rh(head + Vec3::X * x, Vec3::new(0.0, -0.45, -0.6), Vec3::Y),
            proj: Mat4::perspective_rh(1.6, 4.0 / 3.0, 0.05, 20.0),
            size: (320, 240),
        });
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let shown = view.present_diorama(&mut encoder, &display, &scene, n as f64 / 60.0, &eyes, Mat4::IDENTITY).is_some_and(|presented| presented.screen < 1.0);
        gpu.queue.submit([encoder.finish()]);
        dioramas += u32::from(shown && view.diorama_space().is_some());
    }
    assert!(dioramas > 150, "the field shows as a diorama ({dioramas} of {FRAMES} frames)");
    assert_eq!(hash(&mut session), skipped, "showing a diorama changes nothing");
    let shown = whole(&mut session);
    presentation_only(&mut session, &shown, &unseen).unwrap_or_else(|at| panic!("a diorama changes the whole state: {at}"));
}

/// `xem-headless --control`'s `reveal_frames`, whose answer arm64-smoke.sh
/// compares under qemu with the native one: Lahan drawn as a wider framing
/// with the camera pulled back reveals faces the game culls, more than
/// without the pull-back (the port's floating point judges those); every
/// frame it plays counts, the first too (120 frames in one call trace as
/// many of the game's own primitives as 120 calls of one frame each, each
/// call's trace starting anew); and the RAM goes on as plain frames leave
/// it.
#[test]
fn reveal_frames_counts_every_frame_reveals_more_culled_faces_pulled_back_and_leaves_the_ram_as_plain_frames_do() {
    use std::io::Write as _;
    use std::process::{Command, Stdio};
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    // The answers to Lahan's entry, 300 frames, `then` and the RAM hash.
    let play = |then: Vec<serde_json::Value>| -> Vec<serde_json::Value> {
        let mut child = Command::new(env!("CARGO_BIN_EXE_xem-headless"))
            .args(["--control", "--disc"])
            .arg(&disc)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::null())
            .spawn()
            .expect("xem-headless");
        let mut stdin = child.stdin.take().unwrap();
        let entry = [json!({"cmd": "enter_field", "map": 1}), json!({"cmd": "frames", "count": 300})];
        for command in entry.into_iter().chain(then).chain([json!({"cmd": "memory_hash"})]) {
            writeln!(stdin, "{command}").unwrap();
        }
        drop(stdin);
        let output = child.wait_with_output().expect("xem-headless's answers");
        assert!(output.status.success(), "xem-headless failed");
        String::from_utf8(output.stdout).unwrap().lines().map(|line| serde_json::from_str(line).unwrap()).collect()
    };
    let pulled_back = play(vec![json!({"cmd": "reveal_frames", "count": 120, "reach": [85, 32], "pull_back": 3000})]);
    let wider = play(vec![json!({"cmd": "reveal_frames", "count": 120, "reach": [85, 32]})]);
    let plain = play(vec![json!({"cmd": "frames", "count": 120})]);
    // The default count, one frame, 120 times.
    let one_by_one = play(vec![json!({"cmd": "reveal_frames", "reach": [85, 32], "pull_back": 3000}); 120]);
    let revealed = |answers: &[serde_json::Value]| {
        assert_eq!(answers[2]["frames"], 120, "{}", answers[2]);
        assert!(answers[2]["drawn"].as_u64() > answers[2]["revealed"].as_u64(), "{}", answers[2]);
        answers[2]["revealed"].as_u64().unwrap()
    };
    let (far, near) = (revealed(&pulled_back), revealed(&wider));
    // The game's own primitives, those traced less the culled faces: what
    // the GPU drew, whenever the trace started.
    let own = |answer: &serde_json::Value| answer["drawn"].as_u64().unwrap() - answer["revealed"].as_u64().unwrap();
    let singles = &one_by_one[2..122];
    assert!(singles.iter().all(|answer| answer["frames"] == 1), "{singles:?}");
    let (whole, each) = (own(&pulled_back[2]), singles.iter().map(own).sum::<u64>());
    let ram = |answers: &[serde_json::Value]| answers.last().cloned().unwrap();
    eprintln!(
        "Lahan, 120 frames: {far} culled faces revealed pulled back, {near} without; the game's own primitives {whole}, \
         {each} a frame a call; RAM {}",
        ram(&plain)
    );
    assert!(near > 0 && far > 2 * near, "culled faces revealed: {far} pulled back, {near} without");
    assert!(each > 0 && each == whole, "the game's own primitives in 120 frames: {whole} in one call, {each} in 120 calls of one");
    assert_eq!(ram(&pulled_back), ram(&plain), "the RAM after reveal_frames pulled back is that of plain frames");
    assert_eq!(ram(&wider), ram(&plain), "the RAM after reveal_frames is that of plain frames");
    assert_eq!(ram(&one_by_one), ram(&plain), "the RAM after reveal_frames a frame at a time is that of plain frames");
}
