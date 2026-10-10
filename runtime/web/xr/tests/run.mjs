// WebXR adapter tests in desktop Chromium. IWER emulates the headset,
// controllers and hands; Playwright clicks the page's controls.
//
//   node runtime/web/xr/tests/run.mjs [scenario...]
//
// Needs XEM_CHROMIUM (nix develop path:./nix/runtime) and a built package
// (runtime/web/xr/build.sh). Screenshots go to $XEM_XR_OUT or build/xr-test.

import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  GL_TO_WGPU_DEPTH, check, decodePng, isFloor, isSky, launch, mean, mul, near, openPage,
  serve, state, until,
} from './harness.mjs';

const here = dirname(fileURLToPath(import.meta.url));
const webRoot = resolve(here, '../..');
const out = process.env.XEM_XR_OUT ?? resolve(here, '../../../../build/xr-test');

// ---------------------------------------------------------------- page hooks

/** Records whether each requestSession call ran with transient user activation. */
function recordActivation() {
  const xr = navigator.xr;
  const request = xr.requestSession.bind(xr);
  window.__activation = [];
  xr.requestSession = (...args) => {
    window.__activation.push(navigator.userActivation.isActive);
    return request(...args);
  };
}

/**
 * Makes IWER behave like a headset browser's compositor in two respects it
 * does not emulate: the layer gets an opaque framebuffer of its own (IWER
 * hands out the canvas's default framebuffer, `null`), which is cleared at
 * the start of each frame and shown (blitted to the canvas) after it; and the
 * eyes get asymmetric, mirrored frusta like a real HMD's (IWER's are
 * symmetric).
 */
function installHeadsetCompositor() {
  const DEG = Math.PI / 180;
  const FOV = { left: [52, 41, 41, 51], right: [41, 52, 41, 51] }; // left, right, up, down
  const frustum = ([l, r, u, d], near = 0.1, far = 1000) => {
    const L = -Math.tan(l * DEG) * near;
    const R = Math.tan(r * DEG) * near;
    const U = Math.tan(u * DEG) * near;
    const D = -Math.tan(d * DEG) * near;
    const m = new Float32Array(16);
    m[0] = (2 * near) / (R - L);
    m[5] = (2 * near) / (U - D);
    m[8] = (R + L) / (R - L);
    m[9] = (U + D) / (U - D);
    m[10] = -(far + near) / (far - near);
    m[11] = -1;
    m[14] = (-2 * far * near) / (far - near);
    return m;
  };
  const projections = { left: frustum(FOV.left), right: frustum(FOV.right) };
  Object.defineProperty(window.XRView.prototype, 'projectionMatrix', {
    configurable: true,
    get() {
      return projections[this.eye] ?? frustum([45, 45, 45, 45]);
    },
  });

  const targets = new WeakMap();
  const opaque = (layer) => {
    const gl = layer.context;
    const width = gl.drawingBufferWidth;
    const height = gl.drawingBufferHeight;
    let target = targets.get(layer);
    if (!target || target.width !== width || target.height !== height) {
      const framebuffer = gl.createFramebuffer();
      const color = gl.createRenderbuffer();
      gl.bindRenderbuffer(gl.RENDERBUFFER, color);
      gl.renderbufferStorage(gl.RENDERBUFFER, gl.RGBA8, width, height);
      gl.bindFramebuffer(gl.FRAMEBUFFER, framebuffer);
      gl.framebufferRenderbuffer(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.RENDERBUFFER, color);
      gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      gl.bindRenderbuffer(gl.RENDERBUFFER, null);
      target = { framebuffer, width, height };
      targets.set(layer, target);
    }
    return target;
  };
  Object.defineProperty(window.XRWebGLLayer.prototype, 'framebuffer', {
    configurable: true,
    get() {
      return opaque(this).framebuffer;
    },
  });
  window.__compositedFrames = 0;
  const requestAnimationFrame = window.XRSession.prototype.requestAnimationFrame;
  window.XRSession.prototype.requestAnimationFrame = function (callback) {
    return requestAnimationFrame.call(this, (time, frame) => {
      const layer = this.renderState.baseLayer;
      const target = layer && opaque(layer);
      const gl = layer?.context;
      if (target) {
        gl.disable(gl.SCISSOR_TEST);
        gl.bindFramebuffer(gl.FRAMEBUFFER, target.framebuffer);
        gl.clearBufferfv(gl.COLOR, 0, [0, 0, 0, 0]);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
      }
      callback(time, frame);
      if (target) {
        gl.disable(gl.SCISSOR_TEST);
        gl.bindFramebuffer(gl.READ_FRAMEBUFFER, target.framebuffer);
        gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER, null);
        const { width, height } = target;
        gl.blitFramebuffer(0, 0, width, height, 0, 0, width, height, gl.COLOR_BUFFER_BIT, gl.NEAREST);
        gl.bindFramebuffer(gl.FRAMEBUFFER, null);
        window.__compositedFrames++;
      }
    });
  };
}

