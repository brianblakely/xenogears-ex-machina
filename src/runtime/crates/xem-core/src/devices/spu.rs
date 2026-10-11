//! SPU: the register window, transfers, the SPU IRQ and mixing over
//! [`xem_spu::Spu`].
//!
//! - **Register window.** libspu and the sound driver write the registers into
//!   game memory at 0x1F801C00-0x1F801FFF (src/port/include/xem/spu.h). After every
//!   step and interrupt (`sync`) and on `xem.spu_sync`, the device reads the
//!   0x400-byte window, applies it with [`xem_spu::Spu::sync_window`] and writes
//!   the hardware values back. The crate documents that contract's limits: the
//!   sync sees values, so two writes to one register between syncs collapse;
//!   the port syncs at each of libspu's hardware waits, and transfers never go
//!   through the window.
//! - **Transfers.** `xem.spu_transfer` (DMA channel 4) moves the data at once
//!   and raises the DMA interrupt for channel 4 after [`TRANSFER_CYCLES_PER_HALFWORD`]
//!   per halfword; `xem.spu_write_io` (the FIFO) completes within the call.
//! - **IRQ.** When the SPU's IRQ flag rises (a voice, transfer, capture or
//!   reverb access at the IRQ address) the device raises interrupt 9; the flag
//!   stays up until the game acknowledges it through SPUCNT bit 6.
//! - **Time.** `advance` runs the SPU in whole 44.1 kHz samples up to the
//!   clock ([`CYCLES_PER_SAMPLE`] cycles each). With output enabled the samples
//!   go to a ring the host drains with [`Spu::take_audio`]; without it (headless)
//!   the SPU advances the same state without producing them.
//! - **CD input.** The runtime feeds the CD device's decoded audio to
//!   [`Spu::push_cd_audio`] before the SPU advances (`Services::route_cd_audio`).

use std::collections::VecDeque;

use super::{Context, Device, unknown};
use crate::clock::CPU_HZ;
use crate::module::Action;
use crate::runtime::IRQ_DMA;
use crate::snapshot::{Reader, SnapshotError, Writer};

/// The register window's address in game memory.
pub const REGISTERS: u32 = 0x1F80_1C00;
/// CPU cycles per 44.1 kHz sample (33868800 / 44100).
pub const CYCLES_PER_SAMPLE: u64 = CPU_HZ / xem_spu::SAMPLE_RATE as u64;
/// The modelled DMA-4 rate: 16 CPU cycles per halfword moved (the rate the
/// DuckStation emulator gives SPU transfers; psx-spx states none), so a
/// 64 KB wave bank takes about 15.5 ms.
pub const TRANSFER_CYCLES_PER_HALFWORD: u64 = 16;
/// The SPU interrupt (src/port/include/xem/port.h XEM_IRQ_SPU).
pub const IRQ_SPU: u32 = 9;
/// DMA channel 4, the SPU's.
pub const DMA_SPU: u32 = 4;
/// Output samples (interleaved L/R) the ring keeps for a slow host: 2 s.
pub const OUTPUT_LIMIT: usize = 2 * 2 * xem_spu::SAMPLE_RATE as usize;

const SNAPSHOT_VERSION: u32 = 4;
// Divisible by every supported factor (1..20), preserving fractional audio
// cycles across arbitrary speed changes and snapshot/rewind.
const AUDIO_UNITS: u64 = 232_792_560;

pub struct Spu {
    chip: xem_spu::Spu,
    /// Cycles at which started DMA-4 transfers complete, in order.
    transfers: VecDeque<u64>,
    /// The IRQ flag was up at the last check (interrupt 9 already raised).
    irq_raised: bool,
    /// Output ring (interleaved L/R) when a host consumes audio.
    output: Option<VecDeque<i16>>,
    scratch: Vec<i16>,
    virtual_now: u64,
    audio_cycles: u64,
    audio_fraction: u64,
    speed: u32,
    external_audio: bool,
}

impl Default for Spu {
    fn default() -> Self {
        Spu {
            chip: xem_spu::Spu::new(),
            transfers: VecDeque::new(),
            irq_raised: false,
            output: None,
            scratch: Vec::new(),
            virtual_now: 0,
            audio_cycles: 0,
            audio_fraction: 0,
            speed: 1,
            external_audio: false,
        }
    }
}

