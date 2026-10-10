//! The OpenXR adapter: an immersive session rendered through the shared wgpu
//! device.
//!
//! Ownership, as plan.md's host rules require:
//!
//! - **Device.** The runtime chooses the Vulkan physical device and creates the
//!   instance and device (XR_KHR_vulkan_enable2) from the create infos wgpu-hal
//!   would use; wgpu owns and destroys both. [`XrContext::gpu`] is the host's
//!   one [`Gpu`]: the scene, the compositor and the Slint panel all use it.
//! - **Queue.** wgpu's single queue is the queue the session is bound to.
//!   Rendering is submitted to it before each `xrReleaseSwapchainImage`, which
//!   is all the synchronisation XR_KHR_vulkan_enable2 asks of the application:
//!   the runtime orders its own work on the same queue. Every call that may
//!   touch the queue (frame and swapchain calls, wgpu submissions) happens on
//!   the thread that owns the [`XrSession`], so the queue is never used
//!   concurrently.
//! - **Swapchain images.** The runtime owns them; they are wrapped as wgpu
//!   textures that never destroy the image. An image is rendered only between
//!   acquire+wait and release, starts in `COLOR_ATTACHMENT_OPTIMAL` and is
//!   explicitly returned to it before release.
//! - **Frame scheduling.** `xrWaitFrame` paces the loop; nothing else blocks on
//!   the display. The predicted display time of each frame is the time views,
//!   poses and input are located at.
//! - **Lifetime.** Swapchains and the session are destroyed before the wgpu
//!   device (the session holds a [`Gpu`] clone, dropped last), and wgpu
//!   finishes its pending work before swapchain images go away.

mod input;
mod math;
mod vulkan;

pub use input::{HandInput, InputState, Joint};
pub use math::{Fov, Pose};
pub use openxr;
pub use vulkan::VulkanBinding;

use openxr as xr;
use std::fmt;
use xem_render::Gpu;

const VIEW_TYPE: xr::ViewConfigurationType = xr::ViewConfigurationType::PRIMARY_STEREO;

#[derive(Debug)]
pub struct XrError {
    pub context: &'static str,
    pub detail: String,
    /// The OpenXR result, when an OpenXR call failed.
    pub result: Option<xr::sys::Result>,
}

impl XrError {
    fn new(context: &'static str, detail: String) -> Self {
        Self {
            context,
            detail,
            result: None,
        }
    }

    fn xr(context: &'static str) -> impl FnOnce(xr::sys::Result) -> Self {
        move |result| Self {
            context,
            detail: result.to_string(),
            result: Some(result),
        }
    }
}

impl fmt::Display for XrError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: {}", self.context, self.detail)
    }
}

impl std::error::Error for XrError {}

/// What the runtime and system offer. Optional input is used only when listed
/// here; nothing is assumed from the device type.
#[derive(Clone, Debug)]
pub struct Capabilities {
    pub runtime: String,
    pub system: String,
    pub vendor_id: u32,
    /// XR_EXT_hand_tracking is enabled and the system tracks hands.
    pub hand_tracking: bool,
    /// XR_EXT_eye_gaze_interaction is enabled and the system supports it.
    pub eye_gaze: bool,
    /// Recommended per-eye image size.
    pub eye_size: (u32, u32),
    pub max_layers: u32,
    pub blend_mode: xr::EnvironmentBlendMode,
    pub gpu: String,
}

/// The platform data the loader and instance need: `()` off Android, an
/// [`openxr::AndroidPlatformInfo`] (XR_KHR_loader_init_android and
/// XR_KHR_android_create_instance) on Android.
#[cfg(not(target_os = "android"))]
pub type Platform = ();
#[cfg(target_os = "android")]
pub type Platform = xr::AndroidPlatformInfo;

/// The OpenXR instance, the head-mounted system and the wgpu device created
/// for it.
pub struct XrContext {
    gpu: Gpu,
    binding: VulkanBinding,
    capabilities: Capabilities,
    system: xr::SystemId,
    instance: xr::Instance,
}

