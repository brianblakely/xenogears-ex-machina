// The local server: serve.json's settings (none: localhost, plain HTTP,
// nothing more mounted) and the single byte ranges the page's ?disc= reads.

import assert from 'node:assert/strict';
import { mkdirSync, mkdtempSync, symlinkSync, writeFileSync } from 'node:fs';
import { request as httpRequest } from 'node:http';
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

describe('path containment', () => {
  test('rejects encoded traversal into a sibling whose name shares the root prefix', async () => {
    const publicDir = join(dir, 'public');
    const neighbor = join(dir, 'public-private');
    mkdirSync(publicDir);
    mkdirSync(neighbor);
    writeFileSync(join(neighbor, 'secret.txt'), 'private');
    const server = await serve({ '/': publicDir });
    try {
      const response = await fetch('http://127.0.0.1:' + server.address().port + '/%2e%2e%2fpublic-private/secret.txt');
      assert.equal(response.status, 404);
    } finally {
      server.close();
    }
  });

  test('rejects symlinks that escape the root and a mounted share', async () => {
    const root = join(dir, 'symlink-root');
    const mount = join(dir, 'symlink-mount');
    const outside = join(dir, 'symlink-outside');
    const internal = join(root, 'internal.txt');
    mkdirSync(root);
    mkdirSync(mount);
    mkdirSync(outside);
    writeFileSync(join(outside, 'secret.txt'), 'private');
    writeFileSync(join(outside, 'index.html'), 'private index');
    writeFileSync(internal, 'inside');
    symlinkSync(join(outside, 'secret.txt'), join(root, 'leak.txt'));
    symlinkSync(join(outside, 'secret.txt'), join(mount, 'leak.txt'));
    symlinkSync(outside, join(root, 'leak-directory'));
    symlinkSync(internal, join(root, 'internal-link.txt'));
    const server = await serve({ '/': root, '/mounted/': mount });
    try {
      for (const path of ['/leak.txt', '/leak-directory/', '/mounted/leak.txt']) {
        const response = await fetch('http://127.0.0.1:' + server.address().port + path);
        assert.equal(response.status, 404, path);
      }
      const internal = await fetch('http://127.0.0.1:' + server.address().port + '/internal-link.txt');
      assert.equal(internal.status, 200);
      assert.equal(await internal.text(), 'inside');
    } finally {
      server.close();
    }
  });
});

describe('malformed request paths', () => {
  test('returns 400 for an invalid percent escape and keeps serving', async () => {
    writeFileSync(join(dir, 'alive.txt'), 'alive');
    const server = await serve({ '/': dir });
    const port = server.address().port;
    const getRawPath = (path) => new Promise((resolve, reject) => {
      const req = httpRequest({ host: '127.0.0.1', port, path, method: 'GET' }, (res) => {
        res.resume();
        res.on('end', () => resolve(res.statusCode));
      });
      req.on('error', reject);
      req.setTimeout(2000, () => req.destroy(new Error('request timed out')));
      req.end();
    });
    try {
      for (const path of ['/%', '/%FF']) assert.equal(await getRawPath(path), 400, path);
      assert.equal(await getRawPath('/alive.txt'), 200);
    } finally {
      server.close();
    }
  });
});
