// The self-administered listening test (tools/listening/README.md): the
// sessions the stimulus generator planned (localization.json,
// coherence.json under stimuli/), each after the headphone checks, every
// answer saved at once to results/ by serve.py. Nothing here judges the
// answers; analyse.py does, afterwards.

const VERSION = 1;
const app = document.getElementById('app');

let audio = null;
const cache = new Map();
const CACHE_SIZE = 24;
let manifests = { localization: null, coherence: null };
let run = null;

// ---------------------------------------------------------------- helpers

function h(tag, props = {}, ...children) {
  const node = document.createElement(tag);
  for (const [key, value] of Object.entries(props)) {
    if (value === undefined || value === null || value === false) continue;
    if (key === 'class') node.className = value;
    else if (key === 'dataset') Object.assign(node.dataset, value);
    else if (key.startsWith('on')) node.addEventListener(key.slice(2), value);
    else node.setAttribute(key, value === true ? '' : value);
  }
  for (const child of children.flat()) {
    if (child === null || child === undefined || child === false) continue;
    node.append(child instanceof Node ? child : document.createTextNode(String(child)));
  }
  return node;
}

function svg(tag, props = {}, ...children) {
  const node = document.createElementNS('http://www.w3.org/2000/svg', tag);
  for (const [key, value] of Object.entries(props)) node.setAttribute(key, value);
  for (const child of children) node.append(child);
  return node;
}

function show(...nodes) {
  app.replaceChildren(...nodes.filter(Boolean));
  window.scrollTo(0, 0);
}

/** A promise and the function that keeps it. */
function deferred() {
  let resolve;
  const promise = new Promise((r) => { resolve = r; });
  return { promise, resolve };
}

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

