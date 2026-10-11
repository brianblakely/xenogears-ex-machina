// IndexedDB: the memory card images, save blobs and, where the File System
// Access API exists, the handle of the user's disc image so it can be reopened
// after a reload. All of it stays in this browser profile; nothing is uploaded.

const DB = 'xem';
const VERSION = 2;
const STORES = ['saves', 'handles', 'cards'];

function open() {
  return new Promise((resolve, reject) => {
    let blocked = false;
    const request = indexedDB.open(DB, VERSION);
    request.onupgradeneeded = () => {
      for (const store of STORES) {
        if (!request.result.objectStoreNames.contains(store)) request.result.createObjectStore(store);
      }
    };
    request.onsuccess = () => { if (blocked) request.result.close(); else resolve(request.result); };
    request.onerror = () => reject(request.error);
    request.onblocked = () => { blocked = true; reject(new Error('Storage is blocked by another open page; close it and retry.')); };
  });
}

async function transact(store, mode, action) {
  const db = await open();
  try {
    return await new Promise((resolve, reject) => {
      const tx = db.transaction(store, mode, mode === 'readwrite' ? { durability: 'strict' } : undefined);
      const request = action(tx.objectStore(store));
      tx.oncomplete = () => resolve(request.result);
      tx.onerror = () => reject(tx.error);
      tx.onabort = () => reject(tx.error ?? new Error('Storage write was interrupted; the previous stored value is unchanged.'));
    });
  } finally {
    db.close();
  }
}

export const put = (store, key, value) => transact(store, 'readwrite', (s) => s.put(value, key));
export const get = (store, key) => transact(store, 'readonly', (s) => s.get(key));
export const remove = (store, key) => transact(store, 'readwrite', (s) => s.delete(key));

export async function status() {
  try {
    const estimate = await navigator.storage?.estimate();
    const persistent = await navigator.storage?.persisted();
    return { available: true, persistent: persistent ?? false, usage: estimate?.usage, quota: estimate?.quota,
      notice: 'Storage belongs to this browser profile and may be cleared or evicted. Export backups.' };
  } catch (error) {
    return { available: false, error: String(error) };
  }
}

/** Only from an explicit player action; persistence is a browser decision. */
export async function requestPersistence() {
  return Boolean(await navigator.storage?.persist());
}

export async function sha256(bytes) {
  const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
  return Array.from(digest, (b) => b.toString(16).padStart(2, '0')).join('');
}
