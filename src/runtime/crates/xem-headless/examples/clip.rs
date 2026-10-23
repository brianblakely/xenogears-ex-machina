//! Records a video clip of the game through the hosts' renderer (`GameView`),
//! offscreen and frame-exact, with the game's own audio (local only: it
//! needs the user's discs and the game module).
//!
//!     cargo run --release -p xem-headless --example clip -- script.json
//!
//! The script: {"out": "clip.mp4", "disc"?, "cards"?, "size"?: [w, h],
//! "graphics"?: {Graphics fields}, "binaural"?: {Config}, "setup"?: [command],
//! "frames": N, "events"?: [[frame, command]], "crf"?, "audio"?: bool}.
//! Commands are the session's (`enter_field`, `press`, `hold`, `scenario`,
//! `debug`, ...) or the recorder's: `graphics` (fields to change),
//! `first_person` {on, yaw?, pitch?} (degrees), `turn` {yaw, pitch} (degrees
//! a frame from now on), `hide_dialogue` {on}, `speed` {factor} (game frames
//! a video frame; audio is silent above 1), `rewind` {on} (steps back as a
//! held Rewind does), `auto_jump` {on} (press Jump whenever assisted movement
//! waits at a takeoff), `skip` {frames} (run without recording), `mark`.

#[cfg(not(has_game_module))]
fn main() {
    eprintln!("this build has no game module (python3 tools/game_module.py builds it)");
    std::process::exit(1);
}

#[cfg(has_game_module)]
use serde_json::{Value, json};
#[cfg(has_game_module)]
use std::io::Write;
#[cfg(has_game_module)]
use xem_render::ps1::{GameView, HostClass, ViewConfig};
#[cfg(has_game_module)]
use xem_render::{Compositor, Gpu, Picture, Quad, Rect, present_rect, read_rgba8, wgpu};
#[cfg(has_game_module)]
use xem_settings::Graphics;

#[cfg(has_game_module)]
type Session = xem_core::control::Session<xem_game::NativeModule>;

#[cfg(has_game_module)]
const FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;

#[cfg(has_game_module)]
fn merge(value: &mut Value, change: &Value) {
    for (key, field) in change.as_object().expect("an object") {
        match (value.get_mut(key), field) {
            (Some(inner), Value::Object(_)) if inner.is_object() => merge(inner, field),
            _ => value[key] = field.clone(),
        }
    }
}

#[cfg(has_game_module)]
fn merged(graphics: &Graphics, change: &Value) -> Graphics {
    let mut value = serde_json::to_value(graphics).expect("graphics");
    let mut change = change.clone();
    change.as_object_mut().expect("an object").remove("cmd");
    merge(&mut value, &change);
    serde_json::from_value(value).unwrap_or_else(|e| panic!("graphics {change}: {e}"))
}

/// A setting moving from one value to another over some frames.
#[cfg(has_game_module)]
struct Ramp {
    field: String,
    from: f64,
    to: f64,
    start: u64,
    frames: u64,
}

/// An eye looking at the headset's diorama table from `distance` metres,
/// `yaw` degrees round from the seat's side and `pitch` up, turning
/// `yaw_rate` degrees a frame.
#[cfg(has_game_module)]
struct Orbit {
    yaw: f32,
    pitch: f32,
    distance: f32,
    yaw_rate: f32,
    fov: f32,
}

/// The pointer drawn over the picture: where it is and where it goes, in
/// game screen pixels.
#[cfg(has_game_module)]
#[derive(Default)]
struct Cursor {
    shown: bool,
    from: (f32, f32),
    to: (f32, f32),
    start: u64,
    frames: u64,
}

#[cfg(has_game_module)]
fn texture(gpu: &Gpu, size: (u32, u32), usage: wgpu::TextureUsages) -> wgpu::Texture {
    gpu.device.create_texture(&wgpu::TextureDescriptor {
        label: Some("clip"),
        size: wgpu::Extent3d { width: size.0, height: size.1, depth_or_array_layers: 1 },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: FORMAT,
        usage,
        view_formats: &[],
    })
}

