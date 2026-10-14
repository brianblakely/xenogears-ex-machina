// The game's own sound in browsers (local only: it needs the user's disc 1,
// the game module and a native xem-headless): from a field state and one in
// the opening movie (whose sound is CD input), the browser's raw Mono,
// Stereo and Wide output, Binaural's bypass, and Binaural turned on and off
// again are the native host's sample for sample. Raw playback is integer
// arithmetic on both hosts, so the tolerance is 0; only while Binaural
// presents do the hosts' own sin, cos and atan2 allow its declared 2
// (binaural.test.mjs).

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, ROOT, USER_DISC, WEB, shot, start } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
const FRAMES = 240;
/** Binaural's declared tolerance between hosts, while it presents. */
const BINAURAL_TOLERANCE = 2;
/** Samples per frame (44.1 kHz at 60 Hz), interleaved left and right. */
const PER_FRAME = 2 * 735;
/** The presentation's fade (100 ms) and one block, after Binaural goes off. */
const FADE = 2 * (4410 + 128);

/** What each capture plays: commands before the state is restored, and
 * commands halfway through its frames. */
const CAPTURES = [
  { name: 'Mono', before: [{ cmd: 'sound_mode', mode: 0 }] },
  { name: 'Stereo', before: [{ cmd: 'sound_mode', mode: 1 }] },
  { name: 'Wide', before: [{ cmd: 'sound_mode', mode: 2 }] },
  { name: 'Binaural bypassed', before: [{ cmd: 'sound_mode', mode: 1 }, { cmd: 'binaural', enabled: true, bypass: true }] },
  { name: 'Binaural on, then off', before: [{ cmd: 'sound_mode', mode: 1 }, { cmd: 'binaural', enabled: true }],
    halfway: [{ cmd: 'binaural', enabled: false }] },
];
/** Each capture starts with the presentation off and not bypassed. */
const RESET = { cmd: 'binaural', enabled: false, bypass: false };

/** The new-game route into the opening movie (pad 1 by frame), as a schedule. */
function movieRoute() {
  const holds = JSON.parse(readFileSync(join(ROOT, 'tests', 'reference-inputs', 'new-game-sound-mode-stereo.json'), 'utf8'));
  const commands = holds.flatMap((hold) => [
    [hold.start, { cmd: 'pad', port: 0, buttons: hold.buttons }],
    [hold.end, { cmd: 'pad', port: 0, buttons: [] }],
  ]).sort((a, b) => a[0] - b[0]);
  return { cmd: 'schedule', commands };
}

/** The states: field 15 (its music and three positional emitters), and the
 * opening movie at frame 4200 of the new-game route. */
const STATES = [
  { name: 'field 15', file: 'sound-field15.state', setup: [{ cmd: 'enter_field', map: 15 }, { cmd: 'frames', count: 600 }] },
  { name: 'the opening movie', file: 'sound-movie.state', setup: [movieRoute(), { cmd: 'frames', count: 4200 }] },
];

const samples = (reply) => {
  const hex = reply.samples;
  const out = new Int16Array(hex.length / 4);
  for (let i = 0; i < out.length; i++) {
    const lo = parseInt(hex.slice(i * 4, i * 4 + 2), 16), hi = parseInt(hex.slice(i * 4 + 2, i * 4 + 4), 16);
    out[i] = (hi << 8 | lo) << 16 >> 16;
  }
  return out;
};

/** Runs `commands` in one native xem-headless --control session; the replies. */
function headless(commands) {
  const output = execFileSync(HEADLESS, ['--disc', USER_DISC, '--control', '--wav', shot('sound-unused.wav')], {
    input: commands.map((c) => JSON.stringify(c)).join('\n') + '\n',
    maxBuffer: 256 << 20,
  });
  const replies = output.toString().trim().split('\n').map((line) => JSON.parse(line));
  for (const reply of replies) assert.equal(reply.error, undefined, JSON.stringify(reply).slice(0, 400));
  return replies;
}