// ----------------------------------------------------------------- helpers

const ctx = { base: '', browser: null };

async function screenshot(page, name) {
  const file = join(out, `${name}.png`);
  const png = await page.screenshot();
  await writeFile(file, png);
  return decodePng(png);
}

/** The canvas element's rectangle in screenshot pixels. */
const canvasBox = (page) => page.locator('canvas#scene').boundingBox();

async function expectFlatRendering(page, name) {
  const before = (await state(page)).flatFrames;
  const after = await until(page, (s) => s.flatFrames >= before + 10, 'flat frames');
  check(after.status === 'flat', `status ${after.status}, expected flat`);
  check(!after.session, 'no session expected');
  const image = await screenshot(page, name);
  const box = await canvasBox(page);
  // demo_camera looks slightly down at the scene: sky at the top, floor below.
  const top = mean(image, box.x + 10, box.y + 4, box.x + box.width - 10, box.y + 20);
  const bottom = mean(image, box.x + 10, box.y + box.height - 20, box.x + box.width - 10, box.y + box.height - 4);
  check(isSky(top), `${name}: flat canvas top is not sky: ${top}`);
  check(!isSky(bottom), `${name}: flat canvas bottom looks like sky: ${bottom}`);
  return after;
}

async function enterByClick(page) {
  await page.click('#enter');
  return until(page, (s) => s.status === 'immersive' && s.frame?.count >= 5, 'an immersive session');
}

const sub = (a, b) => a.map((v, i) => v - b[i]);
const length = (v) => Math.hypot(...v);

function checkProjections(frame) {
  for (const view of frame.views) {
    const expected = mul(GL_TO_WGPU_DEPTH, view.xrProjection);
    expected.forEach((v, i) => near(view.proj[i], v, 1e-5, `${view.eye} proj[${i}]`));
  }
}

function checkStereo(frame, ipd) {
  check(frame.views.length === 2, `expected 2 views, got ${frame.views.length}`);
  const [left, right] = frame.views;
  check(left.eye === 'left' && right.eye === 'right', `eyes ${left.eye}/${right.eye}`);
  const [w, h] = frame.layerSize;
  check(left.viewport.join() === [0, 0, w / 2, h].join(), `left viewport ${left.viewport}`);
  check(right.viewport.join() === [w / 2, 0, w / 2, h].join(), `right viewport ${right.viewport}`);
  near(length(sub(right.eyePosition, left.eyePosition)), ipd, 1e-4, 'eye separation');
  check(left.view.some((v, i) => Math.abs(v - right.view[i]) > 1e-4), 'view matrices are identical');
  checkProjections(frame);
}

