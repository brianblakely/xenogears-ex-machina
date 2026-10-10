//! Sanity checks against the user's own disc image, when present: the
//! first frame of disc 1's first movie stream decoded to a PNG, and its
//! first XA sectors to a WAV, both under `.local/media-check/` (ignored).
//! The disc is `$XEM_DISC1`, or `.local/discs/disc1.bin` (raw MODE2/2352)
//! in the repository or a directory above it. Skipped when absent; nothing
//! read from the disc is ever checked in.

use std::fs;
use std::io::{Read, Seek, SeekFrom};
use std::path::PathBuf;

use xem_media::bitstream::{self, StrSectorHeader};
use xem_media::xa::{Delivery, Subheader, mode, submode};
use xem_media::{Depth, Mdec, XaAudio};

const SECTOR: usize = 2352;
/// Disc 1 slot 2, the first movie stream (`.local/extract/disc1/manifest.json`).
const MOVIE_LBA: u64 = 825;

fn local_dir() -> Option<PathBuf> {
    let manifest = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    manifest
        .ancestors()
        .map(|dir| dir.join(".local"))
        .find(|dir| dir.join("discs/disc1.bin").is_file())
}

fn disc_path() -> Option<PathBuf> {
    std::env::var_os("XEM_DISC1")
        .map(PathBuf::from)
        .or_else(|| local_dir().map(|d| d.join("discs/disc1.bin")))
}

fn output_dir() -> PathBuf {
    let dir = local_dir()
        .unwrap_or_else(|| PathBuf::from(env!("CARGO_TARGET_TMPDIR")))
        .join("media-check");
    fs::create_dir_all(&dir).unwrap();
    dir
}

fn read_sectors(lba: u64, count: usize) -> Option<Vec<u8>> {
    let mut file = fs::File::open(disc_path()?).ok()?;
    file.seek(SeekFrom::Start(lba * SECTOR as u64)).ok()?;
    let mut data = vec![0u8; count * SECTOR];
    file.read_exact(&mut data).ok()?;
    Some(data)
}

#[test]
fn first_movie_frame_to_png() {
    let Some(sectors) = read_sectors(MOVIE_LBA, 16) else {
        eprintln!("skipped: no disc 1 image");
        return;
    };
    let mut frame = Vec::new();
    let mut first: Option<StrSectorHeader> = None;
    for sector in sectors.chunks_exact(SECTOR) {
        let Some(header) = StrSectorHeader::parse(&sector[24..]) else {
            continue;
        };
        if header.frame != 1 {
            break;
        }
        assert_eq!(
            usize::from(header.sector),
            frame.len() / StrSectorHeader::DATA_BYTES
        );
        frame.extend_from_slice(&sector[24 + 0x20..24 + 0x20 + StrSectorHeader::DATA_BYTES]);
        first.get_or_insert(header);
    }
    let header = first.expect("movie stream starts with a video sector");
    frame.truncate(header.frame_size as usize);
    let (width, height) = (usize::from(header.width), usize::from(header.height));
    let macroblocks = width.div_ceil(16) * height.div_ceil(16);
    let rle = bitstream::decode(&frame, Some(macroblocks)).unwrap();
    assert_eq!(rle.macroblocks, macroblocks);
    assert_eq!(rle.header.version, 2);
    // libpress's size field counts the padded run-level words.
    assert_eq!(
        rle.halfwords.len().next_multiple_of(64) / 2,
        usize::from(rle.header.rle_words)
    );

    let mut mdec = Mdec::new();
    let image = mdec.decode_frame(&rle.halfwords, width, height, Depth::Bits24);
    assert_eq!(
        image,
        Mdec::new().decode_frame(&rle.halfwords, width, height, Depth::Bits24)
    );
    let path = output_dir().join("disc1-movie825-frame1.png");
    fs::write(&path, png_rgb(width, height, &image)).unwrap();
    eprintln!(
        "wrote {} ({width}x{height}, quant {})",
        path.display(),
        rle.header.quant
    );
}

