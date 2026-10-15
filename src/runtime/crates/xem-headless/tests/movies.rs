//! Enhanced and Original movies on the original game (local only: it needs
//! the user's disc 1 and a game module build; the GPU is whatever wgpu
//! finds, lavapipe in the runtime shell, which enlarges rebuilt frames
//! bilinearly unless FSR 1 is forced). Presented every frame as a desktop
//! host presents them, at 640x480:
//!
//! - every frame of the title movie that shows a picture is rebuilt from
//!   what the MDEC decoded, by a view enlarging bilinearly and by one
//!   forced to FSR 1, until the movie eases into the MDEC's own picture
//!   over its last second (its last frame and the frames held after it the
//!   MDEC's) and the title's 15-bit frames follow; each rebuilt picture,
//!   averaged back to the display's pixels, stays within the MDEC's error
//!   of the device's picture and shows its 8x8 blocks' edges less than the
//!   device's picture does;
//! - under the Original setting no frame is rebuilt, and the game's RAM ends
//!   the same with Enhanced, Original or no presentation;
//! - the new-game movie is rebuilt to its last frame and the next frame is
//!   the field's 15-bit one;
//! - a state restored mid-movie, the trace restarted for a new internal
//!   scale and a view attached mid-movie show rebuilt frames again within
//!   two movie frames, never the picture of a timeline left behind.
//!
//!     XEM_DISC1=<disc 1 image> cargo test --release -p xem-headless --test movies

#![cfg(all(has_game_module, not(debug_assertions)))]

use serde_json::json;
use std::path::PathBuf;
use xem_core::control::{Session, button_bit};
use xem_game::{NativeModule, boot};
use xem_gpu::DisplayInfo;
use xem_render::Gpu;
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_settings::{Graphics, MovieFilter};

/// The output a view presents into (a 320x216 movie frame rebuilt at
/// twice its size, 640x432).
const OUTPUT: (u32, u32) = (640, 480);

fn disc() -> Option<PathBuf> {
    let path = std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../discs/Xenogears disc 1.chd"));
    path.exists().then_some(path)
}

fn read(session: &mut Session<NativeModule>, path: &str) -> i64 {
    let value = session.execute(&json!({"cmd": "inspect", "path": path, "depth": 0}));
    value.as_i64().unwrap_or_else(|| panic!("{path}: {value}"))
}

fn hash(session: &mut Session<NativeModule>) -> String {
    format!("{:016x}", session.memory_hash().unwrap())
}

/// The default graphics, movies under `movies`.
fn graphics(movies: MovieFilter) -> Graphics {
    let mut graphics = Graphics::default();
    graphics.filtering.movies = movies;
    graphics
}

/// A view enlarging rebuilt movie frames as this GPU does or, with `fsr`,
/// by FSR 1.
fn view(gpu: &Gpu, fsr: bool) -> GameView {
    let mut view = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    if fsr {
        view.set_software_movies(false);
    }
    view
}

/// What the game drew since the last call, for each of `views` (configured
/// from `graphics`; the trace restarted when one asks, as a host does), and
/// the display and scene.
fn take(session: &mut Session<NativeModule>, views: &mut [GameView], graphics: &Graphics) -> (DisplayInfo, String) {
    let mut restart = false;
    for view in views.iter_mut() {
        view.configure(ViewConfig::resolve(graphics, HostClass::Desktop, OUTPUT, view.max_scale()));
        restart |= view.take_restart();
    }
    let scene = session.scene();
    let device = session.runtime.services().gpu.device_mut();
    if restart || !device.trace_enabled() {
        device.set_trace(true);
    }
    device.set_reveals(views[0].reveals());
    let trace = device.take_trace();
    for view in views.iter_mut() {
        view.push(trace.clone());
    }
    (device.display(), scene)
}

/// `view`'s presentation of `display` at frame `n`: the picture's size, if
/// it shows one (none for a 24-bit frame left to the device's own picture).
fn present(gpu: &Gpu, view: &mut GameView, display: &DisplayInfo, scene: &str, n: u64) -> Option<(u32, u32)> {
    let mut encoder = gpu.device.create_command_encoder(&Default::default());
    let size = view.present(&mut encoder, display, scene, n as f64 / 60.0).map(|presented| presented.size);
    gpu.queue.submit([encoder.finish()]);
    size
}

/// A picture as RGB levels (0-255), row by row.
struct Picture {
    width: usize,
    height: usize,
    rgb: Vec<[f32; 3]>,
}

