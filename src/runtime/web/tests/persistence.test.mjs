// Failure and interchange tests exercise the real browser storage and shared
// commands. Original state/card files are generated in temporary local paths.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, ROOT, USER_DISC, WEB, buildFixtures, settingsSection, start, status } from './support.mjs';

const fixture = '?game=/fixtures/fixture.wasm';
const forever = async (page) => page.evaluate(async () => {
  await window.xem.boot({ executable: false, rewind: true });
  window.xem.command({ cmd: 'write_memory', address: 0x80000100, hex: '01000000' });
  // The fixture's endless-play flag must also be in its initial rewind state.
  window.xem.importSnapshot(window.xem.exportSnapshot());
});
const history = async (page) => {
  await page.evaluate(() => window.xem.setRunning(true));
  await page.waitForFunction(() => window.xem.status().session.rewind.states >= 3);
  await page.evaluate(() => window.xem.setRunning(false));
};
const faultInjector = () => {
  window.storageFault = null;
  const put = IDBObjectStore.prototype.put;
  IDBObjectStore.prototype.put = function (...args) {
    const mode = window.storageFault;
    if (mode === 'quota') throw new DOMException('Storage quota exhausted', 'QuotaExceededError');
    const request = put.apply(this, args);
    if (mode === 'interrupt') {
      window.storageFault = null;
      request.onsuccess = () => this.transaction.abort();
    }
    return request;
  };
};

// --- Synthetic saves in the containers players bring (no original content) ----
const FRAME = 0x80, BLOCK = 0x2000, CARD = 0x20000;
const seal = (bytes, at) => { let x = 0; for (let i = 0; i < 0x7f; i++) x ^= bytes[at + i]; bytes[at + 0x7f] = x; };
/** This game's save of file `digit` (one block): its title frame, its file
 * digit and the payload's checksum as the game writes them. */
function gameSave(digit) {
  const block = Buffer.alloc(BLOCK);
  block.set([0x53, 0x43, 0x11, 1]);
  block.set([0x82, 0x77, 0x82, 0x64, 0x82, 0x6d, 0x82, 0x6e, 0x82, 0x66, 0x82, 0x64, 0x82, 0x60, 0x82, 0x71, 0x82, 0x72,
    0x81, 0x5e, 0x82, 0x6d, 0x82, 0x8f, 0x81, 0x44, 0x82, 0x4f + Math.floor((digit + 1) / 10), 0x82, 0x4f + (digit + 1) % 10], 4);
  block[0x123] = digit;
  let sum = 0;
  for (let i = 0x100; i < 0x1fff; i++) sum = (sum + block[i]) & 0xff;
  block[0x1fff] = sum;
  return block;
}
const gameName = (digit) => `BASLUS-00664${String.fromCharCode(0x30 + digit)}`;
/** The containers of a single save: .mcs (its directory frame first), PS3
 * .psv (its header, type 1), Action Replay's 54-byte header. */
function mcs(name, blocks) {
  const frame = Buffer.alloc(FRAME);
  frame[0] = 0x51; frame.writeUInt32LE(blocks.length, 4); frame.writeUInt16LE(0xffff, 8);
  frame.write(name, 0x0a, 'latin1'); seal(frame, 0);
  return Buffer.concat([frame, blocks]);
}
function psv(name, blocks) {
  const header = Buffer.alloc(0x84, 0);
  header.write('\0VSP', 0, 'latin1'); header.fill(0xa5, 0x08, 0x30);
  header[0x38] = 0x14; header[0x3c] = 1; header.writeUInt32LE(blocks.length, 0x40); header[0x44] = 0x84;
  header.write(name, 0x64, 'latin1');
  return Buffer.concat([header, blocks]);
}
function actionReplay(name, blocks) {
  const header = Buffer.alloc(54);
  header.write(name, 0, 'latin1'); header.write('Xenogears', 0x15, 'latin1');
  return Buffer.concat([header, blocks]);
}
/** The containers of a whole card around a raw image. */
function vgs(card) {
  const header = Buffer.alloc(0x40);
  header.write('VgsM', 0, 'latin1'); header[4] = 1; header[8] = 1; header[12] = 1; header[17] = 2;
  return Buffer.concat([header, card]);
}
function vmp(card) {
  const header = Buffer.alloc(0x80, 0x5a);
  header.write('\0PMV', 0, 'latin1'); header.writeUInt32LE(0x80, 4);
  return Buffer.concat([header, card]);
}
function gme(card) {
  const header = Buffer.alloc(0xf40);
  header.write('123-456-STD', 0, 'latin1');
  return Buffer.concat([header, card]);
}

