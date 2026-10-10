//! The Slint settings panel as a custom Slint platform under the host loop.
//!
//! There is no Slint event loop and no Slint window: the host forwards its input
//! as [`WindowEvent`]s, drives timers and animations from its frame loop and gets
//! the panel as a wgpu texture rendered on the host's own device and queue (when
//! Slint asks for a redraw). Panel edits become [`SettingChange`]s applied
//! through [`SettingsService::apply`]; the panel shows the acknowledged values.

use slint::platform::{Platform, Renderer, WindowAdapter};
use slint::{ComponentHandle, PhysicalSize, PlatformError};
use std::cell::{Cell, RefCell};
use std::rc::{Rc, Weak};
use xem_settings::{
    AudioOutput, Filter, Rejected, Scale, SettingChange, Settings, SettingsService,
};

// Slint compiles its FemtoVG renderer module out on Android; the renderer
// crate itself builds there.
#[cfg(target_os = "android")]
use i_slint_renderer_femtovg::FemtoVGWGPURenderer;
#[cfg(not(target_os = "android"))]
use slint::platform::femtovg_renderer::FemtoVGWGPURenderer;

pub use slint::platform::{Key, PointerEventButton, WindowEvent};
pub use slint::{LogicalPosition, LogicalSize, SharedString};

slint::include_modules!();

/// The panel texture's format. It holds sRGB-encoded values: composite it with
/// `srgb_encoded` set.
pub const PANEL_FORMAT: wgpu::TextureFormat = wgpu::TextureFormat::Rgba8Unorm;

/// The wgpu objects the panel renders with: the host's, shared.
#[derive(Clone)]
pub struct SharedGpu {
    pub instance: wgpu::Instance,
    pub device: wgpu::Device,
    pub queue: wgpu::Queue,
}

thread_local! {
    // The device the next window adapter renders with, and that adapter once
    // Slint has created it for a new component.
    static NEXT_GPU: RefCell<Option<SharedGpu>> = const { RefCell::new(None) };
    static CREATED: RefCell<Option<Rc<PanelWindow>>> = const { RefCell::new(None) };
}

/// Slint's platform on the host thread: it only creates texture-backed windows.
struct HostPlatform;

impl Platform for HostPlatform {
    fn create_window_adapter(&self) -> Result<Rc<dyn WindowAdapter>, PlatformError> {
        let gpu = NEXT_GPU
            .with_borrow(Clone::clone)
            .ok_or_else(|| PlatformError::Other("no GPU for the Slint window".into()))?;
        let renderer = FemtoVGWGPURenderer::new(gpu.instance, gpu.device, gpu.queue)?;
        let window = Rc::new_cyclic(|adapter: &Weak<PanelWindow>| PanelWindow {
            window: slint::Window::new(adapter.clone() as Weak<dyn WindowAdapter>),
            renderer,
            size: Cell::new(PhysicalSize::new(1, 1)),
            redraw: Cell::new(true),
        });
        CREATED.set(Some(window.clone()));
        Ok(window)
    }
}

struct PanelWindow {
    window: slint::Window,
    renderer: FemtoVGWGPURenderer,
    size: Cell<PhysicalSize>,
    redraw: Cell<bool>,
}

impl WindowAdapter for PanelWindow {
    fn window(&self) -> &slint::Window {
        &self.window
    }

    fn size(&self) -> PhysicalSize {
        self.size.get()
    }

    fn renderer(&self) -> &dyn Renderer {
        &self.renderer
    }

    fn request_redraw(&self) {
        self.redraw.set(true);
    }
}

/// The settings panel bound to a [`SettingsService`].
pub struct SettingsUi {
    window: Rc<PanelWindow>,
    panel: SettingsPanel,
    device: wgpu::Device,
    texture: wgpu::Texture,
    view: wgpu::TextureView,
}

impl SettingsUi {
    /// Creates the panel at `size` physical pixels and `scale_factor` physical
    /// pixels per logical pixel. Installs the custom Slint platform on this
    /// thread the first time; all panels of a thread must use the same device.
    pub fn new(
        gpu: SharedGpu,
        settings: Rc<RefCell<SettingsService>>,
        size: (u32, u32),
        scale_factor: f32,
    ) -> Result<Self, PlatformError> {
        let device = gpu.device.clone();
        NEXT_GPU.set(Some(gpu));
        match slint::platform::set_platform(Box::new(HostPlatform)) {
            Ok(()) | Err(slint::platform::SetPlatformError::AlreadySet) => {}
            Err(error) => return Err(PlatformError::Other(error.to_string())),
        }
        let panel = SettingsPanel::new()?;
        let window = CREATED
            .take()
            .ok_or_else(|| PlatformError::Other("Slint created no window".into()))?;
        bind(&panel, &settings);
        let (texture, view) = panel_texture(&device, size);
        let ui = Self {
            window,
            panel,
            device,
            texture,
            view,
        };
        ui.resize_window(size, scale_factor);
        ui.panel.show()?;
        Ok(ui)
    }

    fn resize_window(&self, size: (u32, u32), scale_factor: f32) {
        self.window.size.set(PhysicalSize::new(size.0, size.1));
        let window = &self.window.window;
        window.dispatch_event(WindowEvent::ScaleFactorChanged { scale_factor });
        window.dispatch_event(WindowEvent::Resized {
            size: PhysicalSize::new(size.0, size.1).to_logical(scale_factor),
        });
        self.window.redraw.set(true);
    }

    /// Changes the panel's physical size and scale factor.
    pub fn resize(&mut self, size: (u32, u32), scale_factor: f32) {
        if size != self.size() {
            (self.texture, self.view) = panel_texture(&self.device, size);
        }
        self.resize_window(size, scale_factor);
    }