#[cfg(has_game_module)]
struct Recorder {
    gpu: Gpu,
    view: GameView,
    compositor: Compositor,
    target: wgpu::Texture,
    size: (u32, u32),
    graphics: Graphics,
    display_image: Option<(wgpu::Texture, (u32, u32))>,
    first_person: bool,
    turn: (f32, f32),
    look: (f32, f32),
    speed: u32,
    rewinding: bool,
    rewind_wait: u32,
    /// Video frames drawn (setup's too): the presentation's clock, which
    /// never goes back.
    frame: u64,
    /// Video frames since the audio began (the recording's start).
    audio_frame: u64,
    ramps: Vec<Ramp>,
    cursor: Cursor,
    cursor_image: Option<(wgpu::Texture, (u32, u32))>,
    orbit: Option<Orbit>,
    /// Press Jump when assisted movement waits at a takeoff (the player's
    /// part of a route that needs a jump).
    auto_jump: bool,
}

#[cfg(has_game_module)]
impl Recorder {
    /// One recorder command, else the session's; true if it was the recorder's.
    fn command(&mut self, session: &mut Session, command: &Value) -> bool {
        let number = |name: &str| command.get(name).and_then(Value::as_f64);
        match command["cmd"].as_str().unwrap_or("") {
            "graphics" => self.graphics = merged(&self.graphics, command),
            "first_person" => {
                let on = command.get("on").and_then(Value::as_bool).unwrap_or(true);
                self.first_person = on;
                self.view.set_first_person(on);
                session.set_inspecting(on);
                self.look = (0.0, 0.0);
                if let (Some(yaw), Some(pitch)) = (number("yaw"), number("pitch")) {
                    self.look = (yaw as f32, pitch as f32);
                    self.view.look_around(self.look.0.to_radians(), self.look.1.to_radians());
                }
            }
            "turn" => self.turn = (number("yaw").unwrap_or(0.0) as f32, number("pitch").unwrap_or(0.0) as f32),
            "hide_dialogue" => self.view.set_dialogue_hidden(command.get("on").and_then(Value::as_bool).unwrap_or(true)),
            "speed" => self.speed = number("factor").unwrap_or(1.0).max(1.0) as u32,
            "rewind" => self.rewinding = command.get("on").and_then(Value::as_bool).unwrap_or(true),
            "auto_jump" => self.auto_jump = command.get("on").and_then(Value::as_bool).unwrap_or(true),
            "diorama" => {
                self.orbit = command.get("on").and_then(Value::as_bool).unwrap_or(true).then(|| Orbit {
                    yaw: number("yaw").unwrap_or(0.0) as f32,
                    pitch: number("pitch").unwrap_or(35.0) as f32,
                    distance: number("distance").unwrap_or(0.75) as f32,
                    yaw_rate: number("yaw_rate").unwrap_or(0.0) as f32,
                    fov: number("fov").unwrap_or(50.0) as f32,
                });
                if self.orbit.is_none() {
                    self.view.set_xr_view(xem_render::ps1::XrView::Screen);
                }
            }
            "ramp" => {
                let field = command["field"].as_str().expect("ramp field").to_owned();
                let current = serde_json::to_value(&self.graphics).expect("graphics")[&field].as_f64().expect("a number field");
                self.ramps.push(Ramp { field, from: number("from").unwrap_or(current), to: number("to").expect("ramp to"), start: self.frame, frames: number("frames").unwrap_or(60.0) as u64 });
            }
            "cursor" => {
                if command.get("hide").and_then(Value::as_bool).unwrap_or(false) {
                    self.cursor.shown = false;
                } else {
                    let at = |v: &Value| (v[0].as_f64().unwrap() as f32, v[1].as_f64().unwrap() as f32);
                    let now = self.cursor_at();
                    let to = at(&command["to"]);
                    let from = if self.cursor.shown { now } else { command.get("from").map_or(to, at) };
                    self.cursor = Cursor { shown: true, from, to, start: self.frame, frames: number("frames").unwrap_or(0.0) as u64 };
                }
            }
            "mark" => eprintln!("mark {} at frame {}", command.get("label").unwrap_or(&Value::Null), self.frame),
            _ => {
                let reply = session.execute(command);
                eprintln!("{} -> {}", command, truncate(&reply));
                return false;
            }
        }
        true
    }

