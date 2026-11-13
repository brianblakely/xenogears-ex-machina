#!/usr/bin/env python3
"""The test suite's runner: suite.sh starts it inside the runtime shell.

    suite.py TIER [--force] [--threads N]

A run is made of units: a build, one Rust test binary, one browser test
file, a device's smoke test. Each names the units it needs first, the
processor threads and memory it takes, and what it reads.

Room. A unit starts once what it needs has passed and the run has room for
it: the units running together never count more threads than the run may
keep busy (--threads; a thread for each processor core unless given, which
leaves a core's second thread, worth a quarter of the first here, to
whatever else the machine does), nor more memory than was available when
the run began. The run makes way for what else the machine does: a unit
starts beside others only while the machine still has that unit's memory to
give and nothing else keeps its threads busy. Units start in their order;
where one must wait for room, a later one starts ahead of it only if that
does not make it wait longer. A unit counts for what its last run here took
(build/suite/results.json): the memory it held with the processes it
started, and a browser test file the threads it kept busy. A Rust test
binary runs its tests on a share of the threads in proportion to its work
(that run's processor time), so the binaries end together and none idles
the run; one never run here counts as a quarter of it. A unit that measures
time (`alone`) runs with nothing beside it, after the others.

Results. A unit that passed is not run again while what it reads is what it
read then: the binaries it runs, the tracked and untracked files under the
paths it names, its command, the pinned tools (nix/) and the private inputs'
sizes and times. It is reported as `same`, with the time of the pass it
stands on, and its log is that pass's. A unit that failed always runs
again; --force runs every unit. Private inputs (the discs, .local) are
known by size and time only: after changing what one holds, use --force.

Logs: build/suite/<unit>.log; the units' results and times in
build/suite/summary.txt.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime
import fcntl
import hashlib
import json
import math
import os
import queue
import re
import signal
import subprocess
import sys
import threading
import time
from collections.abc import Callable, Iterable
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
RUNTIME = ROOT / "src/runtime"
LOGS = ROOT / "build/suite"
# The native builds' own cargo target directory (none of them play.sh's).
TARGET = RUNTIME / "target-suite"
DISC = ROOT / "discs/Xenogears disc 1.chd"
NIX = ["nix", "--extra-experimental-features", "nix-command flakes", "develop"]
# What every unit's result stands on besides its own inputs: the pinned
# tools and the private inputs, by size and time.
TOOLS = ("nix",)
PRIVATE = ("discs/*.chd", ".local/discs/*", ".local/extract/*/manifest.json", ".local/scenarios/*/*", ".local/scenarios/*.json")


@dataclasses.dataclass
class Unit:
    name: str
    command: list[str]
    cwd: Path = ROOT
    env: dict[str, str] = dataclasses.field(default_factory=dict)
    needs: tuple[str, ...] = ()
    # Processor threads and MiB it is counted for (its memory, only until
    # it has run here: then what it held, unless it is a build).
    cpus: float = 1.0
    memory: int = 512
    # A Rust test binary: its tests run on `cpus` threads (RUST_TEST_THREADS),
    # and what it held grows with them.
    tests: bool = False
    alone: bool = False
    # What its result stands on (None: it runs every time, as a build does,
    # which rebuilds only what changed).
    reads: Callable[[], object] | None = None
    # What it makes: without them it runs whatever it read.
    makes: tuple[Path, ...] = ()
    # Units to add once it has passed (a build's test binaries).
    then: Callable[[], list[Unit]] | None = None
    # What to run by hand to look into its failure, shown with it.
    look: str = ""


@dataclasses.dataclass
class Result:
    outcome: str  # pass, same, FAIL, or left (a unit it needs failed)
    seconds: float = 0.0
    note: str = ""


# --- What a unit reads -------------------------------------------------------


def digest(*parts: object) -> str:
    return hashlib.blake2b(json.dumps(parts, sort_keys=True, default=str).encode(), digest_size=16).hexdigest()


_files: dict[tuple[str, int, int], str] = {}


def file_digest(path: Path | str) -> str:
    """A file's contents."""
    try:
        stat = os.stat(path)
        key = (str(path), stat.st_size, stat.st_mtime_ns)
        if key not in _files:
            with open(path, "rb") as file:
                _files[key] = hashlib.file_digest(file, lambda: hashlib.blake2b(digest_size=16)).hexdigest()
        return _files[key]
    except OSError:
        return "absent"


def directory_digest(path: Path) -> str:
    """The files of a build's output directory, by name and contents."""
    files = sorted(file for file in path.rglob("*") if file.is_file())
    return digest([(str(file.relative_to(path)), file_digest(file)) for file in files])


_trees: dict[tuple, str] = {}


