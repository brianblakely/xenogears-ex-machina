//! Behaviour of the whole SPU through its public interface.

use std::sync::Arc;
use std::sync::atomic::{AtomicU32, Ordering};

use xem_spu::adpcm::{FLAG_LOOP_END, FLAG_LOOP_REPEAT, FLAG_LOOP_START};
use xem_spu::{Phase, SnapshotError, Spu, WINDOW_SIZE, control, reg, status};

const ON: u16 = control::ENABLE | control::UNMUTE;

/// A 16-byte ADPCM block whose 28 nibbles all hold `nibble`.
fn block(shift: u8, filter: u8, flags: u8, nibble: u8) -> [u8; 16] {
    let mut b = [(nibble & 0xF) * 0x11; 16];
    b[0] = (filter << 4) | shift;
    b[1] = flags;
    b
}

fn upload(spu: &mut Spu, address: u32, data: &[u8]) {
    spu.write16(reg::TRANSFER_ADDRESS, (address >> 3) as u16);
    spu.write16(
        reg::CONTROL,
        spu.read16(reg::CONTROL) | control::TRANSFER_DMA_WRITE,
    );
    spu.dma_write(data);
    spu.write16(
        reg::CONTROL,
        spu.read16(reg::CONTROL) & !control::TRANSFER_MODE,
    );
}

fn enabled() -> Spu {
    let mut spu = Spu::new();
    spu.write16(reg::TRANSFER_CONTROL, 4);
    spu.write16(reg::CONTROL, ON);
    spu.write16(reg::MAIN_VOLUME_LEFT, 0x3FFF);
    spu.write16(reg::MAIN_VOLUME_RIGHT, 0x3FFF);
    spu
}

fn voice_reg(v: usize, r: usize) -> usize {
    reg::voice(v) + r
}

fn setup_voice(spu: &mut Spu, v: usize, start: u32, pitch: u16, adsr1: u16, adsr2: u16) {
    spu.write16(voice_reg(v, reg::VOICE_VOLUME_LEFT), 0x3FFF);
    spu.write16(voice_reg(v, reg::VOICE_VOLUME_RIGHT), 0x3FFF);
    spu.write16(voice_reg(v, reg::VOICE_PITCH), pitch);
    spu.write16(voice_reg(v, reg::VOICE_START), (start >> 3) as u16);
    spu.write16(voice_reg(v, reg::VOICE_ADSR1), adsr1);
    spu.write16(voice_reg(v, reg::VOICE_ADSR2), adsr2);
}

fn key_on(spu: &mut Spu, v: usize) {
    if v < 16 {
        spu.write16(reg::KEY_ON, 1 << v);
    } else {
        spu.write16(reg::KEY_ON + 2, 1 << (v - 16));
    }
}

fn key_off(spu: &mut Spu, v: usize) {
    if v < 16 {
        spu.write16(reg::KEY_OFF, 1 << v);
    } else {
        spu.write16(reg::KEY_OFF + 2, 1 << (v - 16));
    }
}

fn endx(spu: &Spu) -> u32 {
    spu.read16(reg::ENDX) as u32 | (spu.read16(reg::ENDX + 2) as u32) << 16
}

/// ADSR holding a level written to the envelope register: attack and
/// sustain rates frozen, sustain level 0x8000 (decay ends at once).
const HOLD_ADSR1: u16 = 0x7F0F;
const HOLD_ADSR2: u16 = 0x7F << 6;

/// Keys a voice on and pins its envelope at full level.
fn key_on_held(spu: &mut Spu, v: usize) {
    key_on(spu, v);
    spu.write16(voice_reg(v, reg::VOICE_ENVELOPE), 0x7FFF);
}

