// Immersive play in a headset browser (local only: it needs the user's disc
// 1, the game module and xem-headless): Quest Browser's user agent with
// IWER's Meta Quest 3 as navigator.xr. The page renders through an
// XR-compatible WebGL2 context, Enter VR starts an immersive session whose
// frames run the game as the canvas's do (never per view) and draw it on a
// flat screen in each view; head tracking continues while the game is
// paused (and with it the Binaural mode's listener, the music's stage
// turning with the head, or staying put and heard against it with
// head-tracked music); leaving returns to the canvas; and the game's RAM
// after the same frames is the same in and out of the headset. First-person
// inspection (field 30, IWER's tracked hands): the underhand grab looks from
// within the scene through each view, a head turn changes the view while the
// player's Pause holds the game, the controllers' click and stick toggle and
// turn it then too and the Original renderer ends it, and the grab again
// gives the screen back at its size. The title movie in the headset with
// wider framing on, the same frame shown twice from one snapshot: rebuilt
// from what the MDEC decoded under the Enhanced movies setting
// (status().renderer.movie), the device's own picture under Original, on the
// screen in both views, the screen without wider framing's sides
// (status().xr.screen) in the display's shape, the game's RAM the same; the
// title's 15-bit screen after it has the sides.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HAND_MODEL, HEADLESS, IWER, NO_WEBGPU, QUEST, QUEST_BROWSER, SHOTS, USER_DISC, WEB, start, status } from './support.mjs';

const available = IWER && existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
mkdirSync(SHOTS, { recursive: true });

const xr = async (page) => (await status(page)).page.xr;

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
});
