// A static file server for the built page (build/web): localhost is a secure
// context, which Web Audio worklets, WebGPU and the File System Access API
// need. Hosted play needs only static files; this is for local use and tests.
//
//   node runtime/web/serve.mjs [dir=build/web] [port=8080]

import { createServer } from 'node:http';
import { createReadStream, statSync } from 'node:fs';
import { extname, join, normalize, resolve } from 'node:path';
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

/** Serve `mounts` ({'/': dir, '/fixtures/': other}) on `port` (0: any free port). */
export function serve(mounts, port = 0) {
  const roots = Object.entries(mounts)
    .map(([prefix, dir]) => [prefix, resolve(dir)])
    .sort((a, b) => b[0].length - a[0].length);
  const server = createServer((request, response) => {
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
      response.writeHead(200, {
        'content-type': TYPES[extname(file)] ?? 'application/octet-stream',
        'content-length': size,
        'cache-control': 'no-store',
      });
      createReadStream(file).pipe(response);
    } catch {
      response.writeHead(404).end();
    }
  });
  return new Promise((done) => server.listen(port, '127.0.0.1', () => done(server)));
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const [dir = 'build/web', port = '8080'] = process.argv.slice(2);
  const server = await serve({ '/': dir }, Number(port));
  console.log(`serving ${resolve(dir)} at http://localhost:${server.address().port}/`);
}