#[test]
fn adpcm_playback_interpolates_and_follows_loop_flags() {
    let mut spu = enabled();
    // Block A (loop start) holds 0x1000 (nibble 1, shift 0); block B ends
    // with repeat and holds the same value.
    let mut data = Vec::new();
    data.extend_from_slice(&block(0, 0, FLAG_LOOP_START, 1));
    data.extend_from_slice(&block(0, 0, FLAG_LOOP_END | FLAG_LOOP_REPEAT, 1));
    upload(&mut spu, 0x1000, &data);
    setup_voice(&mut spu, 0, 0x1000, 0x1000, HOLD_ADSR1, HOLD_ADSR2);
    spu.write16(voice_reg(0, reg::VOICE_REPEAT), 0x3000 >> 3);
    key_on_held(&mut spu, 0);

    spu.run(1, None);
    assert_eq!(
        spu.read16(voice_reg(0, reg::VOICE_REPEAT)),
        0x1000 >> 3,
        "loop start sets the repeat address"
    );
    // Index 0 interpolates only the newest sample: gauss[0] = -1.
    assert_eq!(spu.voice(0).output, (((-0x1000) >> 15) * 0x7FFF) >> 15);
    spu.run(3, None);
    // From the fourth sample all four inputs are 0x1000: the products
    // 0x12C7, 0x59B3, 0x1307 and -1 times 0x1000, each >> 15, sum to 4077;
    // the envelope 0x7FFF scales that to 4076.
    assert_eq!(spu.voice(0).output, 4076);
    assert_eq!(spu.voice(0).envelope, 0x7FFF);

    spu.run(24, None);
    assert_eq!(
        spu.voice(0).address,
        0x1010,
        "28 samples at pitch 0x1000 finish block A"
    );
    assert_eq!(endx(&spu) & 1, 0);
    spu.run(28, None);
    assert_eq!(endx(&spu) & 1, 1, "leaving the end block sets ENDX");
    assert_eq!(
        spu.voice(0).address,
        0x1000,
        "and jumps to the repeat address"
    );
    assert_eq!(
        spu.voice(0).phase,
        Phase::Sustain,
        "end+repeat keeps playing"
    );
    assert_eq!(spu.voice(0).output, 4076);

    // End without repeat: ENDX, release with the envelope at zero.
    upload(&mut spu, 0x2000, &block(0, 0, FLAG_LOOP_END, 1));
    setup_voice(&mut spu, 1, 0x2000, 0x1000, HOLD_ADSR1, HOLD_ADSR2);
    key_on_held(&mut spu, 1);
    spu.run(27, None);
    assert_eq!(endx(&spu) & 2, 0);
    assert_eq!(spu.voice(1).envelope, 0x7FFF);
    spu.run(1, None);
    assert_eq!(endx(&spu) & 2, 2);
    // Released at zero, the envelope is off within the same cycle.
    assert_eq!(spu.voice(1).envelope, 0);
    assert_eq!(spu.voice(1).phase, Phase::Off);

    // Key on clears ENDX.
    key_on(&mut spu, 0);
    assert_eq!(endx(&spu) & 1, 0);
    assert_eq!(endx(&spu) & 2, 2);
}

#[test]
fn pitch_sets_the_sample_step() {
    let mut spu = enabled();
    upload(
        &mut spu,
        0x1000,
        &block(12, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 0),
    );
    for (v, pitch) in [(0, 0x0800), (1, 0x1000), (2, 0x3FFF), (3, 0x8000)] {
        setup_voice(&mut spu, v, 0x1000, pitch, HOLD_ADSR1, HOLD_ADSR2);
        key_on(&mut spu, v);
    }
    spu.run(5, None);
    assert_eq!(spu.voice(0).pitch_counter, 5 * 0x800);
    assert_eq!(spu.voice(1).pitch_counter, 5 * 0x1000);
    assert_eq!(spu.voice(2).pitch_counter, 5 * 0x3FFF);
    assert_eq!(
        spu.voice(3).pitch_counter,
        5 * 0x4000,
        "steps above 0x3FFF are 0x4000"
    );
}

#[test]
fn pitch_modulation_scales_by_the_previous_voice() {
    let mut spu = enabled();
    // Voice 0 plays a constant -0x8000 (nibble 8, shift 0) at full envelope.
    upload(
        &mut spu,
        0x1000,
        &block(0, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 8),
    );
    upload(
        &mut spu,
        0x1010,
        &block(12, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 0),
    );
    setup_voice(&mut spu, 0, 0x1000, 0x1000, HOLD_ADSR1, HOLD_ADSR2);
    setup_voice(&mut spu, 1, 0x1010, 0x1000, HOLD_ADSR1, HOLD_ADSR2);
    spu.write16(reg::PITCH_MODULATION, 1 << 1);
    key_on_held(&mut spu, 0);
    key_on(&mut spu, 1);
    spu.run(4, None);
    let before = spu.voice(1).pitch_counter;
    let out = spu.voice(0).output as i32;
    assert!(out < -0x7000, "voice 0 is loud and negative: {out}");
    spu.run(1, None);
    let out = spu.voice(0).output as i32;
    let step = (0x1000 * (out + 0x8000)) >> 15;
    assert_eq!(spu.voice(1).pitch_counter, before + step as u32);
    assert!(step < 0x100);
}

