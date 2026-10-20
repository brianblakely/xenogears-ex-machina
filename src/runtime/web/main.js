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
  frameError: null,
  hidden: document.hidden,
  deviceLosses: [],
  storageError: null,
  // Why the BIOS image IndexedDB keeps is not used (the check refuses it).
  biosNotice: null,
  operationStatus: null,
  storage: await storage.status(),
  // An immersive session (WebXR) while one runs; its frames replace the
  // canvas's animation frames.
  xr: null,
  xrSupported: false,
  arSupported: false,
  xrSessions: 0,
  xrError: null,
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
// The player's BIOS image beside them (Import BIOS), whose Kanji font every
// boot takes for save titles; the substitute glyphs without one. One the
// check refuses stays stored, is reported on the storage line and boots
// place the substitute glyphs, as natively; storage that cannot be read is
// the cards' error (their store).
try {
  const bios = await storage.get('cards', 'bios');
  if (bios) {
    try { app.set_bios(bios); }
    catch (error) { page.biosNotice = `Imported BIOS image not used: ${error.message ?? error}; save titles use the substitute glyphs.`; }
  }
} catch (error) {
  cards.error ??= String(error);
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
    // Fingers play the touch controls and tap the picture (the panel takes
    // them while it shows).
    if (event.pointerType === 'touch' && kind !== 'wheel' && kind !== 'leave') {
      // Keep a finger's moves even when it slides off the canvas (a touch
      // the browser no longer tracks cannot be captured).
      if (kind === 'down') try { canvas.setPointerCapture(event.pointerId); } catch {}
      if (app.touch(kind, event.pointerId, x, y, performance.now())) {
        event.preventDefault();
        startAudioOnGesture();
        dispatchAppRequests();
        return;
      }
    }
    // Mouse motion turns the field camera (sideways) or first-person
    // inspection's view; the wheel zooms the field camera.
    if (kind === 'down') startAudioOnGesture();
    const [dx, dy] = kind === 'wheel' ? [event.deltaX, event.deltaY] : [event.movementX ?? 0, event.movementY ?? 0];
    if (app.pointer(kind, x, y, event.button ?? 0, dx, dy)) event.preventDefault();
    // With the mouse camera on, the game keeps the mouse (hidden, unbounded
    // motion) while it has the input.
    if (kind === 'down' && event.pointerType !== 'touch' && document.pointerLockElement !== canvas
        && app.mouse_camera_active()) {
      canvas.requestPointerLock?.()?.catch?.(() => {});
    }
    dispatchAppRequests();
  };
  canvas.addEventListener('contextmenu', (event) => event.preventDefault());
  canvas.addEventListener('pointermove', forward('move'));
  canvas.addEventListener('pointerdown', forward('down'));
  canvas.addEventListener('pointerup', forward('up'));
  canvas.addEventListener('pointerleave', forward('leave'));
  canvas.addEventListener('pointercancel', forward('cancel'));
  canvas.addEventListener('wheel', forward('wheel'), { passive: false });
}
wireCanvas();
// Immersive play is submitted through an XR-compatible WebGL2 context
// (WebXR layers on WebGPU are not available to test): a browser that offers
// immersive VR renders with one from the start; others use WebGPU first.
try {
  page.xrSupported = Boolean(navigator.xr) && await navigator.xr.isSessionSupported('immersive-vr');
} catch (error) {
  page.xrError = String(error);
}
// Immersive AR is a headset's, offered beside immersive VR only: a phone's
// handheld AR (immersive AR alone) has no input this page maps (the screen's
// touches), so the phone plays flat as before. A browser whose WebXR knows
// no immersive AR rejects the question: no AR.
page.arSupported = page.xrSupported && await navigator.xr.isSessionSupported('immersive-ar').catch(() => false);
const prefer = params.get('backend') ?? (page.xrSupported ? 'webgl2-xr' : 'auto');
if (!(await startRenderer(prefer)) && prefer !== 'webgl2') await startRenderer('webgl2');
new ResizeObserver(sizeCanvas).observe(document.querySelector('main'));

