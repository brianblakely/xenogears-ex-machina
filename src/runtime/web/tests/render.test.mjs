// The game renderer in browsers (local only: it needs the user's disc 1 and
// the game module): field 12 (an interior with a dialogue window and a
// portrait) under the Modern, HD-2D and PS1 profiles, wider framing and the
// field camera pulled back, on WebGPU and on WebGL2. Each profile draws a picture of its own; none of them
// changes the game: the same frames from the same snapshot end at the same
// RAM hash whatever is selected and however often the page renders. The
// page's state exported after it drew under HD-2D with wider framing and the
// camera pulled back (the draw trace and its reveals on) loads in
// xem-headless to the page's RAM hash, and 240 frames there end where the
// page's 240 frames end. The
// title movie's frame 860, shown twice from one snapshot: under the
// Enhanced movies setting rebuilt from what the MDEC decoded
// (status().renderer.movie, no wider framing's sides), under Original the
// device's own picture, the two different but within the MDEC's error of
// each other over the display's pixels, the game's RAM the same; the
// backends' rebuilt frames agree.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, SHOTS, USER_DISC, WEB, WEBGPU, decodePng, native, pixelStats, shot, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
mkdirSync(SHOTS, { recursive: true });

const graphics = (overrides) => ({
  profile: 'Modern', internal_scale: 2, widescreen: false, interpolation: false, ...overrides,
});

/** A page with the disc imported and field 12's state, made natively. */
async function field(browser) {
  const state = join(SHOTS, 'render-field12.state');
  execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
    input: [
      { cmd: 'enter_field', map: 12 },
      { cmd: 'frames', count: 400 },
      { cmd: 'state_save', path: state },
    ].map((c) => JSON.stringify(c)).join('\n') + '\n',
  });
  const page = await browser.page();
  await page.setInputFiles('#file', USER_DISC);
  await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
  await page.evaluate(() => window.xem.boot());
  await page.evaluate((base64) => {
    const bytes = Uint8Array.from(atob(base64), (c) => c.charCodeAt(0));
    window.xem.importSnapshot(bytes);
  }, readFileSync(state).toString('base64'));
  return page;
}

/** Select `settings`, let the page render a few frames, screenshot the canvas. */
async function picture(page, name, settings) {
  const current = await page.evaluate(() => window.xem.settings.get());
  const applied = await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }),
    { ...current.graphics, ...settings });
  assert.equal(applied.error, undefined, JSON.stringify(applied));
  // New game frames reach the renderer as the page runs them.
  await page.evaluate(() => window.xem.runFrames(6));
  await page.evaluate(() => new Promise((done) => requestAnimationFrame(() => requestAnimationFrame(done))));
  await page.evaluate(() => window.xem.runFrames(2));
  await page.evaluate(() => new Promise((done) => requestAnimationFrame(() => requestAnimationFrame(done))));
  const png = await page.locator('canvas').screenshot({ path: shot(name) });
  return { png, stats: pixelStats(png) };
}

