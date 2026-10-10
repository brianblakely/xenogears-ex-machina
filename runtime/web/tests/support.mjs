// Shared test support: the built page served on localhost, headless Chromium
// from the Nix shell, synthetic inputs (a disc image, a game module fixture)
// and PNG decoding for pixel checks. Run inside nix/runtime after
// `python3 runtime/web/build.py`.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { inflateSync } from 'node:zlib';
import { chromium } from 'playwright-core';
import { serve } from '../serve.mjs';

export const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
export const WEB = join(ROOT, 'build', 'web');
/** Screenshots the tests take, for people to look at. */
export const SHOTS = join(ROOT, 'build', 'web-test');
export const USER_DISC = process.env.XEM_DISC1 ?? join(ROOT, 'discs', 'Xenogears disc 1.chd');
export const HEADLESS = join(ROOT, 'runtime', 'target', 'release', 'xem-headless');

/** Chromium flags. WebGPU on SwiftShader's Vulkan: with Chromium's default
 * Vulkan selection the headless browser destroys a WebGPU device after its
 * canvas presents a frame. */
export const WEBGPU = ['--enable-unsafe-webgpu', '--enable-features=Vulkan', '--use-vulkan=swiftshader',
  '--use-angle=swiftshader'];
export const NO_WEBGPU = ['--disable-features=WebGPU'];

let fixtures;
/** The fixture game module, built with wabt and binaryen like the real one. */
export function buildFixtures() {
  if (fixtures) return fixtures;
  fixtures = mkdtempSync(join(tmpdir(), 'xem-web-'));
  const here = dirname(fileURLToPath(import.meta.url));
  execFileSync('wat2wasm', [join(here, 'fixture.wat'), '-o', join(fixtures, 'fixture.raw.wasm')]);
  execFileSync('wasm-opt', ['-O1', '--asyncify', '--pass-arg=asyncify-imports@xem.yield,xem.restart',
    join(fixtures, 'fixture.raw.wasm'), '-o', join(fixtures, 'fixture.wasm')]);
  writeFileSync(join(fixtures, 'stubs.txt'), '0 FixtureStub\n');
  return fixtures;
}

export async function start({ args = [], fixtures: withFixtures = false } = {}) {
  if (!existsSync(join(WEB, 'index.html'))) throw new Error(`${WEB} is missing: run runtime/web/build.py`);
  const mounts = { '/': WEB };
  if (withFixtures) mounts['/fixtures/'] = buildFixtures();
  const server = await serve(mounts);
  const browser = await chromium.launch({ executablePath: process.env.XEM_CHROMIUM, headless: true, args });
  const context = await browser.newContext({ viewport: { width: 1280, height: 900 } });
  const base = `http://localhost:${server.address().port}/`;
  return {
    base,
    context,
    async page(query = '') {
      const page = await context.newPage();
      page.errors = [];
      page.on('pageerror', (error) => page.errors.push(error.message));
      page.on('console', (message) => {
        if (message.type() === 'error' && !message.text().includes('404')) page.errors.push(message.text());
      });
      await page.goto(base + query);
      await page.waitForFunction(() => document.body.dataset.ready === 'true');
      return page;
    },
    async close() {
      await browser.close();
      server.close();
    },
  };
}

export const status = (page) => page.evaluate(() => window.xem.status());

export function shot(name) {
  mkdirSync(SHOTS, { recursive: true });
  return join(SHOTS, name);
}

