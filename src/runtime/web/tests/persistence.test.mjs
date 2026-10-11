// Failure and interchange tests exercise the real browser storage and shared
// commands. Original state/card files are generated in temporary local paths.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { test } from 'node:test';
import { HEADLESS, NO_WEBGPU, ROOT, USER_DISC, WEB, settingsSection, start, status } from './support.mjs';

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
    assert.equal((await status(page)).session.rewind.max_bytes, 64 * 1024 * 1024);
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
    const before = (await status(page)).session.frames;
    await page.keyboard.down('KeyR');
    await page.waitForFunction((before) => window.xem.status().session.frames < before,before);
    const first = (await status(page)).session.frames;
    await page.waitForFunction((first) => window.xem.status().session.frames < first,first);
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
    await page.evaluate(() => { window.xem.setRunning(true); window.padButtons[6]=true; window.padButtons[8]=true; });
    const padBefore = (await status(page)).session.frames;
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
    await page.evaluate(()=>{
      window.xem.settings.apply({BindApp:{action:'TogglePauseOnFocusLoss',binding:{keyboard:['KeyO'],gamepad:['Back','West']}}});
    });
    await page.keyboard.press('KeyO');
    assert.equal((await page.evaluate(()=>window.xem.settings.get())).settings.pause_on_focus_loss,false);
    await page.evaluate(()=>{window.padButtons[8]=true;window.padButtons[2]=true;});
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
    for(const section of [3,2]) {
      await settingsSection(page,section);
      for(let field=0;field<2;field++) {
        for(let tabs=0;tabs<40;tabs++) {
          await page.keyboard.press('Tab');
          if((await status(page)).renderer.panelTextEditorFocused) break;
        }
        assert.equal((await status(page)).renderer.panelTextEditorFocused,true,'a Settings text editor has focus');
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
    const result = await page.evaluate(async (rate) => {
      const x = window.xem;
      const saved = x.settings.apply({ FastForwardRate: rate });
      if (!saved.ok) throw new Error(saved.reason);
      x.fastForward(true); await x.runFrames(3);
      const before = x.status(); const start = performance.now(); x.setRunning(true);
      await new Promise((resolve) => setTimeout(resolve, 1000)); x.setRunning(false);
      const after = x.status();
      return { before, after, elapsed: performance.now()-start };
    }, rate);
    const samples = result.after.audio.frames - result.before.audio.frames;
    assert.ok(samples > 30_000, `music produced only ${samples} samples in ${result.elapsed} ms`);
    assert.ok(samples < 60_000, `music accelerated to ${samples} samples in ${result.elapsed} ms`);
    assert.ok(result.after.session.frames - result.before.session.frames > 60, 'gameplay accelerates');
    assert.equal(result.after.session.fast_forward,true);
    assert.deepEqual(result.after.session.fast_forward_rate,rate);
    assert.equal(result.after.loop.maxFramesPerAnimation,rate === 'Uncapped' ? null : 80);
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

test('original-game state and raw card files interchange between native and browser', {
  skip: ![USER_DISC, HEADLESS, join(WEB, 'game/game.wasm')].every(existsSync) && 'needs original disc and native/browser builds',
}, async () => {
  const dir = mkdtempSync(join(tmpdir(), 'xem-interchange-'));
  const state = join(dir,'native.xemstate'); const again = join(dir,'browser.xemstate');
  const card = join(dir,'card1.mcd');
  const native = (commands) => JSON.parse(execFileSync(HEADLESS, ['--disc',USER_DISC,'--control','--cards',join(dir,'cards')], {
    cwd: ROOT, input: commands.map((command) => JSON.stringify(command)).join('\n')+'\n', encoding: 'utf8', maxBuffer: 1<<26,
  }).trim().split('\n').at(-1));
  const saved = native([{cmd:'frames',count:600},{cmd:'state_save',path:state},{cmd:'card_export',slot:0,path:card}]);
  assert.equal(saved.ok,true);
  const browser = await start({ args: NO_WEBGPU });
  try {
    const page = await browser.page(); await page.setInputFiles('#file',USER_DISC);
    await page.waitForFunction(() => window.xem.status().disc?.state === 'identified');
    const result = await page.evaluate(async ({state,card}) => {
      const x = window.xem; await x.cards.store(0,Uint8Array.from(card)); await x.boot();
      x.importSnapshot(Uint8Array.from(state));
      const exact = Array.from(x.exportSnapshot());
      await x.runFrames(30);
      return { exact, bytes: Array.from(x.exportSnapshot()), hash: x.memoryHash(), card: Array.from(x.cards.image(0)) };
    }, {state:Array.from(readFileSync(state)),card:Array.from(readFileSync(card))});
    assert.deepEqual(result.exact,Array.from(readFileSync(state)));
    assert.deepEqual(result.card,Array.from(readFileSync(card)));
    writeFileSync(again,Uint8Array.from(result.bytes));
    assert.equal(native([{cmd:'state_load',path:again},{cmd:'memory_hash'}]),result.hash);
    assert.equal(native([{cmd:'state_load',path:state},{cmd:'frames',count:30},{cmd:'state_save',path:state}]).saved,true);
    assert.deepEqual(readFileSync(state),Buffer.from(result.bytes));
    assert.deepEqual(page.errors, []);
  } finally { await browser.close(); }
});
