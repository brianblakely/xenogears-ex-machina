// The seated diorama in a headset browser (local only: it needs the user's
// disc 1, the game module and xem-headless): Quest Browser's user agent with
// IWER's Meta Quest 3 as navigator.xr, the headset view set to the diorama,
// field 30 with Fei walked into the room. Each view sees the room on the
// table from where it is: moving the head 10 cm sideways shifts the table's
// near rows more than its far ones (parallax, not a flat picture); the player's
// Recenter puts the table in front of the head turned away, and the
// runtime's own recentring (IWER's, its reference space's `reset`) puts the
// seat back at the space's origin; the automation's frames end at the same
// RAM in and out of the headset; and a pinch with the head aimed through
// the table at a floor point heads the walk for it and walks Fei there.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HAND_MODEL, HEADLESS, IWER, NO_WEBGPU, QUEST, QUEST_BROWSER, SHOTS, USER_DISC, WEB, start, status } from './support.mjs';

const available = IWER && existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
mkdirSync(SHOTS, { recursive: true });

/** The shift (in points) that maps row `after` on row `before` best
 *  (after[i] ~ before[i + shift]; the least of equal ones), and whether that
 *  is a match: the compared part of the row has texture, and the shift lies
 *  inside the search and well below the median. */
function shift(before, after, reach = 45) {
  const luma = (row, i) => row[4 * i] * 0.3 + row[4 * i + 1] * 0.6 + row[4 * i + 2] * 0.1;
  const n = before.length / 4;
  const sad = new Map();
  for (let s = -reach; s <= reach; s++) {
    let sum = 0;
    for (let i = reach; i < n - reach; i++) sum += Math.abs(luma(after, i) - luma(before, i + s));
    sad.set(s, sum / (n - 2 * reach));
  }
  const shifts = [...sad.keys()].sort((a, b) => sad.get(a) - sad.get(b) || Math.abs(a) - Math.abs(b));
  const best = shifts[0];
  const median = sad.get(shifts[shifts.length >> 1]);
  const compared = Array.from({ length: n - 2 * reach }, (_, k) => luma(after, reach + k));
  const mean = compared.reduce((a, b) => a + b, 0) / compared.length;
  const texture = Math.sqrt(compared.reduce((sum, v) => sum + (v - mean) ** 2, 0) / compared.length);
  return { shift: best, matched: texture > 4 && Math.abs(best) < reach && sad.get(best) < 0.6 * median };
}

/** Whether two rows of RGBA bytes show the same picture (within a few levels). */
const same = (a, b) => a.reduce((sum, v, i) => sum + (i % 4 === 3 ? 0 : Math.abs(v - b[i])), 0) / (a.length * 3 / 4) <= 6;