/** Reads back layer pixels near the top and bottom of each eye in the XR frame. */
async function readEyes(page) {
  const { frame } = await state(page);
  const [w, h] = frame.layerSize;
  const points = [[w / 4, h - 3], [w / 4, 2], [(3 * w) / 4, h - 3], [(3 * w) / 4, 2]].map((p) => p.map(Math.floor));
  await page.evaluate((p) => window.xemXr.requestReadback(p), points);
  const s = await until(page, (x) => x.readback, 'a readback');
  const [leftTop, leftBottom, rightTop, rightBottom] = s.readback.pixels;
  return { readback: s.readback, leftTop, leftBottom, rightTop, rightBottom };
}

function checkUpright(eyes, label) {
  // Layer rows count from the bottom: the top of each eye must be sky.
  check(isSky(eyes.leftTop) && isSky(eyes.rightTop), `${label}: eye tops are not sky: ${eyes.leftTop} ${eyes.rightTop}`);
  check(isFloor(eyes.leftBottom) && isFloor(eyes.rightBottom), `${label}: eye bottoms are not floor: ${eyes.leftBottom} ${eyes.rightBottom}`);
}

/** Both halves of the emulated headset's canvas show the scene, upright. */
function checkStereoScreenshot(image, label) {
  const { width, height } = image;
  for (const [eye, x0] of [['left', 0], ['right', width / 2]]) {
    // The right part of each eye: the page's controls sit over the left one.
    const top = mean(image, x0 + width / 4, 4, x0 + width / 2 - 20, 30);
    const bottom = mean(image, x0 + width / 4, height - 30, x0 + width / 2 - 20, height - 4);
    check(isSky(top), `${label} ${eye}: screenshot top is not sky: ${top}`);
    check(isFloor(bottom), `${label} ${eye}: screenshot bottom is not floor: ${bottom}`);
  }
}

// --------------------------------------------------------------- scenarios

