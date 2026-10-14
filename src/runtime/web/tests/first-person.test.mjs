// First-person inspection in browsers (local only: it needs the user's disc
// 1, the game module and a native xem-headless to make the field's state).
// In field 30, with the page running the game: Backspace toggles the view
// and stops the route under way, and new routes are refused; while it shows
// the walking and jumping keys reach no pad and the character stays, the
// arrow keys and the mouse the game holds turn it, the wheel's zoom rests
// and the Binaural listener looks where it looks; the touch deck's SELECT
// toggles it and a finger dragged on the picture turns it; Backspace again
// gives the keys back; the Original renderer ends it. The canvas's pictures
// go to build/web-test (first-person-*.png).

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, SHOTS, USER_DISC, WEB, pixelStats, shot, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
mkdirSync(SHOTS, { recursive: true });

/** Animation frames of the page. */
const frames = (page, count) => page.evaluate(async (count) => {
  for (let i = 0; i < count; i++) await new Promise(requestAnimationFrame);
}, count);
const command = (page, cmd) => page.evaluate((cmd) => window.xem.command(cmd), cmd);
const renderer = async (page) => (await status(page)).renderer;
const player = async (page) => {
  const field = await command(page, { cmd: 'field' });
  return field.actors.find((actor) => actor.index === field.player).position;
};
/** A key held for a moment of play (the page reads it at its frames). */
async function tap(page, key, ms = 150) {
  await page.keyboard.down(key);
  await page.waitForTimeout(ms);
  await page.keyboard.up(key);
  await frames(page, 3);
}
async function canvasShot(page, name) {
  await frames(page, 4);
  return pixelStats(await page.locator('canvas').screenshot({ path: shot(name) }));
}

