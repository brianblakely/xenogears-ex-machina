"""Execute the actual packaged binaries without the build shell's library paths."""
import argparse
import os
import platform
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=["linux", "windows", "macos"])
    parser.add_argument("--version", required=True)
    parser.add_argument("--artifact", type=Path, help="test a freshly built artifact before copying it to dist")
    parser.add_argument("--graphical", action="store_true", help="test the Linux GUI with the pinned software driver and Xvfb")
    parser.add_argument("--disc", type=Path, help="also test first-run organization with a private CHD or raw image")
    args = parser.parse_args()
    arch = platform.machine().lower().replace("amd64", "x86_64").replace("arm64", "aarch64")
    stem = f"xenogears-ex-machina-{args.version}-{args.kind}-{arch}"
    env = {k: v for k, v in os.environ.items() if not k.startswith(("XEM_", "LD_", "DYLD_"))}
    if args.disc and (not args.disc.is_file() or args.disc.suffix.lower() not in {".chd", ".bin", ".iso"}):
        parser.error("--disc must name an existing CHD, BIN or raw ISO")
    (ROOT / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="xem portable ", dir=ROOT / "build") as tmp:
        root = Path(tmp)
        elsewhere = root / "different working directory"
        elsewhere.mkdir()
        if args.kind == "linux":
            image = root / f"{stem}.AppImage"
            shutil.copyfile(args.artifact or ROOT / "dist" / image.name, image)
            image.chmod(0o755)
            env["APPIMAGE_EXTRACT_AND_RUN"] = "1"
            desktop, headless = [str(image)], [str(image), "--headless"]
        else:
            with zipfile.ZipFile(args.artifact or ROOT / "dist" / f"{stem}.zip") as archive:
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
        imported = None
        if args.disc:
            imported = root / ("Disc One" + args.disc.suffix)
            # Only a disposable hard link moves; the user's image stays put.
            os.link(args.disc, imported)
        if args.kind != "linux" or args.graphical:
            graphical = desktop
            if args.kind == "linux":
                env["SDL_VIDEO_DRIVER"] = "x11"
                env["SDL_AUDIO_DRIVER"] = "dummy"
                env["VK_ICD_FILENAMES"] = os.environ["XEM_LAVAPIPE_ICD"]
                graphical = ["xvfb-run", "-a", *desktop]
            subprocess.run([*graphical, "--exit-after-frames", "3", "--show-settings", "--screenshot", str(root / "frame.png")],
                           cwd=elsewhere, env=env, check=True, timeout=120)
            if not (root / "frame.png").is_file():
                raise SystemExit("packaged application produced no picture")
            captures = ROOT / "build/release-smoke"
            captures.mkdir(exist_ok=True)
            shutil.copyfile(root / "frame.png", captures / f"{stem}.png")
            for name in ["discs", "cards", "saves"]:
                if not (root / name).is_dir() or (elsewhere / name).exists():
                    raise SystemExit(f"incorrect portable storage location: {name}")
            if imported:
                relocated = root / "discs" / imported.name
                if imported.exists() or not relocated.is_file() or not args.disc.is_file():
                    raise SystemExit("first-run disc organization failed")
                subprocess.run([*headless, "--disc", str(relocated), "--frames", "120"],
                               cwd=elsewhere, env=env, check=True, timeout=120)
    print(f"{stem}: packaged startup passed")


if __name__ == "__main__":
    main()