impl XrContext {
    /// Loads the OpenXR loader, creates an instance with the extensions the
    /// runtime offers that are used here, finds the head-mounted system and
    /// creates the wgpu device through the runtime.
    pub fn new(platform: &Platform) -> Result<Self, XrError> {
        // SAFETY: the loader found by the platform's library search is trusted
        // to implement OpenXR.
        let entry = unsafe { xr::Entry::load(platform) }
            .map_err(|error| XrError::new("loading the OpenXR loader", error.to_string()))?;
        let available = entry
            .enumerate_extensions()
            .map_err(XrError::xr("xrEnumerateInstanceExtensionProperties"))?;
        if !available.khr_vulkan_enable2 {
            return Err(XrError::new(
                "OpenXR runtime",
                "XR_KHR_vulkan_enable2 is not offered".into(),
            ));
        }
        let mut extensions = xr::ExtensionSet::default();
        extensions.khr_vulkan_enable2 = true;
        extensions.ext_hand_tracking = available.ext_hand_tracking;
        extensions.ext_eye_gaze_interaction = available.ext_eye_gaze_interaction;
        #[cfg(target_os = "android")]
        {
            extensions.khr_android_create_instance = true;
        }
        let instance = entry
            .create_instance(
                &xr::ApplicationInfo {
                    application_name: "Xenogears Ex Machina",
                    application_version: 0,
                    engine_name: "xem",
                    engine_version: 0,
                    api_version: xr::Version::new(1, 0, 0),
                },
                &extensions,
                &[],
                platform,
            )
            .map_err(XrError::xr("xrCreateInstance"))?;
        let properties = instance
            .properties()
            .map_err(XrError::xr("xrGetInstanceProperties"))?;
        let system = instance
            .system(xr::FormFactor::HEAD_MOUNTED_DISPLAY)
            .map_err(XrError::xr("xrGetSystem"))?;
        let system_properties = instance
            .system_properties(system)
            .map_err(XrError::xr("xrGetSystemProperties"))?;
        let views = instance
            .enumerate_view_configuration_views(system, VIEW_TYPE)
            .map_err(XrError::xr("xrEnumerateViewConfigurationViews"))?;
        if views.len() != 2 {
            return Err(XrError::new(
                "view configuration",
                format!("primary stereo has {} views", views.len()),
            ));
        }
        let blend_mode = *instance
            .enumerate_environment_blend_modes(system, VIEW_TYPE)
            .map_err(XrError::xr("xrEnumerateEnvironmentBlendModes"))?
            .first()
            .ok_or_else(|| XrError::new("blend modes", "none offered".into()))?;
        let hand_tracking = extensions.ext_hand_tracking
            && instance
                .supports_hand_tracking(system)
                .map_err(XrError::xr("xrGetSystemProperties"))?;
        let eye_gaze = extensions.ext_eye_gaze_interaction && supports_eye_gaze(&instance, system)?;

        let (gpu, binding) = vulkan::create_gpu(&instance, system)?;
        let info = gpu.adapter.get_info();
        let capabilities = Capabilities {
            runtime: format!("{} {}", properties.runtime_name, properties.runtime_version),
            system: system_properties.system_name,
            vendor_id: system_properties.vendor_id,
            hand_tracking,
            eye_gaze,
            eye_size: (
                views[0].recommended_image_rect_width,
                views[0].recommended_image_rect_height,
            ),
            max_layers: system_properties.graphics_properties.max_layer_count,
            blend_mode,
            gpu: format!("{} ({})", info.name, info.driver),
        };
        Ok(Self {
            gpu,
            binding,
            capabilities,
            system,
            instance,
        })
    }

    pub fn gpu(&self) -> &Gpu {
        &self.gpu
    }

    pub fn capabilities(&self) -> &Capabilities {
        &self.capabilities
    }

    pub fn vulkan(&self) -> VulkanBinding {
        self.binding
    }