impl Spu {
    pub fn speed(&self) -> u32 { self.speed }
    pub fn audio_cycles(&self) -> u64 { self.audio_cycles }
    pub fn set_external_audio(&mut self, enabled: bool) { self.external_audio = enabled; }

    pub fn set_speed(&mut self, speed: u32) {
        assert!((1..=20).contains(&speed));
        if self.speed != speed && let Some(ring) = &mut self.output { ring.clear(); }
        self.speed = speed;
    }
    pub(crate) fn restore_state(&mut self, mut state: Self) {
        state.set_output(self.output.is_some());
        state.external_audio = self.external_audio;
        state.speed = self.speed;
        *self = state;
    }
    /// The SPU itself (RAM, voices, registers), for inspection.
    pub fn chip(&self) -> &xem_spu::Spu {
        &self.chip
    }

    /// Produce output samples from now on (a host plays them), or stop.
    pub fn set_output(&mut self, enabled: bool) {
        self.output = enabled.then(VecDeque::new);
    }

    /// Move up to `out.len()` produced samples (interleaved L/R) into `out`;
    /// returns how many.
    pub fn take_audio(&mut self, out: &mut [i16]) -> usize {
        let Some(ring) = self.output.as_mut() else { return 0 };
        let n = out.len().min(ring.len());
        for (slot, sample) in out.iter_mut().zip(ring.drain(..n)) {
            *slot = sample;
        }
        n
    }

    /// Samples waiting in the output ring.
    pub fn audio_queued(&self) -> usize {
        self.output.as_ref().map_or(0, VecDeque::len)
    }

    /// Queue CD audio (44.1 kHz, interleaved L/R) for the SPU's CD input.
    pub fn push_cd_audio(&mut self, interleaved: &[i16]) {
        self.chip.push_cd_audio(interleaved);
    }

    fn sync_window(&mut self, context: &mut Context<'_>) -> bool {
        let mut window = [0u8; xem_spu::WINDOW_SIZE];
        if let Err(error) = context.memory.read(REGISTERS, &mut window) {
            *context.trap_reason = Some(error.to_string());
            return false;
        }
        self.chip.sync_window(&mut window);
        if let Err(error) = context.memory.write(REGISTERS, &window) {
            *context.trap_reason = Some(error.to_string());
            return false;
        }
        self.check_irq(context);
        true
    }

    /// Raise interrupt 9 when the IRQ flag rose.
    fn check_irq(&mut self, context: &mut Context<'_>) {
        let flag = self.chip.irq_flag();
        if flag && !self.irq_raised {
            context.raised.push((IRQ_SPU, 0));
        }
        self.irq_raised = flag;
    }

    /// Point the transfer address at `spu_address` (bytes) after applying the
    /// window, as the original's write to 0x1F801DA6 before each transfer.
    fn start_transfer(&mut self, spu_address: u32, context: &mut Context<'_>) -> bool {
        if !self.sync_window(context) {
            return false;
        }
        self.chip.write16(xem_spu::reg::TRANSFER_ADDRESS, (spu_address >> 3) as u16);
        true
    }

    fn transfer(&mut self, args: &[u32], context: &mut Context<'_>) -> Action {
        let [spu_address, ram, bytes, direction] = [0, 1, 2, 3].map(|i| args.get(i).copied().unwrap_or(0));
        if !self.start_transfer(spu_address, context) {
            return Action::Trap;
        }
        let mut data = vec![0u8; bytes as usize];
        let moved = if direction == 0 {
            context.memory.read(ram, &mut data).map(|()| self.chip.dma_write(&data))
        } else {
            self.chip.dma_read(&mut data);
            context.memory.write(ram, &data)
        };
        if let Err(error) = moved {
            *context.trap_reason = Some(format!("SPU transfer: {error}"));
            return Action::Trap;
        }
        self.check_irq(context);
        let busy_from = self.transfers.back().copied().unwrap_or(0).max(context.now);
        self.transfers.push_back(busy_from + (bytes as u64 / 2) * TRANSFER_CYCLES_PER_HALFWORD);
        Action::Return(0)
    }

