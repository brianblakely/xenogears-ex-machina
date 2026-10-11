// The browser host page: thin glue between browser APIs and the runtime
// (src/runtime/crates/xem-web). The page owns the canvas, input events, Web
// Audio, asynchronous file and storage access and the animation callback; the
// runtime owns the game, settings, rendering, the pad mapping and the mix.

import init, { XemApp, create_renderer } from './pkg/xem_web.js';
import { loadGameModule, instantiate } from './game.js';
import * as disc from './disc.js';
import * as storage from './storage.js';
import { AudioOut } from './audio.js';
import { Cards } from './cards.js';
import { createAutomation } from './xem.js';

const params = new URLSearchParams(location.search);
const $ = (id) => document.getElementById(id);

await init();
const app = new XemApp();
const audio = new AudioOut(app);
const cards = new Cards(app);
const page = {
  game: { available: false },
  stubs: false,
  schema: false,
  rendererError: null,
  discFile: null,
  frameError: null,
  hidden: document.hidden,
  deviceLosses: [],
  storageError: null,
  operationStatus: null,
  storage: await storage.status(),
};

// --- Game module (optional: the page runs without one) ----------------------
const gameUrl = params.get('game') ?? 'game/game.wasm';
page.game = await loadGameModule(gameUrl);
if (page.game.available) {
  // Its build reports, next to it: stub names for readable stops, the schema for inspect.
  const beside = (name) => new URL(name, new URL(gameUrl, location.href));
  const stubs = await fetch(params.get('stubs') ?? beside('stubs.txt'));
  if (stubs.ok) {
    app.set_stub_names(await stubs.text());
    page.stubs = true;
  }
  const schema = await fetch(params.get('schema') ?? beside('schema.json'));
  if (schema.ok) {
    app.set_schema(await schema.text());
    page.schema = true;
  }
}

// The memory cards, before any session boots.
try {
  await cards.load();
} catch (error) {
  cards.error = String(error);
}

// --- Canvas renderer: WebGPU, else WebGL2 -----------------------------------
let canvas = $('screen');
function sizeCanvas() {
  const scale = window.devicePixelRatio || 1;
  const width = Math.max(1, Math.round(canvas.clientWidth * scale));
  const height = Math.max(1, Math.round(canvas.clientHeight * scale));
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
  }
  app.resize(width, height, scale);
}

async function startRenderer(prefer, freshCanvas = false) {
  if (freshCanvas) replaceCanvas();
  sizeCanvas();
  try {
    app.attach_renderer(await create_renderer(canvas, prefer));
    return true;
  } catch (error) {
    page.rendererError = `${prefer}: ${error.message ?? error}`;
    console.warn(`xem: renderer (${prefer}) failed:`, error);
    // A canvas keeps the first context type it was given: start over on a new one.
    replaceCanvas();
    return false;
  }
}

function replaceCanvas() {
  const fresh = canvas.cloneNode(false);
  canvas.replaceWith(fresh);
  canvas = fresh;
  wireCanvas();
}

function wireCanvas() {
  const position = (event) => {
    const scale = window.devicePixelRatio || 1;
    return [event.offsetX * scale, event.offsetY * scale];
  };
  const forward = (kind) => (event) => {
    const [x, y] = position(event);
    if (app.pointer(kind, x, y, event.button ?? 0, event.deltaX ?? 0, event.deltaY ?? 0)) event.preventDefault();
    dispatchAppRequests();
  };
  canvas.addEventListener('pointermove', forward('move'));
  canvas.addEventListener('pointerdown', forward('down'));
  canvas.addEventListener('pointerup', forward('up'));
  canvas.addEventListener('pointerleave', forward('leave'));
  canvas.addEventListener('wheel', forward('wheel'), { passive: false });
}
wireCanvas();
const prefer = params.get('backend') ?? 'auto';
if (!(await startRenderer(prefer)) && prefer !== 'webgl2') await startRenderer('webgl2');
new ResizeObserver(sizeCanvas).observe(document.querySelector('main'));

