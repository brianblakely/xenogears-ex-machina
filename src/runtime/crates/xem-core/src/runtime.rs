//! The run loop: start, suspend, resume and restart the game, and advance the
//! virtual clock at its waits.

use crate::clock::{Clock, Tick, VBLANK_CYCLES};
use crate::devices::{self, Context, Device};
use crate::memory::GameMemory;
use crate::module::{Action, AsyncState, GameModule, Import, ImportHandler, Trap};
use crate::pad::Pads;

/// Where `xem_run` starts (src/port/include/xem/port.h).
pub const RESTART_BOOT: u32 = 0;
pub const RESTART_DISPATCH: u32 = 1;
/// The console's reset: the port returns the BIOS's own state to power-on,
/// then boots as `RESTART_BOOT` (src/port/boot.c).
pub const RESTART_POWER_ON: u32 = 2;

/// The fibers (src/port/include/xem/port.h): the game, whose bottom frame is
/// `xem_run`, and the arena task, whose bottom frame is `xem_task_run`.
pub const FIBER_GAME: u32 = 0;
pub const FIBER_TASK: u32 = 1;

/// Interrupt sources (src/port/include/xem/port.h, the PS1's I_STAT bits).
pub const IRQ_VBLANK: u32 = 0;
pub const IRQ_DMA: u32 = 3;
pub const IRQ_RCNT0: u32 = 4;

/// Yield reasons (src/port/include/xem/port.h).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum YieldReason {
    VSync,
    DrawSync,
    Poll,
    Other(u32),
}

impl YieldReason {
    fn from_raw(raw: u32) -> Self {
        match raw {
            1 => YieldReason::VSync,
            2 => YieldReason::DrawSync,
            3 => YieldReason::Poll,
            other => YieldReason::Other(other),
        }
    }
}

/// Why `Runtime::step` returned.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Stop {
    /// The game waits; the host advances the clock and resumes it.
    Yield(YieldReason),
    /// The game abandoned its stack; the next step starts `xem_run(kind, arg)`.
    Restart { kind: u32, arg: u32 },
    /// `xem_run` returned, which the original program never does.
    Returned,
}

/// What the import that unwound the game asked for.
#[derive(Debug, Clone, PartialEq, Eq)]
enum Pending {
    Stop(Stop),
    /// Suspend the running fiber and continue `fiber`, afresh on the shadow
    /// stack ending at `stack_top` when that is nonzero.
    Switch { fiber: u32, stack_top: u32 },
}

/// The services the imports reach: the console's devices and the clock.
#[derive(Default)]
pub struct Services {
    pending: Option<Pending>,
    pub(crate) focus_pause: crate::focus::FocusPause,
    /// The host's view for the game's actors (src/port/activity.c): the
    /// field camera's pull-back in percent plus 100 (bits 0-15) and the
    /// pixels shown beyond each side (bits 16-31); 0 the original's.
    pub(crate) actor_view: u32,
    /// The host keeps the battle's attack page shown through the attack
    /// sequence (src/port/attack_page.c): the player began it by pointing at
    /// its attacks.
    pub(crate) attack_page: bool,
    /// The host's sound mode, the game's own (src/port/sound.c): 0 Mono,
    /// 1 Stereo, 2 Wide; none leaves the game's as the original sets it.
    pub(crate) sound_mode: Option<u32>,
    /// The host changed its sound mode since the driver's tick took it.
    pub(crate) sound_mode_changed: bool,
    /// A mode the game's Sound screen chose that the host has not taken
    /// for its setting yet (`Session::take_sound_mode_choice`).
    pub(crate) sound_mode_chosen: Option<u32>,
    /// The field maps whose random battles the host turns off
    /// (src/port/encounters.c), ascending, each once; none the original's.
    pub(crate) random_battles_off: Vec<u16>,
    /// The host halves the field's random battles elsewhere (the draw runs
    /// every other field frame); off the original's.
    pub(crate) half_encounters: bool,
    /// Where the Kanji font in game memory came from
    /// ([`Runtime::install_kanji_font`]); none before one is placed.
    pub kanji_font: Option<crate::bios::KanjiFont>,
    /// Why the BIOS image the player imported is not used: the native boot
    /// could not read it or `bios::check` refused it, and placed the
    /// substitute glyphs (xem_game `BootOptions::stored_bios`). The native
    /// hosts show it on their storage line until an image is installed.
    pub kanji_font_notice: Option<String>,
    /// The glyphs a message window's step reveals at least
    /// (src/port/text_speed.c); 0 the original's, which the import also
    /// answers while a field script counts out a page before its wait for
    /// Confirm ([`crate::dialogue::page_counted_out`]).
    pub(crate) text_speed: u32,
    /// The host lets the player's press end any movie
    /// (src/port/movie_skip.c).
    pub(crate) movie_skip: bool,
    /// The disc in the drive could not give one of the party's field sprite
    /// blocks for the full-size alternatives (`full_sprite`): those not read
    /// yet are not asked again.
    full_sprites_unreadable: bool,
    pub clock: Clock,
    pub pads: Pads,
    pub presentation: crate::presentation::Presentation,
    /// The mode the dispatcher last entered, and how many times it dispatched.
    pub mode: Option<u32>,
    pub dispatches: u64,
    /// The game's pad queue reads (pad_dequeue_state) in the current and the
    /// last complete frame.
    pub input_reads: u32,
    pub input_reads_last_frame: u32,
    /// What the game last waited for.
    pub last_wait: Option<YieldReason>,
    pub gpu: devices::gpu::Gpu,
    pub cd: devices::cd::Cd,
    pub mdec: devices::mdec::Mdec,
    pub spu: devices::spu::Spu,
    pub card: devices::card::Card,
    pub missing: Vec<u32>,
    pub log: Vec<String>,
    pub trap_reason: Option<String>,
    /// Names of the trapping stubs, by number (build/game/stubs.txt).
    pub stub_names: Vec<String>,
    /// Interrupts raised by device services, delivered at the next wait.
    pub raised: Vec<(u32, u32)>,
}

