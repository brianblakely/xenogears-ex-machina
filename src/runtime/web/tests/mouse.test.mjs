// Mouse clicks in browsers (local only: it needs the user's disc 1, the game
// module and a native xem-headless to make the field's state). In Lahan (map
// 1), with the page running the game: a left click where the picture shows
// walkable floor away from everyone walks there (the route's goal is the
// floor `pick` finds at that screen point) and holds no pad button of the
// mouse's binding (Confirm) while it is down; with Turn the camera with the
// mouse on, the same click is the camera's (the page takes the mouse with
// it) and walks nowhere.

import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, test } from 'node:test';
import { HEADLESS, NO_WEBGPU, SHOTS, USER_DISC, WEB, start, status } from './support.mjs';

const available = existsSync(USER_DISC) && existsSync(join(WEB, 'game', 'game.wasm')) && existsSync(HEADLESS);
mkdirSync(SHOTS, { recursive: true });

/** Animation frames of the page (each one a frame of its loop). */
const frames = (page, count) => page.evaluate(async (count) => {
  for (let i = 0; i < count; i++) await new Promise(requestAnimationFrame);
}, count);
const command = (page, cmd) => page.evaluate((cmd) => window.xem.command(cmd), cmd);
const pads = async (page) => (await command(page, { cmd: 'status' })).pads[0].buttons;

describe('mouse clicks in browsers', { skip: !available && `needs ${USER_DISC}, the game module and ${HEADLESS}` }, () => {
  test('a click on the floor walks there, and under the mouse camera it is the camera\'s', { timeout: 600_000 }, async () => {
    const state = join(SHOTS, 'mouse-lahan.state');
    execFileSync(HEADLESS, ['--disc', USER_DISC, '--control'], {
      input: [{ cmd: 'enter_field', map: 1 }, { cmd: 'frames', count: 400 }, { cmd: 'state_save', path: state }]
        .map((c) => JSON.stringify(c)).join('\n') + '\n',
    });
    const browser = await start({ args: NO_WEBGPU });
    try {
      const page = await browser.page();
      await page.setInputFiles('#file', USER_DISC);
      await page.waitForFunction(() => window.xem.status().disc?.state === 'identified', null, { timeout: 60_000 });
      await page.evaluate(async (base64) => {
        await window.xem.boot();
        window.xem.importSnapshot(Uint8Array.from(atob(base64), (c) => c.charCodeAt(0)));
        window.xem.setRunning(true);
        document.activeElement?.blur();
      }, readFileSync(state).toString('base64'));
      await page.waitForFunction(() => window.xem.command({ cmd: 'context' }).kind === 'walk');
      await frames(page, 10);

      // Walkable floor some way off Fei that the picture shows, with no one
      // within a click's reach of it (those standing where Fei stands, the
      // party with him, are never clicked on).
      const target = await page.evaluate(() => {
        const field = window.xem.command({ cmd: 'field' });
        const me = field.actors.find((actor) => actor.index === field.player).position;
        const planar = (a, b) => Math.hypot(a.x - b.x, a.z - b.z);
        const others = field.actors.filter((actor) => actor.index !== field.player && planar(actor.position, me) > 2);
        for (let sy = 32; sy <= 208; sy += 4) {
          for (let sx = 32; sx <= 288; sx += 4) {
            const hit = window.xem.command({ cmd: 'pick', screen: [sx, sy] });
            const away = hit && planar(hit.point, me);
            if (hit?.reach !== 'walk' || away < 60 || away > 240) continue;
            if (others.some((actor) => planar(actor.position, hit.point) < 40
                || (actor.screen && Math.hypot(actor.screen[0] - sx, actor.screen[1] - sy) < 48))) continue;
            return { screen: [sx, sy], point: hit.point };
          }
        }
        return null;
      });
      assert.ok(target, 'walkable floor the picture shows, away from everyone');
      // Where the canvas shows that screen point (CSS pixels).
      const { renderer } = await status(page);
      const picture = renderer.picture;
      const box = await page.locator('canvas').boundingBox();
      const u = (target.screen[0] + picture.side) / (picture.display[0] + 2 * picture.side);
      const x = picture.x + u * picture.width, y = picture.y + target.screen[1] / picture.display[1] * picture.height;
      const at = [box.x + x * box.width / renderer.size[0], box.y + y * box.height / renderer.size[1]];

      await page.mouse.move(...at);
      await page.mouse.down();
      await frames(page, 3);
      assert.ok(!(await pads(page)).includes('cross'), 'the click holds no Confirm');
      await page.mouse.up();
      await page.waitForFunction(() => window.xem.status().session.assist?.intent?.kind === 'walk');
      const { to } = (await status(page)).session.assist.intent;
      assert.ok(Math.hypot(to.x - target.point.x, to.z - target.point.z) < 16,
        `the click walks to the floor the picture shows there: ${JSON.stringify(to)} for ${JSON.stringify(target.point)}`);
      await command(page, { cmd: 'assist_stop' });
      await frames(page, 3);
      assert.equal((await status(page)).session.assist, null, 'the route stopped');

      // Under the mouse camera the click is the camera's: it walks nowhere.
      await page.evaluate(() => window.xem.settings.apply({ MouseCamera: true }));
      await frames(page, 3);
      await page.mouse.click(...at);
      await frames(page, 10);
      assert.equal((await status(page)).session.assist, null, 'under the mouse camera a click walks nowhere');
      assert.deepEqual(page.errors, []);
    } finally {
      await browser.close();
    }
  });
});
