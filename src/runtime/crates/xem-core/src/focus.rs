//! A transient request for the original game's pause screen. The game still
//! runs and decides when pausing is legal; this is never saved in a game state.
#[derive(Default)]
pub(crate) struct FocusPause {
    active: bool,
    handled: bool,
}
impl FocusPause {
    pub fn active(&self) -> bool { self.active }
    pub fn requested(&self) -> bool { self.active && !self.handled }
    pub fn set_active(&mut self, active: bool) {
        if self.active != active {
            self.active = active;
            self.handled = false;
        }
    }
    pub fn reached(&mut self) { self.handled = true; }
}