    /// The cursor's place now (eased between its points).
    fn cursor_at(&self) -> (f32, f32) {
        let c = &self.cursor;
        let t = if c.frames == 0 { 1.0 } else { ((self.frame - c.start) as f32 / c.frames as f32).min(1.0) };
        let t = t * t * (3.0 - 2.0 * t);
        (c.from.0 + (c.to.0 - c.from.0) * t, c.from.1 + (c.to.1 - c.from.1) * t)
    }

    /// Runs the game for one video frame (several when fast-forwarding, a
    /// rewind step when rewinding) and draws it into the target.
    fn step(&mut self, session: &mut Session, audio: &mut Vec<i16>) {
        if !self.ramps.is_empty() {
            let mut value = serde_json::to_value(&self.graphics).expect("graphics");
            for ramp in &self.ramps {
                let t = ((self.frame - ramp.start) as f64 / ramp.frames.max(1) as f64).min(1.0);
                let t = t * t * (3.0 - 2.0 * t);
                value[&ramp.field] = json!((ramp.from + (ramp.to - ramp.from) * t).round() as i64);
            }
            self.graphics = serde_json::from_value(value).expect("ramped graphics");
            let frame = self.frame;
            self.ramps.retain(|r| frame < r.start + r.frames);
        }
        let mut samples = Vec::new();
        if self.rewinding {
            // A held rewind steps back every REWIND_STEP_MS.
            if self.rewind_wait == 0 {
                session.rewind_step().expect("rewind step");
                self.rewind_wait = (xem_core::rewind::REWIND_STEP_MS as f64 / (1000.0 / 60.0)).round().max(1.0) as u32;
            }
            self.rewind_wait -= 1;
        } else {
            for _ in 0..self.speed {
                let waiting = session.assist.as_ref().is_some_and(|a| a.phase == xem_core::assist::Phase::AwaitingJump);
                if self.auto_jump && waiting {
                    session.execute(&json!({"cmd": "press", "buttons": ["triangle"], "frames": 4}));
                }
                if self.graphics.actors_in_view {
                    session.set_actor_view(self.view.actor_view()).expect("actor view");
                }
                session.set_listener_view(self.view.listener_axes());
                session.frame().expect("frame");
                let spu = &mut session.runtime.services().spu;
                let start = samples.len();
                samples.resize(start + spu.audio_queued(), 0);
                spu.take_audio(&mut samples[start..]);
            }
        }
        // 44 100 Hz over the NTSC field rate: the video's 60000/1001.
        let wanted = (((self.audio_frame + 1) as f64 * 44_100.0 * 1001.0 / 60_000.0).round() as usize) * 2;
        if self.speed == 1 && !self.rewinding {
            audio.extend_from_slice(&samples);
        } else {
            // Silence for the frame's time.
            audio.resize(wanted, 0);
        }
        if self.first_person && self.turn != (0.0, 0.0) {
            // `look_around` turns from where the view looks now.
            let pitch = (self.look.1 + self.turn.1).clamp(-60.0, 60.0);
            self.view.look_around(self.turn.0.to_radians(), (pitch - self.look.1).to_radians());
            self.look = (self.look.0 + self.turn.0, pitch);
        }
        self.draw(session);
        self.frame += 1;
        self.audio_frame += 1;
    }

