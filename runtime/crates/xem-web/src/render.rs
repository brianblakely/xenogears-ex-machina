//! wgpu on the page's canvas: WebGPU where the browser offers an adapter,
//! WebGL2 otherwise. One device serves the scene, the compositor and the Slint
//! settings panel, as on the desktop host.

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use web_sys::HtmlCanvasElement;
use xem_render::{Compositor, Gpu, Quad, Rect, SceneRenderer, ViewTarget, Viewport, demo_camera, present_rect, wgpu};
use xem_settings::{Filter, SettingsService};
use xem_ui::{LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};

/// The stand-in game picture's size (the original's typical display mode).
const GAME_SIZE: (u32, u32) = (320, 240);
const GAME_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
/// The panel's size in logical pixels.
const PANEL_SIZE: (f32, f32) = (440.0, 560.0);

pub struct WebRenderer {
    gpu: Gpu,
    surface: wgpu::Surface<'static>,
    config: wgpu::SurfaceConfiguration,
    scene: SceneRenderer,
    compositor: Compositor,
    game_view: wgpu::TextureView,
    pub backend: &'static str,
    pub adapter: String,
    scale: f32,
    panel: Option<SettingsUi>,
    pub panel_error: Option<String>,
    pub panel_visible: bool,
    pub frames: u64,
    /// Why the browser took the device away, once it has.
    lost: Arc<Mutex<Option<String>>>,
}

/// Create the renderer on `canvas`. `prefer` "webgl2" skips WebGPU.
pub async fn create(canvas: HtmlCanvasElement, prefer: &str) -> Result<WebRenderer, String> {
    let webgpu = prefer != "webgl2" && wgpu::util::is_browser_webgpu_supported().await;
    let mut descriptor = wgpu::InstanceDescriptor::new_without_display_handle();
    descriptor.backends = if webgpu { wgpu::Backends::BROWSER_WEBGPU } else { wgpu::Backends::GL };
    let instance = wgpu::Instance::new(descriptor);
    let (width, height) = (canvas.width().max(1), canvas.height().max(1));
    let surface = instance
        .create_surface(wgpu::SurfaceTarget::Canvas(canvas))
        .map_err(|e| format!("canvas surface: {e}"))?;
    let gpu = Gpu::new(instance, Some(&surface)).await.map_err(|e| e.to_string())?;
    let lost = Arc::new(Mutex::new(None));
    let on_lost = lost.clone();
    gpu.device.set_device_lost_callback(move |reason, message| {
        *on_lost.lock().unwrap() = Some(format!("{reason:?}: {message}"));
    });
    let info = gpu.adapter.get_info();
    let capabilities = surface.get_capabilities(&gpu.adapter);
    // As on the desktop: composite in sRGB-encoded values into a linear format.
    let format = capabilities
        .formats
        .iter()
        .copied()
        .find(|format| !format.is_srgb())
        .unwrap_or(capabilities.formats[0]);
    let config = wgpu::SurfaceConfiguration {
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
        format,
        width,
        height,
        present_mode: wgpu::PresentMode::Fifo,
        desired_maximum_frame_latency: 2,
        alpha_mode: capabilities.alpha_modes[0],
        color_space: wgpu::SurfaceColorSpace::Auto,
        view_formats: vec![],
    };
    surface.configure(&gpu.device, &config);
    let game_view = gpu
        .device
        .create_texture(&wgpu::TextureDescriptor {
            label: Some("game image"),
            size: wgpu::Extent3d { width: GAME_SIZE.0, height: GAME_SIZE.1, depth_or_array_layers: 1 },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: GAME_FORMAT,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        })
        .create_view(&Default::default());
    Ok(WebRenderer {
        scene: SceneRenderer::new(&gpu.device, &gpu.queue, GAME_FORMAT),
        compositor: Compositor::new(&gpu.device, &gpu.queue, format),
        backend: if webgpu { "webgpu" } else { "webgl2" },
        adapter: format!("{} ({:?}, {})", info.name, info.backend, info.driver),
        gpu,
        surface,
        config,
        game_view,
        scale: 1.0,
        panel: None,
        panel_error: None,
        panel_visible: false,
        frames: 0,
        lost,
    })
}

impl WebRenderer {
    /// Create the Slint settings panel on this renderer's device.
    pub fn attach_panel(&mut self, settings: Rc<RefCell<SettingsService>>, on_close: impl FnMut() + 'static) {
        let shared = SharedGpu {
            instance: self.gpu.instance.clone(),
            device: self.gpu.device.clone(),
            queue: self.gpu.queue.clone(),
        };
        let size = panel_size(self.config.width, self.config.height, self.scale);
        match SettingsUi::new(shared, settings, size, self.scale) {
            Ok(panel) => {
                panel.on_close(on_close);
                self.panel = Some(panel);
            }
            Err(error) => self.panel_error = Some(error.to_string()),
        }
    }

    /// Why the device was lost; the page then makes a new renderer.
    pub fn lost(&self) -> Option<String> {
        self.lost.lock().unwrap().clone()
    }

    pub fn has_panel(&self) -> bool {
        self.panel.is_some()
    }

