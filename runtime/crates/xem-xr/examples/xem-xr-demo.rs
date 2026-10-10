//! The stereo test scene and the Slint settings panel in an OpenXR session.
//!
//! One wgpu device (created through the runtime) serves the scene, the panel
//! and the swapchains. The scene's animation clock stands in for the
//! simulation: `--paused` freezes it while head tracking, view location and
//! rendering continue every frame. Each frame prints one line of
//! `key=value` fields (see `runtime/scripts/xr-smoke.sh`).
//!
//!     xem-xr-demo [--paused] [--exit-after-frames N] [--capture DIR [--capture-frame K]]

use glam::{Mat4, Quat, Vec3};
use std::cell::RefCell;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::time::{Duration, Instant};
use xem_render::{SceneRenderer, ViewTarget, Viewport, wgpu};
use xem_settings::{MemoryStore, SettingsService};
use xem_ui::{LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};
use xem_xr::{
    EyeTarget, Layer, PanelPlacement, Pose, SessionConfig, SessionEvent, SwapchainOp, Target,
    XrContext, XrSession,
};

/// Panel size in logical pixels and its scale factor.
const PANEL_LOGICAL: (f32, f32) = (440.0, 560.0);
const PANEL_SCALE: f32 = 2.0;
/// Panel width in metres.
const PANEL_WIDTH: f32 = 0.55;
/// Where the viewer's LOCAL origin stands in the test scene: 1.6 m above the
/// floor, 4.5 m in front of the cubes.
const VIEWER_IN_SCENE: Vec3 = Vec3::new(0.0, 1.6, 4.5);

struct Options {
    paused: bool,
    exit_after_frames: Option<u64>,
    capture: Option<PathBuf>,
    capture_frame: Option<u64>,
}

fn options() -> Result<Options, String> {
    let mut options = Options {
        paused: false,
        exit_after_frames: None,
        capture: None,
        capture_frame: None,
    };
    let mut args = std::env::args().skip(1);
    let number = |value: Option<String>, flag: &str| -> Result<u64, String> {
        let value = value.ok_or(format!("{flag} needs a number"))?;
        value
            .parse()
            .map_err(|_| format!("bad number {value:?} for {flag}"))
    };
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--paused" => options.paused = true,
            "--exit-after-frames" => {
                options.exit_after_frames = Some(number(args.next(), &arg)?);
            }
            "--capture" => {
                options.capture = Some(args.next().ok_or("--capture needs a directory")?.into());
            }
            "--capture-frame" => options.capture_frame = Some(number(args.next(), &arg)?),
            "--help" => {
                println!(
                    "xem-xr-demo [--paused] [--exit-after-frames N] [--capture DIR [--capture-frame K]]"
                );
                std::process::exit(0);
            }
            other => return Err(format!("unknown argument {other:?} (see --help)")),
        }
    }
    Ok(options)
}

fn main() {
    if let Err(message) = options().and_then(run) {
        eprintln!("xem-xr-demo: {message}");
        std::process::exit(1);
    }
}

