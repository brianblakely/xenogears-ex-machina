// The user's own disc 1 (local only, skipped without it): identification in
// place, streamed sector reads within the cache budget, the same boot outcome
// as the native xem-headless, and the retained new-game route played through
// the automation API to the same state as xem-headless --control.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdtempSync, statSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { after, before, describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, ROOT, USER_DISC, WEB, WEBGPU, pixelStats, shot, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm'));

/** The same commands through xem-headless --control. The browser always has
 * two memory cards (fresh formatted ones in a new profile): so does this run. */
function native(commands) {
  const cards = mkdtempSync(join(tmpdir(), 'xem-cards-'));
  const input = commands.map((c) => JSON.stringify(c)).join('\n') + '\n';
  return execFileSync(HEADLESS, ['--disc', USER_DISC, '--control', '--cards', cards],
    { cwd: ROOT, input, encoding: 'utf8', maxBuffer: 1 << 26 })
    .trim().split('\n').map((line) => JSON.parse(line));
}

/** Run commands in the page in order; frames and run_until through xem.run,
 * which fetches the disc sectors frames wait for and continues. */
function inPage(page, commands) {
  return page.evaluate(async (commands) => {
    const replies = [];
    for (const command of commands) replies.push(await window.xem.run(command));
    return replies;
  }, commands);
}

/** Two animation frames, so the canvas shows the display the last commands left. */
const rendered = (page) => page.evaluate(() => new Promise((resolve) =>
  requestAnimationFrame(() => requestAnimationFrame(resolve))));

describe('the user\'s disc 1', { skip: !available && `needs ${USER_DISC} and the game module` }, () => {
  let browser;
  let page;
  const requests = [];
  before(async () => {
    browser = await start({ args: NO_WEBGPU });
    page = await browser.page();
    page.on('request', (request) => requests.push(`${request.method()} ${new URL(request.url()).pathname}`));
  });
  after(() => browser.close());

  test('is identified in place', async () => {
    await page.setInputFiles('#file', USER_DISC);
    await page.waitForFunction(() => ['identified', 'failed'].includes(window.xem.status().disc?.state), null,
      { timeout: 60_000 });
    const { disc } = await status(page);
    assert.equal(disc.state, 'identified');
    assert.equal(disc.kind, 'chd');
    assert.equal(disc.bytes, statSync(USER_DISC).size);
    assert.deepEqual([disc.identity.disc, disc.identity.serial, disc.identity.boot_path], [1, 'SLUS-00664', 'SLUS_006.64;1']);
    assert.equal(disc.identity.sha256, 'dc0b2dd786203d4cce5927c5a3fc85a18f39a3f7406078860076ebb0bbae7119');
    assert.ok(disc.peak_resident_bytes <= disc.chunk_budget);
    console.log(`identified with ${disc.fetched_bytes} of ${disc.bytes} bytes read, peak ${disc.peak_resident_bytes}`);
  });

  test('streams sectors across the whole disc within the budget', async () => {
    const reads = await page.evaluate(async () => {
      const out = [];
      for (const lba of [16, 100_000, 200_000, 16]) {
        const data = await window.xem.readSectors(lba, 16);
        out.push({ lba, bytes: data.length, sync: data[0] === 0 && data[1] === 0xff && data[11] === 0 });
      }
      return { out, disc: window.xem.status().disc };
    });
    assert.ok(reads.out.every((read) => read.bytes === 16 * 2352 && read.sync));
    assert.ok(reads.disc.peak_resident_bytes <= reads.disc.chunk_budget);
    assert.ok(reads.disc.fetched_bytes < 32 * 1024 * 1024);
  });

  test('answers the same control commands as xem-headless', async () => {
    const commands = [
      { cmd: 'status' },
      { cmd: 'snapshot', name: 'boot' },
      { cmd: 'frames', count: 600 },
      { cmd: 'status' },
      { cmd: 'memory_hash' },
      { cmd: 'restore', name: 'boot' },
      { cmd: 'run_until', max_frames: 600, until: { vblanks: 600 } },
      { cmd: 'status' },
      { cmd: 'memory_hash' },
    ];
    await page.evaluate(() => window.xem.boot());
    const browser = await inPage(page, commands);
    await rendered(page);
    await page.screenshot({ path: shot('disc1-boot.png') });
    for (const [command, reply] of commands.map((c, i) => [c, browser[i]])) {
      console.log(`${JSON.stringify(command)} -> ${JSON.stringify(reply)}`);
    }
    assert.equal(browser[2].error ?? null, browser[3].stopped, 'a stop halts the frames and is reported');
    assert.deepEqual(browser[6], browser[2].error ? { error: browser[2].error } : browser[6]);
    assert.equal(browser[8], browser[4], 'the same state after restore and the same frames');

    if (existsSync(HEADLESS)) {
      const replies = native(commands);
      assert.deepEqual(browser, replies);
      console.log(`native xem-headless --control gives the same ${replies.length} replies`);
    } else {
      console.log(`no ${HEADLESS}: native comparison skipped`);
    }
    assert.ok(requests.every((request) => request.startsWith('GET ')), requests.join('\n'));
    assert.ok(!requests.some((request) => request.includes('.chd')), 'the disc never left the machine');
  });
});