    fn write_io(&mut self, args: &[u32], context: &mut Context<'_>) -> Action {
        let [spu_address, data_address, bytes] = [0, 1, 2].map(|i| args.get(i).copied().unwrap_or(0));
        if !self.start_transfer(spu_address, context) {
            return Action::Trap;
        }
        let mut data = vec![0u8; bytes as usize];
        if let Err(error) = context.memory.read(data_address, &mut data) {
            *context.trap_reason = Some(format!("SPU FIFO write: {error}"));
            return Action::Trap;
        }
        self.chip.dma_write(&data);
        self.check_irq(context);
        Action::Return(0)
    }

    /// Run the SPU up to cycle `now`.
    fn run_until(&mut self, now: u64, context: &mut Context<'_>) {
        if self.external_audio { self.virtual_now = self.virtual_now.max(now); return; }
        let delta = now.saturating_sub(self.virtual_now);
        let units = u128::from(delta) * u128::from(AUDIO_UNITS / u64::from(self.speed)) + u128::from(self.audio_fraction);
        self.audio_cycles += (units / u128::from(AUDIO_UNITS)) as u64;
        self.audio_fraction = (units % u128::from(AUDIO_UNITS)) as u64;
        self.virtual_now = self.virtual_now.max(now);
        self.render_audio(context);
    }

    /// Normal music time supplied by the pacing owner, independent of how many
    /// accelerated game frames fit its work budget.
    pub fn advance_audio(&mut self, cycles: u64, context: &mut Context<'_>) {
        self.audio_cycles += cycles;
        self.render_audio(context);
    }

    fn render_audio(&mut self, context: &mut Context<'_>) {
        let target = self.audio_cycles / CYCLES_PER_SAMPLE;
        while self.chip.clock() < target {
            let n = (target - self.chip.clock()).min(4096) as u32;
            match self.output.as_mut() {
                Some(ring) => {
                    self.scratch.resize(n as usize * 2, 0);
                    self.chip.run(n, Some(&mut self.scratch));
                    ring.extend(self.scratch.iter().copied());
                    let excess = ring.len().saturating_sub(OUTPUT_LIMIT);
                    ring.drain(..excess);
                }
                None => self.chip.run(n, None),
            }
            self.check_irq(context);
        }
    }
}

impl Device for Spu {
    fn import(&mut self, op: &str, args: &[u32], context: &mut Context<'_>) -> Action {
        match op {
            "sync" => {
                if self.sync_window(context) {
                    Action::Return(0)
                } else {
                    Action::Trap
                }
            }
            "transfer" => self.transfer(args, context),
            "write_io" => self.write_io(args, context),
            _ => unknown("spu", op, context),
        }
    }

    fn next_event(&self) -> Option<u64> {
        self.transfers.front().copied()
    }

    fn run_events(&mut self, context: &mut Context<'_>) {
        while self.transfers.front().is_some_and(|&at| at <= context.now) {
            self.transfers.pop_front();
            context.raised.push((IRQ_DMA, DMA_SPU));
        }
    }

    fn advance(&mut self, context: &mut Context<'_>) {
        self.run_until(context.now, context);
    }

    fn sync(&mut self, context: &mut Context<'_>) {
        self.sync_window(context);
    }

    fn save(&self) -> Vec<u8> {
        let mut w = Writer::default();
        w.u32(SNAPSHOT_VERSION);
        w.bytes(&self.chip.save());
        w.u32(self.transfers.len() as u32);
        for &at in &self.transfers { w.u64(at); }
        w.u32(self.irq_raised as u32);
        w.u64(self.virtual_now);
        w.u64(self.audio_cycles);
        w.u64(self.audio_fraction);
        w.0
    }

