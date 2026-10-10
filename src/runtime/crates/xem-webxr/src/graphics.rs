//! The graphics side of the adapter: one WebGL2 context, made XR-compatible,
//! carries the wgpu device that draws both the flat canvas and the immersive
//! layer. The layer's framebuffer is wrapped as a wgpu texture in place, so
//! frames reach the XR compositor without a CPU copy.

use wasm_bindgen::{JsCast, JsValue};
use web_sys::{HtmlCanvasElement, WebGl2RenderingContext, WebGlFramebuffer, XrWebGlLayer};
use wgpu::hal::{api::Gles, gles};
use xem_render::glam::{Mat4, Vec4};
use xem_render::{Compositor, Gpu, Quad, SceneRenderer, ViewTarget, Viewport};

/// The colour format the scene is drawn in before it reaches a target.
const SCENE_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8UnormSrgb;
/// WebGL default and opaque XR framebuffers are 8-bit RGBA without sRGB
/// encoding on write; the compositor encodes in its shader for such targets.
const LAYER_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;
const GL_RGBA8: u32 = 0x8058;
const GL_RGBA: u32 = 0x1908;
const GL_UNSIGNED_BYTE: u32 = 0x1401;
const GL_READ_FRAMEBUFFER: u32 = 0x8CA8;

/// One eye (or a mono view) of an immersive frame, already in wgpu
/// conventions except for `viewport`, which is the layer's own rectangle
/// (pixels, origin bottom-left, as WebXR reports it).
pub struct EyeView {
    pub view: Mat4,
    pub proj: Mat4,
    pub viewport: [i32; 4],
}

/// The layer framebuffer wrapped as a wgpu render target. `framebuffer` is
/// `None` when the layer renders to the canvas's default framebuffer (inline
/// sessions, and IWER's emulated immersive layer).
struct LayerTarget {
    framebuffer: Option<WebGlFramebuffer>,
    size: (u32, u32),
    view: wgpu::TextureView,
}

struct EyeBuffer {
    size: (u32, u32),
    view: wgpu::TextureView,
}

/// The flat canvas presentation: a wgpu surface on the same context.
struct Flat {
    surface: wgpu::Surface<'static>,
    config: wgpu::SurfaceConfiguration,
}

pub struct XrGraphics {
    pub gl: WebGl2RenderingContext,
    pub gpu: Gpu,
    canvas: HtmlCanvasElement,
    scene: SceneRenderer,
    compositor: Compositor,
    flat: Flat,
    layer: Option<LayerTarget>,
    eyes: Option<EyeBuffer>,
}

impl XrGraphics {
    /// Creates the WebGL2 context of `canvas` (XR-compatible from the start
    /// when `xr_compatible`), the wgpu GLES adapter and device over that
    /// context, and the flat surface on the same canvas.
    pub async fn new(canvas: HtmlCanvasElement, xr_compatible: bool) -> Result<Self, String> {
        let attributes = js_sys::Object::new();
        for (key, value) in [
            ("alpha", false),
            ("antialias", false),
            ("depth", false),
            ("stencil", false),
            ("xrCompatible", xr_compatible),
        ] {
            js_sys::Reflect::set(&attributes, &key.into(), &value.into()).map_err(js_error)?;
        }
        let gl: WebGl2RenderingContext = canvas
            .get_context_with_context_options("webgl2", &attributes)
            .map_err(js_error)?
            .ok_or("WebGL2 is not available")?
            .dyn_into()
            .map_err(|_| "getContext('webgl2') did not return a WebGL2 context")?;

        let instance = wgpu::Instance::new(wgpu::InstanceDescriptor {
            backends: wgpu::Backends::GL,
            ..wgpu::InstanceDescriptor::new_without_display_handle()
        });
        // SAFETY: the context is live; this struct keeps it (and the canvas)
        // alive for as long as the adapter and everything made from it.
        let exposed = unsafe { gles::Adapter::new_external(gl.clone(), Default::default()) }
            .ok_or("the WebGL2 context exposes no wgpu adapter")?;
        // SAFETY: the adapter was exposed by the GLES backend this instance enables.
        let adapter = unsafe { instance.create_adapter_from_hal(exposed) };
        let (device, queue) = adapter
            .request_device(&wgpu::DeviceDescriptor {
                label: Some("xem-webxr"),
                required_limits: adapter.limits(),
                ..Default::default()
            })
            .await
            .map_err(|error| format!("wgpu device request failed: {error}"))?;

        // The surface asks the canvas for "webgl2" again and receives the
        // context created above, so flat and immersive frames share one device.
        let surface = instance
            .create_surface(wgpu::SurfaceTarget::Canvas(canvas.clone()))
            .map_err(|error| format!("canvas surface: {error}"))?;
        let capabilities = surface.get_capabilities(&adapter);
        let format = if capabilities.formats.contains(&SCENE_FORMAT) {
            SCENE_FORMAT
        } else {
            return Err(format!(
                "the canvas surface lacks {SCENE_FORMAT:?}: {:?}",
                capabilities.formats
            ));
        };
        let config = wgpu::SurfaceConfiguration {
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
            format,
            width: canvas.width().max(1),
            height: canvas.height().max(1),
            present_mode: wgpu::PresentMode::Fifo,
            desired_maximum_frame_latency: 1,
            alpha_mode: capabilities.alpha_modes[0],
            view_formats: vec![],
            color_space: wgpu::SurfaceColorSpace::Auto,
        };
        surface.configure(&device, &config);

        let gpu = Gpu {
            instance,
            adapter,
            device,
            queue,
        };
        let scene = SceneRenderer::new(&gpu.device, &gpu.queue, SCENE_FORMAT);
        let compositor = Compositor::new(&gpu.device, &gpu.queue, LAYER_FORMAT);
        Ok(Self {
            gl,
            gpu,
            canvas,
            scene,
            compositor,
            flat: Flat { surface, config },
            layer: None,
            eyes: None,
        })
    }