impl Picture {
    fn new(width: u32, height: u32, rgba: &[u8]) -> Picture {
        let rgb = rgba.chunks_exact(4).map(|p| [p[0], p[1], p[2]].map(f32::from)).collect();
        Picture { width: width as usize, height: height as usize, rgb }
    }

    /// The display as the device shows it.
    fn device(session: &mut Session<NativeModule>) -> Picture {
        let (width, height, rgba) = session.runtime.services().gpu.display_rgba8();
        Picture::new(width, height, &rgba)
    }

    /// What `view` presented last, `size` of it.
    fn presented(gpu: &Gpu, view: &GameView, size: (u32, u32)) -> Picture {
        let texture = view.presented_texture().expect("a presented picture");
        assert_eq!((texture.width(), texture.height()), size, "the picture fills its texture");
        Picture::new(size.0, size.1, &xem_render::read_rgba8(&gpu.device, &gpu.queue, texture))
    }

    /// Averaged over `factor` x `factor` pixels.
    fn boxed(&self, factor: usize) -> Picture {
        let (width, height) = (self.width / factor, self.height / factor);
        let mut rgb = vec![[0.0; 3]; width * height];
        for (i, out) in rgb.iter_mut().enumerate() {
            let (x, y) = (i % width * factor, i / width * factor);
            for dy in 0..factor {
                for dx in 0..factor {
                    let p = self.rgb[(y + dy) * self.width + x + dx];
                    (0..3).for_each(|c| out[c] += p[c] / (factor * factor) as f32);
                }
            }
        }
        Picture { width, height, rgb }
    }

    /// The mean absolute difference from `other`, levels per channel.
    fn mean_error(&self, other: &Picture) -> f32 {
        assert_eq!((self.width, self.height), (other.width, other.height));
        let sum: f32 = self.rgb.iter().zip(&other.rgb).map(|(a, b)| (0..3).map(|c| (a[c] - b[c]).abs()).sum::<f32>()).sum();
        sum / (3 * self.rgb.len()) as f32
    }

    /// The peak signal-to-noise ratio against `other`, dB.
    fn psnr(&self, other: &Picture) -> f32 {
        let sum: f32 = self.rgb.iter().zip(&other.rgb).map(|(a, b)| (0..3).map(|c| (a[c] - b[c]).powi(2)).sum::<f32>()).sum();
        let mse = (sum / (3 * self.rgb.len()) as f32).max(1e-6);
        10.0 * (255.0 * 255.0 / mse).log10()
    }

    fn mean(&self) -> f32 {
        self.rgb.iter().map(|p| p.iter().sum::<f32>()).sum::<f32>() / (3 * self.rgb.len()) as f32
    }

    /// How much more the luminance steps between neighbours across the
    /// edges of the 8x8 blocks the MDEC decodes than between neighbours
    /// within them (about 1 for a picture without blocks).
    fn block_edges(&self) -> f32 {
        let luma = |x: usize, y: usize| {
            let [r, g, b] = self.rgb[y * self.width + x];
            0.299 * r + 0.587 * g + 0.114 * b
        };
        let (mut edges, mut inside) = ([0f64; 2], [0f64; 2]);
        let mut add = |at_edge: bool, step: f32| {
            let sums = if at_edge { &mut edges } else { &mut inside };
            sums[0] += f64::from(step);
            sums[1] += 1.0;
        };
        for y in 0..self.height {
            for x in 1..self.width {
                add(x % 8 == 0, (luma(x, y) - luma(x - 1, y)).abs());
            }
        }
        for y in 1..self.height {
            for x in 0..self.width {
                add(y % 8 == 0, (luma(x, y) - luma(x, y - 1)).abs());
            }
        }
        ((edges[0] / edges[1]) / (inside[0] / inside[1]).max(1e-6)) as f32
    }
}

/// A rebuilt picture against the device's of the same frame: the mean
/// error and PSNR of the rebuilt picture averaged back to the display's
/// pixels, and both pictures' block edges (the rebuilt one's so averaged).
fn compare(rebuilt: &Picture, device: &Picture) -> (f32, f32, f32, f32) {
    let factor = rebuilt.width / device.width;
    assert_eq!((rebuilt.width, rebuilt.height), (device.width * factor, device.height * factor), "rebuilt at a whole multiple");
    let boxed = rebuilt.boxed(factor);
    (boxed.mean_error(device), boxed.psnr(device), boxed.block_edges(), device.block_edges())
}

/// What the game says of the title movie: the frame it showed last and its
/// last one, while the movie mode plays it.
fn title_movie(session: &mut Session<NativeModule>) -> Option<(i64, i64)> {
    (session.runtime.services().mode == Some(6) && read(session, "cd_movie_request_kind") == 1)
        .then(|| (read(session, "movie_shown_frame"), read(session, "movie_end_frame")))
}

