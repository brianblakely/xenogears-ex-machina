//! The SDL3 window and the raw handles wgpu presents to.

#[cfg(target_os = "android")]
use raw_window_handle::{AndroidDisplayHandle, AndroidNdkWindowHandle};
use raw_window_handle::{
    DisplayHandle, HandleError, HasDisplayHandle, HasWindowHandle, RawDisplayHandle,
    RawWindowHandle, WindowHandle,
};
#[cfg(not(target_os = "android"))]
use raw_window_handle::{
    WaylandDisplayHandle, WaylandWindowHandle, XlibDisplayHandle, XlibWindowHandle,
};
use sdl3_sys::everything::*;
use std::ffi::{CStr, CString, c_void};
use std::ptr::{NonNull, null_mut};

pub fn error() -> String {
    // SAFETY: SDL_GetError returns a valid, NUL-terminated thread-local string.
    unsafe { CStr::from_ptr(SDL_GetError()) }
        .to_string_lossy()
        .into_owned()
}

/// SDL itself: initialised on creation, shut down on drop (after the window).
pub struct Sdl;

impl Sdl {
    pub fn init() -> Result<Self, String> {
        // SAFETY: called once on the main thread before any other SDL call.
        let ok = unsafe {
            SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)
        };
        if ok {
            Ok(Sdl)
        } else {
            Err(format!("SDL_Init: {}", error()))
        }
    }
}

impl Drop for Sdl {
    fn drop(&mut self) {
        // SAFETY: every SDL object has been released by now.
        unsafe { SDL_Quit() };
    }
}

/// The display connection wgpu's instance needs for GL on Wayland/X11 (and the
/// empty Android display).
#[derive(Debug)]
pub struct SdlDisplay(RawDisplayHandle);

// SAFETY: the display connection is owned by SDL, outlives every wgpu object
// (they are dropped before the window and SDL) and is only used by wgpu.
unsafe impl Send for SdlDisplay {}
unsafe impl Sync for SdlDisplay {}

impl HasDisplayHandle for SdlDisplay {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        // SAFETY: valid for the life of the window, see above.
        Ok(unsafe { DisplayHandle::borrow_raw(self.0) })
    }
}

pub struct Window {
    pub raw: *mut SDL_Window,
    display: RawDisplayHandle,
}

impl Window {
    /// A resizable high-density window; on Android the activity's full screen.
    pub fn new(_sdl: &Sdl, title: &str, width: i32, height: i32) -> Result<Self, String> {
        let title = CString::new(title).expect("title without NUL");
        let mut flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if cfg!(target_os = "android") {
            flags |= SDL_WINDOW_FULLSCREEN;
        }
        // SAFETY: SDL is initialised; the title outlives the call.
        let raw = unsafe { SDL_CreateWindow(title.as_ptr(), width, height, flags) };
        if raw.is_null() {
            return Err(format!("SDL_CreateWindow: {}", error()));
        }
        // SAFETY: raw is a live window; the properties belong to it.
        let display = if cfg!(target_os = "android") {
            Ok(android_display())
        } else {
            unsafe { native_handles(raw) }.map(|(_, display)| display)
        };
        match display {
            Ok(display) => Ok(Self { raw, display }),
            Err(message) => {
                // SAFETY: raw was created above and is not used again.
                unsafe { SDL_DestroyWindow(raw) };
                Err(message)
            }
        }
    }

    pub fn display(&self) -> SdlDisplay {
        SdlDisplay(self.display)
    }

    /// The identity of the native surface the window presents to now: on
    /// Android none while the activity is in the background, and a new one
    /// whenever the activity's surface is recreated.
    pub fn native_surface(&self) -> Option<usize> {
        // SAFETY: live window.
        let (window, _) = unsafe { native_handles(self.raw) }.ok()?;
        Some(match window {
            RawWindowHandle::AndroidNdk(handle) => handle.a_native_window.as_ptr() as usize,
            RawWindowHandle::Wayland(handle) => handle.surface.as_ptr() as usize,
            RawWindowHandle::Xlib(handle) => handle.window as usize,
            _ => 0,
        })
    }

    pub fn has_native_surface(&self) -> bool {
        self.native_surface().is_some()
    }

    /// The drawable size in pixels.
    pub fn pixel_size(&self) -> (u32, u32) {
        let (mut width, mut height) = (0, 0);
        // SAFETY: live window, valid out pointers.
        unsafe { SDL_GetWindowSizeInPixels(self.raw, &mut width, &mut height) };
        (width.max(0) as u32, height.max(0) as u32)
    }

