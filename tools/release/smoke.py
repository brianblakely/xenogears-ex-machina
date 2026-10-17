"""Execute the actual packaged binaries without the build shell's library paths."""
import argparse
import os
import platform
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=["linux", "windows", "macos"])
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    arch = platform.machine().lower().replace("amd64", "x86_64").replace("arm64", "aarch64")
    stem = f"xenogears-ex-machina-{args.version}-{args.kind}-{arch}"
    env = {k: v for k, v in os.environ.items() if not k.startswith(("XEM_", "LD_", "DYLD_"))}
    with tempfile.TemporaryDirectory(prefix="xem portable ") as tmp:
        root = Path(tmp)
        elsewhere = root / "different working directory"
        elsewhere.mkdir()
        if args.kind == "linux":
            import shutil
            image = root / f"{stem}.AppImage"
            shutil.copyfile(ROOT / "dist" / image.name, image)
            image.chmod(0o755)
            env["APPIMAGE_EXTRACT_AND_RUN"] = "1"
            desktop, headless = [str(image)], [str(image), "--headless"]
        else:
            with zipfile.ZipFile(ROOT / "dist" / f"{stem}.zip") as archive:
                archive.extractall(root)
                if args.kind == "macos":
                    for item in archive.infolist():
                        (root / item.filename).chmod((item.external_attr >> 16) & 0o777)
            binaries = root if args.kind == "windows" else root / "Xenogears Ex Machina.app/Contents/MacOS"
            suffix = ".exe" if args.kind == "windows" else ""
            desktop, headless = [str(binaries / f"xem-desktop{suffix}")], [str(binaries / f"xem-headless{suffix}")]
        for executable in [desktop, headless]:
            output = subprocess.check_output([*executable, "--version"], cwd=elsewhere, env=env, text=True, timeout=60)
            if args.version not in output:
                raise SystemExit(f"wrong packaged version: {output}")
        # CI's Linux graphical execution is handled by the desktop suite with
        # its controlled compositor. Windows/macOS runners have a user session.
        if args.kind != "linux":
            subprocess.run([*desktop, "--exit-after-frames", "3", "--show-settings", "--screenshot", str(root / "frame.png")],
                           cwd=elsewhere, env=env, check=True, timeout=120)
            if not (root / "frame.png").is_file():
                raise SystemExit("packaged application produced no picture")
            for name in ["discs", "cards", "saves"]:
                if not (root / name).is_dir() or (elsewhere / name).exists():
                    raise SystemExit(f"incorrect portable storage location: {name}")
    print(f"{stem}: packaged startup passed")


if __name__ == "__main__":
    main()