/// The title movie, presented on every frame from boot to the title's
/// screen by a view enlarging as this GPU does and one using FSR 1.
#[test]
fn the_title_movie_is_rebuilt_on_every_frame_clearer_than_the_mdecs_and_eases_into_the_title() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let graphics = graphics(MovieFilter::Enhanced);
    assert_eq!(Graphics::default().filtering.movies, MovieFilter::Enhanced, "Enhanced is the default");
    let mut views = [view(&gpu, false), view(&gpu, true)];
    // The first frame rebuilt, the first showing a picture, the frame the
    // game shows its last movie frame at, the first 15-bit frame after it.
    let (mut first, mut picture, mut last, mut title) = (None, None, None, None);
    let (mut rebuilt, mut new_frames) = (0, 0);
    let mut samples = Vec::new();
    let mut eased = Vec::new();
    let mut shown_before = None;
    for n in 0..1500u64 {
        assert!(session.frame().expect("frame"));
        let (display, scene) = take(&mut session, &mut views, &graphics);
        let movie = title_movie(&mut session);
        let sizes: Vec<_> = views.iter_mut().map(|view| present(&gpu, view, &display, &scene, n)).collect();
        let shows: Vec<_> = views.iter().map(GameView::presented_movie).collect();
        assert_eq!(shows[0], shows[1], "frame {n}: both enlargements rebuild the same frames");
        let device = Picture::device(&mut session);
        if display.color24 && picture.is_none() && device.mean() > 1.0 {
            picture = Some(n);
        }
        if shows[0] {
            first.get_or_insert(n);
            rebuilt += 1;
            assert_eq!(sizes[0], Some((640, 432)), "frame {n}: rebuilt at the output's size");
        }
        // The game's last movie frame: the MDEC's own picture, and so are the
        // frames held after it until the title's draw.
        if let Some((shown, end)) = movie
            && shown == end
            && end > 0
        {
            last.get_or_insert(n);
        }
        if display.color24 && first.is_some() && last.is_none() {
            assert!(shows[0], "frame {n}: a movie frame ({display:?}) left to the device's picture");
        }
        if last.is_some() && display.color24 {
            assert!(!shows[0] && sizes[0].is_none(), "frame {n}: rebuilt after the movie's last frame");
        }
        if last.is_some() && !display.color24 && title.is_none() {
            title = Some(n);
            assert!(scene.starts_with("field"), "frame {n}: the title's screen ({scene})");
            assert!(sizes[0].is_some() && !shows[0], "frame {n}: the title drawn by the renderer");
        }
        // New movie frames: some at full strength, and those the ease mixes.
        let new = movie.map(|(shown, _)| shown) != shown_before;
        shown_before = movie.map(|(shown, _)| shown);
        let left = movie.map_or(i64::MAX, |(shown, end)| end - shown);
        if !(shows[0] && new && device.mean() > if left < 15 { 1.0 } else { 20.0 }) {
            continue;
        }
        new_frames += 1;
        if left >= 15 && new_frames % 8 != 0 {
            continue;
        }
        let pictures: Vec<_> = views.iter().zip(&sizes).map(|(view, size)| Picture::presented(&gpu, view, size.unwrap())).collect();
        if left < 15 {
            eased.push((left, compare(&pictures[0], &device).0));
        } else {
            for (fsr, rebuilt) in [false, true].into_iter().zip(&pictures) {
                samples.push((n, fsr, compare(rebuilt, &device)));
            }
        }
    }
    let (first, picture, last, title) = (first.expect("a rebuilt frame"), picture.expect("a movie picture"), last.expect("the movie's last frame"), title.expect("the title's screen"));
    eprintln!("title movie: pictures from frame {picture}, rebuilt {first}..{last} ({rebuilt} frames), the title at {title}");
    assert!(first <= picture, "the first picture ({picture}) is rebuilt (from {first})");
    assert!(rebuilt > 900, "{rebuilt} frames rebuilt");
    assert!(samples.len() >= 20, "{} frames compared", samples.len());
    for (n, fsr, (error, psnr, edges, device_edges)) in &samples {
        eprintln!("frame {n} fsr={fsr}: mean error {error:.2}, PSNR {psnr:.1} dB, block edges {edges:.3} (the device's {device_edges:.3})");
        assert!(*error <= 6.0 && *psnr >= 30.0, "frame {n} fsr={fsr}: the rebuilt picture strays from the MDEC's ({error:.2}, {psnr:.1} dB)");
        assert!(edges < device_edges, "frame {n} fsr={fsr}: the blocks' edges show as much as the device's ({edges:.3} against {device_edges:.3})");
    }
    // Over its last second the movie eases into the MDEC's picture: each
    // frame closer than the one before, the last nearly the MDEC's.
    eprintln!("eased (frames left, mean error): {eased:?}");
    assert!(eased.len() >= 10, "{} eased frames compared", eased.len());
    for pair in eased.windows(2) {
        assert!(pair[0].0 > pair[1].0 && pair[1].1 < pair[0].1, "the ease goes on toward the MDEC's picture: {pair:?}");
    }
    let (most, least) = (eased[0], eased[eased.len() - 1]);
    assert!(least.0 <= 2 && least.1 < most.1 / 4.0, "the ease ends near the MDEC's picture: {least:?} after {most:?}");
}