fn run(options: Options) -> Result<(), String> {
    let context = XrContext::new(&()).map_err(|e| e.to_string())?;
    let caps = context.capabilities();
    println!(
        "xr runtime={:?} system={:?} gpu={:?} eye_size={}x{} max_layers={} blend={:?} \
         hand_tracking={} eye_gaze={}",
        caps.runtime,
        caps.system,
        caps.gpu,
        caps.eye_size.0,
        caps.eye_size.1,
        caps.max_layers,
        caps.blend_mode,
        caps.hand_tracking,
        caps.eye_gaze
    );
    let gpu = context.gpu().clone();

    let settings = Rc::new(RefCell::new(SettingsService::with_defaults(
        MemoryStore::default(),
    )));
    let panel_px = (
        (PANEL_LOGICAL.0 * PANEL_SCALE) as u32,
        (PANEL_LOGICAL.1 * PANEL_SCALE) as u32,
    );
    let mut ui = SettingsUi::new(
        SharedGpu {
            instance: gpu.instance.clone(),
            device: gpu.device.clone(),
            queue: gpu.queue.clone(),
        },
        settings,
        panel_px,
        PANEL_SCALE,
    )
    .map_err(|e| e.to_string())?;

    let mut session = context
        .create_session(&SessionConfig {
            panel: Some(panel_px),
            readback: options.capture.is_some(),
            ..Default::default()
        })
        .map_err(|e| e.to_string())?;
    println!(
        "xr session eye_format={:?} eye_size={:?} panel_size={:?}",
        session.eye_format(),
        session.eye_size(),
        session.panel_size()
    );
    let mut scene = SceneRenderer::new(&gpu.device, &gpu.queue, session.eye_format());
    let panel = PanelPlacement {
        pose: Pose {
            position: Vec3::new(0.45, -0.15, -1.0),
            orientation: Quat::from_rotation_y(-20f32.to_radians()),
        },
        size: (
            PANEL_WIDTH,
            PANEL_WIDTH * panel_px.1 as f32 / panel_px.0 as f32,
        ),
    };
    let scene_from_local = Mat4::from_translation(VIEWER_IN_SCENE);
    let local_from_scene = scene_from_local.inverse();
    let capture_frame = options
        .capture_frame
        .or(options.exit_after_frames.map(|n| n.saturating_sub(1)))
        .unwrap_or(30);

    let mut sim_time = 0f32;
    let mut frames = 0u64;
    let mut exit_requested = false;
    let mut pointer = PanelPointer::default();
    let mut panel_captured = false;
    let start = Instant::now();
    loop {
        for event in session.poll_events().map_err(|e| e.to_string())? {
            match event {
                SessionEvent::StateChanged { state, time } => {
                    println!("event state={state:?} time_ns={time}");
                }
                other => println!("event {other:?}"),
            }
        }
        if session.should_exit() {
            break;
        }
        if !session.is_running() {
            if start.elapsed() > Duration::from_secs(20) {
                return Err(format!(
                    "no running session after 20 s (state {:?})",
                    session.state()
                ));
            }
            std::thread::sleep(Duration::from_millis(5));
            continue;
        }
        let Some(frame) = session.begin_frame().map_err(|e| e.to_string())? else {
            continue;
        };
        // The simulation advances by whole display periods, never per eye.
        if !options.paused {
            sim_time += frame.predicted_display_period.as_nanos() as f32 * 1e-9;
        }
        let should_render = frame.should_render;
        let eyes = frame.eyes;
        let hit = pointer.update(&ui, &panel, panel_px, &frame.input.hands[1]);

        // Slint renders on the shared queue only when it asked to redraw; the
        // quad layer keeps showing the last released panel image otherwise.
        // Swapchain images may be updated whether or not this frame is shown.
        if ui.update().map_err(|e| e.to_string())? {
            let capture = options.capture.as_ref().filter(|_| !panel_captured);
            let mut readback = None;
            session
                .render_panel(|encoder, target| {
                    encoder.copy_texture_to_texture(
                        ui.texture().as_image_copy(),
                        target.as_image_copy(),
                        ui.texture().size(),
                    );
                    if capture.is_some() {
                        readback = Some(Readback::record(&gpu.device, encoder, target));
                    }
                })
                .map_err(|e| e.to_string())?;
            if let (Some(dir), Some(readback)) = (capture, readback) {
                readback.save(&gpu.device, &[dir.join("panel.png")])?;
                panel_captured = true;
            }
        }

        if should_render {
            let capture = options.capture.as_ref().filter(|_| frames == capture_frame);
            let mut readback = None;
            session
                .render_eyes(|encoder, target: &EyeTarget| {
                    let size = target.size;
                    let views = [0, 1].map(|eye| ViewTarget {
                        view: eyes[eye].view * local_from_scene,
                        proj: eyes[eye].proj,
                        target: target.views[eye],
                        viewport: Viewport::full(size.0, size.1),
                    });
                    scene.render_views(encoder, &views, sim_time);
                    if capture.is_some() {
                        readback = Some(Readback::record(&gpu.device, encoder, target.texture));
                    }
                })
                .map_err(|e| e.to_string())?;
            if let (Some(dir), Some(readback)) = (capture, readback) {
                readback.save(&gpu.device, &[dir.join("left.png"), dir.join("right.png")])?;
                println!("capture frame={frames} dir={}", dir.display());
            }
        }

        let ended = session.end_frame(Some(panel)).map_err(|e| e.to_string())?;
        log_frame(frames, &session, &ended, sim_time, hit);
        frames += 1;
        if options.exit_after_frames.is_some_and(|n| frames >= n) && !exit_requested {
            println!("request_exit frame={frames}");
            session.request_exit().map_err(|e| e.to_string())?;
            exit_requested = true;
        }
    }
    println!("exit frames={frames} state={:?}", session.state());
    drop(session);
    Ok(())
}

