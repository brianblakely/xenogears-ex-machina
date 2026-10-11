//! Browser persistence: settings in `localStorage` (synchronous); memory
//! cards and save blobs in IndexedDB, which the page reads and writes
//! (asynchronous).

use std::cell::RefCell;
use std::io;
use std::rc::Rc;

use xem_core::devices::card::{CardStorage, formatted};
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

/// The memory cards: the card device's `CardStorage` (synchronous) over
/// IndexedDB (asynchronous). The page reads both images from IndexedDB
/// before a session boots ([`set_image`](Self::set_image)); the device's
/// writes replace the image here at once and mark it dirty; the page takes
/// the dirty images after each animation frame and automation step
/// ([`take_write`](Self::take_write)) and writes them to IndexedDB. The image
/// here is the authority; IndexedDB trails it by at most the writes in flight.
#[derive(Clone, Default)]
pub struct BrowserCards(Rc<RefCell<Cards>>);

#[derive(Default)]
struct Cards {
    images: [Option<Vec<u8>>; 2],
    dirty: [bool; 2],
}

impl BrowserCards {
    pub fn image(&self, slot: usize) -> Option<Vec<u8>> { self.0.borrow().images[slot].clone() }
    /// The image IndexedDB holds for `slot` (None: a fresh formatted card).
    pub fn set_image(&self, slot: usize, image: Option<Vec<u8>>) {
        let mut cards = self.0.borrow_mut();
        cards.images[slot] = image;
        cards.dirty[slot] = false;
    }

    /// `slot`'s image if the device changed it since the last call.
    pub fn take_write(&self, slot: usize) -> Option<Vec<u8>> {
        let mut cards = self.0.borrow_mut();
        std::mem::take(&mut cards.dirty[slot]).then(|| cards.images[slot].clone()).flatten()
    }

    pub fn dirty(&self) -> bool {
        self.0.borrow().dirty.contains(&true)
    }
}

impl CardStorage for BrowserCards {
    fn load(&mut self, slot: usize) -> io::Result<Option<Vec<u8>>> {
        Ok(Some(self.0.borrow().images[slot].clone().unwrap_or_else(formatted)))
    }

    fn store(&mut self, slot: usize, image: &[u8]) -> io::Result<()> {
        let mut cards = self.0.borrow_mut();
        cards.images[slot] = Some(image.to_vec());
        cards.dirty[slot] = true;
        Ok(())
    }
}
