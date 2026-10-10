//! The PS1's movie hardware as deterministic host services: the MDEC
//! ([`mdec::Mdec`]) and the CD drive's XA-ADPCM audio path
//! ([`xa::XaAudio`]), plus a reference decoder for the MDEC bitstream
//! ([`bitstream`]) that the game itself decodes in software (libpress).
//!
//! Everything here is integer arithmetic implemented from psx-spx, with no
//! clock, threads or allocation-order effects: equal inputs give equal
//! outputs on every host, and each device saves and loads its whole state.

pub mod bitstream;
pub mod mdec;
mod snapshot;
pub mod xa;

pub use bitstream::{BitstreamError, BsHeader, RleFrame, StrSectorHeader};
pub use mdec::{Depth, Mdec};
pub use snapshot::SnapshotError;
pub use xa::{Coding, Delivery, Subheader, XaAudio};