impl Services {
    fn devices(&mut self) -> [(&'static str, &mut dyn Device); 5] {
        [
            ("gpu_", &mut self.gpu),
            ("cd_", &mut self.cd),
            ("mdec_", &mut self.mdec),
            ("spu_", &mut self.spu),
            ("card_", &mut self.card),
        ]
    }

    /// Put the user's disc `number` (0 for a disc without one) in the drive.
    pub fn insert_disc(&mut self, number: u8, disc: Box<dyn xem_disc::SectorSource>) {
        self.cd.insert_disc(number, disc);
    }

    /// Give the drive's changer the game's other disc `number`, for the
    /// game's disc changes (a game waiting for it gets it now).
    pub fn add_disc(&mut self, number: u8, disc: Box<dyn xem_disc::SectorSource>) {
        self.cd.add_disc(number, disc);
    }

    pub fn set_playback_speed(&mut self, speed: u32) {
        self.clock.set_audio_speed(speed);
        self.spu.set_speed(speed);
    }

    /// Feed the CD device's decoded audio to the SPU's CD input.
    fn route_cd_audio(&mut self) {
        let mut pcm = Vec::new();
        self.cd.take_audio(&mut pcm);
        if !pcm.is_empty() {
            self.spu.push_cd_audio(&pcm);
        }
    }

    /// Hand the colour macroblocks the MDEC decoded to the GPU's draw trace,
    /// which finds them in the transfers that load their output into VRAM
    /// (presentation only: the MDEC keeps them only while a trace is on).
    fn route_movie(&mut self) {
        let tracing = self.gpu.device().trace_enabled();
        self.mdec.keep_decoded(tracing);
        let blocks = self.mdec.take_decoded();
        if !blocks.is_empty() {
            self.gpu.device_mut().trace_macroblocks(blocks.into_iter().map(|block| {
                let xem_media::mdec::Macroblock { luma, cb, cr, quant, bits24, key } = block;
                (key, xem_gpu::MovieBlock { luma, cb, cr, quant, bits24 })
            }));
        }
    }

    /// The earliest scheduled device event.
    pub fn next_device_event(&mut self) -> Option<u64> {
        self.devices().into_iter().filter_map(|(_, d)| d.next_event()).min()
    }

    /// Call `f` on every device with a context.
    fn each_device(&mut self, memory: &mut dyn GameMemory, mut f: impl FnMut(&mut dyn Device, &mut Context<'_>)) {
        let now = self.clock.now;
        let mut raised = std::mem::take(&mut self.raised);
        let mut reason = self.trap_reason.take();
        for (_, device) in self.devices() {
            f(device, &mut Context { memory, now, raised: &mut raised, trap_reason: &mut reason });
        }
        self.raised = raised;
        self.trap_reason = reason;
        self.clock.audio_now = self.spu.audio_cycles();
        if self.clock.external_audio { self.clock.reconcile_audio_due(); }
    }

    /// Run the device events due now.
    fn run_device_events(&mut self, memory: &mut dyn GameMemory) {
        let now = self.clock.now;
        let mut raised = std::mem::take(&mut self.raised);
        let mut reason = self.trap_reason.take();
        for (_, device) in self.devices() {
            if device.next_event().is_some_and(|at| at <= now) {
                device.run_events(&mut Context { memory, now, raised: &mut raised, trap_reason: &mut reason });
            }
        }
        self.raised = raised;
        self.trap_reason = reason;
        self.route_movie();
    }

    /// Where the port finds `character`'s field sprite block (directory 4,
    /// file character + 5, unpacked; `presentation::FULL_SPRITES`), read from
    /// the disc in the drive the first time it is asked for; 0 while none
    /// is there (no disc in the drive, its bytes still being fetched, or a
    /// disc that cannot give it). Presentation only: the drive and the game
    /// see nothing of it, and snapshots hold none of it.
    fn full_sprite(&mut self, memory: &mut dyn GameMemory, character: u32) -> u32 {
        use crate::presentation::{FULL_SPRITES, FULL_SPRITE_BYTES, unpack_lzss};
        if character >= 11 {
            return 0;
        }
        let address = FULL_SPRITES + character * FULL_SPRITE_BYTES;
        if memory.read_u32(address).is_ok_and(|sections| sections != 0) {
            return address;
        }
        if self.full_sprites_unreadable {
            return 0;
        }
        match self.cd.read_file(4, character + 5) {
            Ok(None) | Err(xem_disc::Error::NotReady) => 0,
            Ok(Some(packed)) => match unpack_lzss(&packed, FULL_SPRITE_BYTES as usize) {
                Some(block) if block.len() >= 4 && memory.write(address, &block).is_ok() => address,
                _ => {
                    self.full_sprites_unreadable = true;
                    0
                }
            },
            Err(_) => {
                self.full_sprites_unreadable = true;
                0
            }
        }
    }

    fn stub_name(&self, id: u32) -> String {
        if id >= 0x10000 {
            return format!("unmapped inline assembly #{}", id - 0x10000);
        }
        self.stub_names.get(id as usize).cloned().unwrap_or_else(|| format!("stub #{id}"))
    }
}

impl ImportHandler for Services {
    fn import(&mut self, import: Import<'_>, memory: &mut dyn GameMemory) -> Action {
        let arg = |i: usize| import.args.get(i).copied().unwrap_or(0);
        match import.name {
            "yield" => {
                self.last_wait = Some(YieldReason::from_raw(arg(0)));
                self.pending = Some(Pending::Stop(Stop::Yield(YieldReason::from_raw(arg(0)))));
                Action::Unwind
            }
            "restart" => {
                self.pending = Some(Pending::Stop(Stop::Restart { kind: arg(0), arg: arg(1) }));
                Action::Unwind
            }
            "task_switch" => {
                self.pending = Some(Pending::Switch { fiber: arg(0), stack_top: arg(1) });
                Action::Unwind
            }
            "rcnt_set" => {
                self.clock.set_counter(arg(0) as usize % 3, arg(1), arg(2));
                Action::Return(0)
            }
            "rcnt_start" => {
                self.clock.start_counter(arg(0) as usize % 3);
                Action::Return(0)
            }
            "rcnt_stop" => {
                self.clock.stop_counter(arg(0) as usize % 3);
                Action::Return(0)
            }
            "rcnt_read" => Action::Return(self.clock.read_counter(arg(0) as usize % 3)),
            "pad_read" => match self.pads.write_receive_buffer(arg(0), arg(1), arg(2), memory) {
                Ok(()) => Action::Return(0),
                Err(error) => {
                    self.trap_reason = Some(error.to_string());
                    Action::Trap
                }
            },
            "poll_now" => {
                // A wait inside an interrupt handler: the earliest device event
                // happens now (time passes as the hardware's does while a
                // handler spins); its interrupts stay pending until the
                // handler returns.
                match self.next_device_event() {
                    Some(at) => {
                        self.clock.now = self.clock.now.max(at);
                        self.each_device(memory, |device, context| device.advance(context));
                        self.run_device_events(memory);
                        Action::Return(1)
                    }
                    None => Action::Return(0),
                }
            }
            "mode" => {
                self.mode = Some(arg(0));
                self.dispatches += 1;
                Action::Return(0)
            }
            "input_read" => {
                self.input_reads += 1;
                Action::Return(0)
            }
            "pause_requested" => Action::Return(u32::from(self.focus_pause.requested())),
            "actor_view" => Action::Return(self.actor_view),
            "attack_page" => Action::Return(u32::from(self.attack_page)),
            "random_battles" => {
                // The draw's pace on the map: 0 off, 2 half, 1 the original's.
                let off = u16::try_from(arg(0)).is_ok_and(|map| self.random_battles_off.binary_search(&map).is_ok());
                Action::Return(if off { 0 } else if self.half_encounters { 2 } else { 1 })
            }
            // What a battle gives: twice its experience, gold and use counts
            // while the encounters are halved, wherever the battle is.
            "reward_scale" => Action::Return(if self.half_encounters { 2 } else { 1 }),
            // A party character's field sprite block, for the full-size
            // alternative of its small sprite (src/port/sprite_facing.c).
            "full_sprite" => Action::Return(self.full_sprite(memory, arg(0))),
            // The original's reveal while a field script counts out a page
            // before its wait for Confirm (the opening narration's).
            "text_speed" => Action::Return(
                if self.text_speed != 0 && crate::dialogue::page_counted_out(&*memory, self.mode) { 0 } else { self.text_speed },
            ),
            "movie_skip" => Action::Return(u32::from(self.movie_skip)),
            "sound_mode" => Action::Return(self.sound_mode.unwrap_or(u32::MAX)),
            "sound_mode_change" => {
                let changed = std::mem::take(&mut self.sound_mode_changed);
                Action::Return(self.sound_mode.filter(|_| changed).unwrap_or(u32::MAX))
            }
            "sound_mode_chosen" => {
                // The game's own choice is the host's mode from now on.
                let mode = arg(0);
                if mode <= 2 && self.sound_mode != Some(mode) {
                    self.sound_mode = Some(mode);
                    self.sound_mode_chosen = Some(mode);
                }
                self.sound_mode_changed = false;
                Action::Return(0)
            }
            "pause_reached" => {
                self.focus_pause.reached();
                Action::Return(0)
            }
            "present" => {
                let alternative = [crate::presentation::SPRITE_FACING, crate::presentation::SPRITE_FULL_SIZE].contains(&arg(0));
                if alternative && self.gpu.device().trace_enabled() {
                    match crate::presentation::sprite_facing(memory, arg(1)) {
                        Ok((first, last, facing)) if arg(0) == crate::presentation::SPRITE_FACING => {
                            self.gpu.device_mut().trace_sprite_facing(first, last, facing)
                        }
                        Ok((first, last, full_size)) => self.gpu.device_mut().trace_sprite_full_size(first, last, full_size),
                        Err(error) => { self.trap_reason = Some(error); return Action::Trap; }
                    }
                }
                if self.presentation.enabled {
                    let mut bytes = vec![0; arg(2).min(0x1000) as usize];
                    if memory.read(arg(1), &mut bytes).is_err() {
                        self.trap_reason = Some("presentation record outside memory".into());
                        return Action::Trap;
                    }
                    self.presentation.record(arg(0), bytes);
                }
                Action::Return(0)
            }
            "debug_break" => {
                self.log.push(format!("break {}", arg(0)));
                Action::Return(0)
            }
            "missing" => {
                self.missing.push(arg(0));
                self.trap_reason = Some(format!("called {}, which the port does not define", self.stub_name(arg(0))));
                Action::Trap
            }
            "bad_call" => {
                self.trap_reason = Some(format!(
                    "indirect call to {:#010x} (signature #{}) found no loaded function",
                    arg(0),
                    arg(1)
                ));
                Action::Trap
            }
            other => {
                let now = self.clock.now;
                let mut raised = std::mem::take(&mut self.raised);
                let mut reason = self.trap_reason.take();
                let mut action = None;
                for (prefix, device) in self.devices() {
                    if let Some(op) = other.strip_prefix(prefix) {
                        let mut context = Context { memory, now, raised: &mut raised, trap_reason: &mut reason };
                        action = Some(device.import(op, import.args, &mut context));
                        break;
                    }
                }
                self.raised = raised;
                self.trap_reason = reason;
                action.unwrap_or_else(|| {
                    self.trap_reason = Some(format!("unknown import xem.{other}"));
                    Action::Trap
                })
            }
        }
    }
}

/// What a frame did.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct FrameReport {
    /// The frame stopped early: the disc source is still fetching a sector
    /// (browser hosts). Fetch it and call run_frame again; no time passed.
    pub blocked: bool,
    pub steps: u64,
    pub restarts: Vec<(u32, u32)>,
    pub interrupts: u64,
}

/// What the field says of the player's buttons ([`Runtime::field_controls`]):
/// whether it runs, whether the player walks there (the player control read
/// the pad in the last event pass, with no dialogue open, and no menu
/// runs), whether a menu runs, and whether Select shows the map's picture
/// there (else the hosts' first-person toggle).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct FieldControls {
    pub field: bool,
    pub walking: bool,
    pub menu: bool,
    pub picture: bool,
}

