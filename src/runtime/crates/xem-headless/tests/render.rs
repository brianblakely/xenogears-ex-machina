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
fn first_person_sprite_facings_leave_original_ram_vram_and_sound_unchanged() {
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
    let (original, no_facings) = run(&mut session,false);
    assert_eq!(no_facings,0);
    session.restore_bytes(&initial).unwrap();
    let (looking, facings) = run(&mut session,true);
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
