//! A build without the game module (it needs the user's matched PS1 builds):
//! no game can exist, so the hosts' game paths are unreachable.

/// No game runs in this build.
pub enum Game {}

impl Game {
    pub fn command(&mut self, _: &serde_json::Value) -> Result<serde_json::Value,String> { match *self {} }
    pub fn snapshot_bytes(&mut self) -> Result<Vec<u8>,String> { match *self {} }
    pub fn restore_bytes(&mut self, _: &[u8]) -> Result<(),String> { match *self {} }
    pub fn card_image(&mut self, _: usize) -> Option<Vec<u8>> { match *self {} }
    pub fn import_card(&mut self, _: usize, _: Vec<u8>) -> Result<(),String> { match *self {} }
    pub fn retry_storage(&mut self) -> Result<(),String> { match *self {} }
    pub fn configure_fast_forward(&mut self, _: xem_settings::FastForwardRate) -> Result<(),String> { match *self {} }
    pub fn sync_time_settings(&mut self, _: &xem_settings::Settings) -> Result<(),String> { match *self {} }
    pub fn configure_rewind(&mut self, _: bool, _: u32, _: u32) -> Result<(),String> { match *self {} }
    pub fn set_rewind_held(&mut self, _: bool, _: bool) -> Result<(),String> { match *self {} }
    pub fn time_status(&mut self) -> (bool,String) { match *self {} }
    pub fn storage_status(&mut self) -> String { match *self {} }
    pub fn has_audio(&self) -> bool {
        match *self {}
    }

    pub fn vblanks(&mut self) -> u64 {
        match *self {}
    }

    pub fn update(&mut self, _buttons: u16, _settings: &xem_settings::Settings, _focused: impl FnMut() -> bool) -> Result<u32, String> {
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

    pub fn take_frame(&mut self, _restart: bool) -> (xem_render::xem_gpu::Trace, xem_render::xem_gpu::DisplayInfo, String) {
        match *self {}
    }

    pub fn schedule(&mut self, _log: &serde_json::Value) -> Result<(), String> {
        match *self {}
    }

    pub fn frames_run(&self) -> u64 {
        match *self {}
    }

    pub fn memory_hash(&mut self) -> String {
        match *self {}
    }

    pub fn set_frame_limit(&mut self, _limit: Option<u64>) {
        match *self {}
    }
}
