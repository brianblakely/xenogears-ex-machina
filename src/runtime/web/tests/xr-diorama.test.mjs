// The seated diorama in a headset browser (local only: it needs the user's
// disc 1, the game module and xem-headless): Quest Browser's user agent with
// IWER's Meta Quest 3 as navigator.xr, the headset view set to the diorama,
// field 30 with Fei walked into the room. Each view sees the room on the
// table from where it is: moving the head 10 cm sideways shifts the table's
// near rows more than its far ones (parallax, not a flat picture); the player's
// Recenter puts the table in front of the head turned away, and the
// runtime's own recentring (IWER's, its reference space's `reset`) puts the
// seat back at the space's origin; the automation's frames end at the same
// RAM in and out of the headset; a pinch with the head aimed through the
// table at a floor point heads the walk for it and walks Fei there; two
// whole-hand pinches spreading grow the diorama live about its anchor and
// commit its scale once, the RAM still the flat run's at that scale; and
// immersive AR (offered: IWER's Quest 3 supports it), asked for with nothing
// AR-specific and its layer with alpha, blends by alpha: the layer is
// transparent where VR shows the room's backdrop or the game's clear on the
// wall behind the table, and drawn where it shows the diorama (VR's layer
// opaque). In the debug battle, its party attacking, the table stands
// still while the battle's camera moves; on the world map, its vehicle
// driven on, it stays on the camera's target; both show in depth, and the
// automation's frames end at the same RAM in and out of the headset. With
// IWER's hands taking a simple hand model's poses, over the diorama:
// either hand's overhand grab pauses and resumes, both hands' together
// toggle once, a sideways grab and a fist turned while closed toggle
// nothing; while the player's Pause holds the game the head moved 10 cm
// sees the table in depth, the game's clock standing still; the underhand
// grab looks from within the scene from the first session frame after it
// and, again, gives the diorama back on the first session frame after it,
// at the scale and place it had (every session frame recorded); the field's
// menu, opened and closed through the automation, eases from the diorama to
// the screen, shows on the screen alone and eases back, the settings panel's
// place beside the pane while the diorama shows and below the screen while
// the menu shows on it; prayer hands open
// the settings panel beside the pane, beyond the keys beside it and facing
// the seat (the pane in view), its own pause holding the game, and close
// it; Graphics' headset view set to the screen through xem.settings.apply
// in the session shows the screen alone from the next session frame, and
// set to the diorama again eases in from it. From map 25's scene (its
// dialogue auto-advanced to game frame 5080 by xem-headless), the player's
// own play eases out of the field's diorama over its story movie's first
// frames, shows the movie on the screen alone and eases back into map 25's
// diorama after it, every session frame within its time over the switch,
// the settings panel's place again following what shows.

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

/** An init script (after QUEST) recording, while `window.xrRecording` is
 *  set, what each session frame presented, as the page's frame callback
 *  left it: the frame's time (the view eases by it), the screen's share,
 *  whether the diorama showed, inspection and its eyes from within, whether
 *  the screen showed a movie frame rebuilt from what the MDEC decoded, the
 *  game's frames, and where the settings panel stood (its middle) and the
 *  seat. */
const RECORDER = `
window.xrTimeline = [];
window.xrRecording = false;
{
  const request = window.XRSession.prototype.requestAnimationFrame;
  window.XRSession.prototype.requestAnimationFrame = function (callback) {
    return request.call(this, (time, frame) => {
      callback(time, frame);
      if (!window.xrRecording) return;
      const s = window.xem.status();
      window.xrTimeline.push({ time, frame: s.page.xr.frames, share: s.xr.share, diorama: s.xr.diorama, inspecting: s.xr.inspecting,
        within: s.xr.within, movie: s.renderer?.movie ?? false, gameFrames: s.loop.gameFrames, panel: s.xr.panel?.position, seat: s.xr.seat });
    });
  };
}`;

/** What a recorded session frame showed: the diorama alone (D), the
 *  screen alone (S) or the view easing between the two (E). */
const phase = (f) => (f.diorama && f.share === 0 ? 'D' : f.share === 1 && !f.diorama ? 'S' : 'E');

/** The recorded session frames whose screen's share moved by more than
 *  their time allows over the 0.3 s switch. */
const jumps = (timeline) => timeline.slice(1).filter((f, i) => Math.abs(f.share - timeline[i].share) > (f.time - timeline[i].time) / 1000 / 0.3 + 0.01);

/** How far round to the right of the seat's way (`seat`: its place and its
 *  turn, radians to the left) a room point stands, about the vertical
 *  through the seat (degrees). */
const roundFrom = (seat, p) => {
  const [dx, dz] = [p[0] - seat.position[0], p[2] - seat.position[2]];
  const [c, s] = [Math.cos(seat.yaw), Math.sin(seat.yaw)];
  return Math.atan2(dx * c - dz * s, -dx * s - dz * c) * 180 / Math.PI;
};

/** Where a recorded session frame's settings panel stood: beside the
 *  diorama's pane (its middle 20 degrees or more round to the right of the
 *  seat's way) or below the screen (straight ahead, half a metre or more
 *  below the seat). */
const panelPlace = (f) => {
  if (!f.panel || !f.seat) return 'nowhere';
  const round = roundFrom(f.seat, f.panel);
  const up = f.panel[1] - f.seat.position[1];
  return round >= 20 ? 'beside' : Math.abs(round) < 0.5 && up < -0.5 ? 'below' : `${round.toFixed(1)} degrees round, ${up.toFixed(2)} m up`;
};

/** The recorded session frames whose settings panel stood elsewhere than
 *  beside the pane while the diorama showed and below the screen while the
 *  screen stood in for it. */
const misplaced = (timeline) => timeline.filter((f) => panelPlace(f) !== (f.diorama ? 'beside' : 'below')).map((f) => [f.frame, f.diorama, panelPlace(f)]);