fn envx(spu: &Spu, v: usize) -> i16 {
    spu.read16(voice_reg(v, reg::VOICE_ENVELOPE)) as i16
}

fn trace(spu: &mut Spu, v: usize, n: usize) -> Vec<i16> {
    (0..n)
        .map(|_| {
            spu.run(1, None);
            envx(spu, v)
        })
        .collect()
}

#[test]
fn adsr_attack_linear_and_exponential() {
    let mut spu = enabled();
    upload(
        &mut spu,
        0x1000,
        &block(12, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 0),
    );
    // Linear attack, shift 0, step +7; decay shift 15; sustain level max.
    setup_voice(&mut spu, 0, 0x1000, 0x1000, 0x00FF, HOLD_ADSR2);
    // Exponential attack.
    setup_voice(&mut spu, 1, 0x1000, 0x1000, 0x80FF, HOLD_ADSR2);
    spu.write16(reg::KEY_ON, 0b11);
    assert_eq!(envx(&spu, 0), 0, "key on starts at zero");
    let lin = trace(&mut spu, 0, 4);
    let mut spu1 = enabled();
    upload(
        &mut spu1,
        0x1000,
        &block(12, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 0),
    );
    setup_voice(&mut spu1, 1, 0x1000, 0x1000, 0x80FF, HOLD_ADSR2);
    key_on(&mut spu1, 1);
    let exp = trace(&mut spu1, 1, 4);
    assert_eq!(lin, [0x3800, 0x7000, 0x7FFF, 0x7FFF]);
    // Above 0x6000 the exponential attack takes a quarter step.
    assert_eq!(exp, [0x3800, 0x7000, 0x7E00, 0x7FFF]);
}

#[test]
fn adsr_timing_follows_the_shift() {
    let mut spu = enabled();
    upload(
        &mut spu,
        0x1000,
        &block(12, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 0),
    );
    // Attack shift 15 (one step per 16 cycles), step +7 (field 0).
    setup_voice(&mut spu, 0, 0x1000, 0x1000, 15 << 10, HOLD_ADSR2);
    // Attack shift 11, step +4 (field 3): +4 every cycle.
    setup_voice(
        &mut spu,
        1,
        0x1000,
        0x1000,
        (11 << 10) | (3 << 8),
        HOLD_ADSR2,
    );
    spu.write16(reg::KEY_ON, 0b11);
    spu.run(15, None);
    assert_eq!(envx(&spu, 0), 0);
    assert_eq!(envx(&spu, 1), 60);
    spu.run(1, None);
    assert_eq!(envx(&spu, 0), 7);
    spu.run(160, None);
    assert_eq!(envx(&spu, 0), 77);
    assert_eq!(envx(&spu, 1), 4 * 176);
}

#[test]
fn adsr_decay_sustain_release() {
    let mut spu = enabled();
    upload(
        &mut spu,
        0x1000,
        &block(12, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 0),
    );
    // Attack linear shift 0; decay shift 2; sustain level 3 (0x2000);
    // sustain linear decrease shift 11 step -8 (field 0): -8 per cycle;
    // release linear shift 0: -0x4000 per cycle.
    let adsr1 = (2 << 4) | 3;
    let adsr2 = 0x4000 | (11 << 8);
    setup_voice(&mut spu, 0, 0x1000, 0x1000, adsr1, adsr2);
    key_on(&mut spu, 0);
    spu.run(3, None);
    assert_eq!(envx(&spu, 0), 0x7FFF);
    assert_eq!(spu.voice(0).phase, Phase::Attack);
    // Exponential decay: -8 << 9 scaled by the level.
    let decay = trace(&mut spu, 0, 40);
    assert_eq!(decay[0] as i32, 0x7FFF + ((-(8 << 9) * 0x7FFF) >> 15));
    let sustain_at = decay
        .iter()
        .position(|&l| l <= 0x2000)
        .expect("decay reaches the sustain level");
    for w in decay[..=sustain_at].windows(2) {
        assert!(w[1] < w[0], "decay falls");
        assert!(w[0] - w[1] <= (8 << 9), "exponential steps shrink");
    }
    spu.run(1, None);
    assert_eq!(spu.voice(0).phase, Phase::Sustain);
    let s0 = envx(&spu, 0);
    spu.run(10, None);
    assert_eq!(envx(&spu, 0), s0 - 80, "linear sustain decrease");

    key_off(&mut spu, 0);
    assert_eq!(spu.voice(0).phase, Phase::Release);
    let release = trace(&mut spu, 0, 3);
    assert_eq!(release, [0, 0, 0]);
    assert_eq!(spu.voice(0).phase, Phase::Off);

    // Exponential release slows as the level falls, and ends at zero.
    let mut spu = enabled();
    upload(
        &mut spu,
        0x1000,
        &block(12, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 0),
    );
    setup_voice(
        &mut spu,
        0,
        0x1000,
        0x1000,
        HOLD_ADSR1,
        HOLD_ADSR2 | 0x20 | 4,
    );
    key_on_held(&mut spu, 0);
    spu.run(2, None);
    key_off(&mut spu, 0);
    let release = trace(&mut spu, 0, 3000);
    let steps: Vec<i32> = release
        .windows(2)
        .map(|w| w[0] as i32 - w[1] as i32)
        .collect();
    assert!(
        steps[0] > steps[100] && steps[100] > steps[400],
        "exponential release"
    );
    assert_eq!(*release.last().unwrap(), 0);
    assert_eq!(spu.voice(0).phase, Phase::Off);
}