    fn draw(&mut self, session: &mut Session) {
        let gpu = &self.gpu;
        let original = self.graphics.renderer == xem_settings::Renderer::Original;
        let scene = session.scene();
        let mut encoder = gpu.device.create_command_encoder(&Default::default());
        let target_view = self.target.create_view(&Default::default());
        let device = session.runtime.services().gpu.device_mut();
        let display = device.display();
        let mut presented = None;
        let mut diorama: Option<wgpu::TextureView> = None;
        if original {
            if device.trace_enabled() {
                device.set_trace(false);
            }
            self.view.away();
        } else {
            let mut config = ViewConfig::resolve(&self.graphics, HostClass::Desktop, self.size, self.view.max_scale());
            let placed = present_rect((display.width, display.height), self.size, xem_settings::Scale::Fit);
            config.movies = config.movies.map(|_| (placed.width.round() as u32, placed.height.round() as u32));
            self.view.configure(config);
            let restart = self.view.take_restart();
            let device = session.runtime.services().gpu.device_mut();
            if restart || !device.trace_enabled() {
                device.set_trace(true);
            }
            device.set_reveals(self.view.reveals());
            let trace = device.take_trace();
            self.view.push(trace);
            let now = self.frame as f64 * 1001.0 / 60_000.0;
            if let (Some(orbit), true) = (&mut self.orbit, GameView::shows(&display)) {
                // The headset's diorama, seen by one eye looking round it.
                use xem_render::glam::{Mat4, Vec3};
                let table = match xem_render::ps1::xr_view(&self.graphics) {
                    xem_render::ps1::XrView::Diorama(table) => table,
                    _ => xem_render::ps1::Table::default(),
                };
                self.view.set_xr_view(xem_render::ps1::XrView::Diorama(table));
                let centre = table.room_from_table(Mat4::IDENTITY).transform_point3(Vec3::ZERO);
                let (yaw, pitch) = (orbit.yaw.to_radians(), orbit.pitch.to_radians());
                let at = centre + Vec3::new(yaw.sin() * pitch.cos(), pitch.sin(), yaw.cos() * pitch.cos()) * orbit.distance;
                orbit.yaw += orbit.yaw_rate;
                let eye = xem_render::ps1::DioramaEye {
                    view: Mat4::look_at_rh(at, centre, Vec3::Y),
                    proj: Mat4::perspective_rh(orbit.fov.to_radians(), self.size.0 as f32 / self.size.1 as f32, 0.01, 20.0),
                    size: self.size,
                };
                if let Some(shown) = self.view.present_diorama(&mut encoder, &display, &scene, now, &[eye], Mat4::IDENTITY) {
                    if let Some(eye) = shown.eyes.first() {
                        diorama = Some((*eye).clone());
                    } else if let Some(frame) = shown.frame {
                        presented = Some((frame.view.clone(), frame.size));
                    }
                }
            } else if GameView::shows(&display) || self.view.shows_movie(&display) {
                presented = self.view.present(&mut encoder, &display, &scene, now).map(|p| (p.view.clone(), p.size));
            } else {
                self.view.away();
            }
        }
        if let Some(eye) = &diorama {
            // Premultiplied, transparent round the table: over black.
            let mut quad = Quad::screen(eye, xem_render::Rect { x: 0.0, y: 0.0, width: self.size.0 as f32, height: self.size.1 as f32 }, self.size);
            quad.srgb_encoded = true;
            self.compositor.draw(&mut encoder, &target_view, Some(wgpu::Color::BLACK), &[quad]);
            gpu.queue.submit([encoder.finish()]);
            return;
        }
        let (picture, size, encoded) = match presented {
            Some((view, size)) => (view, size, true),
            None => {
                let (width, height, rgba) = session.runtime.services().gpu.display_rgba8();
                if self.display_image.as_ref().is_none_or(|(_, size)| *size != (width, height)) {
                    let usage = wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST;
                    self.display_image = Some((texture(gpu, (width.max(1), height.max(1)), usage), (width, height)));
                }
                let (image, _) = self.display_image.as_ref().expect("made above");
                if width > 0 && height > 0 {
                    gpu.queue.write_texture(
                        image.as_image_copy(),
                        &rgba,
                        wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(width * 4), rows_per_image: Some(height) },
                        wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
                    );
                }
                (image.create_view(&Default::default()), (width.max(1), height.max(1)), false)
            }
        };
        let placed = present_rect(size, self.size, xem_settings::Scale::Fit);
        let mut quad = Quad::screen(&picture, placed, self.size);
        quad.srgb_encoded = encoded;
        quad.opaque = true;
        quad.filter = if encoded { wgpu::FilterMode::Linear } else { wgpu::FilterMode::Nearest };
        let mut quads = vec![quad];
        let cursor_view;
        if let (true, Some((image, (w, h)))) = (self.cursor.shown, &self.cursor_image) {
            let side = if encoded && !self.view.presented_movie() { self.view.config().map_or(0, |c| c.side) as f32 } else { 0.0 };
            let mapping = Picture { placed, display: (display.width as f32, display.height as f32), side };
            let (x, y) = mapping.from_screen(self.cursor_at());
            cursor_view = image.create_view(&Default::default());
            let mut pointer = Quad::screen(&cursor_view, Rect { x: x.round(), y: y.round(), width: *w as f32, height: *h as f32 }, self.size);
            pointer.filter = wgpu::FilterMode::Linear;
            quads.push(pointer);
        }
        self.compositor.draw(&mut encoder, &target_view, Some(wgpu::Color::BLACK), &quads);
        gpu.queue.submit([encoder.finish()]);
    }
}

