// Immersive play in a headset browser (local only: it needs the user's disc
// 1, the game module and xem-headless): Quest Browser's user agent with
// IWER's Meta Quest 3 as navigator.xr. The page renders through an
// XR-compatible WebGL2 context, Enter VR starts an immersive session whose
// frames run the game as the canvas's do (never per view) and draw it on a
// flat screen in each view; head tracking continues while the game is
// paused (and with it the Binaural mode's listener, the music's stage
// turning with the head, or staying put and heard against it with
// head-tracked music); with the head turned away the player's Recenter and
// then IWER's own recentring (its reference space's `reset`) each put the
// screen in front again; the settings' gameplay options reach the session
// (random battles off on the platforming maps, the others the original's),
// as an agent's change does; leaving returns to the canvas; and the game's RAM
// after the same frames is the same in and out of the headset, and the
// state exported in the headset is xem-headless's after the same frames
// byte for byte (the screen asks for no reveals), loads there to its RAM and
// goes on there as in the headset. First-person
// inspection (field 30, IWER's tracked hands): the underhand grab looks from
// within the scene through each view, a head turn changes the view while the
// player's Pause holds the game, the controllers' click and stick toggle and
// turn it then too and the Original renderer ends it, and the grab again
// gives the screen back at its size; on as the session ends, the canvas
// goes on with the view, and a new session looks from within the scene at
// once (the canvas and the headset share one view). The title movie in the headset with
// wider framing on, the same frame shown twice from one snapshot: rebuilt
// from what the MDEC decoded under the Enhanced movies setting
// (status().renderer.movie), the device's own picture under Original, on the
// screen in both views, the screen without wider framing's sides
// (status().xr.screen) in the display's shape, the game's RAM the same; the
// title's 15-bit screen after it has the sides. A tour of the game's scenes
// (states made natively: field 30, field 12's dialogue, the field's menu,
// the world map, a battle on foot and in Gears, the title movie and the
// title screen, each shown with the game running): the screen in front in
// both views, the left hand's underhand grab recognised everywhere and
// looking from within the scene only in the field and its dialogue
// (pictures to build/web-test/xr-tour-*.png), and leaving the field for a
// battle ending the view. Saves and time controls in the session: the
// game's own save to card 2 (the slice's save route, its last frames played
// in the headset) reaches IndexedDB during the session, the panel's Save
// state and Load state by a controller's ray, fast-forward by the
// controllers' Back+RT and off by the panel's button, Back+LT rewinding
// through Settings' pause under the HUD's icon, the panel's Hold to rewind
// held the same way by a controller's select and, with hands alone (IWER's
// tracked hands, the head aiming), by a held pinch, the head turning off
// the button meanwhile, and the backups' pickers refused.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HAND_MODEL, HEADLESS, IWER, NO_WEBGPU, QUEST, QUEST_BROWSER, ROOT, SHOTS, USER_DISC, WEB, native, rotate, start, status } from './support.mjs';

const available = IWER && existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
mkdirSync(SHOTS, { recursive: true });

