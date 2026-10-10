// The memory cards: two 128 KB images (the .mcd layout) in IndexedDB. The
// runtime's card device reads and writes them synchronously (its CardStorage
// is an in-memory copy, xem-web storage.rs); the page loads both images
// before a session boots and writes a changed image back after each
// animation frame and automation step. Writes are chained, so IndexedDB gets
// them in order; until one lands, the runtime's copy is the newer one.

import * as storage from './storage.js';

const key = (slot) => `card${slot + 1}`;

export class Cards {
  constructor(app) {
    this.app = app;
    this.writing = Promise.resolve();
    this.pending = 0;
    this.written = 0;
    this.error = null;
  }

  /** Hand both stored images to the runtime (none stored: a fresh formatted card). */
  async load() {
    for (const slot of [0, 1]) this.app.set_card_image(slot, (await storage.get('cards', key(slot))) ?? undefined);
  }

  /** Start writing the images the card device changed; resolves when every write so far landed. */
  flush() {
    for (const slot of [0, 1]) {
      const image = this.app.take_card_write(slot);
      if (!image) continue;
      this.pending++;
      this.writing = this.writing
        .then(() => storage.put('cards', key(slot), image))
        .then(() => {
          this.written++;
        }, (error) => {
          this.error = String(error);
        })
        .finally(() => {
          this.pending--;
        });
    }
    return this.writing;
  }

  /** Put an image (a .mcd's 128 KB) in `slot`: IndexedDB keeps it and the
   * next session boots with it. */
  async store(slot, image) {
    await storage.put('cards', key(slot), image);
    this.app.set_card_image(slot, image);
  }

  /** The image IndexedDB holds for `slot`, or undefined. */
  stored(slot) {
    return storage.get('cards', key(slot));
  }

  status() {
    return { pending: this.pending, written: this.written, error: this.error };
  }
}
