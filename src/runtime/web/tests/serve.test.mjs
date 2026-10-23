// The local server: serve.json's settings (none: localhost, plain HTTP,
// nothing more mounted) and the single byte ranges the page's ?disc= reads.

import assert from 'node:assert/strict';
import { mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { loadConfig, serve } from '../serve.mjs';
import { ROOT } from './support.mjs';

const dir = mkdtempSync(join(tmpdir(), 'xem-serve-'));
const config = (settings) => {
  const file = join(dir, `${Math.random()}.json`);
  writeFileSync(file, JSON.stringify(settings));
  return file;
};

describe('serve.json', () => {
  test('without it, localhost over plain HTTP with nothing more mounted', () => {
    assert.deepEqual(loadConfig(join(dir, 'missing.json')), { host: '127.0.0.1', tls: undefined, mounts: {} });
  });

  test('sets the host and mounts directories from the repository', () => {
    const { host, tls, mounts } = loadConfig(config({ host: '0.0.0.0', mounts: { '/discs/': 'discs' } }));
    assert.equal(host, '0.0.0.0');
    assert.equal(tls, undefined);
    assert.deepEqual(mounts, { '/discs/': join(ROOT, 'discs') });
  });

  test('refuses unknown settings, a lone cert and malformed mounts', () => {
    assert.throws(() => loadConfig(config({ port: 1 })), /unknown setting port/);
    assert.throws(() => loadConfig(config({ cert: 'cert.pem' })), /cert and key must be given together/);
    assert.throws(() => loadConfig(config({ mounts: { discs: 'discs' } })), /mount discs/);
  });
});

describe('byte ranges', () => {
  test('answer 206 with the range, and 416 outside the file', async () => {
    writeFileSync(join(dir, 'image.bin'), Buffer.from('0123456789'));
    const server = await serve({ '/': dir });
    const url = `http://localhost:${server.address().port}/image.bin`;
    const get = (range) => fetch(url, { headers: { range } });
    try {
      const whole = await fetch(url);
      assert.equal(whole.headers.get('accept-ranges'), 'bytes');
      assert.equal(await whole.text(), '0123456789');
      for (const [range, body, header] of [['bytes=2-4', '234', 'bytes 2-4/10'], ['bytes=7-', '789', 'bytes 7-9/10'],
        ['bytes=-2', '89', 'bytes 8-9/10'], ['bytes=8-20', '89', 'bytes 8-9/10']]) {
        const response = await get(range);
        assert.equal(response.status, 206, range);
        assert.equal(response.headers.get('content-range'), header, range);
        assert.equal(await response.text(), body, range);
      }
      for (const range of ['bytes=10-12', 'bytes=4-2', 'bytes=0-1,4-5', 'items=0-1']) {
        const response = await get(range);
        assert.equal(response.status, 416, range);
        assert.equal(response.headers.get('content-range'), 'bytes */10', range);
      }
    } finally {
      server.close();
    }
  });
});
