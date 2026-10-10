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

  test('boots to the same stop as xem-headless, with snapshot and restore', async () => {
    const result = await page.evaluate(async () => {
      const xem = window.xem;
      await xem.boot();
      const start = xem.snapshot();
      const report = xem.runUntil({ maxSteps: 1000, condition: { until: 'halt' } });
      const first = { lines: xem.status().bootLines, digest: xem.digest(), session: xem.status().session };
      xem.restore(start);
      const again = xem.runUntil({ maxSteps: 1000, condition: { until: 'halt' } });
      return { report, first, again, digest: xem.digest(), memory: xem.status().gameMemoryBytes };
    });
    await page.screenshot({ path: shot('disc1-boot.png') });
    console.log(result.first.lines.join('\n'));
    console.log(`ram digest ${result.first.digest}`);
    assert.equal(result.report.halted, true);
    assert.deepEqual(result.again.last, result.report.last);
    assert.equal(result.digest, result.first.digest);

    if (existsSync(HEADLESS)) {
      const native = execFileSync(HEADLESS, ['--disc', USER_DISC], { cwd: ROOT, encoding: 'utf8' }).split('\n');
      const steps = native.filter((line) => line.startsWith('step '));
      const digest = native.find((line) => line.startsWith('ram digest ')).slice('ram digest '.length);
      assert.deepEqual(result.first.lines, steps);
      assert.equal(result.first.digest, digest);
      console.log(`native xem-headless agrees: ${steps.length} step lines, ram digest ${digest}`);
    } else {
      console.log(`no ${HEADLESS}: native comparison skipped`);
    }
    assert.ok(requests.every((request) => request.startsWith('GET ')), requests.join('\n'));
    assert.ok(!requests.some((request) => request.includes('.chd')), 'the disc never left the machine');
  });
});