// --- Host operations (the controls and window.xem call these) ---------------
const host = {
  app,
  cards,
  flushCards: () => cards.flush(),
  appOperation: (request) => appOperation(request),
  fillDisc: () => disc.fillGameReads(app),
  async boot({ executable = true, run = false, rewind = run } = {}) {
    if (!page.game.available) throw new Error(`no game module: ${page.game.error}`);
    const instance = await instantiate(page.game);
    const status = JSON.parse(app.boot(instance, page.game.names, executable, page.game.moduleId, rewind));
    audio.reset();
    app.set_running(run);
    if (cards.notice) operationStatus(cards.notice);
    return status;
  },
  /** Import BIOS: the runtime checks the image, IndexedDB takes it beside
   * the cards, then every boot takes its Kanji font and a running game has
   * it now. Resolves to what it did; a refused file or a failed write
   * changes nothing. */
  async importBios(bytes) {
    try { app.check_bios(bytes); }
    catch (error) { throw new Error(`BIOS not imported: ${error.message ?? error}.`); }
    await storage.put('cards', 'bios', bytes);
    app.set_bios(bytes);
    page.biosNotice = null;
    return 'BIOS imported: save titles use its Kanji font.';
  },
  storedBios: () => storage.get('cards', 'bios'),
  async importDisc(file, name = file.name) {
    try { localStorage.setItem('xem.disc.last', JSON.stringify({ name, bytes: file.size })); }
    catch (error) { page.storageError = String(error); }
    return disc.importDisc(app, file.name === undefined ? new File([file], name) : file);
  },
  readSectors(lba, count) {
    return disc.readSectors(app, lba, count);
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
      biosNotice: page.biosNotice,
      operationStatus: page.operationStatus,
      gamepad: gamepadName,
      fileSystemAccess: disc.fileSystemAccess,
      hidden: page.hidden,
      crossOriginIsolated: window.crossOriginIsolated,
      xr: {
        supported: page.xrSupported,
        arSupported: page.arSupported,
        capable: app.xr_capable(),
        active: Boolean(page.xr),
        // The running session's mode and environment blend mode.
        mode: page.xr?.mode ?? null,
        blend: page.xr?.blend ?? null,
        sessions: page.xrSessions,
        frames: page.xr?.frames ?? 0,
        views: page.xr?.views ?? 0,
        viewer: page.xr?.viewer ?? null,
        visibility: page.xr?.session.visibilityState ?? null,
        selects: page.xr?.selects ?? 0,
        error: page.xrError,
      },
    };
  },
  enterVr: () => enterXr('immersive-vr'),
  enterAr: () => enterXr('immersive-ar'),
  exitVr: () => page.xr?.session.end(),
  /** RGBA bytes of the immersive layer at [x, y] points (from the bottom left). */
  xrReadback: (points) => page.xr && new Promise((resolve) => { page.xr.readbackRequest = { points, resolve }; }),
};
window.xem = createAutomation(host);

// --- Input: the keyboard and the first gamepad to pad 1 (the runtime maps them) --
let gamepadName = null;
function pollGamepad() {
  const pad = navigator.getGamepads?.().find((p) => p?.connected);
  gamepadName = pad?.id ?? null;
  const pressed = Uint8Array.from(pad?.buttons ?? [], (b) => (b.pressed ? 1 : 0));
  app.gamepad(pressed, Float32Array.from(pad?.axes ?? []));
  releasePointerForGame();
  dispatchAppRequests();
}
// The browser takes Escape to release the mouse: that release opens the
// settings panel, as Escape does otherwise.
let pointerReleasedAt = -Infinity;
let releasingPointer = false;
document.addEventListener('pointerlockchange', () => {
  const captured = document.pointerLockElement === canvas;
  app.set_pointer_captured(captured);
  if (captured) { releasingPointer = false; return; }
  // A game menu or pause releases the mouse without opening Settings.
  if (releasingPointer) { releasingPointer = false; return; }
  if (!document.hasFocus() || !app.mouse_camera_active()) return;
  pointerReleasedAt = performance.now();
  app.set_panel_visible(true);
});
function releasePointerForGame() {
  if (!app.mouse_camera_active() && document.pointerLockElement) {
    releasingPointer = true;
    app.set_pointer_captured(false);
    document.exitPointerLock();
  }
}
window.addEventListener('blur', () => app.release_input());
window.addEventListener('focus', () => app.resume_input());

