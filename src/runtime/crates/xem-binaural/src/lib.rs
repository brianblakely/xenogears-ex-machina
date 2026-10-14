//! Binaural presentation of the SPU's voices for headphones (plan.md,
//! Phase 8, Binaural; docs/runtime.md, Binaural sound mode).
//!
//! The SPU says what each output sample was mixed from
//! (`xem_spu::Spu::run_tapped`): every voice's signal before its volume and
//! the volumes it was mixed with, the reverb return, the CD input and the
//! main volume. [`Renderer`] mixes the same parts again: a voice the
//! caller routes to a source loses its pan and is placed by the source's
//! direction through [`hrtf`] filters instead, at the gain the driver
//! resolved for it before panning ([`pan`]); every other voice, the reverb
//! return and the CD input (unless it is premixed music to spread, a
//! labelled estimate) keep the mix the SPU gave them, at the same main
//! volume. Nothing is added: no distance law, no room, no level of its
//! own. The output has exactly two channels.
//!
//! The renderer works in blocks of [`BLOCK`] samples (2.9 ms; its only
//! delay): each source's block is convolved with its direction's filters
//! by FFT ([`fft`]), directions glide between blocks, and switching the
//! presentation on or off, or a source between placed and original,
//! crossfades. It is the same Rust on every host (native and WebAssembly);
//! [`music`] holds the rules that arrange a sequence's parts around the
//! listener.

pub mod fft;
pub mod hrtf;
pub mod music;
pub mod pan;
mod render;

pub use render::{CdRoute, Change, JOURNAL_LIMIT, Journal, Renderer, Route, SourceStatus, Target};

/// The SPU's output rate, which the renderer keeps.
pub const SAMPLE_RATE: u32 = 44_100;
/// Samples per rendered block.
pub const BLOCK: usize = 128;