const scenarios = {
  /** (1) A browser without navigator.xr: XR unavailable, flat canvas renders. */
  async 'no-webxr'() {
    const page = await openPage(ctx.browser, `${ctx.base}/xr/`, {
      initScript: () => {
        delete Navigator.prototype.xr;
      },
    });
    const s = await expectFlatRendering(page, 'no-webxr-flat');
    check(s.support.navigatorXr === false, 'navigator.xr should be missing');
    check(s.support.immersiveVr === null, 'immersive-vr should be unknown');
    check(await page.isDisabled('#enter'), 'Enter VR should be disabled');
    check((await page.textContent('#enter')) === 'WebXR unavailable', 'button label');
    await page.close();
    return `flat frames ${s.flatFrames}, adapter ${s.support.adapter}`;
  },

  /** (6a) Chromium's own navigator.xr without an XR runtime. */
  async 'no-runtime'() {
    const page = await openPage(ctx.browser, `${ctx.base}/xr/`);
    const s = await expectFlatRendering(page, 'no-runtime-flat');
    check(s.support.navigatorXr === true, 'Chromium exposes navigator.xr');
    check(s.support.immersiveVr !== true, 'no runtime: immersive-vr must not be supported');
    check(await page.isDisabled('#enter'), 'Enter VR should be disabled');
    await page.close();
    return `immersiveVr=${s.support.immersiveVr} webgpu=${s.support.webgpu} XRGPUBinding=${s.support.xrGpuBinding}`;
  },

  /**
   * Chromium 152 exposes XRGPUBinding (WebGPU XR layers) only behind a Blink
   * flag. The adapter detects and reports it but submits through WebGL2: no
   * runtime available here (IWER included) can test WebGPU submission.
   */
  async 'gpu-binding'() {
    const browser = await launch(['--enable-blink-features=WebXRGPUBinding']);
    try {
      const page = await openPage(browser, `${ctx.base}/xr/`);
      const s = await state(page);
      check(s.support.xrGpuBinding === true, 'XRGPUBinding should be detected under the flag');
      check(s.support.submission === 'webgl2', `submission ${s.support.submission}`);
      const plain = await openPage(ctx.browser, `${ctx.base}/xr/`);
      const without = await state(plain);
      await plain.close();
      check(without.support.xrGpuBinding === false, 'XRGPUBinding is off by default in Chromium 152');
      return `XRGPUBinding detected with --enable-blink-features=WebXRGPUBinding (absent by default); WebGPU ${s.support.webgpu}; submission stays ${s.support.submission}`;
    } finally {
      await browser.close();
    }
  },

  /** (6b) A device that supports only inline sessions. */
  async unsupported() {
    const page = await openPage(ctx.browser, `${ctx.base}/xr/?emulate=quest3-inline`);
    const s = await expectFlatRendering(page, 'unsupported-flat');
    check(s.support.navigatorXr && s.support.immersiveVr === false, `support ${JSON.stringify(s.support)}`);
    check(await page.isDisabled('#enter'), 'Enter VR should be disabled');
    check((await page.textContent('#enter')) === 'VR not supported', 'button label');
    await page.close();
    return 'immersive-vr reported unsupported; flat play continues';
  },

  /** (6c) The user (or browser) denies the session request. */
  async denied() {
    const page = await openPage(ctx.browser, `${ctx.base}/xr/?emulate=quest3`);
    await page.evaluate(() => {
      const xr = navigator.xr;
      window.__restoreRequest = xr.requestSession;
      xr.requestSession = () => Promise.reject(new DOMException('The user denied the request.', 'NotAllowedError'));
    });
    await page.click('#enter');
    const s = await until(page, (x) => x.status === 'flat' && x.lastError, 'the denial');
    check(s.lastError.includes('NotAllowedError'), `error ${s.lastError}`);
    check(s.sessionsStarted === 0, 'no session should start');
    await expectFlatRendering(page, 'denied-flat');
    check(!(await page.isDisabled('#enter')), 'Enter VR should be enabled again');
    // The page stays usable: the next request succeeds.
    await page.evaluate(() => {
      navigator.xr.requestSession = window.__restoreRequest;
    });
    const entered = await enterByClick(page);
    await page.click('#exit');
    await until(page, (x) => x.status === 'flat' && x.sessionsEnded === 1, 'exit');
    await page.close();
    return `denied: ${s.lastError}; retry entered (${entered.frame.count} frames)`;
  },

  /**
   * (2)-(5) Emulated Quest 3 with IWER's own layer (the canvas framebuffer):
   * entry by click, stereo views, rendering, scripted head/controller/hand
   * input, paused head tracking, exit by the app and by the runtime, re-entry.
   */
  async immersive() {
    const page = await openPage(ctx.browser, `${ctx.base}/xr/?emulate=quest3`);
    await page.evaluate(recordActivation);
    const flat = await expectFlatRendering(page, 'immersive-before');
    check(flat.support.immersiveVr === true, 'immersive-vr supported');
    const report = [];

    // (2) Entry by a real click (transient user activation), session frames.
    let s = await enterByClick(page);
    const activation = await page.evaluate(() => window.__activation);
    check(activation.length === 1 && activation[0] === true, `requestSession activation ${activation}`);
    const features = s.session.grantedFeatures;
    check(features.includes('local-floor') && features.includes('hand-tracking') && features.includes('bounded-floor'),
      `granted ${features}`);
    check(s.session.referenceSpace === 'local-floor', 'reference space');
    check(s.frame.framebuffer === 'default', 'IWER layers render to the canvas framebuffer');
    const flatAtEntry = s.flatFrames;
    const frameAtEntry = s.frame.count;
    s = await until(page, (x) => x.frame.count >= frameAtEntry + 30, '30 more XR frames');
    check(s.flatFrames <= flatAtEntry + 1, `flat loop kept running: ${flatAtEntry} -> ${s.flatFrames}`);
    check(!s.flatRunning, 'window animation frames must stop while immersive');
    checkStereo(s.frame, 0.063);
    check(s.frame.tracked && s.frame.viewer, 'viewer pose');
    near(s.frame.viewer.position[1], 1.6, 1e-4, 'default head height');
    report.push(`entered with features [${features}], layer ${s.frame.layerSize.join('x')}`);

    const eyes = await readEyes(page);
    checkUpright(eyes, 'IWER layer');
    const image = await screenshot(page, 'immersive-iwer-stereo');
    checkStereoScreenshot(image, 'IWER layer');
    report.push(`readback top ${eyes.leftTop}/${eyes.rightTop} bottom ${eyes.leftBottom}/${eyes.rightBottom}`);

    // (3) Head pose scripted through IWER.
    await page.evaluate(() => {
      const { device } = window.xemXr.emulator;
      device.position.set(0.5, 1.7, -0.3);
      const yaw = (30 * Math.PI) / 180;
      device.quaternion.set(0, Math.sin(yaw / 2), 0, Math.cos(yaw / 2));
    });
    s = await until(page, (x) => Math.abs(x.frame.viewer.position[0] - 0.5) < 1e-4, 'the scripted head pose');
    const [hx, hy, hz] = s.frame.viewer.position;
    near(hy, 1.7, 1e-4, 'head y');
    near(hz, -0.3, 1e-4, 'head z');
    near(s.frame.viewer.orientation[1], Math.sin(Math.PI / 12), 1e-4, 'head yaw');
    // The eyes sit ipd/2 either side of the head along its rotated x axis.
    const mid = s.frame.views[0].eyePosition.map((v, i) => (v + s.frame.views[1].eyePosition[i]) / 2);
    near(length(sub(mid, [hx, hy, hz])), 0, 1e-4, 'eye midpoint');
    const axis = sub(s.frame.views[1].eyePosition, s.frame.views[0].eyePosition).map((v) => v / 0.063);
    near(axis[0], Math.cos(Math.PI / 6), 1e-3, 'eye axis x');
    near(axis[2], -Math.sin(Math.PI / 6), 1e-3, 'eye axis z');
    report.push(`head pose followed: ${s.frame.viewer.position.map((v) => v.toFixed(3))}`);

    // Visibility changes and reference-space resets (recentering) reach the app.
    await page.evaluate(() => window.xemXr.emulator.device.updateVisibilityState('visible-blurred'));
    await until(page, (x) => x.session.visibilityState === 'visible-blurred', 'visible-blurred');
    await page.evaluate(() => window.xemXr.emulator.device.updateVisibilityState('visible'));
    await until(page, (x) => x.session.visibilityState === 'visible', 'visible again');
    await page.evaluate(() => window.xemXr.emulator.device.recenter());
    s = await until(page, (x) => x.session.referenceSpaceResets >= 1, 'a reference-space reset');
    report.push(`visibility visible-blurred/visible and ${s.session.referenceSpaceResets} reference-space reset reported`);

    // (3) Controllers: poses, buttons, axes, select events.
    await page.evaluate(() => {
      const { device } = window.xemXr.emulator;
      const right = device.controllers.right;
      right.position.set(0.25, 1.2, -0.4);
      right.updateButtonValue('trigger', 1);
      right.updateButtonValue('a-button', 1);
      right.updateAxes('thumbstick', 0.5, -0.25);
    });
    s = await until(page, (x) => {
      const right = x.input.find((i) => i.handedness === 'right');
      return right?.gamepad?.buttons[0]?.value === 1 && x.session.events.selectstart >= 1;
    }, 'the right trigger');
    const right = s.input.find((i) => i.handedness === 'right');
    const left = s.input.find((i) => i.handedness === 'left');
    check(right.targetRayMode === 'tracked-pointer' && right.profiles.length > 0, `right ${right.targetRayMode} ${right.profiles}`);
    check(right.grip && right.targetRay, 'right poses');
    near(right.grip.position[0], 0.25, 0.1, 'right grip x');
    near(right.targetRay.position[1], 1.2, 0.1, 'right ray y');
    check(right.gamepad.buttons[0].pressed, 'trigger pressed');
    check(right.gamepad.buttons.some((b, i) => i > 2 && b.pressed), 'a-button pressed');
    check(right.gamepad.axes.includes(0.5) && right.gamepad.axes.includes(-0.25), `axes ${right.gamepad.axes}`);
    check(right.hand === null && left.hand === null, 'controllers are not hands');
    await page.evaluate(() => window.xemXr.emulator.device.controllers.right.updateButtonValue('trigger', 0));
    s = await until(page, (x) => x.session.events.select >= 1 && x.session.events.selectend >= 1, 'select/selectend');
    report.push(`controller: profile ${right.profiles[0]}, axes [${right.gamepad.axes}], events ${JSON.stringify(s.session.events)}`);

    // (3) Hands: joints, pinch.
    await page.evaluate(() => {
      window.xemXr.emulator.device.primaryInputMode = 'hand';
    });
    s = await until(page, (x) => x.input.length === 2 && x.input.every((i) => i.hand?.length === 25), 'tracked hands');
    const tipGap = (state) => {
      const hand = state.input.find((i) => i.handedness === 'right').hand;
      const joint = (name) => hand.find((j) => j.name === name).pose.position;
      return length(sub(joint('thumb-tip'), joint('index-finger-tip')));
    };
    const open = tipGap(s);
    const selectsBefore = s.session.events.select ?? 0;
    await page.evaluate(() => window.xemXr.emulator.device.hands.right.updatePinchValue(1));
    s = await until(page, (x) => x.input.length === 2 && tipGap(x) < open / 2, 'a pinched right hand');
    const pinched = tipGap(s);
    await page.evaluate(() => window.xemXr.emulator.device.hands.right.updatePinchValue(0));
    s = await until(page, (x) => (x.session.events.select ?? 0) > selectsBefore, 'the pinch select');
    const hand = s.input.find((i) => i.handedness === 'right');
    check(hand.targetRayMode === 'tracked-pointer', `hand ray ${hand.targetRayMode}`);
    report.push(`hands: 25 joints each; thumb-index gap ${open.toFixed(3)} m open, ${pinched.toFixed(3)} m pinched; profile ${hand.profiles[0]}`);
    await screenshot(page, 'immersive-iwer-hands');
    await page.evaluate(() => {
      window.xemXr.emulator.device.primaryInputMode = 'controller';
    });

    // (4) Paused simulation: scene time frozen, head tracking continues.
    await page.click('#pause');
    s = await until(page, (x) => x.paused && x.frame.paused, 'pause');
    const frozen = s.sceneTime;
    const pausedFrame = s.frame.count;
    const views = [];
    for (let i = 0; i < 5; i++) {
      await page.evaluate((step) => window.xemXr.emulator.device.position.set(-0.2 + step * 0.1, 1.6, 0.2), i);
      s = await until(page, (x) => Math.abs(x.frame.viewer.position[0] - (-0.2 + i * 0.1)) < 1e-4, `paused head step ${i}`);
      check(s.sceneTime === frozen && s.frame.sceneTime === frozen, `scene time moved while paused: ${frozen} -> ${s.sceneTime}`);
      views.push(s.frame.views[0].view[12]);
    }
    check(new Set(views.map((v) => v.toFixed(4))).size === 5, `view matrices did not follow the head: ${views}`);
    check(s.frame.count > pausedFrame + 4, 'frames kept running while paused');
    const pausedEyes = await readEyes(page);
    checkUpright(pausedEyes, 'paused');
    await screenshot(page, 'immersive-iwer-paused');
    await page.click('#pause');
    s = await until(page, (x) => !x.paused && x.sceneTime > frozen + 0.1, 'resume');
    report.push(`paused at scene time ${frozen.toFixed(3)} over ${s.frame.count - pausedFrame} frames while the head moved; resumed`);

    // (5) Exit through the app's control, then re-enter.
    await page.click('#exit');
    s = await until(page, (x) => x.status === 'flat' && x.sessionsEnded === 1, 'exit by the app');
    check(!s.session && s.input.length === 0, 'session state cleared');
    await expectFlatRendering(page, 'immersive-after-exit');
    s = await enterByClick(page);
    check(s.sessionsStarted === 2, 're-entry');
    checkStereo(s.frame, 0.063);

    // (5) The runtime ends the session (system menu, headset removed...).
    await page.evaluate(() => window.xemXr.emulator.device.activeSession.end());
    s = await until(page, (x) => x.status === 'flat' && x.sessionsEnded === 2, 'exit by the runtime');
    await expectFlatRendering(page, 'immersive-after-runtime-end');
    report.push('exit by app control and by the runtime returned to the flat canvas; re-entry worked');
    const errors = page.xemLog.filter((line) => line.startsWith('[error]') || line.startsWith('[pageerror]'));
    check(errors.length === 0, `console errors:\n${errors.join('\n')}`);
    await page.close();
    return report.join('\n    ');
  },

  /**
   * The opaque-framebuffer path a headset browser uses: the layer owns a
   * WebGLFramebuffer, which wgpu renders into as an external framebuffer, and
   * the eyes have asymmetric projections.
   */
  async opaque() {
    const page = await openPage(ctx.browser, `${ctx.base}/xr/?emulate=quest3`);
    await page.evaluate(installHeadsetCompositor);
    let s = await enterByClick(page);
    check(s.frame.framebuffer === 'opaque', `framebuffer ${s.frame.framebuffer}`);
    s = await until(page, (x) => x.frame.count >= 20, 'frames');
    checkStereo(s.frame, 0.063);
    const [left, right] = s.frame.views;
    // Mirrored off-axis frusta: the horizontal offset terms have opposite signs.
    check(left.xrProjection[8] < -0.01 && right.xrProjection[8] > 0.01, `asymmetry ${left.xrProjection[8]} ${right.xrProjection[8]}`);
    near(left.xrProjection[8], -right.xrProjection[8], 1e-6, 'mirrored frusta');
    check(left.xrProjection[9] < -0.01, 'vertical offset (more down than up)');
    const eyes = await readEyes(page);
    check(eyes.readback.framebuffer === 'opaque', 'readback from the opaque framebuffer');
    checkUpright(eyes, 'opaque layer');
    const composited = await page.evaluate(() => window.__compositedFrames);
    check(composited >= 20, `composited ${composited}`);
    const image = await screenshot(page, 'opaque-stereo');
    checkStereoScreenshot(image, 'opaque layer');
    await page.click('#exit');
    await until(page, (x) => x.status === 'flat', 'exit');
    await expectFlatRendering(page, 'opaque-after-exit');
    await page.close();
    return `opaque ${s.frame.layerSize.join('x')} framebuffer rendered through wgpu; proj[8] ${left.xrProjection[8].toFixed(3)}/${right.xrProjection[8].toFixed(3)}`;
  },
};

// -------------------------------------------------------------------- main

const selected = process.argv.slice(2);
const names = selected.length ? selected : Object.keys(scenarios);
await mkdir(out, { recursive: true });
const server = await serve(webRoot);
ctx.base = server.url;
ctx.browser = await launch();
console.log(`Chromium ${ctx.browser.version()}, serving ${webRoot} at ${server.url}, screenshots in ${out}`);
let failed = 0;
for (const name of names) {
  const scenario = scenarios[name];
  if (!scenario) {
    console.log(`?? ${name}: no such scenario`);
    failed++;
    continue;
  }
  try {
    const detail = await scenario();
    console.log(`ok ${name}\n    ${detail}`);
  } catch (error) {
    failed++;
    console.log(`FAIL ${name}\n    ${error.stack ?? error}`);
  }
}
await ctx.browser.close();
server.close();
console.log(failed ? `${failed} of ${names.length} failed` : `all ${names.length} passed`);
process.exit(failed ? 1 : 0);