// --- Audio starts with the player's first gesture (a click, a key, a touch):
// browsers allow an AudioContext to run only from one. "Start audio" stays
// for the explicit start; a context the browser suspends resumes the same way.
function startAudioOnGesture() {
  const state = audio.status().state;
  if (state === 'running') return;
  audio.start().catch((error) => { page.audioError = String(error.message ?? error); });
}

// --- Fullscreen: a button, never a key the browser reserves. Entering or
// leaving it lets go of held input (the browser may have taken keys).
const fullscreenButton = $('fullscreen');
fullscreenButton.hidden = !document.documentElement.requestFullscreen;
fullscreenButton.addEventListener('click', () => {
  if (document.fullscreenElement) document.exitFullscreen?.();
  else document.documentElement.requestFullscreen?.().catch((error) => { page.fullscreenError = String(error.message ?? error); });
});
document.addEventListener('fullscreenchange', () => {
  page.fullscreen = Boolean(document.fullscreenElement);
  fullscreenButton.textContent = page.fullscreen ? 'Exit fullscreen' : 'Fullscreen';
  app.release_input();
  if (document.hasFocus()) app.resume_input();
});

// --- Immersive play (WebXR): a flat virtual screen in front of the player --
// The game runs from the session's animation frames exactly as from the
// canvas's (the frames due by the clock, never per view); every view draws
// the same game picture as a screen, or the diorama. Controllers stand in
// for a pad (xr-standard mapping): left stick the D-pad, A/B/X/Y
// Cross/Circle/Square/Triangle, triggers L1/R1, squeeze L2/R2, stick clicks
// Start/Select. Immersive AR (where the browser offers it) is the same play
// over the real room: a session whose blend mode is not opaque gets a layer
// with alpha, and the frames leave the room around what they draw
// transparent.
async function enterXr(mode) {
  if (page.xr || !navigator.xr) return;
  // VR and AR ask for the same optional features: nothing AR-specific
  // (hit tests, planes, anchors, depth) is asked for, let alone required.
  const session = await navigator.xr.requestSession(mode, { optionalFeatures: ['local-floor', 'hand-tracking'] });
  page.xrSessions += 1;
  const gl = canvas.getContext('webgl2');
  const blend = session.environmentBlendMode ?? 'opaque';
  // ?xrScale=F asks for a smaller (or larger) layer than the headset's default.
  const framebufferScaleFactor = Number(params.get('xrScale') ?? 1);
  const layer = new XRWebGLLayer(session, gl, { antialias: false, depth: false, stencil: false, alpha: blend !== 'opaque', framebufferScaleFactor });
  session.updateRenderState({ baseLayer: layer });
  const space = await session.requestReferenceSpace('local');
  // The runtime recentred the space (the system's recentring): everything
  // stands in front of its new origin again, as OpenXR's hosts do it.
  space.addEventListener('reset', () => app.xr_reference_reset());
  app.xr_session(blend);
  page.xr = { session, layer, space, mode, blend, frames: 0, views: 0, selecting: [false, false], selected: [false, false],
    pinching: [false, false], pinched: [false, false] };
  (mode === 'immersive-ar' ? arButton : vrButton).textContent = mode === 'immersive-ar' ? 'Exit AR' : 'Exit VR';
  // A controller's select (trigger) presses the settings panel's pointer
  // where it points; a hand's select is WebXR's standard pinch. Each holds
  // from selectstart to selectend (one that starts and ends within a frame
  // still counts), so either held on Hold to rewind rewinds. A hidden
  // session (the runtime's own menu) lets go of everything held.
  const handOf = (source) => (source?.handedness === 'left' ? 0 : 1);
  session.addEventListener('selectstart', (event) => {
    if (!page.xr) return;
    const i = handOf(event.inputSource);
    if (event.inputSource?.hand) { page.xr.pinching[i] = true; page.xr.pinched[i] = true; }
    else { page.xr.selecting[i] = true; page.xr.selected[i] = true; page.xr.selects = (page.xr.selects ?? 0) + 1; }
  });
  session.addEventListener('selectend', (event) => {
    if (!page.xr) return;
    const i = handOf(event.inputSource);
    if (event.inputSource?.hand) page.xr.pinching[i] = false; else page.xr.selecting[i] = false;
  });
  session.addEventListener('visibilitychange', () => {
    if (session.visibilityState !== 'visible') app.release_input();
    else if (document.hasFocus()) app.resume_input();
  });
  session.addEventListener('end', () => {
    page.xr = null;
    vrButton.textContent = 'Enter VR';
    arButton.textContent = 'Enter AR';
    app.xr_ended();
    app.release_input();
    if (document.hasFocus()) app.resume_input();
    requestAnimationFrame(frame);
  });
  session.requestAnimationFrame(xrFrame);
}

