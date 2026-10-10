// The browser host in headless Chromium: rendering on both backends, the Slint
// settings panel, settings and save persistence, audio, disc import and the
// automation API over a game module fixture. Needs no private inputs.

import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { NO_WEBGPU, WEBGPU, pixelStats, shot, start, status, syntheticDisc } from './support.mjs';

async function canvasShot(page, name) {
  const png = await page.locator('canvas').screenshot({ path: shot(name) });
  return pixelStats(png);
}

describe('rendering', () => {
  for (const [label, args, expected] of [['webgpu', WEBGPU, 'webgpu'], ['webgl2 (WebGPU disabled)', NO_WEBGPU, 'webgl2']]) {
    test(`renders the test scene on ${label}`, async () => {
      const browser = await start({ args });
      try {
        const page = await browser.page();
        await page.waitForFunction(() => window.xem.status().renderer?.frames > 30);
        const { renderer, page: host } = await status(page);
        assert.equal(renderer.backend, expected);
        assert.equal(renderer.lost, null);
        assert.deepEqual(host.deviceLosses, []);
        const pixels = await canvasShot(page, `scene-${expected}.png`);
        assert.ok(pixels.colours > 200, `only ${pixels.colours} colours`);
        assert.ok(pixels.lit > 0.4, `only ${pixels.lit} of the canvas is lit`);

        // The Slint panel: shown by F1 (as a player would) and by automation.
        assert.equal(renderer.panel, 'slint');
        await page.keyboard.press('F1');
        assert.equal(await page.evaluate(() => window.xem.panel.visible()), true);
        await page.waitForTimeout(300);
        const withPanel = await canvasShot(page, `panel-${expected}.png`);
        assert.ok(withPanel.colours > 200);
        await page.keyboard.press('Escape');
        assert.equal(await page.evaluate(() => window.xem.panel.visible()), false);
        assert.deepEqual(page.errors, []);
      } finally {
        await browser.close();
      }
    });
  }

  test('the WebGL2 path can be chosen explicitly', async () => {
    const browser = await start({ args: WEBGPU });
    try {
      const page = await browser.page('?backend=webgl2');
      assert.equal((await status(page)).renderer.backend, 'webgl2');
    } finally {
      await browser.close();
    }
  });
});

