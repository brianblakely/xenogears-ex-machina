//! Assisted input (plan.md, the shared headset hand/gaze contract): hand
//! gestures, gaze and pointer targets resolved to the game's legal commands
//! and typed actions. Nothing here moves the game but through the pad.
pub mod gesture;
pub mod synthetic;
pub mod dispatch;
pub mod headset;
pub mod overlay;
pub mod touch;