    pub fn instance(&self) -> &xr::Instance {
        &self.instance
    }

    /// Creates the session on the shared device and queue, its stereo eye
    /// swapchain and, with `panel` set, a quad-layer swapchain of that size.
    pub fn create_session(&self, config: &SessionConfig) -> Result<XrSession, XrError> {
        XrSession::new(self, config)
    }
}

fn supports_eye_gaze(instance: &xr::Instance, system: xr::SystemId) -> Result<bool, XrError> {
    let mut gaze = xr::sys::SystemEyeGazeInteractionPropertiesEXT {
        ty: xr::sys::SystemEyeGazeInteractionPropertiesEXT::TYPE,
        next: std::ptr::null_mut(),
        supports_eye_gaze_interaction: false.into(),
    };
    // SAFETY: an output structure chain of the right types for the call.
    let mut properties: xr::sys::SystemProperties = unsafe { std::mem::zeroed() };
    properties.ty = xr::sys::SystemProperties::TYPE;
    properties.next = std::ptr::from_mut(&mut gaze).cast();
    let result = unsafe {
        (instance.fp().get_system_properties)(instance.as_raw(), system, &mut properties)
    };
    if result.into_raw() < 0 {
        return Err(XrError::xr("xrGetSystemProperties")(result));
    }
    Ok(gaze.supports_eye_gaze_interaction.into())
}

#[derive(Clone, Debug)]
pub struct SessionConfig {
    /// The panel's swapchain size in pixels; `None` creates no quad layer.
    pub panel: Option<(u32, u32)>,
    /// Lets the host copy rendered eye and panel images out (adds transfer-source
    /// usage to the swapchains), for tests and captures.
    pub readback: bool,
    /// Near and far planes of the eye projections, metres.
    pub depth_range: (f32, f32),
}

impl Default for SessionConfig {
    fn default() -> Self {
        Self {
            panel: None,
            readback: false,
            depth_range: (0.05, 200.0),
        }
    }
}

/// A swapchain whose images are wrapped as wgpu textures.
struct Swapchain {
    // The textures go before the swapchain that owns their images.
    textures: Vec<wgpu::Texture>,
    handle: xr::Swapchain<xr::Vulkan>,
    size: (u32, u32),
    /// The image last released, which a layer shows.
    released: Option<u32>,
}