def tree(*specs: str, root: Path = ROOT) -> str:
    """The files git would track under `specs` (pathspecs from the
    repository's root), by path and contents: tracked files as the working
    tree has them, and untracked ones that are not ignored. The same whether
    or not a change is staged or committed."""
    if (root, *specs) in _trees:
        return _trees[(root, *specs)]

    def git(*args: str) -> list[str]:
        out = subprocess.run(["git", "-C", str(root), *args, "-z", "--", *specs], check=True, capture_output=True).stdout
        return [os.fsdecode(record) for record in out.split(b"\0") if record]

    blobs = {}
    for record in git("ls-files", "--stage"):
        meta, path = record.split("\t", 1)
        blobs[path] = meta.split()[1]
    for path in set(git("ls-files", "--modified", "--deleted", "--others", "--exclude-standard")):
        try:
            data = (root / path).read_bytes()
        except OSError:
            blobs.pop(path, None)
            continue
        blobs[path] = hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()
    _trees[(root, *specs)] = digest(sorted(blobs.items()))
    return _trees[(root, *specs)]


def private(*patterns: str) -> list[tuple[str, int, int]]:
    """Private inputs too large to read every run: name, size and time."""
    found = []
    for pattern in patterns:
        for path in sorted(ROOT.glob(pattern)):
            if path.is_file():
                stat = path.stat()
                found.append((str(path.relative_to(ROOT)), stat.st_size, stat.st_mtime_ns))
    return found


# --- Running -----------------------------------------------------------------


@dataclasses.dataclass
class Ran:
    passed: bool
    seconds: float
    # Processor seconds of all its threads, and the most MiB it held with
    # the processes it started.
    work: float
    held: int


_running: dict[int, str] = {}
# The most MiB each unit running has held, as the watch has seen it.
_held: dict[int, int] = {}
_watching = threading.Lock()
STATUS = re.compile(r"^(PPid|RssAnon|RssShmem):\s+(\d+)", re.MULTILINE)