// The controllers as a standard gamepad (the layout the game bindings name):
// A South, B East, X West, Y North, triggers L1/R1, grips LT/RT, the right
// stick's click Back, the left's Start, and both sticks.
function xrGamepad(session) {
  const pressed = new Uint8Array(17);
  const axes = new Float32Array(4);
  for (const source of session.inputSources) {
    const pad = source.gamepad;
    if (!pad || pad.mapping !== 'xr-standard') continue;
    const button = (i) => (pad.buttons[i]?.pressed ? 1 : 0);
    const right = source.handedness === 'right';
    pressed[right ? 0 : 2] |= button(4);
    pressed[right ? 1 : 3] |= button(5);
    pressed[right ? 5 : 4] |= button(0);
    pressed[right ? 7 : 6] |= button(1);
    pressed[right ? 8 : 9] |= button(3);
    axes[right ? 2 : 0] = pad.axes[2] ?? 0;
    axes[right ? 3 : 1] = pad.axes[3] ?? 0;
  }
  return { pressed, axes };
}

// A headset frame's input for the runtime's gestures (XemApp.xr_input): the
// viewer's pose and visibility, then each hand's target ray, its 25 joints
// and its standard pinch (1 a hand source, 1 pinching).
const XR_HAND = 8 + 1 + 75 + 2;
const xrData = new Float32Array(8 + 2 * XR_HAND);
function xrInput(xrframe, viewer, session, space) {
  xrData.fill(0);
  const put = (at, pose) => {
    const p = pose.transform.position, o = pose.transform.orientation;
    xrData.set([p.x, p.y, p.z, o.x, o.y, o.z, o.w], at);
  };
  put(0, viewer);
  xrData[7] = session.visibilityState === 'visible' ? 1 : 0;
  for (const source of session.inputSources) {
    const hand = source.handedness === 'left' ? 0 : source.handedness === 'right' ? 1 : -1;
    if (hand < 0) continue;
    const base = 8 + hand * XR_HAND;
    const ray = xrframe.getPose(source.targetRaySpace, space);
    if (ray) { xrData[base] = 1; put(base + 1, ray); }
    if (source.hand) {
      let tracked = true;
      let j = 0;
      for (const joint of source.hand.values()) {
        const pose = xrframe.getJointPose?.(joint, space);
        if (!pose) { tracked = false; break; }
        const p = pose.transform.position;
        xrData.set([p.x, p.y, p.z], base + 9 + j * 3);
        j += 1;
      }
      xrData[base + 8] = tracked && j === 25 ? 1 : 0;
      xrData[base + 84] = 1;
      xrData[base + 85] = page.xr.pinching[hand] || page.xr.pinched[hand] ? 1 : 0;
      page.xr.pinched[hand] = false;
    }
  }
  return xrData;
}

