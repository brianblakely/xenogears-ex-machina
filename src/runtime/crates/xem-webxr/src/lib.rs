//! The WebXR adapter of the browser runtime.
//!
//! It detects `navigator.xr` and immersive-vr support, requests a session on a
//! user gesture, makes the WebGL2 context XR-compatible and submits frames
//! through an `XRWebGLLayer`. wgpu's GLES adapter is created over that same
//! context, and the layer's framebuffer is wrapped as a wgpu texture, so the
//! shared `xem-render` scene reaches the XR compositor without a CPU copy.
//! While a session runs, its `requestAnimationFrame` schedules every frame;
//! the window's animation frames drive only the flat canvas. Input sources are
//! exposed as raw state (gamepad buttons and axes, target-ray and grip poses,
//! hand joints when granted), never as interpreted gestures.
//!
//! Submission is WebGL2. `XRGPUBinding` (WebGPU layers) is detected and
//! reported but not used: no available runtime could test it.

#![cfg(target_arch = "wasm32")]

mod app;
mod graphics;
mod input;

pub use app::XemXr;
pub use graphics::{EyeView, XrGraphics};
pub use input::{HAND_JOINTS, InputSource, Pose};
