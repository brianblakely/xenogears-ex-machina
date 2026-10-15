// The self-administered listening test's page (tools/listening) in
// headless Chromium, answered by a script, then analysed. Every session the
// manifests plan runs to its end through the page's own controls (the three
// headphone checks, the azimuth dial, the elevation slider, its Level button
// and the start and end markers, the MUSHRA buttons and sliders), each answer
// saved by tools/listening/serve.py; no answer is taken before its height is
// set; a session is left mid-block and resumed; a save the server refuses
// stops the session with a message, and starting it again asks that trial
// again; and (on the synthetic set) an unfinished session saved against
// other stimuli is left beside a fresh one; then
// tools/listening/analyse.py reads the answers back as the script gave them,
// against expectations worked out here independently. Nobody listens: this
// shows the page and the analysis work, not what anyone hears.
//
// Needs no private inputs: by default the stimuli are a small synthetic set
// written here in the generator's format. With XEM_LISTENING_STIMULI=DIR it
// takes the stimuli the generator wrote there (cargo run --release -p
// xem-headless --example listening -- localization, and -- music) and runs
// every session they plan; the answers go to a temporary directory, never
// DIR's results.
//
//   cd src/runtime/web && node --test tests/listening.test.mjs

import assert from 'node:assert/strict';
import { execFileSync, spawn } from 'node:child_process';
import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';
import { chromium } from 'playwright-core';
import { ROOT } from './support.mjs';

const REAL = process.env.XEM_LISTENING_STIMULI;
const LISTENING = join(ROOT, 'tools', 'listening');

// --- a synthetic stimulus set in the generator's format ---------------------

function wav(path, seconds, seed) {
  const frames = Math.round(seconds * 44100);
  const data = Buffer.alloc(44 + frames * 4);
  data.write('RIFF', 0); data.writeUInt32LE(36 + frames * 4, 4); data.write('WAVEfmt ', 8);
  data.writeUInt32LE(16, 16); data.writeUInt16LE(1, 20); data.writeUInt16LE(2, 22);
  data.writeUInt32LE(44100, 24); data.writeUInt32LE(44100 * 4, 28); data.writeUInt16LE(4, 32); data.writeUInt16LE(16, 34);
  data.write('data', 36); data.writeUInt32LE(frames * 4, 40);
  let x = seed * 2654435761 >>> 0;
  for (let n = 0; n < frames; n++) {
    x = (Math.imul(x, 1664525) + 1013904223) >>> 0;
    const s = ((x >>> 16) - 32768) >> 4;
    data.writeInt16LE(s, 44 + n * 4);
    data.writeInt16LE(s, 46 + n * 4);
  }
  mkdirSync(join(path, '..'), { recursive: true });
  writeFileSync(path, data);
}