// --- Host operations (the controls and window.xem call these) ---------------
const host = {
  app,
  cards,
  flushCards: () => cards.flush(),
  appOperation: (request) => appOperation(request),
  fillDisc: () => disc.fillGameReads(app, page.discFile),
  async boot({ executable = true, run = false, rewind = run } = {}) {
    if (!page.game.available) throw new Error(`no game module: ${page.game.error}`);
    const instance = await instantiate(page.game);
    const status = JSON.parse(app.boot(instance, page.game.names, executable, page.game.moduleId, rewind));
    audio.reset();
    app.set_running(run);
    if (cards.notice) operationStatus(cards.notice);
    return status;
  },
  async importDisc(file, name = file.name) {
    page.discFile = file;
    try { localStorage.setItem('xem.disc.last', JSON.stringify({ name, bytes: file.size })); }
    catch (error) { page.storageError = String(error); }
    return disc.importDisc(app, file.name === undefined ? new File([file], name) : file);
  },
  readSectors(lba, count) {
    if (!page.discFile) throw new Error('no disc is open');
    return disc.readSectors(app, page.discFile, lba, count);
  },
  async save(slot) {
    const bytes = app.snapshot_bytes();
    const kind = 'snapshot';
    const sha256 = await storage.sha256(bytes);
    try {
      await storage.put('saves', slot, { kind, bytes, sha256, savedAt: Date.now() });
      page.storageError = null;
    } catch (error) { page.storageError = String(error); throw error; }
    return { slot, kind, bytes: bytes.length, sha256 };
  },
  async load(slot) {
    const record = await storage.get('saves', slot);
    if (!record) { page.storageError = 'No stored state in this slot. It may have been cleared or evicted; import a backup.'; return null; }
    if (record.kind !== 'snapshot' || !(record.bytes instanceof Uint8Array) || record.bytes.length > 64 * 1024 * 1024) {
      page.storageError = 'Invalid stored state. Import a backup.';
      throw new Error(page.storageError);
    }
    const sha256 = await storage.sha256(record.bytes);
    if (sha256 !== record.sha256) {
      page.storageError = 'Stored state checksum failed. Import a backup.';
      throw new Error(page.storageError);
    }
    return { slot, kind: record.kind, bytes: record.bytes.length, sha256, storedSha256: record.sha256,
             verified: sha256 === record.sha256, savedAt: record.savedAt, data: record.bytes };
  },
  pageStatus() {
    return {
      game: page.game.available
        ? { available: true, url: page.game.url, imports: page.game.names, exports: page.game.exports,
            stubs: page.stubs, schema: page.schema }
        : { available: false, url: page.game.url, error: page.game.error },
      rendererError: page.rendererError,
      frameError: page.frameError,
      deviceLosses: page.deviceLosses,
      audio: audio.status(),
      cards: cards.status(),
      storage: { ...page.storage, error: page.storageError },
      operationStatus: page.operationStatus,
      gamepad: gamepadName,
      fileSystemAccess: disc.fileSystemAccess,
      hidden: page.hidden,
      crossOriginIsolated: window.crossOriginIsolated,
    };
  },
};
window.xem = createAutomation(host);

// --- Input: the keyboard and the first gamepad to pad 1 (the runtime maps them) --
let gamepadName = null;
let gamepadHome = false;
function pollGamepad() {
  const pad = navigator.getGamepads?.().find((p) => p?.connected);
  gamepadName = pad?.id ?? null;
  const pressed = Uint8Array.from(pad?.buttons ?? [], (b) => (b.pressed ? 1 : 0));
  // Its Home button toggles the settings panel.
  const home = pressed[16] === 1;
  if (home && !gamepadHome) app.set_panel_visible(!app.panel_visible());
  gamepadHome = home;
  app.gamepad(pressed);
  dispatchAppRequests();
}
window.addEventListener('blur', () => app.release_input());
window.addEventListener('focus', () => app.resume_input());

