"""Corresponding sources and build recipes for bundled Nix runtime libraries.

Collect source inputs of the actual runtime closure, not a second dependency
list. Include the locked Nixpkgs recipes/patches and each library's original
source archive, with the derivations recording exact build options. Build tools
are not bundled runtime libraries. The application itself is covered by the
separate authored-source archive.
"""
import gzip
import io
import json
import os
import re
import subprocess
import tarfile
from pathlib import Path


def store_root(path):
    match = re.match(r"(/nix/store/[^/]+)", str(path))
    if not match:
        raise ValueError(f"not a Nix store library: {path}")
    return match[1]


def bundle(components, source_roots, destination):
    closure = subprocess.check_output(["nix-store", "--query", "--requisites", *components], text=True).splitlines()
    # A cached output's preferred deriver may have been garbage-collected or
    # differ from the local instantiation. Query the valid local derivations.
    derivers = subprocess.check_output(["nix-store", "--query", "--valid-derivers", *closure], text=True).splitlines()
    drvs = sorted(set(path for path in derivers if path.endswith(".drv")))
    if not drvs:
        raise ValueError("no runtime source derivations found")
    document = json.loads(subprocess.check_output(["nix", "--extra-experimental-features", "nix-command flakes",
                                                  "derivation", "show", *drvs], text=True))
    if document.get("version") != 4:
        raise ValueError("expected the pinned Nix derivation JSON version 4")
    derivations = document["derivations"]
    sources = {store_root(path) for path in source_roots}
    derived = {"/nix/store/" + output["path"] for drv in derivations.values()
               for output in drv["outputs"].values() if "path" in output}
    for path in set(closure) - derived:
        deriver = subprocess.check_output(["nix-store", "--query", "--deriver", path], text=True).strip()
        if deriver.endswith(".drv"):
            raise ValueError(f"no locally instantiated source derivation for {path}: {deriver}")
        sources.add(path)
    for drv in derivations.values():
        environment = drv.get("env", {})
        if environment.get("name", "").startswith("xem-"):
            continue
        sources.update("/nix/store/" + path for path in drv["inputs"]["srcs"])
        for key in ["src", "patches"]:
            sources.update(store_root(path) for path in re.findall(r"/nix/store/[^\s\"']+", environment.get(key, "")))
    missing = [path for path in sources if not Path(path).exists()]
    if missing:
        # Realize source derivations from the pinned recipes when a binary cache
        # supplied the runtime without its source archives.
        subprocess.run(["nix-store", "--realise", *missing], check=True)
    def normalized(info):
        info.uid = info.gid = info.mtime = 0
        info.uname = info.gname = ""
        return info
    with open(destination, "wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=0) as compressed:
        with tarfile.open(fileobj=compressed, mode="w|", format=tarfile.PAX_FORMAT) as archive:
            metadata = json.dumps({"runtime_paths": sorted(set(closure)), "derivations": derivations}, indent=2, sort_keys=True).encode()
            info = tarfile.TarInfo("runtime-builds.json")
            info.size = len(metadata)
            archive.addfile(normalized(info), io.BytesIO(metadata))
            for name in sorted(sources):
                archive.add(name, arcname="sources/" + os.path.basename(name), filter=normalized)
    return destination
