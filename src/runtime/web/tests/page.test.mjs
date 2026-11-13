// The browser host in headless Chromium: rendering on both backends, the Slint
// settings panel, settings persistence, disc import and, over a game module
// fixture, the automation API, the game's sound, input, memory cards in
// IndexedDB and snapshot saves. Needs no private inputs.

import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { after, before, describe, test } from 'node:test';
import { runInNewContext } from 'node:vm';
import { NO_WEBGPU, WEBGPU, decodePng, pixelStats, settingsSection, shot, start, status, syntheticDisc } from './support.mjs';

async function canvasShot(page, name) {
  const png = await page.locator('canvas').screenshot({ path: shot(name) });
  return pixelStats(png);
}

test('audio worklet keeps consecutive samples through animation scheduling jitter', () => {
  let Output;
  runInNewContext(readFileSync(new URL('../audio-worklet.js', import.meta.url), 'utf8'), {
    AudioWorkletProcessor: class { constructor() { this.port = { postMessage() {} }; } },
    registerProcessor(_name, processor) { Output = processor; },
  });
  const output = new Output();
  const sample = (at) => Math.fround(Math.sin(at * 0.07) * 0.5);
  let produced = 0;
  const push = (count) => {
    const samples = new Float32Array(count * 2);
    for (let i = 0; i < count; i++) {
      samples[i * 2] = sample(produced + i);
      samples[i * 2 + 1] = -samples[i * 2];
    }
    produced += count;
    output.push(samples);
  };
  const process = () => {
    const left = new Float32Array(128), right = new Float32Array(128);
    output.process([], [[left, right]]);
    return [left, right];
  };
  push(735);
  assert.ok(process()[0].every((value) => value === 0));
  assert.equal(output.buffered,735,'one animation frame is retained until playback has a cushion');
  push(1470);
  let consumed = 0;
  // Audio runs every 128 samples. A busy page alternates between 11.6 and
  // 23.2 ms batches; it supplies exactly elapsed audio time, as FF does.
  let nextPush = 512, lastPush = 0, interval = 0;
  const underruns = output.underruns;
  for (let elapsed = 0; elapsed < 44100; elapsed += 128) {
    if (elapsed >= nextPush) {
      push(elapsed - lastPush);
      lastPush = elapsed;
      nextPush += ++interval % 2 ? 1024 : 512;
    }
    const [left, right] = process();
    for (let i = 0; i < left.length; i++, consumed++) {
      assert.equal(left[i],sample(consumed));
      assert.equal(right[i],-sample(consumed));
    }
  }
  assert.equal(output.underruns,underruns,'jitter must not insert silence into the stream');
  assert.equal(output.overflows,0);
  output.port.onmessage({ data: { reset: true } });
  push(735);
  assert.ok(process()[0].every((value) => value === 0));
  assert.equal(output.buffered,735,'a restored timeline primes from its new audio');
});

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

        // The Slint panel: shown by Escape (as a player would) and by automation.
        assert.equal(renderer.panel, 'slint');
        await page.keyboard.press('Escape');
        assert.equal(await page.evaluate(() => window.xem.panel.visible()), true);
        await page.waitForTimeout(300);
        const withPanel = await canvasShot(page, `panel-${expected}.png`);
        assert.ok(withPanel.colours > 200);
        await page.keyboard.press('Escape');
        assert.equal(await page.evaluate(() => window.xem.panel.visible()), false);

        // Show FPS must draw a counter on the canvas even with Settings closed.
        const box=await page.locator('canvas').boundingBox();
        const counter=()=>page.screenshot({clip:{x:box.x+8,y:box.y+8,width:96,height:28}});
        assert.equal(pixelStats(await counter()).lit,0,'the corner starts black');
        await page.evaluate(()=>window.xem.settings.apply({ShowFps:true}));
        await page.waitForFunction(()=>/^\d+ FPS$/.test(window.xem.status().renderer.fps));
        assert.ok(pixelStats(await counter()).lit>0.1,'the FPS counter is visible');
        await page.evaluate(()=>window.xem.settings.apply({ShowFps:false}));
        await page.waitForTimeout(100);
        assert.equal(pixelStats(await counter()).lit,0,'disabling Show FPS removes the counter');
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
    const panel=await settingsSection(page,1);
    const before = (await page.evaluate(() => window.xem.settings.get())).settings.presentation.show_fps;
    // The Show FPS switch, at the end of the Picture card's last row.
    await page.mouse.click(panel.left+586,panel.top+307);
    await page.waitForTimeout(200);
    await page.locator('canvas').screenshot({ path: shot('panel-click.png') });
    const after = (await page.evaluate(() => window.xem.settings.get())).settings.presentation.show_fps;
    assert.equal(after, !before);
    await page.close();
  });

  test('audio starts from a user gesture at the SPU rate', async () => {
    const page = await browser.page();
    assert.equal((await status(page)).page.audio.state, 'not started');
    await page.click('#audio');
    await page.waitForFunction(() => window.xem.status().page.audio.state === 'running');
    const { audio, page: host } = await status(page);
    assert.equal(host.audio.sampleRate, 44100);
    assert.equal(audio.sampleRate, 44100);
    // Without a session nothing is produced: the worklet plays silence.
    assert.equal(audio.frames, 0);
    await page.close();
  });

  test('saving needs a session', async () => {
    const page = await browser.page();
    await assert.rejects(page.evaluate(() => window.xem.save('slot-test')), /no game session/);
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

  const STOP = 'host stopped the game: called FixtureStub, which the port does not define';

  test('control commands: frames, run_until, memory, snapshots', async () => {
    const result = await page.evaluate(async () => {
      const xem = window.xem;
      // This baseline compares the complete command replies before/after a
      // restore. Rewind history is deliberately reset when loading a state;
      // held rewind and its retained allocations have their own tests.
      xem.settings.apply({ RewindEnabled: false });
      await xem.boot({ executable: false });
      const first = xem.step(1);
      const until = xem.runUntil({ maxFrames: 10, until: { frames: 3 } });
      const word = xem.command({ cmd: 'read_memory', address: 0x80000000, length: 4 });
      const snapshot = xem.snapshot('three');
      const halt = xem.step(1000);
      const end = { status: xem.command({ cmd: 'status' }), hash: xem.memoryHash() };
      const restored = xem.restore('three');
      const again = xem.step(1000);
      const end2 = { status: xem.command({ cmd: 'status' }), hash: xem.memoryHash() };
      const tooFar = xem.runUntil({ maxFrames: 1e9, until: { vblanks: 1e9 } });
      const unbounded = xem.command({ cmd: 'run_until', until: { vblanks: 1e9 } });
      return { first, until, word, snapshot, halt, end, restored, again, end2, tooFar, unbounded };
    });
    assert.deepEqual([result.first.frames, result.first.stopped], [1, null]);
    assert.deepEqual([result.until.met, result.until.frames, result.until.status.frames], [true, 2, 3]);
    assert.equal(result.word, '02000000');
    assert.equal(result.snapshot.name, 'three');
    assert.deepEqual(result.halt, { error: STOP });
    // Three frame waits, a hundred polls (one per frame), then the stop.
    assert.deepEqual([result.end.status.vblanks, result.end.status.stopped], [103, STOP]);
    assert.deepEqual([result.restored.frames, result.restored.stopped], [3, null]);
    assert.deepEqual(result.again, result.halt);
    assert.deepEqual(result.end2, result.end);
    assert.match(result.tooFar.error, /at most 10000 frames/);
    assert.match(result.unbounded.error, /needs max_frames/);
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
      await waitFor(() => status().session.vblanks >= 10);
      // A one-second stall of the page (as a throttled or frozen tab sees).
      const stalled = status().session.vblanks;
      const until = performance.now() + 1000;
      while (performance.now() < until);
      await new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)));
      const afterStall = status().session.vblanks - stalled;
      await waitFor(() => status().session.stopped);
      const s = status();
      return { ...s.loop, before, afterStall, running: s.running, session: s.session };
    });
    assert.ok(loop.maxBatch <= loop.maxFramesPerAnimation, `batch of ${loop.maxBatch}`);
    assert.ok(loop.afterStall <= 2 * loop.maxFramesPerAnimation, `${loop.afterStall} frames after the stall`);
    assert.ok(loop.dropped - loop.before.dropped >= 50, `${loop.dropped - loop.before.dropped} dropped`);
    assert.equal(loop.gameFrames - loop.before.gameFrames, 104);
    assert.equal(loop.running, false);
    assert.deepEqual([loop.session.vblanks, loop.session.stopped], [103, STOP]);
  });

  test('the game\'s sound reaches the AudioWorklet', async () => {
    await page.click('#audio');
    const audio = await page.evaluate(async () => {
      await window.xem.boot({ executable: false, run: true });
      await new Promise((resolve) => {
        const check = () => (window.xem.status().session.stopped ? resolve() : setTimeout(check, 10));
        check();
      });
      await new Promise((resolve) => setTimeout(resolve, 300));
      const s = window.xem.status();
      return { runtime: s.audio, page: s.page.audio, vblanks: s.session.vblanks };
    });
    // 735 samples per frame at 44.1 kHz and 60 frames a second.
    assert.ok(audio.runtime.frames > 735 * 60, JSON.stringify(audio));
    assert.ok(audio.page.posted > 735 * 60, JSON.stringify(audio));
    assert.ok(audio.page.played > 4800, JSON.stringify(audio));
  });

  test('the card device\'s writes reach IndexedDB and survive a reload', async () => {
    const hex = (bytes) => Array.from(bytes, (b) => b.toString(16).padStart(2, '0')).join('');
    const text = (bytes, from, length) => String.fromCharCode(...bytes.subarray(from, from + length)).replace(/\0.*$/, '');
    const saved = await page.evaluate(async () => {
      const xem = window.xem;
      await xem.boot({ executable: false });
      // The fixture saves a file on card 1 through the BIOS file calls.
      xem.command({ cmd: 'write_memory', address: 0x80000100, hex: '01000000' });
      await xem.runFrames(2);
      const result = xem.command({ cmd: 'read_memory', address: 0x80000104, length: 8 });
      const image = xem.cards.image(0);
      await xem.cards.flush();
      const s = xem.status();
      return { result, image, stored: await xem.cards.stored(0), other: await xem.cards.stored(1),
               unsaved: s.cards.unsaved, errors: s.cards.errors, page: s.page.cards };
    });
    // Descriptor 2, 128 bytes written.
    assert.equal(saved.result, '0200000080000000');
    const image = Uint8Array.from(Object.values(saved.image));
    const stored = Uint8Array.from(Object.values(saved.stored));
    assert.equal(image.length, 128 * 1024);
    assert.equal(hex(stored), hex(image), 'IndexedDB holds the device\'s image');
    assert.equal(text(image, 0, 2), 'MC');
    // Directory frame 1: a file's first block named XEMTEST; its data in block 1.
    assert.equal(image[0x80], 0x51);
    assert.equal(text(image, 0x8a, 20), 'XEMTEST');
    assert.equal(text(image, 0x2000, 128), 'Saved by the browser host\'s card test.');
    assert.equal(saved.other, undefined, 'card 2 was never written');
    assert.deepEqual([saved.unsaved, saved.errors, saved.page.pending, saved.page.error], [false, [], 0, null]);

    await page.reload();
    await page.waitForFunction(() => document.body.dataset.ready === 'true');
    const reloaded = await page.evaluate(async () => {
      const xem = window.xem;
      const stored = await xem.cards.stored(0);
      await xem.boot({ executable: false });
      const image = xem.cards.image(0);
      // Creating the file again fails: the card the device loaded has it.
      xem.command({ cmd: 'write_memory', address: 0x80000100, hex: '01000000' });
      await xem.runFrames(1);
      return { stored, image, result: xem.command({ cmd: 'read_memory', address: 0x80000104, length: 4 }) };
    });
    assert.equal(hex(Uint8Array.from(Object.values(reloaded.stored))), hex(image));
    assert.equal(hex(Uint8Array.from(Object.values(reloaded.image))), hex(image));
    assert.equal(reloaded.result, 'ffffffff');
  });

  test('pad 1 follows the keyboard while the panel is hidden, and automation sets pads directly', async () => {
    const pads = await page.evaluate(async () => {
      await window.xem.boot({ executable: false });
      // The fixture's card path, which then waits for VSync forever (its file
      // exists by now, so it saves nothing).
      window.xem.command({ cmd: 'write_memory', address: 0x80000100, hex: '01000000' });
      window.xem.setRunning(true);
      const frames = () => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)));
      const buttons = () => window.xem.command({ cmd: 'status' }).pads[0].buttons;
      const out = {};
      window.xem.command({ cmd: 'pad', port: 0, buttons: ['triangle'] });
      await frames();
      out.automation = buttons();
      return out;
    });
    assert.deepEqual(pads.automation, ['triangle'], 'the player\'s idle input leaves an automation pad alone');
    await page.keyboard.down('KeyE');
    await page.keyboard.down('Enter');
    await page.waitForTimeout(100);
    const held = await page.evaluate(() => window.xem.command({ cmd: 'status' }).pads[0].buttons);
    await page.keyboard.up('KeyE');
    await page.keyboard.up('Enter');
    await page.waitForTimeout(100);
    const released = await page.evaluate(() => window.xem.command({ cmd: 'status' }).pads[0].buttons);
    await page.evaluate(() => window.xem.panel.show(true));
    await page.keyboard.down('ArrowUp');
    await page.waitForTimeout(100);
    const withPanel = await page.evaluate(() => window.xem.command({ cmd: 'status' }).pads[0].buttons);
    await page.keyboard.up('ArrowUp');
    await page.evaluate(() => window.xem.panel.show(false));
    assert.deepEqual(held, ['start', 'cross']);
    assert.deepEqual(released, []);
    assert.deepEqual(withPanel, [], 'the panel has the input');
  });

  test('fingers play the deck of what the game plays: the stick, its buttons and the app\'s keys, at the opacity set', async () => {
    const touch = await browser.page('?game=/fixtures/fixture.wasm');
    try {
      await touch.evaluate(async () => {
        await window.xem.boot({ executable: false });
        // The fixture reports the field's mode; with no menu requested, no
        // window open and the player's control polled, the player walks (on
        // map 30, whose scripts test no button).
        window.xem.command({ cmd: 'write_memory', address: 0x80000110, hex: '01000000' });
        window.xem.command({ cmd: 'write_memory', address: 0x8004F34C, hex: '1e000000' });
        window.xem.command({ cmd: 'write_memory', address: 0x800ADB64, hex: 'ff' });
        window.xem.command({ cmd: 'write_memory', address: 0x800ADB68, hex: '01000000' });
        window.xem.setRunning(true);
        for (let i = 0; i < 4; i++) await new Promise(requestAnimationFrame);
      });
      assert.equal((await touch.evaluate(() => window.xem.command({ cmd: 'context' }))).kind, 'walk');
      // A finger at `at`: a deck control's name, canvas pixels, or the
      // canvas's middle.
      const finger = (type, id, at) => touch.evaluate(async ({ type, id, at }) => {
        const canvas = document.querySelector('canvas');
        const scale = window.devicePixelRatio || 1;
        const c = typeof at === 'string' ? window.xem.status().touch.controls.find((k) => k.control === at) : null;
        const [cx, cy] = c ? [c.x + c.width / 2, c.y + c.height / 2] : at ?? [canvas.width / 2, canvas.height / 2];
        const box = canvas.getBoundingClientRect();
        canvas.dispatchEvent(new PointerEvent(type, { pointerType: 'touch', pointerId: id, clientX: box.left + cx / scale, clientY: box.top + cy / scale, bubbles: true, isPrimary: id === 1 }));
        for (let i = 0; i < 3; i++) await new Promise(requestAnimationFrame);
        const s = window.xem.status();
        return { held: s.touch.held, buttons: window.xem.command({ cmd: 'status' }).pads[0].buttons.sort(), touched: s.touch.touched };
      }, { type, id, at });
      const deck = () => touch.evaluate(() => {
        const s = window.xem.status();
        return {
          labels: Object.fromEntries(s.touch.controls.map((c) => [c.control, c.label])), controls: s.touch.controls,
          stick: s.touch.stick, shown: s.touch.stickShown, width: document.querySelector('canvas').width,
        };
      });
      // The first touch shows the deck (laid out from the picture's place):
      // the walking deck, its buttons named for what they do.
      await finger('pointerdown', 9, null);
      await finger('pointerup', 9, null);
      const { labels, controls, stick, width } = await deck();
      assert.deepEqual(labels, {
        Settings: 'gear', Confirm: 'TALK', CameraLeft: 'turn left', Jump: 'JUMP', CameraRight: 'turn right',
        Menu: 'MENU', Run: 'RUN', Look: 'LOOK', Start: 'pause',
      });
      // TALK under the right thumb, the largest; the arc about it.
      const place = (name) => controls.find((c) => c.control === name);
      const [talk, jump] = [place('Confirm'), place('Jump')];
      assert.ok(talk.x > width / 2 && controls.every((c) => c.control === 'Confirm' || c.width < talk.width || c.width > c.height), JSON.stringify(controls));
      assert.ok(jump.x < talk.x && jump.y < talk.y && place('CameraRight').y < jump.y && place('CameraLeft').x < jump.x, JSON.stringify(controls));
      assert.ok(stick && stick.rests && stick.x < width / 2 && stick.reach > stick.deadZone, `the stick rests on the left: ${JSON.stringify({ stick, width })}`);
      // The stick: a finger on the screen's left half centres it where it
      // touched (here below where it rests); its travel holds the D-pad's
      // eighth.
      const [x, y] = [stick.x, stick.y + stick.reach / 2];
      assert.deepEqual(await finger('pointerdown', 1, [x, y]), { held: [], buttons: [], touched: true }, 'still, it holds nothing');
      assert.deepEqual(await touch.evaluate(() => window.xem.status().touch.stickShown.held), true);
      assert.deepEqual(await finger('pointermove', 1, [x, y - stick.reach]), { held: ['Up'], buttons: ['up'], touched: true });
      assert.deepEqual(await finger('pointerdown', 2, 'Confirm'), { held: ['Up', 'Confirm'], buttons: ['cross', 'up'], touched: true });
      assert.deepEqual(await finger('pointermove', 1, [x + stick.reach, y]), { held: ['Right', 'Confirm'], buttons: ['cross', 'right'], touched: true }, 'the finger steers');
      assert.deepEqual(await finger('pointermove', 1, [x + stick.reach * 0.7, y + stick.reach * 0.7]), { held: ['Down', 'Right', 'Confirm'], buttons: ['cross', 'down', 'right'], touched: true }, 'a diagonal');
      assert.deepEqual(await finger('pointerup', 2, 'Confirm'), { held: ['Down', 'Right'], buttons: ['down', 'right'], touched: true });
      assert.deepEqual(await finger('pointercancel', 1, [x, y]), { held: [], buttons: [], touched: true }, 'a cancelled touch lets go');
      assert.deepEqual(await touch.evaluate(() => window.xem.status().touch.stickShown), { centre: [stick.x, stick.y], knob: [stick.x, stick.y], held: false }, 'back at rest');
      // JUMP is the game's Triangle while a finger is on it.
      assert.deepEqual(await finger('pointerdown', 4, 'Jump'), { held: ['Jump'], buttons: ['triangle'], touched: true });
      assert.deepEqual(await finger('pointerup', 4, 'Jump'), { held: [], buttons: [], touched: true });
      // RUN is the app's Run by default (no button of the game's): on, the
      // stick runs (the game's Circle beside the direction).
      const runs = (on) => touch.waitForFunction((on) => window.xem.settings.get().settings.run_by_default === on, on);
      assert.deepEqual(await finger('pointerdown', 5, 'Run'), { held: ['Run'], buttons: [], touched: true });
      await finger('pointerup', 5, 'Run');
      await runs(true);
      await finger('pointerdown', 1, [x, y]);
      assert.deepEqual((await finger('pointermove', 1, [x, y - stick.reach])).buttons, ['circle', 'up']);
      await finger('pointerup', 1, [x, y - stick.reach]);
      await finger('pointerdown', 5, 'Run');
      await finger('pointerup', 5, 'Run');
      await runs(false);
      // The touch controls' opacity (Settings' slider): the deck over the
      // canvas at 100% and at 30%, a held button whole whatever the setting.
      const brightness = async (control) => {
        const c = (await deck()).controls.find((k) => k.control === control);
        const box = await touch.locator('canvas').boundingBox();
        const scale = await touch.evaluate(() => window.devicePixelRatio || 1);
        const clip = { x: box.x + c.x / scale, y: box.y + c.y / scale, width: c.width / scale, height: c.height / scale };
        const { rgba } = decodePng(await touch.screenshot({ clip }));
        let sum = 0;
        for (let i = 0; i < rgba.length; i += 4) sum += rgba[i] + rgba[i + 1] + rgba[i + 2];
        return sum / (rgba.length / 4);
      };
      const opacity = async (percent) => {
        const result = await touch.evaluate((percent) => {
          const assisted = window.xem.settings.get().settings.assisted;
          return window.xem.settings.apply({ Assisted: { ...assisted, touch_opacity: percent } });
        }, percent);
        for (let i = 0; i < 3; i++) await touch.evaluate(() => new Promise(requestAnimationFrame));
        return result;
      };
      assert.equal((await touch.evaluate(() => window.xem.settings.get().settings.assisted.touch_opacity)), 100, 'as designed by default');
      // The picture under the deck (no deck: Touch controls off).
      const assisted = await touch.evaluate(() => window.xem.settings.get().settings.assisted);
      await touch.evaluate((assisted) => window.xem.settings.apply({ Assisted: { ...assisted, touch_controls: false } }), assisted);
      for (let i = 0; i < 3; i++) await touch.evaluate(() => new Promise(requestAnimationFrame));
      const bare = await brightness('Confirm');
      await touch.evaluate((assisted) => window.xem.settings.apply({ Assisted: assisted }), assisted);
      for (let i = 0; i < 3; i++) await touch.evaluate(() => new Promise(requestAnimationFrame));
      const whole = await brightness('Confirm');
      assert.equal((await opacity(30)).ok, true);
      const faint = await brightness('Confirm');
      assert.ok(Math.abs(whole - bare) > 20, `TALK shows over the picture: ${whole} over ${bare}`);
      const share = (faint - bare) / (whole - bare);
      assert.ok(share > 0.2 && share < 0.4, `at 30% TALK is drawn about 30% as strongly: ${JSON.stringify({ bare, whole, faint, share })}`);
      await finger('pointerdown', 6, 'Confirm');
      const held = await brightness('Confirm');
      await finger('pointerup', 6, 'Confirm');
      assert.ok(Math.abs(held - bare) > 2 * Math.abs(faint - bare), `a held button shows whole: ${JSON.stringify({ bare, faint, held })}`);
      assert.match(JSON.stringify(await opacity(5)), /opacity must be 10–100 percent/, 'never invisible');
      assert.equal((await opacity(100)).ok, true);
      // The deck follows what the game plays: a dialogue's page opens (a
      // window open, none of the four taking a choice): NEXT, AUTO, HIDE
      // and LOOK; the stick no longer rests in view, yet a finger steers it.
      await touch.evaluate(async () => {
        for (let window_ = 0; window_ < 4; window_++) {
          window.xem.command({ cmd: 'write_memory', address: 0x800C2698 + window_ * 0x498 + 0x40E, hex: '0100' });
        }
        window.xem.command({ cmd: 'write_memory', address: 0x800B2174, hex: '0100' });
        for (let i = 0; i < 4; i++) await new Promise(requestAnimationFrame);
      });
      assert.deepEqual((await touch.evaluate(() => window.xem.command({ cmd: 'context' }))), { kind: 'dialogue', choice: null });
      const dialogue = await deck();
      assert.deepEqual(dialogue.labels, { Settings: 'gear', Confirm: 'NEXT', Auto: 'AUTO', Select: 'HIDE', Look: 'LOOK', Start: 'pause' });
      assert.deepEqual(dialogue.controls.find((c) => c.control === 'Confirm'), { ...talk, label: 'NEXT' }, 'Confirm where the thumb left it');
      assert.ok(!dialogue.stick.rests && dialogue.shown === null, JSON.stringify(dialogue.stick));
      await finger('pointerdown', 1, [x, y]);
      assert.deepEqual(await finger('pointermove', 1, [x, y - stick.reach]), { held: ['Up'], buttons: ['up'], touched: true });
      assert.equal((await deck()).shown.held, true, 'shown under the finger that steers it');
      await finger('pointerup', 1, [x, y - stick.reach]);
      // A map whose scripts test the pad (map 0: Circle, L1 and R1) adds
      // those buttons to its decks, by the symbols the game's prompts name.
      await touch.evaluate(async () => {
        window.xem.command({ cmd: 'write_memory', address: 0x8004F34C, hex: '00000000' });
        for (let i = 0; i < 4; i++) await new Promise(requestAnimationFrame);
      });
      const activity = await deck();
      assert.deepEqual(activity.labels, {
        Settings: 'gear', Confirm: 'NEXT', Auto: 'AUTO', Select: 'HIDE', Look: 'LOOK',
        Cancel: 'circle', CameraRight: 'R1', CameraLeft: 'L1', Start: 'pause',
      });
      assert.deepEqual(await finger('pointerdown', 7, 'Cancel'), { held: ['Cancel'], buttons: ['circle'], touched: true });
      assert.deepEqual(await finger('pointerup', 7, 'Cancel'), { held: [], buttons: [], touched: true });
      assert.ok(activity.controls.find((c) => c.control === 'CameraLeft').x < activity.width / 2, 'L1 over the stick');
      // Settings is a touch away (not only Escape).
      await finger('pointerdown', 3, 'Settings');
      assert.equal(await touch.evaluate(() => window.xem.panel.visible()), true);
      assert.deepEqual(touch.errors, []);
    } finally {
      await touch.close();
    }
  });

  test('AUTO on the touch deck in a field scene turns auto-advance on and off through the settings', async () => {
    const touch = await browser.page('?game=/fixtures/fixture.wasm');
    try {
      await touch.evaluate(async () => {
        await window.xem.boot({ executable: false });
        // The fixture reports the field's mode; with no menu requested and
        // no window open, the field's context is a scene's.
        window.xem.command({ cmd: 'write_memory', address: 0x80000110, hex: '01000000' });
        window.xem.command({ cmd: 'write_memory', address: 0x800ADB64, hex: 'ff' });
        window.xem.setRunning(true);
        for (let i = 0; i < 4; i++) await new Promise(requestAnimationFrame);
      });
      const tap = (id, control) => touch.evaluate(async ({ id, control }) => {
        const canvas = document.querySelector('canvas');
        const scale = window.devicePixelRatio || 1;
        const c = window.xem.status().touch.controls.find((k) => k.control === control);
        const box = canvas.getBoundingClientRect();
        const x = box.left + (c ? c.x + c.width / 2 : canvas.width / 2) / scale;
        const y = box.top + (c ? c.y + c.height / 2 : canvas.height / 2) / scale;
        for (const type of ['pointerdown', 'pointerup']) {
          canvas.dispatchEvent(new PointerEvent(type, { pointerType: 'touch', pointerId: id, clientX: x, clientY: y, bubbles: true, isPrimary: true }));
          for (let i = 0; i < 3; i++) await new Promise(requestAnimationFrame);
        }
      }, { id, control });
      const enabled = (on) => touch.waitForFunction((on) => window.xem.settings.get().settings.auto_advance.enabled === on, on);
      // The first touch shows the deck, with AUTO in the field's scene.
      await tap(1, null);
      const controls = await touch.evaluate(() => window.xem.status().touch.controls.map((c) => c.control));
      assert.ok(controls.includes('Auto') && controls.includes('Confirm'), JSON.stringify(controls));
      await tap(2, 'Auto');
      await enabled(true);
      await touch.waitForFunction(() => window.xem.status().session.auto_advance === 120);
      assert.deepEqual(await touch.evaluate(() => window.xem.command({ cmd: 'status' }).pads[0].buttons), [], 'no press of the game\'s');
      await tap(3, 'Auto');
      await enabled(false);
      await touch.waitForFunction(() => window.xem.status().session.auto_advance === 0);
      assert.deepEqual(touch.errors, []);
    } finally {
      await touch.close();
    }
  });

  test('gamepad sticks drive movement and camera, and Back+Start owns the settings toggle', async () => {
    const controls = await browser.page('?game=/fixtures/fixture.wasm');
    try {
      await controls.evaluate(async () => {
        await window.xem.boot({ executable: false });
        window.xem.command({ cmd: 'write_memory', address: 0x80000100, hex: '01000000' });
        window.testPad = { id: 'Test gamepad', connected: true, mapping: 'standard',
          buttons: Array.from({ length: 17 }, () => ({ pressed: false })), axes: [0, 0, 0, 0] };
        Object.defineProperty(navigator, 'getGamepads', { configurable: true, value: () => [window.testPad] });
        window.xem.setRunning(true);
      });
      const sample = (buttons = [], axes = [0, 0, 0, 0], connected = true) => controls.evaluate(async ({ buttons, axes, connected }) => {
        window.testPad.buttons.forEach((button, i) => { button.pressed = buttons.includes(i); });
        window.testPad.axes = axes;
        window.testPad.connected = connected;
        for (let i = 0; i < 3; i++) await new Promise(requestAnimationFrame);
        return { buttons: window.xem.command({ cmd: 'status' }).pads[0].buttons.sort(),
          panel: window.xem.panel.visible() };
      }, { buttons, axes, connected });
      assert.deepEqual(await sample([0, 13], [-0.8, 0, 0.8, 0]),
        { buttons: ['cross', 'down', 'left', 'r1'], panel: false });
      assert.deepEqual(await sample([], [0.8, 0.8, -0.8, 0]),
        { buttons: ['down', 'l1', 'right'], panel: false });
      assert.deepEqual(await sample([], [-0.8, -0.8, 0, 0]),
        { buttons: ['left', 'up'], panel: false });
      assert.deepEqual(await sample([], [0.2, -0.2, 0.2, 1]), { buttons: [], panel: false });
      assert.deepEqual(await sample([16]), { buttons: [], panel: false }, 'Guide has no app binding');
      assert.deepEqual(await sample([8]), { buttons: ['select'], panel: false });
      assert.deepEqual(await sample([9]), { buttons: ['start'], panel: false });
      assert.deepEqual(await sample([8, 9]), { buttons: [], panel: true });
      assert.deepEqual(await sample([8, 9]), { buttons: [], panel: true }, 'holding the chord does not repeat');
      assert.deepEqual(await sample([8]), { buttons: [], panel: true });
      assert.deepEqual(await sample(), { buttons: [], panel: true });
      assert.deepEqual(await sample([8, 9]), { buttons: [], panel: false });
      assert.deepEqual(await sample([9]), { buttons: [], panel: false }, 'releasing Back first does not send Start');
      assert.deepEqual(await sample(), { buttons: [], panel: false });
      assert.deepEqual(await sample([9]), { buttons: ['start'], panel: false }, 'Start works again after releasing the chord');
      assert.deepEqual(await sample([], [0.8, 0, 0.8, 0]), { buttons: ['r1', 'right'], panel: false });
      assert.deepEqual(await sample([], [0.8, 0, 0.8, 0], false), { buttons: [], panel: false }, 'disconnect releases the sticks');
      assert.deepEqual(controls.errors, []);
    } finally {
      await controls.close();
    }
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
      const xem = window.xem;
      const record = await xem.load('fixture');
      await xem.boot({ executable: false });
      xem.importSnapshot(record.data);
      const at = xem.command({ cmd: 'status' }).frames;
      const next = xem.step(1).frames;
      const word = xem.command({ cmd: 'read_memory', address: 0x80000000, length: 4 });
      return { verified: record.verified, sha256: record.sha256, at, next, word };
    });
    assert.equal(loaded.verified, true);
    assert.equal(loaded.sha256, saved.sha256);
    assert.deepEqual([loaded.at, loaded.next, loaded.word], [2, 3, '02000000']);
  });
});