describe('the seated diorama in a headset browser', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  test('the table shows in depth, recentres and takes a pinch through it', { timeout: 600_000 }, async () => {
    // Field 30 with Fei walked into the room (as xr-game-smoke.sh does).
    const state = join(SHOTS, 'xr-diorama-field30.state');
    execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
      input: [{ cmd: 'enter_field', map: 30 }, { cmd: 'frames', count: 400 }, { cmd: 'press', buttons: ['down'], frames: 60 },
        { cmd: 'frames', count: 30 }, { cmd: 'state_save', path: state }].map((c) => JSON.stringify(c)).join('\n') + '\n',
    });
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST + HAND_MODEL }] });
    try {
      const page = await browser.page('?xrScale=0.25');
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      const base64 = readFileSync(state).toString('base64');
      const load = (base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
      await page.evaluate(load, base64);
      // The same frames outside the headset first, for the state check.
      await page.evaluate(() => window.xem.runFrames(120));
      const flatHash = await page.evaluate(() => window.xem.memoryHash());
      await page.evaluate(load, base64);
      const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      const applied = await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, xr: { ...g.xr, view: 'Diorama' } } }), graphics);
      assert.equal(applied.ok, true, JSON.stringify(applied));

      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().xr.input && window.xem.status().page.xr.frames > 20, null, { timeout: 60_000 });
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);
      // The automation's frames in the headset end where the flat ones did.
      await page.evaluate(() => window.xem.runFrames(120));
      assert.equal(await page.evaluate(() => window.xem.memoryHash()), flatHash, 'the diorama changes nothing in the game');
      await page.evaluate(load, base64);
      // A game frame or two draws the diorama (the trace asks the port for
      // every face and the camera from the frame after the view changes).
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => window.xem.status().xr.diorama && window.xem.status().xr.share === 0, null, { timeout: 60_000 });
      // The session's and the game's rates over two seconds (recorded, not checked).
      const before = await status(page);
      const t0 = Date.now();
      await page.waitForTimeout(2000);
      const after = await status(page);
      const seconds = (Date.now() - t0) / 1000;
      console.log(`with the diorama: session ${((after.page.xr.frames - before.page.xr.frames) / seconds).toFixed(1)} frames/s, `
        + `game ${((after.loop.gameFrames - before.loop.gameFrames) / seconds).toFixed(1)} frames/s`);
      await page.evaluate(() => window.xem.setRunning(false));
      await frames(4);
      let info = await status(page);
      assert.equal(info.xr.diorama, true, 'the views show the diorama');
      console.log(`diorama: space ${JSON.stringify(info.xr.space)}, seat ${JSON.stringify(info.xr.seat)}`);

      // Rows of the left view across the table: low in the view the table's
      // near part, higher up its far part (layer pixels from the bottom).
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      const heights = Array.from({ length: 12 }, (_, i) => 0.04 + i * 0.04);
      const count = 200;
      const points = heights.flatMap((v) => Array.from({ length: count }, (_, i) => [Math.floor((0.05 + 0.4 * i / count) * layer[0]), Math.floor(v * layer[1])]));
      const read = async () => {
        const bytes = await page.evaluate((points) => window.xem.xr.readback(points), points);
        return heights.map((_, r) => bytes.slice(r * count * 4, (r + 1) * count * 4));
      };
      const rows = await read();
      await page.screenshot({ path: join(SHOTS, 'xr-diorama.png') });
      const lit = rows.map((row) => row.filter((_, i) => i % 4 === 0).filter((v, i) => Math.abs(v - 26) + Math.abs(row[4 * i + 1] - 30) + Math.abs(row[4 * i + 2] - 38) > 24).length / count);
      console.log(`rows covered by the table: ${lit.map((l) => l.toFixed(2))}`);
      assert.ok(lit.some((l) => l > 0.5), 'the table fills rows of the view');

      // Parallax: the head 10 cm to the right; the table stays in the room,
      // so its near rows move across the view more than its far ones.
      const head = (await status(page)).page.xr.viewer;
      const origin = await page.evaluate(() => { const p = window.xrDevice.position; return [p.x, p.y, p.z]; });
      await page.evaluate(([x, y, z]) => window.xrDevice.position.set(x + 0.1, y, z), origin);
      await frames(6);
      const moved = await read();
      await page.screenshot({ path: join(SHOTS, 'xr-diorama-moved.png') });
      const shifts = rows.map((row, r) => shift(row, moved[r]));
      console.log(`parallax for 10 cm sideways, by row (near first): ${JSON.stringify(shifts)} points`);
      // The rows across the table (those the move matches) shift by their
      // depth: the nearest more than the farthest, unlike a flat picture's
      // one shift (a desk nearer than the floor behind it may shift more
      // than a row below it).
      const matched = shifts.filter((s) => s.matched);
      assert.ok(matched.length >= 4, `rows the move matches: ${JSON.stringify(shifts)}`);
      assert.ok(matched[0].shift - matched[matched.length - 1].shift >= 5, `near rows shift more than far ones: ${JSON.stringify(shifts)}`);
      await page.evaluate((p) => window.xrDevice.position.set(...p), origin);
      await frames(6);
      const back = await read();
      assert.ok(rows.every((row, r) => same(row, back[r])), 'the head back, the view is too');

      // The player's Recenter with the head turned 30 degrees to the left:
      // the seat turns with it, so the table stands in front of the head again.
      const turn = Math.PI / 6;
      await page.evaluate((t) => window.xrDevice.quaternion.set(0, Math.sin(t / 2), 0, Math.cos(t / 2)), turn);
      await frames(6);
      const away = await read();
      assert.notDeepEqual(away, rows, 'turned away, the view shows the table elsewhere');
      await page.evaluate(() => window.xem.appOperation({ Action: 'Recenter' }));
      await frames(6);
      info = await status(page);
      assert.equal(info.xr.recenters, 1, 'one Recenter');
      assert.ok(Math.abs(info.xr.seat.yaw - turn) < 1e-3, `the seat faces the head's way: ${JSON.stringify(info.xr.seat)}`);
      assert.ok(Math.hypot(...info.xr.seat.position.map((v, i) => v - head[i])) < 0.01, `the seat at the head: ${JSON.stringify(info.xr.seat)} for ${head}`);
      const recentred = await read();
      assert.ok(recentred.every((row, r) => same(rows[r], row)), 'in front of the head again');
      // The runtime's recentring: IWER levels the head to the space's -Z and
      // resets its reference space; the seat is the space's origin again.
      await page.evaluate(() => window.xrDevice.recenter());
      await frames(6);
      info = await status(page);
      assert.equal(info.xr.resets, 1, 'the reference space reset reached the app');
      assert.ok(Math.abs(info.xr.seat.yaw) < 1e-6 && info.xr.seat.position.every((v) => Math.abs(v) < 1e-6), `the seat at the origin: ${JSON.stringify(info.xr.seat)}`);
      const reset = await read();
      assert.ok(reset.every((row, r) => same(rows[r], row)), 'the table in front of the head, as at first');

      // A pinch (the hand's select) with the head aimed through the table at
      // a floor point of the room (its point in the game's world, as the
      // table shows it) walks Fei there.
      await page.evaluate(() => {
        const device = window.xrDevice;
        device.primaryInputMode = 'hand';
        window.handModel.install(device);
        device.hands.left.position.set(-0.15, 1.4, -0.3);
        device.hands.right.position.set(0.15, 1.4, -0.3);
        window.handModel.set(device, 'left', 'open');
        window.handModel.set(device, 'right', 'open');
      });
      await page.evaluate(() => window.xem.setRunning(true));
      await frames(6);
      info = await status(page);
      // A floor point Fei walks to from where he stands, clear of everyone.
      const goal = [100, 0, 20];
      const m = info.xr.space.roomFromWorld;
      const room = [0, 1, 2].map((i) => m[i] * goal[0] + m[4 + i] * goal[1] + m[8 + i] * goal[2] + m[12 + i]);
      const eye = info.page.xr.viewer;
      await page.evaluate(({ room, eye }) => {
        // The head's -Z along the way from the eye to the point.
        const d = room.map((v, i) => v - eye[i]);
        const length = Math.hypot(...d);
        const [x, y, z] = d.map((v) => v / length);
        const yaw = Math.atan2(-x, -z);
        const pitch = Math.asin(y);
        const q = [Math.cos(yaw / 2), Math.sin(yaw / 2)];
        const p = [Math.cos(pitch / 2), Math.sin(pitch / 2)];
        // yaw about +Y, then pitch about the turned +X.
        window.xrDevice.quaternion.set(q[0] * p[1], q[1] * p[0], -q[1] * p[1], q[0] * p[0]);
      }, { room, eye });
      await frames(6);
      // The walk's target as the session holds it while Fei walks (he runs
      // there: the walk may be over before the test looks again).
      await page.evaluate(() => {
        window.walkTarget = null;
        window.walkPoll = setInterval(() => {
          const intent = window.xem.command({ cmd: 'status' }).assist?.intent;
          if (intent?.kind === 'walk' && !window.walkTarget) window.walkTarget = intent.to;
        }, 5);
      });
      await page.evaluate(() => window.xrDevice.hands.right.updatePinchValue(1));
      await frames(3);
      await page.evaluate(() => window.xrDevice.hands.right.updatePinchValue(0));
      await frames(3);
      // Walking, then arrived.
      await page.waitForFunction(() => window.xem.command({ cmd: 'status' }).assist_outcome === 'arrived', null, { timeout: 120_000 });
      const to = await page.evaluate(() => { clearInterval(window.walkPoll); return window.walkTarget; });
      const field = await page.evaluate(() => window.xem.command({ cmd: 'field' }));
      const fei = field.actors.find((a) => a.index === field.player).position;
      console.log(`walked to ${JSON.stringify(fei)}, the walk heading for ${JSON.stringify(to)}, for the floor point ${goal}`);
      assert.ok(to && Math.hypot(to.x - goal[0], to.z - goal[2]) < 2, `the walk heads for the floor point gazed at: ${JSON.stringify(to)}`);
      assert.ok(Math.hypot(fei.x - goal[0], fei.z - goal[2]) < 16, `Fei stands at the floor point gazed at: ${JSON.stringify(fei)}`);
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