/// The same frames from boot through the title movie presented under
/// Enhanced, under Original and not at all.
#[test]
fn the_original_setting_rebuilds_no_frame_and_neither_setting_changes_the_game() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    const FRAMES: u64 = 1100;
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut hashes = Vec::new();
    for movies in [Some(MovieFilter::Enhanced), Some(MovieFilter::Original), None] {
        let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
        let mut views = [view(&gpu, false)];
        let (mut movie_frames, mut rebuilt) = (0, 0);
        for n in 0..FRAMES {
            assert!(session.frame().expect("frame"));
            let Some(movies) = movies else { continue };
            let (display, scene) = take(&mut session, &mut views, &graphics(movies));
            let size = present(&gpu, &mut views[0], &display, &scene, n);
            movie_frames += u32::from(display.color24);
            rebuilt += u32::from(views[0].presented_movie());
            if movies == MovieFilter::Original {
                assert!(!views[0].shows_movie(&display) && !views[0].presented_movie(), "frame {n}: rebuilt under Original");
                assert!(!display.color24 || size.is_none(), "frame {n}: a 24-bit frame is the device's own picture");
                assert!(!GameView::shows(&display) || size.is_some(), "frame {n}: a 15-bit frame drawn");
            }
        }
        eprintln!("{movies:?}: {movie_frames} 24-bit frames, {rebuilt} rebuilt");
        if movies.is_some() {
            assert!(movie_frames > 600, "{movie_frames} frames of the title movie");
        }
        if movies == Some(MovieFilter::Enhanced) {
            assert!(rebuilt > 600, "{rebuilt} frames rebuilt under Enhanced");
        }
        hashes.push(hash(&mut session));
    }
    assert_eq!(hashes[0], hashes[2], "Enhanced movies leave the game as it is");
    assert_eq!(hashes[1], hashes[2], "Original movies leave the game as it is");
}

/// The new-game route (tests/reference-inputs/new-game-movie-transition.json):
/// its movie plays to its end from frame 2168 to 20013 (field 4's movie
/// 13); a view attached at frame 19 300 presents its last 714 frames and
/// the field's first ones.
#[test]
fn the_new_game_movie_is_rebuilt_to_its_last_frame_and_the_field_follows_in_15_bit() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    const ROUTE: [(u64, u64, &str); 5] = [(901, 910, "start"), (1300, 1310, "start"), (1500, 1510, "cross"), (1750, 1760, "up"), (1810, 1820, "cross")];
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    for n in 0..19_300u64 {
        let held = ROUTE.iter().filter(|(start, end, _)| (*start..*end).contains(&n)).fold(0, |bits, (_, _, name)| bits | button_bit(name).unwrap());
        session.runtime.services().pads.0[0].buttons = held;
        assert!(session.frame().expect("frame"));
    }
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut views = [view(&gpu, false)];
    let graphics = graphics(MovieFilter::Enhanced);
    let (mut first, mut last, mut field) = (None, None, None);
    for n in 19_300..20_200u64 {
        assert!(session.frame().expect("frame"));
        let (display, scene) = take(&mut session, &mut views, &graphics);
        let size = present(&gpu, &mut views[0], &display, &scene, n);
        let rebuilt = views[0].presented_movie();
        if rebuilt {
            first.get_or_insert(n);
            last = Some(n);
        }
        if first.is_some() && display.color24 && field.is_none() {
            assert!(rebuilt, "frame {n}: a movie frame left to the device's picture");
        }
        if first.is_some() && !display.color24 && field.is_none() {
            field = Some(n);
            assert!(scene.starts_with("field") && size.is_some() && !rebuilt, "frame {n}: the field's own frame ({scene})");
        }
    }
    let (first, last, field) = (first.expect("a rebuilt frame"), last.expect("a rebuilt frame"), field.expect("the field's frame"));
    eprintln!("new-game movie: rebuilt {first}..={last}, the field's 15-bit frame at {field}");
    assert!(first <= 19_300 + 8, "rebuilt frames begin within two movie frames of the trace starting ({first})");
    assert_eq!(field, last + 1, "the field's frame follows the movie's last, rebuilt");
}

