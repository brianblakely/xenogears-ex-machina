//! The game as the SDL3 hosts play it: a booted session paced at the
//! console's 60 frames a second by the wall clock (a stalled or paused host
//! drops time, never catches up) with the SPU's output on an SDL3 audio
//! stream.

use sdl3_sys::everything::*;
use std::time::{Duration, Instant};
use xem_core::control::Session;
use xem_game::NativeModule;

const FRAME: Duration = Duration::from_nanos(1_000_000_000 / 60);

pub struct Game {
    pub session: Session<NativeModule>,
    audio: *mut SDL_AudioStream,
    paced: Instant,
    samples: Vec<i16>,
}

impl Game {
    /// Plays `session` (booted with `audio_output`) from now; the audio
    /// stream opens on the default playback device, or the game runs silent.
    pub fn new(session: Session<NativeModule>) -> Self {
        let spec = SDL_AudioSpec {
            format: SDL_AUDIO_S16LE,
            channels: 2,
            freq: 44100,
        };
        // SAFETY: SDL is initialised with audio; a null callback queues data.
        let audio = unsafe {
            SDL_OpenAudioDeviceStream(
                SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                &spec,
                None,
                std::ptr::null_mut(),
            )
        };
        if !audio.is_null() {
            // SAFETY: an open stream.
            unsafe { SDL_ResumeAudioStreamDevice(audio) };
        }
        Game {
            session,
            audio,
            paced: Instant::now(),
            samples: Vec::new(),
        }
    }

    /// Whether the SPU's output reaches an audio device.
    pub fn has_audio(&self) -> bool {
        !self.audio.is_null()
    }

    /// The game's frame count: vertical blanks since boot.
    pub fn vblanks(&mut self) -> u64 {
        self.session.runtime.services().clock.vblanks
    }

    /// Runs the frames due by the wall clock (at most two per call: a stalled
    /// host drops time rather than catching up) with pad 1 holding
    /// `buttons`, then queues their sound at `volume` percent. Returns the
    /// frames run.
    pub fn update(&mut self, buttons: u16, volume: u8) -> Result<u32, String> {
        let mut due = 0;
        while self.paced + FRAME <= Instant::now() && due < 2 {
            self.paced += FRAME;
            due += 1;
        }
        if self.paced + FRAME <= Instant::now() {
            self.paced = Instant::now();
        }
        for _ in 0..due {
            self.session.runtime.services().pads.0[0].buttons = buttons;
            self.session.frame().map_err(|trap| trap.to_string())?;
        }
        let spu = &mut self.session.runtime.services().spu;
        self.samples.resize(spu.audio_queued(), 0);
        spu.take_audio(&mut self.samples);
        if !self.audio.is_null() && !self.samples.is_empty() {
            let gain = i32::from(volume.min(100));
            for sample in &mut self.samples {
                *sample = (i32::from(*sample) * gain / 100) as i16;
            }
            // SAFETY: an open stream; SDL copies the bytes.
            unsafe {
                SDL_PutAudioStreamData(
                    self.audio,
                    self.samples.as_ptr().cast(),
                    (self.samples.len() * 2) as i32,
                );
            }
        }
        Ok(due)
    }

    /// Stops the game clock and the sound (the host went to the background):
    /// nothing runs until [`resume`](Self::resume), and queued sound is
    /// dropped.
    pub fn pause(&mut self) {
        if !self.audio.is_null() {
            // SAFETY: an open stream.
            unsafe {
                SDL_PauseAudioStreamDevice(self.audio);
                SDL_ClearAudioStream(self.audio);
            }
        }
    }

    /// Continues from now: the paused span is not run.
    pub fn resume(&mut self) {
        self.paced = Instant::now();
        if !self.audio.is_null() {
            // SAFETY: an open stream.
            unsafe { SDL_ResumeAudioStreamDevice(self.audio) };
        }
    }

    /// The display as RGBA8: (width, height, pixels).
    pub fn display(&mut self) -> (u32, u32, Vec<u8>) {
        self.session.runtime.services().gpu.display_rgba8()
    }
}

impl Drop for Game {
    fn drop(&mut self) {
        if !self.audio.is_null() {
            // SAFETY: opened by SDL_OpenAudioDeviceStream, destroyed once.
            unsafe { SDL_DestroyAudioStream(self.audio) };
        }
    }
}