/** RGBA pixels of an 8-bit RGB or RGBA PNG. */
export function decodePng(png) {
  let offset = 8;
  let width = 0;
  let height = 0;
  let channels = 0;
  const data = [];
  while (offset < png.length) {
    const length = png.readUInt32BE(offset);
    const type = png.toString('ascii', offset + 4, offset + 8);
    const body = png.subarray(offset + 8, offset + 8 + length);
    if (type === 'IHDR') {
      width = body.readUInt32BE(0);
      height = body.readUInt32BE(4);
      if (body[8] !== 8 || ![2, 6].includes(body[9]) || body[12] !== 0) throw new Error('unsupported PNG');
      channels = body[9] === 6 ? 4 : 3;
    } else if (type === 'IDAT') {
      data.push(body);
    }
    offset += 12 + length;
  }
  const raw = inflateSync(Buffer.concat(data));
  const stride = width * channels;
  const rows = Buffer.alloc(stride * height);
  for (let y = 0; y < height; y++) {
    const filter = raw[y * (stride + 1)];
    for (let x = 0; x < stride; x++) {
      const value = raw[y * (stride + 1) + 1 + x];
      const a = x >= channels ? rows[y * stride + x - channels] : 0;
      const b = y > 0 ? rows[(y - 1) * stride + x] : 0;
      const c = x >= channels && y > 0 ? rows[(y - 1) * stride + x - channels] : 0;
      const p = a + b - c;
      const paeth = Math.abs(p - a) <= Math.abs(p - b) && Math.abs(p - a) <= Math.abs(p - c) ? a
        : Math.abs(p - b) <= Math.abs(p - c) ? b : c;
      const predictor = [0, a, b, (a + b) >> 1, paeth][filter];
      rows[y * stride + x] = (value + predictor) & 0xff;
    }
  }
  const rgba = Buffer.alloc(width * height * 4);
  for (let i = 0; i < width * height; i++) {
    for (let k = 0; k < 3; k++) rgba[i * 4 + k] = rows[i * channels + k];
    rgba[i * 4 + 3] = channels === 4 ? rows[i * channels + 3] : 255;
  }
  return { width, height, rgba };
}

/** Distinct colours and the share of non-black pixels in a screenshot. */
export function pixelStats(png) {
  const { width, height, rgba } = decodePng(png);
  const colours = new Set();
  let lit = 0;
  for (let i = 0; i < width * height; i++) {
    const [r, g, b] = rgba.subarray(i * 4, i * 4 + 3);
    colours.add((r << 16) | (g << 8) | b);
    if (r + g + b > 30) lit++;
  }
  return { width, height, colours: colours.size, lit: lit / (width * height) };
}

// --- A synthetic disc: ISO9660 with SYSTEM.CNF and a small PS-X EXE ------------

const SECTOR = 2352;

function bcd(value) {
  return ((value / 10) << 4) | value % 10;
}

function sector(lba, data) {
  const out = Buffer.alloc(SECTOR);
  out.fill(0xff, 1, 11);
  const frames = lba + 150;
  out[12] = bcd(Math.floor(frames / 4500));
  out[13] = bcd(Math.floor(frames / 75) % 60);
  out[14] = bcd(frames % 75);
  out[15] = 2;
  for (const at of [16, 20]) out[at + 2] = 0x08; // subheader: data, Form 1
  data.copy(out, 24);
  return out;
}

function record(name, lba, size, directory) {
  const nameBytes = Buffer.from(name, 'latin1');
  const length = 33 + nameBytes.length + ((nameBytes.length + 1) % 2);
  const r = Buffer.alloc(length);
  r[0] = length;
  r.writeUInt32LE(lba, 2);
  r.writeUInt32BE(lba, 6);
  r.writeUInt32LE(size, 10);
  r.writeUInt32BE(size, 14);
  r[25] = directory ? 2 : 0;
  r[32] = nameBytes.length;
  nameBytes.copy(r, 33);
  return r;
}

/** A raw MODE2/2352 image whose boot program is not a Xenogears one. */
export function syntheticDisc() {
  const exe = Buffer.alloc(0x800 + 0x1000);
  exe.write('PS-X EXE', 0, 'latin1');
  exe.writeUInt32LE(0x80010000, 0x10);
  exe.writeUInt32LE(0x80010000, 0x18);
  exe.writeUInt32LE(0x1000, 0x1c);
  for (let i = 0x800; i < exe.length; i++) exe[i] = (i * 7) & 0xff;
  const cnf = Buffer.from('BOOT = cdrom:\\SLUS_999.99;1\r\nTCB = 4\r\n', 'latin1');
  const user = Array.from({ length: 24 }, () => Buffer.alloc(2048));
  const root = Buffer.concat([
    record('\x00', 18, 2048, true),
    record('\x01', 18, 2048, true),
    record('SLUS_999.99;1', 20, exe.length, false),
    record('SYSTEM.CNF;1', 19, cnf.length, false),
  ]);
  user[16].write('\x01CD001', 0, 'latin1');
  record('\x00', 18, 2048, true).copy(user[16], 156);
  root.copy(user[18]);
  cnf.copy(user[19]);
  for (let n = 0; n * 2048 < exe.length; n++) exe.subarray(n * 2048, (n + 1) * 2048).copy(user[20 + n]);
  const image = Buffer.concat(user.map((data, lba) => sector(lba, data)));
  return { image, exeSha256: createHash('sha256').update(exe).digest('hex') };
}