function fixture(dir) {
  const generated = { tool: 'listening.test.mjs, a synthetic set', seed: 1 };
  const file = (name) => `localization/${name}.wav`;
  const stimuli = [];
  let seed = 1;
  const add = (stimulus, seconds = 0.12) => {
    stimuli.push({ ...stimulus, file: file(stimulus.id), token: 0, loudness: -23 });
    wav(join(dir, file(stimulus.id)), seconds, seed++);
  };
  const directions = [0, 30, 150, 180, 210, 330].flatMap((a) => [[a, 0], [a, 30], [a, -30]]);
  for (const condition of ['rendered', 'kemar', 'stereo']) {
    for (const [a, e] of directions) add({ id: `${condition}-${a}-${e}`, condition, motion: 'static', start: [a, e], end: [a, e] });
  }
  add({ id: 'rendered-move', condition: 'rendered', motion: 'moving', path: 'h000cw', start: [330, 0], end: [30, 0] }, 0.3);
  add({ id: 'stereo-move', condition: 'stereo', motion: 'moving', path: 'v090up', start: [90, -30], end: [90, 30] }, 0.3);
  add({ id: 'catch-left', condition: 'catch', motion: 'static', side: 'left', start: [270, 0], end: [270, 0] });
  add({ id: 'catch-right', condition: 'catch', motion: 'static', side: 'right', start: [90, 0], end: [90, 0] });
  add({ id: 'training-a', condition: 'rendered', motion: 'static', training: true, start: [60, 30], end: [60, 30] });
  add({ id: 'training-b', condition: 'rendered', motion: 'static', training: true, start: [240, 0], end: [240, 0] });
  for (const name of ['pink', 'left', 'right', 'in-phase', 'anti-phase']) wav(join(dir, file(`calibration/${name}`)), 0.15, seed++);
  const tests = stimuli.filter((s) => !s.training).map((s) => s.id);
  // A fixed shuffle, as the generator's.
  tests.sort((a, b) => (a.length * 7 + a.charCodeAt(0)) % 5 - (b.length * 7 + b.charCodeAt(0)) % 5 || a.localeCompare(b));
  const half = Math.ceil(tests.length / 2);
  const trial = (id, block, i) => ({ trial: `L1-${block}-${String(i + 1).padStart(3, '0')}`, stimulus: id, rove_db: ((i % 7) - 3) * 0.5, catch: id.startsWith('catch') });
  writeFileSync(join(dir, 'localization.json'), JSON.stringify({
    kind: 'xem-listening-localization', version: 1, generated,
    level: { target_lufs: -23, rove_db: 3 },
    conditions: { rendered: 'r', kemar: 'k', stereo: 's', catch: 'c' },
    calibration: { pink: file('calibration/pink'), left: file('calibration/left'), right: file('calibration/right'), in_phase: file('calibration/in-phase'), anti_phase: file('calibration/anti-phase') },
    stimuli,
    sessions: [{
      name: 'L1',
      training: ['training-a', 'training-b'].map((id, i) => ({ trial: `L1-T-${i + 1}`, stimulus: id, rove_db: 0, feedback: true })),
      blocks: [tests.slice(0, half).map((id, i) => trial(id, 1, i)), tests.slice(half).map((id, i) => trial(id, 2, i))],
    }],
  }));
  const conditions = ['reference', 'anchor', 'immersion-0', 'immersion-35', 'immersion-70', 'immersion-100'];
  const excerpts = ['song-a', 'song-b'].map((id, e) => {
    const described = {};
    for (const [c, name] of conditions.entries()) {
      const path = `coherence/${id}/${name}.wav`;
      wav(join(dir, path), 0.6, 100 + e * 10 + c);
      described[name] = { file: path, loudness: -20, gain_db: -3, peak: 0.1, iacc: [0.5, 0.5, 0.9 - c * 0.1, 0.9 - c * 0.1, 0.9 - c * 0.1, 0.9 - c * 0.1, 0.5] };
    }
    return { id, label: `Synthetic ${id}`, kind: 'test', seconds: 0.6, conditions: described, identical: [], estimate: false };
  });
  const attributes = [
    { id: 'coherence', label: 'Coherence', question: 'q', high: 'h', low: 'l', reference_top: true },
    { id: 'stability', label: 'Lead and foundation', question: 'q', high: 'h', low: 'l', reference_top: true },
    { id: 'envelopment', label: 'Envelopment', question: 'q', high: 'h', low: 'l', reference_top: false },
    { id: 'tone', label: 'Tone colour', question: 'q', high: 'h', low: 'l', reference_top: true },
    { id: 'preference', label: 'Preference', question: 'q', high: 'h', low: 'l', reference_top: false },
  ];
  const sessions = ['C1', 'C2'].map((name, s) => ({
    name,
    trials: excerpts.flatMap((e) => attributes.map((a) => ({ excerpt: e.id, attribute: a.id })))
      .map((t, i) => ({ trial: `${name}-${String(i + 1).padStart(2, '0')}`, ...t, order: [...conditions].sort((x, y) => ((x.length + i + s) % 3) - ((y.length + i + s) % 3) || x.localeCompare(y)) })),
  }));
  writeFileSync(join(dir, 'coherence.json'), JSON.stringify({
    kind: 'xem-listening-coherence', version: 1, generated: { tool: 'listening.test.mjs, a synthetic set', seed: 2 },
    level: { target_lufs: -23 }, attributes, conditions: Object.fromEntries(conditions.map((c) => [c, c])), excerpts, sessions,
  }));
}

