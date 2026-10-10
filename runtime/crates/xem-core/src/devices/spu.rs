//! SPU: register window sync, transfers and mixing (xem-spu).

use super::{Context, Device, unknown};
use crate::module::Action;

#[derive(Default)]
pub struct Spu {}

impl Device for Spu {
    fn import(&mut self, op: &str, _args: &[u32], context: &mut Context<'_>) -> Action {
        unknown("spu", op, context)
    }
}