/// The game frames run until a presentation is rebuilt again after
/// `restart` (each before it checked to be the device's own picture), that
/// rebuilt picture and the device's of the same frame; the first
/// presentation comes before another game frame runs.
fn resumes(gpu: &Gpu, session: &mut Session<NativeModule>, views: &mut [GameView], graphics: &Graphics, restart: impl FnOnce(&mut Session<NativeModule>, &mut [GameView])) -> (u64, Picture, Picture) {
    restart(session, views);
    for waited in 0..40u64 {
        if waited > 0 {
            assert!(session.frame().expect("frame"));
        }
        let (display, scene) = take(session, views, graphics);
        let size = present(gpu, &mut views[0], &display, &scene, 1_000_000 + waited);
        if views[0].presented_movie() {
            let device = Picture::device(session);
            let picture = Picture::presented(gpu, &views[0], size.expect("a rebuilt picture"));
            return (waited, picture, device);
        }
        assert!(size.is_none(), "after {waited} frames: neither rebuilt nor the device's picture");
    }
    panic!("no rebuilt frame within 40 frames");
}

/// The title movie mid-way: a state taken a moment before restored, one
/// taken five seconds before, the trace restarted for a new internal
/// scale, and a view attached while it plays.
#[test]
fn rebuilt_frames_go_on_after_a_restore_a_restart_and_a_view_attached_mid_movie() {
    let Some(disc) = disc() else {
        eprintln!("skipping: no disc 1 (set XEM_DISC1)");
        return;
    };
    let (mut session, _) = boot(&disc, &Default::default()).expect("boot");
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let mut graphics = graphics(MovieFilter::Enhanced);
    let mut views = [view(&gpu, false)];
    let mut states = Vec::new();
    for n in 0..760u64 {
        assert!(session.frame().expect("frame"));
        if n == 450 || n == 730 {
            states.push(session.snapshot_bytes().unwrap());
        }
        let (display, scene) = take(&mut session, &mut views, &graphics);
        present(&gpu, &mut views[0], &display, &scene, n);
        if n > 450 {
            assert!(views[0].presented_movie(), "frame {n}: rebuilt");
        }
    }
    let shown = Picture::device(&mut session);
    let cases: [(&str, Box<dyn FnOnce(&mut Session<NativeModule>, &mut [GameView])>); 2] = [
        ("a state from 30 frames before", Box::new(|session: &mut Session<NativeModule>, _: &mut [GameView]| session.restore_bytes(&states[1]).unwrap())),
        ("a state from 310 frames before", Box::new(|session: &mut Session<NativeModule>, _: &mut [GameView]| session.restore_bytes(&states[0]).unwrap())),
    ];
    for (case, restart) in cases {
        let (waited, picture, device) = resumes(&gpu, &mut session, &mut views, &graphics, restart);
        let (error, psnr, ..) = compare(&picture, &device);
        eprintln!("{case}: rebuilt after {waited} frames, mean error {error:.2} ({psnr:.1} dB) against the restored picture");
        assert!(error <= 6.0, "{case}: the rebuilt picture is the restored one");
        assert!(device.mean_error(&shown) > 1.0, "{case}: the restored picture differs from the one before");
        assert!(waited <= 8, "{case}: rebuilt again within two movie frames ({waited})");
    }
    // A new internal scale restarts the trace.
    graphics.internal_scale = 3;
    let (waited, ..) = resumes(&gpu, &mut session, &mut views, &graphics, |_, _| {});
    eprintln!("a new internal scale: rebuilt after {waited} frames");
    assert!(waited <= 8, "rebuilt again within two movie frames of a restart ({waited})");
    // A view attached while the movie plays (its own new trace).
    let mut attached = [view(&gpu, false)];
    let (waited, ..) = resumes(&gpu, &mut session, &mut attached, &graphics, |session, _| session.runtime.services().gpu.device_mut().set_trace(false));
    eprintln!("a view attached mid-movie: rebuilt after {waited} frames");
    assert!(waited <= 8, "rebuilt within two movie frames of attaching ({waited})");
}