/// A fiber's state between steps: where it starts, or where it suspended.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Fiber {
    /// Not started: it starts on the shadow stack ending at `stack_top`.
    Fresh { stack_top: u32 },
    /// Unwound into the save area `area` with the shadow stack at `stack_pointer`.
    Suspended { stack_pointer: u32, area: u32 },
}

/// The game module with its services.
pub struct Runtime<M: GameModule> {
    pub identity: crate::snapshot::StateIdentity,
    module: M,
    services: *mut Services,
    entry: (u32, u32),
    stack_top: u32,
    /// The game fiber and the arena task fiber (`FIBER_GAME`, `FIBER_TASK`).
    fibers: [Option<Fiber>; 2],
    current: u32,
    /// A clock advance a blocked disc read interrupted: (frame end, limit,
    /// first interrupt only).
    resume_advance: Option<(u64, u64, bool)>,
    /// Set when an advance stopped before the frame's vertical blank, which
    /// the next frame delivers first.
    stopped_before_vblank: bool,
    /// Frames run to completion since boot (a frame a blocked disc read
    /// interrupted counts once it completes).
    frames: u64,
    /// A 32-bit write made when the game next restarts into the dispatcher,
    /// before `xem_run` enters it: (address, value).
    dispatch_write: Option<(u32, u32)>,
    /// The executable's text as the BIOS copied it to its address at
    /// power-on (code and initialized data), which a reset copies again.
    executable: Option<(u32, Vec<u8>)>,
    /// The first trap a host's query of the port met ([`Self::game_paused`],
    /// [`Self::combo_input`], [`Self::field_controls`]): the query answered
    /// as if nothing applied, and status reports it.
    query_trap: Option<String>,
}

