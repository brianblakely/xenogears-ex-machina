// The Binaural sound mode in browsers (local only: it needs the user's disc
// 1, the game module and a native xem-headless): one game state, restored
// natively and in the browser, presents the same binaural mix from the
// shared renderer compiled for each, within a declared tolerance; and the
// presentation's cost per second of sound in the browser, on this
// processor and on one throttled four times (a phone's budget).

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, USER_DISC, WEB, shot, start } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
/** The declared tolerance between hosts: the filters are made with each
 * host's own sin, cos and atan2, which may differ in their last bit. */
const TOLERANCE = 2;
/** A field with three positional emitters and its music. */
const FIELD = 15;
const FRAMES = 240;
const SECONDS = FRAMES / 60;

const samples = (reply) => {
  const hex = reply.samples;
  const out = new Int16Array(hex.length / 4);
  for (let i = 0; i < out.length; i++) {
    const lo = parseInt(hex.slice(i * 4, i * 4 + 2), 16), hi = parseInt(hex.slice(i * 4 + 2, i * 4 + 4), 16);
    out[i] = (hi << 8 | lo) << 16 >> 16;
  }
  return out;
};

/** The field's state, made natively, and the native capture of `FRAMES`
 * frames from it with the presentation on. */
function native(state) {
  const commands = [
    { cmd: 'sound_mode', mode: 1 },
    { cmd: 'binaural', enabled: true },
    { cmd: 'enter_field', map: FIELD },
    { cmd: 'frames', count: 600 },
    { cmd: 'state_save', path: state },
    // From the state, as the browser starts.
    { cmd: 'state_load', path: state },
    { cmd: 'audio_capture', action: 'start' },
    { cmd: 'frames', count: FRAMES },
    { cmd: 'audio_capture', action: 'stop', samples: true },
  ];
  const output = execFileSync(HEADLESS, ['--disc', USER_DISC, '--control', '--wav', shot('binaural-unused.wav')], {
    input: commands.map((c) => JSON.stringify(c)).join('\n') + '\n',
    maxBuffer: 64 << 20,
  });
  const replies = output.toString().trim().split('\n').map((line) => JSON.parse(line));
  for (const reply of replies) assert.equal(reply.error, undefined, JSON.stringify(reply).slice(0, 400));
  return replies.at(-1);
}

/** A page with the disc imported, the presentation set and the state restored. */
async function restored(browser, state, binaural) {
  const page = await browser.page();
  await page.setInputFiles('#file', USER_DISC);
  await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
  await page.evaluate(() => window.xem.boot());
  for (const command of [{ cmd: 'sound_mode', mode: 1 }, { cmd: 'binaural', enabled: binaural }]) {
    const reply = await page.evaluate((c) => window.xem.command(c), command);
    assert.equal(reply.error, undefined, JSON.stringify(reply));
  }
  await page.evaluate((base64) => {
    window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
  }, readFileSync(state).toString('base64'));
  return page;
}

/** Runs `FRAMES` frames capturing their sound: the reply and the wall time. */
async function capture(page) {
  return page.evaluate(async (frames) => {
    window.xem.command({ cmd: 'audio_capture', action: 'start' });
    const started = performance.now();
    const ran = await window.xem.runFrames(frames);
    const ms = performance.now() - started;
    return { ran, ms, reply: window.xem.command({ cmd: 'audio_capture', action: 'stop', samples: true }) };
  }, FRAMES);
}

describe('the Binaural mode in browsers', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  test('native and browser present the same binaural mix from one state', { timeout: 900_000 }, async () => {
    const state = shot('binaural-field15.state');
    const reference = native(state);
    const expected = samples(reference);
    const browser = await start({ args: NO_WEBGPU });
    try {
      const page = await restored(browser, state, true);
      const { reply } = await capture(page);
      assert.equal(reply.error, undefined, JSON.stringify(reply).slice(0, 400));
      const got = samples(reply);
      assert.equal(got.length, expected.length, 'the same number of samples');
      let worst = 0, same = 0;
      for (let i = 0; i < got.length; i++) {
        const difference = Math.abs(got[i] - expected[i]);
        worst = Math.max(worst, difference);
        if (difference === 0) same++;
      }
      console.log(`${got.length / 2} frames: ${same} of ${got.length} samples identical, largest difference ${worst}; `
        + `native ${reference.hash}, browser ${reply.hash}`);
      assert.ok(worst <= TOLERANCE, `the hosts agree within ${TOLERANCE}: largest difference ${worst}`);
      assert.ok(reply.peak[0] > 100, 'there is sound to compare');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });

  test('the presentation costs a bounded share of real time, even on a phone-class processor', { timeout: 900_000 }, async () => {
    const state = shot('binaural-field15.state');
    if (!existsSync(state)) native(state);
    const browser = await start({ args: NO_WEBGPU });
    try {
      const measure = async (binaural, throttle) => {
        const page = await restored(browser, state, binaural);
        const cdp = await page.context().newCDPSession(page);
        await cdp.send('Emulation.setCPUThrottlingRate', { rate: throttle });
        // The best of three runs from the state: the least disturbed.
        let best = Infinity;
        for (let run = 0; run < 3; run++) {
          await page.evaluate((base64) => {
            window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
          }, readFileSync(state).toString('base64'));
          best = Math.min(best, (await capture(page)).ms);
        }
        await page.close();
        return best;
      };
      for (const throttle of [1, 4]) {
        const off = await measure(false, throttle);
        const on = await measure(true, throttle);
        const share = (on - off) / (SECONDS * 1000);
        console.log(`CPU ×${throttle}: ${FRAMES} frames in ${off.toFixed(0)} ms without, ${on.toFixed(0)} ms with Binaural: `
          + `${(share * 100).toFixed(1)}% of real time for ${SECONDS} s of sound`);
        assert.ok(share < (throttle === 1 ? 0.1 : 0.4), `Binaural takes ${(share * 100).toFixed(1)}% of real time at ×${throttle}`);
      }
    } finally {
      await browser.close();
    }
  });
});
