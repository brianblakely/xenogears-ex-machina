//! Enough of a cue sheet to find the single MODE2/2352 data track.

use crate::source::{Result, format_error};

/// The first track of a cue sheet.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct CueTrack {
    /// The `FILE` holding the track, as written in the sheet.
    pub file: String,
    pub number: u32,
    /// Sector within the file where `INDEX 01` (LBA 0) starts.
    pub start: u32,
    /// Sector within the file where the next track of the same file starts,
    /// or None when the track runs to the end of the file.
    pub end: Option<u32>,
}

/// Split a line into words, a double-quoted string being one word.
fn words(line: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut chars = line.trim().chars().peekable();
    while let Some(&c) = chars.peek() {
        if c.is_whitespace() {
            chars.next();
        } else if c == '"' {
            chars.next();
            out.push(chars.by_ref().take_while(|&c| c != '"').collect());
        } else {
            let mut word = String::new();
            while let Some(&c) = chars.peek() {
                if c.is_whitespace() {
                    break;
                }
                word.push(c);
                chars.next();
            }
            out.push(word);
        }
    }
    out
}

/// `mm:ss:ff` to a sector count.
fn msf(text: &str) -> Result<u32> {
    let parts: Vec<&str> = text.split(':').collect();
    let parsed: Option<Vec<u32>> = parts.iter().map(|p| p.parse().ok()).collect();
    match parsed.as_deref() {
        Some(&[m, s, f]) if s < 60 && f < 75 => Ok((m * 60 + s) * 75 + f),
        _ => Err(format_error(format!("cue: bad time {text}"))),
    }
}

/// The first track, which must be a `MODE2/2352` track with an `INDEX 01`.
pub fn parse_cue(text: &str) -> Result<CueTrack> {
    let mut file: Option<String> = None;
    let mut first: Option<CueTrack> = None;
    let mut first_mode = String::new();
    // Inside the first track; inside the track after it, in the same file.
    let mut in_first = false;
    let mut next_in_file = false;
    for line in text.lines() {
        let words = words(line);
        let Some(command) = words.first() else {
            continue;
        };
        match command.to_ascii_uppercase().as_str() {
            "FILE" => {
                let name = words
                    .get(1)
                    .ok_or_else(|| format_error("cue: FILE without a name"))?;
                file = Some(name.clone());
                in_first = false;
                next_in_file = false;
            }
            "TRACK" => {
                if first.is_some() {
                    // A next track in the same file bounds the first at its
                    // earliest index (00 or 01).
                    next_in_file = in_first;
                    in_first = false;
                    continue;
                }
                let number = words
                    .get(1)
                    .and_then(|n| n.parse().ok())
                    .ok_or_else(|| format_error("cue: bad TRACK"))?;
                first_mode = words.get(2).cloned().unwrap_or_default();
                let file = file
                    .clone()
                    .ok_or_else(|| format_error("cue: TRACK before FILE"))?;
                first = Some(CueTrack {
                    file,
                    number,
                    start: u32::MAX,
                    end: None,
                });
                in_first = true;
            }
            "INDEX" => {
                let (Some(number), Some(time)) = (words.get(1), words.get(2)) else {
                    return Err(format_error("cue: bad INDEX"));
                };
                let sector = msf(time)?;
                let Some(track) = first.as_mut() else {
                    continue;
                };
                if in_first {
                    if number.parse::<u32>().ok() == Some(1) {
                        track.start = sector;
                    }
                } else if next_in_file {
                    track.end = Some(sector);
                    next_in_file = false;
                }
            }
            _ => {}
        }
    }
    let track = first.ok_or_else(|| format_error("cue: no TRACK"))?;
    if !first_mode.eq_ignore_ascii_case("MODE2/2352") {
        return Err(format_error(format!(
            "cue: first track is {first_mode}, not MODE2/2352"
        )));
    }
    if track.start == u32::MAX {
        return Err(format_error("cue: first track has no INDEX 01"));
    }
    if track.end.is_some_and(|end| end < track.start) {
        return Err(format_error("cue: second track starts before the first"));
    }
    Ok(track)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn single_track_sheet() {
        let text = "FILE \"disc1.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n";
        let track = parse_cue(text).unwrap();
        assert_eq!(
            track,
            CueTrack {
                file: "disc1.bin".into(),
                number: 1,
                start: 0,
                end: None
            }
        );
    }

    #[test]
    fn pregap_in_file_and_following_audio_track() {
        let text = "REM test\r\nFILE \"Some Game (Track 1).bin\" BINARY\r\n  TRACK 01 MODE2/2352\r\n    INDEX 00 00:00:00\r\n    INDEX 01 00:02:00\r\n  TRACK 02 AUDIO\r\n    INDEX 00 10:00:00\r\n    INDEX 01 10:02:00\r\n";
        let track = parse_cue(text).unwrap();
        assert_eq!(track.file, "Some Game (Track 1).bin");
        assert_eq!((track.start, track.end), (150, Some(45_000)));
    }

    #[test]
    fn second_track_in_its_own_file_does_not_bound_the_first() {
        let text = "FILE a.bin BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:00:00\nFILE b.bin BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n";
        let track = parse_cue(text).unwrap();
        assert_eq!((track.file.as_str(), track.end), ("a.bin", None));
    }

    #[test]
    fn rejects_other_layouts() {
        assert!(parse_cue("FILE x.bin BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\n").is_err());
        assert!(parse_cue("FILE x.bin BINARY\nTRACK 01 MODE2/2352\n").is_err());
        assert!(parse_cue("TRACK 01 MODE2/2352\nINDEX 01 00:00:00\n").is_err());
        assert!(parse_cue("FILE x.bin BINARY\nTRACK 01 MODE2/2352\nINDEX 01 00:61:00\n").is_err());
    }
}
