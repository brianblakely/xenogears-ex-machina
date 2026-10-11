// The game renderer in browsers (local only: it needs the user's disc 1 and
// the game module): field 12 (an interior with a dialogue window and a
// portrait) under the Modern, HD-2D and PS1 profiles and wider framing, on
// WebGPU and on WebGL2. Each profile draws a picture of its own; none of them
// changes the game: the same frames from the same snapshot end at the same
// RAM hash whatever is selected and however often the page renders.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, SHOTS, USER_DISC, WEB, WEBGPU, decodePng, pixelStats, shot, start, status } from './support.mjs';

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
    for (const [name, settings] of [
      ['modern', graphics({})],
      ['hd2d', graphics({ profile: 'Hd2d' })],
      ['ps1', graphics({ profile: 'Ps1' })],
      ['wide', graphics({ widescreen: true })],
    ]) {
      shots[name] = await picture(page, `render-${backend}-${name}.png`, settings);
      assert.ok(shots[name].stats.lit > 0.2, `${name} shows the room (${shots[name].stats.lit})`);
      assert.ok(shots[name].stats.colours > 500, `${name} has detail (${shots[name].stats.colours} colours)`);
    }
    // Each look is its own picture.
    for (const [a, b] of [['modern', 'hd2d'], ['modern', 'ps1'], ['modern', 'wide']]) {
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
    assert.equal(hd2d, modern, 'HD-2D, wider framing and interpolation leave the game unchanged');
    assert.equal(ps1, modern, 'the PS1 profile leaves the game unchanged');
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
    for (const name of ['modern', 'hd2d', 'ps1', 'wide']) {
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
});