// Each test owns its browser and storage. Keep them sequential: this suite
// also measures elapsed play time, and competing software-rendered browsers
// can stall a fast-forward batch while its history clock keeps advancing.
describe('browser storage, rewind, pause and interchange', { concurrency: 1 }, () => {
  test('a card or save file imports into a card by its contents, and a refused one changes nothing', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true, initScripts: [faultInjector] });
    try {
      const page = await browser.page(fixture);
      // Enable native chooser interception before the picker opens.
      page.on('filechooser', () => {});
      const toPage = (buffer) => Array.from(buffer);
      // A single save (.mcs) before a boot: a fresh card 1 holding it, in
      // IndexedDB and then in the session.
      const first = await page.evaluate(async (file) => {
        const x = window.xem;
        const message = await x.cards.importFile(0, Uint8Array.from(file), 'file1.mcs');
        const stored = Array.from(await x.cards.stored(0));
        await x.boot({ executable: false });
        return { message, stored, card: Array.from(x.cards.image(0)), listed: x.command({ cmd: 'card_list', slot: 0 }) };
      }, toPage(mcs(gameName(0), gameSave(0))));
      assert.equal(first.message, 'Card 1: single save (.mcs, .ps1) on a fresh card, 1 save');
      assert.deepEqual(first.card, first.stored, 'the session uses the stored card');
      const saves = first.listed.card.saves;
      assert.deepEqual(saves.map((s) => [s.name, s.first_block, s.kind.file, s.kind.disc, s.problems.length]), [[gameName(0), 1, 1, 1, 0]]);
      assert.equal(saves[0].title, 'ＸＥＮＯＧＥＡＲＳ／Ｎｏ．０１');
      assert.deepEqual(first.card.slice(BLOCK, 2 * BLOCK), toPage(gameSave(0)), 'its block on the card');

      // Into the running session's card 2: the other single saves, each a
      // fresh card holding only it, and card 1 in each card container.
      const card = Buffer.from(first.card);
      const damaged = Buffer.from(card); damaged[FRAME + 0x20] ^= 1;
      const result = await page.evaluate(async ({ singles, whole, refused, psvFile }) => {
        const x = window.xem;
        const bytes = (array) => Uint8Array.from(array);
        const card1 = x.cards.image(0);
        const same = (image) => image.length === card1.length && image.every((b, i) => b === card1[i]);
        const found = [];
        for (const [name, file] of singles) {
          const message = await x.cards.importFile(1, bytes(file), name);
          found.push([message, x.command({ cmd: 'card_list', slot: 1 }).card.saves.map((s) => s.name)]);
        }
        for (const [name, file] of whole) {
          const message = await x.cards.importFile(1, bytes(file), name);
          found.push([message, same(x.cards.image(1)) && same(await x.cards.stored(1))]);
        }
        const errors = [];
        for (const [name, file] of refused) {
          try { await x.cards.importFile(0, bytes(file), name); errors.push(`${name} accepted`); } catch (error) { errors.push(String(error.message ?? error)); }
        }
        // IndexedDB takes a card before the session: a failed write changes neither.
        window.storageFault = 'quota';
        try { await x.cards.importFile(0, bytes(psvFile), 'BASLUS-006641.PSV'); errors.push('accepted without storage'); } catch (error) { errors.push(String(error.message ?? error)); }
        window.storageFault = null;
        return { found, errors, unchanged: same(x.cards.image(0)) && same(await x.cards.stored(0)) };
      }, {
        singles: [['BASLUS-006641.PSV', toPage(psv(gameName(1), gameSave(1)))], ['file3.psx', toPage(actionReplay(gameName(2), gameSave(2)))],
          [gameName(3), toPage(gameSave(3))]],
        whole: [['card1.gme', toPage(gme(card))], ['card.vgs', toPage(vgs(card))], ['SCEVMC1.VMP', toPage(vmp(card))], ['card1.mcr', toPage(card)]],
        refused: [['damaged.mcd', toPage(damaged)], ['notes.txt', new Array(1000).fill(7)], ['my save.bin', toPage(gameSave(5))]],
        psvFile: toPage(psv(gameName(1), gameSave(1))),
      });
      assert.deepEqual(result.found, [
        ['Card 2: PS3 save (.psv) on a fresh card, 1 save', [gameName(1)]],
        ['Card 2: Action Replay save (.psx, .mcb, .mcx, .pda) on a fresh card, 1 save', [gameName(2)]],
        ['Card 2: raw save on a fresh card, 1 save', [gameName(3)]],
        ['Card 2: DexDrive card, 1 save', true], ['Card 2: Connectix VGS card, 1 save', true],
        ['Card 2: PSP or PS Vita card, 1 save', true], ['Card 2: raw card, 1 save', true],
      ]);
      assert.match(result.errors[0], /^memory card 1 not changed: the picked card's directory is damaged: directory frame 1/);
      assert.match(result.errors[1], /notes\.txt \(1000 bytes\) is not a memory card or a save/);
      assert.match(result.errors[2], /named by its file name/);
      assert.match(result.errors[3], /quota/i);
      assert.ok(result.unchanged, 'card 1 is as it was, in the session and in IndexedDB');

      // The page's own picker (Import card 2, on a key here) accepts every
      // supported extension and says what it found (the operation's
      // message): a raw single save named by its file becomes a fresh card 2.
      const accept = await page.$eval('#import-card', (input) => input.accept.split(','));
      for (const extension of ['.mcd', '.mcr', '.mc', '.srm', '.mem', '.bin', '.vm1', '.ddf', '.ps', '.gme', '.vgs', '.vmp', '.mcs', '.ps1', '.psv', '.psx', '.mcb', '.mcx', '.pda']) {
        assert.ok(accept.includes(extension), `the picker accepts ${extension}`);
      }
      const bound = await page.evaluate(() => window.xem.settings.apply({ BindApp: { action: 'ImportCard2', binding: { keyboard: ['F9'], gamepad: [] } } }));
      assert.equal(bound.ok, true);
      const picker = page.waitForEvent('filechooser');
      await page.keyboard.press('F9');
      await (await picker).setFiles({ name: gameName(4), mimeType: 'application/octet-stream', buffer: gameSave(4) });
      await page.waitForFunction(() => window.xem.status().page.operationStatus?.startsWith('Card 2:'));
      const picked = await page.evaluate(async () => ({ status: window.xem.status().page.operationStatus,
        saves: window.xem.command({ cmd: 'card_list', slot: 1 }).card.saves.map((s) => s.name), stored: (await window.xem.cards.stored(1)).length }));
      assert.deepEqual(picked, { status: 'Card 2: raw save on a fresh card, 1 save', saves: [gameName(4)], stored: CARD });
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('a BIOS image imports beside the cards for every boot, and a file that is not one changes nothing', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true, initScripts: [faultInjector] });
    try {
      const page = await browser.page(fixture);
      page.on('filechooser', () => {});
      const result = await page.evaluate(async () => {
        const x = window.xem;
        // A plausible font where a BIOS keeps it (0x66000): a blank
        // full-width space, its digits and Latin letters (glyphs 147-208)
        // each drawn differently. No original content.
        const image = (mark) => {
          const rom = new Uint8Array(0x80000);
          for (let glyph = 147; glyph <= 208; glyph++) rom[0x66000 + glyph * 30 + 2] = glyph - 146;
          rom[0] = mark;
          return rom;
        };
        const errors = [];
        const refused = async (bytes) => {
          try { await x.bios.import(bytes); errors.push('accepted'); } catch (error) { errors.push(String(error.message ?? error)); }
        };
        const before = x.status().bios;
        await refused(new Uint8Array(1000).fill(7));
        await refused(new Uint8Array(0x80000));
        const nothing = (await x.bios.stored()) === undefined && !x.status().bios;
        const message = await x.bios.import(image(1));
        // IndexedDB takes an image before the session: a failed write changes nothing.
        window.storageFault = 'quota';
        await refused(image(2));
        window.storageFault = null;
        const stored = await x.bios.stored();
        return { before, errors, nothing, message, stored: [stored.length, stored[0]], after: x.status().bios };
      });
      assert.deepEqual(result.errors.slice(0, 2), [
        'BIOS not imported: a PS1 BIOS image is 512 KiB (524288 bytes), not 1000 bytes.',
        'BIOS not imported: this file has no Kanji font where a PS1 BIOS keeps it (offset 0x66000): a full-width letter or digit is blank.',
      ]);
      assert.match(result.errors[2], /quota/i);
      assert.equal(result.before, false);
      assert.ok(result.nothing, 'a refused file is neither stored nor kept');
      assert.equal(result.message, 'BIOS imported: save titles use its Kanji font.');
      assert.deepEqual(result.stored, [0x80000, 1], 'IndexedDB holds the image imported, not the one its write refused');
      assert.equal(result.after, true);

      // A reload takes it from IndexedDB for the boots to come.
      await page.reload(); await page.waitForFunction(() => document.body.dataset.ready === 'true');
      assert.equal(await page.evaluate(() => window.xem.status().bios), true);

      // Import BIOS, on a key here, opens the page's own picker.
      const accept = await page.$eval('#import-bios', (input) => input.accept.split(','));
      assert.ok(accept.includes('.bin') && accept.includes('.rom'), `${accept}`);
      const bound = await page.evaluate(() => window.xem.settings.apply({ BindApp: { action: 'ImportBios', binding: { keyboard: ['F9'], gamepad: [] } } }));
      assert.equal(bound.ok, true);
      const rom = Buffer.alloc(0x80000);
      for (let glyph = 147; glyph <= 208; glyph++) rom[0x66000 + glyph * 30 + 2] = glyph - 146;
      rom[0] = 3;
      const picker = page.waitForEvent('filechooser');
      await page.keyboard.press('F9');
      await (await picker).setFiles({ name: 'SCPH1001.BIN', mimeType: 'application/octet-stream', buffer: rom });
      await page.waitForFunction(() => window.xem.status().page.operationStatus?.startsWith('BIOS imported'));
      assert.equal(await page.evaluate(async () => (await window.xem.bios.stored())[0]), 3);
      assert.equal(await page.evaluate(() => window.xem.status().page.biosNotice), null);

      // A stored image the check refuses (written past the import's check,
      // as a damaged one would be) stays stored and is reported, and boots
      // take the substitute glyphs; importing an image ends the notice.
      await page.evaluate(async () => (await import('/storage.js')).put('cards', 'bios', new Uint8Array(1000).fill(7)));
      await page.reload(); await page.waitForFunction(() => document.body.dataset.ready === 'true');
      const kept = await page.evaluate(async () => {
        const x = window.xem;
        const refused = { bios: x.status().bios, notice: x.status().page.biosNotice, stored: (await x.bios.stored()).length };
        const rom = new Uint8Array(0x80000);
        for (let glyph = 147; glyph <= 208; glyph++) rom[0x66000 + glyph * 30 + 2] = glyph - 146;
        await x.bios.import(rom);
        return { refused, imported: { bios: x.status().bios, notice: x.status().page.biosNotice } };
      });
      assert.deepEqual(kept.refused, { bios: false, stored: 1000,
        notice: 'Imported BIOS image not used: a PS1 BIOS image is 512 KiB (524288 bytes), not 1000 bytes; save titles use the substitute glyphs.' });
      assert.deepEqual(kept.imported, { bios: true, notice: null });
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('HUD shows active time controls and state results without memory-card notices', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true, initScripts: [faultInjector] });
    try {
      const page = await browser.page(fixture); await forever(page);
      await page.evaluate(() => {
        const x = window.xem;
        x.settings.apply({ ShowFps: false });
        x.command({ cmd: 'fast_forward', enabled: true });
      });
      assert.equal((await status(page)).renderer.hud.fast_forward,false,'enabled FF without play is idle');
      await history(page);
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => window.xem.status().renderer.hud.fast_forward);
      await page.evaluate(() => window.xem.setRunning(false));
      await page.waitForFunction(() => !window.xem.status().renderer.hud.fast_forward);
      await page.evaluate(async () => {
        await window.xem.appOperation({ Rewind: true });
        window.xem.setRunning(true);
      });
      await page.waitForFunction(() => window.xem.status().renderer.hud.rewinding);
      assert.equal((await status(page)).renderer.hud.fast_forward,false,'rewind takes precedence over enabled FF');
      await page.waitForFunction(() => !window.xem.status().renderer.hud.rewinding);
      await page.evaluate(async () => {
        await window.xem.appOperation({ Rewind: false });
        window.xem.setRunning(false);
        await window.xem.appOperation({ Action: 'SaveState' });
      });
      assert.equal((await status(page)).renderer.hud.notice,'State saved');
      await page.evaluate(async () => {
        const x = window.xem;
        await x.cards.store(0,x.cards.image(0));
      });
      assert.equal((await status(page)).renderer.hud.notice,'State saved','ordinary card writes do not replace the state notice');
      await page.evaluate(() => window.xem.appOperation({ Action: 'LoadState' }));
      assert.equal((await status(page)).renderer.hud.notice,'State loaded');
      await page.evaluate(async () => {
        window.storageFault = 'quota';
        try { await window.xem.appOperation({ Action: 'SaveState' }); } catch {}
        window.storageFault = null;
      });
      assert.match((await status(page)).renderer.hud.notice,/Could not save state:.*quota/i);
      await page.evaluate(async () => {
        const storage = await import('/storage.js');
        await storage.remove('saves','slot0');
        try { await window.xem.appOperation({ Action: 'LoadState' }); } catch {}
      });
      assert.match((await status(page)).renderer.hud.notice,/Could not load state:.*No stored state/);
      assert.equal((await status(page)).renderer.panelVisible,false);
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('quota and interrupted writes retain the previous state and pending cards for retry', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true, initScripts: [faultInjector] });
    try {
      const page = await browser.page(fixture); await forever(page);
      const result = await page.evaluate(async () => {
        const x = window.xem;
        const first = await x.save('errors');
        window.storageFault = 'quota';
        let quota, interrupted, cardError;
        try { await x.save('errors'); } catch (e) { quota = String(e); }
        const afterQuota = await x.load('errors');
        try { await x.runFrames(3); } catch (e) { cardError = String(e); }
        const pending = x.status().page.cards;
        const backup = Array.from(x.cards.image(0));
        window.storageFault = null;
        await x.cards.retry();
        const stored = Array.from(await x.cards.stored(0));
        window.storageFault = 'interrupt';
        try { await x.save('errors'); } catch (e) { interrupted = String(e); }
        const afterInterrupt = await x.load('errors');
        return { first, quota, interrupted, cardError, pending, backup, stored,
          afterQuota: afterQuota.sha256, afterInterrupt: afterInterrupt.sha256, cards: x.status().page.cards };
      });
      assert.match(result.quota, /quota/i); assert.match(result.cardError, /quota/i);
      assert.match(result.interrupted, /interrupted/i);
      assert.equal(result.afterQuota, result.first.sha256);
      assert.equal(result.afterInterrupt, result.first.sha256);
      assert.equal(result.pending.pending, 1); assert.match(result.pending.error, /quota/i);
      assert.deepEqual(result.stored, result.backup);
      assert.equal(result.cards.pending, 0); assert.equal(result.cards.error, null);
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('denied storage remains usable and refuses to acknowledge unsaved settings', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true, initScripts: [() => {
      Object.defineProperty(window, 'localStorage', { get() { throw new DOMException('Storage denied', 'SecurityError'); } });
      Object.defineProperty(window, 'indexedDB', { get() { throw new DOMException('Storage denied', 'SecurityError'); } });
    }] });
    try {
      const page = await browser.page(fixture);
      const result = await page.evaluate(async () => {
        const x = window.xem;
        await x.boot({ executable: false });
        const before = x.settings.get();
        const rejected = x.settings.apply({ MasterVolume: 20 });
        let saveError;
        try { await x.save('denied'); } catch (e) { saveError = String(e); }
        return { before, after: x.settings.get(), rejected, saveError, status: x.status() };
      });
      assert.equal(result.rejected.ok, false); assert.equal(result.rejected.rejected, 'storage');
      assert.deepEqual(result.before.settings, result.after.settings);
      assert.match(result.before.error, /Storage|storage/);
      assert.match(result.saveError, /denied/i);
      assert.match(result.status.page.cards.error, /denied/i);
      assert.equal(result.status.page.biosNotice, null, 'storage that cannot be read is the cards\' error, not a BIOS image\'s');
      assert.ok(result.status.renderer.frames >= 0);
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('corruption and eviction are explicit and leave the running state unchanged', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true });
    try {
      const page = await browser.page(fixture); await forever(page);
      const result = await page.evaluate(async () => {
        const x = window.xem; const storage = await import('/storage.js');
        await x.save('corrupt');
        const record = await storage.get('saves', 'corrupt'); record.bytes[80] ^= 1;
        await storage.put('saves', 'corrupt', record);
        const before = Array.from(x.exportSnapshot()); let error;
        try { await x.load('corrupt'); } catch (e) { error = String(e); }
        const after = Array.from(x.exportSnapshot());
        await storage.remove('saves', 'corrupt');
        const missing = await x.load('corrupt');
        return { before, after, error, missing, notice: x.status().page.storage.error };
      });
      assert.match(result.error, /checksum/i); assert.deepEqual(result.before, result.after);
      assert.equal(result.missing, null); assert.match(result.notice, /cleared or evicted/);
    } finally { await browser.close(); }
  });

  test('a state another game module build saved is refused by name, and the running state and stored state stay', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true });
    try {
      const page = await browser.page(fixture); await forever(page);
      await page.evaluate(() => window.xem.appOperation({ Action: 'SaveState' }));
      const stored = () => page.evaluate(async () => (await (await import('/storage.js')).get('saves', 'slot0'))?.sha256);
      const saved = await stored();
      // The page again, running the other build.
      await page.goto(`${browser.base}?game=/fixtures/fixture-b.wasm`);
      await page.waitForFunction(() => document.body.dataset.ready === 'true');
      await forever(page);
      const result = await page.evaluate(async () => {
        const x = window.xem; const before = Array.from(x.exportSnapshot()); let error;
        try { await x.appOperation({ Action: 'LoadState' }); } catch (e) { error = String(e.message ?? e); }
        return { before, after: Array.from(x.exportSnapshot()), error, notice: x.status().renderer.hud.notice };
      });
      const [state, running] = ['fixture.wasm', 'fixture-b.wasm']
        .map((name) => createHash('sha256').update(readFileSync(join(buildFixtures(), name))).digest('hex'));
      assert.notEqual(state, running);
      assert.equal(result.error, `incompatible state: the state's game module is "${state}", this session's is "${running}"`);
      assert.equal(result.notice, `Could not load state: ${result.error}`);
      assert.deepEqual(result.after, result.before, 'the running state is unchanged');
      assert.equal(await stored(), saved, 'the stored state is kept');
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('state restore preserves ordinary card saves and an imported card survives an in-game save', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true });
    try {
      const page = await browser.page(fixture);
      const result = await page.evaluate(async () => {
        const x = window.xem;
        await x.boot({ executable: false, rewind: true });
        const initial = x.exportSnapshot();
        await x.appOperation({ Action: 'SaveState' });
        const imported = x.cards.image(0); imported[0x1ffff] = 0x77;
        await x.cards.store(0, imported);
        const inserted = x.cards.image(0)[0x1ffff];
        x.command({ cmd: 'write_memory', address: 0x80000100, hex: '01000000' });
        await x.runFrames(3); await x.cards.flush();
        const saved = await x.cards.stored(0);
        const written = x.command({cmd:'read_memory',address:0x80000108,length:4});
        const unchanged = async () => {
          await x.cards.flush();
          return (await x.cards.stored(0)).every((b,i) => b === saved[i]) && x.cards.image(0).every((b,i) => b === saved[i]);
        };
        const rewind = x.rewind(); const afterRewind = await unchanged();
        x.importSnapshot(initial); const afterImport = await unchanged();
        await x.appOperation({ Action: 'LoadState' }); const afterLoad = await unchanged();
        return {inserted, written, retained: saved[0x1ffff], rewind, afterRewind, afterImport, afterLoad};
      });
      assert.equal(result.inserted,0x77);
      assert.equal(result.written,'80000000','the fixture saved through the BIOS');
      assert.equal(result.retained,0x77,'the original imported bytes survive that save');
      assert.ok(!result.rewind.error,JSON.stringify(result.rewind));
      assert.ok(result.afterRewind && result.afterImport && result.afterLoad,'all restore paths keep current live and durable cards');
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('evicted cards produce a visible missing-storage notice on boot', async () => {
    const browser = await start({args:NO_WEBGPU,fixtures:true});
    try {
      const page = await browser.page(fixture);
      await page.evaluate(async () => {
        const x = window.xem; await x.boot({executable:false});
        await x.cards.store(0,x.cards.image(0));
        const storage = await import('/storage.js'); await storage.remove('cards','card1');
      });
      await page.reload(); await page.waitForFunction(() => window.xem);
      const current = await page.evaluate(async () => { await window.xem.boot({executable:false}); return window.xem.status(); });
      assert.match(current.page.cards.notice,/No stored card.*cleared or evicted/);
      assert.match(current.page.operationStatus,/No stored card/);
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('animation frames and unrelated Settings changes preserve agent time-control commands', async () => {
    const browser = await start({args:NO_WEBGPU,fixtures:true});
    try {
      const page = await browser.page(fixture); await forever(page);
      const result = await page.evaluate(async () => {
        const x = window.xem;
        x.command({cmd:'fast_forward_configure',rate:{Fixed:5}});
        x.command({cmd:'rewind_configure',max_bytes:8*1024*1024,interval_seconds:3,history_seconds:12});
        const before = x.status().session;
        x.settings.apply({MasterVolume:55}); x.setRunning(true);
        await new Promise((resolve) => setTimeout(resolve,1250)); x.setRunning(false);
        return {before,after:x.status().session};
      });
      assert.deepEqual(result.after.fast_forward_rate,{Fixed:5});
      for (const field of ['max_bytes','interval_seconds','history_seconds','oldest_frame']) {
        assert.equal(result.after.rewind[field],result.before.rewind[field],field);
      }
      assert.ok(result.after.frames > result.before.frames);
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('random battles stay off in the platforming areas until Original is chosen, which persists and reaches the session', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true });
    try {
      const page = await browser.page(fixture); await forever(page);
      await page.evaluate(() => window.xem.setRunning(true));
      // The running page gives the session the default policy's maps before its frames.
      await page.waitForFunction(() => window.xem.status().session.random_battles_off.includes(414));
      const off = (await status(page)).session.random_battles_off;
      assert.deepEqual(off, [...off].sort((a, b) => a - b), 'ascending');
      assert.equal((await page.evaluate(() => window.xem.settings.get())).settings.random_battles, 'NotWhilePlatforming');
      const reply = await page.evaluate(() => window.xem.settings.apply({ RandomBattles: 'Original' }));
      assert.equal(reply.ok, true, JSON.stringify(reply));
      await page.waitForFunction(() => window.xem.status().session.random_battles_off.length === 0);
      assert.equal(await page.evaluate(() => JSON.parse(localStorage.getItem('xem.settings')).random_battles), 'Original');
      // The choice lives in the panel's Gameplay section.
      await page.evaluate(() => window.xem.panel.show(true));
      await page.waitForTimeout(300);
      await settingsSection(page, 6);
      await page.evaluate(() => window.xem.setRunning(false));
      await page.reload(); await page.waitForFunction(() => document.body.dataset.ready === 'true');
      assert.equal((await page.evaluate(() => window.xem.settings.get())).settings.random_battles, 'Original');
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('half encounters, text speed, movie skipping and auto-advance reach the session from the settings, and T toggles auto-advance', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true });
    try {
      const page = await browser.page(fixture); await forever(page);
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => window.xem.status().session.frames > 2);
      const session = async () => (await status(page)).session;
      const before = await session();
      assert.deepEqual([before.half_encounters, before.text_speed, before.movie_skip, before.auto_advance], [false, 0, false, 0], 'the original by default');
      for (const change of [{ EncounterRate: 'Half' }, { TextSpeed: 'Instant' }, { MovieSkip: true }]) {
        const reply = await page.evaluate((c) => window.xem.settings.apply(c), change);
        assert.equal(reply.ok, true, JSON.stringify(reply));
      }
      await page.waitForFunction(() => { const s = window.xem.status().session; return s.half_encounters === true && s.text_speed === 255 && s.movie_skip === true; });
      // The app action's default key goes through the settings too.
      await page.keyboard.press('KeyT');
      await page.waitForFunction(() => window.xem.status().session.auto_advance === 120);
      assert.equal((await page.evaluate(() => window.xem.settings.get())).settings.auto_advance.enabled, true);
      const stored = await page.evaluate(() => JSON.parse(localStorage.getItem('xem.settings')));
      assert.deepEqual([stored.text_speed, stored.movie_skip, stored.auto_advance], ['Instant', true, { enabled: true, dwell: 120 }]);
      await page.keyboard.press('KeyT');
      await page.waitForFunction(() => window.xem.status().session.auto_advance === 0);
      await page.evaluate(() => window.xem.setRunning(false));
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('an old xem.settings reads the gameplay defaults, agents change each option with an acknowledgement, and the choices survive a reload and a backup', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true });
    const ready = async (page) => page.waitForFunction(() => document.body.dataset.ready === 'true');
    const gameplay = (s) => [s.random_battles, s.encounter_rate, s.text_speed, s.auto_advance, s.movie_skip];
    try {
      const page = await browser.page(fixture);
      // Settings saved before the gameplay options and the auto-advance
      // shortcut existed, with a choice of their own.
      await page.evaluate(() => {
        const old = window.xem.settings.get().settings;
        for (const key of ['random_battles', 'encounter_rate', 'text_speed', 'auto_advance', 'movie_skip']) delete old[key];
        delete old.app_bindings.ToggleAutoAdvance;
        localStorage.setItem('xem.settings', JSON.stringify({ ...old, master_volume: 40 }));
      });
      await page.reload(); await ready(page);
      const read = (await page.evaluate(() => window.xem.settings.get())).settings;
      assert.deepEqual([...gameplay(read), read.master_volume],
        ['NotWhilePlatforming', 'Original', 'Original', { enabled: false, dwell: 120 }, false, 40], 'the defaults, the rest kept');
      assert.deepEqual(read.app_bindings.ToggleAutoAdvance, { keyboard: ['KeyT'], gamepad: ['Back', 'West'] }, 'the new shortcut reads its default');
      await forever(page);
      await page.evaluate(() => window.xem.setRunning(true));
      await page.waitForFunction(() => { const s = window.xem.status().session; return s.random_battles_off.includes(414) && s.text_speed === 0; });
      // Agents change each through the settings service: acknowledged in turn.
      const first = (await page.evaluate(() => window.xem.settings.get())).revision;
      const changes = [{ RandomBattles: 'Original' }, { EncounterRate: 'Half' }, { TextSpeed: 'Instant' },
        { AutoAdvance: { enabled: true, dwell: 90 } }, { MovieSkip: true }];
      for (const [n, change] of changes.entries()) {
        const reply = await page.evaluate((c) => window.xem.settings.apply(c), change);
        assert.deepEqual([reply.ok, reply.revision], [true, first + n + 1], JSON.stringify(reply));
      }
      const refused = await page.evaluate(() => window.xem.settings.apply({ AutoAdvance: { enabled: true, dwell: 5 } }));
      assert.deepEqual([refused.ok, refused.rejected], [false, 'invalid'], JSON.stringify(refused));
      assert.match(refused.reason, /auto-advance dwell must be 30–600/);
      // The session's status shows each on the next frames.
      await page.waitForFunction(() => {
        const s = window.xem.status().session;
        return s.random_battles_off.length === 0 && s.half_encounters && s.text_speed === 255 && s.auto_advance === 90 && s.movie_skip;
      });
      const chosen = ['Original', 'Half', 'Instant', { enabled: true, dwell: 90 }, true];
      const backup = await page.evaluate(() => window.xem.settings.export());
      await page.evaluate(() => window.xem.setRunning(false));
      await page.reload(); await ready(page);
      assert.deepEqual(gameplay((await page.evaluate(() => window.xem.settings.get())).settings), chosen, 'kept by a reload');
      // After another change, importing the backup brings them back.
      assert.equal((await page.evaluate(() => window.xem.settings.apply({ RandomBattles: 'NotWhilePlatforming' }))).ok, true);
      const imported = await page.evaluate((text) => window.xem.settings.import(text), backup);
      assert.deepEqual(gameplay(imported.settings), chosen, 'carried by the backup');
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('Settings operations have keyboard and gamepad paths and bindings survive reload', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true, initScripts: [() => {
      window.padButtons = Array(17).fill(false);
      Object.defineProperty(navigator, 'getGamepads', { value: () => [{ connected: true, id: 'Test standard gamepad',
        buttons: window.padButtons.map((pressed) => ({ pressed, value: Number(pressed) })) }] });
    }] });
    try {
      const page = await browser.page(fixture);
      // Enable native chooser interception before exercising bound operations.
      page.on('filechooser', () => {});
      await forever(page);
      // Default bindings route through the same app operation handlers.
      await page.keyboard.press('F7');
      assert.equal((await status(page)).session.rewind.max_bytes, 0);
      await page.keyboard.press('F7');
      assert.equal((await status(page)).session.rewind.max_bytes, 2 * 1024 * 1024 * 1024);
      await page.keyboard.press('Backquote'); assert.equal((await status(page)).session.fast_forward, true);
      assert.deepEqual((await status(page)).session.fast_forward_rate, { Fixed: 10 });
      await page.evaluate(() => { window.padButtons[8] = true; window.padButtons[7] = true; });
      await page.waitForFunction(() => window.xem.status().session.fast_forward === false);
      await page.waitForTimeout(100);
      assert.equal((await status(page)).session.fast_forward, false, 'holding a chord never repeats it');
      await page.evaluate(() => { window.padButtons.fill(false); });
      const fpsBinding=await page.evaluate(()=>window.xem.settings.apply({BindApp:{action:'ToggleFps',binding:{keyboard:['F6'],gamepad:['Start','East']}}}));
      assert.equal(fpsBinding.ok,true);
      await page.keyboard.press('F6');
      assert.equal((await page.evaluate(()=>window.xem.settings.get())).settings.presentation.show_fps,true);
      await page.evaluate(()=>{window.padButtons[9]=true;window.padButtons[1]=true;});
      await page.waitForFunction(()=>window.xem.settings.get().settings.presentation.show_fps===false);
      await page.evaluate(()=>{window.padButtons.fill(false);});
      await page.keyboard.press('F5');
      await page.waitForFunction(async () => Boolean(await window.xem.load('slot0')));
      const binding = await page.evaluate(() => window.xem.settings.apply({ BindApp: {
        action: 'ImportState', binding: { keyboard: ['F11'], gamepad: ['Start','North'] } } }));
      assert.equal(binding.ok, true);
      const picker = page.waitForEvent('filechooser');
      await page.keyboard.press('F11');
      const chooser = await picker;
      assert.equal(chooser.isMultiple(), false);
      await chooser.setFiles([]);
      await page.reload(); await page.waitForFunction(() => document.body.dataset.ready === 'true');
      const settings = await page.evaluate(() => window.xem.settings.get());
      assert.deepEqual(settings.settings.app_bindings.ImportState, { keyboard: ['F11'], gamepad: ['Start','North'] });
      assert.equal(await page.locator('#save, #load, #speed, footer details').count(), 0, 'persistence UI belongs in Settings');
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('rewind follows held R and LT, and inverted trigger chords send L2/R2', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true, initScripts: [() => {
      window.padButtons = Array(17).fill(false);
      Object.defineProperty(navigator,'getGamepads',{value:()=>[{connected:true,id:'Triggers',
        buttons:window.padButtons.map(pressed=>({pressed,value:Number(pressed)}))}]});
    }] });
    try {
      const page = await browser.page(fixture); await forever(page);
      await history(page); await page.evaluate(() => window.xem.setRunning(true));
      // Capture the baseline at the real keydown: play can advance between
      // automation calls. Observe each animation in the page so a slow
      // round-trip cannot miss either of the two held rewind steps.
      await page.evaluate(() => {
        window.heldRewindSteps = [];
        const begin = (event) => {
          if (event.code !== 'KeyR' || event.repeat) return;
          window.removeEventListener('keydown', begin, true);
          window.heldRewindBefore = window.xem.status().session.frames;
          let previous = window.heldRewindBefore;
          const sample = () => {
            const frame = window.xem.status().session.frames;
            if (frame < previous) window.heldRewindSteps.push(frame);
            previous = frame;
            if (window.heldRewindSteps.length < 2) requestAnimationFrame(sample);
          };
          requestAnimationFrame(sample);
        };
        window.addEventListener('keydown', begin, true);
      });
      await page.keyboard.down('KeyR');
      await page.waitForFunction(() => window.heldRewindSteps.length >= 2);
      const { before, steps } = await page.evaluate(() => ({
        before: window.heldRewindBefore, steps: window.heldRewindSteps,
      }));
      assert.ok(steps[0] < before && steps[1] < steps[0], `held rewind must restore two distinct earlier frames: ${before} -> ${steps}`);
      await page.keyboard.up('KeyR');
      const released = (await status(page)).session.frames;
      await page.waitForFunction((released) => window.xem.status().session.frames > released,released);
      await history(page); await page.evaluate(() => window.xem.setRunning(true));
      const focusBefore = (await status(page)).session.frames;
      await page.keyboard.down('KeyR');
      await page.waitForFunction((focusBefore) => window.xem.status().session.frames < focusBefore,focusBefore);
      await page.evaluate(() => window.dispatchEvent(new Event('blur')));
      const focusReleased = (await status(page)).session.frames;
      await page.waitForFunction(frame=>window.xem.status().session.frames>frame,focusReleased);
      await page.keyboard.up('KeyR');
      await page.evaluate(() => window.dispatchEvent(new Event('focus')));
      await history(page);
      const padBefore = await page.evaluate(() => {
        const before = window.xem.status().session.frames;
        window.xem.setRunning(true); window.padButtons[6]=true; window.padButtons[8]=true;
        return before;
      });
      await page.waitForFunction((padBefore) => window.xem.status().session.frames < padBefore,padBefore);
      await page.evaluate(() => window.dispatchEvent(new Event('blur')));
      const padFocusReleased = (await status(page)).session.frames;
      await page.waitForFunction(frame=>window.xem.status().session.frames>frame,padFocusReleased);
      await page.evaluate(() => { window.padButtons.fill(false); window.dispatchEvent(new Event('focus')); });
      const padReleased = (await status(page)).session.frames;
      await page.waitForFunction((padReleased) => window.xem.status().session.frames > padReleased,padReleased);
      await page.evaluate(() => window.xem.setRunning(false));
      // The exact raw PS1 pad verifies that neither the app chord nor its Back
      // modifier leaks into gameplay when the chord is inverted.
      await page.evaluate(() => {
        const x=window.xem; x.settings.apply({InvertTriggerChords:true});
        window.padButtons[6]=true; window.padButtons[8]=true; x.setRunning(true);
      });
      await page.waitForFunction(() => window.xem.status().session.pads[0].buttons.includes('l2'));
      assert.deepEqual((await status(page)).session.pads[0].buttons,['l2']);
      await page.evaluate(() => { window.padButtons[6]=false; window.padButtons[7]=true; });
      await page.waitForFunction(() => window.xem.status().session.pads[0].buttons.includes('r2'));
      assert.deepEqual((await status(page)).session.pads[0].buttons,['r2']);
      assert.equal((await status(page)).session.fast_forward,false);
      const invertedBefore = (await status(page)).session.frames;
      await page.evaluate(() => { window.padButtons.fill(false); window.padButtons[6]=true; });
      await page.waitForFunction((invertedBefore) => window.xem.status().session.frames < invertedBefore,invertedBefore);
      await page.evaluate(() => { window.padButtons.fill(false); });
      const invertedReleased = (await status(page)).session.frames;
      await page.waitForFunction((invertedReleased) => window.xem.status().session.frames > invertedReleased,invertedReleased);
      await page.evaluate(() => { window.padButtons[7]=true; });
      await page.waitForFunction(() => window.xem.status().session.fast_forward === true);
      await page.waitForTimeout(100);
      assert.equal((await status(page)).session.fast_forward,true,'holding RT does not repeat the FF toggle');
      await page.evaluate(() => { window.padButtons.fill(false); window.xem.setRunning(false); });
      await page.reload(); await page.waitForFunction(() => document.body.dataset.ready === 'true');
      assert.equal((await page.evaluate(() => window.xem.settings.get())).settings.invert_trigger_chords,true);
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('focus loss uses the original pause screen, respects script locks, and stays paused after refocus', {
    skip: ![USER_DISC,HEADLESS,join(WEB,'game/game.wasm')].every(existsSync) && 'needs original disc and native/browser builds',
  }, async () => {
    const dir=mkdtempSync(join(tmpdir(),'xem-focus-pause-'));
    const state=join(dir,'field.xemstate');
    const commands=[{cmd:'enter_field',map:22},{cmd:'frames',count:300},{cmd:'state_save',path:state}];
    const replies=execFileSync(HEADLESS,['--disc',USER_DISC,'--control','--cards',join(dir,'cards')],{
      cwd:ROOT,input:commands.map(command=>JSON.stringify(command)).join('\n')+'\n',encoding:'utf8',maxBuffer:1<<26,
    }).trim().split('\n').map(line=>JSON.parse(line));
    assert.ok(replies.every(reply=>!reply.error),JSON.stringify(replies));
    const browser=await start({args:NO_WEBGPU,initScripts:[()=>{
      window.padButtons=Array(17).fill(false);
      Object.defineProperty(navigator,'getGamepads',{value:()=>[{connected:true,id:'Focus pause controls',
        buttons:window.padButtons.map(pressed=>({pressed,value:Number(pressed)}))}]});
    }]});
    try {
      const page=await browser.page(); await page.setInputFiles('#file',USER_DISC);
      await page.waitForFunction(()=>window.xem.status().disc?.state==='identified');
      await page.evaluate(async bytes=>{
        const x=window.xem; await x.boot({rewind:true}); x.importSnapshot(Uint8Array.from(bytes));
        x.configureRewind(64*1024*1024,1,2); x.setRunning(true);
      },Array.from(readFileSync(state)));
      assert.equal((await page.evaluate(()=>window.xem.settings.get())).settings.pause_on_focus_loss,true);
      await page.waitForFunction(()=>window.xem.status().session.rewind.states>=2);
      const locked=await page.evaluate(()=>{
        const x=window.xem;
        x.command({cmd:'write',path:'field_work.unk2358',value:1});
        x.fastForward(true); window.dispatchEvent(new Event('blur'));
        return x.status().session.frames;
      });
      await page.waitForFunction(frame=>window.xem.status().session.frames>frame+10,locked);
      assert.equal(await page.evaluate(()=>window.xem.command({cmd:'inspect',path:'sound_driver_flags'})&0x40),0,
        'a script pause lock defers the focus pause');
      const cancelled=await page.evaluate(()=>{
        window.dispatchEvent(new Event('focus'));
        window.xem.command({cmd:'write',path:'field_work.unk2358',value:0});
        return window.xem.status().session.frames;
      });
      await page.waitForFunction(frame=>window.xem.status().session.frames>frame+10,cancelled);
      assert.equal(await page.evaluate(()=>window.xem.command({cmd:'inspect',path:'sound_driver_flags'})&0x40),0,
        'refocus before the next legal pause cancels the request');
      await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
      await page.waitForFunction(()=>window.xem.command({cmd:'inspect',path:'sound_driver_flags'})&0x40);
      const paused=(await status(page)).session;
      await page.waitForTimeout(2200);
      assert.deepEqual((await status(page)).session.rewind,paused.rewind,'the in-game pause freezes rewind history');
      await page.evaluate(()=>window.dispatchEvent(new Event('focus')));
      await page.waitForFunction(frame=>window.xem.status().session.frames>frame,paused.frames);
      assert.equal(await page.evaluate(()=>window.xem.command({cmd:'inspect',path:'sound_driver_flags'})&0x40),0x40,
        'focus gain leaves the original pause screen open');
      await page.evaluate(()=>window.xem.fastForward(false));
      await page.keyboard.down('Enter'); await page.waitForTimeout(80); await page.keyboard.up('Enter');
      await page.waitForFunction(()=>!(window.xem.command({cmd:'inspect',path:'sound_driver_flags'})&0x40));
      // A chord no default binding uses (Back+West toggles auto-advance).
      const bound=await page.evaluate(()=>
        window.xem.settings.apply({BindApp:{action:'TogglePauseOnFocusLoss',binding:{keyboard:['KeyO'],gamepad:['Back','L1']}}}));
      assert.equal(bound.ok,true,JSON.stringify(bound));
      await page.keyboard.press('KeyO');
      assert.equal((await page.evaluate(()=>window.xem.settings.get())).settings.pause_on_focus_loss,false);
      await page.evaluate(()=>{window.padButtons[8]=true;window.padButtons[4]=true;});
      await page.waitForFunction(()=>window.xem.settings.get().settings.pause_on_focus_loss);
      await page.evaluate(()=>{
        window.padButtons.fill(false);
        window.xem.settings.apply({PauseOnFocusLoss:false}); window.dispatchEvent(new Event('blur'));
      });
      const disabled=(await status(page)).session.frames;
      await page.waitForFunction(frame=>window.xem.status().session.frames>frame+4,disabled);
      assert.equal(await page.evaluate(()=>window.xem.command({cmd:'inspect',path:'sound_driver_flags'})&0x40),0);
      await page.evaluate(()=>{window.dispatchEvent(new Event('focus'));window.xem.setRunning(false);});
      await page.reload(); await page.waitForFunction(()=>document.body.dataset.ready==='true');
      assert.equal((await page.evaluate(()=>window.xem.settings.get())).settings.pause_on_focus_loss,false);
      assert.deepEqual(page.errors,[]);
    } finally {await browser.close();}
  });

  test('mouse camera releases on original Start pause and battle combo input without opening Settings', {
    skip: ![USER_DISC,HEADLESS,join(WEB,'game/game.wasm')].every(existsSync) && 'needs original disc and native/browser builds',
  }, async () => {
    const dir = mkdtempSync(join(tmpdir(),'xem-mouse-pause-'));
    const state = join(dir,'field.xemstate');
    const commands = [{cmd:'enter_field',map:22},{cmd:'frames',count:300},{cmd:'state_save',path:state}];
    const replies = execFileSync(HEADLESS,['--disc',USER_DISC,'--control','--cards',join(dir,'cards')],{
      cwd:ROOT,input:commands.map(command=>JSON.stringify(command)).join('\n')+'\n',encoding:'utf8',maxBuffer:1<<26,
    }).trim().split('\n').map(line=>JSON.parse(line));
    assert.ok(replies.every(reply=>!reply.error),JSON.stringify(replies));
    const battleState = join(dir,'battle.xemstate');
    const battleCommands = [
      {cmd:'frames',count:60},{cmd:'write',path:'mode_battle_standalone',value:1},
      {cmd:'boot_mode',mode:2},{cmd:'frames',count:900},
      {cmd:'press',buttons:['start'],frames:1},{cmd:'frames',count:1520},
      {cmd:'press',buttons:['cross'],frames:2},{cmd:'frames',count:18},
      {cmd:'state_save',path:battleState},
    ];
    const battleReplies = execFileSync(HEADLESS,['--disc',USER_DISC,'--control','--cards',join(dir,'cards')],{
      cwd:ROOT,input:battleCommands.map(command=>JSON.stringify(command)).join('\n')+'\n',encoding:'utf8',maxBuffer:1<<26,
    }).trim().split('\n').map(line=>JSON.parse(line));
    assert.ok(battleReplies.every(reply=>!reply?.error),JSON.stringify(battleReplies));
    const browser = await start({args:NO_WEBGPU});
    try {
      const page = await browser.page();
      await page.setInputFiles('#file',USER_DISC);
      await page.waitForFunction(()=>window.xem.status().disc?.state==='identified');
      await page.evaluate(async bytes=>{
        const x=window.xem;
        await x.boot(); x.importSnapshot(Uint8Array.from(bytes));
        x.settings.apply({MouseCamera:true}); x.setRunning(true);
      },Array.from(readFileSync(state)));
      const canvas = page.locator('canvas');
      await canvas.click();
      await page.waitForFunction(()=>document.pointerLockElement === document.querySelector('canvas'));
      await page.keyboard.down('Enter');
      await page.waitForFunction(()=>window.xem.command({cmd:'inspect',path:'sound_driver_flags'})&0x40);
      await page.keyboard.up('Enter');
      await page.waitForFunction(()=>!document.pointerLockElement);
      assert.equal((await status(page)).renderer.panelVisible,false,'game pause must not open app Settings');
      // A click on the pause screen resumes play (it presses Start once);
      // the next click gives the game the mouse again.
      await canvas.click();
      await page.waitForFunction(()=>!(window.xem.command({cmd:'inspect',path:'sound_driver_flags'})&0x40));
      assert.equal(await page.evaluate(()=>!!document.pointerLockElement),false,'clicking the pause screen cannot recapture the cursor');
      await canvas.click();
      await page.waitForFunction(()=>document.pointerLockElement === document.querySelector('canvas'));
      await page.evaluate(bytes=>window.xem.importSnapshot(Uint8Array.from(bytes)),Array.from(readFileSync(battleState)));
      await page.waitForFunction(()=>window.xem.command({cmd:'context'}).attack);
      await page.waitForFunction(()=>!document.pointerLockElement);
      assert.equal((await status(page)).renderer.panelVisible,false,'battle input releases without opening Settings');
      const ap = await page.evaluate(()=>window.xem.command({cmd:'context'}).ap);
      // A click on the attack page's Weak attack (a click on the enemy only
      // chooses it as the target).
      const weak = await page.evaluate(()=>window.xem.command({cmd:'targets'}).entries.find(entry=>entry.press==='AttackWeak').rect);
      const {renderer} = await status(page);
      const box = await canvas.boundingBox(), picture = renderer.picture;
      const u = (weak.x + weak.width/2 + picture.side)/(picture.display[0] + 2*picture.side);
      const x = picture.x + u*picture.width, y = picture.y + (weak.y + weak.height/2)/picture.display[1]*picture.height;
      await page.mouse.move(box.x + x*box.width/renderer.size[0], box.y + y*box.height/renderer.size[1]);
      await page.mouse.down();
      await page.waitForFunction(ap=>window.xem.command({cmd:'context'}).ap===ap-1,ap);
      await page.mouse.up();
      assert.equal(await page.evaluate(()=>!!document.pointerLockElement),false,'attacking by mouse cannot recapture the cursor');
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('rewind keeps the current FF toggle, rate and app settings', async () => {
    const browser=await start({args:NO_WEBGPU,fixtures:true});
    try {
      const page=await browser.page(fixture); await forever(page);
      await page.evaluate(()=>window.xem.fastForward(true));
      await history(page);
      await page.evaluate(()=>{
        const x=window.xem; x.fastForward(false);
        x.settings.apply({FastForwardRate:{Fixed:20}}); x.settings.apply({MasterVolume:33});
      });
      const before=(await status(page)).session.frames;
      await page.keyboard.down('KeyR');
      await page.evaluate(()=>window.xem.setRunning(true));
      await page.waitForFunction((before)=>window.xem.status().session.frames<before,before);
      let current=await status(page);
      assert.equal(current.session.fast_forward,false,'rewind must not re-enable FF');
      assert.deepEqual(current.session.fast_forward_rate,{Fixed:20});
      assert.equal((await page.evaluate(()=>window.xem.settings.get())).settings.master_volume,33);
      await page.keyboard.up('KeyR'); await page.evaluate(()=>window.xem.setRunning(false));
      await page.evaluate(()=>{
        const x=window.xem; x.settings.apply({FastForwardRate:'Uncapped'}); x.fastForward(true);
      });
      await history(page);
      const again=(await status(page)).session.frames;
      await page.keyboard.down('KeyR'); await page.evaluate(()=>window.xem.setRunning(true));
      await page.waitForFunction((again)=>window.xem.status().session.frames<again,again);
      current=await status(page);
      assert.equal(current.session.fast_forward,true,'rewind must keep FF enabled too');
      assert.equal(current.session.fast_forward_rate,'Uncapped');
      await page.keyboard.up('KeyR'); await page.evaluate(()=>window.xem.setRunning(false));
      assert.deepEqual(page.errors,[]);
    } finally {await browser.close()}
  });

  test('save states omit app controls and Settings, including the rewind interval', async () => {
    const browser=await start({args:NO_WEBGPU,fixtures:true});
    try {
      const page=await browser.page(fixture); await forever(page);
      const result=await page.evaluate(async()=>{
        const x=window.xem; x.fastForward(true); await x.runFrames(120);
        const saved=Array.from(x.exportSnapshot()); await x.appOperation({Action:'SaveState'});
        x.fastForward(false);
        for (const change of [{FastForwardRate:{Fixed:20}},{MasterVolume:33},{InvertTriggerChords:true},{RewindIntervalSeconds:3},{RewindHistorySeconds:10}]) {
          if (!x.settings.apply(change).ok) throw new Error('setting was rejected');
        }
        x.command({cmd:'record'});
        const afterAppChanges=Array.from(x.exportSnapshot());
        await x.runFrames(20); await x.appOperation({Action:'LoadState'});
        const loaded={session:x.status().session,settings:x.settings.get().settings,bytes:Array.from(x.exportSnapshot())};
        x.fastForward(true); x.settings.apply({FastForwardRate:'Uncapped'});
        await x.appOperation({Action:'LoadState'});
        return {saved,afterAppChanges,loaded,again:x.status().session};
      });
      assert.deepEqual(result.afterAppChanges,result.saved,'app changes do not change the serialized game state');
      assert.deepEqual(result.loaded.bytes,result.saved);
      assert.equal(result.loaded.session.frames,120);
      assert.equal(result.loaded.session.fast_forward,false);
      assert.deepEqual(result.loaded.session.fast_forward_rate,{Fixed:20});
      assert.equal(result.loaded.session.recording,true);
      assert.equal(result.loaded.settings.master_volume,33);
      assert.equal(result.loaded.settings.invert_trigger_chords,true);
      assert.equal(result.loaded.settings.rewind_interval_seconds,3);
      assert.equal(result.loaded.settings.rewind_history_seconds,10);
      assert.equal(result.again.fast_forward,true);
      assert.equal(result.again.fast_forward_rate,'Uncapped');
      assert.deepEqual(page.errors,[]);
    } finally {await browser.close()}
  });

  test('rewind uses elapsed seconds during FF and its timing settings have keyboard and gamepad bindings', async () => {
    const browser=await start({args:NO_WEBGPU,fixtures:true,initScripts:[()=>{
      window.padButtons=Array(17).fill(false);
      Object.defineProperty(navigator,'getGamepads',{value:()=>[{connected:true,id:'Interval controls',
        buttons:window.padButtons.map(pressed=>({pressed,value:Number(pressed)}))}]});
    }]});
    try {
      const page=await browser.page(fixture); await forever(page);
      assert.equal((await page.evaluate(()=>window.xem.settings.get())).settings.rewind_interval_seconds,1);
      assert.equal((await status(page)).session.rewind.history_seconds,30);
      const started=await page.evaluate(()=>{
        const x=window.xem; x.settings.apply({FastForwardRate:{Fixed:20}}); x.fastForward(true); x.setRunning(true);
        return performance.now();
      });
      await page.waitForFunction(()=>window.xem.status().session.rewind.states>=2);
      const captured=await page.evaluate(()=>({now:performance.now(),session:window.xem.status().session}));
      assert.ok(captured.now-started>=900,'a 1-second real-time interval is not accelerated by FF');
      assert.ok(captured.session.rewind.newest_frame>60,'the queue is not created every fixed number of game frames');
      await page.evaluate(()=>{
        const x=window.xem; x.setRunning(false);
        x.settings.apply({BindApp:{action:'IncreaseRewindInterval',binding:{keyboard:['KeyI'],gamepad:['North']}}});
        x.settings.apply({BindApp:{action:'DecreaseRewindInterval',binding:{keyboard:['KeyU'],gamepad:['East']}}});
        x.settings.apply({BindApp:{action:'IncreaseRewindHistory',binding:{keyboard:['KeyO'],gamepad:['L1']}}});
        x.settings.apply({BindApp:{action:'DecreaseRewindHistory',binding:{keyboard:['KeyP'],gamepad:['R1']}}});
      });
      await page.keyboard.press('KeyI');
      assert.equal((await status(page)).session.rewind.interval_seconds,2);
      await page.keyboard.press('KeyU');
      assert.equal((await status(page)).session.rewind.interval_seconds,1);
      await page.evaluate(()=>{window.padButtons[3]=true});
      await page.waitForFunction(()=>window.xem.status().session.rewind.interval_seconds===2);
      await page.evaluate(()=>window.padButtons.fill(false));
      await page.waitForTimeout(100);
      await page.evaluate(()=>{window.padButtons[1]=true});
      await page.waitForFunction(()=>window.xem.status().session.rewind.interval_seconds===1);
      await page.evaluate(()=>window.padButtons.fill(false));
      await page.keyboard.press('KeyO');
      assert.equal((await status(page)).session.rewind.history_seconds,31);
      await page.keyboard.press('KeyP');
      assert.equal((await status(page)).session.rewind.history_seconds,30);
      await page.waitForTimeout(100);
      await page.evaluate(()=>{window.padButtons[4]=true});
      await page.waitForFunction(()=>window.xem.status().session.rewind.history_seconds===31);
      await page.evaluate(()=>window.padButtons.fill(false));
      await page.waitForTimeout(100);
      await page.evaluate(()=>{window.padButtons[5]=true});
      await page.waitForFunction(()=>window.xem.status().session.rewind.history_seconds===30);
      await page.evaluate(()=>window.padButtons.fill(false));
      await page.reload(); await page.waitForFunction(()=>document.body.dataset.ready==='true');
      const settings=(await page.evaluate(()=>window.xem.settings.get())).settings;
      assert.equal(settings.rewind_interval_seconds,1);
      assert.equal(settings.rewind_history_seconds,30);
      assert.deepEqual(settings.app_bindings.IncreaseRewindInterval,{keyboard:['KeyI'],gamepad:['North']});
      assert.deepEqual(settings.app_bindings.IncreaseRewindHistory,{keyboard:['KeyO'],gamepad:['L1']});
      assert.deepEqual(page.errors,[]);
    } finally {await browser.close()}
  });

  test('typing R in Settings timing and binding fields does not rewind', async () => {
    const browser=await start({args:NO_WEBGPU,fixtures:true});
    try {
      const page=await browser.page(fixture); await forever(page);
      await page.evaluate(async()=>{await window.xem.runFrames(150);window.xem.setRunning(true)});
      await page.click('#settings');
      // Time (its rewind fields), then select a shortcut to open its editors.
      for(const section of [4,3]) {
        const panel = await settingsSection(page,section);
        if(section === 3) await page.mouse.click(panel.left+270,panel.top+97); // Save state row
        for(let field=0;field<2;field++) {
          for(let tabs=0;tabs<40;tabs++) {
            await page.keyboard.press('Tab');
            if((await status(page)).renderer.panelTextEditorFocused) break;
          }
          assert.equal((await status(page)).renderer.panelTextEditorFocused,true,`Settings section ${section}, field ${field} has focus`);
          const before=(await status(page)).session.frames;
          await page.keyboard.down('KeyR'); await page.waitForTimeout(250);
          const after=await status(page); await page.keyboard.up('KeyR');
          assert.ok(after.session.frames>before,`typing R rewound from ${before} to ${after.session.frames}`);
          assert.notEqual(after.page.operationStatus,'Rewinding while held.');
        }
      }
      assert.deepEqual(page.errors,[]);
    } finally {await browser.close()}
  });

  test('the configured history length expires game saves by elapsed play time', async () => {
    const browser=await start({args:NO_WEBGPU,fixtures:true});
    try {
      const page=await browser.page(fixture); await forever(page);
      await page.evaluate(()=>{
        const x=window.xem;
        if(!x.settings.apply({RewindHistorySeconds:2}).ok) throw new Error('history setting was rejected');
        x.fastForward(true); x.setRunning(true);
      });
      await page.waitForFunction(()=>window.xem.status().session.rewind.oldest_frame>0);
      const current=await status(page);
      assert.equal(current.session.rewind.history_seconds,2);
      assert.ok(current.session.rewind.available_seconds<=2);
      assert.ok(current.session.rewind.states<=3);
      assert.ok(current.session.rewind.oldest_frame>60,'retention follows seconds of FF play, not a game frame limit');
      await page.evaluate(()=>window.xem.setRunning(false));
      await page.reload(); await page.waitForFunction(()=>document.body.dataset.ready==='true');
      assert.equal((await page.evaluate(()=>window.xem.settings.get())).settings.rewind_history_seconds,2);
      assert.deepEqual(page.errors,[]);
    } finally {await browser.close()}
  });

  test('20× and uncapped fast-forward keep music on host time', async () => {
    const browser = await start({ args: NO_WEBGPU, fixtures: true });
    try {
      const page = await browser.page(fixture);
      for (const rate of [{ Fixed: 20 }, 'Uncapped']) {
      await forever(page);
      await page.evaluate(async (rate) => {
        const x = window.xem;
        const saved = x.settings.apply({ FastForwardRate: rate });
        if (!saved.ok) throw new Error(saved.reason);
        x.fastForward(true); await x.runFrames(3);
        x.setRunning(true);
      }, rate);
      await page.waitForFunction(() => window.xem.status().renderer.hud.fast_forward);
      const result = await page.evaluate(async () => {
        // Finish the first visible HUD frame before measuring: the browser
        // may compile its GPU work after the frame callback returns.
        await new Promise(requestAnimationFrame);
        const x = window.xem;
        const before = x.status(); const start = performance.now();
        await new Promise((resolve) => setTimeout(resolve, 1000)); x.setRunning(false);
        const after = x.status();
        return { before, after, elapsed: performance.now()-start };
      });
      const samples = result.after.audio.frames - result.before.audio.frames;
      assert.ok(samples > 30_000, `music produced only ${samples} samples in ${result.elapsed} ms`);
      assert.ok(samples < 60_000, `music accelerated to ${samples} samples in ${result.elapsed} ms`);
      assert.ok(result.after.session.frames - result.before.session.frames > 60, 'gameplay accelerates');
      assert.equal(result.after.session.fast_forward,true);
      assert.deepEqual(result.after.session.fast_forward_rate,rate);
      // Fixed fast-forward adapts to the pace this host reaches. The batch
      // limit follows that recorded pace; the configured rate remains 20x.
      const pace = result.after.session.fast_forward_pace ?? rate.Fixed;
      if (rate !== 'Uncapped') assert.ok(Number.isInteger(pace) && pace >= 1 && pace <= 20);
      assert.equal(result.after.loop.maxFramesPerAnimation,rate === 'Uncapped' ? null : 4 * pace);
      }
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });

  test('original Start pause freezes rewind history while browser frames keep running', {
    skip: ![USER_DISC, HEADLESS, join(WEB, 'game/game.wasm')].every(existsSync) && 'needs original disc and native/browser builds',
  }, async () => {
    const dir = mkdtempSync(join(tmpdir(), 'xem-pause-rewind-'));
    const state = join(dir,'field.xemstate');
    const commands = [{cmd:'enter_field',map:22},{cmd:'frames',count:300},{cmd:'state_save',path:state}];
    const replies = execFileSync(HEADLESS, ['--disc',USER_DISC,'--control','--cards',join(dir,'cards')], {
      cwd:ROOT,input:commands.map(command=>JSON.stringify(command)).join('\n')+'\n',encoding:'utf8',maxBuffer:1<<26,
    }).trim().split('\n').map(line=>JSON.parse(line));
    assert.ok(replies.every(reply=>!reply.error),JSON.stringify(replies));
    const browser = await start({args:NO_WEBGPU});
    try {
      const page = await browser.page(); await page.setInputFiles('#file',USER_DISC);
      await page.waitForFunction(()=>window.xem.status().disc?.state==='identified');
      await page.evaluate(async bytes=>{
        const x=window.xem; await x.boot({rewind:true}); x.importSnapshot(Uint8Array.from(bytes));
        x.configureRewind(64*1024*1024,1,2); x.setRunning(true);
      },Array.from(readFileSync(state)));
      await page.waitForFunction(()=>window.xem.status().session.rewind.states>=2);
      await page.evaluate(()=>window.xem.command({cmd:'press',buttons:['start'],frames:4}));
      await page.waitForFunction(()=>window.xem.command({cmd:'inspect',path:'sound_driver_flags'}) & 0x40);
      const paused = (await status(page)).session;
      await page.waitForTimeout(3000);
      const later = (await status(page)).session;
      assert.ok(later.frames>paused.frames,'the original pause screen keeps receiving game frames');
      assert.deepEqual(later.rewind,paused.rewind,'paused frames neither capture nor expire playing history');
      await page.evaluate(()=>window.xem.command({cmd:'press',buttons:['start'],frames:4}));
      await page.waitForFunction(()=>!(window.xem.command({cmd:'inspect',path:'sound_driver_flags'}) & 0x40));
      const resumed = (await status(page)).session.rewind;
      for (const field of ['states','bytes','oldest_frame','newest_frame']) {
        assert.equal(resumed[field],paused.rewind[field],`resume must not capture immediately: ${field}`);
      }
      assert.ok(resumed.available_seconds-paused.rewind.available_seconds<1,'only resumed play advances queue time');
      await page.waitForFunction(frame=>window.xem.status().session.rewind.newest_frame>frame,paused.rewind.newest_frame);
      await page.evaluate(()=>window.xem.setRunning(false));
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('missing inspection schema cannot unpause on blur or record paused rewind frames', {
    skip: ![USER_DISC,HEADLESS,join(WEB,'game/game.wasm')].every(existsSync) && 'needs original disc and native/browser builds',
  }, async () => {
    const dir=mkdtempSync(join(tmpdir(),'xem-no-schema-pause-'));
    const state=join(dir,'field.xemstate');
    const replies=execFileSync(HEADLESS,['--disc',USER_DISC,'--control','--cards',join(dir,'cards')],{
      cwd:ROOT,input:[{cmd:'enter_field',map:22},{cmd:'frames',count:300},{cmd:'state_save',path:state}]
        .map(command=>JSON.stringify(command)).join('\n')+'\n',encoding:'utf8',maxBuffer:1<<26,
    }).trim().split('\n').map(JSON.parse);
    assert.ok(replies.every(reply=>!reply.error),JSON.stringify(replies));
    const address=JSON.parse(readFileSync(join(WEB,'game/schema.json'),'utf8')).globals.sound_driver_flags.address;
    const browser=await start({args:NO_WEBGPU});
    try {
      await browser.context.route('**/game/schema.json',route=>route.fulfill({status:404,body:'unavailable'}));
      const page=await browser.page();
      assert.equal((await status(page)).page.game.schema,false);
      await page.setInputFiles('#file',USER_DISC);
      await page.waitForFunction(()=>window.xem.status().disc?.state==='identified');
      await page.evaluate(async ({bytes,address})=>{
        const x=window.xem; await x.boot({rewind:true}); x.importSnapshot(Uint8Array.from(bytes));
        window.isGamePaused=()=>parseInt(x.command({cmd:'read_memory',address,length:1}),16)&0x40;
        x.configureRewind(64*1024*1024,1,30); x.setRunning(true);
      },{bytes:Array.from(readFileSync(state)),address});
      await page.waitForFunction(()=>window.xem.status().session.rewind.states>=2);
      await page.evaluate(()=>window.xem.command({cmd:'press',buttons:['start'],frames:4}));
      await page.waitForFunction(()=>window.isGamePaused());
      const paused=(await status(page)).session;
      await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
      await page.waitForTimeout(2200);
      assert.ok(await page.evaluate(()=>window.isGamePaused()),'blur must not synthesize resume Start');
      const later=(await status(page)).session;
      assert.ok(later.frames>paused.frames,'the original pause screen must keep running');
      assert.deepEqual(later.rewind,paused.rewind,'no schema is needed to freeze the rewind queue');
      await page.evaluate(()=>window.dispatchEvent(new Event('focus')));
      await page.keyboard.down('Enter'); await page.waitForTimeout(100); await page.keyboard.up('Enter');
      await page.waitForFunction(()=>!window.isGamePaused());
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('original-game browser rewind frame p99 stays below 16 ms', {
    skip: ![USER_DISC,HEADLESS,join(WEB,'game/game.wasm')].every(existsSync) && 'needs original disc and native/browser builds',
  }, async () => {
    const dir=mkdtempSync(join(tmpdir(),'xem-rewind-latency-'));
    const state=join(dir,'field.xemstate');
    const replies=execFileSync(HEADLESS,['--disc',USER_DISC,'--control','--cards',join(dir,'cards')],{
      cwd:ROOT,input:[{cmd:'enter_field',map:22},{cmd:'frames',count:300},{cmd:'state_save',path:state}]
        .map(command=>JSON.stringify(command)).join('\n')+'\n',encoding:'utf8',maxBuffer:1<<26,
    }).trim().split('\n').map(JSON.parse);
    assert.ok(replies.every(reply=>!reply.error),JSON.stringify(replies));
    const browser=await start({args:NO_WEBGPU});
    try {
      const page=await browser.page(); await page.setInputFiles('#file',USER_DISC);
      await page.waitForFunction(()=>window.xem.status().disc?.state==='identified');
      const result=await page.evaluate(async bytes=>{
        const x=window.xem; await x.boot(); x.importSnapshot(Uint8Array.from(bytes));
        await x.runFrames(120); // Warm game code and required disc sectors.
        const realNow=performance.now.bind(performance), samples=[], checkpoints=[];
        let clock=realNow();
        Object.defineProperty(performance,'now',{configurable:true,value:()=>clock});
        try {
          const configured=x.configureRewind(64*1024*1024,1,30);
          if (configured.error) throw new Error(configured.error);
          for (let frame=0;frame<300;frame++) {
            if (frame%10===0) clock+=1000;
            const status=x.status().session, before=status.rewind.newest_frame;
            const started=realNow(), reply=x.step();
            const elapsed=realNow()-started;
            if (reply.error || reply.blocked || reply.frames!==status.frames+1) throw new Error(JSON.stringify(reply));
            samples.push(elapsed);
            if (x.status().session.rewind.newest_frame!==before) checkpoints.push(elapsed);
          }
        } finally { delete performance.now; }
        samples.sort((a,b)=>a-b); checkpoints.sort((a,b)=>a-b);
        return {p99:samples[296],median:checkpoints[Math.floor(checkpoints.length/2)],
          max:checkpoints.at(-1),checkpoints:checkpoints.length};
      },Array.from(readFileSync(state)));
      assert.ok(result.checkpoints>=29,JSON.stringify(result));
      console.log('browser rewind latency (ms):',result);
      assert.ok(result.p99<16,`browser rewind frame p99: ${JSON.stringify(result)}`);
      assert.deepEqual(page.errors,[]);
    } finally { await browser.close(); }
  });

  test('original-game states and raw card files interchange between native and browser', {
    skip: ![USER_DISC, HEADLESS, join(WEB, 'game/game.wasm')].every(existsSync) && 'needs original disc and native/browser builds',
  }, async () => {
    const dir = mkdtempSync(join(tmpdir(), 'xem-interchange-'));
    const card = join(dir,'card1.mcd');
    const native = (commands) => JSON.parse(execFileSync(HEADLESS, ['--disc',USER_DISC,'--control','--cards',join(dir,'cards')], {
      cwd: ROOT, input: commands.map((command) => JSON.stringify(command)).join('\n')+'\n', encoding: 'utf8', maxBuffer: 1<<26,
    }).trim().split('\n').at(-1));
    // Native states inside the opening movie (frame 600) and on the world map
    // out of Lahan's south exit, at the end of its session log.
    const route = JSON.parse(readFileSync(join(ROOT,'tests/reference-inputs/world-map-lahan-exit.json'),'utf8'));
    const places = [
      { place: 'the opening movie', commands: [{cmd:'frames',count:600}] },
      { place: 'the world map', commands: [{cmd:'schedule',commands:route.commands},{cmd:'run_until',max_frames:route.frames,until:{frames:route.frames}}],
        hash: route.memory_hash },
    ].map(({place,commands,hash},i) => {
      const state = join(dir,`native-${i}.xemstate`);
      assert.equal(native([...commands,{cmd:'state_save',path:state},{cmd:'card_export',slot:0,path:card}]).ok,true,place);
      if (hash) assert.equal(native([{cmd:'state_load',path:state},{cmd:'memory_hash'}]),hash,`${place}: the route's end`);
      return { place, state };
    });
    const browser = await start({ args: NO_WEBGPU });
    try {
      const page = await browser.page(); await page.setInputFiles('#file',USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified');
      const results = await page.evaluate(async ({states,card}) => {
        const x = window.xem; await x.cards.store(0,Uint8Array.from(card)); await x.boot();
        const results = [];
        for (const state of states) {
          x.importSnapshot(Uint8Array.from(state));
          const exact = Array.from(x.exportSnapshot());
          await x.runFrames(30);
          results.push({ exact, bytes: Array.from(x.exportSnapshot()), hash: x.memoryHash(), scene: x.command({cmd:'readiness'}).mode_name, card: Array.from(x.cards.image(0)) });
        }
        return results;
      }, {states:places.map(({state}) => Array.from(readFileSync(state))),card:Array.from(readFileSync(card))});
      assert.deepEqual(results.map(({scene}) => scene), ['movie', 'world map'], 'where the states were saved');
      for (const [i,{place,state}] of places.entries()) {
        const result = results[i];
        assert.deepEqual(result.exact,Array.from(readFileSync(state)),`${place}: imported exactly`);
        assert.deepEqual(result.card,Array.from(readFileSync(card)));
        const again = join(dir,`browser-${i}.xemstate`);
        writeFileSync(again,Uint8Array.from(result.bytes));
        assert.equal(native([{cmd:'state_load',path:again},{cmd:'memory_hash'}]),result.hash,`${place}: the browser's state natively`);
        assert.equal(native([{cmd:'state_load',path:state},{cmd:'frames',count:30},{cmd:'state_save',path:state}]).saved,true);
        assert.deepEqual(readFileSync(state),Buffer.from(result.bytes),`${place}: the same 30 frames natively`);
      }
      assert.deepEqual(page.errors, []);
    } finally { await browser.close(); }
  });
});
