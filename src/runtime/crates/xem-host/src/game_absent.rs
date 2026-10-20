//! A build without the game module (it needs the user's matched PS1 builds):
//! no game can exist, so the hosts' game paths are unreachable.

/// No game runs in this build.
pub enum Game {}

impl Game {
    pub fn command(&mut self, _: &serde_json::Value) -> Result<serde_json::Value,String> { match *self {} }
    pub fn snapshot_bytes(&mut self) -> Result<Vec<u8>,String> { match *self {} }
    pub fn debug_status(&self) -> (bool, &str) { match *self {} }
    pub fn restore_bytes(&mut self, _: &[u8]) -> Result<(),String> { match *self {} }
    pub fn card_image(&mut self, _: usize) -> Option<Vec<u8>> { match *self {} }
    pub fn import_card(&mut self, _: usize, _: &[u8], _: &str) -> Result<String,String> { match *self {} }
    pub fn retry_storage(&mut self) -> Result<(),String> { match *self {} }
    pub fn install_bios(&mut self, _: &[u8]) -> Result<(),String> { match *self {} }
    pub fn configure_fast_forward(&mut self, _: xem_settings::FastForwardRate) -> Result<(),String> { match *self {} }
    pub fn sync_time_settings(&mut self, _: &xem_settings::Settings) -> Result<(),String> { match *self {} }
    pub fn configure_rewind(&mut self, _: bool, _: u32, _: u32) -> Result<(),String> { match *self {} }
    pub fn set_rewind_held(&mut self, _: bool, _: bool) -> Result<(),String> { match *self {} }
    pub fn rewind_held(&self) -> bool { match *self {} }
    pub fn time_activity(&self) -> (bool,bool) { match *self {} }
    pub fn mouse_camera_active(&mut self, _: &xem_settings::Settings) -> bool { match *self {} }
    pub fn time_status(&mut self) -> (bool,String) { match *self {} }
    pub fn storage_status(&mut self) -> String { match *self {} }
    pub fn add_disc(&mut self, _: u8, _: Box<dyn xem_disc::SectorSource>) { match *self {} }
    pub fn wanted_disc(&mut self) -> Option<u8> { match *self {} }
    pub fn has_audio(&self) -> bool {
        match *self {}
    }

    pub fn vblanks(&mut self) -> u64 {
        match *self {}
    }

    pub fn pad_context(&mut self) -> crate::PadContext {
        match *self {}
    }

    pub fn targets(&mut self) -> Option<xem_core::menu::Targets> {
        match *self {}
    }

    pub fn field(&mut self) -> Option<xem_core::field::Field> {
        match *self {}
    }

    pub fn destination(&self) -> Option<xem_core::field::V3> {
        match *self {}
    }

    pub fn assist_state(&self) -> String {
        match *self {}
    }

    pub fn update(&mut self, _buttons: u16, _settings: &xem_settings::Settings, _focused: impl FnMut() -> bool) -> Result<u32, String> {
        match *self {}
    }

    pub fn follow_sound_mode(&mut self, _settings: &std::cell::RefCell<xem_settings::SettingsService>) {
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

    pub fn display_info(&mut self) -> xem_render::xem_gpu::DisplayInfo {
        match *self {}
    }

    pub fn take_frame(&mut self, _restart: bool, _reveals: Option<xem_render::xem_gpu::Reveals>) -> (xem_render::xem_gpu::Trace, xem_render::xem_gpu::DisplayInfo, String) {
        match *self {}
    }

    pub fn want_actor_view(&mut self, _view: u32) {
        match *self {}
    }

    pub fn want_attack_page(&mut self, _kept: bool) {
        match *self {}
    }

    pub fn set_listener_view(&mut self, _view: Option<xem_core::binaural::ListenerView>) {
        match *self {}
    }

    pub fn want_inspection(&mut self, _on: bool) {
        match *self {}
    }

    pub fn set_head(&mut self, _axes: Option<[[f32; 3]; 3]>) {
        match *self {}
    }

    pub fn stop_trace(&mut self) {
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