impl<M: GameModule> Runtime<M> {
    pub fn new(mut module: M) -> Self {
        let services = Box::into_raw(Box::<Services>::default());
        // SAFETY: the box lives until Drop; the runtime only touches it between
        // export calls.
        unsafe { module.set_import_handler(services as *mut dyn ImportHandler) };
        let stack_top = module.stack_pointer();
        let identity = crate::snapshot::StateIdentity { module: module.content_id(), ..Default::default() };
        Runtime {
            identity,
            module,
            services,
            entry: (RESTART_BOOT, 0),
            stack_top,
            fibers: [Some(Fiber::Fresh { stack_top }), None],
            resume_advance: None,
            current: FIBER_GAME,
            dispatch_write: None,
            stopped_before_vblank: false,
            frames: 0,
            executable: None,
            query_trap: None,
        }
    }

    /// Copy the executable's text to `address` as the BIOS does at power-on
    /// (and keep it for a reset).
    pub fn load_executable(&mut self, address: u32, text: &[u8]) -> Result<(), crate::memory::OutOfBounds> {
        self.memory().write(address, text)?;
        self.executable = Some((address, text.to_vec()));
        Ok(())
    }

    /// Place the Kanji font where Krom2RawAdd finds it: `bios`'s, a BIOS
    /// image `bios::check` accepts (refused, nothing changed, otherwise), or
    /// the substitute glyphs without one (`bios::install_kanji_font`). Hosts
    /// do at every boot and when the player imports a BIOS image; the game
    /// draws from it the next time it draws a title. An image installed
    /// ends the notice that an imported one was not used.
    pub fn install_kanji_font(&mut self, bios: Option<&[u8]>) -> Result<crate::bios::KanjiFont, String> {
        let font = crate::bios::install_kanji_font(self.memory(), bios)?;
        let services = self.services();
        services.kanji_font = Some(font);
        if bios.is_some() {
            services.kanji_font_notice = None;
        }
        Ok(font)
    }

    pub fn services(&mut self) -> &mut Services {
        // SAFETY: no export runs while `self` is borrowed mutably here.
        unsafe { &mut *self.services }
    }

    /// Tells the cards whether the game's file screen shows
    /// (`Card::file_screen`), from the game's memory now: once a frame
    /// ends, a state is restored or the console resets, so changes from
    /// outside (between frames) know it.
    fn note_file_screen(&mut self) {
        let mode = self.services().mode;
        let shown = crate::context::file_screen(self.module.memory(), mode);
        self.services().card.file_screen = shown;
    }

    pub fn module(&mut self) -> &mut M {
        &mut self.module
    }

    pub fn memory(&mut self) -> &mut dyn GameMemory {
        self.module.memory()
    }

    fn host_trap(&mut self, trap: Trap) -> Trap {
        match (trap, self.services().trap_reason.take()) {
            (Trap::Wasm(_), Some(reason)) => Trap::Host(reason),
            (trap, _) => trap,
        }
    }