    pub fn adapter_info(&self) -> wgpu::AdapterInfo {
        self.gpu.adapter.get_info()
    }

    /// Draws one flat frame onto the canvas, resizing the surface to the
    /// canvas first.
    pub fn render_flat(&mut self, view: Mat4, proj: Mat4, time: f32) -> Result<(), String> {
        let (width, height) = (self.canvas.width().max(1), self.canvas.height().max(1));
        if (width, height) != (self.flat.config.width, self.flat.config.height) {
            self.flat.config.width = width;
            self.flat.config.height = height;
            self.flat
                .surface
                .configure(&self.gpu.device, &self.flat.config);
        }
        let frame = match self.flat.surface.get_current_texture() {
            wgpu::CurrentSurfaceTexture::Success(frame)
            | wgpu::CurrentSurfaceTexture::Suboptimal(frame) => frame,
            other => return Err(format!("no canvas frame: {other:?}")),
        };
        let target = frame.texture.create_view(&Default::default());
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        self.scene.render_views(
            &mut encoder,
            &[ViewTarget {
                view,
                proj,
                target: &target,
                viewport: Viewport::full(width, height),
            }],
            time,
        );
        self.gpu.queue.submit([encoder.finish()]);
        self.gpu.queue.present(frame);
        Ok(())
    }

    /// Draws `views` into `layer` for the current XR animation frame. The
    /// scene goes into an eye buffer the size of the layer (wgpu owns its
    /// colour and depth), and one compositor pass copies it into the layer's
    /// framebuffer on the GPU, flipped vertically: wgpu's GLES backend stores
    /// images top row first while WebXR (like any GL framebuffer it shows)
    /// reads the bottom row first. Returns whether the layer framebuffer is the
    /// opaque XR framebuffer (`true`) or the canvas's default one.
    pub fn render_layer(
        &mut self,
        layer: &XrWebGlLayer,
        views: &[EyeView],
        time: f32,
    ) -> Result<bool, String> {
        let framebuffer = layer.framebuffer();
        let size = (layer.framebuffer_width(), layer.framebuffer_height());
        if size.0 == 0 || size.1 == 0 {
            return Err(format!("layer framebuffer is {}x{}", size.0, size.1));
        }
        let stale = self.layer.as_ref().is_none_or(|target| {
            target.size != size
                || target.framebuffer.as_ref().map(JsValue::from)
                    != framebuffer.as_ref().map(JsValue::from)
        });
        if stale {
            self.layer = Some(self.wrap_layer(framebuffer, size));
        }
        if self.eyes.as_ref().is_none_or(|eyes| eyes.size != size) {
            self.eyes = Some(self.eye_buffer(size));
        }
        let (Some(target), Some(eyes)) = (&self.layer, &self.eyes) else {
            unreachable!()
        };

        let height = size.1 as i32;
        let targets: Vec<ViewTarget> = views
            .iter()
            .map(|eye| {
                let [x, y, width, view_height] = eye.viewport;
                ViewTarget {
                    view: eye.view,
                    proj: eye.proj,
                    target: &eyes.view,
                    // WebXR viewports count rows from the bottom; the eye
                    // buffer counts from the top until the flip below.
                    viewport: Viewport {
                        x: x as f32,
                        y: (height - y - view_height) as f32,
                        width: width as f32,
                        height: view_height as f32,
                    },
                }
            })
            .collect();
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        self.scene.render_views(&mut encoder, &targets, time);
        // The unit square (u right, v down) onto the whole target with v = 0
        // at the bottom of clip space.
        let flip = Mat4::from_cols(
            Vec4::new(2.0, 0.0, 0.0, 0.0),
            Vec4::new(0.0, 2.0, 0.0, 0.0),
            Vec4::Z,
            Vec4::new(-1.0, -1.0, 0.0, 1.0),
        );
        self.compositor.draw(
            &mut encoder,
            &target.view,
            Some(wgpu::Color::BLACK),
            &[Quad {
                texture: &eyes.view,
                transform: flip,
                filter: wgpu::FilterMode::Nearest,
                opacity: 1.0,
                srgb_encoded: false,
            }],
        );
        self.gpu.queue.submit([encoder.finish()]);
        Ok(target.framebuffer.is_some())
    }