impl Swapchain {
    fn new(
        session: &xr::Session<xr::Vulkan>,
        device: &wgpu::Device,
        format: (u32, wgpu::TextureFormat),
        size: (u32, u32),
        layers: u32,
        usage: wgpu::TextureUsages,
        label: &'static str,
    ) -> Result<Self, XrError> {
        let mut flags = xr::SwapchainUsageFlags::COLOR_ATTACHMENT;
        if usage.contains(wgpu::TextureUsages::COPY_SRC) {
            flags |= xr::SwapchainUsageFlags::TRANSFER_SRC;
        }
        if usage.contains(wgpu::TextureUsages::COPY_DST) {
            flags |= xr::SwapchainUsageFlags::TRANSFER_DST;
        }
        if usage.contains(wgpu::TextureUsages::TEXTURE_BINDING) {
            flags |= xr::SwapchainUsageFlags::SAMPLED;
        }
        let handle = session
            .create_swapchain(&xr::SwapchainCreateInfo {
                create_flags: xr::SwapchainCreateFlags::EMPTY,
                usage_flags: flags,
                format: format.0,
                sample_count: 1,
                width: size.0,
                height: size.1,
                face_count: 1,
                array_size: layers,
                mip_count: 1,
            })
            .map_err(XrError::xr("xrCreateSwapchain"))?;
        let images = handle
            .enumerate_images()
            .map_err(XrError::xr("xrEnumerateSwapchainImages"))?;
        let desc = wgpu::TextureDescriptor {
            label: Some(label),
            size: wgpu::Extent3d {
                width: size.0,
                height: size.1,
                depth_or_array_layers: layers,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: wgpu::TextureDimension::D2,
            format: format.1,
            usage: usage | wgpu::TextureUsages::RENDER_ATTACHMENT,
            view_formats: &[],
        };
        let textures = images
            .into_iter()
            // SAFETY: images of a swapchain created on this device with `desc`;
            // the textures are dropped before the swapchain.
            .map(|image| unsafe { vulkan::wrap_swapchain_image(device, image, &desc) })
            .collect();
        Ok(Self {
            textures,
            handle,
            size,
            released: None,
        })
    }

    fn rect(&self) -> xr::Rect2Di {
        xr::Rect2Di {
            offset: xr::Offset2Di { x: 0, y: 0 },
            extent: xr::Extent2Di {
                width: self.size.0 as i32,
                height: self.size.1 as i32,
            },
        }
    }
}

/// The session lifecycle state, as the runtime reported it last.
pub type SessionState = xr::SessionState;

/// One step of the swapchain protocol, in call order, for logs and checks.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SwapchainOp {
    Acquire { swapchain: Target, image: u32 },
    Wait { swapchain: Target },
    Submit { swapchain: Target },
    Release { swapchain: Target },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Target {
    Eyes,
    Panel,
}

/// What the event pump saw.
#[derive(Clone, Debug, PartialEq)]
pub enum SessionEvent {
    StateChanged { state: SessionState, time: i64 },
    InstanceLossPending,
    EventsLost(u32),
    InteractionProfileChanged,
    ReferenceSpaceChangePending,
}

/// One eye's view at a frame's predicted display time.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EyeView {
    /// The eye in the session's reference space (LOCAL).
    pub pose: Pose,
    pub fov: Fov,
    /// World-to-eye, for a world that is the reference space.
    pub view: glam::Mat4,
    /// Asymmetric projection to wgpu clip space.
    pub proj: glam::Mat4,
}

/// A frame between `xrWaitFrame`/`xrBeginFrame` and `xrEndFrame`.
pub struct Frame {
    pub predicted_display_time: xr::Time,
    pub predicted_display_period: xr::Duration,
    pub should_render: bool,
    /// Located at the predicted display time; flags say whether tracked.
    pub eyes: [EyeView; 2],
    pub views_valid: bool,
    /// The head (VIEW space) in the reference space.
    pub head: Option<Pose>,
    pub input: InputState,
    pub ops: Vec<SwapchainOp>,
    rendered: bool,
}

/// The eye swapchain image being rendered: one texture of two array layers,
/// layer 0 the left eye.
pub struct EyeTarget<'a> {
    pub texture: &'a wgpu::Texture,
    pub views: [&'a wgpu::TextureView; 2],
    pub size: (u32, u32),
    pub image: u32,
}

/// Where the panel quad layer is shown, in the reference space.
#[derive(Clone, Copy, Debug)]
pub struct PanelPlacement {
    pub pose: Pose,
    /// Width and height in metres.
    pub size: (f32, f32),
}

/// A running OpenXR session on the shared wgpu device.
pub struct XrSession {
    // Field order is drop order: everything that belongs to the session goes
    // before it, and the wgpu device (in `gpu`) outlives the session.
    eye_views: Vec<[wgpu::TextureView; 2]>,
    eyes: Swapchain,
    panel: Option<Swapchain>,
    input: input::Input,
    view_space: xr::Space,
    space: xr::Space,
    frame_waiter: xr::FrameWaiter,
    frame_stream: xr::FrameStream<xr::Vulkan>,
    session: xr::Session<xr::Vulkan>,
    instance: xr::Instance,
    gpu: Gpu,
    blend_mode: xr::EnvironmentBlendMode,
    depth_range: (f32, f32),
    state: SessionState,
    running: bool,
    exiting: bool,
    frame: Option<Frame>,
}