    pub fn size(&self) -> (u32, u32) {
        (self.config.width, self.config.height)
    }

    /// The canvas changed to `width` x `height` pixels at `scale` pixels per CSS pixel.
    pub fn resize(&mut self, width: u32, height: u32, scale: f32) {
        if width == 0 || height == 0 {
            return;
        }
        self.scale = scale;
        if (width, height) != self.size() {
            self.config.width = width;
            self.config.height = height;
            self.surface.configure(&self.gpu.device, &self.config);
        }
        if let Some(panel) = &mut self.panel {
            panel.resize(panel_size(width, height, scale), scale);
        }
    }

    fn panel_rect(&self) -> Option<Rect> {
        let (width, height) = self.panel.as_ref()?.size();
        Some(Rect {
            x: (self.config.width.saturating_sub(width) / 2) as f32,
            y: (self.config.height.saturating_sub(height) / 2) as f32,
            width: width as f32,
            height: height as f32,
        })
    }

    /// Forward pointer input (canvas pixels) to the visible panel; returns
    /// whether the panel took it.
    pub fn pointer(&mut self, kind: &str, x: f32, y: f32, button: i32, delta: (f32, f32)) -> bool {
        let (Some(rect), true) = (self.panel_rect(), self.panel_visible) else { return false };
        let Some(panel) = &self.panel else { return false };
        let scale = panel.scale_factor();
        let position = LogicalPosition::new((x - rect.x) / scale, (y - rect.y) / scale);
        let button = match button {
            0 => PointerEventButton::Left,
            1 => PointerEventButton::Middle,
            2 => PointerEventButton::Right,
            _ => PointerEventButton::Other,
        };
        panel.dispatch(match kind {
            "down" => WindowEvent::PointerPressed { position, button },
            "up" => WindowEvent::PointerReleased { position, button },
            "wheel" => WindowEvent::PointerScrolled { position, delta_x: -delta.0, delta_y: -delta.1 },
            "leave" => WindowEvent::PointerExited,
            _ => WindowEvent::PointerMoved { position },
        });
        rect.contains(x, y)
    }

    /// Forward a key the panel handles (Slint's key text) while it is visible.
    pub fn key(&mut self, text: &str, down: bool) {
        if let (Some(panel), true) = (&self.panel, self.panel_visible) {
            let text = text.into();
            panel.dispatch(if down { WindowEvent::KeyPressed { text } } else { WindowEvent::KeyReleased { text } });
        }
    }

    /// Render one frame of the scene at `time` seconds with the panel on top.
    pub fn frame(&mut self, time: f32, settings: &SettingsService) -> Result<(), String> {
        if self.lost().is_some() {
            return Ok(());
        }
        if self.panel_visible
            && let Some(panel) = &mut self.panel
        {
            panel.update().map_err(|error| error.to_string())?;
        }
        let frame = match self.surface.get_current_texture() {
            wgpu::CurrentSurfaceTexture::Success(frame) | wgpu::CurrentSurfaceTexture::Suboptimal(frame) => frame,
            wgpu::CurrentSurfaceTexture::Timeout | wgpu::CurrentSurfaceTexture::Occluded => return Ok(()),
            wgpu::CurrentSurfaceTexture::Outdated | wgpu::CurrentSurfaceTexture::Lost => {
                self.surface.configure(&self.gpu.device, &self.config);
                return Ok(());
            }
            wgpu::CurrentSurfaceTexture::Validation => return Err("surface validation error".into()),
        };
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        let (view, proj) = demo_camera(GAME_SIZE.0 as f32 / GAME_SIZE.1 as f32, time);
        self.scene.render_views(
            &mut encoder,
            &[ViewTarget { view, proj, target: &self.game_view, viewport: Viewport::full(GAME_SIZE.0, GAME_SIZE.1) }],
            time,
        );
        let target = self.size();
        let presentation = &settings.settings().presentation;
        let mut game = Quad::screen(&self.game_view, present_rect(GAME_SIZE, target, presentation.scale), target);
        game.filter = match presentation.filter {
            Filter::Nearest => wgpu::FilterMode::Nearest,
            Filter::Linear => wgpu::FilterMode::Linear,
        };
        let mut quads = vec![game];
        if let (true, Some(panel), Some(rect)) = (self.panel_visible, &self.panel, self.panel_rect()) {
            let mut quad = Quad::screen(panel.view(), rect, target);
            quad.srgb_encoded = true;
            quad.filter = wgpu::FilterMode::Nearest;
            quads.push(quad);
        }
        let output = frame.texture.create_view(&Default::default());
        self.compositor.draw(&mut encoder, &output, Some(wgpu::Color::BLACK), &quads);
        drop(quads);
        self.gpu.queue.submit([encoder.finish()]);
        self.gpu.queue.present(frame);
        self.frames += 1;
        Ok(())
    }
}

/// The panel's pixel size for a canvas of `width` x `height` pixels.
fn panel_size(width: u32, height: u32, scale: f32) -> (u32, u32) {
    let fit = |logical: f32, available: u32| ((logical * scale) as u32).min(available.saturating_sub(16)).max(1);
    (fit(PANEL_SIZE.0, width), fit(PANEL_SIZE.1, height))
}