#[test]
fn window_sync_keys_voices_and_mirrors_hardware() {
    let mut spu = Spu::new();
    // Transfer type 2 (normal); the power-on 0 would fill with the last halfword.
    spu.write16(reg::TRANSFER_CONTROL, 4);
    upload(
        &mut spu,
        0x1000,
        &block(12, 0, FLAG_LOOP_START | FLAG_LOOP_END | FLAG_LOOP_REPEAT, 1),
    );
    let mut window = [0u8; WINDOW_SIZE];
    fn put(w: &mut [u8; WINDOW_SIZE], off: usize, v: u16) {
        w[off..off + 2].copy_from_slice(&v.to_le_bytes());
    }
    fn get(w: &[u8; WINDOW_SIZE], off: usize) -> u16 {
        u16::from_le_bytes([w[off], w[off + 1]])
    }

    put(&mut window, reg::CONTROL, ON);
    put(&mut window, reg::MAIN_VOLUME_LEFT, 0x3FFF);
    put(&mut window, voice_reg(18, reg::VOICE_PITCH), 0x1000);
    put(&mut window, voice_reg(18, reg::VOICE_START), 0x1000 >> 3);
    put(&mut window, voice_reg(18, reg::VOICE_ADSR1), 0x00FF);
    put(&mut window, voice_reg(18, reg::VOICE_ADSR2), HOLD_ADSR2);
    put(&mut window, reg::KEY_ON + 2, 1 << 2);
    spu.sync_window(&mut window);
    assert_eq!(spu.voice(18).phase, Phase::Attack);
    assert_eq!(spu.read16(voice_reg(18, reg::VOICE_PITCH)), 0x1000);
    assert_eq!(
        get(&window, reg::KEY_ON + 2),
        0,
        "KON is cleared after it is applied"
    );
    assert_eq!(get(&window, reg::CONTROL), ON);

    spu.run(2, None);
    spu.sync_window(&mut window);
    assert_eq!(
        get(&window, voice_reg(18, reg::VOICE_ENVELOPE)),
        0x7000,
        "current ADSR volume written back"
    );
    assert_eq!(get(&window, reg::STATUS), spu.read16(reg::STATUS));
    assert_eq!(get(&window, reg::CURRENT_MAIN_VOLUME_LEFT), 0x7FFE);
    assert_eq!(get(&window, voice_reg(18, reg::VOICE_REPEAT)), 0x1000 >> 3);

    // No change: the voice keeps going (KON is not re-applied).
    spu.sync_window(&mut window);
    spu.run(1, None);
    assert_eq!(spu.voice(18).envelope, 0x7FFF);

    // ENDX is read back, and a game write to it has no effect.
    spu.run(28, None);
    spu.sync_window(&mut window);
    assert_eq!(get(&window, reg::ENDX + 2), 1 << 2);
    put(&mut window, reg::ENDX + 2, 0);
    spu.sync_window(&mut window);
    assert_eq!(get(&window, reg::ENDX + 2), 1 << 2);

    // KOFF releases.
    put(&mut window, reg::KEY_OFF + 2, 1 << 2);
    spu.sync_window(&mut window);
    assert_eq!(spu.voice(18).phase, Phase::Release);
    assert_eq!(get(&window, reg::KEY_OFF + 2), 0);

    // KOFF and KON in one interval: KOFF first, so the voice ends keyed on.
    spu.run(1, None);
    put(&mut window, reg::KEY_OFF + 2, 1 << 2);
    put(&mut window, reg::KEY_ON + 2, 1 << 2);
    spu.sync_window(&mut window);
    assert_eq!(spu.voice(18).phase, Phase::Attack);
    assert_eq!(spu.voice(18).envelope, 0);
    assert_eq!(endx(&spu), 0, "key on cleared ENDX");

    // Two writes between syncs collapse to the last value.
    put(&mut window, voice_reg(18, reg::VOICE_PITCH), 0x0400);
    put(&mut window, voice_reg(18, reg::VOICE_PITCH), 0x0800);
    spu.sync_window(&mut window);
    assert_eq!(spu.read16(voice_reg(18, reg::VOICE_PITCH)), 0x0800);
}