    fn load(&mut self, state: &[u8]) -> Result<(), String> {
        let read = |state: &[u8]| -> Result<_, SnapshotError> {
            let mut r = Reader::new(state);
            if r.u32()? != SNAPSHOT_VERSION {
                return Err(SnapshotError::Format("SPU device version"));
            }
            let mut chip = xem_spu::Spu::new();
            chip.load(r.bytes()?).map_err(|e| SnapshotError::Device(e.to_string()))?;
            let count = r.count(8, 65536)?;
            let mut transfers = VecDeque::with_capacity(count);
            for _ in 0..count {
                transfers.push_back(r.u64()?);
            }
            let irq_raised = r.u32()? != 0;
            let virtual_now = r.u64()?;
            let audio_cycles = r.u64()?;
            let audio_fraction = r.u64()?;
            if audio_fraction >= AUDIO_UNITS || audio_cycles / CYCLES_PER_SAMPLE != chip.clock() {
                return Err(SnapshotError::Format("SPU audio clock"));
            }
            if !r.done() {
                return Err(SnapshotError::Format("trailing SPU device bytes"));
            }
            Ok((chip, transfers, irq_raised, virtual_now, audio_cycles, audio_fraction))
        };
        let (chip, transfers, irq_raised, virtual_now, audio_cycles, audio_fraction) = read(state).map_err(|e| e.to_string())?;
        self.chip = chip;
        self.transfers = transfers;
        self.irq_raised = irq_raised;
        self.virtual_now = virtual_now;
        self.audio_cycles = audio_cycles;
        self.audio_fraction = audio_fraction;
        if let Some(ring) = self.output.as_mut() {
            ring.clear();
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::memory::{GameMemory, OutOfBounds};
    use xem_spu::{control, reg};

    /// The register window and a buffer at 0x1F802000 (the test's "RAM").
    struct Memory(Vec<u8>);

    const BASE: u32 = 0x1F80_1000;
    const BUFFER: u32 = 0x1F80_2000;

    impl GameMemory for Memory {
        fn read(&self, address: u32, out: &mut [u8]) -> Result<(), OutOfBounds> {
            let start = address.checked_sub(BASE).ok_or(OutOfBounds { address, len: out.len() })? as usize;
            let src = self.0.get(start..start + out.len()).ok_or(OutOfBounds { address, len: out.len() })?;
            out.copy_from_slice(src);
            Ok(())
        }
        fn write(&mut self, address: u32, data: &[u8]) -> Result<(), OutOfBounds> {
            let start = address.checked_sub(BASE).ok_or(OutOfBounds { address, len: data.len() })? as usize;
            let dst = self.0.get_mut(start..start + data.len()).ok_or(OutOfBounds { address, len: data.len() })?;
            dst.copy_from_slice(data);
            Ok(())
        }
    }

    /// Game memory with the transfer type libspu's _spu_init sets (0x1F801DAC = 4,
    /// normal writes).
    fn memory() -> Memory {
        let mut mem = Memory(vec![0u8; 0x3000]);
        write_reg(&mut mem, reg::TRANSFER_CONTROL, 4);
        mem
    }

    fn with_context<R>(
        mem: &mut Memory,
        now: u64,
        raised: &mut Vec<(u32, u32)>,
        f: impl FnOnce(&mut Context<'_>) -> R,
    ) -> R {
        let mut reason = None;
        let mut context = Context { memory: mem, now, raised, trap_reason: &mut reason };
        let result = f(&mut context);
        assert_eq!(reason, None);
        result
    }

    fn write_reg(mem: &mut Memory, offset: usize, value: u16) {
        mem.write(REGISTERS + offset as u32, &value.to_le_bytes()).unwrap();
    }

    fn read_reg(mem: &mut Memory, offset: usize) -> u16 {
        let mut b = [0; 2];
        mem.read(REGISTERS + offset as u32, &mut b).unwrap();
        u16::from_le_bytes(b)
    }

    #[test]
    fn window_writes_reach_the_spu_and_hardware_values_come_back() {
        let mut mem = memory();
        let mut spu = Spu::default();
        let mut raised = Vec::new();
        write_reg(&mut mem, reg::CONTROL, control::ENABLE | control::UNMUTE);
        write_reg(&mut mem, reg::MAIN_VOLUME_LEFT, 0x3FFF);
        write_reg(&mut mem, reg::KEY_ON, 1);
        with_context(&mut mem, 0, &mut raised, |c| spu.sync(c));
        assert_eq!(spu.chip().read16(reg::MAIN_VOLUME_LEFT), 0x3FFF);
        // KON is write-triggered: the window reads 0 afterwards.
        assert_eq!(read_reg(&mut mem, reg::KEY_ON), 0);
        assert_eq!(spu.chip().voice(0).phase, xem_spu::Phase::Attack);
    }

    #[test]
    fn a_transfer_moves_data_at_once_and_completes_after_the_modelled_time() {
        let mut mem = memory();
        let mut spu = Spu::default();
        let mut raised = Vec::new();
        let data: Vec<u8> = (0..0x400u32).map(|i| i as u8).collect();
        mem.write(BUFFER, &data).unwrap();
        let action = with_context(&mut mem, 1000, &mut raised, |c| spu.import("transfer", &[0x1010, BUFFER, 0x400, 0], c));
        assert_eq!(action, Action::Return(0));
        assert_eq!(&spu.chip().ram()[0x1010..0x1410], &data[..]);
        let due = 1000 + 0x200 * TRANSFER_CYCLES_PER_HALFWORD;
        assert_eq!(spu.next_event(), Some(due));
        with_context(&mut mem, due - 1, &mut raised, |c| spu.run_events(c));
        assert!(raised.is_empty());
        with_context(&mut mem, due, &mut raised, |c| spu.run_events(c));
        assert_eq!(raised, vec![(IRQ_DMA, DMA_SPU)]);
        assert_eq!(spu.next_event(), None);

        // Read it back into another buffer.
        raised.clear();
        with_context(&mut mem, due, &mut raised, |c| spu.import("transfer", &[0x1010, BUFFER + 0x800, 0x400, 1], c));
        let mut back = vec![0u8; 0x400];
        mem.read(BUFFER + 0x800, &mut back).unwrap();
        assert_eq!(back, data);
    }

    #[test]
    fn transfers_to_one_address_twice_both_land_there() {
        // The window cannot see a repeated transfer address; the import sets it.
        let mut mem = memory();
        let mut spu = Spu::default();
        let mut raised = Vec::new();
        mem.write(BUFFER, &[0xAA; 64]).unwrap();
        with_context(&mut mem, 0, &mut raised, |c| spu.import("transfer", &[0x2000, BUFFER, 64, 0], c));
        mem.write(BUFFER, &[0x55; 64]).unwrap();
        with_context(&mut mem, 0, &mut raised, |c| spu.import("write_io", &[0x2000, BUFFER, 64], c));
        assert_eq!(&spu.chip().ram()[0x2000..0x2040], &[0x55; 64]);
        assert_eq!(spu.chip().ram()[0x2040], 0);
    }

    #[test]
    fn advance_runs_whole_samples_and_produces_output_only_when_asked() {
        let mut mem = memory();
        let mut spu = Spu::default();
        let mut raised = Vec::new();
        with_context(&mut mem, CYCLES_PER_SAMPLE * 100 + 5, &mut raised, |c| spu.advance(c));
        assert_eq!(spu.chip().clock(), 100);
        assert_eq!(spu.audio_queued(), 0);
        spu.set_output(true);
        with_context(&mut mem, CYCLES_PER_SAMPLE * 150, &mut raised, |c| spu.advance(c));
        assert_eq!(spu.chip().clock(), 150);
        assert_eq!(spu.audio_queued(), 100);
        let mut out = [1i16; 64];
        assert_eq!(spu.take_audio(&mut out), 64);
        assert_eq!(spu.audio_queued(), 36);
    }

    #[test]
    fn fast_forward_preserves_audio_samples_and_pitch() {
        for speed in 1..=20 {
            let mut normal = Spu::default();
            let mut fast = Spu::default();
            let mut mem = memory();
            let mut raised = Vec::new();
            normal.set_output(true);
            fast.set_output(true);
            // Looping ADPCM tone, fixed voice pitch and ADSR. Compare actual
            // rendered samples and voice phase, rather than just sample counts.
            for spu in [&mut normal, &mut fast] {
                let mut block = [0x11; 16]; block[0] = 0; block[1] = 7;
                spu.chip.ram_mut()[0x1000..0x1010].copy_from_slice(&block);
                for (reg, value) in [(0,0x3fff), (2,0x3fff), (4,0x1000), (6,0x200), (8,0x00ff), (10,0x1fc0),
                    (reg::MAIN_VOLUME_LEFT,0x3fff), (reg::MAIN_VOLUME_RIGHT,0x3fff),
                    (reg::CONTROL,control::ENABLE | control::UNMUTE), (reg::KEY_ON,1)] {
                    spu.chip.write16(reg,value);
                }
            }
            fast.set_speed(speed);
            with_context(&mut mem, CPU_HZ / 10, &mut raised, |c| normal.advance(c));
            with_context(&mut mem, CPU_HZ / 10 * u64::from(speed), &mut raised, |c| fast.advance(c));
            assert_eq!(fast.chip.clock(), 4410);
            assert_eq!(fast.chip.save(), normal.chip.save());
            let mut a = vec![0; normal.audio_queued()];
            let mut b = vec![0; fast.audio_queued()];
            normal.take_audio(&mut a); fast.take_audio(&mut b);
            assert!(a.iter().any(|s| *s != 0));
            assert_eq!(a,b);
        }
    }

    #[test]
    fn restore_preserves_fractional_audio_time_across_speed_changes() {
        let mut mem = memory();
        let mut raised = Vec::new();
        let mut spu = Spu::default();
        spu.set_speed(7);
        with_context(&mut mem, 1001, &mut raised, |c| spu.advance(c));
        let saved = spu.save();
        let mut restored = Spu::default(); restored.load(&saved).unwrap();
        for (at,speed) in [(5001,3),(9012,16),(15003,1)] {
            for s in [&mut spu,&mut restored] {
                s.set_speed(speed);
                with_context(&mut mem,at,&mut raised,|c| s.advance(c));
            }
            assert_eq!(spu.save(),restored.save());
        }
    }

    #[test]
    fn the_irq_flag_raises_interrupt_9_once_until_acknowledged() {
        let mut mem = memory();
        let mut spu = Spu::default();
        let mut raised = Vec::new();
        // IRQ at SPU address 0x1000, enabled; a transfer through it hits it.
        write_reg(&mut mem, reg::IRQ_ADDRESS, 0x1000 >> 3);
        write_reg(&mut mem, reg::CONTROL, control::ENABLE | control::IRQ_ENABLE);
        mem.write(BUFFER, &[1; 64]).unwrap();
        with_context(&mut mem, 0, &mut raised, |c| spu.import("transfer", &[0x1000, BUFFER, 64, 0], c));
        assert_eq!(raised, vec![(IRQ_SPU, 0)]);
        with_context(&mut mem, 0, &mut raised, |c| spu.import("transfer", &[0x1000, BUFFER, 64, 0], c));
        assert_eq!(raised.len(), 1);
        // Acknowledge (SPUCNT bit 6 off), re-enable, hit again.
        write_reg(&mut mem, reg::CONTROL, control::ENABLE);
        with_context(&mut mem, 0, &mut raised, |c| spu.import("sync", &[], c));
        write_reg(&mut mem, reg::CONTROL, control::ENABLE | control::IRQ_ENABLE);
        with_context(&mut mem, 0, &mut raised, |c| spu.import("transfer", &[0x1000, BUFFER, 64, 0], c));
        assert_eq!(raised, vec![(IRQ_SPU, 0), (IRQ_SPU, 0)]);
    }

    #[test]
    fn save_and_load_restore_the_device() {
        let mut mem = memory();
        let mut spu = Spu::default();
        let mut raised = Vec::new();
        mem.write(BUFFER, &[7; 128]).unwrap();
        with_context(&mut mem, 10, &mut raised, |c| spu.import("transfer", &[0x3000, BUFFER, 128, 0], c));
        with_context(&mut mem, CYCLES_PER_SAMPLE * 3, &mut raised, |c| spu.advance(c));
        let state = spu.save();
        let mut other = Spu::default();
        other.load(&state).unwrap();
        assert_eq!(other.save(), state);
        assert_eq!(other.next_event(), spu.next_event());
        assert_eq!(&other.chip().ram()[0x3000..0x3080], &[7; 128]);
        assert!(other.load(&state[..state.len() - 1]).is_err());
    }
}