describe('first-person inspection in browsers', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  test('keys, the mouse and touch look around a field while the character stays', { timeout: 600_000 }, async () => {
    const state = join(SHOTS, 'first-person-field30.state');
    execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
      input: [{ cmd: 'enter_field', map: 30 }, { cmd: 'frames', count: 400 }, { cmd: 'state_save', path: state }]
        .map((c) => JSON.stringify(c)).join('\n') + '\n',
    });
    const browser = await start({ args: NO_WEBGPU });
    try {
      const page = await browser.page();
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(async (base64) => {
        await window.xem.boot();
        window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
        window.xem.setRunning(true);
        document.activeElement?.blur();
      }, readFileSync(state).toString('base64'));
      await page.waitForFunction(() => window.xem.command({ cmd: 'context' }).kind === 'walk');
      await frames(page, 10);
      const off = await canvasShot(page, 'first-person-off.png');
      assert.ok(off.lit > 0.2, `the room shows (${off.lit})`);

      // A route under way (a few frames of it, run by command with the
      // page's loop held) stops when the view comes on: Backspace is held
      // when the loop resumes. New routes are refused.
      await page.evaluate(() => window.xem.setRunning(false));
      const start = await player(page);
      const route = await command(page, { cmd: 'navigate', to: [-80, -90] });
      assert.equal(route.error, undefined, JSON.stringify(route));
      await page.evaluate(() => window.xem.runFrames(8));
      assert.notEqual((await status(page)).session.assist, null, 'the route is under way');
      assert.notDeepEqual(await player(page), start, 'and walks');
      await page.keyboard.down('Backspace');
      await page.evaluate(() => window.xem.setRunning(true));
      await frames(page, 6);
      await page.keyboard.up('Backspace');
      await frames(page, 3);
      let r = await renderer(page);
      assert.equal(r.firstPerson, true, 'Backspace toggles the view');
      assert.deepEqual(r.look, { yaw: 0, pitch: 0 });
      let session = (await status(page)).session;
      assert.equal(session.inspecting, true);
      assert.equal(session.assist, null, 'the route stopped');
      assert.equal(session.assist_outcome, 'stopped');
      assert.match((await command(page, { cmd: 'navigate', to: [-80, -90] })).error, /^first-person inspection/);
      const on = await canvasShot(page, 'first-person-on.png');
      assert.ok(on.lit > 0.2, `the view shows the room from the eyes (${on.lit})`);

      // Walking and jumping reach no pad; the character stays.
      await frames(page, 10);
      const stood = await player(page);
      await page.keyboard.down('KeyW');
      await page.keyboard.down('Space');
      await page.waitForTimeout(600);
      const pads = (await command(page, { cmd: 'status' })).pads[0].buttons;
      assert.ok(!pads.includes('up') && !pads.includes('triangle'), JSON.stringify(pads));
      assert.deepEqual(await player(page), stood, 'the character stays where it stands');
      await page.keyboard.up('KeyW');
      await page.keyboard.up('Space');

      // The arrow keys turn the view (2 radians a second across, 1.5 up, a
      // tenth of a second at most a frame: a loaded page turns less).
      await page.keyboard.down('ArrowRight');
      await page.waitForTimeout(500);
      await page.keyboard.up('ArrowRight');
      await page.keyboard.down('ArrowUp');
      await page.waitForTimeout(300);
      await page.keyboard.up('ArrowUp');
      await frames(page, 3);
      r = await renderer(page);
      assert.ok(r.look.yaw > 0.2 && r.look.yaw < 2, `ArrowRight turned it right: ${JSON.stringify(r.look)}`);
      assert.ok(r.look.pitch > 0.05 && r.look.pitch < 1, `ArrowUp turned it up: ${JSON.stringify(r.look)}`);
      const turned = await canvasShot(page, 'first-person-turned.png');
      assert.ok(turned.lit > 0.1, `the turned view shows the room (${turned.lit})`);

      // The wheel's zoom rests while the view shows.
      const zoom = (await page.evaluate(() => window.xem.settings.get())).settings.graphics.zoom_out;
      const box = await page.locator('canvas').boundingBox();
      await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
      await page.mouse.wheel(0, 300);
      await frames(page, 3);
      assert.equal((await page.evaluate(() => window.xem.settings.get())).settings.graphics.zoom_out, zoom, 'no zoom');

      // The Binaural listener looks where the view looks (its facing is in
      // the game camera's view space, which looks down at the room: turned
      // right, it faces right of the camera).
      const binaural = (await page.evaluate(() => window.xem.settings.get())).settings.binaural;
      assert.equal((await page.evaluate((b) => window.xem.settings.apply({ Binaural: { ...b, enabled: true } }), binaural)).ok, true);
      await frames(page, 6);
      let listener = (await command(page, { cmd: 'binaural' })).first_person;
      assert.ok(listener && listener.yaw > 10, `the listener faces right with the view: ${JSON.stringify(listener)}`);

      // The mouse the game holds (the mouse camera) turns it too.
      assert.equal((await page.evaluate(() => window.xem.settings.apply({ MouseCamera: true }))).ok, true);
      await page.locator('canvas').click({ button: 'right' });
      await page.waitForFunction(() => document.pointerLockElement === document.querySelector('canvas'));
      const before = (await renderer(page)).look;
      await page.mouse.move(box.x + box.width / 2 + 100, box.y + box.height / 2 + 40, { steps: 4 });
      await frames(page, 4);
      const after = (await renderer(page)).look;
      assert.ok(after.yaw - before.yaw > 0.1, `the mouse turned it right: ${JSON.stringify([before, after])}`);
      assert.ok(after.pitch < before.pitch, `and down: ${JSON.stringify([before, after])}`);
      await page.evaluate(() => document.exitPointerLock());
      await page.waitForFunction(() => !document.pointerLockElement);
      await page.evaluate(() => window.xem.panel.show(false));
      assert.equal((await page.evaluate(() => window.xem.settings.apply({ MouseCamera: false }))).ok, true);

      // Touch: the deck's SELECT toggles the view, a finger dragged on the
      // picture turns it (the scene follows the finger).
      const finger = (type, id, at) => page.evaluate(async ({ type, id, at }) => {
        const canvas = document.querySelector('canvas');
        const scale = window.devicePixelRatio || 1;
        const s = window.xem.status();
        const c = typeof at === 'string' ? s.touch.controls.find((k) => k.control === at) : null;
        const [x, y] = c ? [c.x + c.width / 2, c.y + c.height / 2] : at ?? [s.renderer.picture.x + s.renderer.picture.width / 2, s.renderer.picture.y + s.renderer.picture.height / 2];
        const box = canvas.getBoundingClientRect();
        canvas.dispatchEvent(new PointerEvent(type, { pointerType: 'touch', pointerId: id, clientX: box.left + x / scale, clientY: box.top + y / scale, bubbles: true, isPrimary: true }));
        for (let i = 0; i < 3; i++) await new Promise(requestAnimationFrame);
        return [x, y];
      }, { type, id, at });
      // The first touch (a tap on the picture, which walks nowhere while the
      // view shows) brings the deck.
      await finger('pointerdown', 9);
      await finger('pointerup', 9);
      assert.equal((await status(page)).session.assist, null, 'a tap on the picture walks nowhere');
      const controls = (await status(page)).touch.controls.map((c) => c.control);
      assert.ok(controls.includes('Select'), JSON.stringify(controls));
      for (const shows of [false, true]) {
        await finger('pointerdown', 1, 'Select');
        await finger('pointerup', 1, 'Select');
        assert.equal((await renderer(page)).firstPerson, shows, 'the deck\'s SELECT toggles the view');
      }
      r = await renderer(page);
      assert.deepEqual(r.look, { yaw: 0, pitch: 0 }, 'a new view looks along the camera\'s heading');
      const [x, y] = await finger('pointerdown', 2);
      const scale = await page.evaluate(() => window.devicePixelRatio || 1);
      for (let step = 1; step <= 4; step++) await finger('pointermove', 2, [x + step * 25 * scale, y]);
      await finger('pointerup', 2, [x + 100 * scale, y]);
      const dragged = (await renderer(page)).look;
      assert.ok(Math.abs(dragged.yaw + 0.3) < 0.02 && Math.abs(dragged.pitch) < 0.02, `100 pixels dragged right turn it 0.3 radians left: ${JSON.stringify(dragged)}`);
      assert.equal((await status(page)).session.assist, null, 'the drag walks nowhere');
      await canvasShot(page, 'first-person-touch.png');

      // Backspace again: the view goes and the keys walk again.
      await tap(page, 'Backspace');
      assert.equal((await renderer(page)).firstPerson, false);
      assert.equal((await status(page)).session.inspecting, false);
      listener = (await command(page, { cmd: 'binaural' })).first_person;
      assert.equal(listener, null, 'the listener has the camera\'s view again');
      await page.keyboard.down('KeyW');
      await page.waitForTimeout(200);
      assert.ok((await command(page, { cmd: 'status' })).pads[0].buttons.includes('up'), 'W walks again');
      await page.keyboard.up('KeyW');

      // The Original renderer ends the view.
      await tap(page, 'Backspace');
      assert.equal((await renderer(page)).firstPerson, true);
      const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, renderer: 'Original' } }), graphics);
      await frames(page, 6);
      assert.equal((await renderer(page)).firstPerson, false, 'the Original renderer has nothing to look around in');
      await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), graphics);
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