    /// Forgets the layer target of an ended session.
    pub fn release_layer(&mut self) {
        self.layer = None;
    }

    fn wrap_layer(&self, framebuffer: Option<WebGlFramebuffer>, size: (u32, u32)) -> LayerTarget {
        let inner = match &framebuffer {
            Some(framebuffer) => gles::TextureInner::ExternalFramebuffer {
                inner: framebuffer.clone(),
            },
            None => gles::TextureInner::DefaultRenderbuffer,
        };
        let hal_texture = gles::Texture {
            inner,
            mip_level_count: 1,
            array_layer_count: 1,
            format: LAYER_FORMAT,
            format_desc: gles::TextureFormatDesc {
                internal: GL_RGBA8,
                external: GL_RGBA,
                data_type: GL_UNSIGNED_BYTE,
            },
            copy_size: wgpu::hal::CopyExtent {
                width: size.0,
                height: size.1,
                depth: 1,
            },
            drop_guard: None,
        };
        // SAFETY: the texture describes the layer's framebuffer (or the
        // canvas's default one) on this device's context; WebXR keeps it valid
        // for the session and the target is rebuilt when it changes. Every
        // pass into it clears first, so its initial contents never matter.
        let texture = unsafe {
            self.gpu.device.create_texture_from_hal::<Gles>(
                hal_texture,
                &wgpu::TextureDescriptor {
                    label: Some("xr layer"),
                    size: wgpu::Extent3d {
                        width: size.0,
                        height: size.1,
                        depth_or_array_layers: 1,
                    },
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
        LayerTarget {
            framebuffer,
            size,
            view: texture.create_view(&Default::default()),
        }
    }

    fn eye_buffer(&self, size: (u32, u32)) -> EyeBuffer {
        let texture = self.gpu.device.create_texture(&wgpu::TextureDescriptor {
            label: Some("xr eyes"),
            size: wgpu::Extent3d {
                width: size.0,
                height: size.1,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: SCENE_FORMAT,
            usage: wgpu::TextureUsages::RENDER_ATTACHMENT | wgpu::TextureUsages::TEXTURE_BINDING,
            view_formats: &[],
        });
        EyeBuffer {
            size,
            view: texture.create_view(&Default::default()),
        }
    }

    /// Debug readback: the RGBA bytes at `points` (layer pixels, origin
    /// bottom-left) of the framebuffer just submitted. Synchronous; tests only.
    pub fn read_layer_pixels(
        &self,
        layer: &XrWebGlLayer,
        points: &[[i32; 2]],
    ) -> Result<Vec<[u8; 4]>, String> {
        let gl = &self.gl;
        gl.bind_framebuffer(GL_READ_FRAMEBUFFER, layer.framebuffer().as_ref());
        let mut pixels = Vec::with_capacity(points.len());
        for &[x, y] in points {
            let mut pixel = [0u8; 4];
            gl.read_pixels_with_opt_u8_array(
                x,
                y,
                1,
                1,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                Some(&mut pixel),
            )
            .map_err(js_error)?;
            pixels.push(pixel);
        }
        gl.bind_framebuffer(GL_READ_FRAMEBUFFER, None);
        Ok(pixels)
    }
}

pub fn js_error(value: JsValue) -> String {
    if let Some(error) = value.dyn_ref::<js_sys::Error>() {
        let name = String::from(error.name());
        let message = String::from(error.message());
        format!("{name}: {message}")
    } else {
        format!("{value:?}")
    }
}
