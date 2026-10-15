// The Binaural mode's listener in a headset browser (local only: it needs
// the user's disc 1, the game module and xem-headless): Quest Browser's user
// agent with IWER's Meta Quest 3 as navigator.xr, field 15 (three emitters
// the presenter places, and its music), Binaural on, the game held while
// the head turns. The presenter's status (the `binaural` command) gives its
// head as the head's turn from the seat; on the screen and in first-person
// inspection each effect is heard where it lay with the head ahead turned
// by the head once (the head's axes, xem_core::binaural::head_axes, applied
// to that direction: a turn of the head to the right by any angle moves it
// that far to the left, elevation kept), first-person's view never takes
// the head (not twice), and the music keeps its places; inspection hears
// the screen's directions turned up by the camera's pitch (its view is
// level on the camera's heading), and the right stick's 30-degree turn
// turns them once more. The player's Recenter and IWER's recentring (its
// reference space's `reset`) re-reference what is heard to the head's turn
// from the seat as it then stands; lost tracking (the viewer's pose
// missing from the frames, as browsers report it) keeps the last head and
// what is heard with it, and nothing is NaN. In the diorama the listener is
// the controlled character, and each effect is heard where the table shows
// it from the head: the direction from the character to its emitter in the
// room (the table's matrix from the game's world) in the head's own axes,
// through Recenter and the reference's reset too; moving the head without
// turning it changes nothing heard, nor does a tilted table's turn reach the
// listener only once the game runs; and two whole-hand pinches growing the
// table from 25% to 400% (the listener taking its axes at every scale)
// change nothing heard and move nobody, nor does the scale change the
// game's frames: its RAM and its sound (the SPU's output) are the same.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HAND_MODEL, HEADLESS, IWER, NO_WEBGPU, QUEST, QUEST_BROWSER, SHOTS, USER_DISC, WEB, rotate, start, status } from './support.mjs';

const available = IWER && existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
mkdirSync(SHOTS, { recursive: true });

/** An init script after QUEST: with `window.xemTrackingLost` set, the
 *  session's frames have no viewer pose (a browser's lost tracking). */
const LOSABLE = `{
  const viewerPose = XRFrame.prototype.getViewerPose;
  XRFrame.prototype.getViewerPose = function (space) { return window.xemTrackingLost ? null : viewerPose.call(this, space); };
}`;

const degrees = (r) => (r * 180) / Math.PI;
const radians = (d) => (d * Math.PI) / 180;
/** A head turned `yaw` degrees to the right, then `pitch` up (x, y, z, w). */
function head(yaw, pitch = 0) {
  const [y, p] = [radians(-yaw) / 2, radians(pitch) / 2];
  // About +Y, then about the turned +X.
  return [Math.cos(y) * Math.sin(p), Math.sin(y) * Math.cos(p), -Math.sin(y) * Math.sin(p), Math.cos(y) * Math.cos(p)];
}
const conjugate = ([x, y, z, w]) => [-x, -y, -z, w];
/** The head's axes (right, down, forward) as the presenter takes them
 *  (xem_core::binaural::head_axes) from its turn `q`. */
const headAxes = (q) => [[1, 0, 0], [0, -1, 0], [0, 0, -1]].map((axis) => {
  const v = rotate(q, axis);
  return [v[0], -v[1], -v[2]];
});
const apply = (axes, d) => axes.map((row) => row[0] * d[0] + row[1] * d[1] + row[2] * d[2]);
const unit = (v) => v.map((c) => c / Math.hypot(...v));
/** A status direction (azimuth to the right, elevation up, degrees) as
 *  (right, down, forward). */
const toward = ({ azimuth, elevation }) => {
  const [a, e] = [radians(azimuth), radians(elevation)];
  return [Math.sin(a) * Math.cos(e), -Math.sin(e), Math.cos(a) * Math.cos(e)];
};
/** Degrees between two unit directions. */
const apart = (a, b) => degrees(2 * Math.asin(Math.min(1, Math.hypot(...a.map((c, i) => c - b[i])) / 2)));
const facing = (f) => ({ yaw: degrees(Math.atan2(f[0], f[2])), pitch: degrees(Math.atan2(-f[1], Math.hypot(f[0], f[2]))) });
/** Whether every direction in a status is a finite number (a NaN would
 *  come as null). */