async function profiles(args, backend) {
  const browser = await start({ args });
  try {
    const page = await field(browser);
    assert.equal((await status(page)).renderer.backend, backend);
    const shots = {};
    // Pulled back three times, the small room fills about a tenth of the frame.
    for (const [name, settings, lit] of [
      ['modern', graphics({}), 0.2],
      ['hd2d', graphics({ profile: 'Hd2d' }), 0.2],
      ['ps1', graphics({ profile: 'Ps1' }), 0.2],
      ['wide', graphics({ widescreen: true }), 0.2],
      ['zoom', graphics({ widescreen: true, zoom_out: 300 }), 0.05],
    ]) {
      shots[name] = await picture(page, `render-${backend}-${name}.png`, settings);
      assert.ok(shots[name].stats.lit > lit, `${name} shows the room (${shots[name].stats.lit})`);
      assert.ok(shots[name].stats.colours > 500, `${name} has detail (${shots[name].stats.colours} colours)`);
    }
    // Each look is its own picture.
    for (const [a, b] of [['modern', 'hd2d'], ['modern', 'ps1'], ['modern', 'wide'], ['wide', 'zoom']]) {
      assert.notDeepEqual(shots[a].png, shots[b].png, `${a} and ${b} differ`);
    }
    assert.deepEqual(page.errors, []);

    // The same 240 frames under two profiles end at the same RAM.
    const snapshot = await page.evaluate(() => window.xem.command({ cmd: 'snapshot' }));
    assert.equal(snapshot.error, undefined);
    const run = async (settings) => {
      await page.evaluate((s) => window.xem.command({ cmd: 'restore', ...s }), snapshot);
      await picture(page, `render-${backend}-state.png`, settings);
      await page.evaluate(() => window.xem.runFrames(240));
      return page.evaluate(() => window.xem.memoryHash());
    };
    const modern = await run(graphics({}));
    const hd2d = await run(graphics({ profile: 'Hd2d', widescreen: true, interpolation: true }));
    const ps1 = await run(graphics({ profile: 'Ps1' }));
    const zoomed = await run(graphics({ widescreen: true, zoom_out: 400 }));
    assert.equal(hd2d, modern, 'HD-2D, wider framing and interpolation leave the game unchanged');
    assert.equal(ps1, modern, 'the PS1 profile leaves the game unchanged');
    assert.equal(zoomed, modern, 'pulling the camera back (the port records the culled scenery) leaves the game unchanged');

    // The page's state after drawing under HD-2D, wider and pulled back,
    // exported: natively it is the page's RAM, and goes on as the page's.
    await page.evaluate((s) => window.xem.command({ cmd: 'restore', ...s }), snapshot);
    await picture(page, `render-${backend}-export.png`, graphics({ profile: 'Hd2d', widescreen: true, zoom_out: 200 }));
    const exported = join(SHOTS, `render-${backend}-export.xemstate`);
    writeFileSync(exported, Buffer.from(await page.evaluate(() => Array.from(window.xem.exportSnapshot()))));
    const at = await page.evaluate(() => window.xem.memoryHash());
    await page.evaluate(() => window.xem.runFrames(240));
    const onward = await page.evaluate(() => window.xem.memoryHash());
    assert.equal(native([{ cmd: 'state_load', path: exported }, { cmd: 'memory_hash' }]), at, 'the export loads natively to the page\'s RAM');
    assert.equal(native([{ cmd: 'state_load', path: exported }, { cmd: 'frames', count: 240 }, { cmd: 'memory_hash' }]), onward,
      'and goes on natively as in the page');
    console.log(`${backend}: the page's state after drawing loads natively to ${at} and goes on to ${onward} in both`);
  } finally {
    await browser.close();
  }
}

/** Two animation frames: the page presents what the game drew. */
const painted = (page) => page.evaluate(() => new Promise((done) => requestAnimationFrame(() => requestAnimationFrame(done))));

/** The game's picture in a canvas screenshot (`picture`, as status() gives
 * it: placed at a whole multiple by the Integer scale), averaged over each
 * display pixel's square. */
function displayPixels(png, picture) {
  const image = decodePng(png);
  const [width, height] = picture.display;
  const factor = picture.width / width;
  assert.ok(Number.isInteger(factor) && picture.height === height * factor, `placed at a whole multiple: ${JSON.stringify(picture)}`);
  const rgb = new Float32Array(width * height * 3);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      for (let k = 0; k < 3; k++) {
        let sum = 0;
        for (let dy = 0; dy < factor; dy++) {
          for (let dx = 0; dx < factor; dx++) sum += image.rgba[((picture.y + y * factor + dy) * image.width + picture.x + x * factor + dx) * 4 + k];
        }
        rgb[(y * width + x) * 3 + k] = sum / (factor * factor);
      }
    }
  }
  return { width, height, rgb };
}

const meanError = (a, b) => a.rgb.reduce((sum, value, i) => sum + Math.abs(value - b.rgb[i]), 0) / a.rgb.length;

/** Each backend's rebuilt movie frame (`movie` below), for comparing them. */
const rebuilt = {};

/** The title movie from a boot, its frame 860 shown twice from a snapshot
 * taken 12 frames before: rebuilt from what the MDEC decoded under the
 * Enhanced movies setting, the device's own picture under Original. */