impl XrSession {
    fn new(context: &XrContext, config: &SessionConfig) -> Result<Self, XrError> {
        let binding = context.binding;
        let instance = context.instance.clone();
        // SAFETY: the Vulkan objects were created for this system through the
        // runtime and stay alive (owned by wgpu) until after the session.
        let (session, frame_waiter, frame_stream) = unsafe {
            instance.create_session::<xr::Vulkan>(
                context.system,
                &xr::vulkan::SessionCreateInfo {
                    instance: ash::vk::Handle::as_raw(binding.instance) as _,
                    physical_device: ash::vk::Handle::as_raw(binding.physical_device) as _,
                    device: ash::vk::Handle::as_raw(binding.device) as _,
                    queue_family_index: binding.queue_family_index,
                    queue_index: binding.queue_index,
                },
            )
        }
        .map_err(XrError::xr("xrCreateSession"))?;

        let formats = session
            .enumerate_swapchain_formats()
            .map_err(XrError::xr("xrEnumerateSwapchainFormats"))?;
        // The runtime lists formats in its order of preference; take its first
        // sRGB colour format the renderer handles (the scene lights in linear
        // space and the panel holds sRGB-encoded bytes).
        let pick = |srgb_only: bool, rgba_only: bool| {
            formats.iter().find_map(|&format| {
                let wgpu = vulkan::wgpu_format(format)?;
                let rgba = matches!(
                    wgpu,
                    wgpu::TextureFormat::Rgba8UnormSrgb | wgpu::TextureFormat::Rgba8Unorm
                );
                ((!srgb_only || wgpu.is_srgb()) && (!rgba_only || rgba)).then_some((format, wgpu))
            })
        };
        let eye_format = pick(true, false)
            .or_else(|| pick(false, false))
            .ok_or_else(|| {
                XrError::new("swapchain formats", format!("none usable in {formats:?}"))
            })?;
        let readback = if config.readback {
            wgpu::TextureUsages::COPY_SRC
        } else {
            wgpu::TextureUsages::empty()
        };
        let device = &context.gpu.device;
        let eyes = Swapchain::new(
            &session,
            device,
            eye_format,
            context.capabilities.eye_size,
            2,
            readback,
            "xr eyes",
        )?;
        let eye_views = eyes
            .textures
            .iter()
            .map(|texture| {
                [0, 1].map(|layer| {
                    texture.create_view(&wgpu::TextureViewDescriptor {
                        label: Some("xr eye"),
                        dimension: Some(wgpu::TextureViewDimension::D2),
                        base_array_layer: layer,
                        array_layer_count: Some(1),
                        ..Default::default()
                    })
                })
            })
            .collect();
        let panel = match config.panel {
            // The panel texture is copied in: it needs an RGBA layout.
            Some(size) => {
                let format = pick(true, true).ok_or_else(|| {
                    XrError::new(
                        "swapchain formats",
                        format!("no RGBA8 format in {formats:?}"),
                    )
                })?;
                Some(Swapchain::new(
                    &session,
                    device,
                    format,
                    size,
                    1,
                    readback | wgpu::TextureUsages::COPY_DST,
                    "xr panel",
                )?)
            }
            None => None,
        };
        let space = session
            .create_reference_space(xr::ReferenceSpaceType::LOCAL, xr::Posef::IDENTITY)
            .map_err(XrError::xr("xrCreateReferenceSpace"))?;
        let view_space = session
            .create_reference_space(xr::ReferenceSpaceType::VIEW, xr::Posef::IDENTITY)
            .map_err(XrError::xr("xrCreateReferenceSpace"))?;
        let input = input::Input::new(&instance, &session, &context.capabilities)?;
        Ok(Self {
            eye_views,
            eyes,
            panel,
            input,
            view_space,
            space,
            frame_waiter,
            frame_stream,
            session,
            instance,
            gpu: context.gpu.clone(),
            blend_mode: context.capabilities.blend_mode,
            depth_range: config.depth_range,
            state: SessionState::IDLE,
            running: false,
            exiting: false,
            frame: None,
        })
    }

    pub fn state(&self) -> SessionState {
        self.state
    }