/** `v` turned by the quaternion `q` (x, y, z, w). */
const turned = ([x, y, z, w], [a, b, c]) => {
  const t = [2 * (y * c - z * b), 2 * (z * a - x * c), 2 * (x * b - y * a)];
  return [a + w * t[0] + y * t[2] - z * t[1], b + w * t[1] + z * t[0] - x * t[2], c + w * t[2] + x * t[1] - y * t[0]];
};

/** Whether two rows of RGBA bytes show the same picture (within a few levels). */
const same = (a, b) => a.reduce((sum, v, i) => sum + (i % 4 === 3 ? 0 : Math.abs(v - b[i])), 0) / (a.length * 3 / 4) <= 6;

/** An init script recording the sessions the page asks for (mode and init)
 *  and the layers it makes for them (the session's mode and the layer's
 *  init: IWER's layer, the canvas's own framebuffer, takes no alpha of its
 *  own, so only the page's request shows what a headset's browser gets). */
const REQUESTS = `
window.xrRequests = [];
window.xrLayers = [];
{
  const modes = new WeakMap();
  const request = navigator.xr.requestSession.bind(navigator.xr);
  navigator.xr.requestSession = async (mode, init) => {
    window.xrRequests.push({ mode, init: JSON.parse(JSON.stringify(init ?? {})) });
    const session = await request(mode, init);
    modes.set(session, mode);
    return session;
  };
  const Layer = window.XRWebGLLayer;
  window.XRWebGLLayer = class extends Layer {
    constructor(session, context, init) {
      super(session, context, init);
      window.xrLayers.push({ mode: modes.get(session) ?? null, init: JSON.parse(JSON.stringify(init ?? {})) });
    }
  };
}`;

