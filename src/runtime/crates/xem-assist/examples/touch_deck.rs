//! A contact sheet of the touch decks for review: each of the game's
//! contexts with its deck over a stand-in picture, on a 20:9 phone
//! (2400x1080 at 3 pixels a dp) with the picture in its middle and with the
//! picture filling it, drawn as the hosts draw them (glass widgets
//! composited in encoded values):
//!
//! `cargo run -p xem-assist --example touch_deck [OUT_DIR] [--opacity PERCENT] [--solid]`
//!
//! writes `sheet.png` (every deck, a third of the size) and each deck at
//! full size (default `build/validation/touch-deck`).
use xem_assist::draw::{Lit, deck_widgets};
use xem_assist::touch::{Deck, Rect, Scene};
use xem_core::context::{Choice, Context};
use xem_render::glass::{Style, draw};

const TARGET: (u32, u32) = (2400, 1080);

/// The decks drawn: a name, what the game plays, and what the app's
/// toggles show lit.
fn scenes() -> Vec<(&'static str, Scene, Lit)> {
    let menu = |screen, quantities| Context::Menu { screen, cursor: 0, choice: 0, choices: 0, commands: true, open: false, switch: false, quantities };
    let battle = |attack, message| Context::Battle { page: if attack { 5 } else { 1 }, attack, ap: 6, list: false, targeting: false, message };
    let off = Lit::default();
    vec![
        ("walking", Context::Walk.into(), Lit { run: true, ..off }),
        ("dialogue", Context::Dialogue { choice: None }.into(), Lit { auto_advance: true, ..off }),
        ("choice", Context::Dialogue { choice: Some(Choice { count: 2, index: 0 }) }.into(), off),
        ("inspecting", Scene { inspecting: true, ..Context::Walk.into() }, Lit { first_person: true, ..off }),
        ("menu", menu(0, false).into(), off),
        ("shop", menu(4, true).into(), off),
        ("battle-ring", battle(false, false).into(), off),
        ("battle-attack", battle(true, false).into(), off),
        ("world-map", Context::WorldMap { movement: 1, scene: 0 }.into(), off),
        ("world-map-ship", Context::WorldMap { movement: 7, scene: 0 }.into(), off),
        ("bout", Context::Arena { state: 1 }.into(), off),
        ("activity", Context::Event { map: 321, buttons: 0xf1dc }.into(), off),
    ]
}

/// A stand-in for the game's picture: sky over ground, so the glass shows
/// over light and dark.
fn backdrop(canvas: &mut [u8], picture: Rect) {
    let (w, h) = TARGET;
    for y in 0..h {
        for x in 0..w {
            let at = ((y * w + x) * 4) as usize;
            let inside = picture.contains((x as f32, y as f32));
            let t = y as f32 / h as f32;
            let colour = if !inside {
                [0.0, 0.0, 0.0]
            } else if t < 0.55 {
                [0.36 + 0.3 * t, 0.52 + 0.3 * t, 0.72 + 0.2 * t]
            } else {
                let check = if ((x / 96) + (y / 96)) % 2 == 0 { 0.0 } else { 0.05 };
                [0.2 + check, 0.3 - 0.2 * (t - 0.55) + check, 0.16 + check]
            };
            for k in 0..3 {
                canvas[at + k] = (colour[k] * 255.0) as u8;
            }
            canvas[at + 3] = 255;
        }
    }
}

