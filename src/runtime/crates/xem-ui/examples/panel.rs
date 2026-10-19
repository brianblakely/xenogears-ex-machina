//! Pictures of the settings panel for review, through the custom platform
//! on a headless device (build/validation/panel/ by default: ignored):
//!
//!     cargo run -p xem-ui --example panel -- [--size WxH] [--scale F] [--out DIR] [--name NAME]
//!         [--section N] [--click X,Y]... [--tab N] [--hover X,Y] [--desktop] [--import-disc] [--error TEXT]
//!         [--locate LABEL]...
//!
//! One picture per section (`NAME-<index>.png`; only section N with
//! `--section`), composited over a dark grey as hosts composite it over the
//! game. A tall size shows a whole section; `--click` presses at logical
//! positions in turn (Quit's confirmation, say), `--tab` then presses Tab
//! N times (focus rings) and `--hover` leaves the pointer at one, for hover
//! states; `--desktop` offers Quit and Fullscreen
//! as the desktop host does, `--import-disc` the disc card as the browser
//! and Android hosts do, `--error` the error bar saying TEXT; `--locate`
//! prints where the elements of an accessible label lie (logical x, y,
//! width, height), for tests that tap the panel.

use std::cell::RefCell;
use std::path::PathBuf;
use std::rc::Rc;
use xem_render::{Gpu, read_rgba8};
use xem_settings::{MemoryStore, SettingsService};
use xem_ui::{SettingsUi, SharedGpu};

fn main() {
    let mut size = (640u32, 560u32);
    let mut scale = 1.0f32;
    let mut name = String::from("panel");
    let mut only: Option<i32> = None;
    let mut hover: Option<(f32, f32)> = None;
    let mut clicks: Vec<(f32, f32)> = Vec::new();
    let mut tabs = 0;
    let mut desktop = false;
    let mut import_disc = false;
    let mut error = String::new();
    let mut locate: Vec<String> = Vec::new();
    let mut out = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../../build/validation/panel");
    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        let mut value = || args.next().expect("a value");
        match arg.as_str() {
            "--size" => {
                let v = value();
                let (w, h) = v.split_once('x').expect("WxH");
                size = (w.parse().unwrap(), h.parse().unwrap());
            }
            "--scale" => scale = value().parse().unwrap(),
            "--name" => name = value(),
            "--section" => only = Some(value().parse().unwrap()),
            "--desktop" => desktop = true,
            "--import-disc" => import_disc = true,
            "--error" => error = value(),
            "--locate" => locate.push(value()),
            "--hover" => hover = Some(position(&value())),
            "--click" => clicks.push(position(&value())),
            "--tab" => tabs = value().parse().unwrap(),
            "--out" => out = value().into(),
            other => panic!("unknown option {other}"),
        }
    }
    std::fs::create_dir_all(&out).unwrap();
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(MemoryStore::default())));
    let shared = SharedGpu { instance: gpu.instance.clone(), device: gpu.device.clone(), queue: gpu.queue.clone() };
    let mut ui = SettingsUi::new(shared, settings, size, scale).expect("the panel");
    ui.set_operation_error(&error);
    ui.on_app_request(|request| eprintln!("{request:?}"));
    if desktop {
        ui.on_quit(|| eprintln!("quit"));
        ui.set_fullscreen_offered(true);
    }
    if import_disc {
        ui.on_import_disc(|| {});
    }
    let sections = ui.component().get_section_count();
    for section in (0..sections).filter(|section| only.is_none_or(|only| only == *section)) {
        ui.component().set_selected_section(section);
        ui.cancel_dialog();
        for &(x, y) in &clicks {
            ui.update().unwrap();
            let position = xem_ui::LogicalPosition::new(x, y);
            let button = xem_ui::PointerEventButton::Left;
            ui.dispatch(xem_ui::WindowEvent::PointerMoved { position });
            ui.dispatch(xem_ui::WindowEvent::PointerPressed { position, button });
            ui.dispatch(xem_ui::WindowEvent::PointerReleased { position, button });
        }
        for _ in 0..tabs {
            ui.update().unwrap();
            let text: xem_ui::SharedString = xem_ui::Key::Tab.into();
            ui.dispatch(xem_ui::WindowEvent::KeyPressed { text: text.clone() });
            ui.dispatch(xem_ui::WindowEvent::KeyReleased { text });
        }
        if let Some((x, y)) = hover {
            ui.update().unwrap();
            ui.dispatch(xem_ui::WindowEvent::PointerMoved { position: xem_ui::LogicalPosition::new(x, y) });
        }
        // Rows made as the section shows, its animations run out, then drawn.
        ui.update().unwrap();
        std::thread::sleep(std::time::Duration::from_millis(250));
        ui.update().unwrap();
        for label in &locate {
            for element in i_slint_backend_testing::ElementHandle::find_by_accessible_label(ui.component(), label) {
                let (at, size) = (element.absolute_position(), element.size());
                println!("section {section} {label:?} {:?}: {} {} {} {}", element.accessible_role(), at.x, at.y, size.width, size.height);
            }
        }
        let pixels = read_rgba8(&gpu.device, &gpu.queue, ui.texture());
        // Premultiplied over #202226.
        let back = [0x20u16, 0x22, 0x26];
        let rgb: Vec<u8> = pixels
            .chunks_exact(4)
            .flat_map(|p| {
                let a = u16::from(p[3]);
                (0..3).map(move |c| (u16::from(p[c]) + back[c] * (255 - a) / 255).min(255) as u8)
            })
            .collect();
        let path = out.join(format!("{name}-{section}.png"));
        let file = std::fs::File::create(&path).unwrap();
        let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), size.0, size.1);
        encoder.set_color(png::ColorType::Rgb);
        encoder.write_header().unwrap().write_image_data(&rgb).unwrap();
        eprintln!("{}", path.display());
    }
}

fn position(value: &str) -> (f32, f32) {
    let (x, y) = value.split_once(',').expect("X,Y");
    (x.parse().unwrap(), y.parse().unwrap())
}
