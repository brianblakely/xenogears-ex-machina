// Test plumbing: a static server for runtime/web, Chromium through
// playwright-core, PNG decoding for screenshots, and small assertions.

import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize, resolve } from 'node:path';
import { inflateSync } from 'node:zlib';
import { chromium } from 'playwright-core';

const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.mjs': 'text/javascript; charset=utf-8',
  '.json': 'application/json',
  '.map': 'application/json',
  '.wasm': 'application/wasm',
};

/** Serves `root` on 127.0.0.1 (a secure context) at a free port. */
export async function serve(root) {
  root = resolve(root);
  const server = createServer(async (request, response) => {
    const path = normalize(decodeURIComponent(new URL(request.url, 'http://x').pathname));
    const file = join(root, path.endsWith('/') ? `${path}index.html` : path);
    if (!file.startsWith(root)) {
      response.writeHead(403).end();
      return;
    }
    try {
      const body = await readFile(file);
      response.writeHead(200, {
        'content-type': TYPES[extname(file)] ?? 'application/octet-stream',
        'cache-control': 'no-store',
      });
      response.end(body);
    } catch {
      response.writeHead(404).end();
    }
  });
  await new Promise((done) => server.listen(0, '127.0.0.1', done));
  return { url: `http://127.0.0.1:${server.address().port}`, close: () => server.close() };
}

export async function launch(extraArgs = []) {
  const executablePath = process.env.XEM_CHROMIUM;
  if (!executablePath) throw new Error('XEM_CHROMIUM is not set (run inside nix develop path:./nix/runtime)');
  // Headless Chromium has no GPU here: WebGL2 runs on SwiftShader.
  return chromium.launch({
    executablePath,
    args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist', ...extraArgs],
  });
}

/** Opens `url` and waits for the page's `window.xemXr` hooks. */
export async function openPage(browser, url, { initScript } = {}) {
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });
  const log = [];
  page.on('console', (message) => log.push(`[${message.type()}] ${message.text()}`));
  page.on('pageerror', (error) => log.push(`[pageerror] ${error.message}`));
  if (initScript) await page.addInitScript(initScript);
  await page.goto(url);
  await page.waitForFunction(() => window.xemXr?.ready, null, { timeout: 30000 });
  const error = await page.evaluate(() => window.xemXr.error);
  if (error) throw new Error(`page failed to start: ${error}\n${log.join('\n')}`);
  page.xemLog = log;
  return page;
}

export const state = (page) => page.evaluate(() => window.xemXr.state());

/** Polls the app state until `predicate(state)` holds. */
export async function until(page, predicate, what, timeout = 15000) {
  const start = Date.now();
  let last;
  while (Date.now() - start < timeout) {
    last = await state(page);
    if (predicate(last)) return last;
    await page.waitForTimeout(50);
  }
  throw new Error(`timed out waiting for ${what}; last state: ${JSON.stringify(last).slice(0, 2000)}`);
}

export function check(condition, message) {
  if (!condition) throw new Error(message);
}

export function near(a, b, epsilon, message) {
  check(Math.abs(a - b) <= epsilon, `${message}: ${a} vs ${b}`);
}

/** Decodes an 8-bit, non-interlaced RGB or RGBA PNG into RGBA pixels. */
export function decodePng(buffer) {
  let offset = 8;
  let width = 0;
  let height = 0;
  let channels = 0;
  const data = [];
  while (offset < buffer.length) {
    const length = buffer.readUInt32BE(offset);
    const type = buffer.toString('latin1', offset + 4, offset + 8);
    const body = buffer.subarray(offset + 8, offset + 8 + length);
    if (type === 'IHDR') {
      width = body.readUInt32BE(0);
      height = body.readUInt32BE(4);
      const [depth, color, , , interlace] = body.subarray(8);
      channels = { 2: 3, 6: 4 }[color];
      if (depth !== 8 || !channels || interlace) throw new Error('unsupported PNG layout');
    } else if (type === 'IDAT') {
      data.push(body);
    }
    offset += 12 + length;
  }
  const raw = inflateSync(Buffer.concat(data));
  const stride = width * channels;
  const pixels = Buffer.alloc(width * height * 4);
  const previous = Buffer.alloc(stride);
  const line = Buffer.alloc(stride);
  for (let y = 0; y < height; y++) {
    const filter = raw[y * (stride + 1)];
    const source = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let i = 0; i < stride; i++) {
      const a = i >= channels ? line[i - channels] : 0;
      const b = previous[i];
      const c = i >= channels ? previous[i - channels] : 0;
      let predictor = 0;
      if (filter === 1) predictor = a;
      else if (filter === 2) predictor = b;
      else if (filter === 3) predictor = (a + b) >> 1;
      else if (filter === 4) {
        const p = a + b - c;
        const pa = Math.abs(p - a);
        const pb = Math.abs(p - b);
        const pc = Math.abs(p - c);
        predictor = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
      }
      line[i] = (source[i] + predictor) & 0xff;
    }
    for (let x = 0; x < width; x++) {
      for (let k = 0; k < 4; k++) {
        pixels[(y * width + x) * 4 + k] = k < channels ? line[x * channels + k] : 255;
      }
    }
    line.copy(previous);
  }
  return { width, height, pixels };
}

/** The RGBA pixel at (x, y) (origin top-left) of a decoded image. */
export function pixel(image, x, y) {
  const i = (Math.floor(y) * image.width + Math.floor(x)) * 4;
  return [...image.pixels.subarray(i, i + 4)];
}

/** Mean RGB over a rectangle of a decoded image. */
export function mean(image, x0, y0, x1, y1) {
  const sum = [0, 0, 0];
  let count = 0;
  for (let y = Math.floor(y0); y < y1; y += 2) {
    for (let x = Math.floor(x0); x < x1; x += 2) {
      const p = pixel(image, x, y);
      sum[0] += p[0];
      sum[1] += p[1];
      sum[2] += p[2];
      count++;
    }
  }
  return sum.map((v) => v / count);
}

/** Sky pixels of the test scene are clearly blue; floor pixels are grey. */
export const isSky = ([r, , b]) => b > 120 && b > r + 40;
export const isFloor = ([r, g, b]) => Math.abs(b - r) < 40 && Math.abs(g - r) < 40 && r < 200;

/** Column-major 4x4 product. */
export function mul(a, b) {
  const out = new Array(16).fill(0);
  for (let c = 0; c < 4; c++) {
    for (let r = 0; r < 4; r++) {
      for (let k = 0; k < 4; k++) out[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    }
  }
  return out;
}

export const GL_TO_WGPU_DEPTH = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0.5, 0, 0, 0, 0.5, 1];
