//! wgpu on the page's canvas: WebGPU where the browser offers an adapter,
//! WebGL2 otherwise. One device serves the scene, the compositor and the Slint
//! settings panel, as on the desktop host.

use std::cell::RefCell;
use std::rc::Rc;
use std::sync::{Arc, Mutex};

use wasm_bindgen::{JsCast, JsValue};
use web_sys::{HtmlCanvasElement, WebGl2RenderingContext, WebGlFramebuffer, XrWebGlLayer};
use xem_render::glam::{Mat4, Vec2, Vec3, Vec4};
use xem_render::ps1::{GameView, HostClass, ViewConfig};
use xem_render::xem_gpu::{DisplayInfo, Reveals, Trace};
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
    /// The XR-compatible WebGL2 context a "webgl2-xr" renderer draws with
    /// (immersive frames go to the session's WebGL layer).
    gl: Option<WebGl2RenderingContext>,
    xr: Option<XrTargets>,
    xr_eye_height: u32,
}

/// One WebXR view of an immersive frame, in wgpu conventions except for
/// `viewport`: the layer's rectangle, pixels, origin bottom-left.
pub struct XrView {
    pub view: Mat4,
    pub proj: Mat4,
    pub viewport: [i32; 4],
}

/// The immersive layer wrapped as a wgpu target, the eye buffers and the
/// layer-sized buffer they are assembled in (top row first).
struct XrTargets {
    framebuffer: Option<WebGlFramebuffer>,
    size: (u32, u32),
    layer: wgpu::TextureView,
    assembled: wgpu::TextureView,
    eyes: Vec<((u32, u32), wgpu::TextureView)>,
}

/// The room around the virtual screen.
const BACKDROP: wgpu::Color = wgpu::Color { r: 0.012, g: 0.014, b: 0.02, a: 1.0 };
/// The WebXR layer: 8-bit RGBA without sRGB writes (the compositor encodes).
const LAYER_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;

/// Create the renderer on `canvas`. `prefer` "webgl2" skips WebGPU;
/// "webgl2-xr" creates an XR-compatible WebGL2 context for immersive play
/// (WebXR layers are submitted through WebGL2; see docs/runtime.md).
pub async fn create(canvas: HtmlCanvasElement, prefer: &str) -> Result<WebRenderer, String> {
    let xr = prefer == "webgl2-xr";
    let webgpu = !xr && prefer != "webgl2" && wgpu::util::is_browser_webgpu_supported().await;
    let mut descriptor = wgpu::InstanceDescriptor::new_without_display_handle();
    descriptor.backends = if webgpu { wgpu::Backends::BROWSER_WEBGPU } else { wgpu::Backends::GL };
    let instance = wgpu::Instance::new(descriptor);
    let (width, height) = (canvas.width().max(1), canvas.height().max(1));
    let gl = if xr { Some(xr_context(&canvas)?) } else { None };
    let surface = instance
        .create_surface(wgpu::SurfaceTarget::Canvas(canvas))
        .map_err(|e| format!("canvas surface: {e}"))?;
    let gpu = match &gl {
        Some(gl) => {
            // SAFETY: the context is live; the renderer keeps it (and so the
            // canvas) alive for as long as the adapter and what it made.
            let exposed = unsafe { wgpu::hal::gles::Adapter::new_external(gl.clone(), Default::default()) }
                .ok_or("the WebGL2 context exposes no wgpu adapter")?;
            // SAFETY: exposed by the GLES backend this instance enables.
            let adapter = unsafe { instance.create_adapter_from_hal(exposed) };
            let (device, queue) = adapter
                .request_device(&wgpu::DeviceDescriptor { label: Some("xem"), required_limits: adapter.limits(), ..Default::default() })
                .await
                .map_err(|e| format!("GPU device request failed: {e}"))?;
            Gpu { instance, adapter, device, queue }
        }
        None => Gpu::new(instance, Some(&surface)).await.map_err(|e| e.to_string())?,
    };
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
        backend: if webgpu { "webgpu" } else if xr { "webgl2-xr" } else { "webgl2" },
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
        gl,
        xr: None,
        xr_eye_height: 1080,
    })
}

