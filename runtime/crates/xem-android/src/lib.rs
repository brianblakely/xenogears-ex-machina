//! The Android host. SDL3's Java layer (`SDLActivity`, subclassed by
//! `runtime/android`'s `XemActivity`) loads this library as `libmain.so` and
//! runs [`SDL_main`](app::SDL_main) on its SDL thread.
//!
//! - Startup: SDL window, wgpu surface on the activity's native window
//!   (Vulkan; the GLES backend only when Vulkan is unavailable), the shared
//!   `xem_host::View` (test scene and Slint settings panel on one device).
//! - Assets: a bundled APK asset read through SDL (the AssetManager); the
//!   user's disc imported through the Storage Access Framework ([`disc`]);
//!   settings in the app's internal files directory.
//! - Lifecycle: SDL's application events drop the surface and stop the
//!   simulation clock ([`clock`]) in the background and recreate the surface
//!   on return, with no catch-up.

pub mod clock;

#[cfg(target_os = "android")]
mod app;
#[cfg(target_os = "android")]
mod disc;
