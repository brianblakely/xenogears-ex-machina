// Hand gestures in a headset browser: IWER's Meta Quest 3 tracking hands
// (no controllers) whose joints take the poses of a simple hand model (the
// one xem_assist::synthetic uses), over the game module fixture. An
// overhand grab toggles the player's Pause (the game's clock stands still
// while head tracking and drawing go on), prayer hands open and close the
// settings panel in the session (with their own pause), and a hand that
// is lost pauses the game until the player's Pause resumes it. Needs no
// private inputs.

import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { NO_WEBGPU, ROOT, start, status } from './support.mjs';

const QUEST_BROWSER = 'Mozilla/5.0 (X11; Linux x86_64; Quest 3) AppleWebKit/537.36 (KHTML, like Gecko) '
  + 'OculusBrowser/37.0.0.0 SamsungBrowser/4.0 Chrome/132.0.0.0 VR Safari/537.36';
const IWER_PATH = join(ROOT, 'src/runtime/web/node_modules/iwer/build/iwer.min.js');
const IWER = existsSync(IWER_PATH) ? readFileSync(IWER_PATH, 'utf8') : '';

// The hand model in the page: a right hand in its own frame (wrist at the
// origin, fingers along -Z, palm down), each finger curled by `curl`, the
// thumb's tip at `thumb`; poses are registered with IWER as left-hand data
// (IWER mirrors them for the right hand).
const MODEL = `
window.handModel = (() => {
  const JOINTS = ['wrist', 'thumb-metacarpal', 'thumb-phalanx-proximal', 'thumb-phalanx-distal', 'thumb-tip'];
  for (const f of ['index-finger', 'middle-finger', 'ring-finger', 'pinky-finger'])
    for (const j of ['metacarpal', 'phalanx-proximal', 'phalanx-intermediate', 'phalanx-distal', 'tip']) JOINTS.push(f + '-' + j);
  const right = (curl, thumb) => {
    const joints = Array.from({ length: 25 }, () => [0, 0, 0]);
    const lateral = [-0.035, -0.012, 0.01, 0.03];
    const bones = [[0.065, 0.04, 0.025, 0.022], [0.064, 0.045, 0.028, 0.024], [0.06, 0.042, 0.027, 0.024], [0.056, 0.034, 0.02, 0.02]];
    for (let f = 0; f < 4; f++) {
      const base = 5 + f * 5;
      joints[base] = [lateral[f] * 0.6, 0, 0];
      let angle = 0;
      let at = [lateral[f], 0, -bones[f][0]];
      joints[base + 1] = at;
      for (let k = 0; k < 3; k++) {
        angle += curl[f] * [1.4, 1.6, 1.2][k];
        at = [at[0], at[1] - Math.sin(angle) * bones[f][k + 1], at[2] - Math.cos(angle) * bones[f][k + 1]];
        joints[base + 2 + k] = at;
      }
    }
    joints[1] = [-0.02, -0.01, -0.02];
    joints[2] = [-0.04, -0.015, -0.04];
    joints[3] = joints[2].map((v, i) => (v + thumb[i]) / 2);
    joints[4] = thumb;
    return joints;
  };
  const rest = [-0.06, -0.01, -0.06];
  const fist = () => {
    const j = right([1, 1, 1, 1], [0, 0, 0]);
    j[4] = [j[12][0] - 0.01, j[12][1] - 0.012, j[12][2]];
    j[3] = j[2].map((v, i) => (v + j[4][i]) / 2);
    return j;
  };
  const shapes = { open: right([0, 0, 0, 0], rest), half: right([0.5, 0.5, 0.5, 0.5], rest), fist: fist() };
  const pose = (joints) => ({
    jointTransforms: Object.fromEntries(JOINTS.map((name, i) => {
      const [x, y, z] = joints[i];
      // Left-hand data: the model's x mirrored.
      return [name, { offsetMatrix: new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x, y, z, 1]), radius: 0.008 }];
    })),
  });
  const handInput = (hand) => hand[Object.getOwnPropertySymbols(hand).find((s) => s.description === '@iwer/xr-hand-input')];
  return {
    install(device) {
      for (const hand of [device.hands.left, device.hands.right]) {
        const poses = handInput(hand).poses;
        for (const [name, joints] of Object.entries(shapes)) poses[name] = pose(joints);
      }
    },
    set(device, hand, shape) {
      device.hands[hand].poseId = shape;
      device.hands[hand].updateHandPose();
    },
  };
})();`;
const QUEST = `${IWER}\n;window.xrDevice = new IWER.XRDevice(IWER.metaQuest3, { stereoEnabled: true });`
  + 'window.xrDevice.installRuntime({ forceInstall: true });' + MODEL;

