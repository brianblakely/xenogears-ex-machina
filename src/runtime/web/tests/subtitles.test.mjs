// The movies' subtitles in browsers (local only: it needs the user's disc 1
// and the game module): the new-game movie's frame 2340, reached by the
// new-game route, shows its line (1.15.30, two lines) over the picture while
// Movie subtitles is on (status().renderer.subtitle) and none once it is
// off, the same frames run again from a snapshot: the two canvases differ
// only in the subtitle's band, which holds the game text palette's white
// and the dark blue of the edge around the glyphs. In
// a headset browser (Quest Browser's user agent, IWER's Meta Quest 3) the
// same frame on the virtual screen differs, on and off, only below the
// middle of both views, where the screen's bottom stands.

import assert from 'node:assert/strict';
import { existsSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { NO_WEBGPU, QUEST, QUEST_BROWSER, SHOTS, USER_DISC, WEB, decodePng, shot, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm'));
mkdirSync(SHOTS, { recursive: true });

const painted = (page) => page.evaluate(() => new Promise((done) => requestAnimationFrame(() => requestAnimationFrame(done))));
const ROUTE = [[901, 910, 'start'], [1300, 1310, 'start'], [1500, 1510, 'cross'], [1750, 1760, 'up'], [1810, 1820, 'cross']];

/** Disc 1 imported, subtitles on, booted, and the new-game route run to the
 *  new-game movie's frame 2340 (its line 1.15.30). `before` runs after the
 *  first frames (the page's first presentation starts the draw trace). */
async function toTheLine(page, before = async () => {}) {
  await page.setInputFiles('#file', USER_DISC);
  await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
  assert.equal((await page.evaluate(() => window.xem.settings.apply({ MovieSubtitles: true }))).error, undefined);
  await page.evaluate(() => window.xem.boot());
  const scheduled = await page.evaluate((route) => window.xem.command({
    cmd: 'schedule', commands: route.map(([at, end, button]) => [at, { cmd: 'press', buttons: [button], frames: end - at }]),
  }), ROUTE);
  assert.equal(scheduled.error, undefined, JSON.stringify(scheduled));
  await page.evaluate(() => window.xem.runFrames(600));
  await before();
  await page.evaluate(() => window.xem.runFrames(9_400));
  const reached = await page.evaluate(() => window.xem.run({ cmd: 'run_until', max_frames: 5_000, until: { path: 'field_movie_frame', equals: 2340 } }));
  assert.equal(reached.met, true, JSON.stringify(reached).slice(0, 400));
}

describe('movie subtitles', { skip: !available && 'needs the user\'s disc 1 and build/web/game' }, () => {
  test('a movie shows its line over the picture while the setting is on', { timeout: 900_000 }, async () => {
    const browser = await start();
    try {
      const page = await browser.page();
      await toTheLine(page, () => painted(page));
      assert.equal((await page.evaluate(() => window.xem.snapshot('line'))).error, undefined);
      const pictures = {};
      for (const on of [true, false]) {
        // The same frames from the same snapshot, the setting on and then off.
        await page.evaluate(() => window.xem.restore('line'));
        assert.equal((await page.evaluate((v) => window.xem.settings.apply({ MovieSubtitles: v }), on)).error, undefined);
        await page.evaluate(() => window.xem.runFrames(12));
        await painted(page);
        await painted(page);
        const info = await status(page);
        const session = await page.evaluate(() => window.xem.command({ cmd: 'status' }));
        assert.equal(session.subtitle?.line, '1.15.30', 'the line the movie shows, whatever the setting');
        assert.equal(info.renderer.subtitle, on, `a subtitle shows only while the setting is on (${on})`);
        const png = await page.locator('canvas').screenshot({ path: shot(`subtitles-${on ? 'on' : 'off'}.png`) });
        pictures[on] = { image: decodePng(png), picture: info.renderer.picture };
      }
      const [withLine, without] = [pictures[true].image, pictures[false].image];
      assert.deepEqual([withLine.width, withLine.height], [without.width, without.height]);
      // The subtitle's band: the display's lines 160 to 205 of 224, as the canvas shows them.
      const { y, height } = pictures[true].picture;
      const ratio = withLine.height / (await page.evaluate(() => document.querySelector('canvas').height));
      const band = [Math.floor((y + height * 160 / 224) * ratio), Math.ceil((y + height * 205 / 224) * ratio)];
      let inBand = 0, outside = 0, white = 0, edge = 0;
      for (let row = 0; row < withLine.height; row += 1) {
        for (let column = 0; column < withLine.width; column += 1) {
          const at = (row * withLine.width + column) * 4;
          const differs = [0, 1, 2].some((c) => Math.abs(withLine.rgba[at + c] - without.rgba[at + c]) > 24);
          if (!differs) continue;
          if (row >= band[0] && row < band[1]) {
            inBand += 1;
            const [r, g, b] = [withLine.rgba[at], withLine.rgba[at + 1], withLine.rgba[at + 2]];
            if (r > 200 && g > 200 && b > 200) white += 1;
            // text_palette's edge, 0xC086: (49, 33, 132).
            if (Math.abs(r - 49) + Math.abs(g - 33) + Math.abs(b - 132) < 40) edge += 1;
          } else {
            outside += 1;
          }
        }
      }
      console.log(`subtitle: ${inBand} pixels changed in its band (${white} white, ${edge} in the edge's blue), ${outside} elsewhere`);
      assert.ok(inBand > 2_000 && white > 500 && edge > 500, 'the line drawn in the game text\'s white and edge');
      assert.equal(outside, 0, 'nothing else changes');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('in a headset the line stands on the virtual screen below the views\' middle', { timeout: 900_000 }, async () => {
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST }] });
    try {
      const page = await browser.page('?xrScale=0.25');
      const frames = (n) => page.evaluate(async (n) => {
        const start = window.xem.status().page.xr.frames;
        while (window.xem.status().page.xr.frames < start + n) await new Promise((r) => setTimeout(r, 10));
      }, n);
      await toTheLine(page, async () => {
        await page.click('#vr');
        await page.waitForFunction(() => window.xem.status().page.xr.frames > 10, null, { timeout: 60_000 });
      });
      await frames(3);
      const layer = await page.evaluate(() => {
        const gl = document.querySelector('canvas').getContext('webgl2');
        return [gl.drawingBufferWidth, gl.drawingBufferHeight];
      });
      const points = [];
      for (let y = 0; y < layer[1]; y += 4) for (let x = 0; x < layer[0]; x += 4) points.push([x, y]);
      const read = {};
      for (const on of [true, false]) {
        assert.equal((await page.evaluate((v) => window.xem.settings.apply({ MovieSubtitles: v }), on)).error, undefined);
        await frames(3);
        read[on] = await page.evaluate(async (p) => Array.from(await window.xem.xr.readback(p)), points);
        assert.equal((await status(page)).renderer.subtitle, on);
      }
      // Readback rows count from the bottom: the lower half of each view is y < h / 2.
      let left = 0, right = 0, above = 0, white = 0;
      points.forEach(([x, y], i) => {
        const [a, b] = [read[true].slice(4 * i, 4 * i + 3), read[false].slice(4 * i, 4 * i + 3)];
        if (![0, 1, 2].some((c) => Math.abs(a[c] - b[c]) > 24)) return;
        if (y >= layer[1] / 2) above += 1;
        else if (x < layer[0] / 2) left += 1;
        else right += 1;
        if (a[0] > 200 && a[1] > 200 && a[2] > 200) white += 1;
      });
      console.log(`headset ${layer}: ${left} points changed in the left view, ${right} in the right, ${above} above the middle, ${white} white`);
      assert.ok(left > 50 && right > 50 && white > 5, 'the line in both views');
      assert.equal(above, 0, 'nothing above the views\' middle changes');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