/// The canvas's WebGL2 context, XR-compatible from the start (no alpha,
/// antialiasing, depth or stencil: the renderer owns its targets).
fn xr_context(canvas: &HtmlCanvasElement) -> Result<WebGl2RenderingContext, String> {
    let attributes = js_sys::Object::new();
    for (key, value) in [("alpha", false), ("antialias", false), ("depth", false), ("stencil", false), ("xrCompatible", true)] {
        js_sys::Reflect::set(&attributes, &key.into(), &value.into()).map_err(|e| format!("{e:?}"))?;
    }
    canvas
        .get_context_with_context_options("webgl2", &attributes)
        .map_err(|e| format!("{e:?}"))?
        .ok_or("WebGL2 is not available")?
        .dyn_into()
        .map_err(|_| "getContext('webgl2') did not return a WebGL2 context".into())
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
                panel.set_close_hint("Esc closes Settings");
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

    /// Which faces the game culls against its screen its trace should
    /// record (a wider or pulled-back view).
    pub fn reveals(&self) -> Option<Reveals> {
        self.renderer.reveals()
    }

    /// The view the game's field characters keep moving in
    /// (`GameView::actor_view`).
    pub fn actor_view(&self) -> u32 {
        self.renderer.actor_view()
    }

    /// The renderer's configuration for `graphics` on this canvas (or in a
    /// headset); returns whether the device's draw trace must restart from
    /// the whole VRAM.
    pub fn configure_game(&mut self, graphics: &xem_settings::Graphics, headset: bool) -> bool {
        let class = if headset { HostClass::Headset } else { HostClass::Browser };
        // The virtual screen fills about half of a view.
        let size = if headset { (self.xr_eye_height / 2, self.xr_eye_height / 2) } else { (self.config.width, self.config.height) };
        let config = ViewConfig::resolve(graphics, class, size, self.renderer.max_scale());
        self.renderer.configure(config);
        self.renderer.take_restart()
    }

    /// The display as the device holds it (the Original renderer).
    pub fn set_display_image(&mut self, (width, height, rgba): (u32, u32, Vec<u8>)) {
        self.game_frame = None;
        self.set_game_image((width, height), &rgba);
    }

    /// What the game drew, its display and scene. A 24-bit display (movie
    /// frames) the view does not rebuild is shown from `display_rgba8` as
    /// the device holds it.
    pub fn set_game_frame(&mut self, trace: Trace, display: DisplayInfo, scene: String, display_rgba8: impl FnOnce() -> (u32, u32, Vec<u8>)) {
        self.renderer.push(trace);
        if GameView::shows(&display) || self.renderer.shows_movie(&display) {
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
        // Movie frames the device shows itself (the Original movies setting,
        // or one Ex Machina cannot rebuild) stay as the decoder made them,
        // each pixel a block; the Original renderer keeps the chosen filter.
        let graphics = settings.settings().graphics;
        if !encoded && self.game_frame.is_none() && self.game_image.is_some() && graphics.renderer == xem_settings::Renderer::ExMachina {
            game.filter = wgpu::FilterMode::Nearest;
        }
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

impl WebRenderer {
    /// The height of the immersive views, for the automatic internal scale.
    pub fn set_xr_eye_height(&mut self, height: u32) {
        if height > 0 {
            self.xr_eye_height = height;
        }
    }

    /// Whether this renderer can draw immersive frames (a "webgl2-xr" one).
    pub fn xr_capable(&self) -> bool {
        self.gl.is_some()
    }

    /// Draws one immersive frame into `layer`: the game (presented once,
    /// whatever the number of views) on a flat virtual screen `width` metres
    /// wide, `distance` metres in front of the reference space's origin, as
    /// each view sees it. Monoscopic content, shown as a screen.
    pub fn render_xr(
        &mut self,
        layer: &XrWebGlLayer,
        views: &[XrView],
        time: f64,
        settings: &SettingsService,
    ) -> Result<(), String> {
        if self.lost().is_some() {
            return Ok(());
        }
        let framebuffer = layer.framebuffer();
        let size = (layer.framebuffer_width(), layer.framebuffer_height());
        if size.0 == 0 || size.1 == 0 {
            return Err(format!("layer framebuffer is {}x{}", size.0, size.1));
        }
        let stale = self.xr.as_ref().is_none_or(|t| {
            t.size != size || t.framebuffer.as_ref().map(JsValue::from) != framebuffer.as_ref().map(JsValue::from)
        });
        if stale {
            self.xr = Some(self.xr_targets(framebuffer, size));
        }
        let graphics = settings.settings().graphics;
        let presentation = &settings.settings().presentation;
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        let presented = match &self.game_frame {
            Some((display, scene)) => self.renderer.present(&mut encoder, display, scene, time).map(|p| (p.view, p.size, true)),
            None => None,
        };
        let (screen, screen_size, encoded) = match (presented, &self.game_image) {
            (Some(presented), _) => (Some(presented.0), presented.1, presented.2),
            (None, Some((_, view, size))) => (Some(view), *size, false),
            (None, None) => (None, (4, 3), false),
        };
        let targets = self.xr.as_mut().expect("made above");
        // Eye buffers, one per view, sized as its viewport.
        while targets.eyes.len() < views.len() {
            targets.eyes.push(((0, 0), targets.layer.clone()));
        }
        for (eye, view) in targets.eyes.iter_mut().zip(views) {
            let wanted = (view.viewport[2].max(1) as u32, view.viewport[3].max(1) as u32);
            if eye.0 != wanted {
                *eye = (wanted, eye_texture(&self.gpu.device, wanted, self.compositor.format()));
            }
        }
        let targets = self.xr.as_ref().expect("made above");
        let width = f32::from(graphics.xr.width_cm) / 100.0;
        let height = width * screen_size.1 as f32 / screen_size.0 as f32;
        let pose = Mat4::from_translation(Vec3::new(0.0, 0.0, -f32::from(graphics.xr.distance_cm) / 100.0));
        let filter = match presentation.filter {
            Filter::Nearest => wgpu::FilterMode::Nearest,
            Filter::Linear => wgpu::FilterMode::Linear,
        };
        let eye_quads: Vec<Vec<Quad>> = views
            .iter()
            .map(|view| {
                screen
                    .iter()
                    .map(|screen| {
                        let mut quad = Quad::world(screen, view.proj * view.view, pose, Vec2::new(width, height));
                        quad.srgb_encoded = encoded;
                        quad.filter = filter;
                        if !encoded && graphics.renderer == xem_settings::Renderer::ExMachina {
                            quad.filter = wgpu::FilterMode::Nearest;
                        }
                        quad
                    })
                    .collect()
            })
            .collect();
        // The eyes into the layer-sized buffer at their viewports (WebXR
        // counts rows from the bottom; the buffer from the top until the flip).
        let assembled: Vec<Quad> = targets
            .eyes
            .iter()
            .zip(views)
            .map(|((_, eye), view)| {
                let [x, y, w, h] = view.viewport;
                let rect = Rect { x: x as f32, y: (size.1 as i32 - y - h) as f32, width: w as f32, height: h as f32 };
                let mut quad = Quad::screen(eye, rect, size);
                quad.srgb_encoded = true;
                quad.filter = wgpu::FilterMode::Nearest;
                quad
            })
            .collect();
        // wgpu's GLES backend stores images top row first; WebXR reads
        // framebuffers bottom row first.
        let flip = Mat4::from_cols(Vec4::new(2.0, 0.0, 0.0, 0.0), Vec4::new(0.0, 2.0, 0.0, 0.0), Vec4::Z, Vec4::new(-1.0, -1.0, 0.0, 1.0));
        let mut flipped = Quad::screen(&targets.assembled, Rect { x: 0.0, y: 0.0, width: size.0 as f32, height: size.1 as f32 }, size);
        flipped.transform = flip;
        flipped.srgb_encoded = true;
        flipped.filter = wgpu::FilterMode::Nearest;
        let mut passes: Vec<(&wgpu::TextureView, Option<wgpu::Color>, &[Quad])> = targets
            .eyes
            .iter()
            .zip(&eye_quads)
            .map(|((_, eye), quads)| (eye, Some(BACKDROP), quads.as_slice()))
            .collect();
        passes.push((&targets.assembled, Some(BACKDROP), &assembled));
        let last = [flipped];
        passes.push((&targets.layer, Some(wgpu::Color::BLACK), &last));
        self.compositor.draw_targets(&mut encoder, &passes);
        drop(passes);
        self.gpu.queue.submit([encoder.finish()]);
        self.frames += 1;
        Ok(())
    }

    /// RGBA bytes at `points` (layer pixels, origin bottom-left) of the
    /// framebuffer just drawn. Synchronous; for tests.
    pub fn read_layer_pixels(&self, layer: &XrWebGlLayer, points: &[[i32; 2]]) -> Result<Vec<u8>, String> {
        let gl = self.gl.as_ref().ok_or("not an XR renderer")?;
        gl.bind_framebuffer(WebGl2RenderingContext::READ_FRAMEBUFFER, layer.framebuffer().as_ref());
        let mut pixels = Vec::with_capacity(points.len() * 4);
        for &[x, y] in points {
            let mut pixel = [0u8; 4];
            gl.read_pixels_with_opt_u8_array(x, y, 1, 1, WebGl2RenderingContext::RGBA, WebGl2RenderingContext::UNSIGNED_BYTE, Some(&mut pixel))
                .map_err(|e| format!("{e:?}"))?;
            pixels.extend_from_slice(&pixel);
        }
        gl.bind_framebuffer(WebGl2RenderingContext::READ_FRAMEBUFFER, None);
        Ok(pixels)
    }

    /// Forgets the targets of an ended session.
    pub fn release_xr(&mut self) {
        self.xr = None;
    }

    fn xr_targets(&self, framebuffer: Option<WebGlFramebuffer>, size: (u32, u32)) -> XrTargets {
        use wgpu::hal::gles;
        let inner = match &framebuffer {
            Some(framebuffer) => gles::TextureInner::ExternalFramebuffer { inner: framebuffer.clone() },
            None => gles::TextureInner::DefaultRenderbuffer,
        };
        let hal_texture = gles::Texture {
            inner,
            mip_level_count: 1,
            array_layer_count: 1,
            format: LAYER_FORMAT,
            format_desc: gles::TextureFormatDesc {
                internal: WebGl2RenderingContext::RGBA8,
                external: WebGl2RenderingContext::RGBA,
                data_type: WebGl2RenderingContext::UNSIGNED_BYTE,
            },
            copy_size: wgpu::hal::CopyExtent { width: size.0, height: size.1, depth: 1 },
            drop_guard: None,
        };
        // SAFETY: the texture describes the session layer's framebuffer (or
        // the canvas's default one) on this device's context; WebXR keeps it
        // valid for the session and the target is rebuilt when it changes.
        // Every pass into it clears first.
        let texture = unsafe {
            self.gpu.device.create_texture_from_hal::<wgpu::hal::api::Gles>(
                hal_texture,
                &wgpu::TextureDescriptor {
                    label: Some("xr layer"),
                    size: wgpu::Extent3d { width: size.0, height: size.1, depth_or_array_layers: 1 },
                    mip_level_count: 1,
                    sample_count: 1,
                    dimension: wgpu::TextureDimension::D2,
                    format: LAYER_FORMAT,
                    usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
                    view_formats: &[],
                },
                wgpu::TextureUses::UNINITIALIZED,
            )
        };
        XrTargets {
            framebuffer,
            size,
            layer: texture.create_view(&Default::default()),
            assembled: eye_texture(&self.gpu.device, size, self.compositor.format()),
            eyes: Vec::new(),
        }
    }
}

fn eye_texture(device: &wgpu::Device, (width, height): (u32, u32), format: wgpu::TextureFormat) -> wgpu::TextureView {
    device
        .create_texture(&wgpu::TextureDescriptor {
            label: Some("xr eye"),
            size: wgpu::Extent3d { width, height, depth_or_array_layers: 1 },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        })
        .create_view(&Default::default())
}

/// The panel's pixel size for a canvas of `width` x `height` pixels.
fn panel_size(width: u32, height: u32, scale: f32) -> (u32, u32) {
    let fit = |logical: f32, available: u32| ((logical * scale) as u32).min(available.saturating_sub(16)).max(1);
    (fit(PANEL_SIZE.0, width), fit(PANEL_SIZE.1, height))
}

