// Shared test support: the built page served on localhost, headless Chromium
// from the Nix shell, synthetic inputs (a disc image, a game module fixture)
// and PNG decoding for pixel checks. Run inside nix/runtime after
// `python3 src/runtime/web/build.py`.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { inflateSync } from 'node:zlib';
import { chromium } from 'playwright-core';
import { serve } from '../serve.mjs';

export const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../../..');
export const WEB = join(ROOT, 'build', 'web');
/** Screenshots the tests take, for people to look at. */
export const SHOTS = join(ROOT, 'build', 'web-test');
export const USER_DISC = process.env.XEM_DISC1 ?? join(ROOT, 'discs', 'Xenogears disc 1.chd');
export const USER_DISC2 = process.env.XEM_DISC2 ?? join(ROOT, 'discs', 'Xenogears disc 2.chd');
// The native host the tests compare with (XEM_HEADLESS: another build's, as the test suite's).
export const HEADLESS = process.env.XEM_HEADLESS ?? join(ROOT, 'src', 'runtime', 'target', 'release', 'xem-headless');

/** Chromium flags. WebGPU on SwiftShader's Vulkan: with Chromium's default
 * Vulkan selection the headless browser destroys a WebGPU device after its
 * canvas presents a frame. */
export const WEBGPU = ['--enable-unsafe-webgpu', '--enable-features=Vulkan', '--use-vulkan=swiftshader',
  '--use-angle=swiftshader'];
export const NO_WEBGPU = ['--disable-features=WebGPU'];

let fixtures;
/** The fixture game module, built with wabt and binaryen like the real one,
 *  and fixture-b.wasm, the same module with a custom section more: another
 *  build, whose states another build's page refuses. */
export function buildFixtures() {
  if (fixtures) return fixtures;
  fixtures = mkdtempSync(join(tmpdir(), 'xem-web-'));
  const here = dirname(fileURLToPath(import.meta.url));
  execFileSync('wat2wasm', [join(here, 'fixture.wat'), '-o', join(fixtures, 'fixture.raw.wasm')]);
  execFileSync('wasm-opt', ['-O1', '--asyncify', '--pass-arg=asyncify-imports@xem.yield,xem.restart',
    join(fixtures, 'fixture.raw.wasm'), '-o', join(fixtures, 'fixture.wasm')]);
  // A custom section (id 0): its size, then its name's length and name, then its bytes.
  const name = Buffer.from('xem-build', 'latin1');
  const section = Buffer.concat([Buffer.from([name.length]), name, Buffer.from('b', 'latin1')]);
  writeFileSync(join(fixtures, 'fixture-b.wasm'),
    Buffer.concat([readFileSync(join(fixtures, 'fixture.wasm')), Buffer.from([0, section.length]), section]));
  writeFileSync(join(fixtures, 'stubs.txt'), '0 FixtureStub\n');
  return fixtures;
}

