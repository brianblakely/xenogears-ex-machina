"""Build portable release artifacts inside the pinned Nix release shells."""
import argparse
import hashlib
import json
import os
import platform
import plistlib
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.release.runtime_sources import bundle as bundle_sources  # noqa: E402
from tools.release.runtime_sources import store_root

RUNTIME = ROOT / "src/runtime"
DIST = ROOT / "dist"


def run(*command, **kwargs):
    return subprocess.run([str(p) for p in command], check=True, **kwargs)


def zip_tree(source, destination):
    """Only the staged files, stable timestamps and Unix executable modes."""
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(source.rglob("*")):
            if path.is_symlink():
                raise ValueError(f"unexpected staged symlink: {path}")
            if path.is_file():
                info = zipfile.ZipInfo(path.relative_to(source).as_posix(), (1980, 1, 1, 0, 0, 0))
                info.create_system = 3
                info.external_attr = (path.stat().st_mode & 0xFFFF) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, path.read_bytes())


def notices(stage):
    for source, name in [(ROOT / "LICENSE", "LICENSE"), (ROOT / "packaging/install.txt", "README.txt"),
                         (ROOT / "build/THIRD_PARTY_NOTICES.txt", "THIRD_PARTY_NOTICES.txt")]:
        shutil.copyfile(source, stage / name)


def windows_libraries(stage):
    # Query the actual toolchain/SDL closure; never assume a developer's DLLs
    # are installed on the player's machine.
    closure = subprocess.check_output(["nix-store", "-qR", os.environ["XEM_WINDOWS_SDL"],
                                      os.environ["XEM_WINDOWS_CC_LIBS"]], text=True).splitlines()
    available = {p.name.lower(): p for root in closure for p in Path(root).rglob("*.dll")}
    # Windows system DLLs. Everything else must be shipped or fail the build.
    system = {"kernel32.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll", "ole32.dll",
              "oleaut32.dll", "ws2_32.dll", "ntdll.dll", "msvcrt.dll", "ucrtbase.dll", "bcrypt.dll",
              "userenv.dll", "secur32.dll", "shlwapi.dll", "imm32.dll", "setupapi.dll", "version.dll",
              "winmm.dll", "cfgmgr32.dll", "dwmapi.dll", "comdlg32.dll", "winspool.drv", "dxgi.dll",
              "d3d12.dll", "d3d11.dll", "dxguid.dll", "hid.dll", "crypt32.dll", "propsys.dll", "dbghelp.dll",
              "bcryptprimitives.dll"}
    pending = list(stage.glob("*.exe"))
    copied = set()
    components = {os.environ["XEM_WINDOWS_SDL"]}
    while pending:
        binary = pending.pop()
        output = subprocess.check_output(["x86_64-w64-mingw32-objdump", "-p", str(binary)], text=True)
        for name in re.findall(r"DLL Name:\s*(\S+)", output):
            key = name.lower()
            if key in system or key.startswith(("api-ms-win-", "ext-ms-win-")) or key in copied:
                continue
            if key not in available:
                raise ValueError(f"unbundled Windows dependency: {name} ({binary.name})")
            target = stage / name
            shutil.copyfile(available[key], target)
            copied.add(key)
            components.add(store_root(available[key]))
            pending.append(target)
    return components


