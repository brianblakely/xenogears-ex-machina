// The automation API, window.xem: the runtime's typed commands for browser
// clients and test drivers. Game commands are the shared control layer's
// (xem_core::control, the one `xem-headless --control` serves), so a client
// drives the browser and the native host with the same JSON; each call goes
// straight to the runtime method the page's controls use. Nothing here
// synthesizes DOM input.

export function createAutomation(host) {
  const { app } = host;
  const parse = (text) => JSON.parse(text);
  const command = (cmd) => parse(app.command(JSON.stringify(cmd)));
  return Object.freeze({
    version: 2,
    /** Renderer, disc, session, loop, audio, storage and page state. */
    status: () => ({ ...parse(app.status()), page: host.pageStatus() }),
    /** Start a session on a fresh game module instance: `{executable: false}`
     * boots a module without a disc (test fixtures), `{run: true}` lets the
     * animation loop advance it. */
    boot: (options = {}) => host.boot(options),
    setRunning: (running) => app.set_running(Boolean(running)),
    /** Any control command, e.g. {cmd: 'inspect', path: 'mode_next_mode'}; errors come back as {error}. */
    command,
    /** Advance `count` frames (each ends at a vertical blank of the virtual clock). */
    step: (count = 1) => command({ cmd: 'frames', count }),
    /** Frames until `until` ({vblanks} or {path, equals | not_equals}), at most maxFrames. */
    runUntil: ({ maxFrames, until }) => command({ cmd: 'run_until', max_frames: maxFrames, until }),
    memoryHash: () => command({ cmd: 'memory_hash' }),
    /** Named snapshots kept in the session. */
    snapshot: (name = 'default') => command({ cmd: 'snapshot', name }),
    restore: (name = 'default') => command({ cmd: 'restore', name }),
    /** Snapshot bytes (Uint8Array) for files and saves, and back. */
    exportSnapshot: () => app.snapshot_bytes(),
    importSnapshot: (bytes) => app.restore_bytes(bytes),
    settings: Object.freeze({
      get: () => parse(app.settings()),
      /** A SettingChange, e.g. {MasterVolume: 40} or {Scale: 'Integer'}. */
      apply: (change) => parse(app.apply_setting(JSON.stringify(change))),
    }),
    panel: Object.freeze({
      show: (visible) => app.set_panel_visible(Boolean(visible)),
      visible: () => app.panel_visible(),
    }),
    /** Import a disc image from a File or Blob the client already holds. */
    importDisc: (blob, name) => host.importDisc(blob, name),
    readSectors: (lba, count = 1) => host.readSectors(lba, count),
    /** Save the session snapshot (or, without a session, a blank memory card) to IndexedDB. */
    save: (slot = 'slot0') => host.save(slot),
    load: (slot = 'slot0') => host.load(slot),
  });
}
