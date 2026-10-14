// The game's own sound in browsers (local only: it needs the user's disc 1,
// the game module and a native xem-headless): from a field state and one in
// the opening movie (whose sound is CD input), the browser's raw Mono,
// Stereo and Wide output, Binaural's bypass, and Binaural turned on and off
// again are the native host's sample for sample. Raw playback is integer
// arithmetic on both hosts, so the tolerance is 0; only while Binaural
// presents do the hosts' own sin, cos and atan2 allow its declared 2
// (binaural.test.mjs). On each host the commands are seen to take effect:
// the three modes sound different in the field, and in the movie Mono is
// Stereo and Wide is not (the sound-mode findings); the bypass is engaged
// and plays Stereo's output; Binaural presents halfway (and changes the
// field's sound) and gives Stereo's output back after its fade.

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
 * commands halfway through its frames (only the capture Binaural presents
 * in has them: it turns Binaural off there). */
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
 * opening movie at frame 4200 of the new-game route, whose sound is only
 * CD input (`cdOnly`: Mono leaves it as Stereo has it, since the driver's
 * CD mix is unreachable, and it never enters the reverb). */
const STATES = [
  { name: 'field 15', file: 'sound-field15.state', setup: [{ cmd: 'enter_field', map: 15 }, { cmd: 'frames', count: 600 }] },
  { name: 'the opening movie', file: 'sound-movie.state', setup: [movieRoute(), { cmd: 'frames', count: 4200 }], cdOnly: true },
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
  assert.equal(replies.length, commands.length, 'a reply to every command');
  return replies;
}

/** A capture's commands after its state is restored: halfway, the
 * presentation's status (a `binaural` command without settings) before the
 * capture's own commands there. */
const played = (capture) => [
  { cmd: 'audio_capture', action: 'start' },
  { cmd: 'frames', count: FRAMES / 2 },
  { cmd: 'binaural' },
  ...(capture.halfway ?? []),
  { cmd: 'frames', count: FRAMES / 2 },
  { cmd: 'audio_capture', action: 'stop', samples: true },
];

/** What the test reads of `played`'s replies: the presentation halfway and
 * the capture. */
const outcome = (replies) => ({ halfway: replies[2], capture: replies[replies.length - 1] });

/** The state, made natively, and the native outcomes from it. */
function native(state) {
  const path = shot(state.file);
  headless([...state.setup, { cmd: 'state_save', path }]);
  const lists = CAPTURES.map((capture) => [RESET, ...capture.before, { cmd: 'state_load', path }, ...played(capture)]);
  const replies = headless(lists.flat());
  let at = 0;
  const outcomes = lists.map((list, i) => outcome(replies.slice(at, at += list.length).slice(-played(CAPTURES[i]).length)));
  return { path, outcomes };
}

/** The browser's outcomes from the same state, the same commands around it. */
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
    const replies = await page.evaluate(async (commands) => {
      const replies = [];
      for (const command of commands) {
        const reply = command.cmd === 'frames' ? await window.xem.runFrames(command.count) : window.xem.command(command);
        replies.push(reply);
        if (reply.error) break;
      }
      return replies;
    }, played(capture));
    for (const reply of replies) assert.equal(reply.error, undefined, JSON.stringify(reply).slice(0, 400));
    out.push(outcome(replies));
  }
  return out;
}

/** On one host, that the commands took effect (`outcomes` in CAPTURES' order). */
function tookEffect(state, host, outcomes) {
  const where = `${state.name}, ${host}`;
  const [mono, stereo, wide, bypassed, onOff] = outcomes;
  // The modes: Mono's centre law and Wide's signs change the field's sound;
  // the movie's CD input only Wide changes.
  if (state.cdOnly) {
    assert.equal(mono.capture.hash, stereo.capture.hash, `${where}: Mono is Stereo where the sound is CD input`);
  } else {
    assert.notEqual(mono.capture.hash, stereo.capture.hash, `${where}: Mono is not Stereo`);
    assert.notEqual(mono.capture.hash, wide.capture.hash, `${where}: Mono is not Wide`);
  }
  assert.notEqual(wide.capture.hash, stereo.capture.hash, `${where}: Wide is not Stereo`);
  // The presentation halfway: only the capture that turns it off then
  // presents; the bypass is engaged, presenting the original mix.
  outcomes.forEach((o, i) => {
    assert.equal(o.halfway.presenting, Boolean(CAPTURES[i].halfway), `${where}, ${CAPTURES[i].name}: presenting halfway`);
  });
  assert.ok(bypassed.halfway.config.enabled && bypassed.halfway.config.bypass, `${where}: the bypass is engaged`);
  const original = samples(stereo.capture);
  const bypass = samples(bypassed.capture);
  assert.ok(bypass.every((s, k) => s === original[k]), `${where}: the bypass plays Stereo's output`);
  // Binaural on, then off: something else while it presents (in the field),
  // Stereo's output again from the end of its fade.
  const round = samples(onOff.capture);
  const presenting = FRAMES / 2 * PER_FRAME + FADE;
  let changed = 0, back = 0;
  for (let k = 0; k < round.length; k += 2) {
    if (round[k] === original[k] && round[k + 1] === original[k + 1]) continue;
    if (k < presenting) changed++;
    else back++;
  }
  console.log(`${where}: Binaural changed ${changed} of the ${presenting / 2} samples to the end of its fade`);
  if (!state.cdOnly) assert.ok(changed > 0, `${where}: Binaural presents another mix than Stereo's`);
  assert.equal(back, 0, `${where}: Stereo's output is back after the fade`);
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
          const [expected, reply] = [samples(state.outcomes[i].capture), samples(got[i].capture)];
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
          console.log(`${state.name}, ${capture.name}: native ${state.outcomes[i].capture.hash}, browser ${got[i].capture.hash}`
            + (presenting > 0 ? `; presenting within ${worst}` : '') + `; raw within ${worstRaw}`);
          assert.equal(worstRaw, 0, `${state.name}, ${capture.name}: the raw output is the native one (first difference at sample ${firstRaw / 2})`);
          assert.ok(worst <= BINAURAL_TOLERANCE, `${state.name}, ${capture.name}: Binaural within ${BINAURAL_TOLERANCE}: ${worst}`);
          if (!capture.halfway) assert.equal(got[i].capture.hash, state.outcomes[i].capture.hash, `${state.name}, ${capture.name}: the same hash`);
          assert.ok(reply.some((s) => s !== 0), `${state.name}, ${capture.name}: there is sound to compare`);
        });
        tookEffect(state, 'native', state.outcomes);
        tookEffect(state, 'browser', got);
      }
      assert.deepEqual(page.errors, []);
    } finally {
      await session.close();
    }
  });
});
