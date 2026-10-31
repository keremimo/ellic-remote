#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Build Melodee Remote on the pinned fm1-base (Saadet's pin), with its overlay, in a disposable copy.

  tools/build_base.py [--host | --no-ble | --release X.Y.Z]   (the arguments go to fm1-base's tools/build.py)

FM1_BASE_MIRROR=PATH fetches the pinned revision from a local clone (Saadet's build/deps one) instead of GitHub.
"""
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REV = "6c0b47e4bfc56fd3cd5b34463210d0e7e0ce0431"      # fm1-base, as Saadet 0.1 builds it
BSP_REV = "0f08fcaa6610b7cabe8cfdcc5c50d6facdae0e7f"  # felucca-bsp, its submodule
URL = "https://github.com/charlesvestal/fm1-base.git"
source = ROOT / "build/deps" / ("fm1-base-" + REV[:12])
work = ROOT / "build/fm1-base-platform"


def run(args, **kw):
    subprocess.run([str(x) for x in args], check=True, **kw)


def head(path):
    return subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()


mirror = os.environ.get("FM1_BASE_MIRROR")
if not source.exists():
    source.parent.mkdir(parents=True, exist_ok=True)
    run(["git", "clone", "--no-checkout", mirror or URL, source])
    run(["git", "-C", source, "checkout", "--detach", REV])
if not (source / "bsp/.git").exists():
    local = []
    if mirror and (Path(mirror) / "bsp/.git").exists():
        run(["git", "-C", source, "config", "submodule.bsp.url", str(Path(mirror) / "bsp")])
        local = ["-c", "protocol.file.allow=always"]   # (git refuses a local submodule's path otherwise)
    run(["git", *local, "-C", source, "submodule", "update", "--init", "--recursive"])
if head(source) != REV:
    raise SystemExit("fm1-base is not at the pinned revision")
if head(source / "bsp") != BSP_REV:
    raise SystemExit("felucca-bsp is not at the pinned revision")
if subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True).strip():
    raise SystemExit("the pinned fm1-base has local changes; keep them elsewhere and use a clean checkout")
if work.exists():
    shutil.rmtree(work)                                # the disposable copy only, never the pinned checkout
shutil.copytree(source, work, ignore=shutil.ignore_patterns(".git", "build", "__pycache__"))
sys.path.insert(0, str(ROOT / "tools"))
from base_overlay import apply  # noqa: E402

apply(work)
env = os.environ.copy()
env.setdefault("JIELI_TOOLCHAIN", str(Path.home() / ".jieli/toolchain"))
env.setdefault("AC79_SDK", str(Path.home() / "fw-AC79_AIoT_SDK"))
run([sys.executable, work / "tools/build.py", ROOT / "app", *sys.argv[1:]], env=env, cwd=ROOT)