    /// Run the game until it waits, restarts or traps. A switch between the
    /// game and the arena task fiber continues the other fiber at once.
    pub fn step(&mut self) -> Result<Stop, Trap> {
        loop {
            let fiber = self.current;
            match self.fibers[fiber as usize] {
                Some(Fiber::Suspended { stack_pointer, area }) => {
                    self.module.set_stack_pointer(stack_pointer);
                    self.module.start_rewind(area);
                }
                Some(Fiber::Fresh { stack_top }) => self.module.set_stack_pointer(stack_top),
                None => return Err(Trap::Host(format!("fiber {fiber} has no stack to run on"))),
            }
            let result = if fiber == FIBER_GAME {
                let (kind, arg) = self.entry;
                self.module.run(kind, arg)
            } else {
                self.module.run_task()
            };
            self.sync_devices();
            if let Err(trap) = result {
                return Err(self.host_trap(trap));
            }
            if self.module.async_state() != AsyncState::Unwinding {
                if fiber == FIBER_GAME {
                    self.fibers[0] = Some(Fiber::Fresh { stack_top: self.stack_top });
                    return Ok(Stop::Returned);
                }
                return Err(Trap::Host("the arena task returned from its entry, which the original re-enters".into()));
            }
            self.module.stop_unwind();
            let suspended = Fiber::Suspended { stack_pointer: self.module.stack_pointer(), area: self.module.unwind_area() };
            match self.services().pending.take() {
                Some(Pending::Stop(Stop::Yield(reason))) => {
                    self.fibers[fiber as usize] = Some(suspended);
                    self.module.set_resume_value(0);
                    return Ok(Stop::Yield(reason));
                }
                Some(Pending::Stop(Stop::Restart { kind, arg })) => {
                    if kind == RESTART_DISPATCH
                        && let Some((address, value)) = self.dispatch_write.take()
                    {
                        self.memory()
                            .write(address, &value.to_le_bytes())
                            .map_err(|e| Trap::Host(format!("dispatch write: {e}")))?;
                    }
                    self.fibers = [Some(Fiber::Fresh { stack_top: self.stack_top }), None];
                    self.current = FIBER_GAME;
                    self.entry = (kind, arg);
                    return Ok(Stop::Restart { kind, arg });
                }
                Some(Pending::Switch { fiber: target, stack_top }) => {
                    if target as usize >= self.fibers.len() || target == fiber {
                        return Err(Trap::Host(format!("fiber {fiber} switched to fiber {target}")));
                    }
                    self.fibers[fiber as usize] = Some(suspended);
                    if stack_top != 0 {
                        self.fibers[target as usize] = Some(Fiber::Fresh { stack_top });
                    } else if !matches!(self.fibers[target as usize], Some(Fiber::Suspended { .. })) {
                        return Err(Trap::Host(format!("fiber {fiber} switched to fiber {target}, which is not suspended")));
                    }
                    self.current = target;
                    self.module.set_resume_value(0);
                }
                _ => return Err(Trap::Host("the game unwound without a reason".into())),
            }
        }
    }

    /// Write `value` (32 bits) at `address` when the game next restarts into
    /// the dispatcher, between the call that requested it and the dispatcher
    /// reading its state.
    pub fn write_before_next_dispatch(&mut self, address: u32, value: u32) {
        self.dispatch_write = Some((address, value));
    }

    /// Abandon the game stack and continue with `xem_run(kind, arg)` at the
    /// next step (as the game's own restarts do).
    pub fn restart(&mut self, kind: u32, arg: u32) {
        self.entry = (kind, arg);
        self.fibers = [Some(Fiber::Fresh { stack_top: self.stack_top }), None];
        self.current = 0;
    }

    /// The console's reset: abandon suspended work, load the executable
    /// again and boot it as the BIOS does after the reset line (the port's
    /// BIOS state back at power-on: no event open, so the abandoned sound
    /// driver's tick does not go on beside the new one's), the sound chip,
    /// the drive and the root counters as at power-on, so nothing of the
    /// abandoned game (its initialized data, voices, a stream the drive was
    /// playing) outlives it. Attached discs and memory cards remain attached.
    pub fn reset(&mut self) {
        self.restart(RESTART_POWER_ON, 0);
        self.resume_advance = None;
        self.stopped_before_vblank = false;
        self.dispatch_write = None;
        // SAFETY: no game export runs here; services and module memory are disjoint.
        let services = unsafe { &mut *self.services };
        services.pending = None;
        services.trap_reason = None;
        services.mode = None;
        services.last_wait = None;
        services.focus_pause.set_active(false);
        services.raised.clear();
        services.clock.counters = Default::default();
        services.clock.reconcile_audio_due();
        services.cd.power_on();
        let memory = self.module.memory();
        let loaded = match &self.executable {
            Some((address, text)) => memory.write(*address, text),
            None => Ok(()),
        };
        if let Err(error) = loaded.and_then(|()| services.spu.power_on(memory)) {
            services.trap_reason = Some(error.to_string());
        }
        self.note_file_screen();
    }

    /// Deliver an interrupt while the game is suspended, on the stack below
    /// the suspended frames.
    pub fn interrupt(&mut self, irq: u32, detail: u32) -> Result<(), Trap> {
        let sp = self.module.stack_pointer();
        let result = self.module.interrupt(irq, detail);
        if result.is_ok() && self.module.async_state() == AsyncState::Unwinding {
            // An interrupt handler cannot wait: on the console it runs with
            // interrupts off and never returns to the game's waits.
            self.module.stop_unwind();
            self.module.set_stack_pointer(sp);
            let what = format!("{:?}", self.services().pending.take());
            return Err(Trap::Host(format!("interrupt {irq} handler suspended ({what})")));
        }
        self.module.set_stack_pointer(sp);
        self.sync_devices();
        result.map_err(|trap| self.host_trap(trap))
    }

    fn sync_devices(&mut self) {
        // SAFETY: no export runs here; services and module memory are disjoint.
        let services = unsafe { &mut *self.services };
        services.each_device(self.module.memory(), |device, context| device.sync(context));
    }

    /// Let the devices catch up with the clock (after it moved).
    fn advance_devices(&mut self) {
        // SAFETY: as in sync_devices.
        let services = unsafe { &mut *self.services };
        services.route_cd_audio();
        services.each_device(self.module.memory(), |device, context| device.advance(context));
    }

    /// Shows the Binaural presentation the scene the frame left: the
    /// listener, the positional emitters, whether a movie plays (its decoder
    /// ran within half a second). Reads only.
    pub fn update_binaural(&mut self) {
        // SAFETY: no game export runs here; services and module memory are disjoint.
        let services = unsafe { &mut *self.services };
        let now = services.clock.now;
        let movie = services.mdec.decoded_at().is_some_and(|at| now.saturating_sub(at) < crate::clock::CPU_HZ / 2);
        let mode = services.mode;
        if let Some(presenter) = services.spu.binaural_mut() {
            presenter.update_scene(&*self.module.memory(), mode, movie);
        }
    }

