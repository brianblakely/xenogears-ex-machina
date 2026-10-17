"""Generate dependency notices; unknown licenses or missing text block release."""
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SLINT_LICENSE_SHA256 = "5167f5056e850419106ab6265efbdca7cba4d99c849d1445ca0bbf6a1e2315fe"


def main():
    output = ROOT / "build/THIRD_PARTY_NOTICES.txt"
    output.parent.mkdir(exist_ok=True)
    result = subprocess.run([
        "cargo-about", "generate", "--manifest-path", str(ROOT / "src/runtime/Cargo.toml"),
        "--config", str(ROOT / "packaging/about.toml"), str(ROOT / "packaging/about.hbs"),
        "--output-file", str(output),
    ], capture_output=True, text=True)
    # cargo-about accepts LicenseRef expressions but cannot harvest their text.
    # Check Slint's custom terms ourselves, against the exact reviewed bytes.
    missing = re.findall(r"(\S+) has no license file for crate '([^']+)'", result.stderr)
    if result.returncode or any(license != "LicenseRef-Slint-Royalty-free-2.0" or not (
        crate.startswith(("slint ", "slint-", "i-slint-")) and crate.endswith(" 1.18.1")) for license, crate in missing):
        raise SystemExit(result.stdout + result.stderr)
    metadata = json.loads(subprocess.check_output(["cargo", "metadata", "--locked", "--format-version", "1",
                                                  "--manifest-path", str(ROOT / "src/runtime/Cargo.toml")], text=True))
    slint_text = None
    for crate in metadata["packages"]:
        if "LicenseRef-Slint-Royalty-free-2.0" not in (crate.get("license") or ""):
            continue
        path = Path(crate["manifest_path"]).parent / "LICENSES/LicenseRef-Slint-Royalty-free-2.0.md"
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != SLINT_LICENSE_SHA256:
            raise SystemExit(f"Slint terms changed: {path}")
        slint_text = data.decode()
    if slint_text is None:
        raise SystemExit("missing Slint license text")
    with output.open("a") as notices:
        notices.write("\n\nSlint 1.18.1 and its i-slint components\n\n" + slint_text)
        for label, variable, name in [("SDL", "XEM_SDL3_SRC", "LICENSE.txt"), ("WABT wasm2c runtime", "XEM_WABT_SRC", "LICENSE")]:
            notices.write(f"\n\n{label}\n\n" + (Path(os.environ[variable]) / name).read_text())
    print(output)


if __name__ == "__main__":
    main()