#[test]
fn manual_and_dma_transfers_round_trip() {
    let mut spu = enabled();
    let words: Vec<u16> = (0..32)
        .map(|i| 0x1111u16.wrapping_mul(i + 1) ^ 0x8001)
        .collect();
    spu.write16(reg::TRANSFER_ADDRESS, 0x3000 >> 3);
    for &w in &words {
        spu.write16(reg::TRANSFER_FIFO, w);
    }
    assert!(
        spu.ram()[0x3000..0x3040].iter().all(|&b| b == 0),
        "the FIFO waits for manual write"
    );
    spu.write16(reg::CONTROL, ON | control::TRANSFER_MANUAL_WRITE);
    let st = spu.read16(reg::STATUS);
    assert_eq!(st & 0x3F, (ON | control::TRANSFER_MANUAL_WRITE) & 0x3F);
    assert_eq!(st & status::BUSY, 0);
    spu.write16(reg::CONTROL, ON);

    spu.write16(reg::TRANSFER_ADDRESS, 0x3000 >> 3);
    spu.write16(reg::CONTROL, ON | control::TRANSFER_DMA_READ);
    let st = spu.read16(reg::STATUS);
    assert_ne!(st & status::DMA_READ_REQUEST, 0);
    assert_ne!(st & status::DMA_REQUEST, 0);
    let mut back = [0u8; 64];
    spu.dma_read(&mut back);
    let back: Vec<u16> = back
        .chunks_exact(2)
        .map(|p| u16::from_le_bytes([p[0], p[1]]))
        .collect();
    assert_eq!(back, words);
    assert_eq!(
        spu.read16(reg::TRANSFER_ADDRESS),
        0x3000 >> 3,
        "the register keeps the start"
    );

    // DMA write wrapping past the end of SPU RAM.
    let data: Vec<u8> = (0..0x200u32).map(|i| (i * 7 + 3) as u8).collect();
    spu.write16(reg::TRANSFER_ADDRESS, (0x7FF00u32 >> 3) as u16);
    spu.write16(reg::CONTROL, ON | control::TRANSFER_DMA_WRITE);
    assert_ne!(spu.read16(reg::STATUS) & status::DMA_WRITE_REQUEST, 0);
    spu.dma_write(&data);
    assert_eq!(&spu.ram()[0x7FF00..], &data[..0x100]);
    assert_eq!(&spu.ram()[..0x100], &data[0x100..]);
    spu.write16(reg::TRANSFER_ADDRESS, (0x7FF00u32 >> 3) as u16);
    let mut back = vec![0u8; 0x200];
    spu.dma_read(&mut back);
    assert_eq!(back, data);

    // Transfer type Rep2 writes A,A,C,C,...
    spu.write16(reg::TRANSFER_CONTROL, 3 << 1);
    spu.write16(reg::CONTROL, ON);
    spu.write16(reg::TRANSFER_ADDRESS, 0x4000 >> 3);
    for w in 1..=4u16 {
        spu.write16(reg::TRANSFER_FIFO, w);
    }
    spu.write16(reg::CONTROL, ON | control::TRANSFER_MANUAL_WRITE);
    assert_eq!(&spu.ram()[0x4000..0x4008], &[1, 0, 1, 0, 3, 0, 3, 0]);
}