describe('the new-game route on disc 1', { skip: !available && `needs ${USER_DISC} and the game module` }, () => {
  // tests/reference-inputs/new-game-movie-skip.json as pad changes per
  // vertical blank (the shared replay command): skip the opening movie, start
  // a new game, skip the next movie into the story text. xem-headless --inputs
  // holds the same buttons per frame instead; both hosts here schedule them
  // with replay, so like is compared with like.
  const replay = {
    cmd: 'replay',
    changes: [[901, 0, 8], [910, 0, 0], [1300, 0, 8], [1310, 0, 0], [1500, 0, 16384], [1510, 0, 0],
      [1750, 0, 16], [1760, 0, 0], [1810, 0, 16384], [1820, 0, 0], [4000, 0, 16384], [4010, 0, 0]],
  };
  const TITLE = 1290;
  const STORY = 2800;
  const END = 4500;
  const hash = { cmd: 'memory_hash' };

  test('reaches the same state as xem-headless --control', { timeout: 900_000 }, async () => {
    const browser = await start({ args: NO_WEBGPU });
    try {
      const page = await browser.page();
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      const started = Date.now();
      const replies = [];
      const shots = {};
      await page.evaluate(() => window.xem.boot());
      replies.push(...await inPage(page, [replay]));
      let at = 0;
      for (const [name, frame] of [['title', TITLE], ['story', STORY], ['end', END]]) {
        replies.push(await page.evaluate((count) => window.xem.runFrames(count), frame - at));
        replies.push(await page.evaluate(() => window.xem.memoryHash()));
        at = frame;
        await rendered(page);
        shots[name] = pixelStats(await page.locator('canvas').screenshot({ path: shot(`route-${name}.png`) }));
      }
      const seconds = (Date.now() - started) / 1000;
      const s = await status(page);
      console.log(`browser: ${END} frames in ${seconds.toFixed(1)} s; hashes ${replies.filter((r) => typeof r === 'string').join(' ')}`);
      console.log(`disc: ${s.disc.fetches} reads, ${s.disc.fetched_bytes} bytes, peak ${s.disc.peak_resident_bytes}`);
      assert.equal(s.session.vblanks, END);
      assert.equal(s.session.stopped, null);
      assert.ok(s.disc.peak_resident_bytes <= s.disc.chunk_budget);
      // The title and the story text are on the canvas, not the test scene.
      assert.ok(shots.title.lit > 0.05 && shots.title.lit < 0.9, `title: ${JSON.stringify(shots.title)}`);
      assert.ok(shots.story.lit > 0.005 && shots.story.lit < 0.2, `story: ${JSON.stringify(shots.story)}`);

      if (existsSync(HEADLESS)) {
        const commands = [replay, { cmd: 'frames', count: TITLE }, hash, { cmd: 'frames', count: STORY - TITLE }, hash,
          { cmd: 'frames', count: END - STORY }, hash];
        const expected = native(commands);
        console.log(`native: hashes ${expected.filter((r) => typeof r === 'string').join(' ')}`);
        assert.deepEqual(replies, expected);
      } else {
        console.log(`no ${HEADLESS}: native comparison skipped`);
      }
    } finally {
      await browser.close();
    }
  });
});

describe('disc 1 on WebGPU', { skip: !available && `needs ${USER_DISC} and the game module` }, () => {
  test('shows the game\'s display', { timeout: 300_000 }, async () => {
    const browser = await start({ args: WEBGPU });
    try {
      const page = await browser.page();
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      const reply = await page.evaluate(async () => {
        await window.xem.boot();
        return window.xem.runFrames(600);
      });
      await rendered(page);
      const pixels = pixelStats(await page.locator('canvas').screenshot({ path: shot('disc1-webgpu.png') }));
      const s = await status(page);
      assert.equal(s.renderer.backend, 'webgpu');
      assert.equal(reply.vblanks, 600);
      // The publisher's logo on black.
      assert.ok(pixels.lit > 0.02 && pixels.lit < 0.5, JSON.stringify(pixels));
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
