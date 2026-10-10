// The user's own disc 1 (local only, skipped without it): identification in
// place, streamed sector reads within the cache budget, and the same boot
// outcome as the native xem-headless.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { after, before, describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, ROOT, USER_DISC, WEB, shot, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm'));

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
    const browser = await page.evaluate(async (commands) => {
      await window.xem.boot();
      return commands.map((command) => window.xem.command(command));
    }, commands);
    await page.screenshot({ path: shot('disc1-boot.png') });
    for (const [command, reply] of commands.map((c, i) => [c, browser[i]])) {
      console.log(`${JSON.stringify(command)} -> ${JSON.stringify(reply)}`);
    }
    assert.equal(browser[2].error, browser[3].stopped, 'a stop halts the frames and is reported');
    assert.deepEqual(browser[6], browser[2].error ? { error: browser[2].error } : browser[6]);
    assert.equal(browser[8], browser[4], 'the same state after restore and the same frames');

    if (existsSync(HEADLESS)) {
      const input = commands.map((c) => JSON.stringify(c)).join('\n') + '\n';
      const native = execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], { cwd: ROOT, input, encoding: 'utf8' })
        .trim().split('\n').map((line) => JSON.parse(line));
      assert.deepEqual(browser, native);
      console.log(`native xem-headless --control gives the same ${native.length} replies`);
    } else {
      console.log(`no ${HEADLESS}: native comparison skipped`);
    }
    assert.ok(requests.every((request) => request.startsWith('GET ')), requests.join('\n'));
    assert.ok(!requests.some((request) => request.includes('.chd')), 'the disc never left the machine');
  });
});
