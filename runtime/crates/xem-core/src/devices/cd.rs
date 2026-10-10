//! CD-ROM drive: commands, sector reads and XA audio over the user's disc (xem-disc, xem-media).

use super::{Context, Device, unknown};
use crate::module::Action;

#[derive(Default)]
pub struct Cd {}

impl Device for Cd {
    fn import(&mut self, op: &str, _args: &[u32], context: &mut Context<'_>) -> Action {
        unknown("cd", op, context)
    }
}