#[cfg(has_game_module)]
fn truncate(value: &Value) -> String {
    let text = value.to_string();
    let limit = if std::env::var_os("CLIP_FULL_REPLIES").is_some() { 20_000 } else { 300 };
    if text.len() > limit { format!("{}...", &text[..limit]) } else { text }
}

#[cfg(has_game_module)]
fn wav(samples: &[i16]) -> Vec<u8> {
    let data = samples.len() as u32 * 2;
    let mut out = Vec::with_capacity(44 + data as usize);
    out.extend_from_slice(b"RIFF");
    out.extend_from_slice(&(36 + data).to_le_bytes());
    out.extend_from_slice(b"WAVEfmt ");
    out.extend_from_slice(&16u32.to_le_bytes());
    out.extend_from_slice(&1u16.to_le_bytes());
    out.extend_from_slice(&2u16.to_le_bytes());
    out.extend_from_slice(&44_100u32.to_le_bytes());
    out.extend_from_slice(&(44_100u32 * 4).to_le_bytes());
    out.extend_from_slice(&4u16.to_le_bytes());
    out.extend_from_slice(&16u16.to_le_bytes());
    out.extend_from_slice(b"data");
    out.extend_from_slice(&data.to_le_bytes());
    for sample in samples {
        out.extend_from_slice(&sample.to_le_bytes());
    }
    out
}

