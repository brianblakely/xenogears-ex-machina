// IndexedDB: save blobs and, where the File System Access API exists, the
// handle of the user's disc image so it can be reopened after a reload. All of
// it stays in this browser profile; nothing is uploaded.

const DB = 'xem';
const VERSION = 1;

function open() {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB, VERSION);
    request.onupgradeneeded = () => {
      request.result.createObjectStore('saves');
      request.result.createObjectStore('handles');
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

async function transact(store, mode, action) {
  const db = await open();
  try {
    return await new Promise((resolve, reject) => {
      const tx = db.transaction(store, mode);
      const request = action(tx.objectStore(store));
      tx.oncomplete = () => resolve(request.result);
      tx.onerror = () => reject(tx.error);
      tx.onabort = () => reject(tx.error);
    });
  } finally {
    db.close();
  }
}

export const put = (store, key, value) => transact(store, 'readwrite', (s) => s.put(value, key));
export const get = (store, key) => transact(store, 'readonly', (s) => s.get(key));
export const remove = (store, key) => transact(store, 'readwrite', (s) => s.delete(key));

export async function sha256(bytes) {
  const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', bytes));
  return Array.from(digest, (b) => b.toString(16).padStart(2, '0')).join('');
}