    pub fn size(&self) -> (u32, u32) {
        let size = self.window.size.get();
        (size.width, size.height)
    }

    pub fn scale_factor(&self) -> f32 {
        self.window.window.scale_factor()
    }

    /// Forwards host input. Positions are logical pixels from the panel's top left.
    pub fn dispatch(&self, event: WindowEvent) {
        self.window.window.dispatch_event(event);
    }

    /// Advances Slint's timers and animations to now and, if Slint asked for a
    /// redraw, renders the panel into its texture. Returns whether it rendered.
    pub fn update(&mut self) -> Result<bool, PlatformError> {
        slint::platform::update_timers_and_animations();
        if !self.window.redraw.take() {
            return Ok(false);
        }
        self.window.renderer.render_to_texture(&self.texture)?;
        if self.window.window.has_active_animations() {
            self.window.redraw.set(true);
        }
        Ok(true)
    }

    /// The rendered panel, premultiplied alpha, [`PANEL_FORMAT`].
    pub fn texture(&self) -> &wgpu::Texture {
        &self.texture
    }

    pub fn view(&self) -> &wgpu::TextureView {
        &self.view
    }

    /// Called when the panel's Close button is pressed.
    pub fn on_close(&self, f: impl FnMut() + 'static) {
        self.panel.on_close_requested(f);
    }

    /// The Slint component, for inspection.
    pub fn component(&self) -> &SettingsPanel {
        &self.panel
    }
}

fn panel_texture(
    device: &wgpu::Device,
    (width, height): (u32, u32),
) -> (wgpu::Texture, wgpu::TextureView) {
    let texture = device.create_texture(&wgpu::TextureDescriptor {
        label: Some("settings panel"),
        size: wgpu::Extent3d {
            width: width.max(1),
            height: height.max(1),
            depth_or_array_layers: 1,
        },
        mip_level_count: 1,
        sample_count: 1,
        dimension: wgpu::TextureDimension::D2,
        format: PANEL_FORMAT,
        usage: wgpu::TextureUsages::RENDER_ATTACHMENT
            | wgpu::TextureUsages::TEXTURE_BINDING
            | wgpu::TextureUsages::COPY_SRC,
        view_formats: &[],
    });
    let view = texture.create_view(&Default::default());
    (texture, view)
}

const AUDIO_OUTPUTS: [AudioOutput; 3] = [AudioOutput::Mono, AudioOutput::Stereo, AudioOutput::Wide];
const SCALES: [Scale; 3] = [Scale::Fit, Scale::Integer, Scale::Stretch];
const FILTERS: [Filter; 2] = [Filter::Nearest, Filter::Linear];

fn index_of<T: PartialEq>(options: &[T], value: &T) -> i32 {
    options
        .iter()
        .position(|option| option == value)
        .unwrap_or(0) as i32
}

/// Shows acknowledged settings in the panel.
fn show(panel: &SettingsPanel, settings: &Settings) {
    panel.set_master_volume(f32::from(settings.master_volume));
    panel.set_audio_output(index_of(&AUDIO_OUTPUTS, &settings.audio_output));
    panel.set_scale(index_of(&SCALES, &settings.presentation.scale));
    panel.set_filter(index_of(&FILTERS, &settings.presentation.filter));
    panel.set_show_fps(settings.presentation.show_fps);
}

/// Routes panel edits through the settings service and shows every
/// acknowledged change, whoever made it.
fn bind(panel: &SettingsPanel, settings: &Rc<RefCell<SettingsService>>) {
    show(panel, settings.borrow().settings());
    let weak = panel.as_weak();
    settings.borrow_mut().subscribe(move |ack| {
        if let Some(panel) = weak.upgrade() {
            show(&panel, &ack.settings);
            panel.set_status(format!("Applied {:?}", ack.change).into());
        }
    });

    let apply = {
        let weak = panel.as_weak();
        let settings = Rc::downgrade(settings);
        move |change: Option<SettingChange>| {
            let (Some(panel), Some(settings)) = (weak.upgrade(), settings.upgrade()) else {
                return;
            };
            let Some(change) = change else { return };
            let result = settings.borrow_mut().apply(change);
            if let Err(rejected) = result {
                // The widget may show the refused value: restore the current one.
                show(&panel, settings.borrow().settings());
                panel.set_status(describe(&rejected).into());
            }
        }
    };
    panel.on_master_volume_edited({
        let apply = apply.clone();
        move |volume| apply(Some(SettingChange::MasterVolume(volume)))
    });
    panel.on_audio_output_edited({
        let apply = apply.clone();
        move |index| apply(pick(&AUDIO_OUTPUTS, index).map(SettingChange::AudioOutput))
    });
    panel.on_scale_edited({
        let apply = apply.clone();
        move |index| apply(pick(&SCALES, index).map(SettingChange::Scale))
    });
    panel.on_filter_edited({
        let apply = apply.clone();
        move |index| apply(pick(&FILTERS, index).map(SettingChange::Filter))
    });
    panel.on_show_fps_edited(move |show| apply(Some(SettingChange::ShowFps(show))));
}

fn pick<T: Copy>(options: &[T], index: i32) -> Option<T> {
    usize::try_from(index)
        .ok()
        .and_then(|i| options.get(i).copied())
}

fn describe(rejected: &Rejected) -> String {
    match rejected {
        Rejected::Invalid { reason, .. } => format!("Not applied: {reason}"),
        Rejected::Storage { reason, .. } => format!("Not saved: {reason}"),
    }
}
