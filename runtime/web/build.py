#!/usr/bin/env python3
"""Build the browser host into build/web/ (inside nix/runtime).

Compiles runtime/crates/xem-web for wasm32-unknown-unknown, generates its
bindings with wasm-bindgen (--target web), copies the page and, unless
--no-game, the game module (build/game/game.wasm and stubs.txt, built from the
recovered source by tools/game_module.py) next to it. The output is static
files: serve it from any secure origin (localhost, or HTTPS). It never contains
the user's discs or anything read from them.
"""

import argparse
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / "runtime" / "web"
PAGE = ["index.html", "style.css", "main.js", "game.js", "disc.js", "storage.js", "audio.js",
        "audio-worklet.js", "xem.js"]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=str(ROOT / "build" / "web"))
    parser.add_argument("--profile", default="release", choices=["release", "dev"])
    parser.add_argument("--no-game", action="store_true", help="leave the game module out (a hosted build)")
    args = parser.parse_args()
    out = Path(args.out)

    env = dict(os.environ, CARGO_BUILD_JOBS=os.environ.get("CARGO_BUILD_JOBS", "6"))
    cargo = ["cargo", "build", "-p", "xem-web", "--target", "wasm32-unknown-unknown"]
    if args.profile == "release":
        cargo.append("--release")
    subprocess.run(cargo, cwd=ROOT / "runtime", env=env, check=True)
    target = "release" if args.profile == "release" else "debug"
    wasm = ROOT / "runtime" / "target" / "wasm32-unknown-unknown" / target / "xem_web.wasm"

    if out.exists():
        shutil.rmtree(out)
    (out / "pkg").mkdir(parents=True)
    subprocess.run(["wasm-bindgen", "--target", "web", "--no-typescript", "--out-dir", str(out / "pkg"), str(wasm)],
                   check=True)
    for name in PAGE:
        shutil.copy2(WEB / name, out / name)
    game = ROOT / "build" / "game"
    if not args.no_game and (game / "game.wasm").exists():
        (out / "game").mkdir()
        for name in ["game.wasm", "stubs.txt"]:
            if (game / name).exists():
                shutil.copy2(game / name, out / "game" / name)
        print(f"included the game module from {game}")
    else:
        print("built without a game module")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
