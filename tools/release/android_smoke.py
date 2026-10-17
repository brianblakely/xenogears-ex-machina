"""Install the signed APK on the dedicated test AVD and preserve data on update."""
import argparse
import os
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = "dev.xem.app"
ADB = ["adb", "-s", "emulator-5584"]


def adb(*arguments, check=True):
    return subprocess.run([*ADB, *arguments], capture_output=True, text=True, check=check, timeout=120)


def start(logfile):
    adb("logcat", "-c")
    adb("shell", "am", "start", "-W", "-n", PACKAGE + "/.XemActivity")
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        log = adb("logcat", "-d", "-v", "brief").stdout
        logfile.write_text(log)
        if "xem: frame rendered frame=120 " in log:
            return
        if f"Process: {PACKAGE}, PID:" in log or not adb("shell", "pidof", PACKAGE, check=False).stdout.strip():
            details = adb("shell", "dumpsys", "activity", "exit-info", PACKAGE, check=False).stdout
            crash = adb("logcat", "-b", "crash", "-d", "-v", "brief", check=False).stdout
            logfile.with_suffix(".exit.log").write_text(details + "\n" + crash)
            raise RuntimeError(f"signed application failed to start (full log: {logfile}):\n" + details + crash + log[-8000:])
        time.sleep(1)
    raise RuntimeError("signed application did not render 120 frames:\n" + log[-8000:])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    apk = ROOT / f"dist/xenogears-ex-machina-{args.version}-android-horizon.apk"
    if not apk.is_file():
        parser.error(f"missing {apk}")
    os.environ["ANDROID_USER_HOME"] = str(ROOT / ".local/android/home")
    subprocess.run([str(ROOT / "src/runtime/scripts/android-smoke.sh")],
                   env=dict(os.environ, XEM_SMOKE_BOOT="1"), check=True)
    # As in android-smoke.sh, let first-boot configuration/activity changes
    # settle before launching the app. The boot-only stage intentionally does
    # not wait, so builds can overlap this interval.
    booted = float((ROOT / "build/android/smoke/booted-at").read_text())
    time.sleep(max(0, booted + 20 - time.time()))
    out = ROOT / "build/android/release-smoke"
    out.mkdir(parents=True, exist_ok=True)
    try:
        # This is the project's disposable Google APIs AVD, never a connected
        # personal device. Root permits inspecting a non-debuggable release.
        adb("root")
        adb("wait-for-device")
        adb("uninstall", PACKAGE, check=False)
        adb("install", str(apk))
        info = adb("shell", "dumpsys", "package", PACKAGE).stdout
        if f"versionName={args.version.removeprefix('v')}" not in info:
            raise RuntimeError("installed APK has the wrong version")
        start(out / "first-start.log")
        result = adb("shell", "run-as", PACKAGE, "pwd", check=False)
        if result.returncode == 0 or "not debuggable" not in result.stdout + result.stderr:
            raise RuntimeError("release APK unexpectedly permits run-as")
        for directory in ["discs", "cards", "saves"]:
            adb("shell", "test", "-d", f"/data/user/0/{PACKAGE}/files/{directory}")
        (out / "first-frame.png").write_bytes(subprocess.check_output([*ADB, "exec-out", "screencap", "-p"], timeout=30))
        adb("shell", "am", "force-stop", PACKAGE)
        for directory in ["cards", "saves"]:
            adb("shell", f"printf 'upgrade preserved' > /data/user/0/{PACKAGE}/files/{directory}/release-smoke-check")
        adb("install", "-r", str(apk))
        for directory in ["cards", "saves"]:
            path = f"/data/user/0/{PACKAGE}/files/{directory}/release-smoke-check"
            if adb("shell", "cat", path).stdout != "upgrade preserved":
                raise RuntimeError(f"APK update lost {directory}")
            adb("shell", "rm", path)
        start(out / "updated-start.log")
        print(f"{apk.name}: signed install, rendering, storage and update passed")
    finally:
        adb("emu", "kill", check=False)
        (ROOT / "build/android/smoke/booted-at").unlink(missing_ok=True)


if __name__ == "__main__":
    main()
