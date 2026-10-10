//! XA-ADPCM decoding of synthetic sound groups, the resampler and the
//! drive's sector selection.

use xem_media::xa::{
    self, Coding, Delivery, History, Resampler, Subheader, XaAudio, decode_adpcm, mode, submode,
};

/// A 0x900-byte sector whose every group has the given unit headers and
/// 4-bit samples `sample(group, unit, j)`.
fn sector4(headers: [u8; 8], sample: impl Fn(usize, usize, usize) -> i8) -> Vec<u8> {
    let mut data = vec![0u8; xa::SECTOR_AUDIO_BYTES];
    for (g, group) in data.chunks_exact_mut(128).enumerate() {
        group[4..12].copy_from_slice(&headers);
        group[..4].copy_from_slice(&headers[..4]);
        group[12..16].copy_from_slice(&headers[4..]);
        for j in 0..28 {
            let word = (0..8).fold(0u32, |w, unit| {
                w | (sample(g, unit, j) as u32 & 0xF) << (unit * 4)
            });
            group[16 + j * 4..20 + j * 4].copy_from_slice(&word.to_le_bytes());
        }
    }
    data
}

fn mono4() -> Coding {
    Coding::from_byte(0x00)
}

#[test]
fn filter0_expands_and_shifts() {
    // Unit u uses shift u; its samples cycle through -8..7.
    let headers = [0, 1, 2, 3, 4, 12, 13, 15];
    let data = sector4(headers, |_, _, j| (j % 16) as i8 - 8);
    let mut history = [History::default(); 2];
    let (mut left, mut right) = (Vec::new(), Vec::new());
    decode_adpcm(&data, mono4(), &mut history, &mut left, &mut right);
    assert_eq!(left.len(), 18 * 8 * 28);
    assert!(right.is_empty());
    for (unit, &header) in headers.iter().enumerate() {
        let shift = if header > 12 { 9 } else { header };
        for j in 0..28 {
            let t = (j % 16) as i32 - 8;
            assert_eq!(
                i32::from(left[unit * 28 + j]),
                (t << 12) >> shift,
                "unit {unit} sample {j}"
            );
        }
    }
}

#[test]
fn filters_1_to_3_predict_from_history() {
    // One impulse of 4096 (nibble 1, shift 0), then zeros.
    let expected: [&[i16]; 3] = [
        &[4096, 3840, 3600],
        &[4096, 7360, 9897],
        &[4096, 6272, 6084],
    ];
    for (filter, expected) in (1u8..=3).zip(expected) {
        let data = sector4([filter << 4; 8], |g, u, j| {
            i8::from(g == 0 && u == 0 && j == 0)
        });
        let mut history = [History::default(); 2];
        let (mut left, mut right) = (Vec::new(), Vec::new());
        decode_adpcm(&data, mono4(), &mut history, &mut left, &mut right);
        assert_eq!(&left[..3], expected, "filter {filter}");
    }
}

#[test]
fn history_carries_across_units_and_sectors_and_saturates() {
    // Filter 1 with every sample at +7 << 12: grows until it saturates.
    let data = sector4([0x10; 8], |_, _, _| 7);
    let mut history = [History::default(); 2];
    let (mut first, mut right) = (Vec::new(), Vec::new());
    decode_adpcm(&data, mono4(), &mut history, &mut first, &mut right);
    assert_eq!(*first.last().unwrap(), 0x7FFF);
    assert_eq!(
        history[0],
        History {
            old: 0x7FFF,
            older: 0x7FFF
        }
    );

    // A second sector of zeros under filter 1 decays from that history.
    let silent = sector4([0x10; 8], |_, _, _| 0);
    let mut second = Vec::new();
    decode_adpcm(&silent, mono4(), &mut history, &mut second, &mut right);
    assert_eq!(second[0], ((0x7FFF * 60 + 32) >> 6) as i16);
    let mut fresh = [History::default(); 2];
    let mut cold = Vec::new();
    decode_adpcm(&silent, mono4(), &mut fresh, &mut cold, &mut right);
    assert_eq!(cold[0], 0);
}

#[test]
fn stereo_interleave_even_units_left_odd_right() {
    // Left units carry +1 << 12, right units -1 << 12, filter 0.
    let data = sector4([0; 8], |_, u, _| if u % 2 == 0 { 1 } else { -1 });
    let mut history = [History::default(); 2];
    let (mut left, mut right) = (Vec::new(), Vec::new());
    decode_adpcm(
        &data,
        Coding::from_byte(0x01),
        &mut history,
        &mut left,
        &mut right,
    );
    assert_eq!((left.len(), right.len()), (2016, 2016));
    assert!(left.iter().all(|&s| s == 4096));
    assert!(right.iter().all(|&s| s == -4096));
}

