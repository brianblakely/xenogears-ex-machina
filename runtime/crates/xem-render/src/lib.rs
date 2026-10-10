//! The wgpu renderer shared by every host: device creation, the stereo-capable
//! test scene and the compositor that places the game image and application
//! panels on a target. Hosts own windows, swapchains and frame scheduling; this
//! crate only records commands into the encoders they pass.

mod compositor;
mod gpu;
mod scene;

pub use compositor::{Compositor, Quad, Rect, present_rect};
#[cfg(not(target_arch = "wasm32"))]
pub use gpu::read_rgba8;
pub use gpu::{Gpu, GpuError};
pub use scene::{SceneRenderer, ViewTarget, Viewport, demo_camera};

pub use glam;
pub use wgpu;