    pub fn set_external_music(&mut self, enabled: bool) {
        let services = self.services();
        if services.clock.external_audio == enabled { return; }
        services.clock.external_audio = enabled;
        services.spu.set_external_audio(enabled);
        services.clock.reconcile_audio_due();
    }

    /// Drive the original music interrupt and SPU from normal elapsed time.
    /// The host calls this once per pacing batch, never per rendered eye/view.
    pub fn advance_music(&mut self, cycles: u64) -> Result<(),Trap> {
        if cycles > crate::clock::CPU_HZ / 10 { return Err(Trap::Host("music advance is bounded to 100 ms".into())); }
        if !self.services().clock.external_audio { return Err(Trap::Host("music clock is paced by game frames".into())); }
        let target = self.services().clock.audio_now + cycles;
        loop {
            let clock = &self.services().clock;
            let tick = clock.audio_running() && clock.audio_due <= target;
            let at = if tick { clock.audio_due.max(clock.audio_now) } else { target };
            // SAFETY: no game export runs while the devices advance.
            let services = unsafe { &mut *self.services };
            let delta = at.saturating_sub(services.clock.audio_now);
            services.spu.advance_audio(delta,&mut Context { memory: self.module.memory(), now: services.clock.now,
                raised: &mut services.raised, trap_reason: &mut services.trap_reason });
            services.clock.audio_now = at;
            if tick { services.clock.audio_due += services.clock.audio_period(); }
            services.clock.reconcile_audio_due();
            let raised = std::mem::take(&mut services.raised);
            for (irq,detail) in raised { self.interrupt(irq,detail)?; }
            if !tick { break; }
            self.interrupt(IRQ_RCNT0+2,0)?;
        }
        Ok(())
    }

    /// Advance the clock to `limit` (or until the first interrupt when
    /// `first_only`), delivering interrupts in time order.
    fn advance(&mut self, limit: u64, first_only: bool, frame_end: u64, report: &mut FrameReport) -> Result<bool, Trap> {
        if self.services().cd.blocked {
            return Ok(false);
        }
        let mut delivered = false;
        loop {
            let raised = std::mem::take(&mut self.services().raised);
            for (irq, detail) in raised {
                self.interrupt(irq, detail)?;
                report.interrupts += 1;
                delivered = true;
            }
            if delivered && first_only {
                return Ok(true);
            }
            // A device event before the clock's next interrupt runs first.
            let device_at = self.services().next_device_event();
            if let Some(at) = device_at.filter(|&at| at <= limit) {
                let services = unsafe { &mut *self.services };
                if at < services.clock.peek().0 {
                    services.clock.now = services.clock.now.max(at);
                    self.advance_devices();
                    let services = unsafe { &mut *self.services };
                    services.run_device_events(self.module.memory());
                    if services.cd.blocked {
                        return Ok(delivered);
                    }
                    continue;
                }
            }
            // A frame ends just before its vertical blank (as an emulator's
            // frame ends at the blank, before the CPU takes the interrupt):
            // what a host does between frames (pad input above all) is in
            // place when the next frame's blank interrupt runs.
            let (at, next) = self.services().clock.peek();
            if next == Tick::VBlank && at <= limit && at >= frame_end {
                self.services().clock.now = at;
                self.advance_devices();
                self.stopped_before_vblank = true;
                return Ok(delivered);
            }
            let Some(tick) = self.services().clock.next_tick(limit) else {
                return Ok(delivered);
            };
            self.advance_devices();
            match tick {
                Tick::VBlank => {
                    let services = self.services();
                    services.presentation.end_frame();
                    services.input_reads_last_frame = std::mem::take(&mut services.input_reads);
                    self.interrupt(IRQ_VBLANK, 0)?
                }
                Tick::RootCounter(n) => self.interrupt(IRQ_RCNT0 + n as u32, 0)?,
            }
            report.interrupts += 1;
            delivered = true;
            if first_only || tick == Tick::VBlank {
                return Ok(true);
            }
        }
    }

    /// Run the game until the clock reaches the next vertical blank: each wait
    /// advances the clock (a frame wait to the blank, a poll to the next
    /// interrupt), delivering interrupts in time order. The frame stops just
    /// before the blank's interrupt; the next frame delivers it first.
    pub fn run_frame(&mut self) -> Result<FrameReport, Trap> {
        let mut report = FrameReport::default();
        // A frame resumes the clock advance the previous one stopped: at its
        // vertical blank, or where a blocked disc read interrupted it.
        self.services().cd.blocked = false;
        let (frame_end, mut pending) = match self.resume_advance.take() {
            Some((frame_end, limit, first_only)) => (frame_end, Some((limit, first_only))),
            None => (self.services().clock.next_vblank(), None),
        };
        loop {
            if pending.is_none() {
                report.steps += 1;
                pending = match self.step()? {
                    Stop::Yield(YieldReason::DrawSync) => {
                        // Drawing ends at once; deliver what it raised.
                        Some((self.services().clock.now, false))
                    }
                    Stop::Yield(_) => Some((frame_end, true)),
                    Stop::Restart { kind, arg } => {
                        report.restarts.push((kind, arg));
                        None
                    }
                    Stop::Returned => return Err(Trap::Host("xem_run returned".into())),
                };
            }
            if let Some((limit, first_only)) = pending.take() {
                self.advance(limit, first_only, frame_end, &mut report)?;
                if std::mem::take(&mut self.stopped_before_vblank) {
                    self.resume_advance = Some((frame_end + VBLANK_CYCLES, limit, first_only));
                    self.frames += 1;
                    self.note_file_screen();
                    return Ok(report);
                }
                if self.services().cd.blocked {
                    self.resume_advance = Some((frame_end, limit, first_only));
                    report.blocked = true;
                    return Ok(report);
                }
            }
            if self.services().clock.now >= frame_end {
                self.frames += 1;
                self.note_file_screen();
                return Ok(report);
            }
        }
    }

