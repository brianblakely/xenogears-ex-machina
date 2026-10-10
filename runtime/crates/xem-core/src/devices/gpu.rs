//! GPU: libgpu's drawing, transfers and display (xem-gpu).

use super::{Context, Device, unknown};
use crate::module::Action;

#[derive(Default)]
pub struct Gpu {}

impl Device for Gpu {
    fn import(&mut self, op: &str, _args: &[u32], context: &mut Context<'_>) -> Action {
        unknown("gpu", op, context)
    }
}
