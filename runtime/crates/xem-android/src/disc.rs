//! Disc import through the Storage Access Framework.
//!
//! SDL's open-file dialog is Android's ACTION_OPEN_DOCUMENT picker; the user's
//! choice comes back to `XemActivity` as a `content://` document URI. The
//! activity takes a persistable read grant on it and hands it over
//! ([`Java_dev_xem_app_XemActivity_nativeDocumentPicked`]), so the import
//! survives restarts: the URI is kept in the app's files directory and
//! reopened on the next start. SDL opens
//! the document through the ContentResolver (`SDL_IOFromFile`); its file
//! descriptor is duplicated into a Rust `File` and read in place by
//! `xem-disc` on a worker thread (no copy of the image is made).

use sdl3_sys::everything::*;
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fs::File;
use std::os::fd::BorrowedFd;
use std::path::PathBuf;
use std::sync::Mutex;
use std::sync::mpsc;
use xem_disc::identify::hex;
use xem_disc::{FileReadAt, identify, open_reader};

use crate::app::log;

/// Documents the picker returned, for the main loop.
static PICKED: Mutex<Vec<String>> = Mutex::new(Vec::new());

pub struct Identified {
    pub source: String,
    pub result: Result<String, String>,
}

pub struct DiscImport {
    persisted: PathBuf,
    sender: mpsc::Sender<Identified>,
    results: mpsc::Receiver<Identified>,
}

impl DiscImport {
    /// Keeps the imported document's URI under `files_dir`.
    pub fn new(files_dir: &std::path::Path) -> Self {
        let (sender, results) = mpsc::channel();
        Self {
            persisted: files_dir.join("imported-disc-uri"),
            sender,
            results,
        }
    }

    /// The document imported earlier, if any.
    pub fn persisted(&self) -> Option<String> {
        std::fs::read_to_string(&self.persisted)
            .ok()
            .map(|uri| uri.trim().to_string())
            .filter(|uri| !uri.is_empty())
    }

    /// Shows the system document picker.
    pub fn pick(&self, window: *mut SDL_Window) {
        log("disc picker opened (ACTION_OPEN_DOCUMENT)");
        // SAFETY: SDL is initialised; no filters (any type) and no default location.
        unsafe {
            SDL_ShowOpenFileDialog(
                Some(picked),
                std::ptr::null_mut(),
                window,
                std::ptr::null(),
                0,
                std::ptr::null(),
                false,
            )
        };
    }

    /// Opens `source` (a `content://` URI or a path) on this thread and
    /// identifies it on a worker.
    pub fn open(&self, source: &str) {
        let file = match open_document(source) {
            Ok(file) => file,
            Err(error) => {
                let _ = self.sender.send(Identified {
                    source: source.into(),
                    result: Err(error),
                });
                return;
            }
        };
        let sender = self.sender.clone();
        let source = source.to_string();
        std::thread::spawn(move || {
            let result = identify_file(file);
            let _ = sender.send(Identified { source, result });
        });
    }

    /// Opens what the picker returned and returns finished identifications;
    /// a successfully identified document is remembered.
    pub fn poll(&mut self) -> Vec<Identified> {
        let picked = std::mem::take(&mut *PICKED.lock().unwrap());
        for uri in picked {
            log(&format!("disc picked {uri}"));
            self.open(&uri);
        }
        let done: Vec<Identified> = self.results.try_iter().collect();
        for identified in &done {
            if identified.result.is_ok()
                && identified.source.starts_with("content://")
                && self.persisted().as_deref() != Some(identified.source.as_str())
            {
                match std::fs::write(&self.persisted, &identified.source) {
                    Ok(()) => log(&format!("disc import remembered {}", identified.source)),
                    Err(error) => log(&format!(
                        "disc import not remembered: {}: {error}",
                        self.persisted.display()
                    )),
                }
            }
        }
        done
    }
}

