//! Memory cards: the BIOS file calls over the host's storage.

use super::{Context, Device, unknown};
use crate::module::Action;

#[derive(Default)]
pub struct Card {}

impl Device for Card {
    fn import(&mut self, op: &str, _args: &[u32], context: &mut Context<'_>) -> Action {
        unknown("card", op, context)
    }
}
