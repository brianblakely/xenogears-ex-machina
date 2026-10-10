// The game module: build/game/game.wasm, compiled from the recovered C. It is
// instantiated beside the Rust runtime module; every "xem" import is one
// generic forward into the runtime (xem_game_import), which answers with a
// value, an asyncify unwind or a trap. The two modules share no memory.

import { xem_game_import } from './pkg/xem_web.js';

/** Fetch and compile the game module; the page keeps working without one. */
export async function loadGameModule(url) {
  try {
    const response = await fetch(url);
    if (!response.ok) throw new Error(`${url}: HTTP ${response.status}`);
    const module = await WebAssembly.compileStreaming(response);
    const names = WebAssembly.Module.imports(module)
      .filter((entry) => entry.module === 'xem' && entry.kind === 'function')
      .map((entry) => entry.name);
    const exports = WebAssembly.Module.exports(module).map((entry) => entry.name);
    return { available: true, url, module, names, exports };
  } catch (error) {
    return { available: false, url, error: String(error) };
  }
}

/** A fresh instance (fresh game memory) whose imports reach the runtime. */
export async function instantiate(game) {
  const xem = {};
  game.names.forEach((name, index) => {
    xem[name] = (...args) => xem_game_import(index, Uint32Array.from(args));
  });
  return WebAssembly.instantiate(game.module, { xem });
}
