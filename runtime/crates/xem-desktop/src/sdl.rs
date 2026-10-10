//! The SDL3 window and the raw handles wgpu presents to.

use raw_window_handle::{
    DisplayHandle, HandleError, HasDisplayHandle, HasWindowHandle, RawDisplayHandle,
    RawWindowHandle, WaylandDisplayHandle, WaylandWindowHandle, WindowHandle, XlibDisplayHandle,
    XlibWindowHandle,
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

/// The display connection wgpu's instance needs for GL on Wayland/X11.
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
    window: RawWindowHandle,
    display: RawDisplayHandle,
}

impl Window {
    pub fn new(_sdl: &Sdl, title: &str, width: i32, height: i32) -> Result<Self, String> {
        let title = CString::new(title).expect("title without NUL");
        // SAFETY: SDL is initialised; the title outlives the call.
        let raw = unsafe {
            SDL_CreateWindow(
                title.as_ptr(),
                width,
                height,
                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
            )
        };
        if raw.is_null() {
            return Err(format!("SDL_CreateWindow: {}", error()));
        }
        // SAFETY: raw is a live window; the properties belong to it.
        let handles = unsafe { native_handles(raw) };
        match handles {
            Ok((window, display)) => Ok(Self {
                raw,
                window,
                display,
            }),
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
        // SAFETY: the handle is valid while the window lives.
        Ok(unsafe { WindowHandle::borrow_raw(self.window) })
    }
}

impl HasDisplayHandle for Window {
    fn display_handle(&self) -> Result<DisplayHandle<'_>, HandleError> {
        // SAFETY: the handle is valid while the window lives.
        Ok(unsafe { DisplayHandle::borrow_raw(self.display) })
    }
}

/// Reads the Wayland or X11 handles of `window` from its SDL properties.
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