#[cfg(has_game_module)]
fn main() {
    let path = std::env::args().nth(1).expect("usage: clip script.json");
    let script: Value = serde_json::from_str(&std::fs::read_to_string(&path).expect("script")).expect("script JSON");
    let root = std::path::PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../../..");
    let disc = script["disc"].as_str().map(std::path::PathBuf::from).unwrap_or(root.join("discs/Xenogears disc 1.chd"));
    let out = std::path::PathBuf::from(script["out"].as_str().expect("out"));
    let size = script.get("size").and_then(Value::as_array).map_or((1920, 1080), |s| (s[0].as_u64().unwrap() as u32, s[1].as_u64().unwrap() as u32));
    let boot = xem_game::BootOptions {
        cards: script["cards"].as_str().map(Into::into),
        audio_output: true,
        ..Default::default()
    };
    let (mut session, _) = xem_game::boot(&disc, &boot).expect("boot");
    session.set_sound_mode(1).expect("stereo");
    if let Some(config) = script.get("binaural").filter(|c| !c.is_null()) {
        session.set_binaural(serde_json::from_value(config.clone()).expect("binaural config"));
    }
    let gpu = pollster::block_on(Gpu::headless()).expect("a wgpu adapter");
    let usage = wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::COPY_SRC;
    let mut recorder = Recorder {
        view: GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter)),
        compositor: Compositor::new(&gpu.device, &gpu.queue, FORMAT),
        target: texture(&gpu, size, usage),
        gpu,
        size,
        graphics: merged(&Graphics::default(), script.get("graphics").unwrap_or(&json!({}))),
        display_image: None,
        first_person: false,
        turn: (0.0, 0.0),
        look: (0.0, 0.0),
        speed: 1,
        rewinding: false,
        rewind_wait: 0,
        frame: 0,
        audio_frame: 0,
        ramps: Vec::new(),
        cursor: Cursor::default(),
        cursor_image: None,
        orbit: None,
        auto_jump: false,
    };
    if let Some(path) = script["cursor_image"].as_str() {
        // A pointer image (premultiplied for the compositor).
        let decoder = png::Decoder::new(std::io::BufReader::new(std::fs::File::open(path).expect("cursor image")));
        let mut reader = decoder.read_info().expect("cursor png");
        let mut rgba = vec![0; reader.output_buffer_size().expect("size")];
        let info = reader.next_frame(&mut rgba).expect("cursor frame");
        assert_eq!(info.color_type, png::ColorType::Rgba, "an RGBA cursor");
        for p in rgba.chunks_exact_mut(4) {
            let a = u16::from(p[3]);
            for c in &mut p[..3] {
                *c = (u16::from(*c) * a / 255) as u8;
            }
        }
        let size = (info.width, info.height);
        let image = texture(&recorder.gpu, size, wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST);
        recorder.gpu.queue.write_texture(
            image.as_image_copy(),
            &rgba[..info.buffer_size()],
            wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(size.0 * 4), rows_per_image: Some(size.1) },
            wgpu::Extent3d { width: size.0, height: size.1, depth_or_array_layers: 1 },
        );
        recorder.cursor_image = Some((image, size));
    }
    let mut scratch = Vec::new();
    for command in script["setup"].as_array().cloned().unwrap_or_default() {
        if command["cmd"] == "skip" {
            // Run (and draw, so the renderer's history follows) unrecorded.
            for _ in 0..command["frames"].as_u64().unwrap_or(0) {
                recorder.step(&mut session, &mut scratch);
            }
        } else {
            recorder.command(&mut session, &command);
        }
    }
    recorder.audio_frame = 0;
    let frames = script["frames"].as_u64().expect("frames");
    // Frames recorded but left out of the clip (events keep their numbers).
    let head = script["head"].as_u64().unwrap_or(0);
    let mut events: Vec<(u64, Value)> = script["events"].as_array().cloned().unwrap_or_default().into_iter()
        .map(|e| (e[0].as_u64().expect("event frame"), e[1].clone())).collect();
    events.sort_by_key(|(at, _)| *at);
    let video = out.with_extension("video.mkv");
    let crf = script["crf"].as_u64().unwrap_or(14).to_string();
    let mut ffmpeg = std::process::Command::new("ffmpeg")
        .args(["-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba", "-s", &format!("{}x{}", size.0, size.1),
            "-framerate", "60000/1001", "-i", "-", "-c:v", "libx264", "-preset", "medium", "-crf", &crf, "-pix_fmt", "yuv420p", "-threads", "4"])
        .arg(&video)
        .stdin(std::process::Stdio::piped())
        .spawn()
        .expect("ffmpeg");
    let mut pipe = ffmpeg.stdin.take().expect("ffmpeg stdin");
    let mut audio = Vec::new();
    let mut next = 0;
    for frame in 0..frames {
        while next < events.len() && events[next].0 <= frame {
            let command = events[next].1.clone();
            if command["cmd"] == "skip" {
                for _ in 0..command["frames"].as_u64().unwrap_or(0) {
                    recorder.step(&mut session, &mut scratch);
                }
                recorder.audio_frame = frame;
            } else {
                recorder.command(&mut session, &command);
            }
            next += 1;
        }
        recorder.step(&mut session, &mut audio);
        if frame >= head {
            let rgba = read_rgba8(&recorder.gpu.device, &recorder.gpu.queue, &recorder.target);
            pipe.write_all(&rgba).expect("ffmpeg write");
        }
        if frame % 300 == 0 {
            eprintln!("frame {frame}/{frames}");
        }
    }
    drop(pipe);
    assert!(ffmpeg.wait().expect("ffmpeg").success(), "ffmpeg failed");
    let with_audio = script["audio"].as_bool().unwrap_or(true);
    if with_audio {
        let sound = out.with_extension("wav");
        let dropped = ((head as f64 * 44_100.0 * 1001.0 / 60_000.0).round() as usize * 2).min(audio.len());
        std::fs::write(&sound, wav(&audio[dropped..])).expect("wav");
        let status = std::process::Command::new("ffmpeg")
            .args(["-y", "-loglevel", "error", "-i"]).arg(&video).arg("-i").arg(&sound)
            .args(["-c:v", "copy", "-c:a", "aac", "-b:a", "256k", "-shortest"]).arg(&out)
            .status().expect("ffmpeg mux");
        assert!(status.success(), "mux failed");
        std::fs::remove_file(&sound).ok();
    } else {
        let status = std::process::Command::new("ffmpeg")
            .args(["-y", "-loglevel", "error", "-i"]).arg(&video).args(["-c:v", "copy"]).arg(&out)
            .status().expect("ffmpeg remux");
        assert!(status.success(), "remux failed");
    }
    std::fs::remove_file(&video).ok();
    println!("{} ({frames} frames, {}x{})", out.display(), size.0, size.1);
}