/** A small seeded generator (mulberry32) for the checks' orders. */
function generator(text) {
  let seed = 2166136261;
  for (const c of text) seed = Math.imul(seed ^ c.charCodeAt(0), 16777619) >>> 0;
  return () => {
    seed = (seed + 0x6D2B79F5) >>> 0;
    let t = seed;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function direction(azimuth, elevation) {
  const a = Math.round(((azimuth % 360) + 360) % 360);
  const side = a === 0 ? 'ahead' : a === 180 ? 'behind' : a < 180 ? `${a}° (right)` : `${a}° (left)`;
  const e = Math.round(elevation);
  return `${side}, ${e === 0 ? 'level' : e > 0 ? `${e}° up` : `${-e}° down`}`;
}

// ---------------------------------------------------------------- server

async function getJSON(url) {
  const response = await fetch(url, { cache: 'no-store' });
  if (!response.ok) throw new Error(`${url}: ${response.status}`);
  return response.json();
}

async function save() {
  run.saved = new Date().toISOString();
  const response = await fetch(`results/${encodeURIComponent(run.file)}`, {
    method: 'PUT', headers: { 'content-type': 'application/json' }, body: JSON.stringify(run, null, 1),
  });
  if (!response.ok) throw new Error(`saving ${run.file}: ${response.status} ${await response.text()}`);
}

// ---------------------------------------------------------------- audio

async function context() {
  if (!audio) audio = new AudioContext({ sampleRate: 44100, latencyHint: 'interactive' });
  if (audio.state === 'suspended') await audio.resume();
  return audio;
}

/** A stimulus decoded (kept for the next few uses; one fetch however
 * many ask at once). */
function buffer(file) {
  if (cache.has(file)) {
    const kept = cache.get(file);
    cache.delete(file);
    cache.set(file, kept);
    return kept;
  }
  const decoded = (async () => {
    const response = await fetch(`stimuli/${file}`);
    if (!response.ok) throw new Error(`stimuli/${file}: ${response.status}`);
    return (await context()).decodeAudioData(await response.arrayBuffer());
  })();
  decoded.catch(() => cache.delete(file));
  cache.set(file, decoded);
  while (cache.size > CACHE_SIZE) cache.delete(cache.keys().next().value);
  return decoded;
}

/** Plays a buffer once (or looped) at `db`; `done` resolves when it ends. */
function play(buf, db = 0, loop = false) {
  const source = audio.createBufferSource();
  source.buffer = buf;
  source.loop = loop;
  const gain = audio.createGain();
  gain.gain.value = 10 ** (db / 20);
  source.connect(gain).connect(audio.destination);
  const ended = deferred();
  source.onended = () => ended.resolve();
  source.start();
  return { done: ended.promise, stop: () => { try { source.stop(); } catch { /* already ended */ } } };
}

// ---------------------------------------------------------------- home

function listenerName() {
  const value = (document.getElementById('listener')?.value || 'listener').trim().toLowerCase();
  return value.replace(/[^a-z0-9-]+/g, '-').replace(/^-+|-+$/g, '') || 'listener';
}

async function home(message) {
  try { manifests.localization = await getJSON('stimuli/localization.json'); } catch { manifests.localization = null; }
  try { manifests.coherence = await getJSON('stimuli/coherence.json'); } catch { manifests.coherence = null; }
  let results = [];
  try { results = await getJSON('results/'); } catch { /* none yet */ }
  const remembered = (key, fallback) => { try { return localStorage.getItem(key) ?? fallback; } catch { return fallback; } };
  const listener = h('input', { type: 'text', id: 'listener', value: remembered('xem-listener', 'listener'), autocomplete: 'off' });
  const headphones = h('input', { type: 'text', id: 'headphones', value: remembered('xem-headphones', ''), placeholder: 'make and model', autocomplete: 'off' });
  const status = (session) => {
    const name = listenerName();
    const mine = results.filter((r) => r.listener === name && r.session === session);
    if (mine.some((r) => r.complete)) return `done (${mine.filter((r) => r.complete).length}×)`;
    const partial = mine.find((r) => !r.complete);
    return partial ? `in progress: ${partial.answered} answered` : 'not started';
  };
  const rows = [];
  const sessions = [];
  if (manifests.localization) {
    for (const s of manifests.localization.sessions) sessions.push({ part: 'localization', session: s, count: s.training.length + s.blocks.flat().length });
  }
  if (manifests.coherence) {
    for (const s of manifests.coherence.sessions) sessions.push({ part: 'coherence', session: s, count: s.trials.length });
  }
  for (const entry of sessions) {
    const cell = h('td', { class: 'muted' }, status(entry.session.name));
    rows.push(h('tr', {},
      h('td', {}, entry.session.name),
      h('td', {}, entry.part === 'localization' ? 'Localization' : 'Musical coherence'),
      h('td', {}, `${entry.count} ${entry.part === 'localization' ? 'trials' : 'screens'}`),
      cell,
      h('td', {}, h('button', { class: 'primary', dataset: { session: entry.session.name }, onclick: () => start(entry) }, 'Start'))));
  }
  listener.addEventListener('input', () => {
    rows.forEach((row, i) => { row.children[3].textContent = status(sessions[i].session.name); });
  });
  const made = (manifest) => {
    const source = manifest?.generated?.source;
    return source?.commit ? `${source.commit.slice(0, 10)}${source.dirty ? ' (with uncommitted changes)' : ''}, seed ${manifest.generated.seed}` : 'unknown';
  };
  const provenance = [manifests.localization && `localization from ${made(manifests.localization)}`, manifests.coherence && `music from ${made(manifests.coherence)}`].filter(Boolean);
  const missing = [];
  if (!manifests.localization) missing.push('localization.json (cargo run --release -p xem-headless --example listening -- localization)');
  if (!manifests.coherence) missing.push('coherence.json (cargo run --release -p xem-headless --example listening -- music)');
  show(
    message ? h('div', { class: 'panel' }, h('p', { class: 'ok' }, message)) : null,
    h('div', { class: 'panel' },
      h('h2', {}, 'Before you start'),
      h('p', {}, 'Use headphones, in a quiet room. Each session starts with three checks: the level, which ear is which, and the headphones\' polarity. Answers are saved after each one; a session can be left and resumed.'),
      h('p', { class: 'muted' }, 'Localization: you hear short noises from many directions and mark where each came from. Musical coherence: you rate versions of game music against the original. Nothing here is graded while you listen.'),
      h('div', { class: 'row' },
        h('label', { class: 'field' }, 'Listener', listener),
        h('label', { class: 'field' }, 'Headphones', headphones))),
    h('div', { class: 'panel' },
      h('h2', {}, 'Sessions'),
      sessions.length ? h('table', {}, h('tbody', {}, rows)) : h('p', { class: 'error' }, 'No stimuli were found.'),
      provenance.length ? h('p', { class: 'muted' }, `Stimuli: ${provenance.join('; ')}.`) : null,
      missing.length ? h('p', { class: 'note' }, `Missing: ${missing.join('; ')}.`) : null));
}

async function start(entry) {
  await context();
  const name = listenerName();
  try {
    localStorage.setItem('xem-listener', document.getElementById('listener').value);
    localStorage.setItem('xem-headphones', document.getElementById('headphones').value);
  } catch { /* private window */ }
  const headphones = document.getElementById('headphones').value.trim();
  const manifest = manifests[entry.part];
  let results = [];
  try { results = await getJSON('results/'); } catch { /* none yet */ }
  const mine = results.filter((r) => r.listener === name && r.session === entry.session.name);
  // An unfinished session resumes only on the stimuli it was answering;
  // after the stimuli were made again it starts afresh in a file of its own.
  const current = JSON.stringify(manifest.generated);
  let resumed = null;
  for (const partial of mine.filter((r) => !r.complete)) {
    const saved = await getJSON(`results/${encodeURIComponent(partial.file)}`);
    if (JSON.stringify(saved.manifest?.generated) === current) {
      resumed = saved;
      break;
    }
  }
  if (resumed) {
    run = resumed;
  } else {
    const taken = mine.length;
    run = {
      kind: 'xem-listening-results', version: VERSION, part: entry.part, session: entry.session.name,
      file: `${name}-${entry.session.name}${taken ? `-r${taken + 1}` : ''}.json`,
      listener: name, headphones, started: new Date().toISOString(), complete: false,
      manifest: { kind: manifest.kind, version: manifest.version, generated: manifest.generated },
      page: { version: VERSION, user_agent: navigator.userAgent, sample_rate: audio.sampleRate, base_latency: audio.baseLatency ?? null, output_latency: audio.outputLatency ?? null },
      calibrations: [], trials: [], ratings: [],
    };
  }
  if (headphones && !run.headphones) run.headphones = headphones;
  const check = await calibrate(manifests.localization);
  run.calibrations.push(check);
  await save();
  if (entry.part === 'localization') await localization(entry.session);
  else await coherence(entry.session);
  run.complete = true;
  run.finished = new Date().toISOString();
  await save();
  home(`Session ${entry.session.name} is done and saved (results/${run.file}).`);
}

// ---------------------------------------------------------------- calibration

/** The level, left/right and polarity checks; what was answered. */
async function calibrate(localization) {
  const files = localization?.calibration;
  if (!files) return { skipped: 'no calibration sounds (localization.json missing)' };
  const record = { at: new Date().toISOString(), level_set: false, left_right: [], polarity: [] };
  const random = generator(`${run.file}-${run.calibrations.length}`);

  // The level.
  {
    const pink = await buffer(files.pink);
    let playing = null;
    const done = deferred();
    const toggle = h('button', { id: 'level-play', onclick: () => {
      if (playing) { playing.stop(); playing = null; toggle.textContent = 'Play the noise'; }
      else { playing = play(pink, 0, true); toggle.textContent = 'Stop'; }
    } }, 'Play the noise');
    const set = h('button', { id: 'level-done', class: 'primary', onclick: () => { playing?.stop(); done.resolve(); } }, 'The level is set');
    show(h('div', { class: 'panel', dataset: { step: 'level' } },
      h('h2', {}, 'Check 1 of 3: the level'),
      h('p', {}, 'Play the noise and set your computer\'s volume so that it is comfortably loud: about as loud as a conversation, never uncomfortable. Then leave the volume alone for the whole session.'),
      h('div', { class: 'row' }, toggle, set)));
    await done.promise;
    record.level_set = true;
  }

  // Left and right: each ear once, in a random order, until both are right.
  for (let attempt = 0; attempt < 3; attempt++) {
    const order = random() < 0.5 ? ['left', 'right'] : ['right', 'left'];
    const answers = [];
    for (let i = 0; i < 2; i++) {
      const sound = await buffer(files[order[i]]);
      const done = deferred();
      const answer = (side) => () => done.resolve(side);
      const left = h('button', { id: 'lr-left', disabled: true, onclick: answer('left') }, 'Left ear');
      const right = h('button', { id: 'lr-right', disabled: true, onclick: answer('right') }, 'Right ear');
      const playButton = h('button', { id: 'lr-play', class: 'primary', onclick: async () => {
        playButton.disabled = true;
        await play(sound).done;
        left.disabled = false; right.disabled = false; playButton.disabled = false;
      } }, i === 0 ? 'Play the first sound' : 'Play the second sound');
      show(h('div', { class: 'panel', dataset: { check: order[i], step: `left-right-${attempt}-${i}` } },
        h('h2', {}, 'Check 2 of 3: left and right'),
        h('p', {}, `Sound ${i + 1} of 2 is in one ear only. Which ear?`),
        attempt ? h('p', { class: 'note' }, 'Last time an ear was mistaken: check that the headphones\' left side is on your left ear.') : null,
        h('div', { class: 'row' }, playButton, left, right)));
      const side = await done.promise;
      answers.push({ played: order[i], answer: side });
    }
    record.left_right.push(...answers);
    if (answers.every((a) => a.played === a.answer)) break;
  }
  record.left_right_ok = record.left_right.slice(-2).every((a) => a.played === a.answer);

  // Polarity: the same low noise in phase and out of phase in the two ears.
  {
    const inPhase = random() < 0.5 ? 'a' : 'b';
    const sounds = { a: await buffer(inPhase === 'a' ? files.in_phase : files.anti_phase), b: await buffer(inPhase === 'b' ? files.in_phase : files.anti_phase) };
    const done = deferred();
    const heard = new Set();
    const choose = { a: h('button', { id: 'pol-answer-a', disabled: true, onclick: () => done.resolve('a') }, 'A is centred'), b: h('button', { id: 'pol-answer-b', disabled: true, onclick: () => done.resolve('b') }, 'B is centred') };
    const player = (key) => {
      const button = h('button', { id: `pol-${key}`, class: 'primary', onclick: async () => {
        button.disabled = true;
        await play(sounds[key]).done;
        button.disabled = false;
        heard.add(key);
        if (heard.size === 2) { choose.a.disabled = false; choose.b.disabled = false; }
      } }, `Play ${key.toUpperCase()}`);
      return button;
    };
    show(h('div', { class: 'panel', dataset: { check: inPhase, step: 'polarity' } },
      h('h2', {}, 'Check 3 of 3: polarity'),
      h('p', {}, 'A and B are the same low noise. One sounds centred, inside your head; the other wide or strange. Which is centred?'),
      h('div', { class: 'row' }, player('a'), player('b'), choose.a, choose.b)));
    const answer = await done.promise;
    record.polarity.push({ in_phase: inPhase, answer });
    record.polarity_ok = answer === inPhase;
    if (!record.polarity_ok) {
      const go = deferred();
      show(h('div', { class: 'panel' },
        h('h2', {}, 'Polarity'),
        h('p', { class: 'note' }, 'That was the out-of-phase one. If the headphones (or a cable or adapter) invert one side, directions will be hard to hear; the session can still go on, and the analysis will note it.'),
        h('button', { id: 'pol-continue', class: 'primary', onclick: () => go.resolve() }, 'Continue')));
      await go.promise;
    }
  }
  return record;
}

// ---------------------------------------------------------------- the dial

/** The azimuth dial (seen from above, ahead at the top) and the elevation
 * slider; with `moving`, a start and an end are marked. */
function answerControls(moving, onChange) {
  const size = 360, c = 180, r = 140;
  const marks = moving ? { start: { azimuth: null, elevation: 0 }, end: { azimuth: null, elevation: 0 } } : { end: { azimuth: null, elevation: 0 } };
  let selected = moving ? 'start' : 'end';
  const point = (azimuth, radius = r) => [c + radius * Math.sin(azimuth * Math.PI / 180), c - radius * Math.cos(azimuth * Math.PI / 180)];
  const face = svg('svg', { viewBox: `0 0 ${size} ${size}`, role: 'img', 'aria-label': 'Azimuth dial: ahead at the top, seen from above' });
  face.append(svg('circle', { class: 'face', cx: c, cy: c, r }));
  for (let a = 0; a < 360; a += 30) {
    const [x1, y1] = point(a, r - (a % 90 ? 6 : 12));
    const [x2, y2] = point(a, r);
    face.append(svg('line', { class: 'tick', x1, y1, x2, y2 }));
  }
  for (const [a, name] of [[0, 'Ahead'], [90, 'Right'], [180, 'Behind'], [270, 'Left']]) {
    const [x, y] = point(a, r + 22);
    const label = svg('text', { class: 'label', x, y });
    label.textContent = name;
    face.append(label);
  }
  // The head: nose up, ears at the sides.
  face.append(svg('ellipse', { class: 'head', cx: c - 26, cy: c, rx: 5, ry: 10 }));
  face.append(svg('ellipse', { class: 'head', cx: c + 26, cy: c, rx: 5, ry: 10 }));
  face.append(svg('path', { class: 'head', d: `M ${c - 8} ${c - 22} L ${c} ${c - 34} L ${c + 8} ${c - 22} Z` }));
  face.append(svg('circle', { class: 'head', cx: c, cy: c, r: 24 }));
  const layer = svg('g');
  const truth = svg('g');
  face.append(truth, layer);
  const dial = h('div', { class: 'dial', id: 'dial', tabindex: 0, role: 'slider', 'aria-label': 'Azimuth', 'aria-valuemin': 0, 'aria-valuemax': 359 });
  dial.append(face);
  const slider = h('input', { type: 'range', id: 'elevation', min: -90, max: 90, step: 1, value: 0, 'aria-label': 'Elevation' });
  const reading = h('div', { class: 'value', id: 'reading' });
  const markerButtons = {};
  const draw = () => {
    layer.replaceChildren();
    for (const [name, mark] of Object.entries(marks)) {
      if (mark.azimuth === null) continue;
      const [x, y] = point(mark.azimuth, r - 10);
      if (name === 'start') layer.append(svg('circle', { class: 'start-mark', cx: x, cy: y, r: 9 }));
      else {
        const [lx, ly] = point(mark.azimuth, 30);
        layer.append(svg('line', { class: 'answer-line', x1: lx, y1: ly, x2: x, y2: y }), svg('circle', { class: 'answer-mark', cx: x, cy: y, r: 8 }));
      }
    }
    const mark = marks[selected];
    slider.value = mark.elevation;
    dial.setAttribute('aria-valuenow', mark.azimuth ?? 0);
    reading.textContent = Object.entries(marks).map(([name, m]) => `${moving ? (name === 'start' ? 'Start: ' : 'End: ') : ''}${m.azimuth === null ? 'click the dial' : direction(m.azimuth, m.elevation)}`).join(' · ');
    for (const [name, button] of Object.entries(markerButtons)) button.classList.toggle('selected', name === selected);
    onChange?.();
  };
  const fromEvent = (event) => {
    const box = face.getBoundingClientRect();
    const x = (event.clientX - box.left) * size / box.width - c;
    const y = c - (event.clientY - box.top) * size / box.height;
    return (Math.round(Math.atan2(x, y) * 180 / Math.PI) + 360) % 360;
  };
  // A press sets the marker; dragging moves it (the dial keeps the pointer
  // until it is released, wherever that is).
  let dragging = false;
  face.addEventListener('pointerdown', (event) => {
    dragging = true;
    face.setPointerCapture(event.pointerId);
    marks[selected].azimuth = fromEvent(event);
    draw();
  });
  face.addEventListener('pointermove', (event) => { if (dragging) { marks[selected].azimuth = fromEvent(event); draw(); } });
  const release = () => { dragging = false; };
  face.addEventListener('pointerup', release);
  face.addEventListener('pointercancel', release);
  dial.addEventListener('keydown', (event) => {
    const step = event.shiftKey ? 1 : 5;
    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
      const now = marks[selected].azimuth ?? 0;
      marks[selected].azimuth = (now + (event.key === 'ArrowRight' ? step : -step) + 360) % 360;
      draw();
      event.preventDefault();
    }
  });
  slider.addEventListener('input', () => { marks[selected].elevation = Number(slider.value); draw(); });
  const elevation = h('div', { class: 'elevation' }, slider, h('div', { class: 'scale-labels' }, h('span', {}, 'Above (90°)'), h('span', {}, 'Level (0°)'), h('span', {}, 'Below (−90°)')));
  const markers = moving ? h('div', { class: 'markers' },
    markerButtons.start = h('button', { id: 'mark-start', onclick: () => { selected = 'start'; draw(); } }, 'Where it started'),
    markerButtons.end = h('button', { id: 'mark-end', onclick: () => { selected = 'end'; draw(); } }, 'Where it ended')) : null;
  draw();
  return {
    element: h('div', {}, h('div', { class: 'answer' }, h('div', { style: 'flex: 1 1 280px; max-width: 360px' }, dial, markers), elevation), reading),
    complete: () => Object.values(marks).every((m) => m.azimuth !== null),
    value: () => (moving ? { start: { ...marks.start }, end: { ...marks.end } } : { ...marks.end }),
    showTruth: (points) => {
      truth.replaceChildren();
      for (const azimuth of points) {
        const [x, y] = point(azimuth, r - 10);
        truth.append(svg('circle', { class: 'truth-mark', cx: x, cy: y, r: 13 }));
      }
    },
    disable: () => { face.style.pointerEvents = 'none'; slider.disabled = true; },
  };
}

// ---------------------------------------------------------------- localization

async function localization(session) {
  const stimuli = new Map(manifests.localization.stimuli.map((s) => [s.id, s]));
  const plan = [
    ...session.training.map((t) => ({ ...t, training: true, block: 'training' })),
    ...session.blocks.flatMap((block, b) => block.map((t) => ({ ...t, block: b + 1 }))),
  ];
  const answered = new Set(run.trials.map((t) => t.trial));
  const blocks = session.blocks.length;
  let previous = run.trials.length ? run.trials[run.trials.length - 1].block : null;
  for (let i = 0; i < plan.length; i++) {
    const trial = plan[i];
    if (answered.has(trial.trial)) continue;
    if (trial.block !== previous) {
      const go = deferred();
      const intro = trial.block === 'training'
        ? [h('h2', {}, `Session ${session.name}: training`), h('p', {}, `${session.training.length} practice sounds. After each answer the dial shows where the sound really was (dashed ring); you may play it again.`)]
        : [h('h2', {}, `Session ${session.name}: block ${trial.block} of ${blocks}`),
          h('p', {}, previous === 'training' || previous === null ? 'The test trials: each sound plays once, and nothing is shown after your answer. Some sounds move: mark where they started and where they ended.' : 'Take a break if you like.')];
      show(h('div', { class: 'panel' }, ...intro,
        h('p', { class: 'muted' }, 'Keep your head still and facing the screen. Click the dial where the sound came from (ahead is at the top; the dial is seen from above), set its height with the slider, then answer. Keys: arrows turn the dial, Enter answers.'),
        h('button', { id: 'block-start', class: 'primary', onclick: () => go.resolve() }, 'Begin')));
      await go.promise;
      previous = trial.block;
    }
    const stimulus = stimuli.get(trial.stimulus);
    if (!stimulus) throw new Error(`no stimulus ${trial.stimulus}`);
    const position = plan.slice(0, i + 1).filter((t) => t.block === trial.block).length;
    const total = plan.filter((t) => t.block === trial.block).length;
    run.trials.push(await localizationTrial(trial, stimulus, position, total));
    await save();
  }
}

async function localizationTrial(trial, stimulus, position, total) {
  const moving = stimulus.motion === 'moving';
  const sound = await buffer(stimulus.file);
  let ended = false;
  let replays = 0;
  const answer = h('button', { id: 'answer', class: 'primary', disabled: true }, 'Answer');
  const controls = answerControls(moving, () => { answer.disabled = !(ended && controls.complete()); });
  const replay = trial.training ? h('button', { id: 'replay', disabled: true }, 'Play again') : null;
  const title = trial.training ? `Training ${position} of ${total}` : `Block ${trial.block}: trial ${position} of ${total}`;
  const panel = h('div', { class: 'panel', dataset: { trial: trial.trial, motion: stimulus.motion } },
    h('h2', {}, title),
    h('div', { class: 'progress' }, h('div', { style: `width: ${(100 * (position - 1)) / total}%` })),
    h('p', {}, moving ? 'The sound moves. Mark where it started and where it ended (choose a marker, then click the dial and set the height).' : 'Where did the sound come from?'),
    h('p', { class: 'muted', id: 'playing' }, 'Listen…'),
    controls.element,
    h('div', { class: 'row', style: 'margin-top: 12px' }, replay, answer));
  show(panel);
  const presented = new Date().toISOString();
  // A moment to settle before the sound.
  await sleep(600);
  await play(sound, trial.rove_db).done;
  ended = true;
  const endedAt = performance.now();
  document.getElementById('playing').textContent = 'Your answer:';
  answer.disabled = !controls.complete();
  if (replay) {
    replay.disabled = false;
    replay.addEventListener('click', async () => { replay.disabled = true; replays++; await play(sound, trial.rove_db).done; replay.disabled = false; });
  }
  const done = deferred();
  answer.addEventListener('click', () => done.resolve());
  const onKey = (event) => { if (event.key === 'Enter' && !answer.disabled) done.resolve(); };
  window.addEventListener('keydown', onKey);
  await done.promise;
  window.removeEventListener('keydown', onKey);
  const record = {
    trial: trial.trial, stimulus: trial.stimulus, block: trial.block, training: !!trial.training, catch: !!trial.catch,
    rove_db: trial.rove_db, presented, answer_ms: Math.round(performance.now() - endedAt), replays, response: controls.value(),
  };
  if (trial.training) {
    controls.disable();
    controls.showTruth(moving ? [stimulus.start[0], stimulus.end[0]] : [stimulus.start[0]]);
    answer.remove();
    const go = deferred();
    const truthText = moving ? `It moved from ${direction(...stimulus.start)} to ${direction(...stimulus.end)}.` : `It was ${direction(...stimulus.start)}.`;
    panel.append(h('p', { class: 'ok', id: 'feedback' }, truthText), h('button', { id: 'next', class: 'primary', onclick: () => go.resolve() }, 'Next'));
    const onNext = (event) => { if (event.key === 'Enter') go.resolve(); };
    window.addEventListener('keydown', onNext);
    await go.promise;
    window.removeEventListener('keydown', onNext);
  }
  return record;
}

// ---------------------------------------------------------------- coherence

async function coherence(session) {
  const excerpts = new Map(manifests.coherence.excerpts.map((e) => [e.id, e]));
  const attributes = new Map(manifests.coherence.attributes.map((a) => [a.id, a]));
  const answered = new Set(run.ratings.map((r) => r.trial));
  if (!run.ratings.length) {
    const go = deferred();
    show(h('div', { class: 'panel' },
      h('h2', {}, `Session ${session.name}: musical coherence`),
      h('p', {}, `${session.trials.length} screens. Each has a reference, the game's original Stereo mix, and six versions A to F of the same music. One of them is the reference itself, hidden; one is deliberately broken apart. Rate each version on the question shown, from 0 to 100.`),
      h('p', {}, 'Press a version\'s button to hear it; switch freely while the music plays, the versions stay in step. Play every version and move every slider before going on. The levels are matched.'),
      h('button', { id: 'coherence-start', class: 'primary', onclick: () => go.resolve() }, 'Begin')));
    await go.promise;
  }
  for (let i = 0; i < session.trials.length; i++) {
    const trial = session.trials[i];
    if (answered.has(trial.trial)) continue;
    const excerpt = excerpts.get(trial.excerpt);
    const attribute = attributes.get(trial.attribute);
    if (!excerpt || !attribute) throw new Error(`no excerpt ${trial.excerpt} or attribute ${trial.attribute}`);
    run.ratings.push(await mushra(trial, excerpt, attribute, i + 1, session.trials.length));
    await save();
  }
}

async function mushra(trial, excerpt, attribute, position, total) {
  const letters = 'ABCDEFGHIJ';
  const names = ['reference', ...trial.order];
  const buffers = await Promise.all(names.map((name) => buffer(excerpt.conditions[name].file)));
  const gains = names.map((name) => 10 ** (excerpt.conditions[name].gain_db / 20));
  let nodes = null;
  let current = -1;
  const played = names.map(() => 0);
  const moved = trial.order.map(() => false);
  const started = performance.now();
  const next = h('button', { id: 'next', class: 'primary', disabled: true }, position === total ? 'Finish' : 'Next');
  const check = () => { next.disabled = !(played.slice(1).every((n) => n > 0) && moved.every(Boolean)); };
  const buttons = [];
  const select = (index) => {
    if (!nodes) {
      const at = audio.currentTime + 0.05;
      nodes = buffers.map((buf) => {
        const source = audio.createBufferSource();
        source.buffer = buf;
        source.loop = true;
        const gain = audio.createGain();
        gain.gain.value = 0;
        source.connect(gain).connect(audio.destination);
        source.start(at);
        return { source, gain };
      });
    }
    const now = audio.currentTime;
    nodes.forEach((node, i) => {
      node.gain.gain.cancelScheduledValues(now);
      node.gain.gain.setValueAtTime(node.gain.gain.value, now);
      node.gain.gain.linearRampToValueAtTime(i === index ? gains[i] : 0, now + 0.02);
    });
    current = index;
    played[index]++;
    buttons.forEach((b, i) => b.classList.toggle('playing', i === index));
    check();
  };
  const stop = () => {
    if (nodes) for (const node of nodes) { try { node.source.stop(); } catch { /* ended */ } }
    nodes = null;
    current = -1;
    buttons.forEach((b) => b.classList.remove('playing'));
  };
  buttons.push(h('button', { id: 'play-reference', onclick: () => select(0) }, 'Reference'));
  const sliders = trial.order.map((name, i) => {
    const value = h('div', { class: 'value', id: `value-${letters[i]}` }, '–');
    const flag = h('div', { class: 'unrated' }, 'not rated');
    const slider = h('input', { type: 'range', id: `rate-${letters[i]}`, min: 0, max: 100, step: 1, value: 50, 'aria-label': `Rating of ${letters[i]}` });
    slider.addEventListener('input', () => { moved[i] = true; value.textContent = slider.value; flag.textContent = ''; check(); });
    const button = h('button', { id: `play-${letters[i]}`, onclick: () => select(i + 1) }, letters[i]);
    buttons.push(button);
    return { slider, column: h('div', { class: 'column' }, value, slider, button, flag) };
  });
  const grades = h('div', { class: 'column' }, h('div', { class: 'value' }, '\u00a0'),
    h('div', { class: 'grades' }, h('span', {}, `100: ${attribute.high}`), h('span', {}, '50'), h('span', {}, `0: ${attribute.low}`)));
  const panel = h('div', { class: 'panel', dataset: { trial: trial.trial, excerpt: excerpt.id, attribute: attribute.id } },
    h('h2', {}, `${attribute.label}: ${position} of ${total}`),
    h('div', { class: 'progress' }, h('div', { style: `width: ${(100 * (position - 1)) / total}%` })),
    h('p', {}, attribute.question),
    h('p', { class: 'muted' }, `Music: ${excerpt.label}. ${attribute.reference_top ? 'The reference defines the top of this scale.' : 'Rate by your own impression; the reference need not be at the top.'}`),
    h('div', { class: 'row' }, buttons[0], h('button', { id: 'stop', onclick: stop }, 'Stop')),
    h('div', { class: 'mushra', style: 'margin-top: 12px' }, grades, ...sliders.map((s) => s.column)),
    h('div', { class: 'row', style: 'margin-top: 12px' }, next));
  show(panel);
  const done = deferred();
  next.addEventListener('click', () => done.resolve());
  await done.promise;
  stop();
  const ratings = {};
  const plays = { reference_labelled: played[0] };
  trial.order.forEach((name, i) => { ratings[name] = Number(sliders[i].slider.value); plays[name] = played[i + 1]; });
  return { trial: trial.trial, excerpt: excerpt.id, attribute: attribute.id, order: trial.order, ratings, plays, seconds: Math.round((performance.now() - started) / 100) / 10 };
}

home().catch((error) => show(h('div', { class: 'panel' }, h('p', { class: 'error' }, String(error)))));
