// The user's disc image, read in place: the runtime asks for byte ranges, the
// page reads them with Blob.slice().arrayBuffer() and hands them over. Only the
// chunks the runtime's bounded cache holds are in memory at once.

import * as storage from './storage.js';

async function fetchRanges(app, file, ranges) {
  for (const [start, end] of ranges) {
    const bytes = new Uint8Array(await file.slice(start, end).arrayBuffer());
    app.disc_fill(start, bytes);
  }
}

/** Open and identify `file`; resolves to the runtime's disc status. */
export async function importDisc(app, file) {
  app.open_disc(file.name, file.size);
  for (;;) {
    const ranges = JSON.parse(app.disc_poll());
    if (ranges.length === 0) break;
    await fetchRanges(app, file, ranges);
  }
  const status = JSON.parse(app.disc_status());
  console.info(
    `xem: disc ${status.name}: ${status.state}; chunk budget ${status.chunk_budget} B ` +
      `(${status.chunk_bytes} B chunks), peak resident ${status.peak_resident_bytes} B, ` +
      `fetched ${status.fetched_bytes} B in ${status.fetches} reads`,
  );
  return status;
}

/** Raw 2352-byte sectors of the identified disc. */
export async function readSectors(app, file, lba, count) {
  for (;;) {
    const data = app.disc_read(lba, count);
    if (data) return data;
    const ranges = JSON.parse(app.disc_missing());
    if (ranges.length === 0) throw new Error('disc read pending without missing bytes');
    await fetchRanges(app, file, ranges);
  }
}

export const fileSystemAccess = typeof window.showOpenFilePicker === 'function';

/** Pick an image with the File System Access API and remember its handle. */
export async function pickWithHandle() {
  const [handle] = await window.showOpenFilePicker({
    types: [{ description: 'PlayStation disc image', accept: { 'application/octet-stream': ['.chd', '.bin'] } }],
  });
  await storage.put('handles', 'disc', handle);
  return handle.getFile();
}

/** The remembered handle's file, asking for read permission (needs a user gesture). */
export async function reopenRemembered() {
  const handle = await storage.get('handles', 'disc');
  if (!handle) return null;
  if ((await handle.queryPermission({ mode: 'read' })) !== 'granted' &&
      (await handle.requestPermission({ mode: 'read' })) !== 'granted') {
    throw new Error('read permission for the remembered disc was denied');
  }
  return handle.getFile();
}

export async function rememberedName() {
  if (!fileSystemAccess) return null;
  try {
    return (await storage.get('handles', 'disc'))?.name ?? null;
  } catch {
    return null;
  }
}