    /// Pixels per window coordinate (SDL event positions are window coordinates).
    pub fn pixel_density(&self) -> f32 {
        // SAFETY: live window.
        let density = unsafe { SDL_GetWindowPixelDensity(self.raw) };
        if density > 0.0 { density } else { 1.0 }
    }

    /// The user's content scale for this window's display, in pixels per
    /// logical UI pixel.
    pub fn display_scale(&self) -> f32 {
        // SAFETY: live window.
        let scale = unsafe { SDL_GetWindowDisplayScale(self.raw) };
        if scale > 0.0 { scale } else { 1.0 }
    }

    pub fn set_title(&self, title: &str) {
        let title = CString::new(title).expect("title without NUL");
        // SAFETY: live window; SDL copies the title.
        unsafe { SDL_SetWindowTitle(self.raw, title.as_ptr()) };
    }

    pub fn set_text_input(&self, enabled: bool) {
        // SAFETY: live window.
        unsafe {
            if enabled {
                SDL_StartTextInput(self.raw);
            } else {
                SDL_StopTextInput(self.raw);
            }
        }
    }
}

impl Drop for Window {
    fn drop(&mut self) {
        // SAFETY: the surface made from this window has been dropped first.
        unsafe { SDL_DestroyWindow(self.raw) };
    }
}

impl HasWindowHandle for Window {
    fn window_handle(&self) -> Result<WindowHandle<'_>, HandleError> {
        // Read each time: on Android the native window changes when the
        // activity's surface is recreated.
        // SAFETY: live window.
        let (window, _) =
            unsafe { native_handles(self.raw) }.map_err(|_| HandleError::Unavailable)?;
        // SAFETY: the handle is valid while the window (and on Android its
        // current surface) lives.
        Ok(unsafe { WindowHandle::borrow_raw(window) })
    }
}

impl HasDisplayHandle for Window {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        // SAFETY: the handle is valid while the window lives.
        Ok(unsafe { DisplayHandle::borrow_raw(self.display) })
    }
}

fn android_display() -> RawDisplayHandle {
    #[cfg(target_os = "android")]
    return AndroidDisplayHandle::new().into();
    #[cfg(not(target_os = "android"))]
    unreachable!("only Android windows have an Android display")
}

/// Reads the native handles of `window` from its SDL properties: Wayland or
/// X11 on the desktop, the ANativeWindow on Android.
///
/// # Safety
/// `window` must be a live SDL window.
unsafe fn native_handles(
    window: *mut SDL_Window,
) -> Result<(RawWindowHandle, RawDisplayHandle), String> {
    unsafe {
        let props = SDL_GetWindowProperties(window);
        let pointer =
            |name| NonNull::new(SDL_GetPointerProperty(props, name, null_mut::<c_void>()));
        #[cfg(target_os = "android")]
        {
            let Some(native) = pointer(SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER) else {
                return Err("SDL has no Android native window (in the background?)".into());
            };
            Ok((
                AndroidNdkWindowHandle::new(native).into(),
                android_display(),
            ))
        }
        #[cfg(not(target_os = "android"))]
        {
            let driver = CStr::from_ptr(SDL_GetCurrentVideoDriver()).to_string_lossy();
            match &*driver {
                "wayland" => {
                    let display = pointer(SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER);
                    let surface = pointer(SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER);
                    let (Some(display), Some(surface)) = (display, surface) else {
                        return Err("SDL gave no Wayland display/surface".into());
                    };
                    Ok((
                        WaylandWindowHandle::new(surface).into(),
                        WaylandDisplayHandle::new(display).into(),
                    ))
                }
                "x11" => {
                    let display = pointer(SDL_PROP_WINDOW_X11_DISPLAY_POINTER);
                    let screen = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_SCREEN_NUMBER, 0);
                    let xid = SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
                    if display.is_none() || xid == 0 {
                        return Err("SDL gave no X11 display/window".into());
                    }
                    Ok((
                        XlibWindowHandle::new(xid as _).into(),
                        XlibDisplayHandle::new(display, screen as _).into(),
                    ))
                }
                other => Err(format!(
                    "unsupported SDL video driver {other:?} (Wayland and X11 are)"
                )),
            }
        }
    }
}
