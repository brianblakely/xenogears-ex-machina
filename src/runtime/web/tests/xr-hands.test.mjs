// Hand gestures in a headset browser: IWER's Meta Quest 3 tracking hands
// (no controllers) whose joints take the poses of a simple hand model (the
// one xem_assist::synthetic uses), over the game module fixture. An
// overhand grab toggles the player's Pause (the game's clock stands still
// while head tracking and drawing go on), prayer hands open and close the
// settings panel in the session (with their own pause), and a hand that
// is lost pauses the game until the player's Pause resumes it; the pinch is
// the hand's standard select. Needs no private inputs.

import assert from 'node:assert/strict';
import { describe, test } from 'node:test';
import { HAND_MODEL, IWER, NO_WEBGPU, QUEST as HEADSET, QUEST_BROWSER, start, status } from './support.mjs';

const QUEST = HEADSET + HAND_MODEL;

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
      await page.evaluate(() => { const p = window.xrDevice.position; window.xemHeadStart = [p.x, p.y, p.z]; });
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

      // A controller's trigger clicks where it points on the panel: open
      // Settings with prayer hands, then point the right controller at the
      // panel's Close button (440x560 logical, Close at 410, 30) and pull.
      await prayer();
      assert.equal(await page.evaluate(() => window.xem.panel.visible()), true);
      const [panelWidth, panelHeight] = (await xr()).panelSize;
      await page.evaluate(({ panelWidth, panelHeight }) => {
        const d = window.xrDevice;
        d.hands.left.quaternion.set(0, 0, 0, 1);
        d.hands.right.quaternion.set(0, 0, 0, 1);
        d.primaryInputMode = 'controller';
        // The panel's Close button (the header's last control, 30 logical
        // pixels in from the top right) in the session's space (origin at
        // the head as the session began): the panel 1.1 m ahead, 0.75 m
        // down, tilted 25 degrees, 0.55 m wide.
        const tilt = -25 * Math.PI / 180;
        const height = 0.55 * panelHeight / panelWidth;
        const [x, y] = [((panelWidth - 30) / panelWidth - 0.5) * 0.55, (0.5 - 30 / panelHeight) * height];
        const target = [x, -0.75 + y * Math.cos(tilt), -1.1 + y * Math.sin(tilt)];
        const head = window.xemHeadStart;
        const from = [0.2, -0.3, -0.3];
        const dir = target.map((v, i) => v - from[i]);
        const len = Math.hypot(...dir);
        const d3 = dir.map((v) => v / len);
        // The rotation taking -Z to the direction.
        const axis = [-d3[1] * -1 * 0 + (0 * d3[2] - (-1) * d3[1]), (-1) * d3[0] - 0 * d3[2], 0 * d3[1] - 0 * d3[0]];
        const angle = Math.acos(-d3[2]);
        const n = Math.hypot(...axis) || 1;
        const s = Math.sin(angle / 2) / n;
        const right = d.controllers.right;
        right.position.set(head[0] + from[0], head[1] + from[1], head[2] + from[2]);
        right.quaternion.set(axis[0] * s, axis[1] * s, axis[2] * s, Math.cos(angle / 2));
      }, { panelWidth, panelHeight });
      await frames(4);
      await page.evaluate(() => window.xrDevice.controllers.right.updateButtonValue('trigger', 1));
      await frames(3);
      await page.evaluate(() => window.xrDevice.controllers.right.updateButtonValue('trigger', 0));
      await frames(6);
      const clicked = await xr();
      assert.equal(await page.evaluate(() => window.xem.panel.visible()), false, `the trigger on Close closed the panel: ${JSON.stringify(clicked)}`);
      assert.equal((await xr()).settings, false);
      await page.evaluate(() => { window.xrDevice.primaryInputMode = 'hand'; });
      await frames(4);
      for (const name of ['open', 'half', 'fist', 'open']) await shape('right', name);
      if ((await xr()).pauses.player) for (const name of ['open', 'half', 'fist', 'open']) await shape('right', name);

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

      // The pinch is the hand's standard select (IWER's pinch value), not
      // the joints' closeness; WebXR has no thumb microgestures, so the
      // hands play as a pointer (its keys beside the screen).
      const before = (await xr()).pinches;
      await page.evaluate(() => window.xrDevice.hands.right.updatePinchValue(1));
      await frames(3);
      await page.evaluate(() => window.xrDevice.hands.right.updatePinchValue(0));
      await frames(3);
      state = await xr();
      assert.equal(state.pinches, before + 1, `one select, one pinch: ${JSON.stringify(state)}`);
      assert.equal(state.pointer, true, 'no microgestures: a pointer');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