function xrFrame(time, xrframe) {
  const xr = page.xr;
  if (!xr) return;
  xr.session.requestAnimationFrame(xrFrame);
  const pose = xrframe.getViewerPose(xr.space);
  try {
    if (pose) {
      const matrices = new Float32Array(pose.views.length * 32);
      const viewports = new Int32Array(pose.views.length * 4);
      pose.views.forEach((view, i) => {
        matrices.set(view.transform.inverse.matrix, i * 32);
        matrices.set(view.projectionMatrix, i * 32 + 16);
        const v = xr.layer.getViewport(view);
        viewports.set([v.x, v.y, v.width, v.height], i * 4);
      });
      const { pressed, axes } = xrGamepad(xr.session);
      app.gamepad(pressed, axes);
      // Each controller's select held this frame (or begun and ended since
      // the last): a bit per hand, 1 the left's and 2 the right's.
      const selects = [0, 1].reduce((bits, i) => bits | (xr.selecting[i] || xr.selected[i] ? 1 << i : 0), 0);
      xr.selected = [false, false];
      app.xr_input(time, xrInput(xrframe, pose, xr.session, xr.space), selects);
      app.xr_frame(time, xr.layer, matrices, viewports);
      // A readback asked for reads the layer just drawn, inside the frame
      // (an emulated layer may be the canvas's default framebuffer).
      if (xr.readbackRequest) {
        const { points, resolve } = xr.readbackRequest;
        xr.readbackRequest = null;
        resolve(Array.from(app.xr_readback(xr.layer, Int32Array.from(points.flat()))));
      }
      xr.views = pose.views.length;
      const p = pose.transform.position;
      xr.viewer = [p.x, p.y, p.z];
    }
    disc.serveGameReads(app);
    audio.pump();
    // What the session asked for (the panel's operations, the controllers'
    // chords) runs now, and card writes go to IndexedDB, as after the
    // window's frames.
    dispatchAppRequests();
    cards.flush().catch(() => {}); // retained for explicit retry; visible in status
    xr.frames += 1;
    page.frameError = null;
  } catch (error) {
    page.frameError = String(error.message ?? error);
  }
}

