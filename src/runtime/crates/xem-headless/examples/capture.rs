//! Record the game headless as a video with its sound: each script boots
//! the disc anew and runs its steps; the frames a step captures go to
//! ffmpeg, the picture enlarged to 640x480 and the SPU's output beside it.
//!
//!     cargo run --release -p xem-headless --example capture -- DISC OUT.mp4 SCRIPT.json...
//!
//! A script is a JSON array of steps: a session command (`xem_core::control`),
//! run uncaptured; `{"capture": N}`, N frames captured; `{"press": [buttons],
//! "then": N}`, a press of one frame and N frames after it, captured.

#[cfg(not(has_game_module))]
fn main() {
    eprintln!("capture needs the game module (tools/game_module.py)");
    std::process::exit(1);
}

#[cfg(has_game_module)]
fn main() -> Result<(), Box<dyn std::error::Error>> {
    use serde_json::{Value, json};
    use std::io::Write;

    const WIDTH: usize = 640;
    const HEIGHT: usize = 480;
    let args: Vec<String> = std::env::args().skip(1).collect();
    let [disc, out, scripts @ ..] = &args[..] else {
        return Err("usage: capture DISC OUT.mp4 SCRIPT.json...".into());
    };
    let video_path = format!("{out}.video.mp4");
    // The game's frames come at the NTSC field rate.
    let mut encoder = std::process::Command::new("ffmpeg")
        .args(["-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgba", "-s", &format!("{WIDTH}x{HEIGHT}"), "-r", "59.94", "-i", "-"])
        .args(["-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "18", &video_path])
        .stdin(std::process::Stdio::piped())
        .spawn()?;
    let mut video = std::io::BufWriter::new(encoder.stdin.take().expect("ffmpeg's input"));
    let mut audio: Vec<i16> = Vec::new();
    let mut frames = 0u64;
    for script in scripts {
        let steps: Vec<Value> = serde_json::from_str(&std::fs::read_to_string(script)?)?;
        let options = xem_game::BootOptions { stubs: Some("build/game/stubs.txt".into()), audio_output: true, ..Default::default() };
        let (mut session, _) = xem_game::boot(std::path::Path::new(disc), &options)?;
        let mut take = |session: &mut xem_core::control::Session<xem_game::NativeModule>, keep: bool| -> std::io::Result<()> {
            let spu = &mut session.runtime.services().spu;
            let mut samples = vec![0i16; spu.audio_queued()];
            spu.take_audio(&mut samples);
            if !keep {
                return Ok(());
            }
            audio.extend_from_slice(&samples);
            let (width, height, rgba) = session.runtime.services().gpu.display_rgba8();
            let (width, height) = (width as usize, height as usize);
            let mut frame = vec![0u8; WIDTH * HEIGHT * 4];
            if width > 0 && height > 0 {
                for y in 0..HEIGHT {
                    for x in 0..WIDTH {
                        let from = (y * height / HEIGHT * width + x * width / WIDTH) * 4;
                        frame[(y * WIDTH + x) * 4..][..4].copy_from_slice(&rgba[from..from + 4]);
                    }
                }
            }
            frames += 1;
            video.write_all(&frame)
        };
        for step in steps {
            if let Some(count) = step.get("capture").and_then(Value::as_u64) {
                for _ in 0..count {
                    session.frame().map_err(|e| e.to_string())?;
                    take(&mut session, true)?;
                }
            } else if let Some(buttons) = step.get("press") {
                let reply = session.execute(&json!({"cmd": "press", "buttons": buttons, "frames": 1}));
                if let Some(error) = reply.get("error") {
                    return Err(format!("{step}: {error}").into());
                }
                take(&mut session, true)?;
                for _ in 0..step.get("then").and_then(Value::as_u64).unwrap_or(0) {
                    session.frame().map_err(|e| e.to_string())?;
                    take(&mut session, true)?;
                }
            } else {
                let reply = session.execute(&step);
                if let Some(error) = reply.get("error") {
                    return Err(format!("{step}: {error}").into());
                }
                take(&mut session, false)?;
            }
        }
    }
    video.flush()?;
    drop(video);
    encoder.wait()?;
    let audio_path = format!("{out}.pcm");
    std::fs::write(&audio_path, audio.iter().flat_map(|s| s.to_le_bytes()).collect::<Vec<u8>>())?;
    let status = std::process::Command::new("ffmpeg")
        .args(["-y", "-loglevel", "error", "-i", &video_path])
        .args(["-f", "s16le", "-ar", "44100", "-ac", "2", "-i", &audio_path])
        .args(["-c:v", "copy", "-c:a", "aac", "-b:a", "160k", "-shortest", out])
        .status()?;
    std::fs::remove_file(&video_path)?;
    std::fs::remove_file(&audio_path)?;
    println!("{out}: {frames} frames, {} audio samples, ffmpeg {status}", audio.len() / 2);
    Ok(())
}