    /// Between `xrBeginSession` and `xrEndSession`.
    pub fn is_running(&self) -> bool {
        self.running
    }

    /// The session reached EXITING or LOSS_PENDING, or the instance is being
    /// lost: the host should drop the session.
    pub fn should_exit(&self) -> bool {
        self.exiting
    }

    pub fn eye_format(&self) -> wgpu::TextureFormat {
        self.eyes.textures[0].format()
    }

    pub fn eye_size(&self) -> (u32, u32) {
        self.eyes.size
    }

    pub fn panel_size(&self) -> Option<(u32, u32)> {
        self.panel.as_ref().map(|panel| panel.size)
    }

    /// Asks the runtime to end the session; it moves through STOPPING to
    /// EXITING, which [`Self::poll_events`] handles.
    pub fn request_exit(&self) -> Result<(), XrError> {
        match self.session.request_exit() {
            Ok(()) | Err(xr::sys::Result::ERROR_SESSION_NOT_RUNNING) => Ok(()),
            Err(error) => Err(XrError::xr("xrRequestExitSession")(error)),
        }
    }

    /// Drains the runtime's events, beginning the session at READY and ending
    /// it at STOPPING.
    pub fn poll_events(&mut self) -> Result<Vec<SessionEvent>, XrError> {
        let mut events = Vec::new();
        let mut buffer = xr::EventDataBuffer::new();
        while let Some(event) = self
            .instance
            .poll_event(&mut buffer)
            .map_err(XrError::xr("xrPollEvent"))?
        {
            use xr::Event;
            match event {
                Event::SessionStateChanged(change) if change.session() == self.session.as_raw() => {
                    let state = change.state();
                    self.state = state;
                    events.push(SessionEvent::StateChanged {
                        state,
                        time: change.time().as_nanos(),
                    });
                    match state {
                        SessionState::READY => {
                            self.session
                                .begin(VIEW_TYPE)
                                .map_err(XrError::xr("xrBeginSession"))?;
                            self.running = true;
                        }
                        SessionState::STOPPING => {
                            self.session.end().map_err(XrError::xr("xrEndSession"))?;
                            self.running = false;
                        }
                        SessionState::EXITING | SessionState::LOSS_PENDING => {
                            self.exiting = true;
                        }
                        _ => {}
                    }
                }
                Event::InstanceLossPending(_) => {
                    self.exiting = true;
                    events.push(SessionEvent::InstanceLossPending);
                }
                Event::EventsLost(lost) => {
                    events.push(SessionEvent::EventsLost(lost.lost_event_count()));
                }
                Event::InteractionProfileChanged(_) => {
                    events.push(SessionEvent::InteractionProfileChanged);
                }
                Event::ReferenceSpaceChangePending(_) => {
                    events.push(SessionEvent::ReferenceSpaceChangePending);
                }
                _ => {}
            }
        }
        Ok(events)
    }

    /// Waits for the runtime's frame slot (`xrWaitFrame`), begins the frame and
    /// locates the views, head and input at its predicted display time.
    /// Returns `None` when the session is not running.
    pub fn begin_frame(&mut self) -> Result<Option<&mut Frame>, XrError> {
        assert!(self.frame.is_none(), "end the previous frame first");
        if !self.running {
            return Ok(None);
        }
        let state = self
            .frame_waiter
            .wait()
            .map_err(XrError::xr("xrWaitFrame"))?;
        self.frame_stream
            .begin()
            .map_err(XrError::xr("xrBeginFrame"))?;
        let time = state.predicted_display_time;
        let (flags, views) = self
            .session
            .locate_views(VIEW_TYPE, time, &self.space)
            .map_err(XrError::xr("xrLocateViews"))?;
        let valid = xr::ViewStateFlags::POSITION_VALID | xr::ViewStateFlags::ORIENTATION_VALID;
        let (near, far) = self.depth_range;
        let eye = |view: &xr::View| {
            let pose = Pose::from_xr(view.pose);
            let fov = Fov::from_xr(view.fov);
            EyeView {
                pose,
                fov,
                view: pose.matrix().inverse(),
                proj: fov.projection(near, far),
            }
        };
        let head = self
            .view_space
            .locate(&self.space, time)
            .map_err(XrError::xr("xrLocateSpace"))?;
        let head_valid = head.location_flags.contains(
            xr::SpaceLocationFlags::POSITION_VALID | xr::SpaceLocationFlags::ORIENTATION_VALID,
        );
        let input = self.input.sample(&self.session, &self.space, time)?;
        Ok(Some(self.frame.insert(Frame {
            predicted_display_time: time,
            predicted_display_period: state.predicted_display_period,
            should_render: state.should_render,
            eyes: [eye(&views[0]), eye(&views[1])],
            views_valid: flags.contains(valid),
            head: head_valid.then(|| Pose::from_xr(head.pose)),
            input,
            ops: Vec::new(),
            rendered: false,
        })))
    }