describe('page services', () => {
  let browser;
  before(async () => {
    browser = await start({ args: NO_WEBGPU });
  });
  after(() => browser.close());

  test('settings go through the settings service and persist across reload', async () => {
    const page = await browser.page();
    const applied = await page.evaluate(() => [
      window.xem.settings.apply({ MasterVolume: 37 }),
      window.xem.settings.apply({ Scale: 'Integer' }),
      window.xem.settings.apply({ MasterVolume: 101 }),
    ]);
    assert.equal(applied[0].ok, true);
    assert.equal(applied[1].settings.presentation.scale, 'Integer');
    assert.deepEqual([applied[2].ok, applied[2].rejected], [false, 'invalid']);
    await page.reload();
    await page.waitForFunction(() => document.body.dataset.ready === 'true');
    const { settings } = await page.evaluate(() => window.xem.settings.get());
    assert.equal(settings.master_volume, 37);
    assert.equal(settings.presentation.scale, 'Integer');
    await page.close();
  });

  test('settings can be changed through the Slint panel', async () => {
    const page = await browser.page();
    await page.evaluate(() => window.xem.panel.show(true));
    await page.waitForTimeout(300);
    // The panel is centred on the canvas; "Show FPS" sits at its left.
    const box = await page.locator('canvas').boundingBox();
    const before = (await page.evaluate(() => window.xem.settings.get())).settings.presentation.show_fps;
    await page.mouse.click(box.x + box.width / 2 - 195, box.y + box.height / 2 + 62);
    await page.waitForTimeout(200);
    await page.locator('canvas').screenshot({ path: shot('panel-click.png') });
    const after = (await page.evaluate(() => window.xem.settings.get())).settings.presentation.show_fps;
    assert.equal(after, !before);
    await page.close();
  });

  test('audio starts from a user gesture and plays runtime samples', async () => {
    const page = await browser.page();
    assert.equal((await status(page)).page.audio.state, 'not started');
    await page.click('#audio');
    await page.waitForFunction(() => {
      const s = window.xem.status();
      return s.page.audio.state === 'running' && s.page.audio.played > 4800 && s.audioFrames > 4800;
    });
    await page.close();
  });

  test('a save round-trips through IndexedDB across reload', async () => {
    const page = await browser.page();
    const saved = await page.evaluate(() => window.xem.save('slot-test'));
    assert.equal(saved.kind, 'memory-card');
    assert.equal(saved.bytes, 128 * 1024);
    await page.reload();
    await page.waitForFunction(() => document.body.dataset.ready === 'true');
    const loaded = await page.evaluate(async () => {
      const { data, ...rest } = await window.xem.load('slot-test');
      return { ...rest, magic: String.fromCharCode(data[0], data[1]) };
    });
    assert.equal(loaded.verified, true);
    assert.equal(loaded.sha256, saved.sha256);
    assert.equal(loaded.magic, 'MC');
    await page.close();
  });

  test('a synthetic disc image is identified in place with a bounded cache', async () => {
    const page = await browser.page();
    const requests = [];
    page.on('request', (request) => requests.push(request.method()));
    const { image, exeSha256 } = syntheticDisc();
    await page.setInputFiles('#file', { name: 'synthetic.bin', mimeType: 'application/octet-stream', buffer: image });
    await page.waitForFunction(() => ['identified', 'failed'].includes(window.xem.status().disc?.state));
    const { disc } = await status(page);
    assert.equal(disc.state, 'identified');
    assert.equal(disc.kind, 'bin');
    assert.equal(disc.identity.disc, null);
    assert.equal(disc.identity.boot_path, 'SLUS_999.99;1');
    assert.equal(disc.identity.sha256, exeSha256);
    assert.ok(disc.peak_resident_bytes <= disc.chunk_budget);
    const sector = await page.evaluate(async () => Array.from((await window.xem.readSectors(16, 1)).subarray(24, 30)));
    assert.deepEqual(sector, [1, ...Buffer.from('CD001')]);
    assert.match(await page.textContent('#message'), /not a known Xenogears disc/);
    // Booting needs a known disc.
    await assert.rejects(page.evaluate(() => window.xem.boot()), /not a known Xenogears disc/);
    assert.ok(requests.every((method) => method === 'GET'), 'the page only fetched its own files');
    await page.close();
  });

  test('the page keeps working without a game module', async () => {
    const page = await browser.page('?game=missing/game.wasm');
    const s = await status(page);
    assert.equal(s.page.game.available, false);
    assert.ok(s.renderer.frames >= 0);
    assert.equal(await page.isDisabled('#boot'), true);
    await assert.rejects(page.evaluate(() => window.xem.boot({ executable: false })), /no game module/);
    assert.equal(page.errors.length, 0, page.errors.join('\n'));
    await page.close();
  });
});