/// SDL_DialogFileCallback (on Android's UI thread): reports a failed or
/// cancelled pick; a chosen document arrives through
/// [`Java_dev_xem_app_XemActivity_nativeDocumentPicked`].
unsafe extern "C" fn picked(
    _userdata: *mut c_void,
    filelist: *const *const c_char,
    _filter: c_int,
) {
    if filelist.is_null() {
        log(&format!("disc picker failed: {}", xem_host::sdl::error()));
    // SAFETY: SDL passes a NULL-terminated array.
    } else if unsafe { (*filelist).is_null() } {
        log("disc picker cancelled");
    }
}

/// `XemActivity.nativeDocumentPicked(String)`: the document the picker
/// returned, after the activity took its persistable grant. It comes from the
/// activity rather than SDL's dialog callback because Android may destroy and
/// recreate the activity (and so restart SDL_main) while the picker is shown;
/// the queue is process-wide and drained by whichever run is current.
#[unsafe(no_mangle)]
pub unsafe extern "system" fn Java_dev_xem_app_XemActivity_nativeDocumentPicked(
    env: *mut jni_sys::JNIEnv,
    _class: jni_sys::jclass,
    uri: jni_sys::jstring,
) {
    // SAFETY: JNI passes a valid environment and string for this call.
    let uri = unsafe {
        let chars = ((**env).v1_1.GetStringUTFChars)(env, uri, std::ptr::null_mut());
        if chars.is_null() {
            return;
        }
        let text = CStr::from_ptr(chars).to_string_lossy().into_owned();
        ((**env).v1_1.ReleaseStringUTFChars)(env, uri, chars);
        text
    };
    PICKED.lock().unwrap().push(uri);
    // Wake the main loop if it is waiting for events (a no-op before SDL_Init).
    let mut event = SDL_Event::default();
    event.r#type = SDL_EVENT_USER.0;
    // SAFETY: SDL's event queue is thread-safe.
    unsafe { SDL_PushEvent(&mut event) };
}

/// A readable file for a `content://` document (through SDL and the
/// ContentResolver) or a path.
fn open_document(source: &str) -> Result<File, String> {
    if !source.starts_with("content://") {
        return File::open(source).map_err(|error| format!("{source}: {error}"));
    }
    let uri = CString::new(source).map_err(|_| "URI with NUL".to_string())?;
    // SAFETY: SDL is initialised; the strings outlive the call.
    let stream = unsafe { SDL_IOFromFile(uri.as_ptr(), c"rb".as_ptr()) };
    if stream.is_null() {
        return Err(format!("{source}: {}", xem_host::sdl::error()));
    }
    // SAFETY: stream is live until SDL_CloseIO below.
    let fd = unsafe {
        SDL_GetNumberProperty(
            SDL_GetIOProperties(stream),
            SDL_PROP_IOSTREAM_FILE_DESCRIPTOR_NUMBER,
            -1,
        )
    };
    let file = if fd >= 0 {
        // SAFETY: the descriptor is open while the stream is; it is duplicated
        // before SDL closes it.
        unsafe { BorrowedFd::borrow_raw(fd as i32) }
            .try_clone_to_owned()
            .map(File::from)
            .map_err(|error| format!("{source}: {error}"))
    } else {
        Err(format!("{source}: SDL gave no file descriptor"))
    };
    // SAFETY: closed once; the duplicate stays open.
    unsafe { SDL_CloseIO(stream) };
    file
}

fn identify_file(file: File) -> Result<String, String> {
    let reader = FileReadAt::new(file).map_err(|error| error.to_string())?;
    let bytes = xem_disc::ReadAt::len(&reader);
    let mut source = open_reader(reader).map_err(|error| error.to_string())?;
    let sectors = source.sector_count();
    let id = identify(&mut source).map_err(|error| error.to_string())?;
    let disc = match id.disc {
        Some(disc) => format!("Xenogears disc {} ({})", disc.number(), disc.serial()),
        None => "not a Xenogears disc".into(),
    };
    Ok(format!(
        "bytes={bytes} sectors={sectors} boot={} sha256={} disc={disc}",
        id.boot_path,
        hex(&id.sha256)
    ))
}
