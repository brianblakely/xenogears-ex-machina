// The automation API, window.xem: the runtime's typed commands for browser
// clients and test drivers. Each call goes straight to the same runtime method
// the page's controls use (XemApp in runtime/crates/xem-web); nothing here
// synthesizes DOM input.

export function createAutomation(host) {
  const { app } = host;
  const parse = (text) => JSON.parse(text);
  return Object.freeze({
    version: 1,
    /** Renderer, disc, session, loop, audio, storage and page state. */
    status: () => ({ ...parse(app.status()), page: host.pageStatus() }),
    /** Start a session on a fresh game module instance: `{executable: false}`
     * boots a module without a disc (test fixtures), `{run: true}` lets the
     * animation loop advance it. */
    boot: (options = {}) => host.boot(options),
    setRunning: (running) => app.set_running(Boolean(running)),
    step: (count = 1) => parse(app.step(count)),
    runUntil: ({ maxSteps, condition }) => parse(app.run_until(JSON.stringify({ maxSteps, condition }))),
    digest: () => app.digest(),
    /** Snapshot file bytes (Uint8Array) of the session between steps. */
    snapshot: () => app.snapshot(),
    restore: (bytes) => parse(app.restore(bytes)),
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
