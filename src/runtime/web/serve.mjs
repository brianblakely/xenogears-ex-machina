// A static file server for the built page (build/web): localhost is a secure
// context, which Web Audio worklets, WebGPU and the File System Access API
// need. Hosted play needs only static files; this is for local use and tests.
//
//   node src/runtime/web/serve.mjs [dir=build/web] [port=8080]
//
// The repository's serve.json (ignored by Git; without it: localhost, plain
// HTTP, nothing more mounted) can open the server to other devices:
//
//   {"host": "0.0.0.0", "cert": ".local/serve/cert.pem", "key": ".local/serve/key.pem",
//    "mounts": {"/discs/": "discs"}}
//
// Another device reaches the page as a secure context only over HTTPS: `cert`
// and `key` are PEM files (e.g. a self-signed pair its browser is told to
// accept). Each mount serves another directory, e.g. /discs/ for the page's
// ?disc=/discs/<image> (single byte ranges, as the page reads an image).
// Relative paths are from the repository.

import { createServer } from 'node:http';
import { createServer as createSecureServer } from 'node:https';
import { createReadStream, existsSync, readFileSync, statSync } from 'node:fs';
import { dirname, extname, join, normalize, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.wasm': 'application/wasm',
  '.json': 'application/json',
  '.txt': 'text/plain; charset=utf-8',
  '.png': 'image/png',
};

/**
 * Serve `mounts` ({'/': dir, '/fixtures/': other}) on `port` (0: any free port)
 * at `host`, over HTTPS when `tls` ({cert, key}) is given.
 */
export function serve(mounts, port = 0, { host = '127.0.0.1', tls } = {}) {
  const roots = Object.entries(mounts)
    .map(([prefix, dir]) => [prefix, resolve(dir)])
    .sort((a, b) => b[0].length - a[0].length);
  const handler = (request, response) => {
    const path = decodeURIComponent(new URL(request.url, 'http://localhost').pathname);
    const mount = roots.find(([prefix]) => path.startsWith(prefix));
    let file = mount && normalize(join(mount[1], path.slice(mount[0].length)));
    if (!mount || !file.startsWith(mount[1])) {
      response.writeHead(404).end();
      return;
    }
    try {
      if (statSync(file).isDirectory()) file = join(file, 'index.html');
      const size = statSync(file).size;
      const headers = {
        'content-type': TYPES[extname(file)] ?? 'application/octet-stream',
        'accept-ranges': 'bytes',
        'cache-control': 'no-store',
      };
      const range = request.headers.range;
      if (range === undefined) {
        response.writeHead(200, { ...headers, 'content-length': size });
        createReadStream(file).pipe(response);
        return;
      }
      // One range: bytes=first-last, bytes=first- or bytes=-suffix.
      const match = /^bytes=(\d*)-(\d*)$/.exec(range);
      let first = match && match[1] !== '' ? Number(match[1]) : NaN;
      let last = match && match[2] !== '' ? Number(match[2]) : size - 1;
      if (match && match[1] === '' && match[2] !== '') [first, last] = [size - Number(match[2]), size - 1];
      if (!(first >= 0 && first <= last && first < size)) {
        response.writeHead(416, { 'content-range': `bytes */${size}` }).end();
        return;
      }
      last = Math.min(last, size - 1);
      response.writeHead(206, { ...headers, 'content-length': last - first + 1,
        'content-range': `bytes ${first}-${last}/${size}` });
      createReadStream(file, { start: first, end: last }).pipe(response);
    } catch {
      response.writeHead(404).end();
    }
  };
  const server = tls ? createSecureServer(tls, handler) : createServer(handler);
  return new Promise((done) => server.listen(port, host, () => done(server)));
}

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');

/** The settings in `file` (serve.json; none: the defaults): host, TLS and more mounts. */
export function loadConfig(file = join(ROOT, 'serve.json')) {
  if (!existsSync(file)) return { host: '127.0.0.1', tls: undefined, mounts: {} };
  const config = JSON.parse(readFileSync(file, 'utf8'));
  const fail = (message) => { throw new Error(`${file}: ${message}`); };
  for (const key of Object.keys(config)) {
    if (!['host', 'cert', 'key', 'mounts'].includes(key)) fail(`unknown setting ${key}`);
  }
  const { host = '127.0.0.1', cert, key, mounts = {} } = config;
  if (typeof host !== 'string') fail('host must be a string');
  if ((cert === undefined) !== (key === undefined)) fail('cert and key must be given together');
  const path = (name) => resolve(ROOT, name);
  for (const [prefix, dir] of Object.entries(mounts)) {
    if (!/^\/.+\/$/.test(prefix) || typeof dir !== 'string') fail(`mount ${prefix}: expected "/prefix/": "dir"`);
  }
  return {
    host,
    tls: cert === undefined ? undefined : { cert: readFileSync(path(cert)), key: readFileSync(path(key)) },
    mounts: Object.fromEntries(Object.entries(mounts).map(([prefix, dir]) => [prefix, path(dir)])),
  };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const [dir = 'build/web', port = '8080'] = process.argv.slice(2);
  const { host, tls, mounts } = loadConfig();
  const server = await serve({ '/': dir, ...mounts }, Number(port), { host, tls });
  const shown = host === '127.0.0.1' ? 'localhost' : host;
  console.log(`serving ${resolve(dir)} at ${tls ? 'https' : 'http'}://${shown}:${server.address().port}/`);
}
