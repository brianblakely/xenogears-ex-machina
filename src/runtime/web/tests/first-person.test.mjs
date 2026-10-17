// First-person inspection in browsers (local only: it needs the user's disc
// 1, the game module and a native xem-headless to make the field's state).
// In field 30, with the page running the game: Backspace toggles the view
// and stops the route under way, and new routes are refused; while it shows
// the walking and jumping keys reach no pad and the character stays, the
// arrow keys and the mouse the game holds turn it, the wheel's zoom rests
// and the Binaural listener looks where it looks; a tap on the picture walks
// nowhere, the touch deck's SELECT toggles the view and a finger dragged on
// the picture turns it; Backspace again gives the keys back; the Original
// renderer ends it; a gamepad's right stick's click toggles it, its right
// stick turns it and its Back released alone ends it, none of which acts
// while the page has lost focus; an agent's `window.xem.firstPerson` turns
// it on looking where asked (a route under way stops, the agent's walk and
// jump are refused) and off; Toggle first-person view's key (V), the deck's
// LOOK and the automation's app operation toggle it. The canvas's pictures go to build/web-test
// (first-person-*.png). Input is held until the page's frames have read it,
// so a loaded machine (the suite runs browsers side by side) slows the test
// but does not change what it sees. In field 15 the browser's Binaural
// listener looking around in first person (its facing, where each emitter
// is heard, the music's places) is the native desktop host's for the same
// state and look within float rounding: xem-headless's first_person example
// (the same game view natively) takes the page's state and look angles.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, SHOTS, USER_DISC, WEB, pixelStats, shot, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
/** The native desktop host's first-person listener: xem-headless's
 *  first_person example, built beside it (suite.sh's native stage). */
const FIRST_PERSON = join(dirname(HEADLESS), 'examples', 'first_person');
mkdirSync(SHOTS, { recursive: true });