    /// The frame begun by [`Self::begin_frame`].
    pub fn frame(&self) -> Option<&Frame> {
        self.frame.as_ref()
    }

    /// Acquires and waits for the next eye image, lets `record` encode into it,
    /// returns it to `COLOR_ATTACHMENT_OPTIMAL`, submits to the shared queue and
    /// releases the image. Only for frames that should render.
    pub fn render_eyes(
        &mut self,
        record: impl FnOnce(&mut wgpu::CommandEncoder, &EyeTarget),
    ) -> Result<(), XrError> {
        let frame = self.frame.as_mut().expect("render inside a frame");
        assert!(frame.should_render, "the runtime asked not to render");
        let image = acquire(&mut self.eyes, Target::Eyes, &mut frame.ops)?;
        let texture = &self.eyes.textures[image as usize];
        let [left, right] = &self.eye_views[image as usize];
        let target = EyeTarget {
            texture,
            views: [left, right],
            size: self.eyes.size,
            image,
        };
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        record(&mut encoder, &target);
        submit(&self.gpu, encoder, texture, Target::Eyes, &mut frame.ops);
        release(&mut self.eyes, image, Target::Eyes, &mut frame.ops)?;
        frame.rendered = true;
        Ok(())
    }

    /// Updates the panel layer's image the same way: `record` writes the new
    /// panel (usually a copy of the Slint texture). Frames that do not call it
    /// keep showing the last released panel image.
    pub fn render_panel(
        &mut self,
        record: impl FnOnce(&mut wgpu::CommandEncoder, &wgpu::Texture),
    ) -> Result<(), XrError> {
        let frame = self.frame.as_mut().expect("render inside a frame");
        let panel = self.panel.as_mut().expect("a session with a panel");
        let image = acquire(panel, Target::Panel, &mut frame.ops)?;
        let texture = &panel.textures[image as usize];
        let mut encoder = self.gpu.device.create_command_encoder(&Default::default());
        record(&mut encoder, texture);
        submit(&self.gpu, encoder, texture, Target::Panel, &mut frame.ops);
        release(panel, image, Target::Panel, &mut frame.ops)?;
        Ok(())
    }

