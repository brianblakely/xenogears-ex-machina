// Immersive play in a headset browser (local only: it needs the user's disc
// 1, the game module and xem-headless): Quest Browser's user agent with
// IWER's Meta Quest 3 as navigator.xr. The page renders through an
// XR-compatible WebGL2 context, Enter VR starts an immersive session whose
// frames run the game as the canvas's do (never per view) and draw it on a
// flat screen in each view; head tracking continues while the game is
// paused (and with it the Binaural mode's listener, the music's stage
// turning with the head, or staying put and heard against it with
// head-tracked music); leaving returns to the canvas; and the game's RAM
// after the same frames is the same in and out of the headset.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, ROOT, SHOTS, USER_DISC, WEB, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
mkdirSync(SHOTS, { recursive: true });

const QUEST_BROWSER = 'Mozilla/5.0 (X11; Linux x86_64; Quest 3) AppleWebKit/537.36 (KHTML, like Gecko) '
  + 'OculusBrowser/37.0.0.0 SamsungBrowser/4.0 Chrome/132.0.0.0 VR Safari/537.36';
const IWER = existsSync(join(ROOT, 'src/runtime/web/node_modules/iwer/build/iwer.min.js'))
  ? readFileSync(join(ROOT, 'src/runtime/web/node_modules/iwer/build/iwer.min.js'), 'utf8') : '';
const QUEST = `${IWER}\n;window.xrDevice = new IWER.XRDevice(IWER.metaQuest3, { stereoEnabled: true });`
  + 'window.xrDevice.installRuntime({ forceInstall: true });';

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
});