#[test]
fn eight_bit_samples() {
    let mut data = vec![0u8; xa::SECTOR_AUDIO_BYTES];
    for group in data.chunks_exact_mut(128) {
        group[4..8].copy_from_slice(&[0, 4, 8, 12]);
        for j in 0..28 {
            group[16 + j * 4..20 + j * 4].copy_from_slice(&[0x7F, 0x80, 0x81, 0x40]);
        }
    }
    let mut history = [History::default(); 2];
    let (mut left, mut right) = (Vec::new(), Vec::new());
    decode_adpcm(
        &data,
        Coding::from_byte(0x10),
        &mut history,
        &mut left,
        &mut right,
    );
    assert_eq!(left.len(), 18 * 4 * 28);
    let units = [
        0x7F << 8,
        (-0x80 << 8) >> 4,
        (-0x7F << 8) >> 8,
        (0x40 << 8) >> 12,
    ];
    for (unit, &value) in units.iter().enumerate() {
        assert_eq!(i32::from(left[unit * 28]), value, "unit {unit}");
    }
}

#[test]
fn resampler_output_count_per_sector() {
    for (coding, frames) in [
        (0x01u8, 2352usize),
        (0x00, 4704),
        (0x05, 4704),
        (0x04, 9408),
        (0x11, 1176),
        (0x10, 2352),
    ] {
        assert_eq!(
            Coding::from_byte(coding).output_frames(),
            frames,
            "coding {coding:#x}"
        );
        let mut audio = XaAudio::new();
        let data = sector4([0; 8], |_, _, j| (j % 3) as i8);
        audio.decode_sector(coding, &data);
        assert_eq!(audio.available(), frames, "coding {coding:#x}");
        // The six-step counter returns to its start after every sector.
        audio.decode_sector(coding, &data);
        assert_eq!(audio.available(), frames * 2, "coding {coding:#x}");
    }
}

#[test]
fn resampler_passes_a_constant_with_the_tables_gain() {
    let mut resampler = Resampler::new();
    let mut out = std::collections::VecDeque::new();
    for _ in 0..60 {
        resampler.push([0x4000, -0x4000], &mut out);
    }
    assert_eq!(out.len(), 70);
    // Once the ring holds only the constant, each phase gives the constant
    // times its table's sum (about 0.906).
    for &[l, r] in out.iter().skip(42) {
        assert!((14700..14900).contains(&l), "{l}");
        assert!((-14900..-14700).contains(&r), "{r}");
    }
}

#[test]
fn drive_selects_sectors_by_mode_and_filter() {
    let audio_sub = Subheader {
        file: 1,
        channel: 3,
        submode: submode::AUDIO | submode::REAL_TIME | submode::FORM2,
        coding: 1,
    };
    let mut audio = XaAudio::new();
    let data = sector4([0; 8], |_, _, _| 1);
    assert_eq!(
        audio.deliver(audio_sub, &data),
        Delivery::NotAudio,
        "ADPCM disabled"
    );
    audio.set_mode(mode::XA_ADPCM);
    assert_eq!(
        audio.deliver(audio_sub, &data),
        Delivery::Played,
        "no filter"
    );
    audio.set_mode(mode::XA_ADPCM | mode::XA_FILTER);
    audio.set_filter(1, 2);
    assert_eq!(
        audio.deliver(audio_sub, &data),
        Delivery::NotAudio,
        "other channel"
    );
    audio.set_filter(1, 3);
    assert_eq!(audio.deliver(audio_sub, &data), Delivery::Played);
    let not_realtime = Subheader {
        submode: submode::AUDIO | submode::FORM2,
        ..audio_sub
    };
    assert_eq!(audio.deliver(not_realtime, &data), Delivery::NotAudio);
    let video = Subheader {
        submode: submode::VIDEO | submode::REAL_TIME,
        ..audio_sub
    };
    assert_eq!(audio.deliver(video, &data), Delivery::NotAudio);
    assert_eq!(audio.available(), 2 * 2352);

    // A raw sector: sync, header (mode 2), subheader twice, data.
    let mut raw = vec![0u8; 2352];
    raw[15] = 2;
    raw[16..20].copy_from_slice(&[1, 3, audio_sub.submode, 1]);
    raw[24..24 + data.len()].copy_from_slice(&data);
    assert_eq!(audio.deliver_raw(&raw), Delivery::Played);
    raw[15] = 1;
    assert_eq!(audio.deliver_raw(&raw), Delivery::NotAudio);
}

#[test]
fn snapshot_round_trip_continues_identically() {
    let data = sector4(
        [0x21, 0x13, 0x32, 0x05, 0x10, 0x24, 0x31, 0x02],
        |g, u, j| ((g * 7 + u * 3 + j) % 16) as i8 - 8,
    );
    let mut a = XaAudio::new();
    a.set_mode(mode::XA_ADPCM | mode::XA_FILTER);
    a.set_filter(1, 1);
    a.decode_sector(0x01, &data);
    let mut head = vec![[0i16; 2]; 1000];
    a.pop_samples(&mut head);
    let saved = a.save();
    let mut b = XaAudio::new();
    b.load(&saved).unwrap();
    assert_eq!(a, b);
    a.decode_sector(0x01, &data);
    b.decode_sector(0x01, &data);
    let mut out_a = vec![[0i16; 2]; a.available()];
    let mut out_b = vec![[0i16; 2]; b.available()];
    a.pop_samples(&mut out_a);
    b.pop_samples(&mut out_b);
    assert_eq!(out_a, out_b);
    assert_eq!(out_a.len(), 2352 * 2 - 1000);
    assert!(b.load(&saved[1..]).is_err());
}