/// Draws `image` (`size` pixels square, straight alpha) over `canvas` in
/// `rect`, `opacity` opaque, sampled bilinearly.
fn composite(canvas: &mut [u8], image: &[u8], size: u32, rect: Rect, opacity: f32) {
    let (w, h) = TARGET;
    let texel = |x: i32, y: i32| -> [f32; 4] {
        let (x, y) = (x.clamp(0, size as i32 - 1) as u32, y.clamp(0, size as i32 - 1) as u32);
        let p = &image[((y * size + x) * 4) as usize..][..4];
        let a = f32::from(p[3]) / 255.0;
        [f32::from(p[0]) * a, f32::from(p[1]) * a, f32::from(p[2]) * a, a]
    };
    for y in rect.y.floor().max(0.0) as u32..(rect.y + rect.height).ceil().min(h as f32) as u32 {
        for x in rect.x.floor().max(0.0) as u32..(rect.x + rect.width).ceil().min(w as f32) as u32 {
            let u = (x as f32 + 0.5 - rect.x) / rect.width * size as f32 - 0.5;
            let v = (y as f32 + 0.5 - rect.y) / rect.height * size as f32 - 0.5;
            if u < -0.5 || v < -0.5 || u > size as f32 - 0.5 || v > size as f32 - 0.5 {
                continue;
            }
            let (x0, y0) = (u.floor() as i32, v.floor() as i32);
            let (fx, fy) = (u - x0 as f32, v - y0 as f32);
            let mut sample = [0.0f32; 4];
            for (dx, dy, weight) in [(0, 0, (1.0 - fx) * (1.0 - fy)), (1, 0, fx * (1.0 - fy)), (0, 1, (1.0 - fx) * fy), (1, 1, fx * fy)] {
                let t = texel(x0 + dx, y0 + dy);
                for k in 0..4 {
                    sample[k] += t[k] * weight;
                }
            }
            let at = ((y * w + x) * 4) as usize;
            let a = sample[3] * opacity;
            for k in 0..3 {
                canvas[at + k] = (sample[k] * opacity + f32::from(canvas[at + k]) * (1.0 - a)).round().clamp(0.0, 255.0) as u8;
            }
        }
    }
}

fn save(path: &std::path::Path, (width, height): (u32, u32), pixels: &[u8]) {
    let file = std::fs::File::create(path).expect("image file");
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder.write_header().expect("header").write_image_data(pixels).expect("pixels");
}

fn main() {
    let mut out = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../../build/validation/touch-deck");
    let (mut opacity, mut style) = (1.0, Style::default());
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--opacity" => opacity = args.next().and_then(|v| v.parse::<f32>().ok()).expect("--opacity PERCENT") / 100.0,
            "--solid" => style.reduced_transparency = true,
            path => out = path.into(),
        }
    }
    std::fs::create_dir_all(&out).expect("output directory");
    let (w, h) = TARGET;
    let pictures = [
        ("letterboxed", Rect { x: 480.0, y: 0.0, width: 1440.0, height: 1080.0 }),
        ("wide", Rect { x: 0.0, y: 0.0, width: w as f32, height: h as f32 }),
    ];
    let scenes = scenes();
    // The sheet: a third of the size, the letterboxed decks left, the wide right.
    let (tw, th) = (w / 3, h / 3);
    let (sheet_w, sheet_h) = (tw * 2 + 8, (th + 8) * scenes.len() as u32);
    let mut sheet = vec![40u8; (sheet_w * sheet_h * 4) as usize];
    for (row, (name, scene, lit)) in scenes.iter().enumerate() {
        for (column, (shape, picture)) in pictures.iter().enumerate() {
            let mut canvas = vec![0u8; (w * h * 4) as usize];
            backdrop(&mut canvas, *picture);
            let mut deck = Deck::default();
            deck.arrange((w as f32, h as f32), *picture, 3.0, *scene);
            for drawn in deck_widgets(&deck, *lit, opacity) {
                composite(&mut canvas, &draw(&drawn.widget, drawn.pixels, style), drawn.pixels, drawn.rect, drawn.opacity);
            }
            save(&out.join(format!("{name}-{shape}.png")), TARGET, &canvas);
            for y in 0..th {
                for x in 0..tw {
                    // A box filter of each 3x3 block.
                    let mut sum = [0u32; 3];
                    for (dx, dy) in (0..9).map(|i| (i % 3, i / 3)) {
                        let at = (((y * 3 + dy) * w + x * 3 + dx) * 4) as usize;
                        for k in 0..3 {
                            sum[k] += u32::from(canvas[at + k]);
                        }
                    }
                    let to = (((row as u32 * (th + 8) + y) * sheet_w + column as u32 * (tw + 8) + x) * 4) as usize;
                    for k in 0..3 {
                        sheet[to + k] = (sum[k] / 9) as u8;
                    }
                }
            }
        }
        println!("row {row}: {name}");
    }
    sheet.chunks_exact_mut(4).for_each(|p| p[3] = 255);
    save(&out.join("sheet.png"), (sheet_w, sheet_h), &sheet);
    println!("{}", out.join("sheet.png").display());
}