// --- the script's answers and what the analysis must make of them -----------

const rad = (d) => d * Math.PI / 180;
const deg = (r) => r * 180 / Math.PI;
/** Lateral (positive right) and polar (-90 to 270) angles, written apart
 * from analyse.py's. */
function interaural(azimuth, elevation) {
  const x = Math.cos(rad(elevation)) * Math.cos(rad(azimuth));
  const y = Math.cos(rad(elevation)) * Math.sin(rad(azimuth));
  const z = Math.sin(rad(elevation));
  let polar = deg(Math.atan2(z, x));
  if (polar < -90) polar += 360;
  return [deg(Math.asin(Math.max(-1, Math.min(1, y)))), polar];
}

/** The answer the script gives a static trial: rendered ones exactly; raw
 * KEMAR ones exactly but every other front/back-eligible one mirrored
 * front to back; Stereo ones at the right azimuth, always level. */
function scripted(stimulus, count) {
  const [a, e] = stimulus.start;
  if (stimulus.condition === 'catch') return { azimuth: stimulus.side === 'right' ? 90 : 270, elevation: 0 };
  if (stimulus.condition === 'stereo') return { azimuth: a, elevation: 0 };
  if (stimulus.condition === 'kemar') {
    const [lateral, polar] = interaural(a, e);
    const eligible = Math.abs(lateral) <= 30 + 1e-6 && (polar <= 60 + 1e-6 || polar >= 120 - 1e-6);
    if (eligible && count.kemar++ % 2 === 0) return { azimuth: ((180 - a) % 360 + 360) % 360, elevation: e, mirrored: true };
  }
  return { azimuth: a, elevation: e };
}

const RATINGS = { reference: 100, anchor: 8, 'immersion-0': 72, 'immersion-35': 64, 'immersion-70': 57, 'immersion-100': 45 };

// --- driving the page ---------------------------------------------------------

async function setDial(page, azimuth) {
  const box = await page.locator('#dial svg').boundingBox();
  const scale = box.width / 360;
  await page.mouse.click(box.x + (180 + 100 * Math.sin(rad(azimuth))) * scale, box.y + (180 - 100 * Math.cos(rad(azimuth))) * scale);
}

async function setRange(page, selector, value) {
  await page.locator(selector).fill(String(value));
}

/** The marker's height: Level for 0, the slider otherwise. */
async function setElevation(page, value) {
  if (value === 0) await page.click('#level');
  else await setRange(page, '#elevation', value);
}

async function calibrate(page) {
  await page.waitForSelector('[data-step="level"]');
  await page.click('#level-play');
  await page.click('#level-play');
  await page.click('#level-done');
  for (let i = 0; i < 2; i++) {
    const panel = page.locator(`[data-step="left-right-0-${i}"]`);
    await panel.waitFor();
    const side = await panel.getAttribute('data-check');
    await page.click('#lr-play');
    await page.click(`#lr-${side}`);
  }
  const polarity = page.locator('[data-step="polarity"]');
  await polarity.waitFor();
  const inPhase = await polarity.getAttribute('data-check');
  await page.click('#pol-a');
  await page.click('#pol-b');
  await page.click(`#pol-answer-${inPhase}`);
}

async function begin(page, base, session, reload = true) {
  if (reload) await page.goto(base);
  await page.locator(`[data-session="${session}"]`).waitFor();
  await page.fill('#listener', 'scripted');
  await page.fill('#headphones', 'none (a script)');
  await page.click(`[data-session="${session}"]`);
  await calibrate(page);
}

/** A localization session answered to its end: left after `resumeAfter`
 * answers and resumed, and the save of answer `failAt` (from 0) refused
 * once. How many times the session was started. */
