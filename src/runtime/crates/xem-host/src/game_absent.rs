//! A build without the game module (it needs the user's matched PS1 builds):
//! no game can exist, so the hosts' game paths are unreachable.

/// No game runs in this build.
pub enum Game {}

impl Game {
    pub fn has_audio(&self) -> bool {
        match *self {}
    }

    pub fn vblanks(&mut self) -> u64 {
        match *self {}
    }

    pub fn update(&mut self, _buttons: u16, _settings: &xem_settings::Settings) -> Result<u32, String> {
        match *self {}
    }

    pub fn pause(&mut self) {
        match *self {}
    }

    pub fn resume(&mut self) {
        match *self {}
    }

    pub fn display(&mut self) -> (u32, u32, Vec<u8>) {
        match *self {}
    }
}
