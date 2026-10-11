//! wgpu on the page's canvas: WebGPU where the browser offers an adapter,
//! WebGL2 otherwise. One device serves the scene, the compositor and the Slint
//! settings panel, as on the desktop host.

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use web_sys::HtmlCanvasElement;
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_render::xem_gpu::{DisplayInfo, Trace};
use xem_render::{Compositor, Gpu, Quad, Rect, SceneRenderer, ViewTarget, Viewport, demo_camera, present_rect, wgpu};
use xem_settings::{Filter, SettingsService};
use xem_ui::{LogicalPosition, PointerEventButton, SettingsUi, SharedGpu, WindowEvent};

/// The test scene's size (the original's typical display mode), shown until a
/// game session runs.
const GAME_SIZE: (u32, u32) = (320, 240);
const GAME_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
/// The panel's size in logical pixels.
const PANEL_SIZE: (f32, f32) = (640.0, 560.0);

pub struct WebRenderer {
    gpu: Gpu,
    surface: wgpu::Surface<'static>,
    config: wgpu::SurfaceConfiguration,
    scene: SceneRenderer,
    compositor: Compositor,
    game_view: wgpu::TextureView,
    /// The game's display (texture, view, size) as the device holds it
    /// (24-bit movie frames).
    game_image: Option<(wgpu::Texture, wgpu::TextureView, (u32, u32))>,
    /// The game drawn by the renderer from its draw trace, and the display
    /// and scene to present.
    renderer: GameView,
    game_frame: Option<(DisplayInfo, String)>,
    pub backend: &'static str,
    pub adapter: String,
    scale: f32,
    panel: Option<SettingsUi>,
    pub frame_rate: Option<xem_ui::FrameRateUi>,
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
    let renderer = GameView::new(&gpu.device, &gpu.queue, Some(&gpu.adapter));
    Ok(WebRenderer {
        scene: SceneRenderer::new(&gpu.device, &gpu.queue, GAME_FORMAT),
        compositor: Compositor::new(&gpu.device, &gpu.queue, format),
        backend: if webgpu { "webgpu" } else { "webgl2" },
        adapter: format!("{} ({:?}, {})", info.name, info.backend, info.driver),
        gpu,
        surface,
        config,
        game_view,
        game_image: None,
        renderer,
        game_frame: None,
        scale: 1.0,
        panel: None,
        frame_rate: None,
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
        match SettingsUi::new(shared.clone(), settings, size, self.scale) {
            Ok(panel) => {
                panel.on_close(on_close);
                self.panel = Some(panel);
            }
            Err(error) => self.panel_error = Some(error.to_string()),
        }
        match xem_ui::FrameRateUi::new(shared,self.scale) {
            Ok(frame_rate) => self.frame_rate = Some(frame_rate),
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

    pub fn panel(&self) -> Option<&SettingsUi> { self.panel.as_ref() }

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

    /// Show the game's display: `rgba` is `width` x `height` RGBA8 pixels
    /// (sRGB-encoded, as the console outputs them), as xem-host's
    /// `View::set_game_image` does.
    pub fn set_game_image(&mut self, (width, height): (u32, u32), rgba: &[u8]) {
        if width == 0 || height == 0 {
            return;
        }
        if self.game_image.as_ref().is_none_or(|(_, _, size)| *size != (width, height)) {
            let texture = self.gpu.device.create_texture(&wgpu::TextureDescriptor {
                label: Some("game display"),
                size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: GAME_FORMAT,
                usage: wgpu::TextureUsages::TEXTURE_BINDING | wgpu::TextureUsages::COPY_DST,
                view_formats: &[],
            });
            let view = texture.create_view(&Default::default());
            self.game_image = Some((texture, view, (width, height)));
        }
        let (texture, _, _) = self.game_image.as_ref().expect("created above");
        self.gpu.queue.write_texture(
            texture.as_image_copy(),
            rgba,
            wgpu::TexelCopyBufferLayout { offset: 0, bytes_per_row: Some(width * 4), rows_per_image: Some(height) },
            wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
        );
    }

    /// Go back to the test scene (no session).
    pub fn clear_game_image(&mut self) {
        self.game_image = None;
        self.game_frame = None;
    }

    /// The renderer's configuration for `graphics` on this canvas; returns
    /// whether the device's draw trace must restart from the whole VRAM.
    pub fn configure_game(&mut self, graphics: &xem_settings::Graphics) -> bool {
        let config = ViewConfig::resolve(graphics, HostClass::Browser, self.config.height, self.renderer.max_scale());
        self.renderer.configure(config);
        self.renderer.take_restart()
    }

    /// What the game drew, its display and scene. A 24-bit display (movie
    /// frames) is shown from `display_rgba8` as the device holds it.
    pub fn set_game_frame(&mut self, trace: Trace, display: DisplayInfo, scene: String, display_rgba8: impl FnOnce() -> (u32, u32, Vec<u8>)) {
        self.renderer.push(trace);
        if GameView::shows(&display) {
            self.game_frame = Some((display, scene));
        } else {
            self.game_frame = None;
            let (width, height, rgba) = display_rgba8();
            self.set_game_image((width, height), &rgba);
        }
    }

    /// Render one frame: the game's display (or, without a session, the test
    /// scene at `time` seconds) with the panel on top.
    pub fn frame(&mut self, time: f64, settings: &SettingsService) -> Result<(), String> {
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
        let target = self.size();
        let panel_rect = self.panel_rect();
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        let presented = match &self.game_frame {
            Some((display, scene)) => self.renderer.present(&mut encoder, display, scene, time).map(|p| (p.view, p.size)),
            None => None,
        };
        let encoded = presented.is_some();
        let (game_view, game_size) = match (presented, &self.game_image) {
            (Some(presented), _) => presented,
            (None, Some((_, view, size))) => (view, *size),
            (None, None) => {
                let (view, proj) = demo_camera(GAME_SIZE.0 as f32 / GAME_SIZE.1 as f32, time as f32);
                self.scene.render_views(
                    &mut encoder,
                    &[ViewTarget { view, proj, target: &self.game_view, viewport: Viewport::full(GAME_SIZE.0, GAME_SIZE.1) }],
                    time as f32,
                );
                (&self.game_view, GAME_SIZE)
            }
        };
        let presentation = &settings.settings().presentation;
        if let Some(counter) = &mut self.frame_rate {
            counter.frame(std::time::Duration::from_secs_f64(time),self.scale,presentation.show_fps).map_err(|error| error.to_string())?;
        }
        let mut game = Quad::screen(game_view, present_rect(game_size, target, presentation.scale), target);
        game.srgb_encoded = encoded;
        game.filter = match presentation.filter {
            Filter::Nearest => wgpu::FilterMode::Nearest,
            Filter::Linear => wgpu::FilterMode::Linear,
        };
        let mut quads = vec![game];
        if presentation.show_fps && let Some(counter) = &self.frame_rate {
            let (width,height) = counter.size();
            let margin = 8.0*self.scale;
            let mut quad = Quad::screen(counter.view(),Rect { x:margin,y:margin,width:width as f32,height:height as f32 },target);
            quad.srgb_encoded = true;
            quad.filter = wgpu::FilterMode::Nearest;
            quads.push(quad);
        }
        if let (true, Some(panel), Some(rect)) = (self.panel_visible, &self.panel, panel_rect) {
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