async function localizationSession(page, base, manifest, session, resumeAfter, failAt) {
  const stimuli = new Map(manifest.stimuli.map((s) => [s.id, s]));
  const plan = [...session.training.map((t) => ({ ...t, training: true, block: 'training' })),
    ...session.blocks.flatMap((b, i) => b.map((t) => ({ ...t, block: i + 1 })))];
  const count = { kemar: 0 };
  // Each trial's answer, kept for a trial asked again.
  const answers = new Map();
  await begin(page, base, session.name);
  let starts = 1;
  let previous = null;
  let done = 0;
  while (done < plan.length) {
    const trial = plan[done];
    if (trial.block !== previous) {
      await page.click('#block-start');
      previous = trial.block;
    }
    await page.locator(`[data-trial="${trial.trial}"]`).waitFor();
    const stimulus = stimuli.get(trial.stimulus);
    if (done === 0) {
      // The sound has played and the dial is set, but no height yet: no answer.
      await page.locator('#playing', { hasText: 'Your answer:' }).waitFor();
      await setDial(page, stimulus.start[0]);
      assert.equal(await page.locator('#answer').isDisabled(), true, 'no answer before its height is set');
    }
    if (stimulus.motion === 'moving') {
      for (const [mark, at] of [['start', stimulus.start], ['end', stimulus.end]]) {
        await page.click(`#mark-${mark}`);
        await setDial(page, at[0]);
        await setElevation(page, at[1]);
      }
    } else {
      if (!answers.has(done)) answers.set(done, trial.training ? { azimuth: stimulus.start[0], elevation: stimulus.start[1] } : scripted(stimulus, count));
      const answer = answers.get(done);
      await setDial(page, answer.azimuth);
      await setElevation(page, answer.elevation);
    }
    const failing = done === failAt;
    if (failing) {
      await page.route('**/results/*.json', (route) => (route.request().method() === 'PUT'
        ? route.fulfill({ status: 503, contentType: 'text/plain', body: 'the server is away (the test)' })
        : route.continue()));
    }
    await page.click('#answer');
    if (trial.training) {
      await page.locator('#feedback').waitFor();
      await page.click('#next');
    }
    if (failing) {
      // The refused save stops the session with a message; started again,
      // it resumes at the trial whose answer was not saved.
      await page.locator('[data-step="error"]').waitFor();
      assert.match(await page.locator('#error').textContent(), /503/);
      await page.unroute('**/results/*.json');
      await page.click('#error-home');
      await begin(page, base, session.name, false);
      starts++;
      failAt = null;
      previous = done > 0 ? plan[done - 1].block : null;
      continue;
    }
    done++;
    if (resumeAfter && done === resumeAfter) {
      // Leave mid-block and come back: the page resumes after the last
      // saved answer, without the block's introduction.
      await page.locator(`[data-trial="${plan[done].trial}"]`).waitFor();
      await begin(page, base, session.name);
      starts++;
      resumeAfter = null;
    }
  }
  await page.locator('text=is done and saved').waitFor();
  return starts;
}

async function coherenceSession(page, base, manifest, session) {
  await begin(page, base, session.name);
  await page.click('#coherence-start');
  for (const trial of session.trials) {
    await page.locator(`[data-trial="${trial.trial}"]`).waitFor();
    await page.click('#play-reference');
    for (const [i, condition] of trial.order.entries()) {
      const letter = 'ABCDEFGHIJ'[i];
      await page.click(`#play-${letter}`);
      await setRange(page, `#rate-${letter}`, RATINGS[condition]);
    }
    await page.click('#stop');
    await page.click('#next');
  }
  await page.locator('text=is done and saved').waitFor();
}