    /// Frames run to completion since boot.
    pub fn frames(&self) -> u64 {
        self.frames
    }

    pub fn game_paused(&mut self) -> bool {
        let mode = self.services().mode.unwrap_or(u32::MAX);
        let answer = self.module.game_paused(mode);
        self.answered("xem_game_paused", answer)
    }

    /// Whether the battle's attack inputs take the player's buttons.
    pub fn combo_input(&mut self) -> bool {
        let mode = self.services().mode.unwrap_or(u32::MAX);
        let answer = self.module.combo_input(mode);
        self.answered("xem_game_combo_input", answer)
    }

    /// A query's answer, or after a trap its default, the first trap kept
    /// for status and printed (a fault in the port, never the game's).
    fn answered<T: Default>(&mut self, query: &str, answer: Result<T, Trap>) -> T {
        answer.unwrap_or_else(|trap| {
            if self.query_trap.is_none() {
                let report = format!("{query}: {trap}");
                eprintln!("xem: the port's query {report}; it answers as if nothing applied");
                self.query_trap = Some(report);
            }
            T::default()
        })
    }

    /// The first trap a host's query of the port met, if any.
    pub fn query_trap(&self) -> Option<&str> {
        self.query_trap.as_deref()
    }

    /// The game's button assignment, which the typed actions resolve
    /// through (`xem_core::action`).
    pub fn button_assignment(&mut self) -> crate::action::Assignment {
        let mut bytes = [0; 8];
        match self.memory().read(crate::action::ASSIGNMENT_ADDRESS, &mut bytes) {
            Ok(()) => crate::action::Assignment::from_bytes(bytes),
            Err(_) => crate::action::Assignment::default(),
        }
    }

    /// The interface context the player's input reaches now.
    pub fn context(&mut self) -> crate::context::Context {
        let mode = self.services().mode;
        crate::context::read_context(self.memory(), mode)
    }

    /// The interface's selectable entries on the screen (`crate::menu`).
    pub fn targets(&mut self) -> Option<crate::menu::Targets> {
        let ui = self.context();
        crate::menu::targets(self.memory(), ui)
    }

    /// What the field says of the player's buttons (nothing outside it).
    pub fn field_controls(&mut self) -> FieldControls {
        let mode = self.services().mode.unwrap_or(u32::MAX);
        let bits = self.module.field_controls(mode);
        let bits = self.answered("xem_game_field_controls", bits);
        FieldControls { field: mode == 1, walking: bits & 1 != 0, menu: bits & 4 != 0, picture: bits & 2 != 0 }
    }
}

impl<M: GameModule> Runtime<M> {
    /// Game state at a wait (between steps), independent of app pacing.
    pub fn snapshot(&mut self) -> Result<Vec<u8>, crate::snapshot::SnapshotError> {
        Ok(crate::snapshot::seal(self.snapshot_content()?))
    }

    /// The identical save-state payload for the trusted, in-memory queue.
    /// File integrity hashing belongs at the import/export boundary, outside
    /// the periodic game-frame checkpoint.
    pub(crate) fn snapshot_content(&mut self) -> Result<Vec<u8>, crate::snapshot::SnapshotError> {
        use crate::snapshot::*;
        let mut w = Writer::default();
        write_header(&mut w);
        w.bytes(&serde_json::to_vec(&self.identity).expect("state identity"));
        w.u32(self.entry.0);
        w.u32(self.entry.1);
        w.u32(self.current);
        for fiber in &self.fibers {
            match fiber {
                None => w.u32(0),
                Some(Fiber::Fresh { stack_top }) => {
                    w.u32(1);
                    w.u32(*stack_top);
                }
                Some(Fiber::Suspended { stack_pointer, area }) => {
                    w.u32(2);
                    w.u32(*stack_pointer);
                    w.u32(*area);
                }
            }
        }
        w.u32(self.module.stack_pointer());
        let globals = self.module.globals();
        w.u32(globals.len() as u32);
        for g in globals {
            w.u32(g);
        }
        write_memory(&mut w, self.module.memory())?;
        let services = unsafe { &mut *self.services };
        let mut clock = services.clock.clone();
        clock.set_audio_speed(1);
        write_clock(&mut w, &clock);
        write_pads(&mut w, &services.pads);
        w.u32(services.raised.len() as u32);
        for &(irq, detail) in &services.raised {
            w.u32(irq);
            w.u32(detail);
        }
        w.u64(self.frames);
        w.u32(self.stack_top);
        w.u32(self.stopped_before_vblank as u32);
        match self.dispatch_write {
            None => w.u32(0),
            Some((address, value)) => { w.u32(1); w.u32(address); w.u32(value); }
        }
        w.u64(services.mode.map(u64::from).unwrap_or(u64::MAX));
        w.u64(services.dispatches);
        w.u32(services.input_reads);
        w.u32(services.input_reads_last_frame);
        w.u64(match services.last_wait {
            None => u64::MAX,
            Some(YieldReason::VSync) => 1,
            Some(YieldReason::DrawSync) => 2,
            Some(YieldReason::Poll) => 3,
            Some(YieldReason::Other(n)) => u64::from(n),
        });
        match self.resume_advance {
            None => w.u32(0),
            Some((frame_end, limit, first_only)) => {
                w.u32(1);
                w.u64(frame_end);
                w.u64(limit);
                w.u32(first_only as u32);
            }
        }
        for (_, device) in services.devices() {
            w.bytes(&device.save());
        }
        if w.0.len() + 32 > MAX_STATE { return Err(SnapshotError::Format("state exceeds 64 MiB")); }
        w.0.shrink_to_fit();
        Ok(w.0)
    }