const finite = (status) => [status.head, status.first_person].filter(Boolean).every((f) => Number.isFinite(f.yaw) && Number.isFinite(f.pitch))
  && status.sources.every((s) => Number.isFinite(s.azimuth) && Number.isFinite(s.elevation)
    && (!s.target || (Number.isFinite(s.target.azimuth) && Number.isFinite(s.target.elevation))));

describe('the Binaural listener in a headset browser', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  const state = join(SHOTS, 'xr-binaural-field15.state');
  const made = () => {
    if (!existsSync(state)) {
      execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
        input: [{ cmd: 'enter_field', map: 15 }, { cmd: 'frames', count: 600 }, { cmd: 'state_save', path: state }]
          .map((c) => JSON.stringify(c)).join('\n') + '\n',
      });
    }
    return readFileSync(state).toString('base64');
  };

  /** A headset page with the state loaded and Binaural on (the diorama's
   *  view when `diorama`), in a VR session once the presenter places the
   *  three emitters and four music parts or more, the game then held. */
  async function session(browser, diorama) {
    const page = await browser.page('?xrScale=0.25');
    await page.setInputFiles('#file', USER_DISC);
    await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
    await page.evaluate(() => window.xem.boot());
    await page.evaluate((base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0))), made());
    const settings = (await page.evaluate(() => window.xem.settings.get())).settings;
    assert.equal((await page.evaluate((b) => window.xem.settings.apply({ Binaural: { ...b, enabled: true } }), settings.binaural)).ok, true);
    if (diorama) {
      const g = settings.graphics;
      assert.equal((await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), { ...g, xr: { ...g.xr, view: 'Diorama' } })).ok, true);
    }
    await page.evaluate(() => window.xem.setRunning(true));
    await page.click('#vr');
    await page.waitForFunction((diorama) => {
      const s = window.xem.status();
      const b = window.xem.command({ cmd: 'binaural' });
      const kinds = (kind) => (b.sources ?? []).filter((source) => source.kind === kind && source.target).length;
      return s.page.xr.frames > 20 && b.head && kinds('effect') === 3 && kinds('music') >= 4 && (!diorama || (s.xr.diorama && s.xr.share === 0));
    }, diorama, { timeout: 120_000 });
    await page.evaluate(() => window.xem.setRunning(false));
    return page;
  }

  const frames = (page, n) => page.evaluate(async (n) => {
    const start = window.xem.status().page.xr.frames;
    while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
  }, n);
  const heard = (page) => page.evaluate(() => window.xem.command({ cmd: 'binaural' }));
  /** The head turned to `q` (and moved to `at`, metres), then what is heard. */
  const look = async (page, q, at) => {
    await page.evaluate(({ q, at }) => {
      window.xrDevice.quaternion.set(...q);
      if (at) window.xrDevice.position.set(...at);
    }, { q, at });
    await frames(page, 4);
    return heard(page);
  };
  /** Each placed effect's and music part's target, by id. */
  const targets = (status, kind) => new Map(status.sources.filter((s) => s.kind === kind && s.target).map((s) => [s.id, s.target]));
  /** Each music part's place on the stage, by id (its target follows the
   *  song's own pan as the game runs). */
  const placements = (status) => new Map(status.sources.filter((s) => s.kind === 'music').map((s) => [s.id, s.placement]));
  /** How far each effect is heard from `expected(id)` (degrees), the worst. */
  const worstOff = (status, expected) => Math.max(...[...targets(status, 'effect')].map(([id, t]) => apart(toward(t), expected(id))));

  test('the head turns what is heard once, recentring re-references it and lost tracking keeps it, on the screen and looking around', { timeout: 600_000 }, async () => {
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST + LOSABLE }] });
    try {
      const page = await session(browser, false);
      const report = [];
      /** The head's turns checked against what was heard with it ahead
       *  (`ahead`, the music's too), the presenter's head the turn itself. */
      const turns = async (ahead, what) => {
        const music = targets(ahead, 'music');
        const effects = new Map([...targets(ahead, 'effect')].map(([id, t]) => [id, toward(t)]));
        let worst = 0;
        for (const [yaw, pitch] of [[30, 0], [90, 0], [-60, 0], [0, 30], [-45, -20], [120, 35]]) {
          const q = head(yaw, pitch);
          const now = await look(page, q);
          const presented = facing(headAxes(q)[2]);
          assert.ok(Math.abs(now.head.yaw - presented.yaw) < 0.01 && Math.abs(now.head.pitch - presented.pitch) < 0.01,
            `${what}: the presenter's head is the head's turn ${JSON.stringify([yaw, pitch])}: ${JSON.stringify(now.head)}`);
          const off = worstOff(now, (id) => apply(headAxes(q), effects.get(id)));
          worst = Math.max(worst, off);
          assert.ok(off < 0.01, `${what}: turned ${yaw} right and ${pitch} up, each effect is heard turned once by the head: ${off}`);
          if (pitch === 0) {
            for (const [id, t] of targets(now, 'effect')) {
              const was = ahead.sources.find((s) => s.id === id).target;
              const shift = ((t.azimuth - was.azimuth - -yaw + 540) % 360) - 180;
              assert.ok(Math.abs(shift) < 0.01 && Math.abs(t.elevation - was.elevation) < 0.01,
                `${what}: the head ${yaw} to the right moves effect ${id} ${-yaw} in azimuth, its elevation kept: ${JSON.stringify([was, t])}`);
            }
          }
          assert.deepEqual(targets(now, 'music'), music, `${what}: the music keeps its places as the head turns`);
          assert.deepEqual(now.first_person, ahead.first_person, `${what}: the listener's view does not take the head`);
          assert.ok(finite(now), `${what}: nothing is NaN`);
        }
        report.push(`${what}: six head turns within ${worst.toFixed(4)} degrees`);
        await look(page, head(0));
      };

      // The screen, the head ahead.
      const screen = await look(page, head(0));
      assert.equal(screen.first_person, null, 'the screen: the camera\'s own view');
      await turns(screen, 'the screen');

      // The player's Recenter with the head turned 90 degrees to the right
      // and 20 up (and moved): the seat at the head facing its way, the
      // presenter's head its pitch alone, what is heard re-referenced.
      const ahead = new Map([...targets(screen, 'effect')].map(([id, t]) => [id, toward(t)]));
      await look(page, head(90, 20), [0.2, 1.6, 0.1]);
      await page.evaluate(() => window.xem.appOperation({ Action: 'Recenter' }));
      await frames(page, 4);
      let now = await heard(page);
      let xr = (await status(page)).xr;
      assert.equal(xr.recenters, 1);
      assert.ok(Math.abs(xr.seat.yaw + Math.PI / 2) < 1e-3, `the seat turned with the head: ${JSON.stringify(xr.seat)}`);
      assert.ok(Math.abs(now.head.yaw) < 0.01 && Math.abs(now.head.pitch - 20) < 0.01, `recentred, the head is ahead, pitched 20 up: ${JSON.stringify(now.head)}`);
      let off = worstOff(now, (id) => apply(headAxes(head(0, 20)), ahead.get(id)));
      assert.ok(off < 0.01 && finite(now), `recentred, each effect is heard from the head's turn from the seat: ${off}`);
      report.push(`Recenter: within ${off.toFixed(4)}`);
      // IWER's recentring: its head levelled to the space's -Z, the space
      // reset; the seat at its origin again, what is heard from the head as
      // the space now has it.
      await page.evaluate(() => window.xrDevice.recenter());
      await frames(page, 6);
      xr = (await status(page)).xr;
      assert.equal(xr.resets, 1, 'the reference space reset reached the app');
      assert.ok(Math.abs(xr.seat.yaw) < 1e-6, `the seat at the origin: ${JSON.stringify(xr.seat)}`);
      const levelled = await page.evaluate(() => { const q = window.xrDevice.quaternion; return [q.x, q.y, q.z, q.w]; });
      now = await heard(page);
      const presented = facing(headAxes(levelled)[2]);
      assert.ok(Math.abs(now.head.yaw - presented.yaw) < 0.01 && Math.abs(now.head.pitch - presented.pitch) < 0.01, `reset: the head as the space has it: ${JSON.stringify([now.head, presented])}`);
      off = worstOff(now, (id) => apply(headAxes(levelled), ahead.get(id)));
      assert.ok(off < 0.01 && finite(now), `reset, each effect is heard from the head's turn: ${off}`);
      report.push(`the reference's reset (the head at ${JSON.stringify(presented)}): within ${off.toFixed(4)}`);

      // Lost tracking: the frames come without a viewer pose while the head
      // turns; the last head stays, and so does what is heard with it.
      const kept = await look(page, head(45), [0, 1.6, 0]);
      const before = (await status(page)).page.xr.frames;
      await page.evaluate(() => { window.xemTrackingLost = true; window.xrDevice.quaternion.set(0, Math.sin(Math.PI / 8), 0, Math.cos(Math.PI / 8)); });
      await frames(page, 10);
      now = await heard(page);
      assert.ok((await status(page)).page.xr.frames >= before + 10, 'the session goes on without a pose');
      assert.deepEqual([now.head, targets(now, 'effect'), targets(now, 'music')], [kept.head, targets(kept, 'effect'), targets(kept, 'music')],
        'lost tracking keeps the last head and what is heard with it');
      await page.evaluate(() => { window.xemTrackingLost = false; });
      await frames(page, 4);
      now = await heard(page);
      assert.ok(Math.abs(now.head.yaw + 45) < 0.01 && finite(now), `found again, the head is followed: ${JSON.stringify(now.head)}`);
      report.push(`lost tracking kept ${JSON.stringify(kept.head)} through 10 frames, then ${JSON.stringify(now.head)}`);

      // First-person inspection (the right stick's click): its view is level
      // on the camera's heading, so it hears the screen's directions turned
      // up by the camera's pitch; the head turns them once more, the view
      // not at all.
      await look(page, head(0));
      const click = async () => {
        await page.evaluate(() => window.xrDevice.controllers.right.updateButtonValue('thumbstick', 1));
        await frames(page, 3);
        await page.evaluate(() => window.xrDevice.controllers.right.updateButtonValue('thumbstick', 0));
        await frames(page, 3);
      };
      await page.evaluate(() => window.xem.setRunning(true));
      await click();
      await page.waitForFunction(() => window.xem.status().xr.within && window.xem.command({ cmd: 'binaural' }).first_person, null, { timeout: 60_000 });
      await frames(page, 6);
      await page.evaluate(() => window.xem.setRunning(false));
      const inspecting = await look(page, head(0));
      const view = inspecting.first_person;
      assert.ok(Math.abs(view.yaw) < 0.01 && view.pitch > 5, `the view looks along the camera's heading, level, above its pitch: ${JSON.stringify(view)}`);
      const p = radians(view.pitch);
      const level = [[1, 0, 0], [0, Math.cos(p), Math.sin(p)], [0, -Math.sin(p), Math.cos(p)]];
      off = worstOff(inspecting, (id) => apply(level, ahead.get(id)));
      assert.ok(off < 0.05, `inspecting, the screen's directions turned up by the camera's pitch: ${off}`);
      // (The game ran in between: the song's own pan may have moved its
      // parts, not their places.)
      assert.deepEqual(placements(inspecting), placements(screen), 'looking around moves no music');
      report.push(`inspection hears the screen's directions turned by the camera's pitch ${view.pitch.toFixed(2)} within ${off.toFixed(4)}`);
      await turns(inspecting, 'inspecting');
      // The right stick turns the view 30 degrees to the right, the game
      // held: what is heard turns 30 to the left, once, and the music stays.
      await page.evaluate(() => window.xrDevice.controllers.right.updateAxes('thumbstick', 1, 0));
      await frames(page, 6);
      await page.evaluate(() => window.xrDevice.controllers.right.updateAxes('thumbstick', 0, 0));
      await frames(page, 6);
      assert.ok(Math.abs((await status(page)).renderer.look.yaw - Math.PI / 6) < 1e-3, 'one push, one 30-degree step');
      now = await heard(page);
      const snapped = new Map([...targets(inspecting, 'effect')].map(([id, t]) => [id, toward(t)]));
      off = worstOff(now, (id) => apply(headAxes(head(30)), snapped.get(id)));
      assert.ok(off < 0.01, `the stick's turn turns what is heard once, as a head turned 30 degrees would: ${off}`);
      assert.deepEqual(targets(now, 'music'), targets(inspecting, 'music'), 'the stick\'s turn moves no music');
      report.push(`the right stick's 30 degrees, the game held, within ${off.toFixed(4)}`);
      // Off again: the camera's own view.
      await page.evaluate(() => window.xem.setRunning(true));
      await click();
      await page.waitForFunction(() => !window.xem.status().xr.within && window.xem.command({ cmd: 'binaural' }).first_person === null, null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.setRunning(false));
      console.log(report.join('; '));
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 30_000 });
      assert.equal((await heard(page)).head, null, 'no head out of the headset');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('in the diorama the listener stays at the character, hears the table from the head once and nothing of the head\'s place or the table\'s scale', { timeout: 600_000 }, async () => {
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST + HAND_MODEL }] });
    try {
      const page = await session(browser, true);
      const report = [];
      const field = await page.evaluate(() => window.xem.command({ cmd: 'field' }));
      const actor = (index) => field.actors.find((a) => a.index === index).position;
      const character = actor(field.player);
      /** Where each effect lies from the head (orientation `q`): the
       *  character to its emitter's actor in the room, by the table's
       *  matrix (column major, from the game's world), in the head's axes. */
      const fromHead = async (q) => {
        const m = (await status(page)).xr.space.roomFromWorld;
        const sources = (await heard(page)).sources;
        return (id) => {
          const emitter = actor(sources.find((s) => s.id === id).follows);
          const w = ['x', 'y', 'z'].map((k) => emitter[k] - character[k]);
          const room = unit([0, 1, 2].map((i) => m[i] * w[0] + m[4 + i] * w[1] + m[8 + i] * w[2]));
          const local = rotate(conjugate(q), room);
          return [local[0], -local[1], -local[2]];
        };
      };
      let now = await look(page, head(0));
      assert.deepEqual(now.listening_position, [character.x, character.y, character.z], 'the listener is the controlled character');
      assert.match(now.listener, /^field: the controlled character/);
      assert.ok(now.first_person, 'the listener takes the seat\'s axes through the table');
      let worst = 0;
      for (const [yaw, pitch] of [[0, 0], [40, 0], [-30, -25], [100, 30]]) {
        const q = head(yaw, pitch);
        now = await look(page, q);
        const off = worstOff(now, await fromHead(q));
        worst = Math.max(worst, off);
        assert.ok(off < 0.05 && finite(now), `the head ${yaw} right and ${pitch} up hears each effect where the table shows it: ${off}`);
      }
      report.push(`four head turns hear the table within ${worst.toFixed(4)} degrees`);
      // The head moved without turning: nothing heard changes.
      const still = await look(page, head(40, -10), [0, 1.6, 0]);
      for (const at of [[0.15, 1.6, 0], [-0.1, 1.45, -0.2], [0.05, 1.7, 0.25]]) {
        now = await look(page, head(40, -10), at);
        assert.deepEqual([now.head, now.first_person, targets(now, 'effect'), targets(now, 'music')], [still.head, still.first_person, targets(still, 'effect'), targets(still, 'music')],
          `the head moved to ${at}: nothing heard changes`);
      }
      // Recenter with the head turned 30 degrees to the left: the table in
      // front of the head again, heard from it where it now lies; then
      // IWER's recentring, the seat back at the origin.
      now = await look(page, head(-30, 10), [0, 1.6, 0]);
      await page.evaluate(() => window.xem.appOperation({ Action: 'Recenter' }));
      await frames(page, 6);
      now = await heard(page);
      let off = worstOff(now, await fromHead(head(-30, 10)));
      assert.equal((await status(page)).xr.recenters, 1);
      assert.ok(Math.abs(now.head.yaw) < 0.01 && off < 0.05 && finite(now), `recentred, each effect where the table now lies from the head: ${off} (${JSON.stringify(now.head)})`);
      report.push(`Recenter: within ${off.toFixed(4)}`);
      await page.evaluate(() => window.xrDevice.recenter());
      await frames(page, 6);
      assert.equal((await status(page)).xr.resets, 1);
      const levelled = await page.evaluate(() => { const q = window.xrDevice.quaternion; return [q.x, q.y, q.z, q.w]; });
      now = await heard(page);
      off = worstOff(now, await fromHead(levelled));
      assert.ok(off < 0.05 && finite(now), `after the reference's reset, each effect where the table lies from the head: ${off}`);
      report.push(`the reference's reset: within ${off.toFixed(4)}`);
      // The table tilted 30 degrees, the game held: the listener takes the
      // seat's axes through it at once, and hears it from the head as it
      // now lies.
      const flat = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      assert.equal((await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, xr: { ...g.xr, diorama_tilt_deg: 30 } } }), flat)).ok, true);
      await frames(page, 6);
      const tilted = await heard(page);
      off = worstOff(tilted, await fromHead(levelled));
      assert.ok(Math.abs(tilted.first_person.pitch - now.first_person.pitch) > 10 && off < 0.05,
        `the table tilted, the listener's view turns with it (${JSON.stringify([now.first_person, tilted.first_person])}) and hears it from the head: ${off}`);
      assert.equal((await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), flat)).ok, true);
      await frames(page, 6);
      report.push(`tilted 30 degrees, the game held: the view from ${now.first_person.pitch.toFixed(2)} to ${tilted.first_person.pitch.toFixed(2)} up, within ${off.toFixed(4)}`);

      // Two whole-hand pinches spreading, twice, the game held, grow the
      // table from 25% to 400%, the listener taking the table's axes at
      // every scale: what is heard stays and nobody moves; the scale goes
      // to the setting once each gesture.
      await look(page, head(0), [0, 1.6, 0]);
      const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      assert.equal((await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, xr: { ...g.xr, diorama_scale: 25 } } }), graphics)).ok, true);
      await page.evaluate(() => {
        const device = window.xrDevice;
        device.primaryInputMode = 'hand';
        window.handModel.install(device);
        window.handModel.set(device, 'left', 'open');
        window.handModel.set(device, 'right', 'open');
      });
      const at = (x) => page.evaluate((x) => {
        window.xrDevice.hands.left.position.set(-x, 1.4, -0.3);
        window.xrDevice.hands.right.position.set(x, 1.4, -0.3);
      }, x);
      const shape = (name) => page.evaluate((name) => {
        window.handModel.set(window.xrDevice, 'left', name);
        window.handModel.set(window.xrDevice, 'right', name);
      }, name);
      await at(0.08);
      await frames(page, 10);
      const scaled = await heard(page);
      const unmoved = { pinches: (await status(page)).xr.counts.pinch, player: character };
      const seen = [];
      for (let gesture = 0; gesture < 2; gesture++) {
        await at(0.08);
        await frames(page, 3);
        await shape('gather');
        await frames(page, 3);
        for (let i = 1; i <= 8; i++) {
          await at(0.08 + 0.04 * i);
          await frames(page, 2);
          seen.push([(await status(page)).xr, await heard(page), (await page.evaluate(() => window.xem.command({ cmd: 'status' }))).pads[0].buttons]);
        }
        await shape('open');
        await frames(page, 4);
      }
      const shown = seen.map(([xr]) => xr.dioramaScale);
      const committed = (await page.evaluate(() => window.xem.settings.get())).settings.graphics.xr.diorama_scale;
      console.log(`scaling: shown ${shown.map((s) => s.toFixed(2))}, committed ${committed}%`);
      assert.ok(Math.min(...shown) < 0.5 && Math.max(...shown) === 4 && committed === 400, `the table grew from 25% to 400%: ${shown}, ${committed}`);
      for (const [xr, b, pads] of seen) {
        assert.ok(xr.scaling && xr.diorama, 'scaling the diorama shown');
        assert.deepEqual([b.head, b.first_person, b.listening_position, targets(b, 'effect'), targets(b, 'music')],
          [scaled.head, scaled.first_person, scaled.listening_position, targets(scaled, 'effect'), targets(scaled, 'music')],
          `at ${xr.dioramaScale}, nothing heard changes`);
        assert.deepEqual(pads, [], 'the scaling presses nothing');
      }
      const after = await status(page);
      assert.equal(after.xr.counts.pinch, unmoved.pinches, 'no pinch');
      assert.equal(after.session.assist, null, 'no walk');
      const moved = await page.evaluate(() => { const f = window.xem.command({ cmd: 'field' }); return f.actors.find((a) => a.index === f.player).position; });
      assert.deepEqual(moved, unmoved.player, 'the character stays');
      report.push(`scaling 25% to 400% (${seen.length} frames read) changed nothing heard and moved nobody`);

      // The same frames from the state at 25%, 100% and 400%: the game's
      // RAM and its sound (the SPU's output, sample for sample) alike, and
      // what is heard at their end. (A whole state taken while the diorama
      // presents differs from run to run at one scale too, in two words of
      // the module's own memory outside the game's: docs/runtime.md,
      // Binaural, Not done. tests/binaural.rs compares the SPU's own state
      // frame by frame.)
      const outcome = async (scale) => {
        const g = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
        assert.equal((await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), { ...g, xr: { ...g.xr, diorama_scale: scale } })).ok, true);
        await frames(page, 6);
        return page.evaluate(async (base64) => {
          window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
          window.xem.command({ cmd: 'audio_capture', action: 'start' });
          await window.xem.runFrames(240);
          const sound = window.xem.command({ cmd: 'audio_capture', action: 'stop' });
          const b = window.xem.command({ cmd: 'binaural' });
          // Each source by what it is (ids go on counting across restores).
          const what = (s) => (s.kind === 'music' ? `music ${s.song}/${s.part}` : `effect ${s.effect} of ${s.follows}`);
          return { ram: window.xem.memoryHash(), sound: [sound.frames, sound.hash],
            heard: b.sources.filter((s) => s.target).map((s) => [what(s), s.target]).sort(([a], [b]) => a.localeCompare(b)), view: b.first_person };
        }, made());
      };
      const runs = new Map();
      for (const scale of [25, 100, 400]) runs.set(scale, await outcome(scale));
      for (const scale of [100, 400]) {
        const [a, b] = [runs.get(25), runs.get(scale)];
        assert.equal(b.ram, a.ram, `${scale}%: the RAM after 240 frames is 25%'s`);
        assert.deepEqual(b.sound, a.sound, `${scale}%: the sound is 25%'s`);
        assert.deepEqual([b.heard, b.view], [a.heard, a.view], `${scale}%: what is heard is 25%'s`);
      }
      assert.ok(runs.get(25).sound[0] > 100_000, 'there is sound to compare');
      report.push(`240 frames at 25, 100 and 400%: RAM ${runs.get(25).ram}, sound ${runs.get(25).sound[1]} each`);
      console.log(report.join('; '));
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 30_000 });
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