// --- Animation: the game frames due (bounded) and one render per frame -----
let recovering = false;
function frame(time) {
  // An immersive session schedules its own frames.
  if (page.xr) return;
  try {
    pollGamepad();
    app.frame(time);
    disc.serveGameReads(app);
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
const vrButton = $('vr');
const arButton = $('ar');
// Enter AR shows only where the headset's browser offers immersive AR.
vrButton.hidden = !(page.xrSupported && app.xr_capable());
arButton.hidden = !(page.arSupported && app.xr_capable());
for (const [button, mode] of [[vrButton, 'immersive-vr'], [arButton, 'immersive-ar']]) {
  button.addEventListener('click', () => {
    if (page.xr) page.xr.session.end();
    else enterXr(mode).catch((error) => { page.xrError = String(error.message ?? error); });
  });
}
function show(message) {
  $('message').textContent = message;
}

/** Import `file` (its File System Access `handle`, when it has one, is
 * remembered for the disc it turns out to be). */
async function runImport(file, handle = null) {
  show(`Reading ${file.name} locally…`);
  const status = await host.importDisc(file);
  const id = status.identity;
  if (status.state === 'failed') show(`${file.name}: ${status.error}`);
  else if (id?.disc) {
    if (handle) await disc.remember(id.disc, handle);
    const running = JSON.parse(app.status()).session;
    show(`Xenogears disc ${id.disc} (${id.serial}), ${id.boot_path}${running ? ': ready for the game\'s disc changes' : ''}`);
  } else show(`${file.name}: not a known Xenogears disc (boot program ${id?.boot_path}, SHA-256 ${id?.sha256})`);
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
$('file').accept = disc.IMAGE_TYPES.join(',');
$('pick').hidden = !disc.fileSystemAccess;
$('pick').addEventListener('click', guarded(async () => {
  const { file, handle } = await disc.pickWithHandle();
  await runImport(file, handle);
}));
const remembered = (await disc.rememberedNames()).join(' and ');
let last = null;
try { last = JSON.parse(localStorage.getItem('xem.disc.last') ?? 'null'); }
catch (error) { page.storageError = String(error); }
$('reopen').hidden = !remembered;
$('reopen').textContent = remembered ? `Reopen ${remembered}` : 'Reopen';
$('reopen').addEventListener('click', guarded(async () => {
  for (const file of await disc.reopenRemembered()) await runImport(file);
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
/** The largest file a memory card or save container is (a DexDrive card). */
const MAX_CARD_FILE = 0xf40 + 0x20000;
/** An operation's outcome (status().page.operationStatus): what it did, the
 * panel's error bar cleared, or what failed, in the error bar. */
function operationStatus(message) { page.operationStatus = message; app.set_operation_error(''); }
function operationError(message) { page.operationStatus = message; app.set_operation_error(message); }
const operationGuard = (action) => async (event) => {
  try { await action(event); }
  catch (error) { operationError(String(error.message ?? error)); }
};
$('import-state').addEventListener('change', operationGuard(async (event) => {
  const file = event.target.files[0]; if (!file) return;
  try {
    if (file.size > 64 * 1024 * 1024) throw new Error('State file exceeds 64 MiB.');
    app.restore_bytes(new Uint8Array(await file.arrayBuffer()));
    app.state_notice(false, undefined);
    event.target.value = ''; operationStatus('State restored.');
  } catch (error) {
    app.state_notice(false, String(error.message ?? error));
    throw error;
  }
}));
// Any card or save file, by its contents; its message says what was found.
$('import-card').addEventListener('change', operationGuard(async (event) => {
  const file = event.target.files[0]; if (!file) return;
  if (file.size > MAX_CARD_FILE) throw new Error(`${file.name} (${file.size} bytes) is larger than any memory card or save file.`);
  const found = await cards.importFile(importCardSlot, new Uint8Array(await file.arrayBuffer()), file.name);
  event.target.value = ''; operationStatus(found);
}));
$('import-settings').addEventListener('change', operationGuard(async (event) => {
  const file = event.target.files[0]; if (!file) return;
  if (file.size > 1024 * 1024) throw new Error('Settings file exceeds 1 MiB.');
  app.import_settings(await file.text()); event.target.value = ''; operationStatus('Settings imported and saved.');
}));
/** The largest file read as a BIOS image (xem_core::bios::MAX_FILE): the
 * runtime then says what a file of another size is. */
const MAX_BIOS_FILE = 16 * 1024 * 1024;
$('import-bios').addEventListener('change', operationGuard(async (event) => {
  const file = event.target.files[0]; if (!file) return;
  if (file.size > MAX_BIOS_FILE) throw new Error(`BIOS not imported: ${file.name} (${file.size} bytes) is no PS1 BIOS image (512 KiB).`);
  const message = await host.importBios(new Uint8Array(await file.arrayBuffer()));
  event.target.value = ''; operationStatus(message);
}));

/** The backups' pickers and downloads, which an immersive session cannot
 * show: they are refused there, as in native headset play. */
const FILE_ACTIONS = ['ExportState', 'ImportState', 'ExportCard1', 'ImportCard1', 'ExportCard2', 'ImportCard2',
  'ExportSettings', 'ImportSettings', 'ImportBios'];
const XR_PICKERS = 'File pickers are available in windowed play.';
// The panel's Import disc… cannot show the file chooser in a session either.
$('file').addEventListener('click', (event) => {
  if (!page.xr) return;
  event.preventDefault();
  operationError(XR_PICKERS);
});

function control(command) {
  const reply = window.xem.command(command);
  if (reply.error) throw new Error(reply.error);
  return reply;
}
function chooseBackup(id, message = 'Choose a backup file.') {
  const input = $(id);
  input.value = '';
  input.showPicker();
  operationStatus(message);
}
async function appOperation(request) {
  try { return await executeAppOperation(request); }
  catch (error) {
    if (['SaveState', 'LoadState', 'ExportState', 'ImportState'].includes(request.Action)) {
      app.state_notice(['SaveState', 'ExportState'].includes(request.Action), String(error.message ?? error));
    }
    throw error;
  }
}
async function executeAppOperation(request) {
  if ('Debug' in request) return control({ cmd: 'debug', request: request.Debug });
  if ('Rewind' in request || 'UiRewind' in request) {
    const panel = 'UiRewind' in request;
    const held = panel ? request.UiRewind : request.Rewind;
    app.set_rewind_held(held,panel);
    operationStatus(held ? 'Rewinding while held.' : 'Rewind released.');
    return { ok: true };
  }
  const action = request.Action;
  if (page.xr && FILE_ACTIONS.includes(action)) throw new Error(XR_PICKERS);
  switch (action) {
    case 'Reset': {
      const reply = control({ cmd: 'reset' });
      app.set_running(true);
      operationStatus('Console reset.'); return reply;
    }
    case 'SaveState': {
      const saved = await host.save('slot0');
      app.state_notice(true, undefined);
      operationStatus(`Saved state (${saved.bytes} bytes); browser write completed.`); return saved;
    }
    case 'LoadState': {
      const loaded = await host.load('slot0');
      if (!loaded) throw new Error(page.storageError);
      app.restore_bytes(loaded.data);
      app.state_notice(false, undefined);
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
    case 'ToggleAutoAdvance': {
      const auto = window.xem.settings.get().settings.auto_advance;
      const reply = window.xem.settings.apply({ AutoAdvance: { ...auto, enabled: !auto.enabled } });
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus(auto.enabled ? 'Auto-advance dialogue off.' : 'Auto-advance dialogue on: choices still wait for you.');
      return reply;
    }
    case 'IncreaseFastForwardRate': case 'DecreaseFastForwardRate': {
      const reply = JSON.parse(app.step_fast_forward_rate(action === 'IncreaseFastForwardRate'));
      if (!reply.ok) throw new Error(reply.reason);
      operationStatus('Fast-forward rate saved.'); return reply;
    }
    case 'ExportState':
      download('xenogears.xemstate', app.snapshot_bytes()); app.state_notice(true, undefined); operationStatus('State backup exported.'); break;
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
    case 'ImportBios': chooseBackup('import-bios', 'Choose your PS1 BIOS image.'); break;
    case 'RetryStorage': await cards.flush({ retry: true }); operationStatus('Pending card writes completed.'); break;
    case 'RequestPersistence': {
      const granted = await storage.requestPersistence(); page.storage = await storage.status();
      if (granted) operationStatus('Browser persistence granted. Keep exported backups.');
      else operationError('Browser persistence was not granted. Export backups.');
      return { granted };
    }
    // The headset's view in front of the head again (in a session).
    case 'Recenter': operationStatus(app.xr_recenter()); break;
    // The player's own (keys, chords, the deck's LOOK) stay in the runtime
    // for its next frame; this is the automation's appOperation.
    case 'ToggleFirstPerson': {
      const reply = control({ cmd: 'first_person' });
      operationStatus(reply.first_person ? 'First-person view on.' : 'First-person view off.');
      return reply;
    }
    default: throw new Error(`Unknown app operation ${action}`);
  }
  return { ok: true };
}
function dispatchAppRequests() {
  for (const request of JSON.parse(app.take_app_requests())) {
    appOperation(request).catch((error) => operationError(String(error.message ?? error)));
  }
}

// Settings stay reachable while playing: Escape toggles the panel.
const slintKeys = { Tab: '\t', Enter: '\n', Backspace: '\u0008', ArrowLeft: '', ArrowRight: '',
                    ArrowUp: '', ArrowDown: '', Home: '', End: '', Escape: '\u001b', ' ': ' ' };
for (const type of ['keydown', 'keyup']) {
  window.addEventListener(type, (event) => {
    if (event.target instanceof HTMLInputElement) return;
    const down = type === 'keydown';
    if (down) startAudioOnGesture();
    if (down && event.key === 'Escape') {
      // Unless this Escape released the mouse and opened the panel already.
      if (performance.now() - pointerReleasedAt > 250) app.set_panel_visible(!app.panel_visible());
      releasePointerForGame();
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
  app.set_storage_status(`${page.storageError ?? cards.status().error ?? JSON.parse(app.settings()).error ?? page.biosNotice ?? cards.status().notice ?? page.storage.notice ?? page.storage.error ?? ''}\n${cards.status().pending} card writes pending. Ordinary saves use the game save menu.`);
}, 250);
document.body.dataset.ready = 'true';