    /// Restore a snapshot taken by `snapshot` from the same game module.
    pub fn restore(&mut self, data: &[u8]) -> Result<(), crate::snapshot::SnapshotError> {
        self.restore_content(crate::snapshot::checked(data)?)
    }

    /// Only runtime-produced rewind payloads bypass the file checksum. All
    /// bounds, identity and device validation below remain failure-atomic.
    pub(crate) fn restore_content(&mut self, data: &[u8]) -> Result<(), crate::snapshot::SnapshotError> {
        use crate::snapshot::*;
        let mut r = Reader::new(data);
        read_header(&mut r)?;
        let identity: StateIdentity = serde_json::from_slice(r.bytes()?).map_err(|e| SnapshotError::Identity(e.to_string()))?;
        if let Some(difference) = self.identity.difference(&identity) {
            return Err(SnapshotError::Incompatible(difference));
        }
        let entry = (r.u32()?, r.u32()?);
        let current = r.u32()?;
        let mut fibers = [None, None];
        for fiber in fibers.iter_mut() {
            *fiber = match r.u32()? {
                0 => None,
                1 => Some(Fiber::Fresh { stack_top: r.u32()? }),
                2 => Some(Fiber::Suspended { stack_pointer: r.u32()?, area: r.u32()? }),
                _ => return Err(SnapshotError::Format("fiber")),
            };
        }
        let stack_pointer = r.u32()?;
        if current > FIBER_TASK || fibers[current as usize].is_none() {
            return Err(SnapshotError::Format("active fiber"));
        }
        let expected_globals = self.module.globals().len();
        let count = r.count(4, expected_globals)?;
        if count != expected_globals {
            return Err(SnapshotError::Format("module globals"));
        }
        let mut globals = Vec::with_capacity(count);
        for _ in 0..count {
            globals.push(r.u32()?);
        }
        let regions = read_memory(&mut r)?;
        let mut clock = read_clock(&mut r)?;
        let pads = read_pads(&mut r)?;
        let raised_count = r.count(8, 65536)?;
        let mut raised = Vec::with_capacity(raised_count);
        for _ in 0..raised_count {
            raised.push((r.u32()?, r.u32()?));
        }
        let frames = r.u64()?;
        let stack_top = r.u32()?;
        let stopped_before_vblank = r.u32()? != 0;
        let dispatch_write = match r.u32()? {
            0 => None,
            1 => Some((r.u32()?, r.u32()?)),
            _ => return Err(SnapshotError::Format("dispatch write")),
        };
        let mode = match r.u64()? {
            u64::MAX => None,
            n => Some(u32::try_from(n).map_err(|_| SnapshotError::Format("mode"))?),
        };
        let dispatches = r.u64()?;
        let input_reads = r.u32()?;
        let input_reads_last_frame = r.u32()?;
        let last_wait = match r.u64()? {
            u64::MAX => None,
            n => Some(YieldReason::from_raw(u32::try_from(n).map_err(|_| SnapshotError::Format("wait"))?)),
        };
        let resume_advance = match r.u32()? {
            0 => None,
            1 => Some((r.u64()?, r.u64()?, r.u32()? != 0)),
            _ => return Err(SnapshotError::Format("resume advance")),
        };
        // Decode into detached devices. A corrupt late device must not change
        // memory, clocks, cards or an earlier device in the live session.
        let mut loaded = Services::default();
        for (_, device) in loaded.devices() {
            device.load(r.bytes()?).map_err(SnapshotError::Device)?;
        }
        let disc = loaded.cd.inserted();
        if !self.services().cd.holds(disc) {
            return Err(SnapshotError::Device(format!("the state has disc {disc} in the drive, which is not here")));
        }
        if loaded.spu.audio_cycles() != clock.audio_now {
            return Err(SnapshotError::Format("inconsistent music clock speed"));
        }
        if !r.done() {
            return Err(SnapshotError::Format("trailing bytes"));
        }
        let memory = self.module.memory();
        let mut previous = Vec::new();
        for (start, len) in REGIONS {
            let mut bytes = vec![0; len as usize];
            memory.read(start, &mut bytes)?;
            previous.push(bytes);
        }
        for (i, ((start, _), bytes)) in REGIONS.into_iter().zip(&regions).enumerate() {
            if let Err(error) = memory.write(start, bytes) {
                for ((address, _), original) in REGIONS.into_iter().zip(&previous).take(i + 1) {
                    memory.write(address, original)?;
                }
                return Err(error.into());
            }
        }
        let services = unsafe { &mut *self.services };
        let speed = services.clock.audio_speed;
        clock.external_audio = services.clock.external_audio;
        services.gpu.restore_state(loaded.gpu);
        services.cd.restore_state(loaded.cd);
        services.mdec = loaded.mdec;
        services.spu.restore_state(loaded.spu);
        services.card.restore_state(loaded.card);
        services.clock = clock;
        services.set_playback_speed(speed);
        services.pads = pads;
        services.raised = raised;
        services.pending = None;
        services.mode = mode;
        services.dispatches = dispatches;
        services.input_reads = input_reads;
        services.input_reads_last_frame = input_reads_last_frame;
        services.last_wait = last_wait;
        services.trap_reason = None;
        let capture = services.presentation.enabled;
        services.presentation = Default::default();
        services.presentation.enabled = capture;
        services.missing.clear();
        services.log.clear();
        self.resume_advance = resume_advance;
        self.frames = frames;
        self.stack_top = stack_top;
        self.stopped_before_vblank = stopped_before_vblank;
        self.dispatch_write = dispatch_write;
        self.entry = entry;
        self.current = current;
        self.fibers = fibers;
        self.module.set_stack_pointer(stack_pointer);
        self.module.set_globals(&globals);
        self.note_file_screen();
        Ok(())
    }
}

impl<M: GameModule> Drop for Runtime<M> {
    fn drop(&mut self) {
        // SAFETY: created by Box::into_raw in `new`, dropped once.
        drop(unsafe { Box::from_raw(self.services) });
    }
}