def macos_libraries(bundle):
    frameworks = bundle / "Contents/Frameworks"
    frameworks.mkdir()
    pending = list((bundle / "Contents/MacOS").iterdir())
    copied = {}
    while pending:
        binary = pending.pop()
        binary.chmod(0o755)
        output = subprocess.check_output(["otool", "-L", str(binary)], text=True)
        for line in output.splitlines()[1:]:
            library = line.strip().split(" (", 1)[0]
            if library.startswith(("/System/Library/", "/usr/lib/", "@")):
                continue
            source = Path(library)
            if not source.is_file():
                raise ValueError(f"unbundled macOS dependency: {library}")
            if source.name in copied and copied[source.name] != source:
                raise ValueError(f"two libraries named {source.name}")
            target = frameworks / source.name
            if source.name not in copied:
                copied[source.name] = source
                shutil.copyfile(source, target)
                target.chmod(0o755)
                run("install_name_tool", "-id", f"@rpath/{source.name}", target)
                pending.append(target)
            relative = "@loader_path/" if binary.parent == frameworks else "@executable_path/../Frameworks/"
            run("install_name_tool", "-change", library, relative + source.name, binary)
    # Ad-hoc signatures keep modified Mach-O load commands valid on Apple
    # Silicon. This is not an Apple Developer ID/notarization claim.
    for library in frameworks.iterdir():
        run("codesign", "--force", "--sign", "-", library)
    run("codesign", "--force", "--deep", "--sign", "-", bundle)
    return {store_root(path) for path in copied.values()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=["linux", "windows", "macos", "web", "android"])
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"v\d+\.\d+(?:\.\d+)?", args.version):
        parser.error("expected a version such as v0.5 or v0.5.1")
    game = ROOT / "build/game"
    for name in ["game.wasm", "schema.json", "stubs.txt"]:
        if not (game / name).is_file():
            parser.error(f"missing {game / name}; build the release game module first")
    DIST.mkdir(exist_ok=True)
    arch = "x86_64" if args.kind == "windows" else platform.machine().replace("arm64", "aarch64")
    stem = f"xenogears-ex-machina-{args.version}-{args.kind}"
    if args.kind in {"linux", "windows", "macos"}:
        stem += f"-{arch}"
    stage = ROOT / "build/release" / stem
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    env = dict(os.environ, XEM_RELEASE_VERSION=args.version, XEM_GAME_WASM=str(game / "game.wasm"))
    if args.kind in {"linux", "windows", "macos"}:
        target = Path(os.environ.get("CARGO_TARGET_DIR", RUNTIME / "target-release")).resolve()
        env["CARGO_TARGET_DIR"] = str(target)
        command = ["cargo", "build", "--locked", "--release", "-p", "xem-desktop", "-p", "xem-headless"]
        if args.kind == "windows":
            command += ["--target", "x86_64-pc-windows-gnu"]
            target /= "x86_64-pc-windows-gnu"
        run(*command, cwd=RUNTIME, env=env)
        binaries = stage
        if args.kind == "macos":
            bundle = stage / "Xenogears Ex Machina.app"
            binaries = bundle / "Contents/MacOS"
            binaries.mkdir(parents=True)
            (bundle / "Contents/Info.plist").write_bytes(plistlib.dumps({
                "CFBundleExecutable": "xem-desktop", "CFBundleIdentifier": "dev.xem.desktop",
                "CFBundleName": "Xenogears Ex Machina", "CFBundlePackageType": "APPL",
                "CFBundleShortVersionString": args.version[1:], "CFBundleVersion": args.version[1:],
                "NSHighResolutionCapable": True, "LSMinimumSystemVersion": "13.0",
            }))
        for name in ["xem-desktop", "xem-headless"]:
            name += ".exe" if args.kind == "windows" else ""
            shutil.copy2(target / "release" / name, binaries / name)
        notices(stage)
        if args.kind == "linux":
            output = ROOT / "build/release-appimage"
            arguments = ["--impure", "--file", str(ROOT / "packaging/linux.nix"), "--argstr", "binaryDirectory", str(stage),
                         "--argstr", "version", args.version]
            nix = ["nix", "--extra-experimental-features", "nix-command flakes"]
            run(*nix, "build", *arguments, "--out-link", output)
            artifact = DIST / f"{stem}.AppImage"
            shutil.copyfile(output.resolve(), artifact)
            artifact.chmod(0o755)
            components = json.loads(subprocess.check_output([*nix, "eval", *arguments, "--json", "runtimeComponents"], text=True))
            source_roots = json.loads(subprocess.check_output([*nix, "eval", *arguments, "--json", "sourceRoots"], text=True))
        else:
            if args.kind == "windows":
                components = windows_libraries(stage)
            else:
                components = macos_libraries(bundle)
            source_roots = [os.environ["XEM_NIXPKGS_SOURCE"]]
            artifact = DIST / f"{stem}.zip"
            zip_tree(stage, artifact)
        bundle_sources(sorted(components), source_roots, DIST / f"{stem}-runtime-sources.tar.gz")
    elif args.kind == "web":
        run("python3", ROOT / "src/runtime/web/build.py", "--release", "--out", stage, env=env)
        notices(stage)
        artifact = DIST / f"{stem}.zip"
        zip_tree(stage, artifact)
    else:
        assets = ROOT / "build/android/notices"
        assets.mkdir(parents=True, exist_ok=True)
        notices(assets)
        env["XEM_ANDROID_VARIANT"] = "Release"
        run(ROOT / "src/runtime/scripts/android-build.sh", env=env)
        artifact = DIST / f"{stem}-horizon.apk"
        shutil.copyfile(ROOT / "build/android/gradle/app/outputs/apk/release/xem-release.apk", artifact)
        run(Path(os.environ["ANDROID_HOME"]) / "build-tools/34.0.0/apksigner", "verify", "--verbose", artifact)
    print(f"{hashlib.sha256(artifact.read_bytes()).hexdigest()}  {artifact.name}")


if __name__ == "__main__":
    main()