#[test]
fn irq_on_voice_fetch_and_transfer() {
    let mut spu = enabled();
    let mut data = Vec::new();
    data.extend_from_slice(&block(12, 0, FLAG_LOOP_START, 0));
    data.extend_from_slice(&block(12, 0, FLAG_LOOP_END | FLAG_LOOP_REPEAT, 0));
    upload(&mut spu, 0x1000, &data);
    let hits = Arc::new(AtomicU32::new(0));
    let at = Arc::new(AtomicU32::new(u32::MAX));
    {
        let (hits, at) = (hits.clone(), at.clone());
        spu.set_irq_hook(Some(Box::new(move |clock| {
            hits.fetch_add(1, Ordering::SeqCst);
            at.store(clock as u32, Ordering::SeqCst);
        })));
    }
    spu.write16(reg::IRQ_ADDRESS, 0x1010 >> 3);
    spu.write16(reg::CONTROL, ON | control::IRQ_ENABLE);
    setup_voice(&mut spu, 0, 0x1000, 0x1000, HOLD_ADSR1, HOLD_ADSR2);
    key_on(&mut spu, 0);
    let start = spu.clock();
    spu.run(27, None);
    assert!(!spu.irq_flag());
    spu.run(1, None);
    assert!(spu.irq_flag(), "the voice fetched block B");
    assert_eq!(spu.irq_raised_at(), Some(start + 27));
    assert_eq!(at.load(Ordering::SeqCst), (start + 27) as u32);
    assert_ne!(spu.read16(reg::STATUS) & status::IRQ, 0);
    spu.run(200, None);
    assert_eq!(
        hits.load(Ordering::SeqCst),
        1,
        "the flag holds until acknowledged"
    );

    // Acknowledge, re-enable: the next pass over block B raises it again.
    spu.write16(reg::CONTROL, ON);
    assert_eq!(spu.read16(reg::STATUS) & status::IRQ, 0);
    spu.write16(reg::CONTROL, ON | control::IRQ_ENABLE);
    spu.run(56, None);
    assert_eq!(hits.load(Ordering::SeqCst), 2);

    // A DMA write over the IRQ address.
    spu.write16(reg::CONTROL, ON);
    spu.write16(reg::IRQ_ADDRESS, 0x6000 >> 3);
    spu.write16(reg::CONTROL, ON | control::IRQ_ENABLE);
    spu.write16(reg::TRANSFER_ADDRESS, 0x5FF0 >> 3);
    spu.dma_write(&[0u8; 0x0F]);
    assert!(!spu.irq_flag());
    // 0x5FFE, then 0x6000.
    spu.dma_write(&[0u8; 4]);
    assert!(spu.irq_flag());
    assert_eq!(hits.load(Ordering::SeqCst), 3);
}

fn noise_spu() -> Spu {
    let mut spu = enabled();
    // Noise clock: shift 15, step 7 (field 3).
    spu.write16(reg::CONTROL, ON | (15 << 10) | (3 << 8));
    setup_voice(&mut spu, 0, 0x1000, 0x1000, HOLD_ADSR1, HOLD_ADSR2);
    spu.write16(reg::NOISE, 1);
    key_on_held(&mut spu, 0);
    spu
}

#[test]
fn noise_is_deterministic() {
    let mut a = noise_spu();
    let first: Vec<i16> = (0..6)
        .map(|_| {
            a.run(1, None);
            a.voice(0).output
        })
        .collect();
    // The level before each cycle is 0, 1, 3, 7, 15, 31 (shift in a parity
    // of 1); the envelope 0x7FFF takes one off each.
    assert_eq!(first, [0, 0, 2, 6, 14, 30]);

    let mut a = noise_spu();
    let mut b = noise_spu();
    let mut out_a = vec![0i16; 2 * 4000];
    let mut out_b = vec![0i16; 2 * 4000];
    a.run(4000, Some(&mut out_a));
    b.run(4000, Some(&mut out_b));
    assert_eq!(out_a, out_b);
    let left: Vec<i16> = out_a.iter().step_by(2).copied().collect();
    assert!(left.iter().any(|&s| s > 1000) && left.iter().any(|&s| s < -1000));
    let mut distinct = left.clone();
    distinct.sort_unstable();
    distinct.dedup();
    assert!(
        distinct.len() > 1000,
        "noise varies: {} values",
        distinct.len()
    );

    // A slower noise clock steps less often.
    let mut slow = noise_spu();
    slow.write16(reg::CONTROL, ON | (8 << 10));
    let mut out = vec![0i16; 2 * 4000];
    slow.run(4000, Some(&mut out));
    let changes = out
        .chunks_exact(2)
        .collect::<Vec<_>>()
        .windows(2)
        .filter(|w| w[0] != w[1])
        .count();
    assert!(changes < 100, "{changes} changes");
}

/// psx-spx "Reverb Examples": Hall (0xADE0 bytes).
const HALL: [u16; 32] = [
    0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000, //
    0x6000, 0x5C00, 0x15BA, 0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1, //
    0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1, 0x0A00, 0x06CD, //
    0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000,
];

