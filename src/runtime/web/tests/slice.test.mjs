// The Phase 3 slice in browsers (local only: it needs the user's disc 1 and
// the game module): the session logs tests/reference-inputs/slice-*.json,
// recorded natively (tools/native_scenario.py and tools/field_agent.py --log),
// replayed through window.xem on a desktop browser, a phone (Playwright's
// Pixel 7: its viewport, user agent and touch) and a headset browser (Quest
// Browser's user agent with IWER's Meta Quest 3 as navigator.xr). Each ends
// at the RAM/scratchpad hash its native recording ended at. The save writes
// card 2 to IndexedDB through a page reload taken mid-route (the session is
// saved to IndexedDB, the page reloads, the disc is imported again and the
// session restored), and the next page loads that card from the title.

import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { devices } from 'playwright-core';
import { NO_WEBGPU, ROOT, USER_DISC, WEB, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm'));
const log = (name) => JSON.parse(readFileSync(join(ROOT, 'tests', 'reference-inputs', `slice-${name}.json`), 'utf8'));

const QUEST_BROWSER = 'Mozilla/5.0 (X11; Linux x86_64; Quest 3) AppleWebKit/537.36 (KHTML, like Gecko) '
  + 'OculusBrowser/37.0.0.0 SamsungBrowser/4.0 Chrome/132.0.0.0 VR Safari/537.36';
const IWER = readFileSync(join(ROOT, 'src', 'runtime', 'web', 'node_modules', 'iwer', 'build', 'iwer.min.js'), 'utf8');
// Chromium exposes a native navigator.xr (without a runtime): IWER replaces it.
const QUEST = `${IWER}\n;new IWER.XRDevice(IWER.metaQuest3, { stereoEnabled: true }).installRuntime({ forceInstall: true });`;

/** A page with the disc imported and a session booted. */
async function booted(browser) {
  const page = await browser.page();
  await page.setInputFiles('#file', USER_DISC);
  await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
  await page.evaluate(() => window.xem.boot());
  return page;
}

/** Schedule `commands` and run until frame `to` (from boot). */
async function play(page, commands, to) {
  const scheduled = await page.evaluate((commands) => window.xem.command({ cmd: 'schedule', commands }), commands);
  assert.equal(scheduled.error, undefined, JSON.stringify(scheduled));
  const at = (await status(page)).session.frames;
  const reply = await page.evaluate((count) => window.xem.runFrames(count), to - at);
  assert.equal(reply.error, undefined, JSON.stringify(reply));
  assert.equal((await status(page)).session.frames, to);
  return page.evaluate(() => window.xem.memoryHash());
}

async function forest(options) {
  const forest = log('forest');
  const browser = await start({ args: NO_WEBGPU, ...options });
  try {
    const page = await booted(browser);
    const started = Date.now();
    assert.equal(await play(page, forest.commands, forest.frames), forest.memory_hash);
    console.log(`${forest.frames} frames in ${((Date.now() - started) / 1000).toFixed(0)} s`);
    // The battle's reward is in the inventory.
    const item = await page.evaluate(() => window.xem.command({ cmd: 'inspect', path: 'game_data.itemIds[0]', depth: 0 }));
    assert.equal(item, 0x39);
    assert.deepEqual(page.errors, []);
    return page;
  } finally {
    await browser.close();
  }
}

// Each test plays in its own browser, two at a time: the longest (the save
// and its reload) beside the three forest routes in turn.
describe('the Phase 3 slice in browsers', { concurrency: 2, skip: !available && `needs ${USER_DISC} and the game module` }, () => {
  test('the save survives a reload mid-route and the next page loads it', { timeout: 3_600_000 }, async () => {
    const save = log('save');
    const load = log('load-card2');
    const MID = 6000;
    const browser = await start({ args: NO_WEBGPU });
    try {
      // Card 2 starts unformatted, as on the original route.
      let page = await browser.page();
      await page.evaluate(() => window.xem.cards.store(1, new Uint8Array(0x20000)));
      await page.close();

      // Halfway, the session goes to IndexedDB and the page reloads.
      page = await booted(browser);
      await play(page, save.commands, MID);
      await page.evaluate(() => window.xem.save('mid-route'));
      await page.reload();
      await page.waitForFunction(() => document.body.dataset.ready === 'true');
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      const restored = await page.evaluate(async () => {
        const record = await window.xem.load('mid-route');
        return record.verified && window.xem.importSnapshot(record.data);
      });
      assert.ok(restored !== false, 'the saved session restores');
      assert.equal((await status(page)).session.frames, MID);
      const rest = save.commands.filter(([at]) => at >= MID);
      assert.equal(await play(page, rest, save.frames), save.memory_hash);
      await page.evaluate(() => window.xem.cards.flush());
      const card = new Uint8Array(Object.values(await page.evaluate(async () => Array.from(await window.xem.cards.stored(1)))));
      assert.equal(String.fromCharCode(card[0], card[1]), 'MC', 'card 2 formatted');
      assert.equal(card[128], 0x51, 'and holding the save');
      await page.close();

      // A new page loads card 2 from the title screen.
      page = await booted(browser);
      assert.equal(await play(page, load.commands, load.frames), load.memory_hash);
      const map = await page.evaluate(() => window.xem.command({ cmd: 'inspect', path: 'mode_field_map_id', depth: 0 }));
      assert.equal(map, 22);
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('a desktop browser plays the forest route to the native end', { timeout: 1_800_000 }, async () => {
    await forest({});
  });

  test('a phone browser plays it to the same end', { timeout: 1_800_000 }, async () => {
    await forest({ context: { ...devices['Pixel 7'] } });
  });

  test('a headset browser plays it to the same end', { timeout: 1_800_000 }, async () => {
    const browser = await start({ args: NO_WEBGPU, context: { userAgent: QUEST_BROWSER }, initScripts: [{ content: QUEST }] });
    try {
      const page = await booted(browser);
      assert.ok(await page.evaluate(() => navigator.xr.isSessionSupported('immersive-vr')), 'an emulated headset');
      const forest = log('forest');
      assert.equal(await play(page, forest.commands, forest.frames), forest.memory_hash);
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