describe('the seated diorama in a headset browser', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  test('the table shows in depth, recentres, takes a pinch through it, scales about its anchor and stands in the room in AR', { timeout: 600_000 }, async () => {
    // Field 30 with Fei walked into the room (as xr-game-smoke.sh does).
    const state = join(SHOTS, 'xr-diorama-field30.state');
    execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
      input: [{ cmd: 'enter_field', map: 30 }, { cmd: 'frames', count: 400 }, { cmd: 'press', buttons: ['down'], frames: 60 },
        { cmd: 'frames', count: 30 }, { cmd: 'state_save', path: state }].map((c) => JSON.stringify(c)).join('\n') + '\n',
    });
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST + HAND_MODEL + REQUESTS }] });
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
      assert.ok(rows.every((row) => row.every((v, i) => i % 4 !== 3 || v === 255)), 'in VR the layer is opaque (the context\'s alpha is for AR)');
      await page.screenshot({ path: join(SHOTS, 'xr-diorama.png') });
      // The room's backdrop: the layer's top left corner, above everything
      // the view shows (the clear's colour encoded, as OpenXR's is).
      const backdrop = Array.from(await page.evaluate((point) => window.xem.xr.readback([point]), [1, layer[1] - 2])).slice(0, 3);
      const lit = rows.map((row) => Array.from({ length: count }, (_, i) => i).filter((i) => [0, 1, 2].some((k) => Math.abs(row[4 * i + k] - backdrop[k]) > 3)).length / count);
      console.log(`rows covered by the table (the backdrop ${backdrop}): ${lit.map((l) => l.toFixed(2))}`);
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

      // Two-hand scaling, the game stopped and the head facing the table:
      // two whole-hand pinches (the hand model's gather pose) spreading from
      // 20 to 50 cm apart grow the diorama live about its anchor (the
      // camera's target stays where it stands in the room within a
      // millimetre while the table's scale grows), a key beside the pane
      // keeps its width, and the scale shown last becomes the setting once,
      // as the hands let go; nothing else happens.
      await page.evaluate(() => window.xem.setRunning(false));
      await page.evaluate(() => window.xrDevice.quaternion.set(0, 0, 0, 1));
      await frames(6);
      const scaling = { pauses: (await status(page)).xr.pauses, pinches: (await status(page)).xr.counts.pinch, revision: (await status(page)).settingsRevision };
      const at = (x) => page.evaluate((x) => {
        window.xrDevice.hands.left.position.set(-x, 1.4, -0.3);
        window.xrDevice.hands.right.position.set(x, 1.4, -0.3);
      }, x);
      const shape = (name) => page.evaluate((name) => {
        window.handModel.set(window.xrDevice, 'left', name);
        window.handModel.set(window.xrDevice, 'right', name);
      }, name);
      await at(0.1);
      await frames(3);
      await shape('gather');
      await frames(3);
      const seen = [];
      for (let i = 1; i <= 10; i++) {
        await at(0.1 + 0.015 * i);
        await frames(2);
        seen.push((await status(page)).xr);
      }
      await shape('open');
      await frames(4);
      info = await status(page);
      const anchors = seen.map((s) => s.space.anchor);
      const anchorMoved = Math.max(...anchors.map((a) => Math.hypot(...a.map((v, i) => v - anchors[0][i]))));
      const tables = seen.map((s) => s.space.scale);
      console.log(`scaling: shown ${seen.map((s) => s.dioramaScale.toFixed(3))}, the table ${(tables[tables.length - 1] / tables[0]).toFixed(3)} times, `
        + `the anchor within ${(anchorMoved * 1000).toFixed(4)} mm, keys ${[...new Set(seen.map((s) => s.keyWidth))]} m; committed ${(await page.evaluate(() => window.xem.settings.get())).settings.graphics.xr.diorama_scale}%`);
      assert.ok(seen.every((s) => s.scaling && s.diorama), `scaling the diorama shown: ${JSON.stringify(seen.map((s) => [s.scaling, s.diorama]))}`);
      assert.ok(tables[tables.length - 1] / tables[0] > 2 && Math.abs(seen[seen.length - 1].dioramaScale - 2.52) < 0.1, `the table grew live: ${tables}`);
      assert.ok(anchorMoved < 0.001, `the anchor stays put: ${anchorMoved} m`);
      assert.equal(new Set(seen.map((s) => s.keyWidth)).size, 1, 'a key beside the pane keeps its width');
      const committed = (await page.evaluate(() => window.xem.settings.get())).settings.graphics.xr.diorama_scale;
      assert.equal(committed, Math.round(seen[seen.length - 1].dioramaScale * 100), 'the scale shown last is the setting');
      assert.equal(info.settingsRevision, scaling.revision + 1, 'committed once');
      assert.equal(info.xr.scaling, false);
      assert.deepEqual([info.xr.pauses, info.xr.counts.pinch], [scaling.pauses, scaling.pinches], 'no pause and no pinch');
      // The automation's frames at the new scale end at the same RAM.
      await page.evaluate(load, base64);
      await page.evaluate(() => window.xem.runFrames(120));
      assert.equal(await page.evaluate(() => window.xem.memoryHash()), flatHash, 'the diorama at 252% changes nothing in the game');

      // Immersive AR, where the browser offers it (IWER's Quest 3 does):
      // asked for with the same optional features as VR (nothing AR-specific),
      // its layer asked for with alpha (VR's without), its blend mode
      // alpha-blend, the layer transparent where VR showed the room's
      // backdrop or the game's clear on the wall behind the table, and drawn
      // where it showed the diorama. Each mode is read in a session begun
      // afresh at one game frame (the game stopped, the head where the
      // sessions begin), so both place the same table at once (a diorama
      // takes its scale's measure as it begins); at the default scale (as
      // Reset diorama puts it) the wall shows above the scene, the clear
      // black in field 30.
      const scaled = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      const defaultScale = await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, xr: { ...g.xr, diorama_scale: 100 } } }), scaled);
      assert.equal(defaultScale.ok, true, JSON.stringify(defaultScale));
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => window.xem.status().xr.diorama && window.xem.status().xr.share === 0, null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.setRunning(false));
      await page.evaluate((p) => { window.xrDevice.position.set(...p); window.xrDevice.quaternion.set(0, 0, 0, 1); }, origin);
      const grid = [];
      for (let y = 0; y < 30; y++) for (let x = 0; x < 40; x++) grid.push([Math.floor((x + 0.5) / 40 * layer[0]), Math.floor((y + 0.5) / 30 * layer[1])]);
      // The session running ended, one of `mode` begun by its button, once
      // the diorama shows in it again (the canvas's frames between the
      // sessions show the screen): the layer's bytes at the grid's points,
      // where the table lies and the head.
      const fresh = async (button, mode) => {
        await page.evaluate(() => window.xem.xr.exit());
        await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 60_000 });
        assert.equal(await page.isVisible(button), true, `${button} is offered`);
        await page.click(button);
        await page.waitForFunction((mode) => window.xem.status().page.xr.mode === mode && window.xem.status().page.xr.frames > 10, mode, { timeout: 60_000 });
        await page.waitForFunction(() => window.xem.status().xr.diorama && window.xem.status().xr.share === 0, null, { timeout: 60_000 });
        await frames(6);
        const { xr: { space }, page: { xr: { viewer } } } = await status(page);
        return { bytes: await page.evaluate((points) => window.xem.xr.readback(points), grid), space, viewer };
      };
      const inVr = await fresh('#vr', 'immersive-vr');
      const vr = inVr.bytes;
      assert.ok(vr.every((v, i) => i % 4 !== 3 || v === 255), 'in VR the layer is opaque');
      const inAr = await fresh('#ar', 'immersive-ar');
      const ar = inAr.bytes;
      await page.screenshot({ path: join(SHOTS, 'xr-diorama-ar.png') });
      info = await status(page);
      assert.equal(info.page.xr.blend, 'alpha-blend', 'IWER\'s Quest 3 blends AR by alpha');
      assert.equal(info.xr.room, true, 'the room shows around what the frames draw');
      assert.deepEqual([inAr.space, inAr.viewer], [inVr.space, inVr.viewer], 'the same table seen from the same head');
      const requests = await page.evaluate(() => window.xrRequests);
      assert.deepEqual(requests.map((r) => r.mode), ['immersive-vr', 'immersive-vr', 'immersive-ar']);
      for (const r of requests) {
        assert.equal(r.init.requiredFeatures, undefined, `${r.mode}: no feature required`);
        assert.deepEqual(r.init.optionalFeatures, ['local-floor', 'hand-tracking'], `${r.mode}: nothing AR-specific asked for`);
      }
      const layers = await page.evaluate(() => window.xrLayers);
      assert.deepEqual(layers.map((l) => [l.mode, l.init.alpha]), [['immersive-vr', false], ['immersive-vr', false], ['immersive-ar', true]],
        `VR's layers opaque, AR's with alpha: ${JSON.stringify(layers)}`);
      // The room's backdrop in VR is the real room in AR: transparent black;
      // so is the game's clear on the wall behind the table (black in VR:
      // the clear, and any black texel the scene draws, which AR draws);
      // what VR drew over them is drawn.
      const pixel = (bytes, i) => Array.from(bytes.slice(4 * i, 4 * i + 4));
      const indices = grid.map((_, i) => i);
      const around = indices.filter((i) => pixel(vr, i).slice(0, 3).every((v, k) => v === backdrop[k]));
      const black = indices.filter((i) => pixel(vr, i).slice(0, 3).every((v) => v === 0));
      const drawn = indices.filter((i) => !around.includes(i) && !black.includes(i));
      const clear = around.filter((i) => pixel(ar, i).every((v) => v === 0)).length;
      const behind = black.filter((i) => pixel(ar, i).every((v) => v === 0)).length;
      const covered = drawn.filter((i) => pixel(ar, i)[3] > 0).length;
      const opaque = drawn.filter((i) => pixel(ar, i)[3] === 255);
      const alike = opaque.filter((i) => pixel(ar, i).slice(0, 3).every((v, k) => Math.abs(v - pixel(vr, i)[k]) <= 4)).length;
      const off = opaque.map((i) => Math.max(...[0, 1, 2].map((k) => Math.abs(pixel(ar, i)[k] - pixel(vr, i)[k])))).sort((a, b) => a - b);
      console.log(`immersive AR: of ${grid.length} points, ${around.length} showed the room's backdrop ${backdrop} in VR (${clear} transparent in AR), `
        + `${black.length} black (${behind} transparent in AR: the wall's clear), `
        + `${drawn.length} the diorama, its pane or the screen (${covered} drawn in AR, ${opaque.length} opaque, ${alike} of them as in VR; `
        + `levels off: median ${off[off.length >> 1]}, p90 ${off[Math.floor(off.length * 0.9)]}); the table ${JSON.stringify(inAr.space)} from the head ${JSON.stringify(inAr.viewer)}; `
        + `the room's points drawn in AR (point, VR, AR): ${JSON.stringify(around.filter((i) => pixel(ar, i)[3] > 0).slice(0, 6).map((i) => [grid[i], pixel(vr, i), pixel(ar, i)]))}; `
        + `black points drawn in AR: ${JSON.stringify(black.filter((i) => pixel(ar, i)[3] > 0).slice(0, 6).map((i) => [grid[i], pixel(ar, i)]))}; `
        + `opaque points unlike VR (point, VR, AR): ${JSON.stringify(opaque.filter((i) => pixel(ar, i).slice(0, 3).some((v, k) => Math.abs(v - pixel(vr, i)[k]) > 4)).map((i) => [grid[i], pixel(vr, i), pixel(ar, i)]))}`);
      assert.ok(around.length >= grid.length * 0.2 && drawn.length >= grid.length * 0.1, `the room and the table both in view: ${around.length}, ${drawn.length}`);
      assert.ok(clear >= around.length * 0.98, `transparent around the table: ${clear} of ${around.length}`);
      assert.ok(black.length >= grid.length * 0.05 && behind >= black.length * 0.85, `the room behind the table where VR showed the wall's clear: ${behind} of ${black.length}`);
      assert.ok(covered >= drawn.length * 0.98, `the diorama drawn: ${covered} of ${drawn.length}`);
      assert.ok(opaque.length > 0 && alike >= opaque.length * 0.95, `opaque pixels as VR shows them: ${alike} of ${opaque.length}`);
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 60_000 });
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('a battle\'s table stands still while its camera moves and the world map\'s keeps its target, both in depth', { timeout: 600_000 }, async () => {
    // The debug battle on foot with its turns running (Start on the
    // selector's defaults, as xr-game-smoke.sh's battles) and the world
    // map (boot mode 3), each saved by xem-headless.
    const scenes = [
      ['battle', [{ cmd: 'frames', count: 60 }, { cmd: 'write', path: 'mode_battle_standalone', value: 1 }, { cmd: 'boot_mode', mode: 2 },
        { cmd: 'frames', count: 900 }, { cmd: 'press', buttons: ['start'], frames: 1 }, { cmd: 'frames', count: 600 }]],
      ['world-map', [{ cmd: 'boot_mode', mode: 3 }, { cmd: 'frames', count: 900 }]],
    ];
    const states = new Map(scenes.map(([name, commands]) => {
      const path = join(SHOTS, `xr-diorama-${name}.state`);
      execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
        input: [...commands, { cmd: 'state_save', path }].map((c) => JSON.stringify(c)).join('\n') + '\n',
      });
      return [name, readFileSync(path).toString('base64')];
    }));
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST }] });
    try {
      const page = await browser.page('?xrScale=0.25');
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      const load = (base64) => page.evaluate((base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0))), base64);
      // The same frames outside the headset first, for the state check.
      const flat = new Map();
      for (const [name, base64] of states) {
        await load(base64);
        await page.evaluate(() => window.xem.runFrames(120));
        flat.set(name, await page.evaluate(() => window.xem.memoryHash()));
      }
      const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      const applied = await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, xr: { ...g.xr, view: 'Diorama' } } }), graphics);
      assert.equal(applied.ok, true, JSON.stringify(applied));
      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().page.xr.frames > 20, null, { timeout: 60_000 });
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      // Rows of the left view across the table, near first (as above).
      const heights = Array.from({ length: 12 }, (_, i) => 0.04 + i * 0.04);
      const count = 200;
      const points = heights.flatMap((v) => Array.from({ length: count }, (_, i) => [Math.floor((0.05 + 0.4 * i / count) * layer[0]), Math.floor(v * layer[1])]));
      const read = async () => {
        const bytes = await page.evaluate((points) => window.xem.xr.readback(points), points);
        return heights.map((_, r) => bytes.slice(r * count * 4, (r + 1) * count * 4));
      };
      const origin = await page.evaluate(() => { const p = window.xrDevice.position; return [p.x, p.y, p.z]; });
      for (const [name] of scenes) {
        // The automation's frames in the headset end where the flat ones did.
        await load(states.get(name));
        await page.evaluate(() => window.xem.runFrames(120));
        assert.equal(await page.evaluate(() => window.xem.memoryHash()), flat.get(name), `${name}: the diorama changes nothing in the game`);
        // From the state, once the diorama shows, the game held: in depth,
        // the head 10 cm to the right shifts the rows across the table by
        // their depth, unlike a flat picture's one shift (the terrain's
        // hills and the battle's combatants stand nearer than the rows
        // below them, so not nearest first).
        await load(states.get(name));
        await page.evaluate(() => window.xem.setRunning(true));
        await page.waitForFunction(() => window.xem.status().xr.diorama && window.xem.status().xr.share === 0, null, { timeout: 60_000 });
        await page.evaluate(() => window.xem.setRunning(false));
        await frames(4);
        const rows = await read();
        await page.screenshot({ path: join(SHOTS, `xr-diorama-${name}.png`) });
        await page.evaluate(([x, y, z]) => window.xrDevice.position.set(x + 0.1, y, z), origin);
        await frames(6);
        const after = await read();
        const shifts = rows.map((row, r) => shift(row, after[r]));
        console.log(`${name}: parallax for 10 cm sideways, by row (near first): ${JSON.stringify(shifts)} points`);
        const matched = shifts.filter((s) => s.matched);
        const spread = Math.max(...matched.map((s) => s.shift)) - Math.min(...matched.map((s) => s.shift));
        assert.ok(matched.length >= 4, `${name}: rows the move matches: ${JSON.stringify(shifts)}`);
        assert.ok(spread >= 5, `${name}: the rows shift by their depth: ${JSON.stringify(shifts)}`);
        await page.evaluate((p) => window.xrDevice.position.set(...p), origin);
        await frames(6);
        // Played on: the battle's party attacking (Cross every 30 game
        // frames: its camera moves, orbits and cuts), the world map's
        // vehicle (the one boot mode 3 begins in) driven on by its
        // throttle, Triangle, climbing as it goes; the table where each
        // frame put it.
        await page.evaluate(() => window.xem.setRunning(true));
        await page.evaluate((name) => {
          if (name === 'world-map') {
            window.xem.command({ cmd: 'press', buttons: ['triangle'], frames: 400 });
            return;
          }
          let next = 0;
          window.presser = setInterval(() => {
            const at = window.xem.status().loop.gameFrames;
            if (at >= next) {
              window.xem.command({ cmd: 'press', buttons: ['cross'], frames: 2 });
              next = at + 30;
            }
          }, 20);
        }, name);
        const seen = [];
        for (let i = 0; i < 40; i++) {
          await frames(4);
          const info = await status(page);
          seen.push({ ...info.xr, gameFrames: info.loop.gameFrames });
        }
        await page.evaluate(() => { clearInterval(window.presser); window.xem.setRunning(false); });
        await frames(4);
        await page.screenshot({ path: join(SHOTS, `xr-diorama-${name}-played.png`) });
        const shown = seen.filter((s) => s.diorama && s.share === 0).length;
        const spaces = seen.filter((s) => s.space).map((s) => s.space);
        const tables = spaces.map((s) => s.roomFromWorld);
        const moved = Math.max(...tables.map((m) => Math.max(...m.map((v, i) => Math.abs(v - tables[0][i])))));
        const targets = new Set(spaces.map((s) => s.target.join(',')));
        // Where the camera's target stands in the room, across the floor.
        const across = Math.max(...spaces.map((s) => Math.hypot(s.anchor[0] - spaces[0].anchor[0], s.anchor[2] - spaces[0].anchor[2])));
        console.log(`${name}: the diorama alone in ${shown} of ${seen.length} samples over ${seen[seen.length - 1].gameFrames - seen[0].gameFrames} game frames, `
          + `the room from the world within ${moved} of the first, the camera's target at ${targets.size} places (${[...targets].slice(0, 4).join('; ')}..) `
          + `within ${(across * 1000).toFixed(4)} mm of its first place across the room, ${spaces[0]?.scale} m a unit`);
        assert.equal(shown, seen.length, `${name}: the diorama shows throughout`);
        if (name === 'battle') {
          assert.ok(moved < 1e-5, `the battle's table stands still: ${moved}`);
          assert.ok(targets.size >= 5, `the battle's camera moves while its table stands: ${targets.size} targets`);
        } else {
          // Its frame is the target's: (0, its height, 0), the map moving
          // under it; the table follows its height or the ground under it.
          assert.ok([...targets].every((t) => { const [x, , z] = t.split(',').map(Number); return x === 0 && z === 0; }) && targets.size > 1,
            `the world map's camera's target, the vehicle climbing: ${[...targets]}`);
          assert.ok(across < 0.001, `the world map's table stays on its camera's target across the room: ${across} m`);
        }
      }
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 60_000 });
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('either hand\'s grabs work over it, the head tracked in depth while they pause it, inspection gives it back at once as it was, the field\'s menu eases to the screen and back, prayer opens settings beside its pane, and the headset view switched in the session cuts to the screen and eases back', { timeout: 600_000 }, async () => {
    // Field 30 with Fei walked into the room (as the first test), IWER's
    // hands taking the model's poses.
    const state = join(SHOTS, 'xr-diorama-hands.state');
    execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
      input: [{ cmd: 'enter_field', map: 30 }, { cmd: 'frames', count: 400 }, { cmd: 'press', buttons: ['down'], frames: 60 },
        { cmd: 'frames', count: 30 }, { cmd: 'state_save', path: state }].map((c) => JSON.stringify(c)).join('\n') + '\n',
    });
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST + HAND_MODEL + RECORDER }] });
    try {
      const page = await browser.page('?xrScale=0.25');
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      const base64 = readFileSync(state).toString('base64');
      await page.evaluate((base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0))), base64);
      // Each session frame's presentation from now (RECORDER), and the
      // frames recorded.
      const record = () => page.evaluate(() => { window.xrTimeline = []; window.xrRecording = true; });
      const recorded = () => page.evaluate(() => { window.xrRecording = false; return window.xrTimeline; });
      // The diorama at 150%, a scale of the player's own.
      const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      const applied = await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, xr: { ...g.xr, view: 'Diorama', diorama_scale: 150 } } }), graphics);
      assert.equal(applied.ok, true, JSON.stringify(applied));
      await page.evaluate(() => {
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
      const shapes = async (name, n = 3) => {
        await page.evaluate((name) => {
          window.handModel.set(window.xrDevice, 'left', name);
          window.handModel.set(window.xrDevice, 'right', name);
        }, name);
        await frames(n);
      };
      const turn = (hand, q) => page.evaluate(({ hand, q }) => window.xrDevice.hands[hand].quaternion.set(...q), { hand, q });
      // Palms down (as the model is), up and sideways.
      const [DOWN, UP, SIDE] = [[0, 0, 0, 1], [0, 0, 1, 0], [0, 0, Math.SQRT1_2, Math.SQRT1_2]];
      // A five-finger grab closing and opening again, the palm turned `q`.
      const grab = async (hand, q = DOWN) => {
        await turn(hand, q);
        await frames(3);
        for (const name of ['open', 'half', 'fist', 'open']) await shape(hand, name);
        await turn(hand, DOWN);
        await frames(3);
      };
      const xr = async () => (await status(page)).xr;
      const gameFrames = async () => (await status(page)).loop.gameFrames;
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => window.xem.status().xr.diorama && window.xem.status().xr.share === 0, null, { timeout: 60_000 });
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      // Rows of the left view across the table, near first (as above).
      const heights = Array.from({ length: 12 }, (_, i) => 0.04 + i * 0.04);
      const count = 200;
      const points = heights.flatMap((v) => Array.from({ length: count }, (_, i) => [Math.floor((0.05 + 0.4 * i / count) * layer[0]), Math.floor(v * layer[1])]));
      const read = async () => {
        const bytes = await page.evaluate((points) => window.xem.xr.readback(points), points);
        return heights.map((_, r) => bytes.slice(r * count * 4, (r + 1) * count * 4));
      };

      // The left hand's overhand grab: the player's Pause, the diorama on.
      let before = (await xr()).counts;
      await grab('left');
      let info = await status(page);
      assert.equal(info.xr.pauses.player, true, 'the left hand\'s grab paused');
      assert.equal(info.xr.diorama, true, 'the diorama shows through the pause');
      // While it holds the game, the head moved 10 cm to the right: the
      // views follow it, the table in depth (rows across it shift by their
      // depth), the game's clock standing still.
      const held = info.loop.gameFrames;
      const sessionFrames = info.page.xr.frames;
      const viewer = info.page.xr.viewer;
      const rows = await read();
      const origin = await page.evaluate(() => { const p = window.xrDevice.position; return [p.x, p.y, p.z]; });
      await page.evaluate(([x, y, z]) => window.xrDevice.position.set(x + 0.1, y, z), origin);
      await frames(8);
      const moved = await read();
      info = await status(page);
      const shifts = rows.map((row, r) => shift(row, moved[r]));
      const matched = shifts.filter((s) => s.matched);
      const spread = matched.length ? Math.max(...matched.map((s) => s.shift)) - Math.min(...matched.map((s) => s.shift)) : 0;
      console.log(`the player's Pause: game frame ${held} to ${info.loop.gameFrames}, session frames ${sessionFrames} to ${info.page.xr.frames}, `
        + `the head ${viewer} to ${info.page.xr.viewer}; parallax for 10 cm by row (near first) ${JSON.stringify(shifts)}`);
      assert.equal(info.loop.gameFrames, held, 'the game\'s clock stands still under the player\'s Pause');
      assert.notDeepEqual(info.page.xr.viewer, viewer, 'the head is tracked');
      assert.ok(matched.length >= 4 && spread >= 5, `the paused diorama in depth: ${JSON.stringify(shifts)}`);
      await page.evaluate((p) => window.xrDevice.position.set(...p), origin);
      await frames(6);
      // The left hand's grab again: resumed.
      await grab('left');
      assert.equal((await xr()).pauses.player, false, 'the left hand\'s grab resumed');
      const running = await gameFrames();
      await frames(10);
      assert.ok(await gameFrames() > running, 'the game runs again');
      // Both hands closing together: one toggle; the right hand's resumes.
      for (const name of ['half', 'fist', 'open']) await shapes(name);
      info = await status(page);
      assert.equal(info.xr.pauses.player, true, 'both hands\' grab paused');
      assert.equal(info.xr.counts.pause, before.pause + 3, `both hands closing together toggle once: ${JSON.stringify(info.xr.counts)}`);
      await grab('right');
      assert.equal((await xr()).pauses.player, false, 'the right hand\'s grab resumed');
      // A sideways grab, and a fist closed sideways then turned palm down
      // and up while closed: nothing.
      before = (await xr()).counts;
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
      info = await status(page);
      assert.deepEqual(info.xr.counts, before, `a sideways grab and a turned fist toggle nothing: ${JSON.stringify(info.xr.counts)}`);
      assert.equal(info.xr.pauses.player, false);
      assert.equal(info.xr.diorama, true, 'the diorama shows on');

      // The left hand's underhand grab looks from within the scene; again,
      // and the diorama is back as it was (its scale, where it stood: the
      // camera still): each way at once, on the first session frame after
      // the grab toggles inspection (every session frame recorded).
      const was = info.xr.space;
      await record();
      await grab('left', UP);
      info = await status(page);
      assert.equal(info.xr.counts.inspect, before.inspect + 1, 'an underhand grab');
      assert.deepEqual([info.xr.inspecting, info.xr.within, info.xr.diorama], [true, true, false], `inspection looks from within the scene: ${JSON.stringify(info.xr)}`);
      await grab('left', UP);
      await frames(2);
      info = await status(page);
      const looked = await recorded();
      const on = looked.findIndex((f) => f.inspecting);
      const off = looked.findIndex((f, i) => i > on && !f.inspecting);
      const around = (i) => JSON.stringify(looked.slice(Math.max(i - 1, 0), i + 2));
      console.log(`inspection: on at session frame ${looked[on]?.frame} ${around(on)}, off at ${looked[off]?.frame} ${around(off)}; `
        + `the diorama before ${JSON.stringify(was)}, after ${JSON.stringify(info.xr.space)}`);
      assert.ok(on > 0 && looked.slice(0, on).every((f) => f.diorama && f.share === 0), `the diorama until inspection: ${around(on)}`);
      assert.ok(looked[on].within && !looked[on].diorama, `from within from inspection's first session frame: ${around(on)}`);
      assert.ok(off > on && looked.slice(on, off).every((f) => f.within && !f.diorama), 'from within throughout');
      assert.ok(looked[off].diorama && looked[off].share === 0, `the diorama back alone on the first session frame after inspection: ${around(off)}`);
      assert.ok(looked.slice(off).every((f) => !f.inspecting && f.diorama && f.share === 0), 'and on');
      assert.deepEqual([info.xr.inspecting, info.xr.diorama, info.xr.share], [false, true, 0], 'the diorama back');
      assert.equal(info.xr.dioramaScale, 1.5, 'the player\'s scale');
      assert.equal(info.xr.space.scale, was.scale, 'at the scale it had');
      const stood = Math.max(...info.xr.space.roomFromWorld.map((v, i) => Math.abs(v - was.roomFromWorld[i])));
      assert.ok(stood < 1e-4, `standing where it stood: ${stood}`);

      // The field's menu (the automation's Menu, then Cancel): the view eases
      // from the diorama to the screen over the 0.3 s switch, shows the menu
      // on the screen alone, and eases back to the diorama once it closes.
      await record();
      await page.evaluate(() => window.xem.command({ cmd: 'act', actions: ['Menu'], frames: 2 }));
      // Cancel once the menu's commands take the input (opening, it ignores it).
      await page.waitForFunction(() => {
        const context = window.xem.command({ cmd: 'context' });
        return context.kind === 'menu' && context.commands && window.xrTimeline.filter((f) => f.share === 1 && !f.diorama).length >= 10;
      }, null, { timeout: 180_000 });
      await page.evaluate(() => window.xem.command({ cmd: 'act', actions: ['Cancel'], frames: 2 }));
      await page.waitForFunction(() => {
        const t = window.xrTimeline;
        const screen = t.findLastIndex((f) => f.share === 1);
        return screen >= 0 && t.length - screen > 10 && t.slice(-10).every((f) => f.diorama && f.share === 0);
      }, null, { timeout: 180_000 });
      const menu = await recorded();
      const shares = menu.map((f) => f.share);
      const first = shares.findIndex((s) => s === 1);
      const last = shares.findLastIndex((s) => s === 1);
      const out = menu.slice(0, first).filter((f) => f.share > 0);
      const back = menu.slice(last + 1).filter((f) => f.share < 1 && f.share > 0);
      console.log(`the menu over the diorama: ${menu.length} session frames, eased out over ${JSON.stringify(out.map((f) => f.share))}, `
        + `the screen alone in ${last - first + 1}, back over ${JSON.stringify(back.map((f) => f.share))}; the settings panel `
        + `${menu.filter((f) => panelPlace(f) === 'beside').length} frames beside the pane, ${menu.filter((f) => panelPlace(f) === 'below').length} below the screen`);
      assert.ok(menu[0].diorama && menu[0].share === 0, 'the diorama before the menu');
      assert.ok(out.length >= 1 && back.length >= 1, `eased each way, not cut: ${JSON.stringify(shares)}`);
      assert.ok(menu.slice(first, last + 1).every((f) => f.share === 1 && !f.diorama), 'the menu on the screen alone');
      assert.deepEqual(jumps(menu), [], 'the screen\'s share moves by no more than the frames\' time over the 0.3 s switch');
      assert.deepEqual(misplaced(menu), [], 'the settings panel beside the pane while the diorama shows, below the screen while the menu shows on it');
      assert.ok(menu.at(-1).diorama && menu.at(-1).share === 0, 'the diorama again');

      // Prayer hands: the settings panel beside the pane (its inner edge
      // beyond the keys beside the pane's right edge, standing on the pane's
      // lower edge, upright, facing the seat: the pane in view), its own
      // pause holding the game, the diorama on; again, closed.
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
      info = await status(page);
      const opened = info.loop.gameFrames;
      await frames(6);
      const panel = info.xr.panel;
      const pane = info.xr.space?.pane;
      const seat = info.xr.seat;
      const [paneWidth, paneHeight] = info.xr.screen.size;
      // How far round from the pane's right edge the panel's inner edge
      // stands (degrees), how high its lower edge stands over the pane's
      // (metres), and how squarely it faces the seat.
      const right = turned(panel.orientation, [1, 0, 0]);
      const normal = turned(panel.orientation, [0, 0, 1]);
      const inner = panel.position.map((v, i) => v - right[i] * panel.size[0] / 2);
      const edge = [pane[0] + Math.cos(seat.yaw) * paneWidth / 2, pane[1], pane[2] - Math.sin(seat.yaw) * paneWidth / 2];
      const offPane = roundFrom(seat, inner) - roundFrom(seat, edge);
      const above = (panel.position[1] - panel.size[1] / 2) - (pane[1] - paneHeight / 2);
      const toSeat = [seat.position[0] - panel.position[0], seat.position[2] - panel.position[2]];
      const facing = (normal[0] * toSeat[0] + normal[2] * toSeat[1]) / Math.hypot(...toSeat);
      console.log(`prayer: settings ${info.xr.settings}, the panel at ${JSON.stringify(panel)}, the pane at ${JSON.stringify(pane)} (${paneWidth} by ${paneHeight} m); `
        + `the panel's inner edge ${offPane.toFixed(2)} degrees round from the pane's right edge, its lower edge ${above.toFixed(5)} m over the pane's, facing the seat ${facing.toFixed(6)}`);
      assert.deepEqual([info.xr.settings, info.xr.pauses.settings, info.xr.diorama], [true, true, true], `settings opened with the diorama: ${JSON.stringify(info.xr)}`);
      assert.equal(await page.evaluate(() => window.xem.panel.visible()), true, 'the panel shows in the session');
      assert.equal(await gameFrames(), opened, 'the settings\' pause holds the game');
      assert.ok(offPane > 9 && Math.abs(above) < 1e-3 && facing > 0.9999 && Math.abs(normal[1]) < 1e-4,
        `the panel stands beside the pane, beyond its keys, facing the seat: ${JSON.stringify(panel)}, the pane ${pane}`);
      await prayer();
      info = await status(page);
      assert.deepEqual([info.xr.settings, info.xr.pauses.player], [false, false], 'closed again, the player\'s Pause never set');

      // Graphics' headset view switched in the session (as the panel's
      // Headset card switches it, through the shared settings service): to
      // the screen, shown alone from the first session frame after it; to
      // the diorama again, eased in from the screen over the 0.3 s switch
      // (once the trace carries the camera again) to the diorama alone.
      await page.waitForFunction(() => window.xem.status().xr.diorama && window.xem.status().xr.share === 0, null, { timeout: 60_000 });
      const current = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      const view = (graphics, view) => page.evaluate(({ graphics, view }) => {
        const applied = window.xem.settings.apply({ Graphics: { ...graphics, xr: { ...graphics.xr, view } } });
        return { applied, at: window.xrTimeline.length };
      }, { graphics, view });
      await record();
      await frames(10);
      const toScreen = await view(current, 'Screen');
      await frames(30);
      const toDiorama = await view(current, 'Diorama');
      await page.waitForFunction((at) => {
        const t = window.xrTimeline;
        return t.length > at + 10 && t.slice(-10).every((f) => f.diorama && f.share === 0);
      }, toDiorama.at, { timeout: 120_000 });
      const switching = await recorded();
      const onScreen = switching.slice(toScreen.at, toDiorama.at);
      const backIn = switching.slice(toDiorama.at);
      const easedIn = backIn.filter((f) => phase(f) === 'E').map((f) => f.share);
      console.log(`the headset view switched: to the screen at session frame ${switching[toScreen.at]?.frame} ${JSON.stringify(switching.slice(toScreen.at - 1, toScreen.at + 1))}, `
        + `the screen alone for ${onScreen.length}; to the diorama at ${switching[toDiorama.at]?.frame}, eased in over ${JSON.stringify(easedIn)}`);
      assert.deepEqual([toScreen.applied.ok, toDiorama.applied.ok], [true, true], `both switches acknowledged: ${JSON.stringify([toScreen.applied, toDiorama.applied])}`);
      assert.ok(toScreen.at > 0 && switching.slice(0, toScreen.at).every((f) => phase(f) === 'D'), 'the diorama alone before the switch');
      assert.ok(onScreen.length >= 20 && onScreen.every((f) => phase(f) === 'S' && !f.within), `the screen alone from the first session frame after the switch: ${JSON.stringify(onScreen.slice(0, 3))}`);
      assert.ok(backIn[0].share === 1 && easedIn.length >= 1 && phase(backIn.at(-1)) === 'D', `the diorama eased in from the screen: ${JSON.stringify(backIn.map((f) => f.share))}`);
      assert.deepEqual(jumps(backIn), [], 'the share eases in by no more than the frames\' time over the 0.3 s switch');
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 60_000 });
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('a story movie eases out of the field\'s diorama to the screen and back into it', { timeout: 600_000 }, async () => {
    // Map 25's scene, its dialogue auto-advanced to game frame 5080 by
    // xem-headless (as xr-game-smoke.sh's diorama-movie run starts): the
    // player's own play from there reaches its story movie about 350 game
    // frames on, 4 280 frames long, and map 25 again after it.
    const state = join(SHOTS, 'xr-diorama-movie25.state');
    execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
      input: [{ cmd: 'enter_field', map: 25 }, { cmd: 'auto_advance', dwell: 1 }, { cmd: 'frames', count: 5080 }, { cmd: 'auto_advance', dwell: 0 },
        { cmd: 'state_save', path: state }].map((c) => JSON.stringify(c)).join('\n') + '\n',
    });
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST + RECORDER }] });
    try {
      const page = await browser.page('?xrScale=0.25');
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      const base64 = readFileSync(state).toString('base64');
      await page.evaluate((base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0))), base64);
      const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      const applied = await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, xr: { ...g.xr, view: 'Diorama' } } }), graphics);
      assert.equal(applied.ok, true, JSON.stringify(applied));
      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().page.xr.frames > 20, null, { timeout: 60_000 });
      // Every session frame from the state on (RECORDER), the game running,
      // until map 25's diorama shows alone again after the movie.
      await page.evaluate(() => { window.xrTimeline = []; window.xrRecording = true; });
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => {
        const t = window.xrTimeline;
        const last = t.findLastIndex((f) => f.movie);
        return last >= 0 && t.length - last > 20 && t.slice(-10).every((f) => f.diorama && f.share === 0);
      }, null, { timeout: 480_000, polling: 500 });
      const timeline = await page.evaluate(() => { window.xrRecording = false; return window.xrTimeline; });
      await page.evaluate(() => window.xem.setRunning(false));
      // Map 25's diorama, eased out over the movie's first rebuilt frames,
      // the movie on the screen alone, eased back into map 25's diorama.
      const movie = timeline.flatMap((f, i) => (f.movie ? [i] : []));
      const [first, last] = [movie[0], movie.at(-1)];
      const before = timeline.slice(0, first).findLastIndex((f) => phase(f) === 'D');
      const out = timeline.slice(before + 1, first + 30).filter((f) => phase(f) === 'E').map((f) => f.share);
      const alone = movie.filter((i) => phase(timeline[i]) === 'S').length;
      const after = timeline.findIndex((f, i) => i > last && phase(f) === 'D');
      const back = timeline.slice(last + 1, after).filter((f) => phase(f) === 'E').map((f) => f.share);
      const games = (i) => timeline[i]?.gameFrames;
      console.log(`map 25's movie: ${timeline.length} session frames; the diorama alone to session frame ${timeline[before]?.frame} (game frame ${games(before)}), `
        + `eased out over ${JSON.stringify(out)}, ${movie.length} rebuilt movie frames (game frames ${games(first)}-${games(last)}), ${alone} of them on the screen alone, `
        + `eased back over ${JSON.stringify(back)} to the diorama alone from session frame ${timeline[after]?.frame} (game frame ${games(after)}); the settings panel `
        + `${timeline.filter((f) => panelPlace(f) === 'beside').length} frames beside the pane, ${timeline.filter((f) => panelPlace(f) === 'below').length} below the screen`);
      assert.ok(before >= 0 && first > before, 'map 25\'s diorama before the movie');
      assert.ok(out.length >= 1, `eased out of the diorama: ${JSON.stringify(timeline.slice(before, first + 3).map(phase).join(''))}`);
      assert.ok(movie.length >= 100 && alone * 10 >= movie.length * 9, `the movie on the screen alone (${alone} of ${movie.length} rebuilt frames)`);
      assert.ok(after > last && back.length >= 1, `eased back into the diorama after the movie: ${JSON.stringify(timeline.slice(last, last + 12).map(phase).join(''))}`);
      assert.deepEqual(jumps(timeline), [], 'the screen\'s share moves by no more than the frames\' time over the 0.3 s switch');
      assert.deepEqual(misplaced(timeline), [], 'the settings panel beside the pane while the diorama shows, below the screen while the movie shows on it');
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 60_000 });
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