    /// Ends the frame (`xrEndFrame`): the projection layer if the eyes were
    /// rendered, then the panel quad layer at `panel` if its swapchain has an
    /// image. Returns the frame, with the layers it submitted.
    pub fn end_frame(&mut self, panel: Option<PanelPlacement>) -> Result<EndedFrame, XrError> {
        let frame = self.frame.take().expect("end a begun frame");
        let rect = self.eyes.rect();
        let projection_views = [0, 1].map(|eye| {
            xr::CompositionLayerProjectionView::new()
                .pose(frame.eyes[eye].pose.to_xr())
                .fov(frame.eyes[eye].fov.to_xr())
                .sub_image(
                    xr::SwapchainSubImage::new()
                        .swapchain(&self.eyes.handle)
                        .image_array_index(eye as u32)
                        .image_rect(rect),
                )
        });
        let projection = xr::CompositionLayerProjection::new()
            .space(&self.space)
            .views(&projection_views);
        let quad = match (&self.panel, panel) {
            (Some(swapchain), Some(placement)) if swapchain.released.is_some() => Some(
                xr::CompositionLayerQuad::new()
                    .layer_flags(xr::CompositionLayerFlags::BLEND_TEXTURE_SOURCE_ALPHA)
                    .space(&self.space)
                    .eye_visibility(xr::EyeVisibility::BOTH)
                    .sub_image(
                        xr::SwapchainSubImage::new()
                            .swapchain(&swapchain.handle)
                            .image_array_index(0)
                            .image_rect(swapchain.rect()),
                    )
                    .pose(placement.pose.to_xr())
                    .size(xr::Extent2Df {
                        width: placement.size.0,
                        height: placement.size.1,
                    }),
            ),
            _ => None,
        };
        let mut layers: Vec<&xr::CompositionLayerBase<xr::Vulkan>> = Vec::new();
        let mut submitted = Vec::new();
        if frame.rendered && frame.should_render {
            layers.push(&projection);
            submitted.push(Layer::Projection);
        }
        if let Some(quad) = &quad
            && frame.should_render
        {
            layers.push(quad);
            submitted.push(Layer::Quad);
        }
        self.frame_stream
            .end(frame.predicted_display_time, self.blend_mode, &layers)
            .map_err(XrError::xr("xrEndFrame"))?;
        Ok(EndedFrame {
            frame,
            layers: submitted,
        })
    }
}

impl Drop for XrSession {
    fn drop(&mut self) {
        // wgpu may still reference swapchain images in work it has not
        // retired; finish it before the runtime destroys the images.
        let _ = self.gpu.device.poll(wgpu::PollType::wait_indefinitely());
        self.eye_views.clear();
        self.eyes.textures.clear();
        if let Some(panel) = &mut self.panel {
            panel.textures.clear();
        }
        let _ = self.gpu.device.poll(wgpu::PollType::wait_indefinitely());
    }
}

/// A composition layer submitted by `xrEndFrame`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Layer {
    Projection,
    Quad,
}

pub struct EndedFrame {
    pub frame: Frame,
    pub layers: Vec<Layer>,
}

fn acquire(
    swapchain: &mut Swapchain,
    target: Target,
    ops: &mut Vec<SwapchainOp>,
) -> Result<u32, XrError> {
    let image = swapchain
        .handle
        .acquire_image()
        .map_err(XrError::xr("xrAcquireSwapchainImage"))?;
    ops.push(SwapchainOp::Acquire {
        swapchain: target,
        image,
    });
    swapchain
        .handle
        .wait_image(xr::Duration::INFINITE)
        .map_err(XrError::xr("xrWaitSwapchainImage"))?;
    ops.push(SwapchainOp::Wait { swapchain: target });
    Ok(image)
}

fn submit(
    gpu: &Gpu,
    mut encoder: wgpu::CommandEncoder,
    texture: &wgpu::Texture,
    target: Target,
    ops: &mut Vec<SwapchainOp>,
) {
    // XR_KHR_vulkan_enable2: released colour images are in
    // COLOR_ATTACHMENT_OPTIMAL, whatever the frame did with them.
    encoder.transition_resources(
        std::iter::empty(),
        std::iter::once(wgpu::TextureTransition {
            texture,
            selector: None,
            state: wgpu::TextureUses::COLOR_TARGET,
        }),
    );
    gpu.queue.submit([encoder.finish()]);
    ops.push(SwapchainOp::Submit { swapchain: target });
}

fn release(
    swapchain: &mut Swapchain,
    image: u32,
    target: Target,
    ops: &mut Vec<SwapchainOp>,
) -> Result<(), XrError> {
    swapchain
        .handle
        .release_image()
        .map_err(XrError::xr("xrReleaseSwapchainImage"))?;
    swapchain.released = Some(image);
    ops.push(SwapchainOp::Release { swapchain: target });
    Ok(())
}
