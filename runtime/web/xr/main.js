// The WebXR test page: flat canvas, Enter/Exit VR, pause and a debug overlay.
// `window.xemXr` exposes the application state and controls to automation.
//
// `?emulate=quest3` installs IWER (Meta's Immersive Web Emulation Runtime,
// from runtime/web/node_modules) as navigator.xr before the app starts;
// `?emulate=quest3-inline` installs a device without immersive-vr.

import init, { XemXr } from './pkg/xem_webxr.js';

const $ = (id) => document.getElementById(id);
const overlay = $('overlay');
const enter = $('enter');
const exit = $('exit');
const pause = $('pause');

async function installEmulator(kind) {
  const IWER = await import('../node_modules/iwer/build/iwer.module.js');
  const configs = {
    quest3: IWER.metaQuest3,
    'quest3-inline': { ...IWER.metaQuest3, name: 'Meta Quest 3 (inline only)', supportedSessionModes: ['inline'] },
  };
  const config = configs[kind];
  if (!config) throw new Error(`unknown emulated device ${kind}`);
  const device = new IWER.XRDevice(config, { stereoEnabled: true });
  // Chromium exposes a native navigator.xr (without a runtime); replace it.
  device.installRuntime({ forceInstall: true });
  return { IWER, device };
}

function describe(state) {
  const s = state.support;
  const lines = [
    `status: ${state.status}${state.paused ? ' (paused)' : ''}`,
    `navigator.xr: ${s.navigatorXr}  immersive-vr: ${s.immersiveVr}${s.immersiveVrError ? ` (${s.immersiveVrError})` : ''}`,
    `secure context: ${s.secureContext}  WebGPU: ${s.webgpu}  XRGPUBinding: ${s.xrGpuBinding}  submission: ${s.submission}`,
    `adapter: ${s.adapter}`,
    `scene time: ${state.sceneTime.toFixed(3)} s  flat frames: ${state.flatFrames}`,
  ];
  if (state.session) {
    const x = state.session;
    lines.push(`session: ${x.mode}  features: ${x.grantedFeatures ? x.grantedFeatures.join(', ') : 'unreported'}`);
    lines.push(`space: ${x.referenceSpace}  visibility: ${x.visibilityState}  blend: ${x.environmentBlendMode}`);
  }
  const f = state.frame;
  if (f && state.session) {
    lines.push(`xr frame ${f.count}: ${f.framebuffer} framebuffer ${f.layerSize.join('x')}, ${f.views.length} views, tracked ${f.tracked}`);
    if (f.viewer) lines.push(`head: ${f.viewer.position.map((v) => v.toFixed(3)).join(', ')}`);
    for (const v of f.views) lines.push(`  ${v.eye}: viewport ${v.viewport.join(',')} eye ${v.eyePosition.map((n) => n.toFixed(3)).join(', ')}`);
    if (f.renderError) lines.push(`render error: ${f.renderError}`);
  }
  for (const source of state.input) {
    const buttons = source.gamepad ? source.gamepad.buttons.map((b) => b.value.toFixed(2)).join(' ') : '-';
    const hand = source.hand ? `${source.hand.length} joints` : 'no hand';
    lines.push(`input ${source.handedness}/${source.targetRayMode}: ${source.profiles[0] ?? ''} buttons [${buttons}] ${hand}`);
  }
  if (state.lastError) lines.push(`error: ${state.lastError}`);
  return lines.join('\n');
}

async function main() {
  const emulate = new URLSearchParams(location.search).get('emulate');
  const emulator = emulate ? await installEmulator(emulate) : null;
  await init();
  const app = await XemXr.start($('scene'));
  const state = () => JSON.parse(app.stateJson());

  enter.addEventListener('click', () => {
    try {
      app.enterVr();
    } catch (error) {
      overlay.textContent = `${error}`;
    }
  });
  exit.addEventListener('click', () => app.exitVr());
  pause.addEventListener('click', () => app.setPaused(!state().paused));
  window.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && state().session) app.exitVr();
    if (event.key === 'p') app.setPaused(!state().paused);
  });

  const refresh = () => {
    const current = state();
    const s = current.support;
    enter.disabled = !app.canEnterVr();
    enter.textContent = !s.navigatorXr ? 'WebXR unavailable'
      : s.immersiveVr ? 'Enter VR' : 'VR not supported';
    exit.disabled = !current.session;
    pause.textContent = current.paused ? 'Resume' : 'Pause';
    document.body.classList.toggle('immersive', current.status === 'immersive');
    overlay.textContent = describe(current);
  };
  refresh();
  setInterval(refresh, 200);

  window.xemXr = {
    ready: true,
    state,
    enterVr: () => app.enterVr(),
    exitVr: () => app.exitVr(),
    setPaused: (paused) => app.setPaused(paused),
    requestReadback: (points) => app.requestReadback(Int32Array.from(points.flat())),
    emulator,
  };
}

main().catch((error) => {
  overlay.textContent = `failed to start: ${error}`;
  window.xemXr = { ready: true, error: `${error}` };
});