fn fmt_vec(v: Vec3) -> String {
    format!("{:.5},{:.5},{:.5}", v.x, v.y, v.z)
}

fn log_frame(
    index: u64,
    session: &XrSession,
    ended: &xem_xr::EndedFrame,
    sim_time: f32,
    hit: bool,
) {
    let frame = &ended.frame;
    let head = frame.head.map_or("-".into(), |pose| {
        let q = pose.orientation;
        format!(
            "{};{:.5},{:.5},{:.5},{:.5}",
            fmt_vec(pose.position),
            q.x,
            q.y,
            q.z,
            q.w
        )
    });
    let ops: Vec<String> = frame
        .ops
        .iter()
        .map(|op| {
            let (target, name, image) = match *op {
                SwapchainOp::Acquire { swapchain, image } => (swapchain, "acquire", Some(image)),
                SwapchainOp::Wait { swapchain } => (swapchain, "wait", None),
                SwapchainOp::Submit { swapchain } => (swapchain, "submit", None),
                SwapchainOp::Release { swapchain } => (swapchain, "release", None),
            };
            let target = match target {
                Target::Eyes => "eyes",
                Target::Panel => "panel",
            };
            match image {
                Some(image) => format!("{target}.{name}{image}"),
                None => format!("{target}.{name}"),
            }
        })
        .collect();
    let layers: Vec<&str> = ended
        .layers
        .iter()
        .map(|layer| match layer {
            Layer::Projection => "projection",
            Layer::Quad => "quad",
        })
        .collect();
    let hands: Vec<String> = frame
        .input
        .hands
        .iter()
        .map(|hand| {
            format!(
                "{}{}{}",
                if hand.active { "A" } else { "-" },
                if hand.aim.is_some() { "P" } else { "-" },
                if hand.select { "S" } else { "-" }
            )
        })
        .collect();
    let [left, right] = frame.eyes;
    println!(
        "frame n={index} state={:?} pdt_ns={} period_ns={} render={} views_valid={} sim_time={:.6} \
         head={head} eye_l={} eye_r={} ipd={:.5} fov_l={:.4},{:.4},{:.4},{:.4} ops={} layers={} \
         hands={} profiles={:?} gaze={} joints={} panel_hit={}",
        session.state(),
        frame.predicted_display_time.as_nanos(),
        frame.predicted_display_period.as_nanos(),
        u8::from(frame.should_render),
        u8::from(frame.views_valid),
        sim_time,
        fmt_vec(left.pose.position),
        fmt_vec(right.pose.position),
        left.pose.position.distance(right.pose.position),
        left.fov.left,
        left.fov.right,
        left.fov.up,
        left.fov.down,
        if ops.is_empty() {
            "-".into()
        } else {
            ops.join(",")
        },
        if layers.is_empty() {
            "-".into()
        } else {
            layers.join(",")
        },
        hands.join("/"),
        frame.input.profiles,
        frame
            .input
            .gaze
            .map_or("-".into(), |pose| fmt_vec(pose.position)),
        frame
            .input
            .hand_joints
            .iter()
            .map(|hand| hand.as_ref().map_or(0, Vec::len).to_string())
            .collect::<Vec<_>>()
            .join("/"),
        u8::from(hit),
    );
}

/// Points at the panel with the right controller's aim ray: Slint receives
/// pointer moves where the ray meets the quad and select as the left button.
#[derive(Default)]
struct PanelPointer {
    pressed: bool,
}

