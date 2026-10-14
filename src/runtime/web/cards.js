// Changed images stay here until IndexedDB acknowledges their transaction.
// An error retains the latest image for explicit retry and export backup.
// Each card is `cardN`; the card a change from Settings replaced (an import,
// a delete, a restore) is `cardN-previous`.
import * as storage from './storage.js';
const key = (slot) => `card${slot + 1}`;
const previousKey = (slot) => `${key(slot)}-previous`;
const check = (slot, image) => {
  if (![0, 1].includes(slot) || !(image instanceof Uint8Array) || image.length !== 0x20000) {
    throw new Error('Card slot is 0 or 1; image must be exactly 128 KiB (.mcd).');
  }
};

export class Cards {
  constructor(app) {
    this.app = app;
    this.writing = null;
    this.images = new Map();
    this.written = 0;
    this.error = null;
    this.notice = null;
    this.missing = new Set();
  }

  /** Hand both stored images and their previous cards to the runtime (none
   * stored: a fresh formatted card, no previous card). */
  async load() {
    this.missing.clear();
    for (const slot of [0, 1]) {
      const image = await storage.get('cards', key(slot));
      const previous = await storage.get('cards', previousKey(slot));
      if (image) check(slot, image);
      else this.missing.add(slot);
      if (previous) check(slot, previous);
      this.app.set_card_image(slot, image ?? undefined, previous ?? undefined);
    }
    this.updateNotice();
  }

  updateNotice() {
    const slots = [...this.missing].map((slot) => slot + 1);
    this.notice = slots.length ? `No stored card ${slots.join(' or ')}. Fresh cards are in use. Browser saves may have been cleared or evicted; import a backup if needed.` : null;
  }

  /** Start writing the images the card device changed; resolves when every write so far landed. */
  collect() {
    for (const slot of [0, 1]) {
      const image = this.app.take_card_write(slot);
      if (image) this.images.set(key(slot), image);
      const previous = this.app.take_previous_card_write(slot);
      if (previous) this.images.set(previousKey(slot), previous);
    }
  }
  flush({ retry = false } = {}) {
    this.collect();
    if (retry) this.error = null;
    if (this.writing) return this.writing;
    if (this.error) return Promise.reject(new Error(this.error));
    this.writing = (async () => {
      while (this.images.size) {
        const [name, image] = this.images.entries().next().value;
        try {
          await storage.put('cards', name, image);
          if (this.images.get(name) === image) this.images.delete(name);
          this.written++;
          for (const slot of [0, 1]) if (name === key(slot)) this.missing.delete(slot);
          this.updateNotice();
        } catch (error) {
          this.error = String(error);
          throw error;
        }
      }
    })().finally(() => { this.writing = null; });
    return this.writing;
  }

  /** A change of a card from Settings or automation, between frames: play
   * pauses, pending writes land first, the runtime applies it (`operation`,
   * which returns its JSON reply) and both the card and the card it
   * replaced are in IndexedDB before it resolves. */
  async change(operation) {
    const running = JSON.parse(this.app.status()).running;
    this.app.set_running(false);
    try {
      await this.flush({ retry: true });
      const reply = JSON.parse(operation());
      await this.flush();
      return reply;
    } finally {
      if (running) this.app.set_running(true);
    }
  }

  /** Import a file into card `slot`: a whole card (any supported container)
   * replaces it, a single save joins it. */
  importFile(slot, bytes, name) {
    return this.change(() => this.app.card_import_file(slot, bytes, name));
  }

  /** Put a raw card image (Uint8Array, exactly 128 KiB, formatted or not)
   * in card `slot`. */
  store(slot, image) {
    check(slot, image);
    return this.change(() => this.app.card_store(slot, image));
  }

  /** Replace card `slot` with the card a file holds (raw, .gme, .vgs, .vmp). */
  importCard(slot, bytes, name) {
    return this.change(() => this.app.card_import_card(slot, bytes, name));
  }

  /** Add a save to card `slot`: a single save's file's, or with `saveName`
   * the save of that name a whole card's file holds. */
  importSave(slot, bytes, name, saveName) {
    return this.change(() => this.app.card_import_save(slot, bytes, name, saveName ?? undefined));
  }

  deleteSave(slot, name) {
    return this.change(() => this.app.save_delete(slot, name));
  }

  restorePrevious(slot) {
    return this.change(() => this.app.card_restore_previous(slot));
  }

  /** What card `slot` holds (the shared `card_list` reply). */
  list(slot) {
    return JSON.parse(this.app.card_status(slot));
  }

  /** The image IndexedDB holds for `slot`, or undefined. */
  stored(slot) {
    return storage.get('cards', key(slot));
  }

  /** The previous card IndexedDB holds for `slot`, or undefined. */
  storedPrevious(slot) {
    return storage.get('cards', previousKey(slot));
  }

  status() {
    return { pending: this.images.size, written: this.written, error: this.error, notice: this.notice };
  }
}
