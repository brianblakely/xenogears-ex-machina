// The automation API, window.xem: the runtime's typed commands for browser
// clients and test drivers. Game commands are the shared control layer's
// (xem_core::control, the one `xem-headless --control` serves), so a client
// drives the browser and the native host with the same JSON; each call goes
// straight to the runtime method the page's controls use. Nothing here
// synthesizes DOM input: pads are set with the shared `pad`, `press` and
// `replay` commands.
//
// The disc is read in place, asynchronously: a `frames` or `run_until`
// command whose frame waits for a sector not yet fetched returns early with
// {blocked: true, frames} (no virtual time passed for the waiting frame).
// `run` and `runFrames` fetch what is missing and continue, so they answer
// what xem-headless --control answers for the same command.

/** The most frames one command runs (XemApp::command's bound). */
const MAX_FRAMES_PER_COMMAND = 10_000;
/** Blocked replies in a row without a frame of progress before giving up. */
const MAX_STALLS = 1000;

export function createAutomation(host) {
  const { app } = host;
  const parse = (text) => JSON.parse(text);
  const command = (cmd) => parse(app.command(JSON.stringify(cmd)));

  /** A `frames` or `run_until` command run to completion across disc fetches. */
  async function run(cmd) {
    const total = cmd.cmd === 'frames' ? (cmd.count ?? 1) : cmd.max_frames;
    if (!['frames', 'run_until'].includes(cmd.cmd)) return command(cmd);
    if (total > MAX_FRAMES_PER_COMMAND) return command(cmd); // the runtime's error
    let done = 0;
    let stalls = 0;
    for (;;) {
      const left = total - done;
      const reply = command(cmd.cmd === 'frames' ? { ...cmd, count: left } : { ...cmd, max_frames: left });
      if (!reply.blocked) {
        await host.flushCards();
        if (cmd.cmd === 'frames' || reply.error) return reply;
        return { ...reply, frames: done + reply.frames };
      }
      done += reply.frames;
      stalls = reply.frames === 0 ? stalls + 1 : 0;
      if (stalls > MAX_STALLS) throw new Error('the disc read does not complete');
      await host.fillDisc();
    }
  }

  return Object.freeze({
    version: 4,
    /** Renderer, disc, session, loop, audio, storage and page state. */
    status: () => ({ ...parse(app.status()), page: host.pageStatus() }),
    /** Start a session on a fresh game module instance: `{executable: false}`
     * boots a module without a disc (test fixtures), `{run: true}` lets the
     * animation loop advance it. Rewind follows Settings when `run: true`;
     * command-driven boots opt in with `rewind: true`. */
    boot: (options = {}) => host.boot(options),
    setRunning: (running) => app.set_running(Boolean(running)),
    /** Any control command, e.g. {cmd: 'inspect', path: 'mode_next_mode'}; errors come back as {error}. */
    command,
    /** First-person inspection, as the native hosts' `first_person` command:
     * `{on, yaw, pitch}`, each optional (on absent toggles it as Toggle
     * first-person view does, or turns it on where a look is given; yaw
     * degrees right of the heading it began with, pitch up), anywhere in a
     * field but its menu under Ex Machina. Returns {first_person, look} (in
     * degrees) or {error}; the session takes it with the page's next batch
     * of frames. */
    firstPerson: (options = {}) => command({ cmd: 'first_person', ...options }),
    /** The typed Settings/bindings operation, e.g. {Action:'SaveState'}. */
    appOperation: (request) => host.appOperation(request),
    /** `frames` or `run_until` to completion, fetching the disc sectors frames wait
     * for (resolves to the reply xem-headless --control gives); other commands as `command`. */
    run,
    /** Advance `count` frames however many that takes (commands of at most 10 000). */
    async runFrames(count) {
      let reply = null;
      for (let left = count; left > 0; left -= MAX_FRAMES_PER_COMMAND) {
        reply = await run({ cmd: 'frames', count: Math.min(left, MAX_FRAMES_PER_COMMAND) });
        if (reply.error) break;
      }
      return reply ?? command({ cmd: 'status' });
    },
    /** One `frames` command (each frame ends at a vertical blank of the virtual
     * clock); with a disc it may come back {blocked: true}: use runFrames. */
    step: (count = 1) => command({ cmd: 'frames', count }),
    /** Frames until `until` ({vblanks} or {path, equals | not_equals}), at most maxFrames. */
    runUntil: ({ maxFrames, until }) => command({ cmd: 'run_until', max_frames: maxFrames, until }),
    memoryHash: () => command({ cmd: 'memory_hash' }),
    /** Named snapshots kept in the session. */
    snapshot: (name = 'default') => command({ cmd: 'snapshot', name }),
    restore: (name = 'default') => command({ cmd: 'restore', name }),
    rewind: (steps = 1) => command({ cmd: 'rewind', steps }),
    configureRewind: (maxBytes = 2 * 1024 * 1024 * 1024, intervalSeconds = 1, historySeconds = 30) => command({ cmd: 'rewind_configure', max_bytes: maxBytes, interval_seconds: intervalSeconds, history_seconds: historySeconds }),
    fastForward: (enabled) => command({ cmd: 'fast_forward', ...(enabled === undefined ? {} : { enabled }) }),
    /** Snapshot bytes (Uint8Array) for files and saves, and back. */
    exportSnapshot: () => app.snapshot_bytes(),
    importSnapshot: (bytes) => app.restore_bytes(bytes),
    settings: Object.freeze({
      get: () => parse(app.settings()),
      /** A SettingChange, e.g. {MasterVolume: 40} or {Scale: 'Integer'}. */
      apply: (change) => parse(app.apply_setting(JSON.stringify(change))),
      export: () => app.export_settings(),
      import: (text) => parse(app.import_settings(text)),
    }),
    /** Immersive play (WebXR): enter needs a user activation (the page's VR
     * or AR button). */
    xr: Object.freeze({
      enter: () => host.enterVr(),
      enterAr: () => host.enterAr(),
      exit: () => host.exitVr(),
      /** RGBA bytes of the immersive layer at [[x, y], ...] (from the bottom left). */
      readback: (points) => host.xrReadback(points),
    }),
    panel: Object.freeze({
      show: (visible) => app.set_panel_visible(Boolean(visible)),
      visible: () => app.panel_visible(),
    }),
    /** Import a disc image from a File or Blob the client already holds. */
    importDisc: (blob, name) => host.importDisc(blob, name),
    readSectors: (lba, count = 1) => host.readSectors(lba, count),
    /** Save the session snapshot to IndexedDB, and read one back. */
    save: (slot = 'slot0') => host.save(slot),
    load: (slot = 'slot0') => host.load(slot),
    cards: Object.freeze({
      /** The image the card device holds in slot 0 or 1 (a session's). */
      image: (slot) => app.card_image(slot),
      /** The image IndexedDB holds for slot 0 or 1. */
      stored: (slot) => host.cards.stored(slot),
      /** Persist and insert an image (Uint8Array, a .mcd) in slot 0 or 1. */
      store: (slot, image) => host.cards.store(slot, image),
      /** Import a card or save file's bytes into slot 0 or 1 as Import
       * original card 1/2 does (`name` names a raw single save); resolves to
       * what was found. */
      importFile: (slot, bytes, name) => host.cards.importFile(slot, bytes, name),
      /** Resolves when the card device's writes so far are in IndexedDB. */
      flush: () => host.flushCards(),
      retry: () => host.cards.flush({ retry: true }),
    }),
    /** The player's BIOS image, whose Kanji font save titles are drawn from. */
    bios: Object.freeze({
      /** Import a BIOS image's bytes (Uint8Array) as Import BIOS does;
       * resolves to what it did, rejects naming why not. */
      import: (bytes) => host.importBios(bytes),
      /** The image IndexedDB holds, or undefined. */
      stored: () => host.storedBios(),
    }),
  });
}
