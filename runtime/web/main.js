// The browser host page: thin glue between browser APIs and the runtime
// (runtime/crates/xem-web). The page owns the canvas, input, Web Audio,
// asynchronous file and storage access and the animation callback; the runtime
// owns the game, settings and rendering.

import init, { XemApp, create_renderer } from './pkg/xem_web.js';
import { loadGameModule, instantiate } from './game.js';
import * as disc from './disc.js';
import * as storage from './storage.js';
import { AudioOut } from './audio.js';
import { createAutomation } from './xem.js';

const params = new URLSearchParams(location.search);
const $ = (id) => document.getElementById(id);

await init();
const app = new XemApp();
const audio = new AudioOut(app);
const page = {
  game: { available: false },
  stubs: false,
  schema: false,
  rendererError: null,
  discFile: null,
  frameError: null,
  hidden: document.hidden,
  deviceLosses: [],
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
  async boot({ executable = true, run = false } = {}) {
    if (!page.game.available) throw new Error(`no game module: ${page.game.error}`);
    const instance = await instantiate(page.game);
    const status = JSON.parse(app.boot(instance, page.game.names, executable));
    app.set_running(run);
    return status;
  },
  async importDisc(file, name = file.name) {
    page.discFile = file;
    localStorage.setItem('xem.disc.last', JSON.stringify({ name, bytes: file.size }));
    return disc.importDisc(app, file.name === undefined ? new File([file], name) : file);
  },
  readSectors(lba, count) {
    if (!page.discFile) throw new Error('no disc is open');
    return disc.readSectors(app, page.discFile, lba, count);
  },
  async save(slot) {
    let bytes;
    let kind = 'snapshot';
    try {
      bytes = app.snapshot_bytes();
    } catch {
      bytes = app.blank_memory_card();
      kind = 'memory-card';
    }
    const sha256 = await storage.sha256(bytes);
    await storage.put('saves', slot, { kind, bytes, sha256, savedAt: Date.now() });
    return { slot, kind, bytes: bytes.length, sha256 };
  },
  async load(slot) {
    const record = await storage.get('saves', slot);
    if (!record) return null;
    const sha256 = await storage.sha256(record.bytes);
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
      fileSystemAccess: disc.fileSystemAccess,
      hidden: page.hidden,
      crossOriginIsolated: window.crossOriginIsolated,
    };
  },
};
window.xem = createAutomation(host);

// --- Animation: the game frames due (bounded) and one render per frame -----
let recovering = false;
const fps = { since: 0, frames: 0, value: 0 };
function frame(time) {
  fps.frames++;
  if (time - fps.since >= 1000) {
    fps.value = (fps.frames * 1000) / (time - fps.since);
    fps.since = time;
    fps.frames = 0;
  }
  try {
    app.frame(time);
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
const last = JSON.parse(localStorage.getItem('xem.disc.last') ?? 'null');
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
$('step').addEventListener('click', () => window.xem.step(1));
$('settings').addEventListener('click', () => app.set_panel_visible(!app.panel_visible()));
$('audio').addEventListener('click', guarded(async () => {
  await audio.start();
  show(`Audio ${audio.status().state} at ${audio.status().sampleRate} Hz`);
}));
$('save').addEventListener('click', guarded(async () => {
  const saved = await host.save('slot0');
  show(`Saved ${saved.kind} (${saved.bytes} bytes, SHA-256 ${saved.sha256.slice(0, 16)}…)`);
}));
$('load').addEventListener('click', guarded(async () => {
  const loaded = await host.load('slot0');
  if (!loaded) return show('No save in slot0');
  if (loaded.kind === 'snapshot' && JSON.parse(app.status()).session) app.restore_bytes(loaded.data);
  show(`Loaded ${loaded.kind} (${loaded.bytes} bytes, verified ${loaded.verified})`);
}));

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
    if (text !== null && app.panel_visible()) {
      app.key(text, down);
      event.preventDefault();
    }
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
    `audio      ${audio.status().state}, ${audio.status().played} frames played`,
    JSON.parse(app.settings()).settings.presentation.show_fps ? `fps        ${fps.value.toFixed(0)}` : null,
  ];
  return lines.filter((line) => line !== null).join('\n');
}
setInterval(() => {
  $('status').textContent = describe(JSON.parse(app.status()));
}, 250);
document.body.dataset.ready = 'true';