impl PanelPointer {
    fn update(
        &mut self,
        ui: &SettingsUi,
        panel: &PanelPlacement,
        panel_px: (u32, u32),
        hand: &xem_xr::HandInput,
    ) -> bool {
        let Some(aim) = hand.aim else { return false };
        // The aim ray is -z of the aim pose; the quad lies in its pose's xy plane.
        let local_from_panel = panel.pose.matrix().inverse();
        let origin = local_from_panel.transform_point3(aim.position);
        let direction = local_from_panel.transform_vector3(aim.orientation * Vec3::NEG_Z);
        if direction.z.abs() < 1e-6 {
            return false;
        }
        let t = -origin.z / direction.z;
        let point = origin + direction * t;
        let (u, v) = (point.x / panel.size.0 + 0.5, 0.5 - point.y / panel.size.1);
        if t <= 0.0 || !(0.0..1.0).contains(&u) || !(0.0..1.0).contains(&v) {
            return false;
        }
        let scale = ui.scale_factor();
        let position =
            LogicalPosition::new(u * panel_px.0 as f32 / scale, v * panel_px.1 as f32 / scale);
        ui.dispatch(WindowEvent::PointerMoved { position });
        if hand.select != self.pressed {
            self.pressed = hand.select;
            let button = PointerEventButton::Left;
            ui.dispatch(if hand.select {
                WindowEvent::PointerPressed { position, button }
            } else {
                WindowEvent::PointerReleased { position, button }
            });
        }
        true
    }
}

/// A copy of a swapchain image's layers into a mappable buffer, recorded
/// before the image is released.
struct Readback {
    buffer: wgpu::Buffer,
    size: (u32, u32),
    layers: u32,
    padded: u32,
    bgra: bool,
}

impl Readback {
    fn record(
        device: &wgpu::Device,
        encoder: &mut wgpu::CommandEncoder,
        texture: &wgpu::Texture,
    ) -> Self {
        let extent = texture.size();
        let padded = (extent.width * 4).next_multiple_of(wgpu::COPY_BYTES_PER_ROW_ALIGNMENT);
        let buffer = device.create_buffer(&wgpu::BufferDescriptor {
            label: Some("xr readback"),
            size: u64::from(padded * extent.height * extent.depth_or_array_layers),
            usage: wgpu::BufferUsages::COPY_DST | wgpu::BufferUsages::MAP_READ,
            mapped_at_creation: false,
        });
        encoder.copy_texture_to_buffer(
            texture.as_image_copy(),
            wgpu::TexelCopyBufferInfo {
                buffer: &buffer,
                layout: wgpu::TexelCopyBufferLayout {
                    offset: 0,
                    bytes_per_row: Some(padded),
                    rows_per_image: Some(extent.height),
                },
            },
            extent,
        );
        Self {
            buffer,
            size: (extent.width, extent.height),
            layers: extent.depth_or_array_layers,
            padded,
            bgra: matches!(
                texture.format(),
                wgpu::TextureFormat::Bgra8Unorm | wgpu::TextureFormat::Bgra8UnormSrgb
            ),
        }
    }

    /// Waits for the copy and writes one PNG per layer (the bytes as stored:
    /// sRGB-encoded for the sRGB swapchains).
    fn save(self, device: &wgpu::Device, paths: &[PathBuf]) -> Result<(), String> {
        assert_eq!(paths.len(), self.layers as usize);
        self.buffer.map_async(wgpu::MapMode::Read, .., |result| {
            result.expect("map readback")
        });
        device
            .poll(wgpu::PollType::wait_indefinitely())
            .map_err(|e| e.to_string())?;
        let mapped = self
            .buffer
            .get_mapped_range(..)
            .map_err(|e| e.to_string())?;
        let (width, height) = self.size;
        let layer_bytes = (self.padded * height) as usize;
        for (layer, path) in paths.iter().enumerate() {
            let mut pixels = Vec::with_capacity((width * height * 4) as usize);
            let data = &mapped[layer * layer_bytes..(layer + 1) * layer_bytes];
            for row in data.chunks(self.padded as usize) {
                pixels.extend_from_slice(&row[..(width * 4) as usize]);
            }
            if self.bgra {
                pixels.chunks_exact_mut(4).for_each(|p| p.swap(0, 2));
            }
            write_png(path, width, height, &pixels)?;
        }
        Ok(())
    }
}

fn write_png(path: &Path, width: u32, height: u32, rgba: &[u8]) -> Result<(), String> {
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    let file = std::fs::File::create(path).map_err(|e| format!("{}: {e}", path.display()))?;
    let mut encoder = png::Encoder::new(std::io::BufWriter::new(file), width, height);
    encoder.set_color(png::ColorType::Rgba);
    encoder.set_depth(png::BitDepth::Eight);
    encoder
        .write_header()
        .and_then(|mut writer| writer.write_image_data(rgba))
        .map_err(|e| format!("{}: {e}", path.display()))
}