export async function start({ args = [], fixtures: withFixtures = false, context: contextOptions = {}, initScripts = [] } = {}) {
  if (!existsSync(join(WEB, 'index.html'))) throw new Error(`${WEB} is missing: run src/runtime/web/build.py`);
  const mounts = { '/': WEB };
  if (withFixtures) mounts['/fixtures/'] = buildFixtures();
  const server = await serve(mounts);
  const browser = await chromium.launch({ executablePath: process.env.XEM_CHROMIUM, headless: true, args });
  const context = await browser.newContext({ viewport: { width: 1280, height: 900 }, ...contextOptions });
  // The suite runs browsers side by side: a page can take longer than
  // Playwright's 30 s to start on a loaded machine (a hang still fails).
  context.setDefaultTimeout(120_000);
  for (const script of initScripts) await context.addInitScript(script);
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

// --- Headset browsers: Quest Browser's user agent with IWER as navigator.xr --
export const QUEST_BROWSER = 'Mozilla/5.0 (X11; Linux x86_64; Quest 3) AppleWebKit/537.36 (KHTML, like Gecko) '
  + 'OculusBrowser/37.0.0.0 SamsungBrowser/4.0 Chrome/132.0.0.0 VR Safari/537.36';
const IWER_PATH = join(ROOT, 'src/runtime/web/node_modules/iwer/build/iwer.min.js');
export const IWER = existsSync(IWER_PATH) ? readFileSync(IWER_PATH, 'utf8') : '';
/** An init script making IWER's Meta Quest 3 the page's navigator.xr. */
export const QUEST = `${IWER}\n;window.xrDevice = new IWER.XRDevice(IWER.metaQuest3, { stereoEnabled: true });`
  + 'window.xrDevice.installRuntime({ forceInstall: true });';

/** `v` turned by the quaternion `q` (x, y, z, w). */
export function rotate([x, y, z, w], v) {
  const t = [2 * (y * v[2] - z * v[1]), 2 * (z * v[0] - x * v[2]), 2 * (x * v[1] - y * v[0])];
  return [v[0] + w * t[0] + (y * t[2] - z * t[1]), v[1] + w * t[1] + (z * t[0] - x * t[2]), v[2] + w * t[2] + (x * t[1] - y * t[0])];
}

// The hand model in the page (an init script after QUEST): a right hand in
// its own frame (wrist at the origin, fingers along -Z, palm down), each
// finger curled by `curl`, the thumb's tip at `thumb`; poses are registered
// with IWER as left-hand data (IWER mirrors them for the right hand). The
// shapes: open, half, fist and gather (a whole-hand pinch: every fingertip
// at the thumb's). IWER draws a hand's joints toward its pinch pose by the
// pinch value; `pinchPose(device, name)` makes that pose one of the
// model's (a runtime reporting its pinch while the fingertips are
// gathered), and `pinchPose(device)` gives IWER's own back.
export const HAND_MODEL = `
window.handModel = (() => {
  const JOINTS = ['wrist', 'thumb-metacarpal', 'thumb-phalanx-proximal', 'thumb-phalanx-distal', 'thumb-tip'];
  for (const f of ['index-finger', 'middle-finger', 'ring-finger', 'pinky-finger'])
    for (const j of ['metacarpal', 'phalanx-proximal', 'phalanx-intermediate', 'phalanx-distal', 'tip']) JOINTS.push(f + '-' + j);
  const right = (curl, thumb) => {
    const joints = Array.from({ length: 25 }, () => [0, 0, 0]);
    const lateral = [-0.035, -0.012, 0.01, 0.03];
    const bones = [[0.065, 0.04, 0.025, 0.022], [0.064, 0.045, 0.028, 0.024], [0.06, 0.042, 0.027, 0.024], [0.056, 0.034, 0.02, 0.02]];
    for (let f = 0; f < 4; f++) {
      const base = 5 + f * 5;
      joints[base] = [lateral[f] * 0.6, 0, 0];
      let angle = 0;
      let at = [lateral[f], 0, -bones[f][0]];
      joints[base + 1] = at;
      for (let k = 0; k < 3; k++) {
        angle += curl[f] * [1.4, 1.6, 1.2][k];
        at = [at[0], at[1] - Math.sin(angle) * bones[f][k + 1], at[2] - Math.cos(angle) * bones[f][k + 1]];
        joints[base + 2 + k] = at;
      }
    }
    joints[1] = [-0.02, -0.01, -0.02];
    joints[2] = [-0.04, -0.015, -0.04];
    joints[3] = joints[2].map((v, i) => (v + thumb[i]) / 2);
    joints[4] = thumb;
    return joints;
  };
  const rest = [-0.06, -0.01, -0.06];
  const fist = () => {
    const j = right([1, 1, 1, 1], [0, 0, 0]);
    j[4] = [j[12][0] - 0.01, j[12][1] - 0.012, j[12][2]];
    j[3] = j[2].map((v, i) => (v + j[4][i]) / 2);
    return j;
  };
  // Every fingertip gathered towards the middle of the index's and the
  // little finger's (xem_assist::synthetic::whole_pinch).
  const gather = () => {
    const j = right([0.45, 0.45, 0.45, 0.45], rest);
    const centre = j[9].map((v, k) => (v + j[24][k]) / 2);
    for (const tip of [4, 9, 14, 19, 24]) j[tip] = j[tip].map((v, k) => centre[k] + (v - centre[k]) * 0.15);
    return j;
  };
  const shapes = { open: right([0, 0, 0, 0], rest), half: right([0.5, 0.5, 0.5, 0.5], rest), fist: fist(), gather: gather() };
  const pose = (joints) => ({
    jointTransforms: Object.fromEntries(JOINTS.map((name, i) => {
      const [x, y, z] = joints[i];
      // Left-hand data: the model's x mirrored.
      return [name, { offsetMatrix: new Float32Array([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x, y, z, 1]), radius: 0.008 }];
    })),
  });
  const handInput = (hand) => hand[Object.getOwnPropertySymbols(hand).find((s) => s.description === '@iwer/xr-hand-input')];
  const own = new Map();
  return {
    install(device) {
      for (const hand of [device.hands.left, device.hands.right]) {
        const poses = handInput(hand).poses;
        if (!own.has(hand)) own.set(hand, poses.pinch);
        for (const [name, joints] of Object.entries(shapes)) poses[name] = pose(joints);
      }
    },
    set(device, hand, shape) {
      device.hands[hand].poseId = shape;
      device.hands[hand].updateHandPose();
    },
    pinchPose(device, shape) {
      for (const hand of [device.hands.left, device.hands.right]) {
        handInput(hand).poses.pinch = shape ? pose(shapes[shape]) : own.get(hand);
      }
    },
  };
})();`;

/** Select a section through the rendered Slint navigation at 1× scale: the
 *  sidebar's rows (38 high, 40 apart from 75 under the panel's top). A
 *  narrow panel's strip sizes its tabs by their labels, so it is refused. */
export async function settingsSection(page, index) {
  const canvas = await page.locator('canvas').boundingBox();
  const width = Math.min(640, canvas.width - 16), height = Math.min(560, canvas.height - 16);
  const left = canvas.x + (canvas.width - width) / 2, top = canvas.y + (canvas.height - height) / 2;
  if (width < 560) throw new Error(`settingsSection needs the sidebar: the panel is ${width} wide`);
  await page.mouse.click(left + 52, top + 94 + index * 40);
  await page.waitForFunction((index) => window.xem.status().renderer.panelSection === index, index);
  return { left, top, width, height };
}

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
