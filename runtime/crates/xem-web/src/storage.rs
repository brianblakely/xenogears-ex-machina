//! Browser persistence the runtime reaches synchronously: settings in
//! `localStorage`. Save blobs go to IndexedDB from the page (asynchronous).

use std::io;

use xem_settings::SettingsStore;

/// Settings as JSON text under one `localStorage` key.
pub struct LocalStorageStore {
    pub key: &'static str,
}

fn storage() -> io::Result<web_sys::Storage> {
    web_sys::window()
        .and_then(|window| window.local_storage().ok().flatten())
        .ok_or_else(|| io::Error::new(io::ErrorKind::Unsupported, "localStorage is unavailable"))
}

fn js_error(error: wasm_bindgen::JsValue) -> io::Error {
    io::Error::other(crate::game::describe(&error))
}

impl SettingsStore for LocalStorageStore {
    fn load(&self) -> io::Result<Option<String>> {
        storage()?.get_item(self.key).map_err(js_error)
    }

    fn save(&mut self, json: &str) -> io::Result<()> {
        storage()?.set_item(self.key, json).map_err(js_error)
    }
}

/// A formatted, empty PlayStation memory card (128 KiB: header frame,
/// fifteen free directory frames, the broken-sector list, the header copy):
/// the save placeholder until the port's memory card service writes real ones.
pub fn blank_memory_card() -> Vec<u8> {
    const FRAME: usize = 128;
    let mut card = vec![0u8; 128 * 1024];
    let checksum = |frame: &mut [u8]| frame[FRAME - 1] = frame[..FRAME - 1].iter().fold(0, |x, b| x ^ b);
    let mut header = [0u8; FRAME];
    header[..2].copy_from_slice(b"MC");
    checksum(&mut header);
    card[..FRAME].copy_from_slice(&header);
    card[63 * FRAME..64 * FRAME].copy_from_slice(&header);
    for n in 1..16 {
        let frame = &mut card[n * FRAME..(n + 1) * FRAME];
        frame[0] = 0xA0;
        frame[8..10].copy_from_slice(&[0xFF, 0xFF]);
        checksum(frame);
    }
    for n in 16..36 {
        let frame = &mut card[n * FRAME..(n + 1) * FRAME];
        frame[..4].copy_from_slice(&[0xFF; 4]);
        frame[8..10].copy_from_slice(&[0xFF, 0xFF]);
        checksum(frame);
    }
    card
}