describe('hand gestures in a headset browser', { skip: !IWER && 'needs iwer (npm ci)' }, () => {
  test('grabs pause, prayer hands open settings, a lost hand pauses', { timeout: 300_000 }, async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST }] });
    try {
      const page = await browser.page('?game=/fixtures/fixture.wasm&xrScale=0.25');
      await page.evaluate(async () => {
        await window.xem.boot({ executable: false });
        window.xem.command({ cmd: 'write_memory', address: 0x80000100, hex: '01000000' });
        window.xem.setRunning(true);
        const device = window.xrDevice;
        device.primaryInputMode = 'hand';
        window.handModel.install(device);
        // The hands in front of the player, palms down, apart.
        device.hands.left.position.set(-0.15, 1.4, -0.3);
        device.hands.right.position.set(0.15, 1.4, -0.3);
        window.handModel.set(device, 'left', 'open');
        window.handModel.set(device, 'right', 'open');
      });
      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().xr.input && window.xem.status().page.xr.frames > 20, null, { timeout: 60_000 });
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);
      const shape = async (hand, name, n = 3) => {
        await page.evaluate(({ hand, name }) => window.handModel.set(window.xrDevice, hand, name), { hand, name });
        await frames(n);
      };
      const xr = async () => (await status(page)).xr;
      const gameFrames = async () => (await status(page)).loop.gameFrames;

      // An overhand grab: Pause, at once on closure.
      for (const name of ['half', 'fist']) await shape('right', name);
      assert.equal((await xr()).pauses.player, true, 'paused');
      const held = await gameFrames();
      await frames(20);
      assert.equal(await gameFrames(), held, 'the game\'s clock stands still');
      assert.ok((await status(page)).page.xr.frames > 0, 'the session draws on');
      // Reopened and grabbed again: resumed.
      for (const name of ['open', 'half', 'fist', 'open']) await shape('right', name);
      assert.equal((await xr()).pauses.player, false, 'resumed');
      await frames(10);
      assert.ok(await gameFrames() > held, 'the game runs again');

      // Prayer hands: the open palms turned to face each other, together.
      const prayer = async () => {
        await page.evaluate(() => {
          const d = window.xrDevice;
          // Palms facing: the left hand turned palm right, the right palm left.
          d.hands.left.quaternion.set(0, 0, -Math.SQRT1_2, Math.SQRT1_2);
          d.hands.right.quaternion.set(0, 0, Math.SQRT1_2, Math.SQRT1_2);
          d.hands.left.position.set(-0.02, 1.4, -0.3);
          d.hands.right.position.set(0.02, 1.4, -0.3);
        });
        await frames(4);
        await page.evaluate(() => {
          const d = window.xrDevice;
          d.hands.left.position.set(-0.25, 1.4, -0.3);
          d.hands.right.position.set(0.25, 1.4, -0.3);
        });
        await frames(4);
      };
      await prayer();
      let state = await xr();
      assert.equal(state.settings, true, `settings opened: ${JSON.stringify(state)}`);
      assert.equal(state.pauses.settings, true, 'settings pause the game');
      assert.equal(await page.evaluate(() => window.xem.panel.visible()), true, 'the panel shows in the session');
      await prayer();
      state = await xr();
      assert.equal(state.settings, false, 'closed again');
      assert.equal(await page.evaluate(() => window.xem.panel.visible()), false);
      assert.equal(state.pauses.player, false, 'the player\'s Pause was never set');

      // Losing tracking pauses until the player's Pause.
      await page.evaluate(() => {
        const d = window.xrDevice;
        d.hands.left.quaternion.set(0, 0, 0, 1);
        d.hands.right.quaternion.set(0, 0, 0, 1);
        d.hands.left.position.set(-0.15, 1.4, -0.3);
        d.hands.right.position.set(0.15, 1.4, -0.3);
      });
      await frames(4);
      await page.evaluate(() => { window.xrDevice.primaryInputMode = 'controller'; });
      await frames(4);
      await page.evaluate(() => { window.xrDevice.primaryInputMode = 'hand'; });
      await frames(4);
      assert.equal((await xr()).pauses.player, true, 'a lost hand paused the game');
      for (const name of ['open', 'half', 'fist', 'open']) await shape('right', name);
      assert.equal((await xr()).pauses.player, false, 'the player resumed it');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
