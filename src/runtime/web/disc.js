// The user's disc images, read in place: the runtime asks for byte ranges of
// each import (by its id), the page reads them with Blob.slice().arrayBuffer()
// and hands them over. Only the chunks the runtime's bounded caches hold are
// in memory at once.

import * as storage from './storage.js';

/** The imported files by the runtime's import id. */
const files = new Map();

/** Fetch `[[id, start, end], ...]` from the imports' files. */
async function fetchRanges(app, ranges) {
  for (const [id, start, end] of ranges) {
    const file = files.get(id);
    if (!file) throw new Error(`no file for disc import ${id}`);
    const bytes = new Uint8Array(await file.slice(start, end).arrayBuffer());
    app.disc_fill(id, start, bytes);
  }
}

/** Open, identify and (a Xenogears disc) verify `file`; resolves to the
 * runtime's disc status. */
export async function importDisc(app, file) {
  const id = app.open_disc(file.name, file.size);
  files.set(id, file);
  for (;;) {
    const ranges = JSON.parse(app.disc_poll(id));
    if (ranges.length === 0) break;
    await fetchRanges(app, ranges.map(([start, end]) => [id, start, end]));
  }
  const status = JSON.parse(app.disc_status());
  console.info(
    `xem: disc ${status.name}: ${status.state}; chunk budget ${status.chunk_budget} B ` +
      `(${status.chunk_bytes} B chunks), peak resident ${status.peak_resident_bytes} B, ` +
      `fetched ${status.fetched_bytes} B in ${status.fetches} reads`,
  );
  return status;
}

/** Raw 2352-byte sectors of the newest identified image. */
export async function readSectors(app, lba, count) {
  for (;;) {
    const data = app.disc_read(lba, count);
    if (data) return data;
    const ranges = JSON.parse(app.disc_missing());
    if (ranges.length === 0) throw new Error('disc read pending without missing bytes');
    await fetchRanges(app, ranges);
  }
}

/** Fetch the ranges the game's own disc reads wait for (the runtime stops a
 * frame, not its clock, until they arrive), from the animation loop. One
 * fetch runs at a time. */
let serving = null;
export function serveGameReads(app) {
  if (serving || files.size === 0) return;
  const ranges = JSON.parse(app.disc_missing());
  if (ranges.length === 0) return;
  serving = fetchRanges(app, ranges).finally(() => {
    serving = null;
  });
}

/** Fetch everything the game's disc reads wait for, including a fetch the
 * animation loop started; resolves once nothing is missing. */
export async function fillGameReads(app) {
  if (files.size === 0) throw new Error('no disc is open');
  for (;;) {
    if (serving) await serving;
    const ranges = JSON.parse(app.disc_missing());
    if (ranges.length === 0) return;
    await fetchRanges(app, ranges);
  }
}

export const fileSystemAccess = typeof window.showOpenFilePicker === 'function';
/** The image kinds the file choosers offer. */
export const IMAGE_TYPES = ['.chd', '.bin', '.iso'];

/** Pick an image with the File System Access API: its file and handle. */
export async function pickWithHandle() {
  const [handle] = await window.showOpenFilePicker({
    types: [{ description: 'PlayStation disc image', accept: { 'application/octet-stream': IMAGE_TYPES } }],
  });
  return { file: await handle.getFile(), handle };
}

/** Remember `handle` as Xenogears disc `number`'s. */
export async function remember(number, handle) {
  await storage.put('handles', `disc${number}`, handle);
}

/** The remembered discs' handles, disc 1 first. */
async function remembered() {
  const handles = [];
  for (const number of [1, 2]) {
    const handle = await storage.get('handles', `disc${number}`);
    if (handle) handles.push(handle);
  }
  return handles;
}

/** The remembered discs' files, asking for read permission (needs a user gesture). */
export async function reopenRemembered() {
  const reopened = [];
  for (const handle of await remembered()) {
    if ((await handle.queryPermission({ mode: 'read' })) !== 'granted' &&
        (await handle.requestPermission({ mode: 'read' })) !== 'granted') {
      throw new Error(`read permission for ${handle.name} was denied`);
    }
    reopened.push(await handle.getFile());
  }
  return reopened;
}

/** The remembered discs' names. */
export async function rememberedNames() {
  if (!fileSystemAccess) return [];
  try {
    return (await remembered()).map((handle) => handle.name);
  } catch {
    return [];
  }
}