/** Animation frames of the page (each one a frame of its loop). */
const frames = (page, count) => page.evaluate(async (count) => {
  for (let i = 0; i < count; i++) await new Promise(requestAnimationFrame);
}, count);
const command = (page, cmd) => page.evaluate((cmd) => window.xem.command(cmd), cmd);
const renderer = async (page) => (await status(page)).renderer;
const pads = async (page) => (await command(page, { cmd: 'status' })).pads[0].buttons;
const player = async (page) => {
  const field = await command(page, { cmd: 'field' });
  return field.actors.find((actor) => actor.index === field.player).position;
};
/** Backspace held until the view is `on`, then released. */
async function toggle(page, on) {
  await page.keyboard.down('Backspace');
  await page.waitForFunction((on) => window.xem.status().renderer.firstPerson === on, on);
  await page.keyboard.up('Backspace');
  await frames(page, 3);
}
/** A key held for `ms` and at least a few of the page's frames. */
async function hold(page, key, ms) {
  await page.keyboard.down(key);
  await page.waitForTimeout(ms);
  await frames(page, 3);
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
      await page.waitForFunction(() => window.xem.status().renderer.firstPerson);
      await page.keyboard.up('Backspace');
      await frames(page, 3);
      let r = await renderer(page);
      assert.deepEqual(r.look, { yaw: 0, pitch: 0 }, 'the view looks along the camera\'s heading');
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
      await page.waitForTimeout(300);
      await frames(page, 10);
      const held = await pads(page);
      assert.ok(!held.includes('up') && !held.includes('triangle'), JSON.stringify(held));
      assert.deepEqual(await player(page), stood, 'the character stays where it stands');
      await page.keyboard.up('KeyW');
      await page.keyboard.up('Space');

      // The arrow keys turn the view (2 radians a second across, 1.5 up, a
      // tenth of a second at most a frame: a loaded page turns less).
      await hold(page, 'ArrowRight', 500);
      await hold(page, 'ArrowUp', 300);
      r = await renderer(page);
      assert.ok(r.look.yaw > 0.2 && r.look.yaw < 3, `ArrowRight turned it right: ${JSON.stringify(r.look)}`);
      assert.ok(r.look.pitch > 0.05 && r.look.pitch < 1.4, `ArrowUp turned it up: ${JSON.stringify(r.look)}`);
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

      // Touch: a finger at `at` (a deck control's name, canvas pixels, or
      // the picture's middle), then the page's frames.
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
      // The first touch (a tap on the picture, which walks nowhere while
      // the view shows) brings the deck.
      await finger('pointerdown', 9);
      await finger('pointerup', 9);
      await frames(page, 3);
      assert.equal((await status(page)).session.assist, null, 'a tap on the picture walks nowhere');
      const controls = (await status(page)).touch.controls.map((c) => c.control);
      assert.ok(controls.includes('Select'), JSON.stringify(controls));
      // The deck's SELECT toggles the view off and on again.
      for (const shows of [false, true]) {
        await finger('pointerdown', 1, 'Select');
        await page.waitForFunction((shows) => window.xem.status().renderer.firstPerson === shows, shows);
        await finger('pointerup', 1, 'Select');
      }
      r = await renderer(page);
      assert.deepEqual(r.look, { yaw: 0, pitch: 0 }, 'a new view looks along the camera\'s heading');
      const [x, y] = await finger('pointerdown', 2);
      const scale = await page.evaluate(() => window.devicePixelRatio || 1);
      for (let step = 1; step <= 4; step++) await finger('pointermove', 2, [x + step * 25 * scale, y]);
      await finger('pointerup', 2, [x + 100 * scale, y]);
      await frames(page, 3);
      const dragged = (await renderer(page)).look;
      assert.ok(Math.abs(dragged.yaw + 0.3) < 0.02 && Math.abs(dragged.pitch) < 0.02, `100 pixels dragged right turn it 0.3 radians left: ${JSON.stringify(dragged)}`);
      assert.equal((await status(page)).session.assist, null, 'the drag walks nowhere');
      await canvasShot(page, 'first-person-touch.png');

      // Backspace again: the view goes, the listener has the camera's view
      // and the keys walk again.
      await toggle(page, false);
      assert.equal((await status(page)).session.inspecting, false);
      await frames(page, 3);
      listener = (await command(page, { cmd: 'binaural' })).first_person;
      assert.equal(listener, null, 'the listener has the camera\'s view again');
      await page.keyboard.down('KeyW');
      await page.waitForFunction(() => window.xem.command({ cmd: 'status' }).pads[0].buttons.includes('up'));
      await page.keyboard.up('KeyW');

      // The Original renderer ends the view.
      await toggle(page, true);
      const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, renderer: 'Original' } }), graphics);
      await page.waitForFunction(() => !window.xem.status().renderer.firstPerson);
      await frames(page, 3);
      assert.equal((await status(page)).session.inspecting, false, 'the Original renderer has nothing to look around in');
      assert.equal((await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), graphics)).ok, true, 'Ex Machina again');

      // A gamepad (standard mapping, read each of the page's frames): the
      // right stick's click toggles the view, the right stick turns it and
      // Back released alone ends it. While the page has lost focus neither
      // toggles nor turns it, and a click still held when focus returns
      // toggles nothing.
      await page.evaluate(() => {
        window.testPad = { id: 'Test gamepad', connected: true, mapping: 'standard',
          buttons: Array.from({ length: 17 }, () => ({ pressed: false })), axes: [0, 0, 0, 0] };
        Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.testPad] });
      });
      /** The pad's buttons and axes as they are for `ms` and a few of the page's frames. */
      const gamepad = (buttons = [], axes = [0, 0, 0, 0], ms = 0) => page.evaluate(async ({ buttons, axes, ms }) => {
        window.testPad.buttons.forEach((button, i) => { button.pressed = buttons.includes(i); });
        window.testPad.axes = axes;
        await new Promise((resolve) => setTimeout(resolve, ms));
        for (let i = 0; i < 3; i++) await new Promise(requestAnimationFrame);
      }, { buttons, axes, ms });
      const [BACK, R3] = [8, 11];
      await gamepad([R3]);
      await page.waitForFunction(() => window.xem.status().renderer.firstPerson);
      await gamepad();
      await gamepad([], [0, 0, 1, 0], 300);
      await gamepad();
      r = await renderer(page);
      assert.ok(r.look.yaw > 0.1 && Math.abs(r.look.pitch) < 1e-6, `the right stick turned it right: ${JSON.stringify(r.look)}`);
      const focusPause = (await page.evaluate(() => window.xem.settings.get())).settings.pause_on_focus_loss;
      assert.equal((await page.evaluate(() => window.xem.settings.apply({ PauseOnFocusLoss: false }))).ok, true);
      await page.evaluate(() => window.dispatchEvent(new Event('blur')));
      await gamepad([], [0, 0, 1, 0], 300);
      await gamepad();
      assert.deepEqual((await renderer(page)).look, r.look, 'without focus the stick turns nothing');
      await gamepad([R3]);
      await gamepad();
      assert.equal((await renderer(page)).firstPerson, true, 'nor does its click toggle the view');
      await gamepad([BACK]);
      await gamepad();
      assert.equal((await renderer(page)).firstPerson, true, 'nor Back released alone');
      await gamepad([R3]);
      await page.evaluate(() => window.dispatchEvent(new Event('focus')));
      await gamepad([R3]);
      await gamepad();
      assert.equal((await renderer(page)).firstPerson, true, 'a click held as focus returns toggles nothing');
      await gamepad([BACK]);
      await gamepad();
      await page.waitForFunction(() => !window.xem.status().renderer.firstPerson);
      assert.equal((await status(page)).session.inspecting, false, 'Back released alone ends it');
      assert.equal((await page.evaluate((p) => window.xem.settings.apply({ PauseOnFocusLoss: p }), focusPause)).ok, true);
      await gamepad();

      // An agent's typed command: on, looking 30 degrees right and 10 down,
      // stops a route under way (by command, the page's loop held) once the
      // loop runs again; the agent's walk and jump are refused while it
      // shows; off, the session has it off.
      await page.evaluate(() => window.xem.setRunning(false));
      assert.equal((await command(page, { cmd: 'navigate', to: [-80, -90] })).error, undefined);
      await page.evaluate(() => window.xem.runFrames(8));
      assert.notEqual((await status(page)).session.assist, null, 'a route under way');
      let reply = await page.evaluate(() => window.xem.firstPerson({ on: true, yaw: 30, pitch: -10 }));
      assert.deepEqual(reply, { first_person: true, look: { yaw: 30, pitch: -10 } });
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => window.xem.status().session.inspecting);
      session = (await status(page)).session;
      assert.equal(session.assist, null, 'the agent\'s view stopped the route');
      assert.equal(session.assist_outcome, 'stopped');
      r = await renderer(page);
      assert.ok(Math.abs(r.look.yaw - Math.PI / 6) < 1e-4 && Math.abs(r.look.pitch + Math.PI / 18) < 1e-4, `status shows the look in radians: ${JSON.stringify(r.look)}`);
      for (const cmd of [{ cmd: 'act', actions: ['Up'], frames: 5 }, { cmd: 'press', buttons: ['triangle'] }, { cmd: 'navigate', to: [-80, -90] }]) {
        assert.match((await command(page, cmd)).error, /^first-person inspection keeps the character/, JSON.stringify(cmd));
      }
      assert.match((await page.evaluate(() => window.xem.firstPerson({ on: 'yes' }))).error, /on is true or false/);
      reply = await page.evaluate(() => window.xem.firstPerson({ on: false }));
      assert.deepEqual(reply, { first_person: false, look: null });
      await page.waitForFunction(() => !window.xem.status().session.inspecting);

      // Toggle first-person view: its key (V), the deck's LOOK, and the
      // automation's app operation, each once a press.
      await page.keyboard.down('KeyV');
      await page.waitForFunction(() => window.xem.status().renderer.firstPerson);
      await page.keyboard.up('KeyV');
      await frames(page, 3);
      assert.equal((await renderer(page)).firstPerson, true, 'V turned it on, once');
      const deck = (await status(page)).touch.controls.map((c) => c.control);
      assert.ok(deck.includes('Look') && deck.includes('Select'), JSON.stringify(deck));
      await finger('pointerdown', 3, 'Look');
      await page.waitForFunction(() => !window.xem.status().renderer.firstPerson);
      await finger('pointerup', 3, 'Look');
      assert.equal((await status(page)).session.assist, null, 'LOOK walks nowhere');
      assert.equal((await page.evaluate(() => window.xem.appOperation({ Action: 'ToggleFirstPerson' }))).first_person, true);
      assert.equal((await page.evaluate(() => window.xem.firstPerson())).first_person, false, 'the bare command toggles it');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('the Binaural listener looking around in first person is the native host\'s within float rounding', { skip: !existsSync(FIRST_PERSON) && `needs ${FIRST_PERSON}`, timeout: 600_000 }, async () => {
    // Field 15: three emitters the presenter places, and its music; the
    // scene stands still while nobody plays.
    const state = join(SHOTS, 'first-person-field15.state');
    execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
      input: [{ cmd: 'enter_field', map: 15 }, { cmd: 'frames', count: 600 }, { cmd: 'state_save', path: state }]
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
        const binaural = window.xem.settings.get().settings.binaural;
        window.xem.settings.apply({ Binaural: { ...binaural, enabled: true } });
        window.xem.setRunning(true);
        document.activeElement?.blur();
      }, readFileSync(state).toString('base64'));
      await page.waitForFunction(() => window.xem.command({ cmd: 'context' }).kind === 'walk');
      await toggle(page, true);
      await hold(page, 'ArrowRight', 400);
      await hold(page, 'ArrowUp', 300);
      await page.waitForFunction(() => {
        const b = window.xem.command({ cmd: 'binaural' });
        return b.first_person && (b.sources ?? []).filter((s) => s.kind === 'effect' && s.target).length === 3;
      }, null, { timeout: 60_000 });
      // The game held (the listener goes on taking the view's axes), its
      // state and the view's look to the native host.
      await page.evaluate(() => window.xem.setRunning(false));
      await frames(page, 6);
      const { look, heard, saved } = await page.evaluate(() => {
        const bytes = window.xem.exportSnapshot();
        let binary = '';
        for (let i = 0; i < bytes.length; i += 0x8000) binary += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
        return { look: window.xem.status().renderer.look, heard: window.xem.command({ cmd: 'binaural' }), saved: btoa(binary) };
      });
      const looked = join(SHOTS, 'first-person-field15-looked.state');
      writeFileSync(looked, Buffer.from(saved, 'base64'));
      const native = JSON.parse(execFileSync(FIRST_PERSON, ['--disc', USER_DISC, '--state', looked, '--look', `${look.yaw},${look.pitch}`], {
        env: { ...process.env, VK_ICD_FILENAMES: process.env.VK_ICD_FILENAMES ?? process.env.XEM_LAVAPIPE_ICD ?? '' },
        maxBuffer: 64 << 20,
      }).toString());
      assert.deepEqual(native.look, [look.yaw, look.pitch], 'the native view looks as the page\'s does');
      const there = native.binaural;
      // Each source by what it is (the native session's ids are its own).
      const effects = (b) => new Map(b.sources.filter((s) => s.kind === 'effect' && s.target).map((s) => [s.follows, s.target]));
      const places = (b) => new Map(b.sources.filter((s) => s.kind === 'music').map((s) => [`${s.song}/${s.part}`, s.placement]));
      const off = (a, b) => Math.max(Math.abs(a.yaw - b.yaw), Math.abs(a.pitch - b.pitch));
      const listener = off(heard.first_person, there.first_person);
      const [here, native3] = [effects(heard), effects(there)];
      const worst = Math.max(...[...here].map(([actor, t]) => {
        const n = native3.get(actor);
        assert.ok(n, `the native host places the emitter following actor ${actor} too`);
        return Math.max(Math.abs(((t.azimuth - n.azimuth + 540) % 360) - 180), Math.abs(t.elevation - n.elevation));
      }));
      console.log(`first-person listener: the page's ${JSON.stringify(heard.first_person)}, the native host's ${JSON.stringify(there.first_person)} `
        + `(${listener.toExponential(2)} degrees apart); its 3 emitters within ${worst.toExponential(2)} degrees`);
      assert.ok(listener < 1e-3, `the listener faces as the native host's: ${listener} degrees apart`);
      assert.equal(native3.size, 3);
      assert.ok(worst < 1e-3, `each emitter is heard where the native host hears it: ${worst} degrees apart`);
      assert.deepEqual(there.listening_position, heard.listening_position, 'from the same listening position');
      assert.deepEqual(places(there), places(heard), 'the music in the same places');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