fn reverb_spu() -> Spu {
    let mut spu = enabled();
    for (i, &v) in HALL.iter().enumerate() {
        spu.write16(reg::REVERB_CONFIG + 2 * i, v);
    }
    spu.write16(reg::REVERB_BASE, ((0x80000 - 0xADE0) >> 3) as u16);
    spu.write16(reg::REVERB_VOLUME_LEFT, 0x4000);
    spu.write16(reg::REVERB_VOLUME_RIGHT, 0x4000);
    spu.write16(reg::CD_VOLUME_LEFT, 0x7FFF);
    spu.write16(reg::CD_VOLUME_RIGHT, 0x7FFF);
    // CD audio reaches the output only through the reverb.
    spu.write16(
        reg::CONTROL,
        ON | control::REVERB_ENABLE | control::CD_REVERB,
    );
    spu
}

fn energy(samples: &[i16]) -> i64 {
    samples.iter().map(|&s| s as i64 * s as i64).sum()
}

#[test]
fn reverb_impulse_response_decays() {
    let mut spu = reverb_spu();
    spu.push_cd_audio(&[0x7FFF, 0x7FFF]);
    let n = 44_100 * 4;
    let mut out = vec![0i16; 2 * n];
    spu.run(n as u32, Some(&mut out));
    let window = 2 * 4410;
    let energies: Vec<i64> = out.chunks(window).map(energy).collect();
    let peak = *energies.iter().max().unwrap();
    assert!(peak > 0, "the reverb returns the impulse");
    let peak_at = energies.iter().position(|&e| e == peak).unwrap();
    assert!(peak_at < 10, "the response peaks early (window {peak_at})");
    let tail = energies[energies.len() - 5..].iter().sum::<i64>();
    assert!(
        tail * 1000 < peak,
        "the tail decays: peak {peak}, tail {tail}"
    );
    // Both channels carry the return.
    assert!(energy(&out.iter().step_by(2).copied().collect::<Vec<_>>()) > 0);
    assert!(energy(&out.iter().skip(1).step_by(2).copied().collect::<Vec<_>>()) > 0);
    // Stable: the tail keeps shrinking.
    let late = energies[energies.len() - 1];
    let earlier = energies[energies.len() / 2];
    assert!(late <= earlier);

    // With reverb writes disabled nothing enters the buffer: silence.
    let mut quiet = reverb_spu();
    quiet.write16(reg::CONTROL, ON | control::CD_REVERB);
    quiet.push_cd_audio(&[0x7FFF, 0x7FFF]);
    let mut out = vec![0i16; 2 * 44_100];
    quiet.run(44_100, Some(&mut out));
    assert!(out.iter().all(|&s| s == 0));
    assert!(quiet.ram()[0x80000 - 0xADE0..].iter().all(|&b| b == 0));
}

#[test]
fn cd_audio_mixes_through_its_volume() {
    let mut spu = enabled();
    spu.write16(reg::CONTROL, ON | control::CD_AUDIO);
    spu.write16(reg::CD_VOLUME_LEFT, 0x4000);
    spu.write16(reg::CD_VOLUME_RIGHT, 0x7FFF);
    spu.write16(reg::MAIN_VOLUME_LEFT, 0x3FFF);
    // Fixed volume: bits 0-14 hold -0x4000, the volume is -0x8000.
    spu.write16(reg::MAIN_VOLUME_RIGHT, 0x4000);
    spu.run(1, None); // latch the main volumes
    spu.push_cd_audio(&[0x1000, 0x1000, -0x2000, 0x7FFF]);
    let mut out = [0i16; 6];
    spu.run(3, Some(&mut out));
    let main_r = spu.read16(reg::CURRENT_MAIN_VOLUME_RIGHT) as i16 as i32;
    assert_eq!(out[0], ((0x800 * 0x7FFE) >> 15) as i16);
    assert_eq!(out[1], ((((0x1000 * 0x7FFF) >> 15) * main_r) >> 15) as i16);
    assert_eq!(out[2], ((-0x1000 * 0x7FFE) >> 15) as i16);
    assert_eq!(&out[4..], &[0, 0], "an empty queue is silence");
    // Capture buffers hold the raw CD input.
    assert_eq!(i16::from_le_bytes([spu.ram()[2], spu.ram()[3]]), 0x1000);
    assert_eq!(
        i16::from_le_bytes([spu.ram()[0x404], spu.ram()[0x405]]),
        0x7FFF
    );
}

