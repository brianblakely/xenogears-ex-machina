//! A contact sheet of the Seraph Glass widgets for review, over a dark and
//! a light backdrop: `cargo run -p xem-render --example glass [OUT.png]`
//! (default `build/validation/glass/sheet.png`).
use xem_render::glass::{Dir, Icon, Reach, State, Style, Widget, draw};

fn main() {
    let out = std::env::args().nth(1).unwrap_or_else(|| {
        let dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../../build/validation/glass");
        std::fs::create_dir_all(&dir).expect("output directory");
        dir.join("sheet.png").to_string_lossy().into_owned()
    });
    let size = 128u32;
    let widgets = [
        Widget::Cursor,
        Widget::Floor(Reach::Walk),
        Widget::Floor(Reach::Jump),
        Widget::Floor(Reach::Invalid),
        Widget::Destination,
        Widget::Pages { count: 2, current: 1 },
        Widget::Button(Icon::Arrow(Dir::Up), State::Normal),
        Widget::Button(Icon::Cross, State::Hover),
        Widget::Button(Icon::Circle, State::Pressed),
        Widget::Button(Icon::Triangle, State::Active),
        Widget::Button(Icon::Square, State::Disabled),
        Widget::Key(Icon::Text("ALL".into()), State::Normal),
        Widget::Key(Icon::Text("NONE".into()), State::Hover),
        Widget::Key(Icon::Plus, State::Normal),
        Widget::Key(Icon::Minus, State::Pressed),
        Widget::Key(Icon::Text("JUMP".into()), State::Active),
    ];
    let columns = 8u32;
    let rows = (widgets.len() as u32).div_ceil(columns) * 3;
    let (width, height) = (columns * size, rows * size);
    let mut sheet = vec![0u8; (width * height * 4) as usize];
    for (band, (backdrop, style)) in [([18u8, 20, 26], Style::default()), ([150, 160, 150], Style::default()), ([18, 20, 26], Style { reduced_transparency: true })].into_iter().enumerate() {
        for (i, widget) in widgets.iter().enumerate() {
            let image = draw(widget, size, style);
            let (cx, cy) = ((i as u32 % columns) * size, (band as u32 * 2 + i as u32 / columns) * size);
            for y in 0..size {
                for x in 0..size {
                    let s = &image[((y * size + x) * 4) as usize..][..4];
                    let a = f32::from(s[3]) / 255.0;
                    let d = (((cy + y) * width + cx + x) * 4) as usize;
                    for k in 0..3 {
                        // Over the backdrop (with a faint checker for the glass to show).
                        let check = if ((x / 16) + (y / 16)) % 2 == 0 { 0 } else { 18 };
                        let b = f32::from(backdrop[k].saturating_add(check));
                        sheet[d + k] = (f32::from(s[k]) * a + b * (1.0 - a)).round() as u8;
                    }
                    sheet[d + 3] = 255;
                }
            }
        }
    }
    let file = std::fs::File::create(&out).expect("sheet file");
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder.write_header().expect("header").write_image_data(&sheet).expect("pixels");
    println!("{out}");
}