// --- Animation: the game frames due (bounded) and one render per frame -----
let recovering = false;
function frame(time) {
  try {
    pollGamepad();
    app.frame(time);
    disc.serveGameReads(app, page.discFile);
    audio.pump();
    cards.flush().catch(() => {}); // retained for explicit retry; visible in status
    page.frameError = null;
  } catch (error) {
    page.frameError = String(error.message ?? error);
  }
  const lost = app.renderer_lost();
  if (lost && !recovering) {
    // The browser took the GPU device away (driver reset, GPU process
    // restart): render on a new device and canvas; the game is unaffected.
    // A browser that keeps losing WebGPU devices gets WebGL2.
    recovering = true;
    page.deviceLosses.push(lost);
    const backend = page.deviceLosses.length > 2 ? 'webgl2' : JSON.parse(app.status()).renderer.backend;
    startRenderer(backend, true).finally(() => {
      recovering = false;
    });
  }
  requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
document.addEventListener('visibilitychange', () => {
  page.hidden = document.hidden;
  if (document.hidden) app.release_input();
  else if (document.hasFocus()) app.resume_input();
});

// --- Controls ----------------------------------------------------------------
function show(message) {
  $('message').textContent = message;
}

async function runImport(file) {
  show(`Reading ${file.name} locally…`);
  const status = await host.importDisc(file);
  const id = status.identity;
  if (status.state === 'failed') show(`${file.name}: ${status.error}`);
  else if (id?.disc) show(`Xenogears disc ${id.disc} (${id.serial}), ${id.boot_path}`);
  else show(`${file.name}: not a known Xenogears disc (boot program ${id?.boot_path}, SHA-256 ${id?.sha256})`);
}

const guarded = (action) => async (event) => {
  try {
    await action(event);
  } catch (error) {
    show(String(error.message ?? error));
  }
};

$('file').addEventListener('change', guarded(async (event) => {
  const [file] = event.target.files;
  if (file) await runImport(file);
}));
$('pick').hidden = !disc.fileSystemAccess;
$('pick').addEventListener('click', guarded(async () => runImport(await disc.pickWithHandle())));
const remembered = await disc.rememberedName();
let last = null;
try { last = JSON.parse(localStorage.getItem('xem.disc.last') ?? 'null'); }
catch (error) { page.storageError = String(error); }
$('reopen').hidden = !remembered;
$('reopen').textContent = remembered ? `Reopen ${remembered}` : 'Reopen';
$('reopen').addEventListener('click', guarded(async () => {
  const file = await disc.reopenRemembered();
  if (file) await runImport(file);
}));
if (!remembered && last) show(`Last disc: ${last.name}. Choose it again to continue (this browser cannot keep file handles).`);
$('boot').disabled = !page.game.available;
$('boot').addEventListener('click', guarded(async () => {
  await host.boot({ run: true });
  show('Booted; the game runs a frame of its clock per 1/60 s, in bounded batches.');
}));
$('run').addEventListener('click', () => {
  const status = JSON.parse(app.status());
  app.set_running(!status.running);
});
$('step').addEventListener('click', guarded(() => window.xem.runFrames(1)));
$('settings').addEventListener('click', () => app.set_panel_visible(!app.panel_visible()));
$('audio').addEventListener('click', guarded(async () => {
  await audio.start();
  show(`Audio ${audio.status().state} at ${audio.status().sampleRate} Hz`);
}));

function download(name, bytes, type = 'application/octet-stream') {
  const url = URL.createObjectURL(new Blob([bytes], { type }));
  const link = document.createElement('a');
  link.href = url; link.download = name; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
let importCardSlot = 0;
function operationStatus(message) { page.operationStatus = message; app.set_operation_status(message); }
const operationGuard = (action) => async (event) => {
  try { await action(event); }
  catch (error) { operationStatus(String(error.message ?? error)); }
};
$('import-state').addEventListener('change', operationGuard(async (event) => {
  const file = event.target.files[0]; if (!file) return;
  if (file.size > 64 * 1024 * 1024) throw new Error('State file exceeds 64 MiB.');
  app.restore_bytes(new Uint8Array(await file.arrayBuffer()));
  event.target.value = ''; operationStatus('State restored.');
}));
$('import-card').addEventListener('change', operationGuard(async (event) => {
  const file = event.target.files[0]; if (!file) return;
  if (file.size !== 0x20000) throw new Error('Card must be exactly 128 KiB (.mcd).');
  await cards.store(importCardSlot, new Uint8Array(await file.arrayBuffer()));
  event.target.value = ''; operationStatus('Card imported, saved and inserted.');
}));
$('import-settings').addEventListener('change', operationGuard(async (event) => {
  const file = event.target.files[0]; if (!file) return;
  if (file.size > 1024 * 1024) throw new Error('Settings file exceeds 1 MiB.');
  app.import_settings(await file.text()); event.target.value = ''; operationStatus('Settings imported and saved.');
}));

function control(command) {
  const reply = window.xem.command(command);
  if (reply.error) throw new Error(reply.error);
  return reply;
}
function chooseBackup(id) {
  const input = $(id);
  input.value = '';
  input.showPicker();
  operationStatus('Choose a backup file.');
}
async function appOperation(request) {
  if ('Rewind' in request || 'UiRewind' in request) {
    const panel = 'UiRewind' in request;
    const held = panel ? request.UiRewind : request.Rewind;
    app.set_rewind_held(held,panel);
    operationStatus(held ? 'Rewinding while held.' : 'Rewind released.');
    return { ok: true };
  }
  const action = request.Action;
  switch (action) {
    case 'SaveState': {
      const saved = await host.save('slot0');
      operationStatus(`Saved state (${saved.bytes} bytes); browser write completed.`); return saved;
    }
    case 'LoadState': {
      const loaded = await host.load('slot0');
      if (!loaded) throw new Error(page.storageError);
      app.restore_bytes(loaded.data);
      operationStatus('Stored state verified and restored.'); return { ok: true };
    }
    case 'Rewind': throw new Error('Rewind requires a held input.');
    case 'TogglePauseOnFocusLoss': {
      const enabled = window.xem.settings.get().settings.pause_on_focus_loss;
      const reply = window.xem.settings.apply({ PauseOnFocusLoss: !enabled });
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus('Focus pause preference saved.'); return reply;
    }
    case 'ToggleRewind': {
      const enabled = window.xem.settings.get().settings.rewind_enabled;
      const reply = window.xem.settings.apply({ RewindEnabled: !enabled });
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus(enabled ? 'Rewind history disabled.' : 'Rewind history enabled.');
      return reply;
    }
    case 'ToggleTriggerInversion': {
      const invert = window.xem.settings.get().settings.invert_trigger_chords;
      const reply = window.xem.settings.apply({ InvertTriggerChords: !invert });
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus('LT/RT chord preference saved.'); return reply;
    }
    case 'ToggleFastForward': {
      const reply = control({ cmd: 'fast_forward' });
      operationStatus(reply.fast_forward ? 'Fast-forward on. Music keeps its normal tempo and pitch.' : 'Fast-forward off.');
      return reply;
    }
    case 'IncreaseRewindInterval': case 'DecreaseRewindInterval': {
      const reply = JSON.parse(app.step_rewind_interval(action === 'IncreaseRewindInterval'));
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus('Rewind interval saved.'); return reply;
    }
    case 'IncreaseRewindHistory': case 'DecreaseRewindHistory': {
      const reply = JSON.parse(app.step_rewind_history(action === 'IncreaseRewindHistory'));
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus('Rewind history length saved.'); return reply;
    }
    case 'ToggleFps': {
      const reply = window.xem.settings.apply({ ShowFps: !window.xem.settings.get().settings.presentation.show_fps });
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus('FPS display preference saved.'); return reply;
    }
    case 'IncreaseFastForwardRate': case 'DecreaseFastForwardRate': {
      const reply = JSON.parse(app.step_fast_forward_rate(action === 'IncreaseFastForwardRate'));
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus('Fast-forward rate saved.'); return reply;
    }
    case 'ExportState':
      download('xenogears.xemstate', app.snapshot_bytes()); operationStatus('State backup exported.'); break;
    case 'ImportState': chooseBackup('import-state'); break;
    case 'ExportCard1': case 'ExportCard2': {
      const slot = action === 'ExportCard1' ? 0 : 1;
      const image = app.card_image(slot) ?? await cards.stored(slot);
      if (!image) throw new Error('No card image to export.');
      download(`card${slot + 1}.mcd`, image); operationStatus('Original raw memory card exported.'); break;
    }
    case 'ImportCard1': case 'ImportCard2':
      importCardSlot = action === 'ImportCard1' ? 0 : 1;
      chooseBackup('import-card'); break;
    case 'ExportSettings':
      download('xenogears-settings.json', app.export_settings(), 'application/json'); operationStatus('Settings backup exported.'); break;
    case 'ImportSettings': chooseBackup('import-settings'); break;
    case 'RetryStorage': await cards.flush({ retry: true }); operationStatus('Pending card writes completed.'); break;
    case 'RequestPersistence': {
      const granted = await storage.requestPersistence(); page.storage = await storage.status();
      operationStatus(granted ? 'Browser persistence granted. Keep exported backups.' : 'Browser persistence was not granted. Export backups.');
      return { granted };
    }
    default: throw new Error(`Unknown app operation ${action}`);
  }
  return { ok: true };
}
function dispatchAppRequests() {
  for (const request of JSON.parse(app.take_app_requests())) {
    appOperation(request).catch((error) => operationStatus(String(error.message ?? error)));
  }
}

// Settings stay reachable while playing: F1 toggles the panel, Escape closes it.
const slintKeys = { Tab: '\t', Enter: '\n', Backspace: '\u0008', ArrowLeft: '', ArrowRight: '',
                    ArrowUp: '', ArrowDown: '', Home: '', End: '', Escape: '\u001b', ' ': ' ' };
for (const type of ['keydown', 'keyup']) {
  window.addEventListener(type, (event) => {
    if (event.target instanceof HTMLInputElement) return;
    const down = type === 'keydown';
    if (down && (event.key === 'F1' || (event.key === 'Escape' && app.panel_visible()))) {
      app.set_panel_visible(event.key === 'F1' && !app.panel_visible());
      event.preventDefault();
      return;
    }
    const text = slintKeys[event.key] ?? (event.key.length === 1 ? event.key : null);
    const panel = app.panel_visible();
    // A game key is released even while the panel has the input.
    const game = app.game_key(event.code, down);
    if (text !== null && panel) {
      app.key(text, down);
      event.preventDefault();
    } else if (game) {
      event.preventDefault();
    }
    dispatchAppRequests();
  });
}

// --- Status --------------------------------------------------------------------
function describe(status) {
  const r = status.renderer;
  const d = status.disc;
  const s = status.session;
  const lines = [
    `renderer   ${r ? `${r.backend} — ${r.adapter} — ${r.size.join('×')}` : `none (${page.rendererError})`}`,
    `panel      ${r ? (r.panel === 'slint' ? `Slint${r.panelVisible ? ' (shown)' : ''}` : `unavailable: ${r.panelError}`) : '—'}`,
    `game       ${page.game.available ? `${page.game.url} (${page.game.names.length} imports)` : `unavailable: ${page.game.error}`}`,
    `disc       ${d ? `${d.name}: ${d.state}${d.identity ? ` — ${d.identity.serial ?? 'unknown disc'} ${d.identity.boot_path}` : ''}${d.error ? ` — ${d.error}` : ''}` : 'none'}`,
    d ? `disc cache ${(d.peak_resident_bytes / 1048576).toFixed(1)} of ${(d.chunk_budget / 1048576).toFixed(0)} MiB peak, ${d.fetches} reads` : null,
    `session    ${s ? `${s.vblanks} vblanks, ${s.cycles} cycles${s.stopped ? ` — stopped: ${s.stopped}` : status.running ? ' — running' : ''}` : 'none'}`,
    `audio      ${audio.status().state}, ${audio.status().played} frames played, ${audio.status().underruns} silent`,
    `cards      ${status.cards.unsaved || cards.status().pending ? 'saving…' : 'saved'}${cards.status().error ? ` — ${cards.status().error}` : ''}`,
    gamepadName ? `gamepad    ${gamepadName}` : null,
  ];
  return lines.filter((line) => line !== null).join('\n');
}
setInterval(() => {
  $('status').textContent = describe(JSON.parse(app.status()));
  app.set_storage_status(`${page.storageError ?? cards.status().error ?? JSON.parse(app.settings()).error ?? cards.status().notice ?? page.storage.notice ?? page.storage.error ?? ''}\n${cards.status().pending} card writes pending. Ordinary saves use the game save menu.`);
}, 250);
document.body.dataset.ready = 'true';
