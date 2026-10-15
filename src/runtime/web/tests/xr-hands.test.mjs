// Hand gestures in a headset browser: IWER's Meta Quest 3 tracking hands
// (no controllers) whose joints take the poses of a simple hand model (the
// one xem_assist::synthetic uses), over the game module fixture. Either
// hand's overhand grab toggles the player's Pause, both hands' together
// toggle it once (the game's clock stands still while the head is tracked,
// the views following it and the session drawing on); an underhand grab
// of either hand is inspection's (counted: the fixture has no field to
// look around in), and a sideways grab, a fist turned while closed and a
// hand found again closed toggle nothing. Prayer hands open and close the
// settings panel in the session (with their own pause, the head tracked
// through it too), a controller's trigger clicks its Close button, a hand
// that is lost pauses the game until the player's Pause resumes it, and
// the pinch is the hand's standard select. Two whole-hand pinches
// spreading and contracting scale the screen and own both hands through
// their release (the runtime's pinch held through them counts no pinch,
// letting go into fists toggles nothing, the game runs on), and the
// pointer's keys beside the screen keep their size and stay beside it.
// Transient gestures are read from status().xr.counts. Needs no private
// inputs.

import assert from 'node:assert/strict';
import { describe, test } from 'node:test';
import { HAND_MODEL, IWER, NO_WEBGPU, QUEST as HEADSET, QUEST_BROWSER, rotate, start, status } from './support.mjs';

const QUEST = HEADSET + HAND_MODEL;
// Hand orientations: palm down (as the model is), palm up and sideways.
const DOWN = [0, 0, 0, 1];
const UP = [0, 0, 1, 0];
const SIDE = [0, 0, Math.SQRT1_2, Math.SQRT1_2];