const xr = async (page) => (await status(page)).page.xr;
// xem-core's platforming maps (encounters.rs's PLATFORMING_AREAS), ascending.
const platformingMaps = () => {
  const table = readFileSync(join(ROOT, 'src/runtime/crates/xem-core/src/encounters.rs'), 'utf8').match(/PLATFORMING_AREAS: [^=]*= &\[([\s\S]*?)\n\];/)[1];
  return [...table.replace(/"[^"]*"/g, '').matchAll(/\d+/g)].map((m) => Number(m[0])).sort((a, b) => a - b);
};

describe('immersive play in a headset browser', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  test('the game runs on a virtual screen in both views', { timeout: 600_000 }, async () => {
    const state = join(SHOTS, 'xr-field12.state');
    const musicState = join(SHOTS, 'xr-field15.state');
    for (const [map, frames, path] of [[12, 400, state], [15, 600, musicState]]) {
      execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
        input: [{ cmd: 'enter_field', map }, { cmd: 'frames', count: frames }, { cmd: 'state_save', path }]
          .map((c) => JSON.stringify(c)).join('\n') + '\n',
      });
    }
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST }] });
    try {
      // A quarter-size layer: software WebGL draws it fast enough to show the
      // game's own pace.
      const page = await browser.page('?xrScale=0.25');
      const info = await status(page);
      assert.equal(info.renderer.backend, 'webgl2-xr', 'an XR-compatible WebGL2 renderer');
      assert.ok(info.page.xr.supported && info.page.xr.capable);
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      const load = (base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
      const base64 = readFileSync(state).toString('base64');
      await page.evaluate(load, base64);

      // The same frames outside the headset first, for the state check.
      await page.evaluate(() => window.xem.runFrames(120));
      const flatHash = await page.evaluate(() => window.xem.memoryHash());
      await page.evaluate(load, base64);

      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().page.xr.frames > 30, null, { timeout: 60_000 });
      assert.equal((await xr(page)).views, 2, 'two views');
      // The automation's frames in the headset end where the flat ones did.
      await page.evaluate(() => window.xem.runFrames(120));
      assert.equal(await page.evaluate(() => window.xem.memoryHash()), flatHash, 'the headset changes nothing in the game');
      // The state in the headset, exported: natively it is the headset's
      // RAM, and 120 frames there end where the headset's 120 end.
      const exported = join(SHOTS, 'xr-field12-export.xemstate');
      writeFileSync(exported, Buffer.from(await page.evaluate(() => Array.from(window.xem.exportSnapshot()))));
      await page.evaluate(() => window.xem.runFrames(120));
      const onward = await page.evaluate(() => window.xem.memoryHash());
      assert.equal(native([{ cmd: 'state_load', path: exported }, { cmd: 'memory_hash' }]), flatHash, 'the headset\'s state loads natively to its RAM');
      assert.equal(native([{ cmd: 'state_load', path: exported }, { cmd: 'frames', count: 120 }, { cmd: 'memory_hash' }]), onward,
        'and goes on natively as in the headset');
      // The screen without wider framing asks for no reveals: the whole
      // state is xem-headless's after the same frames.
      const same = join(SHOTS, 'xr-field12-native.xemstate');
      native([{ cmd: 'state_load', path: state }, { cmd: 'frames', count: 120 }, { cmd: 'state_save', path: same }]);
      assert.ok(readFileSync(exported).equals(readFileSync(same)), 'the headset\'s state is xem-headless\'s, byte for byte');

      // Running: game frames follow the clock, not the session's frame rate.
      await page.evaluate(() => window.xem.setRunning(true));
      const before = await status(page);
      const t0 = Date.now();
      await page.waitForTimeout(3000);
      const after = await status(page);
      const seconds = (Date.now() - t0) / 1000;
      const gameRate = (after.loop.gameFrames - before.loop.gameFrames) / seconds;
      const xrRate = (after.page.xr.frames - before.page.xr.frames) / seconds;
      console.log(`game ${gameRate.toFixed(1)} frames/s, session ${xrRate.toFixed(1)} frames/s`);
      assert.ok(gameRate > 40 && gameRate < 66, `the game runs near 60 frames a second (${gameRate})`);
      assert.ok(after.loop.xrFrames > before.loop.xrFrames, 'the session drives the frames');
      assert.equal(after.loop.animationFrames, before.loop.animationFrames, 'the canvas loop rests in the headset');

      // The gameplay options in the session: the settings' defaults reach
      // the game (random battles off on xem-core's platforming maps, every
      // other option the original's), and an agent's change is acknowledged
      // and reaches it in the session's next frames.
      const session = (await status(page)).session;
      assert.deepEqual(session.random_battles_off, platformingMaps(), 'random battles off on the platforming maps');
      assert.deepEqual([session.half_encounters, session.text_speed, session.auto_advance, session.movie_skip], [false, 0, 0, false],
        'every other option the original\'s');
      const original = await page.evaluate(() => window.xem.settings.apply({ RandomBattles: 'Original' }));
      assert.equal(original.ok, true, JSON.stringify(original));
      assert.equal(original.settings.random_battles, 'Original');
      await page.waitForFunction(() => window.xem.status().session.random_battles_off.length === 0, null, { timeout: 30_000 });
      assert.ok((await xr(page)).frames > after.page.xr.frames, 'in the session\'s frames');
      assert.equal((await page.evaluate(() => window.xem.settings.apply({ RandomBattles: 'NotWhilePlatforming' }))).ok, true);

      // Both views show the game screen at their centres.
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      const pixels = await page.evaluate(([w, h]) => window.xem.xr.readback([[Math.floor(w / 4), Math.floor(h / 2)], [Math.floor(3 * w / 4), Math.floor(h / 2)]]), layer);
      assert.ok(pixels, 'a readback');
      const [left, right] = [pixels.slice(0, 4), pixels.slice(4, 8)];
      const backdrop = (p) => Math.abs(p[0] - 26) + Math.abs(p[1] - 30) + Math.abs(p[2] - 38) < 24;
      console.log(`view centres ${left} / ${right}`);
      assert.ok(!backdrop(left) || !backdrop(right), 'the screen is in front of the viewer');

      // The Binaural mode's listener turns with the head.
      const binaural = await page.evaluate(() => window.xem.settings.apply({ Binaural: { ...window.xem.settings.get().settings.binaural, enabled: true } }));
      assert.equal(binaural.ok, true, JSON.stringify(binaural));
      await page.waitForFunction(() => window.xem.command({ cmd: 'binaural' }).head, null, { timeout: 30_000 });

      // Paused: head tracking and rendering continue, the game does not.
      await page.evaluate(() => window.xem.setRunning(false));
      const paused = await status(page);
      await page.evaluate(() => { window.xrDevice.position.set(0.2, 1.6, 0.1); });
      // A quarter turn to the right (-90 degrees about +Y).
      await page.evaluate(() => { window.xrDevice.quaternion.set(0, -Math.SQRT1_2, 0, Math.SQRT1_2); });
      await page.waitForTimeout(500);
      const moved = await status(page);
      assert.equal(moved.loop.gameFrames, paused.loop.gameFrames, 'the game stays paused');
      assert.ok(moved.page.xr.frames > paused.page.xr.frames + 5, 'the session keeps rendering');
      assert.notDeepEqual(moved.page.xr.viewer, paused.page.xr.viewer, 'the head is tracked');
      const head = (await page.evaluate(() => window.xem.command({ cmd: 'binaural' }))).head;
      assert.ok(Math.abs(head.yaw - 90) < 2, `the listener turned with the head, the game paused: ${JSON.stringify(head)}`);

      // The music's stage in field 15 (its song plays; field 12 has none),
      // the game paused: without head-tracked music it turns with the head
      // (each part where it was against the head); with it the stage stays
      // put and is heard turned the other way. Each part's target is where
      // it heads, at once, no sound being mixed.
      await page.evaluate(load, readFileSync(musicState).toString('base64'));
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => window.xem.command({ cmd: 'binaural' }).sources?.filter((s) => s.kind === 'music').length >= 4, null, { timeout: 30_000 });
      await page.evaluate(() => window.xem.setRunning(false));
      const AHEAD = [0, 0, 0, 1];
      const RIGHT = [0, -Math.SQRT1_2, 0, Math.SQRT1_2];
      const look = async (quaternion) => {
        await page.evaluate((q) => window.xrDevice.quaternion.set(...q), quaternion);
        await page.waitForTimeout(500);
        const status = await page.evaluate(() => window.xem.command({ cmd: 'binaural' }));
        return status.sources.filter((s) => s.kind === 'music' && s.target)
          .map((s) => [`${s.song}/${s.part}`, s.target.azimuth]).sort(([a], [b]) => a.localeCompare(b));
      };
      const turnedBy = (from, to, degrees) => from.length > 0 && from.length === to.length
        && from.every(([part, azimuth], i) => part === to[i][0] && Math.abs(((to[i][1] - azimuth - degrees + 540) % 360) - 180) < 2);
      const [still, turnedHead] = [await look(AHEAD), await look(RIGHT)];
      assert.ok(turnedBy(still, turnedHead, 0), `music turns with the head: ${JSON.stringify([still, turnedHead])}`);
      const tracked = await page.evaluate(() => window.xem.settings.apply({ Binaural: { ...window.xem.settings.get().settings.binaural, head_tracked_music: true } }));
      assert.equal(tracked.ok, true, JSON.stringify(tracked));
      // The hosts apply settings with each frame batch, the game running.
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => window.xem.command({ cmd: 'binaural' }).config.head_tracked_music, null, { timeout: 30_000 });
      await page.evaluate(() => window.xem.setRunning(false));
      const [stage, againstHead] = [await look(AHEAD), await look(RIGHT)];
      assert.ok(turnedBy(stage, againstHead, -90), `head-tracked music stays put, heard a quarter to the left: ${JSON.stringify([stage, againstHead])}`);
      console.log(`music parts: ${still.length} turning with the head, ${stage.length} staying put`);

      // Recentring the screen view: the head moved and turned a quarter to
      // the right leaves the screen out of both views; the player's
      // Recenter puts the seat at the head, facing its way, and the screen
      // in front of it again; the runtime's own recentring (IWER's: the
      // head levelled to the space's -Z, its reference space's `reset`)
      // puts the seat back at the space's origin, the screen in front.
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);
      const centres = async () => {
        await frames(6);
        const p = await page.evaluate(([w, h]) => window.xem.xr.readback([[Math.floor(w / 4), Math.floor(h / 2)], [Math.floor(3 * w / 4), Math.floor(h / 2)]]), layer);
        return [p.slice(0, 4), p.slice(4, 8)];
      };
      await page.evaluate(() => { window.xrDevice.position.set(0.2, 1.6, 0.1); window.xrDevice.quaternion.set(0, -Math.SQRT1_2, 0, Math.SQRT1_2); });
      const away = await centres();
      assert.ok(away.every(backdrop), `turned away, the screen is out of view: ${away}`);
      const turnedViewer = (await status(page)).page.xr.viewer;
      await page.evaluate(() => window.xem.appOperation({ Action: 'Recenter' }));
      const recentred = await centres();
      let seated = (await status(page)).xr;
      assert.equal(seated.recenters, 1, 'one Recenter');
      assert.ok(Math.abs(seated.seat.yaw + Math.PI / 2) < 1e-3, `the seat faces the head's way: ${JSON.stringify(seated.seat)}`);
      assert.ok(Math.hypot(...seated.seat.position.map((v, i) => v - turnedViewer[i])) < 0.01, `the seat at the head: ${JSON.stringify(seated.seat)} for ${turnedViewer}`);
      assert.ok(!backdrop(recentred[0]) || !backdrop(recentred[1]), `recentred, the screen is in front: ${recentred}`);
      await page.evaluate(() => window.xrDevice.recenter());
      const reset = await centres();
      seated = (await status(page)).xr;
      assert.equal(seated.resets, 1, 'the reference space reset reached the app');
      assert.ok(Math.abs(seated.seat.yaw) < 1e-6 && seated.seat.position.every((v) => Math.abs(v) < 1e-6), `the seat at the origin: ${JSON.stringify(seated.seat)}`);
      assert.ok(!backdrop(reset[0]) || !backdrop(reset[1]), `after the runtime's recentring the screen is in front: ${reset}`);
      console.log(`recentring: turned away ${away}, Recenter ${recentred} (seat ${JSON.stringify((await status(page)).xr.seat)}), reset ${reset}`);

      // Leaving returns to the canvas (the emulator's overlay covers the
      // page's button while a session runs).
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 30_000 });
      assert.equal((await page.evaluate(() => window.xem.command({ cmd: 'binaural' }))).head, null, 'no head out of the headset');
      const flat = (await status(page)).loop.animationFrames;
      await page.waitForTimeout(300);
      assert.ok((await status(page)).loop.animationFrames > flat, 'the canvas draws again');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('first-person inspection looks from within the scene through each view, paused or not', { timeout: 600_000 }, async () => {
    // Field 30 with Fei walked into the room (as xr-game-smoke.sh does).
    const state = join(SHOTS, 'xr-field30.state');
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
      await page.evaluate((base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0))), base64);
      await page.evaluate(() => window.xem.setRunning(true));
      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().xr.input && window.xem.status().page.xr.frames > 20, null, { timeout: 60_000 });
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);
      const look = async () => (await status(page)).renderer.look;
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      // Nine points about each view's centre (layer pixels from the bottom left).
      const points = [0.25, 0.75].flatMap((u) => [-0.04, 0, 0.04].flatMap((dx) => [-0.08, 0, 0.08]
        .map((dy) => [Math.floor((u + dx) * layer[0]), Math.floor((0.5 + dy) * layer[1])])));
      const read = () => page.evaluate((points) => window.xem.xr.readback(points), points);
      // The session's and the game's rates over two seconds (recorded, not checked).
      const rates = async () => {
        const before = await status(page);
        const t0 = Date.now();
        await page.waitForTimeout(2000);
        const after = await status(page);
        const seconds = (Date.now() - t0) / 1000;
        return `session ${((after.page.xr.frames - before.page.xr.frames) / seconds).toFixed(1)} frames/s, `
          + `game ${((after.loop.gameFrames - before.loop.gameFrames) / seconds).toFixed(1)} frames/s`;
      };
      const screen = await read();
      console.log(`as the screen: ${await rates()}`);

      // The controllers' Back (the right stick's click) released alone
      // toggles it; the right stick turns the view 30 degrees a push.
      const click = async () => {
        await page.evaluate(() => window.xrDevice.controllers.right.updateButtonValue('thumbstick', 1));
        await frames(3);
        await page.evaluate(() => window.xrDevice.controllers.right.updateButtonValue('thumbstick', 0));
        await frames(3);
      };
      await click();
      let info = await status(page);
      assert.equal(info.xr.inspecting, true, 'the right stick\'s click turned inspection on');
      assert.equal(info.renderer.firstPerson, true);
      await page.waitForFunction(() => window.xem.status().xr.within, null, { timeout: 30_000 });
      await page.evaluate(() => window.xrDevice.controllers.right.updateAxes('thumbstick', 1, 0));
      await frames(6);
      await page.evaluate(() => window.xrDevice.controllers.right.updateAxes('thumbstick', 0, 0));
      await frames(3);
      assert.ok(Math.abs((await look()).yaw - Math.PI / 6) < 1e-3, `one push, one 30-degree step: ${JSON.stringify(await look())}`);
      assert.equal((await look()).pitch, 0, 'the stick does not pitch: the head does');
      // The Binaural listener looks along the view (turned right: right of
      // the game's camera, in its view space), the head turning it once more.
      const binaural = (await page.evaluate(() => window.xem.settings.get())).settings.binaural;
      assert.equal((await page.evaluate((b) => window.xem.settings.apply({ Binaural: { ...b, enabled: true } }), binaural)).ok, true);
      await frames(6);
      let listening = await page.evaluate(() => window.xem.command({ cmd: 'binaural' }));
      assert.ok(listening.first_person && listening.first_person.yaw > 10 && listening.head,
        `the listener faces the view's turn, the head on it: ${JSON.stringify(listening)}`);
      await click();
      assert.equal((await status(page)).xr.inspecting, false, 'clicked again, off');
      await frames(3);
      listening = await page.evaluate(() => window.xem.command({ cmd: 'binaural' }));
      assert.equal(listening.first_person, null, 'the listener has the camera\'s view again');

      // The hands: the underhand grab (palm up) looks from within the scene.
      await page.evaluate(() => {
        const device = window.xrDevice;
        device.primaryInputMode = 'hand';
        window.handModel.install(device);
        device.hands.left.position.set(-0.15, 1.4, -0.3);
        device.hands.right.position.set(0.15, 1.4, -0.3);
        window.handModel.set(device, 'left', 'open');
        window.handModel.set(device, 'right', 'open');
      });
      await frames(6);
      const grab = async (palmUp) => {
        await page.evaluate((up) => window.xrDevice.hands.right.quaternion.set(0, 0, up ? 1 : 0, up ? 0 : 1), palmUp);
        await frames(3);
        for (const name of ['open', 'half', 'fist', 'open']) {
          await page.evaluate((name) => window.handModel.set(window.xrDevice, 'right', name), name);
          await frames(3);
        }
      };
      await grab(true);
      assert.equal((await status(page)).xr.inspecting, true, 'the underhand grab turned inspection on');
      await page.waitForFunction(() => window.xem.status().xr.within, null, { timeout: 30_000 });
      await frames(3);
      const ahead = await read();
      console.log(`view centres: the screen ${screen.slice(0, 4)}, from within ${ahead.slice(0, 4)}`);
      assert.notDeepEqual(ahead, screen, 'the views show the scene from within, not the screen');
      console.log(`from within the scene: ${await rates()}`);

      // The player's Pause (the overhand grab): the game stands still and
      // a quarter turn of the head to the right turns the view.
      await grab(false);
      info = await status(page);
      assert.equal(info.xr.pauses.player, true, 'the Pause grab paused the game');
      const held = info.loop.gameFrames;
      await frames(3);
      const still = await read();
      await page.evaluate(() => window.xrDevice.quaternion.set(0, -Math.SQRT1_2, 0, Math.SQRT1_2));
      await frames(8);
      const turned = await read();
      info = await status(page);
      assert.equal(info.loop.gameFrames, held, 'the game stays paused');
      assert.equal(info.xr.within, true, 'the views go on looking from within the scene');
      assert.notDeepEqual(turned, still, 'turning the head turns the view while the game is paused');
      await page.evaluate(() => window.xrDevice.quaternion.set(0, 0, 0, 1));
      await frames(8);
      assert.deepEqual(await read(), still, 'the head back, the view is too');

      // Still paused, the controllers (IWER's, the hands put down for a
      // while) toggle the view and turn it, and the Original renderer ends
      // it; the game's frames stand still throughout.
      await page.evaluate(() => { window.xrDevice.primaryInputMode = 'controller'; });
      await frames(6);
      await click();
      assert.equal((await status(page)).xr.inspecting, false, 'paused, the right stick\'s click turned inspection off');
      await click();
      assert.equal((await status(page)).xr.inspecting, true, 'paused, the click turned it on again');
      await page.waitForFunction(() => window.xem.status().xr.within, null, { timeout: 30_000 });
      await page.evaluate(() => window.xrDevice.controllers.right.updateAxes('thumbstick', 1, 0));
      await frames(6);
      await page.evaluate(() => window.xrDevice.controllers.right.updateAxes('thumbstick', 0, 0));
      await frames(3);
      assert.ok(Math.abs((await look()).yaw - Math.PI / 6) < 1e-3, `paused, one push turned the view 30 degrees: ${JSON.stringify(await look())}`);
      const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      await page.evaluate((g) => window.xem.settings.apply({ Graphics: { ...g, renderer: 'Original' } }), graphics);
      await frames(3);
      info = await status(page);
      assert.equal(info.xr.inspecting, false, 'paused, the Original renderer ended the view');
      assert.equal(info.xr.within, false, 'the views show the screen again');
      await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), graphics);
      await click();
      info = await status(page);
      assert.equal(info.xr.inspecting, true, 'on again under Ex Machina');
      assert.equal(info.xr.pauses.player, true, 'the player\'s Pause holds');
      assert.equal(info.loop.gameFrames, held, 'the game stayed paused throughout');
      await page.evaluate(() => { window.xrDevice.primaryInputMode = 'hand'; });
      await frames(6);

      // Resumed, then the underhand grab again: the screen, at its size.
      await grab(false);
      assert.equal((await status(page)).xr.pauses.player, false, 'resumed');
      await grab(true);
      info = await status(page);
      assert.equal(info.xr.inspecting, false, 'the underhand grab again turned inspection off');
      assert.equal(info.xr.within, false, 'the screen again');
      assert.equal(info.xr.scale, 1, 'the screen kept its size');

      // On again as the session ends: the canvas goes on with the view (the
      // page has one view for the canvas and the headset).
      await grab(true);
      assert.equal((await status(page)).xr.inspecting, true);
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 30_000 });
      info = await status(page);
      assert.equal(info.renderer.firstPerson, true, 'the view carries out of the headset');
      await page.waitForTimeout(300);
      assert.ok((await status(page)).loop.animationFrames > info.loop.animationFrames, 'the canvas draws it');
      // And into a new session: Enter VR again, its views look from within
      // the scene with no toggle.
      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().page.xr.active, null, { timeout: 60_000 });
      await page.waitForFunction(() => window.xem.status().xr.within, null, { timeout: 30_000 });
      info = await status(page);
      assert.equal(info.xr.inspecting, true, 'the view carries into a new session');
      assert.equal(info.xr.within, true, 'the new session\'s views look from within the scene');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('a tour of the game\'s scenes on the screen and, where it shows, from within the scene', { timeout: 900_000 }, async () => {
    // Each scene reached natively by the game's own commands (as the
    // validation sheet reaches them), saved, then shown in the headset.
    const press = (button) => [{ cmd: 'press', buttons: [button], frames: 1 }, { cmd: 'frames', count: 20 }];
    const battle = (gears) => [{ cmd: 'frames', count: 60 }, { cmd: 'write', path: 'mode_battle_standalone', value: 1 }, { cmd: 'boot_mode', mode: 2 },
      { cmd: 'frames', count: 900 }, ...(gears ? ['down', 'down', 'cross', 'right', 'cross', 'right', 'cross'].flatMap(press) : []),
      ...press('start'), { cmd: 'frames', count: 1500 }];
    const field = (map) => [{ cmd: 'enter_field', map }, { cmd: 'frames', count: 400 }];
    // Whether first-person inspection shows there (a field but its menu).
    const scenes = [
      ['field', [...field(30), { cmd: 'press', buttons: ['down'], frames: 60 }, { cmd: 'frames', count: 30 }], true],
      ['dialogue', field(12), true],
      ['menu', [...field(30), ...press('square'), { cmd: 'frames', count: 90 }], false],
      ['world-map', [{ cmd: 'boot_mode', mode: 3 }, { cmd: 'frames', count: 900 }], false],
      ['battle-on-foot', battle(false), false],
      ['battle-gear', battle(true), false],
      ['movie', [{ cmd: 'frames', count: 840 }], false],
      ['title', [{ cmd: 'frames', count: 1500 }], false],
    ];
    const states = new Map(scenes.map(([name, commands]) => {
      const path = join(SHOTS, `xr-tour-${name}.state`);
      execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
        input: [...commands, { cmd: 'state_save', path }].map((c) => JSON.stringify(c)).join('\n') + '\n',
      });
      return [name, readFileSync(path).toString('base64')];
    }));
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST + HAND_MODEL }] });
    try {
      const page = await browser.page('?xrScale=0.25');
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().page.xr.frames > 10, null, { timeout: 60_000 });
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);
      // The left hand's underhand grab (IWER's tracked hands), the right
      // hand resting open.
      await page.evaluate(() => {
        const device = window.xrDevice;
        device.primaryInputMode = 'hand';
        window.handModel.install(device);
        device.hands.left.position.set(-0.15, 1.4, -0.3);
        device.hands.right.position.set(0.15, 1.4, -0.3);
        window.handModel.set(device, 'left', 'open');
        window.handModel.set(device, 'right', 'open');
      });
      await frames(6);
      const underhand = async () => {
        await page.evaluate(() => window.xrDevice.hands.left.quaternion.set(0, 0, 1, 0));
        await frames(3);
        for (const name of ['open', 'half', 'fist', 'open']) {
          await page.evaluate((name) => window.handModel.set(window.xrDevice, 'left', name), name);
          await frames(3);
        }
        await page.evaluate(() => window.xrDevice.hands.left.quaternion.set(0, 0, 0, 1));
        await frames(3);
      };
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      // Nine points about each view's centre (layer pixels from the bottom left).
      const points = [0.25, 0.75].flatMap((u) => [-0.06, 0, 0.06].flatMap((dx) => [-0.1, 0, 0.1]
        .map((dy) => [Math.floor((u + dx) * layer[0]), Math.floor((0.5 + dy) * layer[1])])));
      const backdrop = (p) => Math.abs(p[0] - 26) + Math.abs(p[1] - 30) + Math.abs(p[2] - 38) < 24;
      const shown = async () => {
        const bytes = await page.evaluate((points) => window.xem.xr.readback(points), points);
        return Array.from({ length: points.length }, (_, i) => bytes.slice(i * 4, i * 4 + 4));
      };
      const load = (base64) => page.evaluate((base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0))), base64);
      const seen = [];
      await page.evaluate(() => window.xem.setRunning(true));
      for (const [name, , inspectable] of scenes) {
        await load(states.get(name));
        // The game runs on from the restore, drawing the scene.
        await frames(10);
        const screen = await shown();
        let info = await status(page);
        await page.screenshot({ path: join(SHOTS, `xr-tour-${name}.png`) });
        const visible = screen.filter((p) => !backdrop(p)).length;
        assert.ok(visible >= 12, `${name}: the screen shows in front of the viewer (${visible} of 18 points: ${screen.map((p) => p.slice(0, 3)).join(' ')})`);
        assert.equal(info.xr.within, false, `${name}: the screen`);
        // The underhand grab: from within the scene in a field, nothing
        // elsewhere (counted as the grab all the same).
        const grabs = info.xr.counts.inspect;
        await underhand();
        info = await status(page);
        assert.equal(info.xr.counts.inspect, grabs + 1, `${name}: the left hand's underhand grab was recognised`);
        assert.equal(info.xr.inspecting, inspectable, `${name}: first-person inspection ${inspectable ? 'shows' : 'does not show'} here`);
        let within = false;
        if (inspectable) {
          // From within once a game frame records where the character stands.
          await page.waitForFunction(() => window.xem.status().xr.within, null, { timeout: 30_000 });
          await frames(3);
          info = await status(page);
          within = info.xr.within;
          assert.notDeepEqual(await shown(), screen, `${name}: not the screen`);
          await page.screenshot({ path: join(SHOTS, `xr-tour-${name}-within.png`) });
          await underhand();
          assert.equal((await status(page)).xr.inspecting, false, `${name}: the grab again gives the screen back`);
        }
        seen.push(`${name}: screen ${visible}/18${inspectable ? `, from within ${within}` : ''}, movie ${info.renderer.movie}`);
      }
      console.log(`tour: ${seen.join('; ')}`);
      // On from within the field, then a battle: the field is left, so is
      // the view; the battle shows on the screen.
      await load(states.get('field'));
      await frames(10);
      await underhand();
      assert.equal((await status(page)).xr.inspecting, true);
      await page.waitForFunction(() => window.xem.status().xr.within, null, { timeout: 30_000 });
      await load(states.get('battle-on-foot'));
      await frames(10);
      const info = await status(page);
      assert.equal(info.xr.inspecting, false, 'leaving the field ends first-person inspection');
      assert.equal(info.xr.within, false, 'the battle shows on the screen');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('a movie on the virtual screen is rebuilt under Enhanced and the device\'s own under Original', { timeout: 600_000 }, async () => {
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST }] });
    try {
      const page = await browser.page('?xrScale=0.25');
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      // Wider framing on (before the snapshot below: a new configuration
      // restarts the draw trace): a rebuilt movie frame's screen has no
      // sides.
      const wide = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      assert.equal((await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), { ...wide, widescreen: true })).error, undefined);
      // The title movie, bright from about frame 840.
      await page.evaluate(() => window.xem.runFrames(840));
      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().page.xr.frames > 10, null, { timeout: 60_000 });
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);
      // Each immersive frame takes the game frames run since the last.
      await page.evaluate(() => window.xem.runFrames(8));
      await frames(3);
      assert.equal((await page.evaluate(() => window.xem.snapshot('movie'))).error, undefined);
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      const centres = [[Math.floor(layer[0] / 4), Math.floor(layer[1] / 2)], [Math.floor(3 * layer[0] / 4), Math.floor(layer[1] / 2)]];
      const backdrop = (p) => Math.abs(p[0] - 26) + Math.abs(p[1] - 30) + Math.abs(p[2] - 38) < 24;
      const shown = {};
      for (const movies of ['Enhanced', 'Original']) {
        await page.evaluate(() => window.xem.restore('movie'));
        const graphics = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
        const applied = await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), { ...graphics, filtering: { ...graphics.filtering, movies } });
        assert.equal(applied.error, undefined, JSON.stringify(applied));
        await page.evaluate(() => window.xem.runFrames(12));
        await frames(3);
        const pixels = await page.evaluate((points) => window.xem.xr.readback(points), centres);
        const info = await status(page);
        shown[movies] = {
          movie: info.renderer.movie,
          screen: info.xr.screen,
          centres: [pixels.slice(0, 4), pixels.slice(4, 8)],
          hash: await page.evaluate(() => window.xem.memoryHash()),
        };
      }
      console.log(`view centres: Enhanced ${shown.Enhanced.centres.join(' / ')}, Original ${shown.Original.centres.join(' / ')}`);
      console.log(`screens: Enhanced ${JSON.stringify(shown.Enhanced.screen)}, Original ${JSON.stringify(shown.Original.screen)}`);
      assert.equal(shown.Enhanced.movie, true, 'Enhanced shows the movie frame rebuilt in the headset');
      assert.equal(shown.Original.movie, false, 'Original shows the device\'s own picture');
      for (const movies of ['Enhanced', 'Original']) {
        assert.ok(shown[movies].centres.every((p) => !backdrop(p)), `${movies}: the movie is on the screen in both views`);
        const { size, display, side } = shown[movies].screen;
        assert.deepEqual([display, side], [[320, 216], 0], `${movies}: the screen shows the display alone, without wider framing's sides`);
        assert.ok(Math.abs(size[1] / size[0] - 216 / 320) < 0.005, `${movies}: the screen has the display's shape (${size})`);
      }
      assert.deepEqual(shown.Enhanced.hash, shown.Original.hash, 'neither setting changes the game');
      // The title's 15-bit screen after the movie (from game frame 1429)
      // has wider framing's sides.
      await page.evaluate(() => window.xem.runFrames(600));
      await frames(3);
      const title = await status(page);
      console.log(`the title's screen: ${JSON.stringify(title.xr.screen)}`);
      assert.equal(title.renderer.movie, false, 'the title is no movie frame');
      assert.ok(title.xr.screen.side > 0, `the title's screen has wider framing's sides (${title.xr.screen.side})`);
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('saves and time controls work in the session, and the game\'s card saves reach IndexedDB there', { timeout: 900_000 }, async () => {
    // The slice's save route natively up to its card writes, card 2
    // unformatted as on the route; the session plays the rest.
    const route = JSON.parse(readFileSync(join(ROOT, 'tests', 'reference-inputs', 'slice-save.json'), 'utf8'));
    const FROM = 10100;
    const cards = mkdtempSync(join(tmpdir(), 'xem-xr-cards-'));
    writeFileSync(join(cards, 'card2.mcd'), Buffer.alloc(0x20000));
    const state = join(SHOTS, 'xr-save-route.state');
    execFileSync(HEADLESS, ['--disc', USER_DISC, '--cards', cards, '--control'], {
      input: [{ cmd: 'schedule', commands: route.commands }, { cmd: 'run_until', until: { frames: FROM }, max_frames: FROM + 100 },
        { cmd: 'state_save', path: state }].map((c) => JSON.stringify(c)).join('\n') + '\n',
    });
    assert.ok(readFileSync(join(cards, 'card2.mcd')).every((b) => b === 0), 'the route writes card 2 only after frame ' + FROM);
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST + HAND_MODEL }] });
    try {
      let page = await browser.page();
      await page.evaluate(() => window.xem.cards.store(1, new Uint8Array(0x20000)));
      await page.close();
      page = await browser.page('?xrScale=0.25');
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      // With rewind history, as the player's own play has it.
      await page.evaluate(() => window.xem.boot({ rewind: true }));
      await page.evaluate((base64) => window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0))),
        readFileSync(state).toString('base64'));
      assert.equal((await status(page)).session.frames, FROM);
      const scheduled = await page.evaluate((commands) => window.xem.command({ cmd: 'schedule', commands }),
        route.commands.filter(([at]) => at >= FROM));
      assert.equal(scheduled.error, undefined, JSON.stringify(scheduled));
      await page.evaluate(() => { const p = window.xrDevice.position; window.xemHeadStart = [p.x, p.y, p.z]; });
      await page.click('#vr');
      await page.waitForFunction(() => window.xem.status().page.xr.frames > 20, null, { timeout: 60_000 });
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);

      // The game formats card 2 and saves to it in the session's frames; the
      // page writes the card to IndexedDB after them, the session going on.
      let info = await status(page);
      const [canvas, written] = [info.loop.animationFrames, info.page.cards.written];
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction((end) => window.xem.status().session.frames >= end, route.frames, { timeout: 300_000 });
      await page.waitForFunction(() => { const s = window.xem.status(); return s.page.cards.pending === 0 && !s.cards.unsaved; }, null, { timeout: 30_000 });
      info = await status(page);
      assert.equal(info.page.xr.active, true, 'still in the session');
      assert.equal(info.loop.animationFrames, canvas, 'the canvas loop rested: the session\'s frames wrote the card');
      assert.ok(info.page.cards.written > written, `IndexedDB took the game's writes (${written} -> ${info.page.cards.written})`);
      const card = await page.evaluate(async () => Array.from(await window.xem.cards.stored(1)));
      assert.equal(String.fromCharCode(card[0], card[1]), 'MC', 'card 2 formatted');
      assert.equal(card[128], 0x51, 'and holding the save');
      assert.deepEqual(card, await page.evaluate(() => Array.from(window.xem.cards.image(1))), 'IndexedDB holds the card the game wrote');

      // Settings by the right controller's ray and trigger: the panel's
      // logical pixel where the session says the panel stands
      // (status().xr.panel: its centre, orientation and size in the
      // session's space, whose origin is the head as the session began;
      // xem-web's render::xr_panel_quad, below the screen). At this
      // viewport the panel is the canvas's, 640x560 with its sidebar.
      assert.deepEqual(info.xr.panelSize, [640, 560]);
      const aim = async ([x, y]) => {
        const { panelSize: [width, height], panel: { position, orientation, size } } = (await status(page)).xr;
        const target = rotate(orientation, [(x / width - 0.5) * size[0], (0.5 - y / height) * size[1], 0]).map((v, i) => v + position[i]);
        await page.evaluate((target) => {
          const from = [0.2, -0.3, -0.3];
          const toward = target.map((t, i) => t - from[i]);
          const [dx, dy, dz] = toward.map((c) => c / Math.hypot(...toward));
          // The turn taking -Z to that direction, about -Z x direction.
          const [ax, ay] = [dy, -dx];
          const s = Math.sin(Math.acos(-dz) / 2) / (Math.hypot(ax, ay) || 1);
          const [right, head] = [window.xrDevice.controllers.right, window.xemHeadStart];
          right.position.set(head[0] + from[0], head[1] + from[1], head[2] + from[2]);
          right.quaternion.set(ax * s, ay * s, 0, Math.cos(Math.acos(-dz) / 2));
        }, target);
      };
      const pull = async (point) => {
        await aim(point);
        await frames(3);
        await page.evaluate(() => window.xrDevice.controllers.right.updateButtonValue('trigger', 1));
        await frames(3);
        await page.evaluate(() => window.xrDevice.controllers.right.updateButtonValue('trigger', 0));
        await frames(5);
      };
      const [SAVES, TIME, SAVE_STATE, LOAD_STATE, FAST_FORWARD] = [[82, 294], [82, 254], [296, 167], [505, 167], [547, 376]];
      // The history full (its 30 s of play) before Settings' pause, which
      // is no play time: the pause takes no states off it.
      await page.waitForFunction(() => window.xem.status().session.rewind.available_seconds >= 28.5, null, { timeout: 90_000 });
      const history = (await status(page)).session.rewind;
      await page.evaluate(() => window.xem.panel.show(true));
      await frames(4);
      await pull(SAVES);
      assert.equal((await status(page)).renderer.panelSection, 5, 'the ray chose Saves');
      const saved = await page.evaluate(() => window.xem.memoryHash());
      await pull(SAVE_STATE);
      await page.waitForFunction(() => window.xem.status().renderer.hud?.notice === 'State saved', null, { timeout: 30_000 });
      const record = await page.evaluate(async () => { const r = await window.xem.load('slot0'); return r && { verified: r.verified, bytes: r.bytes }; });
      assert.ok(record?.verified, `IndexedDB holds the state, its checksum verified: ${JSON.stringify(record)}`);
      assert.equal(await page.evaluate(() => window.xem.memoryHash()), saved, 'Settings held the game while it saved');
      await page.waitForTimeout(4000);

      // Settings closed, the game at its own pace; then the controllers'
      // Back+RT (the right stick's click with the right grip) fast-forwards
      // it, at about the factor the session paces it at.
      await page.evaluate(() => window.xem.panel.show(false));
      await frames(10);
      const kept = (await status(page)).session.rewind;
      console.log(`Settings' pause: ${history.states} states from game frame ${history.oldest_frame} before, ${kept.states} from ${kept.oldest_frame} after`);
      assert.ok(kept.states >= history.states - 1, `Settings' pause took no play time off the history: ${history.states} states before it, ${kept.states} after`);
      const rate = async () => {
        const [before, t0] = [await status(page), Date.now()];
        const factors = [];
        while (Date.now() - t0 < 3000) {
          await page.waitForTimeout(250);
          const s = (await status(page)).session;
          factors.push(s.fast_forward ? s.fast_forward_pace ?? s.fast_forward_rate.Fixed : 1);
        }
        const after = await status(page);
        return { rate: (after.loop.gameFrames - before.loop.gameFrames) / ((Date.now() - t0) / 1000), factor: factors.reduce((a, b) => a + b) / factors.length };
      };
      const normal = await rate();
      await page.evaluate(() => { const r = window.xrDevice.controllers.right; r.updateButtonValue('thumbstick', 1); r.updateButtonValue('squeeze', 1); });
      await frames(3);
      await page.evaluate(() => { const r = window.xrDevice.controllers.right; r.updateButtonValue('thumbstick', 0); r.updateButtonValue('squeeze', 0); });
      await frames(3);
      info = await status(page);
      assert.equal(info.session.fast_forward, true, 'Back+RT turned fast-forward on');
      assert.equal(info.xr.inspecting, false, 'Back with RT is a chord, not inspection\'s toggle');
      const fast = await rate();
      // The session paces a fixed rate at what the host reaches (a loaded
      // machine's software WebGL reaches less than 10x).
      const share = fast.rate / normal.rate / fast.factor;
      console.log(`1x ${normal.rate.toFixed(1)} frames/s; fast-forward ${fast.rate.toFixed(1)} frames/s at a mean factor of ${fast.factor.toFixed(2)} (${share.toFixed(2)} of it)`);
      assert.ok(fast.factor > 1.2 && fast.rate > 1.15 * normal.rate && share > 0.7 && share < 1.3,
        `fast-forward ran ${fast.rate.toFixed(1)} frames a second against ${normal.rate.toFixed(1)} at a mean factor of ${fast.factor.toFixed(2)}`);
      assert.equal((await status(page)).renderer.hud.fast_forward, true, 'the HUD shows fast-forward');

      // The panel's Stop fast-forward (Time), then Load state (Saves) gives
      // back the saved game.
      await page.evaluate(() => window.xem.panel.show(true));
      await frames(4);
      await pull(TIME);
      await pull(FAST_FORWARD);
      assert.equal((await status(page)).session.fast_forward, false, 'the panel\'s Stop fast-forward turned it off');

      // Back+LT (the right stick's click with the left grip) held with the
      // panel shown rewinds through Settings' pause under the HUD's icon;
      // let go, Settings hold the game again.
      const chord = (on) => page.evaluate((on) => {
        window.xrDevice.controllers.right.updateButtonValue('thumbstick', on);
        window.xrDevice.controllers.left.updateButtonValue('squeeze', on);
      }, on);
      const { frames: ahead, rewind } = (await status(page)).session;
      console.log(`Back+LT: the history holds ${rewind.states} states from game frame ${rewind.oldest_frame}`);
      await chord(1);
      await page.waitForFunction(() => window.xem.status().renderer.hud?.rewinding, null, { timeout: 30_000 });
      await page.waitForFunction((ahead) => window.xem.status().session.frames < ahead - 30, ahead, { timeout: 30_000 });
      await chord(0);
      await frames(5);
      const rewound = (await status(page)).session.frames;
      await frames(20);
      info = await status(page);
      console.log(`Back+LT with Settings shown rewound game frame ${ahead} to ${rewound}`);
      assert.equal(info.session.frames, rewound, 'Settings hold the game again once Back+LT is let go');
      assert.equal(info.renderer.hud.rewinding, false, 'and the HUD\'s rewind icon goes');
      assert.equal(info.renderer.panelVisible, true, 'Settings showed throughout');

      // Time's Hold to rewind held by the right controller's select, from
      // its selectstart to its selectend, then with hands alone (the head,
      // the default pointer source, looking at it) by a held pinch: each
      // rewinds through Settings' pause under the HUD's icon until it ends,
      // and Settings hold the game again after. The head turning off the
      // button mid-pinch changes nothing (the pointer moves nowhere while a
      // pinch holds it).
      const HOLD = [555, 279];
      const held = async (name, press, during, release) => {
        const { frames: ahead, rewind } = (await status(page)).session;
        console.log(`${name}: the history holds ${rewind.states} states from game frame ${rewind.oldest_frame}`);
        await press();
        await page.waitForFunction(() => window.xem.status().renderer.hud?.rewinding, null, { timeout: 30_000 });
        await during();
        // Still held after it: the game goes on back (a step at least).
        const then = (await status(page)).session.frames;
        await page.waitForFunction(({ ahead, then }) => {
          const frames = window.xem.status().session.frames;
          return frames < ahead - 120 && frames < then - 20;
        }, { ahead, then }, { timeout: 30_000 });
        await release();
        await frames(5);
        const rewound = (await status(page)).session.frames;
        await frames(20);
        const after = await status(page);
        console.log(`${name} on Hold to rewind rewound game frame ${ahead} to ${rewound}`);
        assert.equal(after.session.frames, rewound, `${name}: Settings hold the game again once it ends`);
        assert.equal(after.renderer.hud.rewinding, false, `${name}: and the HUD's rewind icon goes`);
      };
      await aim(HOLD);
      await frames(3);
      const trigger = (value) => page.evaluate((value) => window.xrDevice.controllers.right.updateButtonValue('trigger', value), value);
      await held('a controller\'s held select', () => trigger(1), async () => {}, () => trigger(0));
      // The head turned to look at a panel point (the session's origin is
      // the head as the session began, where it still is).
      const look = async ([x, y]) => {
        const { panelSize: [width, height], panel: { position, orientation, size } } = (await status(page)).xr;
        const target = rotate(orientation, [(x / width - 0.5) * size[0], (0.5 - y / height) * size[1], 0]).map((v, i) => v + position[i]);
        await page.evaluate((target) => {
          const [dx, dy, dz] = target.map((c) => c / Math.hypot(...target));
          const [ax, ay] = [dy, -dx];
          const s = Math.sin(Math.acos(-dz) / 2) / (Math.hypot(ax, ay) || 1);
          window.xrDevice.quaternion.set(ax * s, ay * s, 0, Math.cos(Math.acos(-dz) / 2));
        }, target);
      };
      await page.evaluate(() => {
        const device = window.xrDevice;
        window.handModel.install(device);
        device.primaryInputMode = 'hand';
        device.hands.left.position.set(-0.15, 1.4, -0.3);
        device.hands.right.position.set(0.15, 1.4, -0.3);
        window.handModel.set(device, 'left', 'open');
        window.handModel.set(device, 'right', 'open');
      });
      await look(HOLD);
      await frames(6);
      const pinch = (value) => page.evaluate((value) => window.xrDevice.hands.right.updatePinchValue(value), value);
      let pressedAt;
      await held('a held pinch', () => pinch(1), async () => {
        pressedAt = (await status(page)).xr.panelHit;
        await look([HOLD[0], HOLD[1] + 150]);
        await frames(6);
        assert.deepEqual((await status(page)).xr.panelHit, pressedAt, 'the pointer moves nowhere while the pinch holds it');
      }, () => pinch(0));
      const { panelClick, panelHit } = (await status(page)).xr;
      assert.ok(Math.hypot(panelClick[0] - HOLD[0], panelClick[1] - HOLD[1]) < 2, `the pinch pressed where it began: ${panelClick}`);
      assert.notDeepEqual(panelHit, pressedAt, 'let go, the pointer follows the head again');
      // The controllers again, the head ahead.
      await page.evaluate(() => { window.xrDevice.quaternion.set(0, 0, 0, 1); window.xrDevice.primaryInputMode = 'controller'; });
      await frames(6);
      await pull(SAVES);
      await pull(LOAD_STATE);
      await page.waitForFunction(() => window.xem.status().renderer.hud?.notice === 'State loaded', null, { timeout: 30_000 });
      assert.equal(await page.evaluate(() => window.xem.memoryHash()), saved, 'Load state gave back the saved game');

      // No file picker opens in the session: the backups' are refused.
      const refused = await page.evaluate(() => window.xem.appOperation({ Action: 'ExportState' }).then(() => null, (error) => error.message));
      assert.equal(refused, 'File pickers are available in windowed play.');
      assert.equal((await status(page)).page.xr.active, true);
      await page.evaluate(() => window.xem.xr.exit());
      await page.waitForFunction(() => !window.xem.status().page.xr.active, null, { timeout: 30_000 });
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