/** A capture's commands after its state is restored. */
const played = (capture) => [
  { cmd: 'audio_capture', action: 'start' },
  { cmd: 'frames', count: FRAMES / 2 },
  ...(capture.halfway ?? []),
  { cmd: 'frames', count: FRAMES / 2 },
  { cmd: 'audio_capture', action: 'stop', samples: true },
];

/** The state, made natively, and the native captures from it. */
function native(state) {
  const path = shot(state.file);
  headless([...state.setup, { cmd: 'state_save', path }]);
  const commands = CAPTURES.flatMap((capture) => [RESET, ...capture.before, { cmd: 'state_load', path }, ...played(capture)]);
  const replies = headless(commands);
  const stops = replies.filter((reply) => reply.samples !== undefined);
  assert.equal(stops.length, CAPTURES.length);
  return { path, captures: stops };
}

/** The browser's captures from the same state, the same commands around it. */
async function browser(page, path) {
  const state = readFileSync(path).toString('base64');
  const out = [];
  for (const capture of CAPTURES) {
    for (const command of [RESET, ...capture.before]) {
      const reply = await page.evaluate((c) => window.xem.command(c), command);
      assert.equal(reply.error, undefined, JSON.stringify(reply));
    }
    await page.evaluate((base64) => {
      window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
    }, state);
    const reply = await page.evaluate(async (commands) => {
      let last = null;
      for (const command of commands) {
        last = command.cmd === 'frames' ? await window.xem.runFrames(command.count) : window.xem.command(command);
        if (last.error) return last;
      }
      return last;
    }, played(capture));
    assert.equal(reply.error, undefined, JSON.stringify(reply).slice(0, 400));
    out.push(reply);
  }
  return out;
}

describe('the game\'s own sound in browsers', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  test('raw modes, the bypass and Binaural turned off again sound as natively, sample for sample', { timeout: 1_800_000 }, async () => {
    const states = STATES.map((state) => ({ ...state, ...native(state) }));
    const session = await start({ args: NO_WEBGPU });
    try {
      const page = await session.page();
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(() => window.xem.boot());
      for (const state of states) {
        const got = await browser(page, state.path);
        CAPTURES.forEach((capture, i) => {
          const [expected, reply] = [samples(state.captures[i]), samples(got[i])];
          assert.equal(reply.length, expected.length, `${state.name}, ${capture.name}: the same number of samples`);
          // A presentation still under way holds back the block it is
          // mixing (less than a block); one that ended has handed it on.
          const missing = FRAMES * PER_FRAME - reply.length;
          const held = capture.name === 'Binaural bypassed' ? 2 * 128 : 0;
          assert.ok(missing >= 0 && missing <= held, `${state.name}, ${capture.name}: ${FRAMES} frames of sound but ${missing / 2} samples`);
          // Raw playback everywhere but while Binaural presents: from the
          // start to the end of its fade, a block on.
          const presenting = capture.halfway ? FRAMES / 2 * PER_FRAME + FADE : 0;
          let worst = 0, worstRaw = 0, firstRaw = -1;
          for (let k = 0; k < reply.length; k++) {
            const difference = Math.abs(reply[k] - expected[k]);
            if (k < presenting) {
              worst = Math.max(worst, difference);
            } else if (difference > worstRaw) {
              worstRaw = difference;
              if (firstRaw < 0) firstRaw = k;
            }
          }
          console.log(`${state.name}, ${capture.name}: native ${state.captures[i].hash}, browser ${got[i].hash}`
            + (presenting > 0 ? `; presenting within ${worst}` : '') + `; raw within ${worstRaw}`);
          assert.equal(worstRaw, 0, `${state.name}, ${capture.name}: the raw output is the native one (first difference at sample ${firstRaw / 2})`);
          assert.ok(worst <= BINAURAL_TOLERANCE, `${state.name}, ${capture.name}: Binaural within ${BINAURAL_TOLERANCE}: ${worst}`);
          if (!capture.halfway) assert.equal(got[i].hash, state.captures[i].hash, `${state.name}, ${capture.name}: the same hash`);
          assert.ok(reply.some((s) => s !== 0), `${state.name}, ${capture.name}: there is sound to compare`);
        });
      }
      assert.deepEqual(page.errors, []);
    } finally {
      await session.close();
    }
  });
});