describe('hand gestures in a headset browser', { skip: !IWER && 'needs iwer (npm ci)' }, () => {
  test('either hand\'s grabs pause, prayer hands open settings, scaling owns both hands', { timeout: 300_000 }, async () => {
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
      // Both hands to a shape in the same frame.
      const shapes = async (name, n = 3) => {
        await page.evaluate((name) => {
          window.handModel.set(window.xrDevice, 'left', name);
          window.handModel.set(window.xrDevice, 'right', name);
        }, name);
        await frames(n);
      };
      const turn = (hand, q) => page.evaluate(({ hand, q }) => window.xrDevice.hands[hand].quaternion.set(...q), { hand, q });
      // A five-finger grab closing and opening again, the palm turned `q`.
      const grab = async (hand, q = DOWN) => {
        await turn(hand, q);
        await frames(3);
        for (const name of ['open', 'half', 'fist', 'open']) await shape(hand, name);
        await turn(hand, DOWN);
        await frames(3);
      };
      const xr = async () => (await status(page)).xr;
      const counts = async () => (await xr()).counts;
      const gameFrames = async () => (await status(page)).loop.gameFrames;
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      // Nine points about each view's centre (layer pixels from the bottom left).
      const points = [0.25, 0.75].flatMap((u) => [-0.1, 0, 0.1].flatMap((dx) => [-0.15, 0, 0.15]
        .map((dy) => [Math.floor((u + dx) * layer[0]), Math.floor((0.5 + dy) * layer[1])])));
      const read = () => page.evaluate((points) => window.xem.xr.readback(points), points);

      // Head tracking while a pause holds the game: the head moved and
      // turned a quarter to the right, the views follow it, the session
      // draws on and the game's clock stands still; then the head back.
      const tracked = async (pause) => {
        const before = await status(page);
        const seen = await read();
        await page.evaluate(() => {
          const [x, y, z] = window.xemHeadStart;
          window.xrDevice.position.set(x + 0.2, y, z + 0.1);
          window.xrDevice.quaternion.set(0, -Math.SQRT1_2, 0, Math.SQRT1_2);
        });
        await frames(8);
        const after = await status(page);
        const turned = await read();
        console.log(`${pause}: views ${seen.slice(16, 20)} / ${seen.slice(52, 56)}, the head turned ${turned.slice(16, 20)} / ${turned.slice(52, 56)}; `
          + `session frames ${before.page.xr.frames} to ${after.page.xr.frames}, game frame ${after.loop.gameFrames}`);
        assert.equal(after.loop.gameFrames, before.loop.gameFrames, `${pause}: the game's clock stands still`);
        assert.ok(after.page.xr.frames >= before.page.xr.frames + 8, `${pause}: the session draws on (${before.page.xr.frames} to ${after.page.xr.frames})`);
        assert.notDeepEqual(after.page.xr.viewer, before.page.xr.viewer, `${pause}: the head is tracked`);
        assert.notDeepEqual(turned, seen, `${pause}: the views follow the head`);
        await page.evaluate(() => {
          window.xrDevice.position.set(...window.xemHeadStart);
          window.xrDevice.quaternion.set(0, 0, 0, 1);
        });
        await frames(8);
        assert.equal(await gameFrames(), before.loop.gameFrames, `${pause}: the game's clock still stands`);
      };

      // An overhand grab: Pause, at once on closure; the head tracked
      // while it holds; reopened and grabbed again: resumed.
      for (const name of ['half', 'fist']) await shape('right', name);
      assert.equal((await xr()).pauses.player, true, 'paused');
      await tracked('the player\'s Pause');
      for (const name of ['open', 'half', 'fist', 'open']) await shape('right', name);
      assert.equal((await xr()).pauses.player, false, 'resumed');
      const running = await gameFrames();
      await frames(10);
      assert.ok(await gameFrames() > running, 'the game runs again');
      assert.equal((await counts()).pause, 2);

      // The left hand's: Pause and resume.
      await grab('left');
      assert.equal((await xr()).pauses.player, true, 'the left hand\'s grab paused');
      await grab('left');
      assert.equal((await xr()).pauses.player, false, 'and resumed');
      assert.equal((await counts()).pause, 4);
      // Both hands closing together: one toggle.
      for (const name of ['half', 'fist', 'open']) await shapes(name);
      assert.equal((await xr()).pauses.player, true, 'both hands\' grab paused');
      assert.equal((await counts()).pause, 5, 'both hands closing together toggle once');
      await grab('right');
      assert.equal((await xr()).pauses.player, false);

      // Underhand grabs (palm up) are inspection's, either hand's; the
      // fixture has no field to look around in, so nothing else changes.
      let before = await counts();
      await grab('right', UP);
      await grab('left', UP);
      let state = await xr();
      assert.equal(state.counts.inspect, before.inspect + 2, `either hand's underhand grab: ${JSON.stringify(state.counts)}`);
      assert.equal(state.counts.pause, before.pause, 'no Pause from an underhand grab');
      assert.equal(state.pauses.player, false);
      assert.equal(state.inspecting, false, 'no field, no inspection');
      // A sideways grab, and a fist closed sideways then turned palm down
      // and palm up while closed: nothing.
      before = state.counts;
      await grab('right', SIDE);
      await turn('right', SIDE);
      await frames(3);
      for (const name of ['open', 'half', 'fist']) await shape('right', name);
      for (const q of [DOWN, UP, SIDE]) {
        await turn('right', q);
        await frames(3);
      }
      await shape('right', 'open');
      await turn('right', DOWN);
      await frames(3);
      state = await xr();
      assert.deepEqual(state.counts, before, `a sideways grab and a turned fist toggle nothing: ${JSON.stringify(state.counts)}`);
      assert.equal(state.pauses.player, false);

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
      state = await xr();
      assert.equal(state.settings, true, `settings opened: ${JSON.stringify(state)}`);
      assert.equal(state.pauses.settings, true, 'settings pause the game');
      assert.equal(await page.evaluate(() => window.xem.panel.visible()), true, 'the panel shows in the session');
      await tracked('the settings\' pause');
      await prayer();
      state = await xr();
      assert.equal(state.settings, false, 'closed again');
      assert.equal(await page.evaluate(() => window.xem.panel.visible()), false);
      assert.equal(state.pauses.player, false, 'the player\'s Pause was never set');

      // A controller's trigger clicks where it points on the panel: open
      // Settings with prayer hands, then point the right controller at the
      // panel's Close button (the header's last control, 30 logical pixels
      // in from the top right) where the session says the panel stands.
      await prayer();
      assert.equal(await page.evaluate(() => window.xem.panel.visible()), true);
      state = await xr();
      const [panelWidth, panelHeight] = state.panelSize;
      const { position, orientation, size } = state.panel;
      const local = [((panelWidth - 30) / panelWidth - 0.5) * size[0], (0.5 - 30 / panelHeight) * size[1], 0];
      const target = rotate(orientation, local).map((v, i) => v + position[i]);
      await page.evaluate((target) => {
        const d = window.xrDevice;
        d.hands.left.quaternion.set(0, 0, 0, 1);
        d.hands.right.quaternion.set(0, 0, 0, 1);
        d.primaryInputMode = 'controller';
        // The session's space has its origin at the head as the session began.
        const head = window.xemHeadStart;
        const from = [0.2, -0.3, -0.3];
        const dir = target.map((v, i) => v - from[i]);
        const len = Math.hypot(...dir);
        const d3 = dir.map((v) => v / len);
        // The rotation taking -Z to the direction: about -Z x d3 by its angle.
        const axis = [d3[1], -d3[0], 0];
        const angle = Math.acos(-d3[2]);
        const n = Math.hypot(...axis) || 1;
        const s = Math.sin(angle / 2) / n;
        const right = d.controllers.right;
        right.position.set(head[0] + from[0], head[1] + from[1], head[2] + from[2]);
        right.quaternion.set(axis[0] * s, axis[1] * s, axis[2] * s, Math.cos(angle / 2));
      }, target);
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

      // Losing tracking pauses until the player's Pause: the hands go (the
      // controllers taken up) and come back, the right one closed, which
      // toggles nothing; opened and grabbed, the game resumes.
      await page.evaluate(() => {
        const d = window.xrDevice;
        d.hands.left.quaternion.set(0, 0, 0, 1);
        d.hands.right.quaternion.set(0, 0, 0, 1);
        d.hands.left.position.set(-0.15, 1.4, -0.3);
        d.hands.right.position.set(0.15, 1.4, -0.3);
      });
      await frames(4);
      await shape('right', 'half');
      before = await counts();
      await page.evaluate(() => { window.xrDevice.primaryInputMode = 'controller'; });
      await frames(4);
      await page.evaluate(() => window.handModel.set(window.xrDevice, 'right', 'fist'));
      await page.evaluate(() => { window.xrDevice.primaryInputMode = 'hand'; });
      await frames(6);
      state = await xr();
      assert.equal(state.pauses.player, true, 'a lost hand paused the game');
      assert.equal(state.counts.lost, before.lost + 2, `both hands were lost: ${JSON.stringify(state.counts)}`);
      assert.equal(state.counts.pause, before.pause, 'the hand found closed toggled nothing');
      for (const name of ['open', 'half', 'fist', 'open']) await shape('right', name);
      assert.equal((await xr()).pauses.player, false, 'the player resumed it');

      // The pinch is the hand's standard select (IWER's pinch value), not
      // the joints' closeness; WebXR has no thumb microgestures, so the
      // hands play as a pointer (its keys beside the screen).
      before = await counts();
      await page.evaluate(() => window.xrDevice.hands.right.updatePinchValue(1));
      await frames(3);
      await page.evaluate(() => window.xrDevice.hands.right.updatePinchValue(0));
      await frames(3);
      state = await xr();
      assert.equal(state.counts.pinch, before.pinch + 1, `one select, one pinch: ${JSON.stringify(state)}`);
      assert.equal(state.pointer, true, 'no microgestures: a pointer');

      // Two whole-hand pinches 30 cm apart spread to 60 cm: the screen
      // twice as wide, the runtime's pinch held through it (IWER draws the
      // joints to its pinch pose by the pinch's value: that pose is the
      // gathered hand's here, and the pinch ends before the hands let go)
      // counting nothing; let go, then gathered again at 60 cm and
      // contracted to 30: the screen as it was, the hands letting go into
      // fists. The game runs on, the pointer's keys keep their size and
      // stay beside the screen's edge.
      const at = (x) => page.evaluate((x) => {
        window.xrDevice.hands.left.position.set(-x, 1.4, -0.3);
        window.xrDevice.hands.right.position.set(x, 1.4, -0.3);
      }, x);
      const pinching = (value) => page.evaluate((value) => {
        window.xrDevice.hands.left.updatePinchValue(value);
        window.xrDevice.hands.right.updatePinchValue(value);
      }, value);
      const keys = (state) => state.keys.map(([x, , , width]) => [Math.abs(x), width]);
      await page.evaluate(() => window.handModel.pinchPose(window.xrDevice, 'gather'));
      await at(0.15);
      await frames(3);
      state = await xr();
      before = state.counts;
      const [screenWidth] = state.screen.size;
      const keysBefore = keys(state);
      assert.ok(keysBefore.length > 0, 'the pointer\'s keys show');
      const scaling = await gameFrames();
      await shapes('gather');
      await pinching(1);
      for (const x of [0.18, 0.21, 0.24, 0.27, 0.3]) {
        await at(x);
        await frames(2);
      }
      state = await xr();
      assert.ok(Math.abs(state.scale - 2) < 0.1, `spread: the screen twice as wide (${state.scale})`);
      const wide = keys(state);
      await pinching(0);
      await frames(2);
      await shapes('open');
      await shapes('gather');
      await pinching(1);
      for (const x of [0.27, 0.24, 0.21, 0.18, 0.15]) {
        await at(x);
        await frames(2);
      }
      await pinching(0);
      await frames(2);
      await shapes('fist');
      state = await xr();
      console.log(`scaling: keys ${JSON.stringify(keysBefore)} then ${JSON.stringify(wide)} (|x|, width; metres), back to ${state.scale}; counts ${JSON.stringify(state.counts)}`);
      assert.ok(Math.abs(state.scale - 1) < 0.08, `contracted: the screen as it was (${state.scale})`);
      assert.equal(state.counts.scale, before.scale + 2, `two scalings: ${JSON.stringify(state.counts)}`);
      assert.equal(state.counts.pinch, before.pinch, 'the runtime\'s pinch held through the scaling counts no pinch');
      assert.equal(state.counts.pause, before.pause, 'letting go into fists toggles nothing');
      assert.equal(state.pauses.player, false);
      assert.ok(await gameFrames() > scaling, 'the game ran on through the scaling');
      assert.equal(wide.length, keysBefore.length);
      for (const [i, [x, width]] of wide.entries()) {
        assert.ok(Math.abs(width - keysBefore[i][1]) < 1e-6, `key ${i} keeps its size (${keysBefore[i][1]} m, then ${width} m)`);
        assert.ok(x > screenWidth && x > keysBefore[i][0], `key ${i} stays beside the wider screen's edge (${keysBefore[i][0]} m, then ${x} m out)`);
      }
      await shapes('open');
      await page.evaluate(() => window.handModel.pinchPose(window.xrDevice));
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