#[test]
fn first_movie_audio_to_wav() {
    let Some(sectors) = read_sectors(MOVIE_LBA, 400) else {
        eprintln!("skipped: no disc 1 image");
        return;
    };
    // As movie_start sets it: double speed, ADPCM on, filter file 1, channel 1.
    let mut audio = XaAudio::new();
    audio.set_mode(0x80 | mode::XA_ADPCM | mode::XA_FILTER);
    audio.set_filter(1, 1);
    let mut played = 0;
    for sector in sectors.chunks_exact(SECTOR) {
        let sub = Subheader::from_bytes([sector[16], sector[17], sector[18], sector[19]]);
        let before = audio.available();
        if audio.deliver_raw(sector) == Delivery::Played {
            assert_eq!(sub.submode & submode::AUDIO, submode::AUDIO);
            assert_eq!(
                audio.available() - before,
                xem_media::Coding::from_byte(sub.coding).output_frames()
            );
            played += 1;
        }
    }
    assert!(played > 0, "no audio sectors on channel 1");
    let mut pcm = vec![[0i16; 2]; audio.available()];
    audio.pop_samples(&mut pcm);
    let peak = pcm
        .iter()
        .flatten()
        .map(|s| s.unsigned_abs())
        .max()
        .unwrap();
    assert!(peak > 0, "silent");
    let path = output_dir().join("disc1-movie825-audio.wav");
    fs::write(&path, wav_stereo_44100(&pcm)).unwrap();
    eprintln!(
        "wrote {} ({played} sectors, {} frames, peak {peak})",
        path.display(),
        pcm.len()
    );
}

fn crc32(data: &[u8]) -> u32 {
    let mut crc = !0u32;
    for &byte in data {
        crc ^= u32::from(byte);
        for _ in 0..8 {
            crc = if crc & 1 != 0 {
                crc >> 1 ^ 0xEDB8_8320
            } else {
                crc >> 1
            };
        }
    }
    !crc
}

/// An RGB PNG with stored (uncompressed) deflate blocks.
fn png_rgb(width: usize, height: usize, rgb: &[u8]) -> Vec<u8> {
    let mut raw = Vec::with_capacity((width * 3 + 1) * height);
    for row in rgb.chunks_exact(width * 3) {
        raw.push(0);
        raw.extend_from_slice(row);
    }
    let mut zlib = vec![0x78, 0x01];
    let blocks: Vec<&[u8]> = raw.chunks(0xFFFF).collect();
    for (i, block) in blocks.iter().enumerate() {
        zlib.push(u8::from(i + 1 == blocks.len()));
        let len = block.len() as u16;
        zlib.extend(len.to_le_bytes());
        zlib.extend((!len).to_le_bytes());
        zlib.extend_from_slice(block);
    }
    let (mut a, mut b) = (1u32, 0u32);
    for &byte in &raw {
        a = (a + u32::from(byte)) % 65521;
        b = (b + a) % 65521;
    }
    zlib.extend((b << 16 | a).to_be_bytes());

    let mut png = b"\x89PNG\r\n\x1a\n".to_vec();
    let mut chunk = |kind: &[u8; 4], body: &[u8]| {
        png.extend((body.len() as u32).to_be_bytes());
        let mut tagged = kind.to_vec();
        tagged.extend_from_slice(body);
        png.extend_from_slice(&tagged);
        png.extend(crc32(&tagged).to_be_bytes());
    };
    let mut ihdr = Vec::new();
    ihdr.extend((width as u32).to_be_bytes());
    ihdr.extend((height as u32).to_be_bytes());
    ihdr.extend([8, 2, 0, 0, 0]);
    chunk(b"IHDR", &ihdr);
    chunk(b"IDAT", &zlib);
    chunk(b"IEND", &[]);
    png
}

fn wav_stereo_44100(frames: &[[i16; 2]]) -> Vec<u8> {
    let data_len = (frames.len() * 4) as u32;
    let mut wav = Vec::with_capacity(44 + data_len as usize);
    wav.extend(b"RIFF");
    wav.extend((36 + data_len).to_le_bytes());
    wav.extend(b"WAVEfmt ");
    wav.extend(16u32.to_le_bytes());
    wav.extend(1u16.to_le_bytes());
    wav.extend(2u16.to_le_bytes());
    wav.extend(44100u32.to_le_bytes());
    wav.extend((44100u32 * 4).to_le_bytes());
    wav.extend(4u16.to_le_bytes());
    wav.extend(16u16.to_le_bytes());
    wav.extend(b"data");
    wav.extend(data_len.to_le_bytes());
    for frame in frames {
        wav.extend(frame[0].to_le_bytes());
        wav.extend(frame[1].to_le_bytes());
    }
    wav
}