/// A busy SPU: looping voices with reverb, a noise voice, pitch modulation,
/// a sweeping volume and queued CD audio.
fn busy_spu() -> Spu {
    let mut spu = reverb_spu();
    spu.write16(
        reg::CONTROL,
        ON | control::REVERB_ENABLE | control::CD_REVERB | control::CD_AUDIO | (10 << 10),
    );
    let mut data = Vec::new();
    for i in 0..8u8 {
        let flags = match i {
            0 => FLAG_LOOP_START,
            7 => FLAG_LOOP_END | FLAG_LOOP_REPEAT,
            _ => 0,
        };
        let mut b = block(4 + (i % 3), i % 5, flags, 0);
        for (j, byte) in b[2..].iter_mut().enumerate() {
            *byte = (j as u8).wrapping_mul(37).wrapping_add(i * 11);
        }
        data.extend_from_slice(&b);
    }
    upload(&mut spu, 0x1000, &data);
    let adsr1 = (0x30 << 8) | (5 << 4) | 8;
    let adsr2 = 0x4000 | (20 << 6) | 0x20 | 12;
    for v in 0..6 {
        setup_voice(
            &mut spu,
            v,
            0x1000 + 16 * v as u32,
            0x0800 + 0x123 * v as u16,
            adsr1,
            adsr2,
        );
    }
    spu.write16(voice_reg(4, reg::VOICE_VOLUME_LEFT), 0x8000 | (12 << 2) | 1);
    spu.write16(reg::REVERB_ON, 0b11_1111);
    spu.write16(reg::NOISE, 1 << 5);
    spu.write16(reg::PITCH_MODULATION, 1 << 2);
    spu.write16(reg::KEY_ON, 0b11_1111);
    let cd: Vec<i16> = (0..20_000)
        .map(|i: i32| ((i * 997) % 20_000 - 10_000) as i16)
        .collect();
    spu.push_cd_audio(&cd);
    spu
}

#[test]
fn snapshot_round_trip_continues_identically() {
    let mut a = busy_spu();
    a.run(5000, None);
    key_off(&mut a, 3);
    let snap = a.save();
    let mut out_a = vec![0i16; 2 * 8000];
    a.run(8000, Some(&mut out_a));
    assert!(out_a.iter().any(|&s| s != 0));

    let mut b = Spu::new();
    b.load(&snap).expect("load");
    assert_eq!(b.save(), snap);
    let mut out_b = vec![0i16; 2 * 8000];
    b.run(8000, Some(&mut out_b));
    assert_eq!(out_a, out_b);
    assert_eq!(a.save(), b.save());

    // Rejected snapshots leave the SPU unchanged.
    let before = b.save();
    assert_eq!(
        b.load(&snap[..snap.len() - 1]),
        Err(SnapshotError::Truncated)
    );
    let mut bad = snap.clone();
    bad[0] = b'Y';
    assert_eq!(b.load(&bad), Err(SnapshotError::BadMagic));
    let mut extra = snap.clone();
    extra.push(0);
    assert_eq!(b.load(&extra), Err(SnapshotError::TrailingData));
    let mut version = snap.clone();
    version[4] = 99;
    assert_eq!(b.load(&version), Err(SnapshotError::UnsupportedVersion(99)));
    assert_eq!(b.save(), before);
}

#[test]
fn headless_run_matches_run_with_output() {
    let mut a = busy_spu();
    let mut b = busy_spu();
    let mut out = vec![0i16; 2 * 12_000];
    a.run(12_000, Some(&mut out));
    b.run(12_000, None);
    assert_eq!(a.save(), b.save());

    // Splitting a run changes nothing either.
    let mut c = busy_spu();
    for _ in 0..12_000 / 7 {
        c.run(7, None);
    }
    c.run(12_000 % 7, None);
    assert_eq!(a.save(), c.save());
}

#[test]
fn disabling_the_spu_keys_everything_off() {
    let mut spu = busy_spu();
    spu.run(100, None);
    assert!((0..6).any(|v| spu.voice(v).envelope != 0));
    spu.write16(reg::CONTROL, 0);
    assert!(
        (0..VOICES_USED).all(|v| spu.voice(v).phase == Phase::Off && spu.voice(v).envelope == 0)
    );
    key_on(&mut spu, 0);
    assert_eq!(
        spu.voice(0).phase,
        Phase::Off,
        "key on is ignored while off"
    );
}

const VOICES_USED: usize = 6;