async function movie(args, backend) {
  const browser = await start({ args });
  try {
    const page = await browser.page();
    await page.setInputFiles('#file', USER_DISC);
    await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
    assert.equal((await page.evaluate(() => window.xem.settings.apply({ Scale: 'Integer' }))).error, undefined);
    await page.evaluate(() => window.xem.boot());
    assert.equal((await status(page)).renderer.backend, backend, `the ${backend} renderer`);
    // The page's first presentation of the session starts the draw trace:
    // the movie frames decoded after it can be rebuilt.
    await page.evaluate(() => window.xem.runFrames(840));
    await painted(page);
    await page.evaluate(() => window.xem.runFrames(8));
    await painted(page);
    assert.equal((await page.evaluate(() => window.xem.snapshot('movie'))).error, undefined);
    const shown = {};
    for (const movies of ['Enhanced', 'Original']) {
      await page.evaluate(() => window.xem.restore('movie'));
      const current = (await page.evaluate(() => window.xem.settings.get())).settings.graphics;
      // Wider framing on: a rebuilt movie frame is the display alone.
      const graphics = { ...current, widescreen: true, filtering: { ...current.filtering, movies } };
      const applied = await page.evaluate((g) => window.xem.settings.apply({ Graphics: g }), graphics);
      assert.equal(applied.error, undefined, JSON.stringify(applied));
      await page.evaluate(() => window.xem.runFrames(12));
      await painted(page);
      const info = await status(page);
      const png = await page.locator('canvas').screenshot({ path: shot(`render-${backend}-movie-${movies.toLowerCase()}.png`) });
      const hash = await page.evaluate(() => window.xem.memoryHash());
      shown[movies] = { png, movie: info.renderer.movie, picture: info.renderer.picture, hash };
    }
    assert.equal(shown.Enhanced.movie, true, 'Enhanced shows the movie frame rebuilt');
    assert.equal(shown.Original.movie, false, 'Original shows the device\'s own picture');
    assert.equal(shown.Enhanced.picture.side, 0, 'a rebuilt movie frame has no wider framing\'s sides');
    assert.deepEqual(shown.Enhanced.picture.display, shown.Original.picture.display, 'the same display');
    assert.notDeepEqual(shown.Enhanced.png, shown.Original.png, 'the two pictures differ');
    assert.deepEqual(shown.Enhanced.hash, shown.Original.hash, 'neither setting changes the game');
    const [enhanced, original] = [shown.Enhanced, shown.Original].map((s) => displayPixels(s.png, s.picture));
    const error = meanError(enhanced, original);
    console.log(`${backend} (${(await status(page)).renderer.adapter}): the rebuilt frame ${error.toFixed(2)} levels from the device's on average, over its pixels`);
    assert.ok(error <= 6, `the rebuilt frame is the frame the device shows (${error})`);
    rebuilt[backend] = enhanced;
    assert.deepEqual(page.errors, []);
  } finally {
    await browser.close();
  }
}

describe('the game renderer in browsers', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  test('WebGPU draws every profile without changing the game', { timeout: 600_000 }, async () => {
    await profiles(WEBGPU, 'webgpu');
  });

  test('WebGL2 draws every profile without changing the game', { timeout: 600_000 }, async () => {
    await profiles(NO_WEBGPU, 'webgl2');
  });

  test('WebGPU and WebGL2 draw the same pictures', () => {
    // The same state and frames on both backends (the tests above).
    for (const name of ['modern', 'hd2d', 'ps1', 'wide', 'zoom']) {
      const a = decodePng(readFileSync(shot(`render-webgpu-${name}.png`)));
      const b = decodePng(readFileSync(shot(`render-webgl2-${name}.png`)));
      assert.equal(a.width, b.width);
      assert.equal(a.height, b.height);
      let close = 0;
      let total = 0;
      for (let i = 0; i < a.width * a.height; i++) {
        const d = Math.max(...[0, 1, 2].map((k) => Math.abs(a.rgba[i * 4 + k] - b.rgba[i * 4 + k])));
        if (d <= 8) close++;
        total += d;
      }
      const share = close / (a.width * a.height);
      console.log(`${name}: ${(share * 100).toFixed(2)}% of pixels within 8 levels, mean difference ${(total / (a.width * a.height)).toFixed(2)}`);
      assert.ok(share > 0.97, `${name}: the backends agree (${share})`);
    }
  });

  test('WebGPU rebuilds movie frames as Enhanced asks and shows them as they are under Original', { timeout: 600_000 }, async () => {
    await movie(WEBGPU, 'webgpu');
  });

  test('WebGL2 rebuilds movie frames as Enhanced asks and shows them as they are under Original', { timeout: 600_000 }, async () => {
    await movie(NO_WEBGPU, 'webgl2');
  });

  test('WebGPU and WebGL2 rebuild the same movie frame', () => {
    // Over the display's pixels, each enlarging as its adapter's kind
    // chooses (headless Chromium's adapters are SwiftShader's: software,
    // so bilinear on both).
    const [a, b] = [rebuilt.webgpu, rebuilt.webgl2];
    assert.ok(a && b, 'both backends rebuilt the frame (the tests above)');
    let close = 0;
    for (let i = 0; i < a.width * a.height; i++) {
      if (Math.max(...[0, 1, 2].map((k) => Math.abs(a.rgb[i * 3 + k] - b.rgb[i * 3 + k]))) <= 8) close++;
    }
    const share = close / (a.width * a.height);
    console.log(`movie: ${(share * 100).toFixed(2)}% of display pixels within 8 levels, mean difference ${meanError(a, b).toFixed(2)}`);
    assert.ok(share > 0.97, `the backends agree (${share})`);
  });
});