describe('automation over a game module fixture', () => {
  let browser;
  let page;
  before(async () => {
    browser = await start({ args: NO_WEBGPU, fixtures: true });
    page = await browser.page('?game=/fixtures/fixture.wasm');
  });
  after(() => browser.close());

  test('step, runUntil, snapshot and restore', async () => {
    const result = await page.evaluate(async () => {
      const xem = window.xem;
      await xem.boot({ executable: false });
      const first = xem.step(1);
      const untilWord = xem.runUntil({ maxFrames: 10, condition: { until: 'word', address: 0x80000000, value: 2 } });
      const snapshot = xem.snapshot();
      const atSnapshot = xem.status().session;
      const restart = xem.runUntil({ maxFrames: 10, condition: { until: 'restart' } });
      const bounded = xem.runUntil({ maxFrames: 5, condition: { until: 'halt' } });
      const halt = xem.runUntil({ maxFrames: 1000, condition: { until: 'halt' } });
      const end = { status: xem.status().session, digest: xem.digest() };
      const restored = xem.restore(snapshot).session;
      const again = xem.runUntil({ maxFrames: 1000, condition: { until: 'halt' } });
      const end2 = { status: xem.status().session, digest: xem.digest() };
      let tooFar = null;
      try {
        xem.runUntil({ maxFrames: 1e9, condition: { until: 'halt' } });
      } catch (error) {
        tooFar = error.message;
      }
      return { first, untilWord, atSnapshot, restart, bounded, halt, end, restored, again, end2, tooFar,
               lines: xem.status().bootLines, snapshotBytes: snapshot.length };
    });
    const frame = (steps, restarts = []) => ({ stop: 'frame', steps, interrupts: 1, restarts });
    assert.deepEqual(result.first, { frames: 1, met: true, last: frame(1), halted: false });
    assert.deepEqual([result.untilWord.frames, result.untilWord.met], [2, true]);
    assert.deepEqual([result.atSnapshot.frames, result.atSnapshot.vblanks, result.atSnapshot.suspended], [3, 3, true]);
    // The restart and the first poll share a frame.
    assert.deepEqual(result.restart.last, frame(2, [[1, 100]]));
    assert.deepEqual([result.bounded.frames, result.bounded.met, result.bounded.halted], [5, false, false]);
    assert.equal(result.halt.met, true);
    const stop = 'host stopped the game: called FixtureStub, which the port does not define';
    assert.deepEqual(result.halt.last, { stop: 'trap', reason: stop });
    assert.deepEqual([result.end.status.frames, result.end.status.vblanks], [3 + 100 + 1, 103]);
    assert.deepEqual(result.end.status.missing, ['FixtureStub']);
    assert.deepEqual([result.restored.frames, result.restored.halted, result.restored.missing], [3, null, []]);
    assert.deepEqual(result.again.last, result.halt.last);
    assert.equal(result.end2.digest, result.end.digest);
    assert.deepEqual(result.end2.status, result.end.status);
    assert.deepEqual(result.lines, ['frame 3: restart kind 1 arg 0x64', `frame 103: ${stop}`,
      'restored a snapshot at frame 3', 'frame 3: restart kind 1 arg 0x64', `frame 103: ${stop}`]);
    assert.match(result.tooFar, /maxFrames is at most/);
  });

  test('the animation loop runs due frames in bounded batches and drops missed time', async () => {
    const loop = await page.evaluate(async () => {
      const status = () => window.xem.status();
      const waitFor = (done) => new Promise((resolve) => {
        const check = () => (done() ? resolve() : setTimeout(check, 10));
        check();
      });
      await window.xem.boot({ executable: false, run: true });
      const before = status().loop;
      await waitFor(() => status().session.frames >= 10);
      // A one-second stall of the page (as a throttled or frozen tab sees).
      const stalled = status().session.frames;
      const until = performance.now() + 1000;
      while (performance.now() < until);
      await new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)));
      const afterStall = status().session.frames - stalled;
      await waitFor(() => status().session.halted);
      const s = status();
      return { ...s.loop, before, afterStall, running: s.running, session: s.session };
    });
    assert.ok(loop.maxBatch <= loop.maxFramesPerAnimation, `batch of ${loop.maxBatch}`);
    assert.ok(loop.afterStall <= 2 * loop.maxFramesPerAnimation, `${loop.afterStall} frames after the stall`);
    assert.ok(loop.dropped - loop.before.dropped >= 50, `${loop.dropped - loop.before.dropped} dropped`);
    assert.equal(loop.gameFrames - loop.before.gameFrames, 104);
    assert.equal(loop.running, false);
    assert.equal(loop.session.frames, 104);
  });

  test('a snapshot save survives a reload byte for byte', async () => {
    const saved = await page.evaluate(async () => {
      await window.xem.boot({ executable: false });
      window.xem.step(2);
      return window.xem.save('fixture');
    });
    assert.equal(saved.kind, 'snapshot');
    await page.reload();
    await page.waitForFunction(() => document.body.dataset.ready === 'true');
    const loaded = await page.evaluate(async () => {
      const record = await window.xem.load('fixture');
      await window.xem.boot({ executable: false });
      const session = window.xem.restore(record.data).session;
      window.xem.step(1);
      return { verified: record.verified, sha256: record.sha256, session, word: window.xem.status().session };
    });
    assert.equal(loaded.verified, true);
    assert.equal(loaded.sha256, saved.sha256);
    assert.equal(loaded.session.frames, 2);
    assert.equal(loaded.word.frames, 3);
    assert.equal(loaded.word.vblanks, 3);
  });
});