function startServer(stimuli, results) {
  return new Promise((resolve, reject) => {
    const child = spawn('python3', [join(LISTENING, 'serve.py'), '--stimuli', stimuli, '--results', results, '--port', '0'], { stdio: ['ignore', 'pipe', 'pipe'] });
    let out = '';
    child.stdout.on('data', (chunk) => {
      out += chunk;
      const match = out.match(/http:\/\/127\.0\.0\.1:(\d+)\//);
      if (match) resolve({ child, base: match[0] });
    });
    child.stderr.on('data', () => {});
    child.on('exit', (code) => reject(new Error(`serve.py exited (${code}): ${out}`)));
  });
}

test('the listening page runs every planned session on scripted answers and the analysis reads them back', { timeout: 4 * 60 * 60 * 1000 }, async () => {
  const work = mkdtempSync(join(tmpdir(), 'xem-listening-'));
  const stimuli = REAL ?? join(work, 'stimuli');
  if (!REAL) fixture(stimuli);
  const results = join(work, 'results');
  const localization = JSON.parse(readFileSync(join(stimuli, 'localization.json'), 'utf8'));
  let coherence = null;
  try { coherence = JSON.parse(readFileSync(join(stimuli, 'coherence.json'), 'utf8')); } catch { /* localization only */ }
  // On the synthetic set, an unfinished L1 saved against other stimuli
  // waits in the results: the page must start L1 afresh beside it, and the
  // analysis leave it out.
  const files = Object.fromEntries([...localization.sessions, ...(coherence?.sessions ?? [])].map((s) => [s.name, `scripted-${s.name}.json`]));
  if (!REAL) {
    mkdirSync(results, { recursive: true });
    const stale = { kind: 'xem-listening-results', version: 1, part: 'localization', session: 'L1', file: 'scripted-L1.json', listener: 'scripted',
      complete: false, manifest: { kind: localization.kind, version: 1, generated: { ...localization.generated, seed: 999 } }, calibrations: [],
      trials: [{ trial: 'L1-1-001', stimulus: 'gone', training: false, response: { azimuth: 0, elevation: 0 } }], ratings: [] };
    writeFileSync(join(results, 'scripted-L1.json'), JSON.stringify(stale));
    files.L1 = 'scripted-L1-r2.json';
  }
  const { child, base } = await startServer(stimuli, results);
  const starts = {};
  const browser = await chromium.launch({ executablePath: process.env.XEM_CHROMIUM, headless: true, args: ['--autoplay-policy=no-user-gesture-required'] });
  try {
    const context = await browser.newContext({ viewport: { width: 1280, height: 1000 } });
    context.setDefaultTimeout(60_000);
    const page = await context.newPage();
    const errors = [];
    page.on('pageerror', (error) => errors.push(error.message));
    for (const [i, session] of localization.sessions.entries()) starts[session.name] = await localizationSession(page, base, localization, session, i === 0 ? 5 : null, i === 0 ? 8 : null);
    for (const session of coherence?.sessions ?? []) await coherenceSession(page, base, coherence, session);
    assert.deepEqual(errors, [], 'no errors in the page');
  } finally {
    await browser.close();
    child.kill();
  }

  // Each session's answers were saved, every trial once, the calibration
  // once a start (three in the first session: resumed, and started again
  // after the refused save).
  for (const session of localization.sessions) {
    const saved = JSON.parse(readFileSync(join(results, files[session.name]), 'utf8'));
    assert.equal(saved.complete, true);
    assert.equal(saved.headphones, 'none (a script)');
    assert.deepEqual(saved.manifest.generated, localization.generated);
    const planned = [...session.training, ...session.blocks.flat()].map((t) => t.trial);
    assert.deepEqual(saved.trials.map((t) => t.trial), planned, `${session.name}: every trial once, in order`);
    assert.equal(saved.calibrations.length, starts[session.name]);
    assert.ok(saved.calibrations.every((c) => c.level_set && c.left_right_ok && c.polarity_ok));
  }
  for (const session of coherence?.sessions ?? []) {
    const saved = JSON.parse(readFileSync(join(results, files[session.name]), 'utf8'));
    assert.equal(saved.complete, true);
    assert.deepEqual(saved.ratings.map((r) => r.trial), session.trials.map((t) => t.trial));
    for (const r of saved.ratings) {
      assert.deepEqual(r.ratings, Object.fromEntries(r.order.map((c) => [c, RATINGS[c]])));
      assert.ok(Object.values(r.plays).every((n) => n >= 1), 'every version played');
    }
  }

  // The analysis, against what the answers say, worked out here.
  const out = join(work, 'analysis.json');
  execFileSync('python3', [join(LISTENING, 'analyse.py'), '--stimuli', stimuli, '--results', results, '--models', join(work, 'none.json'), '--json', out, '--bootstrap', '200'], { stdio: 'pipe' });
  const analysis = JSON.parse(readFileSync(out, 'utf8'));
  assert.deepEqual(analysis.notes, REAL ? [] : ['scripted-L1.json: answered against other stimuli than the current manifest\'s (made at another commit, with another seed or other files); left out']);
  assert.equal(analysis.listener, 'scripted');
  const stimuliById = new Map(localization.stimuli.map((s) => [s.id, s]));
  const count = { kemar: 0 };
  const expected = { rendered: [], kemar: [], stereo: [] };
  let catches = 0;
  for (const session of localization.sessions) {
    for (const trial of session.blocks.flat()) {
      const s = stimuliById.get(trial.stimulus);
      if (s.condition === 'catch') { catches++; scripted(s, count); continue; }
      if (s.motion !== 'static') continue;
      expected[s.condition].push([s, scripted(s, count)]);
    }
  }
  const close = (actual, wanted, what, tolerance = 1e-6) => assert.ok(Math.abs(actual - wanted) <= tolerance, `${what}: ${actual} against ${wanted}`);
  const loc = analysis.localization;
  // Rendered: exact, so no error at all.
  close(loc.static.rendered.lateral_rms, 0, 'rendered lateral error');
  close(loc.static.rendered.quadrant_error, 0, 'rendered quadrant errors');
  close(loc.static.rendered.front_back, 0, 'rendered front/back');
  close(loc.static.rendered.up_down, 0, 'rendered up/down');
  close(loc.static.rendered.elevation_gain, 1, 'rendered elevation gain');
  // KEMAR: the mirrored ones are the front/back confusions and keep the
  // lateral angle.
  const eligible = expected.kemar.filter(([s]) => {
    const [lateral, polar] = interaural(...s.start);
    return Math.abs(lateral) <= 30 + 1e-6 && (polar <= 60 + 1e-6 || polar >= 120 - 1e-6);
  });
  const mirrored = eligible.filter(([, answer]) => answer.mirrored).length;
  assert.ok(mirrored > 0);
  close(loc.static.kemar.front_back, (100 * mirrored) / eligible.length, 'KEMAR front/back');
  close(loc.static.kemar.lateral_rms, 0, 'KEMAR lateral error');
  // Stereo: level answers, so elevation gain 0 and a lateral error from the
  // targets' elevations alone.
  close(loc.static.stereo.elevation_gain, 0, 'Stereo elevation gain');
  const lateral = Math.sqrt(expected.stereo.reduce((sum, [s, a]) => sum + (interaural(a.azimuth, a.elevation)[0] - interaural(...s.start)[0]) ** 2, 0) / expected.stereo.length);
  close(loc.static.stereo.lateral_rms, lateral, 'Stereo lateral error');
  for (const condition of ['rendered', 'kemar', 'stereo']) assert.equal(loc.static[condition].n, expected[condition].length, `${condition} trials`);
  assert.equal(loc.catch.n, catches);
  close(loc.catch.correct, 100, 'catch trials');
  for (const condition of ['rendered', 'stereo']) {
    if (loc.moving[condition].n) close(loc.moving[condition].right_way, 100, `${condition} moving`);
  }
  if (coherence) {
    const coh = analysis.coherence;
    for (const attribute of coherence.attributes) {
      for (const [condition, value] of Object.entries(RATINGS)) close(coh.attributes[attribute.id][condition].mean, value, `${attribute.id} ${condition}`);
    }
    assert.equal(coh.post_screening.passed, true);
    assert.equal(coh.repeats?.mean_absolute_change ?? 0, 0);
  }
  console.log(`listening page: ${localization.sessions.length} localization and ${coherence?.sessions.length ?? 0} coherence sessions on scripted answers; ` +
    `KEMAR front/back ${loc.static.kemar.front_back.toFixed(1)}% as scripted (${mirrored} of ${eligible.length}); results and analysis in ${work}`);
});