def trees(leaders: Iterable[int]) -> dict[int, int]:
    """The MiB each of those processes holds with its descendants: memory of
    their own, not a file's that the machine can drop (the kernel's RssAnon
    and RssShmem)."""
    parents, holds = {}, {}
    for entry in os.scandir("/proc"):
        if not entry.name.isdigit():
            continue
        try:
            status = dict(STATUS.findall(Path(entry.path, "status").read_text()))
        except OSError:
            continue
        parents[int(entry.name)] = int(status.get("PPid", 0))
        holds[int(entry.name)] = int(status.get("RssAnon", 0)) + int(status.get("RssShmem", 0))
    totals = dict.fromkeys(leaders, 0)
    for pid, held in holds.items():
        while pid and pid not in totals:
            pid = parents.get(pid, 0)
        if pid:
            totals[pid] += held
    return {pid: held // 1024 for pid, held in totals.items()}


def watch() -> None:
    """Every two seconds, what the units running hold."""
    while True:
        time.sleep(2.0)
        for pid, held in trees(list(_running)).items():
            if pid in _running:
                _held[pid] = max(_held.get(pid, 0), held)


def execute(unit: Unit, log: Path) -> Ran:
    """Run a unit's command into its log: whether it passed, and what it took."""
    env = {**os.environ, **unit.env}
    if unit.tests:
        env["RUST_TEST_THREADS"] = str(int(unit.cpus))
    started = time.monotonic()
    with open(log, "wb") as out:
        try:
            process = subprocess.Popen(unit.command, cwd=unit.cwd, env=env, stdin=subprocess.DEVNULL,
                                       stdout=out, stderr=subprocess.STDOUT, start_new_session=True)
        except OSError as error:
            out.write(f"{unit.command[0]}: {error}\n".encode())
            return Ran(False, 0.0, 0.0, 0)
        _held.pop(process.pid, None)
        _running[process.pid] = unit.name
        if _watching.acquire(blocking=False):
            threading.Thread(target=watch, daemon=True).start()
        _, status, usage = os.wait4(process.pid, 0)
        _running.pop(process.pid, None)
        process.returncode = os.waitstatus_to_exitcode(status)
    # A unit done before the watch looked held its largest process at least.
    held = max(_held.pop(process.pid, 0), usage.ru_maxrss // 1024)
    return Ran(process.returncode == 0, time.monotonic() - started, usage.ru_utime + usage.ru_stime, held)


def stop_running() -> None:
    """End every unit's processes (each unit leads a session of its own)."""
    for pid in list(_running):
        try:
            os.killpg(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass


def cores() -> list[list[int]]:
    """The processors this process may run on, each core's threads together."""
    found: dict[str, list[int]] = {}
    for cpu in sorted(os.sched_getaffinity(0)):
        try:
            core = Path(f"/sys/devices/system/cpu/cpu{cpu}/topology/core_cpus_list").read_text().strip()
        except OSError:
            core = str(cpu)
        found.setdefault(core, []).append(cpu)
    return list(found.values())


def processors() -> list[int]:
    """Those processors, a thread of every core before any core's second:
    the first N are the N to keep busy."""
    threads = cores()
    return [core[turn] for turn in range(max(map(len, threads))) for core in threads if turn < len(core)]


def spare() -> int:
    """The MiB this machine has to give now (the kernel's estimate of what
    is available without swapping), less a gigabyte that stays with
    whatever else it runs."""
    available = 0
    for line in Path("/proc/meminfo").read_text().splitlines():
        if line.startswith("MemAvailable:"):
            available = int(line.split()[1]) // 1024
    return available - 1024


def worked() -> float:
    """The processor seconds this machine has worked since it started,
    whatever for."""
    user, nice, system, _, _, irq, softirq, steal = map(int, Path("/proc/stat").read_text().split("\n", 1)[0].split()[1:9])
    return (user + nice + system + irq + softirq + steal) / os.sysconf("SC_CLK_TCK")


@dataclasses.dataclass
class Machine:
    """What a run may take: processor threads kept busy and MiB of memory;
    and, where a run is to make way for what else the machine does, the
    memory it has to give at the moment and the processor seconds it has
    worked."""

    cpus: float
    memory: int
    spare: Callable[[], int] | None = None
    worked: Callable[[], float] | None = None

    @staticmethod
    def this(threads: int | None = None) -> Machine:
        return Machine(float(min(threads or len(cores()), len(processors()))), max(spare(), 2048), spare, worked)


def shares(work: dict[str, float], threads: int) -> dict[str, int]:
    """The threads each test binary runs its tests on, from its work (the
    processor seconds of its last run): the fewest that let every binary end
    within the same time, the binaries with less work than that time taking
    a thread one after another."""
    total = sum(work.values())
    target = total / threads
    while total:
        large = {name: math.ceil(seconds / target) for name, seconds in work.items() if seconds > target}
        small = sum(seconds for seconds in work.values() if seconds <= target)
        if sum(large.values()) + math.ceil(small / target) <= threads:
            return {name: large.get(name, 1) for name in work}
        target *= 1.02
    return {name: 1 for name in work}


@dataclasses.dataclass
class Active:
    unit: Unit
    cpus: float
    memory: int
    # When it is expected to end (now, for one never run here).
    until: float


class Run:
    """Units run as their needs pass and the machine has room."""

    def __init__(self, units: Iterable[Unit], machine: Machine, records: dict, force: bool = False,
                 execute: Callable[[Unit, Path], Ran] = execute, logs: Path = LOGS,
                 report: Callable[[str], None] = lambda text: print(text, flush=True)):
        self.pending = list(units)
        self.machine = machine
        self.records = records
        self.force = force
        self.execute = execute
        self.logs = logs
        self.report = report
        self.results: dict[str, Result] = {}
        self.order: list[str] = []
        self.active: dict[str, Active] = {}
        self.finished: queue.Queue[tuple[Unit, str | None, Ran]] = queue.Queue()
        self.stopping = False
        # Seconds between looks at what the machine has to give.
        self.patience = 10.0 if machine.spare or machine.worked else None
        # The threads what else the machine runs kept busy at the last look:
        # its work since the look before (at least `window` seconds earlier)
        # less what this run's units counted for meanwhile, the thread
        # seconds in `counted` up to `since`.
        self.elsewhere = 0.0
        self.window = 2.0
        self.looked = (time.monotonic(), machine.worked() if machine.worked else 0.0)
        self.counted, self.since = 0.0, self.looked[0]

    def memory(self, unit: Unit) -> int:
        """The memory a unit counts for: what its last run here held (a test
        binary's more by the threads it has more now). A build, which holds
        what there is to build, and a unit never run here count as they
        say."""
        record = self.records.get(unit.name, {})
        if "held" not in record or not (unit.tests or unit.reads):
            return unit.memory
        return int(record["held"] * max(unit.cpus / record.get("threads", unit.cpus), 1.0)) if unit.tests else record["held"]

    def expected(self, unit: Unit) -> float | None:
        """The seconds a unit is expected to take (None: never run here): its
        last run's, or a test binary's work over the threads it has now."""
        record = self.records.get(unit.name)
        if not record:
            return None
        return record["work"] / unit.cpus if unit.tests and record.get("threads") != unit.cpus else record["seconds"]

    def room(self, cpus: float, memory: int, used: tuple[float, int]) -> bool:
        return used[0] + cpus <= self.machine.cpus + 0.5 and used[1] + memory <= self.machine.memory

    def spare(self, cpus: float, memory: int, now: float) -> bool:
        """Whether the machine has that to give now: what else it runs may
        have taken the memory it had when the run began (and a unit more
        than it counts for), and keeps threads busy that the run then
        leaves alone."""
        machine = self.machine
        if machine.spare is not None and memory > machine.spare():
            return False
        if machine.worked is not None and now - self.looked[0] >= self.window:
            self.count(now)
            work = machine.worked()
            self.elsewhere = max((work - self.looked[1] - self.counted) / (now - self.looked[0]), 0.0)
            self.looked, self.counted = (now, work), 0.0
        return self.used()[0] + cpus + self.elsewhere <= machine.cpus + 0.5

    def count(self, now: float) -> None:
        """Before the units running change: what they counted for till now."""
        self.counted += self.used()[0] * (now - self.since)
        self.since = now

    def used(self) -> tuple[float, int]:
        return sum(a.cpus for a in self.active.values()), sum(a.memory for a in self.active.values())

    def held(self, unit: Unit, now: float) -> tuple[float, float, int]:
        """When the units running will have left room for `unit`, which must
        wait for it, and the threads and memory to spare beside it then."""
        cpus, memory = unit.cpus, self.memory(unit)
        used = list(self.used())
        for active in sorted(self.active.values(), key=lambda a: a.until):
            used[0] -= active.cpus
            used[1] -= active.memory
            if self.room(cpus, memory, (used[0], used[1])):
                return max(active.until, now), self.machine.cpus - used[0] - cpus, self.machine.memory - used[1] - memory
        return math.inf, 0.0, 0

    def ready(self, unit: Unit) -> bool:
        return all(self.results.get(need, Result("")).outcome in ("pass", "same") for need in unit.needs)

    def settle(self, unit: Unit, result: Result) -> None:
        if result.outcome == "FAIL":
            result.note = f"{self.logs / unit.name}.log" + (f"; {unit.look}" if unit.look else "")
        self.results[unit.name] = result
        self.order.append(unit.name)
        self.report(line(unit.name, result))
        if result.outcome in ("pass", "same") and unit.then:
            # A build's units go where it stood.
            self.pending[0:0] = unit.then()

    def start(self, unit: Unit, now: float) -> None:
        fingerprint = None
        if unit.reads is not None:
            fingerprint = digest(unit.command, sorted(unit.env.items()), str(unit.cwd), unit.reads())
            record = self.records.get(unit.name, {})
            if not self.force and record.get("fingerprint") == fingerprint and all(path.exists() for path in unit.makes):
                self.settle(unit, Result("same", record.get("seconds", 0.0), f"as passed {record.get('passed', '')}"))
                return
        self.count(now)
        self.active[unit.name] = Active(unit, unit.cpus, self.memory(unit), now + (self.expected(unit) or 0.0))
        log = self.logs / f"{unit.name}.log"

        def work() -> None:
            self.finished.put((unit, fingerprint, self.execute(unit, log)))

        threading.Thread(target=work, daemon=True).start()

    def finish(self, unit: Unit, fingerprint: str | None, ran: Ran) -> None:
        self.count(time.monotonic())
        del self.active[unit.name]
        record = {"seconds": round(ran.seconds, 1), "work": round(ran.work, 1), "held": max(ran.held, 64), "threads": unit.cpus}
        if unit.tests:
            # How many tests it has: it takes no more threads.
            log = self.logs / f"{unit.name}.log"
            record["count"] = sum(int(count) for count in RUNNING_TESTS.findall(log.read_bytes())) if log.exists() else 0
        if ran.passed and fingerprint:
            record |= {"fingerprint": fingerprint, "passed": datetime.datetime.now().strftime("%Y-%m-%d %H:%M")}
        self.records[unit.name] = record
        self.settle(unit, Result("pass" if ran.passed else "FAIL", ran.seconds))

    def begin(self) -> bool:
        """Start what may start now; whether anything did."""
        waiting = [unit for unit in self.pending if self.ready(unit)]
        # Those that measure time wait for the others to be done.
        together = [unit for unit in waiting if not unit.alone]
        if not together:
            for unit in waiting[:1] if not self.active else []:
                self.pending.remove(unit)
                self.start(unit, time.monotonic())
                return True
            return False
        if any(active.unit.alone for active in self.active.values()):
            return False
        began = False
        now = time.monotonic()
        hold = None
        for unit in together:
            cpus, memory = unit.cpus, self.memory(unit)
            fits = not self.active or (self.room(cpus, memory, self.used()) and self.spare(cpus, memory, now))
            if fits and hold:
                # Ahead of one that waits: done before its time, or within
                # what is to spare beside it then.
                until, spare_cpus, spare_memory = hold
                expected = self.expected(unit)
                if expected is not None and now + expected <= until:
                    pass
                elif cpus <= spare_cpus and memory <= spare_memory:
                    hold = (until, spare_cpus - cpus, spare_memory - memory)
                else:
                    fits = False
            if fits:
                self.pending.remove(unit)
                self.start(unit, now)
                began = True
            elif not hold:
                hold = self.held(unit, now)
        return began

    def run(self) -> dict[str, Result]:
        while self.pending or self.active:
            if not self.stopping and self.begin():
                continue
            if self.active:
                # Until one is done, or a while: the machine may have more
                # to give by then.
                try:
                    self.finish(*self.finished.get(timeout=self.patience))
                except queue.Empty:
                    pass
                continue
            # Nothing runs and nothing can start: what is left needs a unit
            # that failed (or the run is being stopped).
            for unit in self.pending:
                failed = [need for need in unit.needs if self.results.get(need, Result("left")).outcome not in ("pass", "same")]
                self.settle(unit, Result("left", note="stopped" if self.stopping else f"needs {', '.join(failed)}"))
            self.pending = []
        return self.results


RUNNING_TESTS = re.compile(rb"^running (\d+) tests?$", re.MULTILINE)


def line(name: str, result: Result) -> str:
    return f"{result.outcome:<4} {result.seconds:6.0f} s  {name}{'  (' + result.note + ')' if result.note else ''}"


# --- The units ---------------------------------------------------------------


def in_shell(shell: str, *command: str) -> list[str]:
    """A command in another of the pinned shells."""
    return [*NIX, f"path:{ROOT}/nix/{shell}", "-c", *command]


def bash(script: str) -> list[str]:
    return ["bash", "-euo", "pipefail", "-c", script]


PORT_TESTS = """tests.test_port_model_draw tests.test_port_gte tests.test_port_gpu tests.test_game_module_pause tests.test_game_module_polls
    tests.test_port_original_traces tests.test_port_resident tests.test_port_arena tests.test_port_worldmap
    tests.test_port_presentation tests.test_port_disc tests.test_dispatch_tables tests.test_platforming
    tests.test_port_encounters tests.test_port_text_speed tests.test_port_movie_skip tests.test_port_dialogue
    tests.test_port_locale tests.test_game_coverage tests.test_game_module_memory""".split()

# The browser tests that measure time (the headset's frame rate, rewind frame
# times, fast-forward's music, the stalled page, a resume read within the
# rewind's one-second interval, the Binaural mode's cost), each a pattern of
# its file's test names; the other tests of those files run beside others.
TIMED = {
    "persistence": ["rewind frame p99", "keep music on host time", "rewind uses elapsed seconds during FF",
                    "the configured history length expires game saves by elapsed play time", "Start pause freezes rewind history"],
    "page": ["drops missed time"],
    "binaural": ["phone-class processor"],
    "xr-game": ["."],
    "xr-diorama": ["."],
}

# The Rust test binaries that measure time (a frame's latency, the desktop's
# window and the settings panel paced by the clock): alone as well.
TIMED_RUST = {("xem-host", "unit"), ("xem-desktop", "control"), ("xem-desktop", "states"), ("xem-ui", "panel")}

# Software Vulkan (Mesa's lavapipe, which draws on the processor) for what
# draws. Tests that draw side by side rasterize in their own threads:
# llvmpipe's default (a pool of a thread a core for every device) takes half
# as much processor time again for the same pictures, and leaves the threads
# a test binary runs on uncounted.
LAVAPIPE = {"VK_ICD_FILENAMES": os.environ.get("XEM_LAVAPIPE_ICD", "")}
INLINE = {"LP_NUM_THREADS": "0"}


def card(host: Path = RUNTIME / "scripts/xem-gpu-host") -> tuple[dict[str, str], object]:
    """The environment that puts the host's graphics card before the software
    rasterizer (scripts/xem-gpu-host), and what tells the card's driver from
    another; lavapipe alone on a machine without one. The Rust tests draw on
    the card (in a quarter of the processor time, where the heaviest draw
    most) but for those that compare pictures to the last bit, which ask for
    the software rasterizer (xem-render's Gpu::reference)."""
    try:
        out = subprocess.run([str(host), "env"], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return dict(LAVAPIPE), None
    env = dict(entry.split("=", 1) for entry in out.splitlines() if "=" in entry)
    env = {name: env[name] for name in ("VK_ICD_FILENAMES", "VK_DRIVER_FILES", "LD_LIBRARY_PATH")}
    drivers = Path(env["LD_LIBRARY_PATH"].split(":")[0])
    identity = [(path.name, path.stat().st_size, path.stat().st_mtime_ns) for path in sorted(drivers.iterdir()) if path.exists()]
    return env, identity


def kept(records: dict, name: str, cpus: float) -> float:
    """The threads a unit counts for: those its last run here kept busy on
    average (no fewer than one), else `cpus`."""
    record = records.get(name, {})
    return max(round(record["work"] / record["seconds"], 1), 1.0) if record.get("seconds", 0.0) >= 1.0 else cpus


def game_module() -> list[str]:
    return [file_digest(ROOT / "build/game" / name) for name in ("game.wasm", "schema.json", "stubs.txt")]


def plan(tier: str, machine: Machine, records: dict) -> list[Unit]:
    """The tier's units, in the order they are tried."""
    covers = lambda *tiers: tier == "full" or tier in tiers  # noqa: E731
    threads = int(machine.cpus)
    # A build takes the run's threads but two, which the tests already built
    # and the checks that need no build go on with; builds follow one another.
    wide = max(threads - 2, 1)
    suite = TARGET / "suite"
    headless = suite / "xem-headless"
    has_disc = DISC.exists()
    common = lambda: [tree(*TOOLS), private(*PRIVATE)]  # noqa: E731
    vulkan, driver = card() if covers("desktop", "web") else (dict(LAVAPIPE), None)
    units = []

    # The game module, again only when its sources or the matched links changed.
    units.append(Unit("module", ["python3", "tools/game_module.py", "--stubs", "--jobs", str(wide)], cpus=wide, memory=2048,
                      reads=lambda: [tree("src/decomp", "src/port", "tools/game_module.py", "tools/game_schema.py", "tools/game_coverage.py"),
                                     private(".local/decomp/build/*.elf"), tree(*TOOLS)],
                      makes=tuple(ROOT / "build/game" / name for name in ("game.wasm", "schema.json", "stubs.txt", "stamp"))))

    # The native hosts every tier compares with, and the examples the tests run.
    cargo = {"CARGO_TARGET_DIR": str(TARGET), "CARGO_BUILD_JOBS": str(wide)}
    build = {"cpus": wide, "memory": wide * 512}
    units.append(Unit("hosts", ["cargo", "build", "--profile", "suite", "-p", "xem-headless", "-p", "xem-desktop", "--bins",
                                "--example", "validation", "--example", "first_person"],
                      cwd=RUNTIME, env=cargo, needs=("module",), **build))

    if covers("desktop"):
        units.append(Unit("tests", bash(f"cargo test --profile suite --workspace --no-run --message-format=json-render-diagnostics >'{LOGS}/tests.json'"),
                          cwd=RUNTIME, env=cargo, needs=("hosts",), **build,
                          then=lambda: rust_tests(LOGS / "tests.json", machine, records, vulkan, driver)))
        units.append(Unit("port", ["python3", "-m", "unittest", *PORT_TESTS], needs=("module",),
                          reads=lambda: [tree("tests", "tools", "src/port", "src/decomp"), game_module(), *common()]))
        # The platforming table's proof on the user's discs: xem-core's
        # PLATFORMING_AREAS is the classifier's. It reads both discs
        # extracted and their images under .local.
        extracted = all((ROOT / f".local/extract/disc{n}/manifest.json").is_file() and (ROOT / f".local/discs/disc{n}.bin").is_file()
                        for n in (1, 2))
        if extracted:
            units.append(Unit("platforming", ["python3", "-m", "tools.analysis.platforming", "--check"],
                              reads=lambda: [tree("tools", "src/runtime/crates/xem-core/src/encounters.rs"), *common()]))
        else:
            print("no platforming check: it needs both discs under .local (extract/discN, discs/discN.bin)")
        if has_disc:
            # The validation sheet's pictures are the software rasterizer's.
            units.append(Unit("validation", [str(suite / "examples/validation")], cwd=RUNTIME, env=LAVAPIPE | INLINE, needs=("hosts",),
                              memory=2048, reads=lambda: [file_digest(suite / "examples/validation"), *common()]))

    if covers("web"):
        units.append(Unit("web", ["python3", "src/runtime/web/build.py"], needs=("module",), **build,
                          env={"CARGO_TARGET_DIR": str(RUNTIME / "target-web"), "CARGO_BUILD_JOBS": str(wide)}))
        web = RUNTIME / "web"
        # A test file reads the page as built, its own tests and what the
        # files share (tests/support.mjs).
        shared = lambda: [directory_digest(ROOT / "build/web"),  # noqa: E731
                          tree("src/runtime/web", "tests/reference-inputs", ":(exclude)src/runtime/web/tests/*.test.mjs"),
                          file_digest(headless), file_digest(suite / "examples/first_person"), driver, *common()]
        browser = lambda name: lambda: [file_digest(web / f"tests/{name}.test.mjs"), *shared()]  # noqa: E731
        # The browsers' WebGL2 draws on the graphics card too (ANGLE on its
        # Vulkan driver), in half the time and less; their WebGPU stays on
        # SwiftShader, and so does a test that asks for it (tests/support.mjs,
        # `software`).
        node = {"XEM_HEADLESS": str(headless)} | vulkan
        files = sorted(path.name.removesuffix(".test.mjs") for path in (web / "tests").glob("*.test.mjs"))
        for name in files:
            if TIMED.get(name) == ["."]:
                continue
            skip = [f"--test-skip-pattern={'|'.join(TIMED[name])}"] if name in TIMED else []
            # A browser keeps a thread or two busy once its pictures are the
            # card's (its page's, and what the card's driver takes), until
            # its file has run here.
            units.append(Unit(f"browser.{name}", ["node", "--test", *skip, f"tests/{name}.test.mjs"], cwd=web, env=node,
                              needs=("web", "hosts"), cpus=kept(records, f"browser.{name}", 2 if driver else 3), memory=3072,
                              reads=browser(name)))
        for name, patterns in TIMED.items():
            for index, pattern in enumerate(patterns):
                select = [] if pattern == "." else [f"--test-name-pattern={pattern}"]
                units.append(Unit(f"timed.{name}{'.' + str(index + 1) if len(patterns) > 1 else ''}",
                                  ["node", "--test", *select, f"tests/{name}.test.mjs"], cwd=web, env=node,
                                  needs=("web", "hosts"), alone=True, memory=3072, reads=browser(name)))

    if covers("android"):
        android = lambda *command: in_shell("runtime#android", "env", f"CARGO_TARGET_DIR={RUNTIME}/target-android",  # noqa: E731
                                            f"CARGO_BUILD_JOBS={wide}", *command)
        apk = ROOT / "build/android/gradle/app/outputs/apk/debug/xem-debug.apk"
        # A smoke test reads its scripts, the session logs it plays and the native host it compares with.
        smoke = lambda *names: [tree(*(f"src/runtime/scripts/{name}" for name in names), "tests/reference-inputs"),  # noqa: E731
                                file_digest(headless), *common()]
        # No Gradle daemon stays behind the build (it would keep its two
        # gigabytes for three hours).
        units.append(Unit("android-build", android("env", "XEM_ANDROID_ABIS=x86_64 arm64-v8a", "GRADLE_OPTS=-Dorg.gradle.daemon=false",
                                                   "src/runtime/scripts/android-build.sh"),
                          needs=("module",), **build))
        units.append(Unit("arm64-build", android("env", f"CARGO_TARGET_DIR={RUNTIME}/target-arm64", "XEM_ARM64_BUILD=1",
                                                 "src/runtime/scripts/arm64-smoke.sh"),
                          needs=("module",), **build))
        arm64 = RUNTIME / "target-arm64/aarch64-linux-android/suite/xem-headless"
        # The arm64 build plays the slice's logs under qemu beside others (it measures nothing).
        units.append(Unit("arm64", android("env", f"CARGO_TARGET_DIR={RUNTIME}/target-arm64", "XEM_SKIP_BUILD=1", f"XEM_HEADLESS={headless}",
                                           "src/runtime/scripts/arm64-smoke.sh"),
                          needs=("arm64-build", "hosts"), cpus=kept(records, "arm64", min(8, threads)), memory=2048,
                          reads=lambda: [file_digest(arm64), *smoke("arm64-smoke.sh")]))
        # The emulator renders on the processor: its smoke runs alone, with
        # the APK's x86_64 library, then its game with the arm64-v8a one
        # (which the emulator runs through its ARM translation).
        units.append(Unit("android", android("env", "XEM_SKIP_BUILD=1", "XEM_SMOKE_KEEP=1", f"XEM_HEADLESS={headless}",
                                             "src/runtime/scripts/android-smoke.sh"),
                          needs=("android-build", "hosts"), alone=True, memory=6144,
                          reads=lambda: [file_digest(apk), *smoke("android-smoke.sh", "android-ui.py")]))
        units.append(Unit("android-arm64", android("env", "XEM_SKIP_BUILD=1", "XEM_SMOKE_ABI=arm64-v8a", "XEM_SMOKE_GAME_ONLY=1",
                                                   f"XEM_HEADLESS={headless}", "src/runtime/scripts/android-smoke.sh"),
                          needs=("android",), alone=True, memory=6144,
                          reads=lambda: [file_digest(apk), *smoke("android-smoke.sh", "android-ui.py")]))

    if covers("xr") and has_disc:
        units.append(Unit("xr", ["src/runtime/scripts/xr-game-smoke.sh"],
                          env={"CARGO_TARGET_DIR": str(TARGET), "XEM_CARGO_PROFILE": "suite", "XEM_SKIP_BUILD": "1"},
                          needs=("hosts",), alone=True, memory=4096,
                          reads=lambda: [file_digest(headless), file_digest(suite / "xem-desktop"),
                                         tree("src/runtime/scripts/xr-game-smoke.sh", "tests/reference-inputs",
                                              "src/runtime/crates/xem-core/src/encounters.rs"),
                                         *common()]))

    if tier == "full":
        # What CI runs (.github/workflows/ci.yml), without a game module
        # (none at that path) as on a clone without the user's discs.
        units.append(Unit("public", ["python3", "tools/repository/check.py"], **build,
                          env={"CARGO_TARGET_DIR": str(RUNTIME / "target-public"), "CARGO_BUILD_JOBS": str(wide),
                               "XEM_GAME_WASM": "/nonexistent/game.wasm"},
                          reads=lambda: [tree("."), *common()]))
        units.append(Unit("matching", in_shell("ghidra#matching", "make", "-C", "src/decomp", "smoke"), cpus=2,
                          reads=lambda: [tree("src/decomp", "tools", "nix"), *common()]))
    return units


def rust_tests(messages: Path, machine: Machine, records: dict, vulkan: dict[str, str], driver: object) -> list[Unit]:
    """A unit for each test binary cargo built (its messages), run as cargo
    runs it, from its package's directory, those with the most work first;
    and the game's order from within the scene again on wgpu's GL backend
    (Mesa's llvmpipe through EGL), whose clip space WebGL2 and GLES draw
    through."""
    tests, binaries = [], {}
    for text in messages.read_text().splitlines():
        if not text.startswith("{"):
            continue
        message = json.loads(text)
        if message.get("reason") != "compiler-artifact" or not message.get("executable"):
            continue
        package = Path(message["manifest_path"]).parent
        if message["profile"]["test"]:
            tests.append((package, message["target"], Path(message["executable"])))
        elif "bin" in message["target"]["kind"]:
            binaries.setdefault(package, []).append(Path(message["executable"]))
    gl = {"WGPU_BACKEND": "gl", "__EGL_VENDOR_LIBRARY_FILENAMES": os.environ.get("XEM_MESA_EGL", ""),
          "LIBGL_ALWAYS_SOFTWARE": "1", "GALLIUM_DRIVER": "llvmpipe"}
    units = []
    for package, target, executable in tests:
        integration = "test" in target["kind"]
        kind = target["name"] if integration else "unit"
        name = f"{package.name.removeprefix('xem-')}.{kind}"
        timed = (package.name, kind) in TIMED_RUST

        def reads(package=package, executable=executable, integration=integration) -> object:
            # An integration test runs its package's programs (CARGO_BIN_EXE_*).
            programs = [file_digest(path) for path in binaries.get(package, [])] if integration else []
            data = tree("tests/reference-inputs", "tests/fixtures", str(package.relative_to(ROOT)), ":(exclude)*.rs")
            return [file_digest(executable), programs, data, driver, tree(*TOOLS), private(*PRIVATE)]

        env = {"CARGO_MANIFEST_DIR": str(package), "CARGO_PKG_NAME": package.name}
        # With line tables, for a backtrace that names the test's lines.
        look = (f"(cd src/runtime && RUST_BACKTRACE=1 VK_ICD_FILENAMES=$XEM_LAVAPIPE_ICD cargo test --profile suite-debug "
                f"-p {package.name} {'--test ' + kind if integration else '--lib'})")
        if timed:
            units.append(Unit(f"rust.{name}", [str(executable)], cwd=package, env=env | vulkan, needs=("tests",), alone=True, reads=reads,
                              look=look))
            continue
        units.append(Unit(f"rust.{name}", [str(executable)], cwd=package, env=env | vulkan | INLINE, needs=("tests",), tests=True,
                          cpus=max(machine.cpus // 4, 1), memory=machine.memory // 4, reads=reads, look=look))
        if integration and (package.name, target["name"]) in (("xem-render", "ps1"), ("xem-headless", "first_person")):
            units.append(Unit(f"rust-gl.{name}", [str(executable), "from_within_a_face_drawn_over_another", "from_within_the_games_order"],
                              cwd=package, env=env | LAVAPIPE | INLINE | gl, needs=("tests",), tests=True, cpus=2, memory=2048, reads=reads))
    # Each binary's threads by its work, where it has run here before (and
    # no more than it has tests).
    work = {unit.name: records[unit.name]["work"] for unit in units if unit.tests and "work" in records.get(unit.name, {})}
    threads = shares(work, int(machine.cpus))
    for unit in units:
        if unit.name in work:
            unit.cpus = max(min(threads[unit.name], records[unit.name].get("count") or threads[unit.name]), 1)
    return sorted(units, key=lambda unit: -work.get(unit.name, math.inf))


# --- A run -------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a tier of the test suite (see suite.sh).")
    parser.add_argument("tier", choices=["desktop", "web", "android", "xr", "full"])
    parser.add_argument("--force", action="store_true", help="run every unit, whatever passed before")
    parser.add_argument("--threads", type=int, help="the processor threads the run may keep busy (default: one a core)")
    args = parser.parse_args()
    if args.threads is not None and args.threads < 1:
        parser.error("--threads takes a count of at least 1")

    LOGS.mkdir(parents=True, exist_ok=True)
    lock = open(LOGS / "lock", "w")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        print(f"Another suite run holds {LOGS}/lock.", file=sys.stderr)
        return 2
    store = LOGS / "results.json"
    records = json.loads(store.read_text()) if store.exists() else {}
    # The Rust disc tests read the raw image (.local/discs/disc1.bin); every
    # other unit finds discs/"Xenogears disc 1.chd" itself.
    os.environ.pop("XEM_DISC1", None)

    machine = Machine.this(args.threads)
    # The run and everything it starts stay on that many processors, so a
    # unit that starts a thread a processor (a browser's rasterizer, a
    # compiler) keeps no more of the machine busy than the run may.
    os.sched_setaffinity(0, processors()[: int(machine.cpus)])
    began = time.monotonic()
    run = Run(plan(args.tier, machine, records), machine, records, args.force)
    # Until the run has looked for itself: what the machine has been doing
    # this last minute.
    run.elsewhere = os.getloadavg()[0]

    def stop(*_: object) -> None:
        run.stopping = True
        stop_running()

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        results = run.run()
    finally:
        store.write_text(json.dumps(records, indent=1, sort_keys=True) + "\n")
        if (ROOT / "build/android/smoke/booted-at").exists():
            # The smoke left its emulator for the unit after it: none stays.
            subprocess.run(in_shell("runtime#android", "env", "XEM_SMOKE_STOP=1", "src/runtime/scripts/android-smoke.sh"),
                           cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)

    counts = {outcome: sum(result.outcome == outcome for result in results.values()) for outcome in ("pass", "same", "FAIL", "left")}
    total = (f"{args.tier} suite: {time.monotonic() - began:.0f} s; {counts['pass']} passed, {counts['same']} the same as when they passed, "
             f"{counts['FAIL']} failed, {counts['left']} left")
    (LOGS / "summary.txt").write_text("".join(line(name, results[name]) + "\n" for name in run.order) + total + "\n")
    for name in run.order:
        if results[name].outcome in ("FAIL", "left"):
            print(line(name, results[name]))
    print(total)
    return 1 if counts["FAIL"] or counts["left"] or run.stopping else 0


if __name__ == "__main__":
    sys.exit(main())
